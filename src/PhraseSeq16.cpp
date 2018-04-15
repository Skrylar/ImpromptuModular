//***********************************************************************************************
//Multi-phrase 16 step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by the SA-100 Stepper Acid Sequencer by Transistor Sounds Labs
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct PhraseSeq16 : Module {
	enum ParamIds {
		LEFT_PARAM,
		RIGHT_PARAM,
		LENGTH_PARAM,
		EDIT_PARAM,
		PATTERN_PARAM,
		RUN_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		OCTP_PARAM,
		OCTM_PARAM,
		GATE1_PARAM,
		GATE2_PARAM,
		SLIDE_BTN_PARAM,
		SLIDE_KNOB_PARAM,
		ATTACH_PARAM,
		ROTATEL_PARAM,
		ROTATER_PARAM,
		PASTESYNC_PARAM,
		ENUMS(KEY_PARAMS, 12),
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		CV_INPUT,
		RESET_INPUT,
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE1_OUTPUT,
		GATE2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, 16*2),// room for GreenRed
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12),
		RUN_LIGHT,
		RESET_LIGHT,
		GATE1_LIGHT,
		GATE2_LIGHT,
		SLIDE_LIGHT,
		ATTACH_LIGHT,
		NUM_LIGHTS
	};

	// Need to save
	bool running;
	//
	int stepIndexEdit;
	int stepIndexRun;
	int pattern;
	int steps;//1 to 16
	//
	int phraseIndexEdit;
	int phraseIndexRun;
	int stepIndexPhraseRun;
	int phrase[16] = {};// This is the song (series of phases; a phrase is a patten number)
	int phrases;//1 to 16
	//
	float cv[16][16] = {}; // [-3.0 : 3.917]. First index is patten number, 2nd index is step
	bool gate1[16][16] = {}; // First index is patten number, 2nd index is step
	bool gate2[16][16] = {}; // First index is patten number, 2nd index is step
	bool slide[16][16] = {}; // First index is patten number, 2nd index is step
	//
	int patternKnob;// save this so no delta triggered when close/open Rack
	float attach;

	// No need to save
	float resetLight = 0.0f;
	unsigned long editingLength;// 0 when not editing length, downward step counter timer when editing length
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate
		
	SchmittTrigger resetTrigger;
	SchmittTrigger leftTrigger;
	SchmittTrigger rightTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger octpTrigger;
	SchmittTrigger octmTrigger;
	SchmittTrigger gate1Trigger;
	SchmittTrigger gate2Trigger;
	SchmittTrigger slideTrigger;
	SchmittTrigger lengthTrigger;
	SchmittTrigger keyTriggers[12];
	SchmittTrigger writeTrigger;
	SchmittTrigger attachTrigger;
	
		
	PhraseSeq16() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		running = false;
		stepIndexEdit = 0;
		stepIndexRun = 0;
		pattern = 0;
		steps = 16;
		phraseIndexEdit = 0;
		phraseIndexRun = 0;
		stepIndexPhraseRun = 0;
		phrases = 1;
		for (int i = 0; i < 16; i++) {
			for (int s = 0; s < 16; s++) {
				cv[i][s] = 0.0f;
				gate1[i][s] = true;
				gate2[i][s] = true;
				slide[i][s] = true;
			}
			phrase[i] = 0;
		}
		patternKnob = 0;
		editingLength = 0ul;
		editingGate = 0ul;
		attach = 1.0f;
	}

	void onRandomize() override {
		running = false;
		stepIndexEdit = 0;
		stepIndexRun = 0;
		pattern = randomu32() % 16;
		steps = 1 + (randomu32() % 16);
		phraseIndexEdit = 0;
		phraseIndexRun = 0;
		stepIndexPhraseRun = 0;
		phrases = 1 + (randomu32() % 16);
		for (int i = 0; i < 16; i++) {
			for (int s = 0; s < 16; s++) {
				cv[i][s] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
				gate1[i][s] = (randomUniform() > 0.5f);
				gate2[i][s] = (randomUniform() > 0.5f);
				slide[i][s] = (randomUniform() > 0.5f);
			}
			phrase[i] = randomu32() % 16;
		}
		patternKnob = 0;
		editingLength = 0ul;
		editingGate = 0ul;
		attach = 1.0f;
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));

		// stepIndexRun
		json_object_set_new(rootJ, "stepIndexRun", json_integer(stepIndexRun));

		// pattern
		json_object_set_new(rootJ, "pattern", json_integer(pattern));

		// steps
		json_object_set_new(rootJ, "steps", json_integer(steps));

		// phraseIndexEdit
		json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

		// phraseIndexRun
		json_object_set_new(rootJ, "phraseIndexRun", json_integer(phraseIndexRun));

		// stepIndexPhraseRun
		json_object_set_new(rootJ, "stepIndexPhraseRun", json_integer(stepIndexPhraseRun));

		// phrases
		json_object_set_new(rootJ, "phrases", json_integer(phrases));

		// CV
		json_t *cvJ = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(cvJ, s + (i<<4), json_real(cv[i][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// gate1
		json_t *gate1J = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(gate1J, s + (i<<4), json_integer((int) gate1[i][s]));
			}
		json_object_set_new(rootJ, "gate1", gate1J);

		// gate2
		json_t *gate2J = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(gate2J, s + (i<<4), json_integer((int) gate2[i][s]));
			}
		json_object_set_new(rootJ, "gate2", gate2J);

		// slide
		json_t *slideJ = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(slideJ, s + (i<<4), json_integer((int) slide[i][s]));
			}
		json_object_set_new(rootJ, "slide", slideJ);

		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, "phrase", phraseJ);

		// patternKnob
		json_object_set_new(rootJ, "patternKnob", json_integer(patternKnob));

		// attach
		json_object_set_new(rootJ, "attach", json_real(attach));

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// stepIndexEdit
		json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
		if (stepIndexEditJ)
			stepIndexEdit = json_integer_value(stepIndexEditJ);
		
		// stepIndexRun
		json_t *stepIndexRunJ = json_object_get(rootJ, "stepIndexRun");
		if (stepIndexRunJ)
			stepIndexRun = json_integer_value(stepIndexRunJ);
		
		// pattern
		json_t *patternJ = json_object_get(rootJ, "pattern");
		if (patternJ)
			pattern = json_integer_value(patternJ);
		
		// steps
		json_t *stepsJ = json_object_get(rootJ, "steps");
		if (stepsJ)
			steps = json_integer_value(stepsJ);
		
		// phraseIndexEdit
		json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
		if (phraseIndexEditJ)
			phraseIndexEdit = json_integer_value(phraseIndexEditJ);
		
		// phraseIndexRun
		json_t *phraseIndexRunJ = json_object_get(rootJ, "phraseIndexRun");
		if (phraseIndexRunJ)
			phraseIndexRun = json_integer_value(phraseIndexRunJ);
		
		// stepIndexPhraseRun
		json_t *stepIndexPhraseRunJ = json_object_get(rootJ, "stepIndexPhraseRun");
		if (stepIndexPhraseRunJ)
			stepIndexPhraseRun = json_integer_value(stepIndexPhraseRunJ);
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, "phrases");
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
		
		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *cvArrayJ = json_array_get(cvJ, s + (i<<4));
					if (cvArrayJ)
						cv[i][s] = json_real_value(cvArrayJ);
				}
		}
		
		// gate1
		json_t *gate1J = json_object_get(rootJ, "gate1");
		if (gate1J) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *gate1arrayJ = json_array_get(gate1J, s + (i<<4));
					if (gate1arrayJ)
						gate1[i][s] = !!json_integer_value(gate1arrayJ);
				}
		}
		
		// gate2
		json_t *gate2J = json_object_get(rootJ, "gate2");
		if (gate2J) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *gate2arrayJ = json_array_get(gate2J, s + (i<<4));
					if (gate2arrayJ)
						gate2[i][s] = !!json_integer_value(gate2arrayJ);
				}
		}
		
		// slide
		json_t *slideJ = json_object_get(rootJ, "slide");
		if (slideJ) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *slideArrayJ = json_array_get(slideJ, s + (i<<4));
					if (slideArrayJ)
						slide[i][s] = !!json_integer_value(slideArrayJ);
				}
		}
		
		// phrase
		json_t *phraseJ = json_object_get(rootJ, "phrase");
		if (phraseJ)
			for (int i = 0; i < 16; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
			
		// patternKnob
		json_t *patternKnobJ = json_object_get(rootJ, "patternKnob");
		if (patternKnobJ)
			patternKnob = json_integer_value(patternKnobJ);
		
		// attach
		json_t *attachJ = json_object_get(rootJ, "attach");
		if (attachJ)
			attach = json_real_value(attachJ);
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		bool editingPattern = params[EDIT_PARAM].value > 0.5f;// true = editing pattern, false = editing song
		
		// Run state and light
		if (runningTrigger.process(params[RUN_PARAM].value)) {
			running = !running;
		}
		lights[RUN_LIGHT].value = (running);

		// Attach button and behavior
		if (attachTrigger.process(params[ATTACH_PARAM].value)) {
			if (running) {
				attach = 1.0f - attach;// toggle
			}			
		}
		if (running && attach > 0.5f) {
			if (editingPattern)
				stepIndexEdit = stepIndexRun;
			else
				phraseIndexEdit = phraseIndexRun;
		}
		
		// Length button
		static const float editTime = 2.0f;// seconds
		if (lengthTrigger.process(params[LENGTH_PARAM].value)) {
			editingLength = (unsigned long) (editTime * engineGetSampleRate());
		}
		else {
			if (editingLength > 0ul)
				editingLength--;
		}
		
		// Left and right buttons
		int delta = 0;
		if (leftTrigger.process(params[LEFT_PARAM].value)) 
			delta = -1;
		if (rightTrigger.process(params[RIGHT_PARAM].value))
			delta = +1;
		if (delta != 0) {
			if (editingLength > 0ul) {
				editingLength = (unsigned long) (editTime * engineGetSampleRate());// restart editing timer
				if (editingPattern) {
					steps += delta;
					if (steps > 16) steps = 16;
					if (steps < 1 ) steps = 1;
					if (stepIndexEdit >= steps) stepIndexEdit = steps - 1;
				}
				else {
					phrases += delta;
					if (phrases > 16) phrases = 16;
					if (phrases < 1 ) phrases = 1;
					if (phraseIndexEdit >= phrases) phraseIndexEdit = phrases - 1;
				}
			}
			else {
				if (!running || attach < 0.5f) {// don't move heads when attach and running
					if (editingPattern)
						stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, steps);
					else
						phraseIndexEdit = moveIndex(phraseIndexEdit, phraseIndexEdit + delta, phrases);
				}
			}
		}
		
		
		// Pattern knob
		int newPatternKnob = (int)roundf(params[PATTERN_PARAM].value*7.0f);
		if (newPatternKnob != patternKnob) {
			if (abs(newPatternKnob - patternKnob) <= 1) {// avoid discontinuous step (initialize for example)
				if (editingPattern) {
					pattern += newPatternKnob - patternKnob;
					if (pattern < 0) pattern = 0;
					if (pattern > 15) pattern = 15;
				}
				else {
					phrase[phraseIndexEdit] += newPatternKnob - patternKnob;
					if (phrase[phraseIndexEdit] < 0) phrase[phraseIndexEdit] = 0;
					if (phrase[phraseIndexEdit] > 15) phrase[phraseIndexEdit] = 15;					
				}
			}
			patternKnob = newPatternKnob;
		}	
		
		// Octave buttons
		static const float gateTime = 0.6f;// seconds
		float deltaOct = 0.0f;
		if (octpTrigger.process(params[OCTP_PARAM].value))
			deltaOct = 1.0f;
		if (octmTrigger.process(params[OCTM_PARAM].value)) 
			deltaOct = -1.0f;
		if (deltaOct > 0.5f || deltaOct < -0.5f) {
			if (editingPattern) {
				float newCV = cv[pattern][stepIndexEdit] + deltaOct;
				if (newCV >= -3.0f && newCV < 4.0f)
					cv[pattern][stepIndexEdit] = newCV;
				editingGate = (unsigned long) (gateTime * engineGetSampleRate());
			}
		}		
		
		// Keyboard and cv input 
		if (editingPattern) {
			for (int i = 0; i < 12; i++) {
				if (keyTriggers[i].process(params[KEY_PARAMS + i].value)) {
					cv[pattern][stepIndexEdit] = floor(cv[pattern][stepIndexEdit]) + ((float) i) / 12.0f;
					editingGate = (unsigned long) (gateTime * engineGetSampleRate());
				}
			}
			if (writeTrigger.process(inputs[WRITE_INPUT].value)) {
				cv[pattern][stepIndexEdit] = inputs[CV_INPUT].value;
				editingGate = (unsigned long) (gateTime * engineGetSampleRate());
			}
		}
		
		// Gate1, Gate2 and slide buttons
		if (gate1Trigger.process(params[GATE1_PARAM].value)) {
			if (editingPattern)
				gate1[pattern][stepIndexEdit] = !gate1[pattern][stepIndexEdit];
		}		
		if (gate2Trigger.process(params[GATE2_PARAM].value)) {
			if (editingPattern)
				gate2[pattern][stepIndexEdit] = !gate2[pattern][stepIndexEdit];
		}		
		if (slideTrigger.process(params[SLIDE_BTN_PARAM].value)) {
			if (editingPattern)
				slide[pattern][stepIndexEdit] = !slide[pattern][stepIndexEdit];
		}		
		
	
		if (running) {
			// Clock
			if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
				if (editingPattern) {
					stepIndexRun++;
					if (stepIndexRun >= steps) stepIndexRun = 0;
				}
				else {
					stepIndexPhraseRun++;
					if (stepIndexPhraseRun >= steps) {
						stepIndexPhraseRun = 0;
						phraseIndexRun++;
						if (phraseIndexRun >= phrases) 
							phraseIndexRun = 0;
					}	
				}
			}
		}	
		
		// CV and gates outputs
		if (running) {
			if (editingPattern) {// editing pattern while running
				outputs[CV_OUTPUT].value = cv[pattern][stepIndexRun];
				outputs[GATE1_OUTPUT].value = (clockTrigger.isHigh() && gate1[pattern][stepIndexRun]) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (clockTrigger.isHigh() && gate2[pattern][stepIndexRun]) ? 10.0f : 0.0f;
			}
			else {// editing song while running
				outputs[CV_OUTPUT].value = cv[phrase[phraseIndexRun]][stepIndexPhraseRun];
				outputs[GATE1_OUTPUT].value = (clockTrigger.isHigh() && gate1[phrase[phraseIndexRun]][stepIndexPhraseRun]) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (clockTrigger.isHigh() && gate2[phrase[phraseIndexRun]][stepIndexPhraseRun]) ? 10.0f : 0.0f;
			}
		}
		else {// not running 
			if (editingPattern) {// editing pattern while not running
				outputs[CV_OUTPUT].value = cv[pattern][stepIndexEdit];
				outputs[GATE1_OUTPUT].value = (editingGate > 0ul) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (editingGate > 0ul) ? 10.0f : 0.0f;
			}
			else {// editing song while not running
				outputs[CV_OUTPUT].value = 0.0f;
				outputs[GATE1_OUTPUT].value = 0.0f;
				outputs[GATE2_OUTPUT].value = 0.0f;
			}
		}
		if (editingGate > 0ul)
			editingGate--;
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			stepIndexEdit = 0;
			stepIndexRun = 0;
			phraseIndexEdit = 0;
			phraseIndexRun = 0;
			resetLight = 1.0f;
		}
		else
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime();
	
		// Step/phrase lights
		for (int i = 0; i < 16; i++) {
			if (editingLength > 0ul) {
				// Length (green)
				if (editingPattern)
					lights[STEP_PHRASE_LIGHTS + (i<<1)].value = ((i < steps) ? 0.5f : 0.0f);
				else
					lights[STEP_PHRASE_LIGHTS + (i<<1)].value = ((i < phrases) ? 0.5f : 0.0f);
				// Nothing (red)
				lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = 0.0f;
			}
			else {
				// Run cursor (green)
				if (editingPattern)
					lights[STEP_PHRASE_LIGHTS + (i<<1)].value = ((running && (i == stepIndexRun)) ? 1.0f : 0.0f);
				else
					lights[STEP_PHRASE_LIGHTS + (i<<1)].value = ((running && (i == phraseIndexRun)) ? 1.0f : 0.0f);
				// Edit cursor (red)
				if (editingPattern)
					lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = (i == stepIndexEdit ? 1.0f : 0.0f);
				else
					lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = (i == phraseIndexEdit ? 1.0f : 0.0f);
			}
		}
	
		// Octave lights
		int octLightIndex = -1;
		if (editingPattern)
			octLightIndex = (int) floor(cv[pattern][stepIndexEdit] + 3.0f);
		for (int i = 0; i < 7; i++) {
			lights[OCTAVE_LIGHTS + i].value = (i == octLightIndex ? 1.0f : 0.0f);
		}
		// Keyboard lights
		int keyLightIndex = -1;
		if (editingPattern) {
			float cvValOffset = cv[pattern][stepIndexEdit] +10.0f;//to properly handle negative note voltages
			keyLightIndex = (int) clamp(  roundf( (cvValOffset-floor(cvValOffset)) * 12.0f ),  0.0f,  11.0f);
		}
		for (int i = 0; i < 12; i++) {
			lights[KEY_LIGHTS + i].value = (i == keyLightIndex ? 1.0f : 0.0f);
		}		
		// Gate1, Gate2 and Slide lights
		lights[GATE1_LIGHT].value = (editingPattern && gate1[pattern][stepIndexEdit]) ? 1.0f : 0.0f;
		lights[GATE2_LIGHT].value = (editingPattern && gate2[pattern][stepIndexEdit]) ? 1.0f : 0.0f;
		lights[SLIDE_LIGHT].value = (editingPattern && slide[pattern][stepIndexEdit]) ? 1.0f : 0.0f;

		// attach light
		lights[ATTACH_LIGHT].value = running ? attach : 0.0f;
		
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	

	}
};


struct PhraseSeq16Widget : ModuleWidget {

	struct PatternDisplayWidget : TransparentWidget {
		PhraseSeq16 *module;
		std::shared_ptr<Font> font;
		
		PatternDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}

		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box);
			nvgFontFaceId(vg, font->handle);
			//nvgTextLetterSpacing(vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			nvgText(vg, textPos.x, textPos.y, "~~", NULL);
			nvgFillColor(vg, textColor);
			char displayStr[3];
			snprintf(displayStr, 3, "%2u", (unsigned) (module->params[PhraseSeq16::EDIT_PARAM].value > 0.5f ? 
				module->pattern : module->phrase[module->phraseIndexEdit]) + 1 );
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	PhraseSeq16Widget(PhraseSeq16 *module) : ModuleWidget(module) {
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/PhraseSeq16.svg")));

		// Screws
		addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));

		
		// ****** Top portion ******
		
		static const int rowRulerT0 = 48;
		static const int columnRulerT0 = 15;// Step/Phase lights
		static const int columnRulerT1 = columnRulerT0 + 260;// Left/Right buttons
		static const int columnRulerT2 = columnRulerT1 + 75;// Length button

		// Step/Phrase lights
		static const int spLightsSpacing = 15;
		for (int i = 0; i < 16; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(columnRulerT0 + spLightsSpacing * i + offsetMediumLight, rowRulerT0 + offsetMediumLight), module, PhraseSeq16::STEP_PHRASE_LIGHTS + (i*2)));
		}
		// Left/Right buttons
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerT1 + offsetCKD6, rowRulerT0 + offsetCKD6), module, PhraseSeq16::LEFT_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerT1 + 32 + offsetCKD6, rowRulerT0 + offsetCKD6), module, PhraseSeq16::RIGHT_PARAM, 0.0f, 1.0f, 0.0f));
		// Length button
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerT2 + offsetCKD6, rowRulerT0 + offsetCKD6), module, PhraseSeq16::LENGTH_PARAM, 0.0f, 1.0f, 0.0f));
		
		
		// ****** Middle keyboard portion ******
		
		static const int rowRulerMK0 = 82;
		static const int rowRulerMK1 = rowRulerMK0 + 82;
		static const int columnRulerMK0 = 15;// Octave lights
		static const int columnRulerMK1 = columnRulerMK0 + 262;// Edit mode and run switch
		static const int columnRulerMK2 = columnRulerT2;// Pattern display and knob

		// Octave lights
		static const int octLightsSpacing = 15;
		for (int i = 0; i < 7; i++) {
			addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMK0 + offsetMediumLight, rowRulerMK0 - 2 + octLightsSpacing * i + offsetMediumLight), module, PhraseSeq16::OCTAVE_LIGHTS + 6 - i));
		}
		// Keys and Key lights
		static const int offsetKeyLEDx = 6;
		static const int offsetKeyLEDy = 28;
		// Black keys and lights
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(61, 89), module, PhraseSeq16::KEY_PARAMS + 1, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(61+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 1));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(89, 89), module, PhraseSeq16::KEY_PARAMS + 3, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(89+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 3));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(146, 89), module, PhraseSeq16::KEY_PARAMS + 6, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(146+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 6));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(174, 89), module, PhraseSeq16::KEY_PARAMS + 8, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(174+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 8));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(202, 89), module, PhraseSeq16::KEY_PARAMS + 10, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(202+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 10));
		// White keys and lights
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(47, 139), module, PhraseSeq16::KEY_PARAMS + 0, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(47+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 0));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(75, 139), module, PhraseSeq16::KEY_PARAMS + 2, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(75+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 2));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(103, 139), module, PhraseSeq16::KEY_PARAMS + 4, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(103+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 4));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(132, 139), module, PhraseSeq16::KEY_PARAMS + 5, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(132+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 5));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(160, 139), module, PhraseSeq16::KEY_PARAMS + 7, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(160+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 7));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(188, 139), module, PhraseSeq16::KEY_PARAMS + 9, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(188+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 9));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(216, 139), module, PhraseSeq16::KEY_PARAMS + 11, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(216+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 11));
		// Edit mode switch
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerMK1 + hOffsetCKSS, rowRulerMK0 + 17 + vOffsetCKSS), module, PhraseSeq16::EDIT_PARAM, 0.0f, 1.0f, 1.0f));
		// Run LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerMK1 - 19 + offsetLEDbezel, rowRulerMK1 + offsetLEDbezel), module, PhraseSeq16::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerMK1 - 19 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK1 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq16::RUN_LIGHT));
		// Reset LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerMK1 + 19 + offsetLEDbezel, rowRulerMK1 + offsetLEDbezel), module, PhraseSeq16::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerMK1 + 19 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK1 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq16::RESET_LIGHT));
		// Pattern display
		PatternDisplayWidget *displayPattern = new PatternDisplayWidget();
		displayPattern->box.pos = Vec(columnRulerMK2-7, rowRulerMK0 + 20 + vOffsetDisplay);
		displayPattern->box.size = Vec(40, 30);// 2 characters
		displayPattern->module = module;
		addChild(displayPattern);
		// Pattern knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(columnRulerMK2 + offsetDavies1900, rowRulerMK0 + 72 + offsetDavies1900), module, PhraseSeq16::PATTERN_PARAM, -INFINITY, INFINITY, 0.0f));		
		
		
		// ****** Middle Buttons portion ******
		
		static const int rowRulerMB0 = 224;
		static const int rowRulerMB1 = rowRulerMB0 + 36;
		static const int columnRulerMB0 = 22;// Oct
		static const int columnRulerMBspacing = 54;
		static const int columnRulerMB1 = columnRulerMB0 + columnRulerMBspacing;// Gate1
		static const int columnRulerMB2 = columnRulerMB1 + columnRulerMBspacing;// Gate2
		static const int columnRulerMB3 = columnRulerMB2 + columnRulerMBspacing + 12;// Slide
		static const int columnRulerMB4 = columnRulerMK1;// Transpose
		static const int columnRulerMB5 = columnRulerMK2;// Copy-paste
		
		// Oct+/Oct- buttons
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerMB0 + offsetCKD6, rowRulerMB0 + offsetCKD6), module, PhraseSeq16::OCTP_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerMB0 + offsetCKD6, rowRulerMB1 + offsetCKD6), module, PhraseSeq16::OCTM_PARAM, 0.0f, 1.0f, 0.0f));
		// Gate 1 light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB1 + offsetMediumLight, rowRulerMB0 + offsetMediumLight), module, PhraseSeq16::GATE1_LIGHT));		
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerMB1 + offsetCKD6, rowRulerMB1 + offsetCKD6), module, PhraseSeq16::GATE1_PARAM, 0.0f, 1.0f, 0.0f));
		// Gate 2 light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB2 + offsetMediumLight, rowRulerMB0 + offsetMediumLight), module, PhraseSeq16::GATE2_LIGHT));		
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerMB2 + offsetCKD6, rowRulerMB1 + offsetCKD6), module, PhraseSeq16::GATE2_PARAM, 0.0f, 1.0f, 0.0f));
		// Slide light, knob and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB3 - 21 + offsetMediumLight, rowRulerMB0 + offsetMediumLight), module, PhraseSeq16::SLIDE_LIGHT));		
		addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(columnRulerMB3 + 21 + offsetRoundSmallBlackKnob, rowRulerMB0 + offsetRoundSmallBlackKnob), module, PhraseSeq16::SLIDE_KNOB_PARAM, 1.0f, 31.0f, 16.0f));		
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerMB3 + offsetCKD6, rowRulerMB1 + offsetCKD6), module, PhraseSeq16::SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f));
		// Attach button and light
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB4 - 10, rowRulerMB0 - 10 + offsetTL1105), module, PhraseSeq16::ATTACH_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB4 + 14 + offsetMediumLight, rowRulerMB0 - 11 + offsetMediumLight), module, PhraseSeq16::ATTACH_LIGHT));		
		// Rotate buttons
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB4 - 10, rowRulerMB1 - 1 + offsetTL1105), module, PhraseSeq16::ROTATEL_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB4 + 20, rowRulerMB1 - 1 + offsetTL1105), module, PhraseSeq16::ROTATER_PARAM, 0.0f, 1.0f, 0.0f));
		// Copy/paste buttons
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB5 - 10, rowRulerMB0 - 10 + offsetTL1105), module, PhraseSeq16::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB5 + 20, rowRulerMB0 - 10 + offsetTL1105), module, PhraseSeq16::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		// Paste sync (and light)
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(columnRulerMB5 - 6 + hOffsetCKSS, rowRulerMB1 - 1 + vOffsetCKSSThree), module, PhraseSeq16::PASTESYNC_PARAM, 0.0f, 2.0f, 0.0f));	
		//addChild(ModuleLightWidget::create<SmallLight<RedLight>>(Vec(columnRulerMB5 + 41, rowRulerMB1 - 5 + 14), module, PhraseSeq16::PENDING_LIGHT));
		
						
		// ****** Bottom portion ******
		
		static const int rowRulerB0 = 315;
		static const int columnRulerB6 = columnRulerMB5;
		static const int outputJackSpacingX = 54;
		static const int columnRulerB5 = columnRulerB6 - outputJackSpacingX;
		static const int columnRulerB4 = columnRulerB5 - outputJackSpacingX;
		static const int columnRulerB0 = columnRulerMB0;
		static const int columnRulerB1 = columnRulerMB1;
		static const int columnRulerB2 = columnRulerMB2;
		static const int columnRulerB3 = columnRulerMB2 + columnRulerMBspacing;

		// Inputs
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB0, rowRulerB0), Port::INPUT, module, PhraseSeq16::WRITE_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB1, rowRulerB0), Port::INPUT, module, PhraseSeq16::CV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB2, rowRulerB0), Port::INPUT, module, PhraseSeq16::RESET_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB3, rowRulerB0), Port::INPUT, module, PhraseSeq16::CLOCK_INPUT));
		// Outputs
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB4, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::CV_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB5, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::GATE1_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB6, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::GATE2_OUTPUT));

	}
};

Model *modelPhraseSeq16 = Model::create<PhraseSeq16, PhraseSeq16Widget>("Impromptu Modular", "Phrase-Seq-16", "Phrase-Seq-16", SEQUENCER_TAG);
