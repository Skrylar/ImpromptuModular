//***********************************************************************************************
//Four channel 64-step writable sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct WriteSeq64 : Module {
	enum ParamIds {
		SHARP_PARAM,
		QUANTIZE_PARAM,
		GATE_PARAM,
		CHANNEL_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RUN_PARAM,
		WRITE_PARAM,
		STEPL_PARAM,
		MONITOR_PARAM,
		STEPR_PARAM,
		STEPS_PARAM,
		STEP_PARAM,
		AUTOSTEP_PARAM,
		RESET_PARAM,
		PASTESYNC_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CHANNEL_INPUT,
		CV_INPUT,
		GATE_INPUT,
		WRITE_INPUT,
		STEPL_INPUT,
		STEPR_INPUT,
		CLOCK12_INPUT,
		CLOCK34_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		GATE_LIGHT,
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(WRITE_LIGHT, 2),// room for GreenRed
		PENDING_LIGHT,
		NUM_LIGHTS
	};

	// Need to save
	bool running;
	int indexChannel;
	int indexStep[5];// [0;63] each
	int indexSteps[5];// [1;64] each
	float cv[5][64] = {};
	bool gates[5][64] = {};
	int stepKnob = 0;// save this so no delta triggered when close/open Rack
	int stepsKnob = 0;// save this so no delta triggered when close/open Rack

	// No need to save
	float cvCPbuffer[64];// copy paste buffer for CVs
	float gateCPbuffer[64];// copy paste buffer for gates
	int stepsCPbuffer;
	float resetLight = 0.0f;
	int pendingPaste;// 0 = nothing to paste, 1 = paste on clk, 2 = paste on seq, destination channel in next msbits

	SchmittTrigger clock12Trigger;
	SchmittTrigger clock34Trigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger channelTrigger;
	SchmittTrigger stepLTrigger;
	SchmittTrigger stepRTrigger;
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger writeTrigger;
	SchmittTrigger gateTrigger;

	
	WriteSeq64() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		running = true;
		indexChannel = 0;
		for (int c = 0; c < 5; c++) {
			indexStep[c] = 0;
			indexSteps[c] = 64;
			for (int s = 0; s < 64; s++) {
				cv[c][s] = 0.0f;
				gates[c][s] = true;
			}
		}
		for (int s = 0; s < 64; s++) {
			cvCPbuffer[s] = 0.0f;
			gateCPbuffer[s] = true;
		}
		stepsCPbuffer = 64;
		stepKnob = 0;
		stepsKnob = 0;
		pendingPaste = 0;
	}

	void onRandomize() override {
		running = true;
		indexChannel = 0;
		for (int c = 0; c < 5; c++) {
			indexStep[c] = 0;
			indexSteps[c] = 64;
			for (int s = 0; s < 64; s++) {
				cv[c][s] = quantize((randomUniform() *10.0f) - 4.0f, params[QUANTIZE_PARAM].value > 0.5f);
				gates[c][s] = (randomUniform() > 0.5f);
			}
		}
		for (int s = 0; s < 64; s++) {
			cvCPbuffer[s] = 0.0f;
			gateCPbuffer[s] = true;
		}
		stepsCPbuffer = 64;
		stepKnob = 0;
		stepsKnob = 0;
		pendingPaste = 0;
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// indexChannel
		json_object_set_new(rootJ, "indexChannel", json_integer(indexChannel));

		// indexStep 
		json_t *indexStepJ = json_array();
		for (int c = 0; c < 5; c++)
			json_array_insert_new(indexStepJ, c, json_integer(indexStep[c]));
		json_object_set_new(rootJ, "indexStep", indexStepJ);
		
		// indexSteps 
		json_t *indexStepsJ = json_array();
		for (int c = 0; c < 5; c++)
			json_array_insert_new(indexStepsJ, c, json_integer(indexSteps[c]));
		json_object_set_new(rootJ, "indexSteps", indexStepsJ);

		// CV
		json_t *cvJ = json_array();
		for (int c = 0; c < 5; c++)
			for (int s = 0; s < 64; s++) {
				json_array_insert_new(cvJ, s + (c<<6), json_real(cv[c][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// gates
		json_t *gatesJ = json_array();
		for (int c = 0; c < 5; c++)
			for (int s = 0; s < 64; s++) {
				json_array_insert_new(gatesJ, s + (c<<6), json_integer((int) gates[c][s]));
			}
		json_object_set_new(rootJ, "gates", gatesJ);

		// stepKnob
		json_object_set_new(rootJ, "stepKnob", json_integer(stepKnob));
		
		// stepsKnob
		json_object_set_new(rootJ, "stepsKnob", json_integer(stepsKnob));

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
		
		// indexChannel
		json_t *indexChannelJ = json_object_get(rootJ, "indexChannel");
		if (indexChannelJ)
			indexChannel = json_integer_value(indexChannelJ);
		
		// indexStep
		json_t *indexStepJ = json_object_get(rootJ, "indexStep");
		if (indexStepJ)
			for (int c = 0; c < 5; c++)
			{
				json_t *indexStepArrayJ = json_array_get(indexStepJ, c);
				if (indexStepArrayJ)
					indexStep[c] = json_integer_value(indexStepArrayJ);
			}
		
		// indexSteps
		json_t *indexStepsJ = json_object_get(rootJ, "indexSteps");
		if (indexStepsJ)
			for (int c = 0; c < 5; c++)
			{
				json_t *indexStepsArrayJ = json_array_get(indexStepsJ, c);
				if (indexStepsArrayJ)
					indexSteps[c] = json_integer_value(indexStepsArrayJ);
			}

		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int c = 0; c < 5; c++)
				for (int i = 0; i < 64; i++) {
					json_t *cvArrayJ = json_array_get(cvJ, i + (c<<6));
					if (cvArrayJ)
						cv[c][i] = json_real_value(cvArrayJ);
				}
		}
		
		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int c = 0; c < 5; c++)
				for (int i = 0; i < 64; i++) {
					json_t *gateJ = json_array_get(gatesJ, i + (c<<6));
					if (gateJ)
						gates[c][i] = !!json_integer_value(gateJ);
				}
		}

		// stepKnob
		json_t *stepKnobJ = json_object_get(rootJ, "stepKnob");
		if (stepKnobJ)
			stepKnob = json_integer_value(stepKnobJ);

		// stepsKnob
		json_t *stepsKnobJ = json_object_get(rootJ, "stepsKnob");
		if (stepsKnobJ)
			stepsKnob = json_integer_value(stepsKnobJ);
	}

	inline float quantize(float cv, bool enable) {
		return enable ? (roundf(cv * 12.0f) / 12.0f) : cv;
	}
	
	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		// Run state and light
		if (runningTrigger.process(params[RUN_PARAM].value)) {
			running = !running;
			//pendingPaste = 0;// no pending pastes across run state toggles
		}
		lights[RUN_LIGHT].value = (running);
	
		// Copy
		if (copyTrigger.process(params[COPY_PARAM].value)) {
			for (int i = 0; i < 64; i++) {
				cvCPbuffer[i] = cv[indexChannel][i];
				gateCPbuffer[i] = gates[indexChannel][i];
			}
			stepsCPbuffer = indexSteps[indexChannel];
		}
		// Paste
		if (pasteTrigger.process(params[PASTE_PARAM].value)) {
			if (params[PASTESYNC_PARAM].value < 0.5f || indexChannel == 4) {
				// Paste realtime, no pending to schedule
				for (int s = 0; s < 64; s++) {
					cv[indexChannel][s] = cvCPbuffer[s];
					gates[indexChannel][s] = gateCPbuffer[s];
				}
				indexSteps[indexChannel] = stepsCPbuffer;
				if (indexStep[indexChannel] >= stepsCPbuffer)
					indexStep[indexChannel] = stepsCPbuffer - 1;
				pendingPaste = 0;
			}
			else {
				pendingPaste = params[PASTESYNC_PARAM].value > 1.5f ? 2 : 1;
				pendingPaste |= indexChannel<<2; // add paste destination channel into pendingPaste				
			}
		}
			
		// Channel selection
		if (channelTrigger.process(params[CHANNEL_PARAM].value + inputs[CHANNEL_INPUT].value)) {
			indexChannel++;
			if (indexChannel >= 5)
				indexChannel = 0;
		}
		
		// Gate button
		if (gateTrigger.process(params[GATE_PARAM].value)) {
			gates[indexChannel][indexStep[indexChannel]] = !gates[indexChannel][indexStep[indexChannel]];
		}
		
		bool canEdit = !running || (indexChannel == 4);

		// Steps knob
		int newStepsKnob = (int)roundf(params[STEPS_PARAM].value*10.0f);
		if (newStepsKnob != stepsKnob) {
			if (abs(newStepsKnob - stepsKnob) <= 2) // avoid discontinuous step (initialize for example)
				indexSteps[indexChannel] = clamp( indexSteps[indexChannel] + newStepsKnob - stepsKnob, 1, 64); 
			stepsKnob = newStepsKnob;
		}	
		// Step knob
		int newStepKnob = (int)roundf(params[STEP_PARAM].value*19.0f);
		if (newStepKnob != stepKnob) {
			if (canEdit && (abs(newStepKnob - stepKnob) <= 2) ) // avoid discontinuous step (initialize for example)
				indexStep[indexChannel] = moveIndex(indexStep[indexChannel], indexStep[indexChannel] + newStepKnob - stepKnob, indexSteps[indexChannel]);
			stepKnob = newStepKnob;// must do this step whether running or not
		}	
		// If steps knob goes down past step, step knob will not get triggered above, so reduce accordingly
		for (int c = 0; c < 5; c++)
			if (indexStep[c] >= indexSteps[c])
				indexStep[c] = indexSteps[c] - 1;


		// Clock
		bool clk12step = clock12Trigger.process(inputs[CLOCK12_INPUT].value);
		bool clk34step = ((!inputs[CLOCK34_INPUT].active) && clk12step) || 
						  clock34Trigger.process(inputs[CLOCK34_INPUT].value);
		if (running) {
			if (clk12step) {
				indexStep[0] = moveIndex(indexStep[0], indexStep[0] + 1, indexSteps[0]);
				indexStep[1] = moveIndex(indexStep[1], indexStep[1] + 1, indexSteps[1]);
			}
			if (clk34step) {
				indexStep[2] = moveIndex(indexStep[2], indexStep[2] + 1, indexSteps[2]);
				indexStep[3] = moveIndex(indexStep[3], indexStep[3] + 1, indexSteps[3]);
			}	

			// Pending paste on clock or end of seq
			if ( ((pendingPaste&0x3) == 1) || ((pendingPaste&0x3) == 2 && indexStep[indexChannel] == 0) ) {
				if ( (clk12step && (indexChannel == 0 || indexChannel == 1)) ||
					 (clk34step && (indexChannel == 2 || indexChannel == 3)) ) {
					int pasteChannel = pendingPaste>>2;
					for (int s = 0; s < 64; s++) {
						cv[pasteChannel][s] = cvCPbuffer[s];
						gates[pasteChannel][s] = gateCPbuffer[s];
					}
					indexSteps[pasteChannel] = stepsCPbuffer;
					if (indexStep[pasteChannel] >= stepsCPbuffer)
						indexStep[pasteChannel] = stepsCPbuffer - 1;
					pendingPaste = 0;
				}
			}
		}
		
		// Step L
		if (stepLTrigger.process(params[STEPL_PARAM].value + inputs[STEPL_INPUT].value)) {
			if (canEdit) {		
				indexStep[indexChannel] = moveIndex(indexStep[indexChannel], indexStep[indexChannel] - 1, indexSteps[indexChannel]); 
			}
		}
		// Step R
		if (stepRTrigger.process(params[STEPR_PARAM].value + inputs[STEPR_INPUT].value)) {
			if (canEdit) {		
				indexStep[indexChannel] = moveIndex(indexStep[indexChannel], indexStep[indexChannel] + 1, indexSteps[indexChannel]); 
			}
		}
		// Write
		if (writeTrigger.process(params[WRITE_PARAM].value + inputs[WRITE_INPUT].value)) {
			if (canEdit) {		
				// CV
				cv[indexChannel][indexStep[indexChannel]] = quantize(inputs[CV_INPUT].value, params[QUANTIZE_PARAM].value > 0.5f);
				// Gate
				if (inputs[GATE_INPUT].active)
					gates[indexChannel][indexStep[indexChannel]] = (inputs[GATE_INPUT].value >= 1.0f) ? true : false;
				// Autostep
				if (params[AUTOSTEP_PARAM].value > 0.5f)
					indexStep[indexChannel] = moveIndex(indexStep[indexChannel], indexStep[indexChannel] + 1, indexSteps[indexChannel]);
			}
		}
		
		// CV and gate outputs (staging area not used)
		if (running) {
			bool clockHigh = false;
			for (int i = 0; i < 4; i++) {
				// CV
				outputs[CV_OUTPUTS + i].value = cv[i][indexStep[i]];
				
				// Gate
				clockHigh = i < 2 ? clock12Trigger.isHigh() : clock34Trigger.isHigh();
				outputs[GATE_OUTPUTS + i].value = (clockHigh && gates[i][indexStep[i]]) ? 10.0f : 0.0f;
			}
		}
		else {
			bool muteGate = false;// (params[WRITE_PARAM].value + params[STEPL_PARAM].value + params[STEPR_PARAM].value) > 0.5f; // set to false if don't want mute gate on button push
			for (int i = 0; i < 4; i++) {
				// CV
				if (params[MONITOR_PARAM].value > 0.5f)
					outputs[CV_OUTPUTS + i].value = cv[i][indexStep[i]];// each CV out monitors the current step CV of that channel
				else
					outputs[CV_OUTPUTS + i].value = quantize(inputs[CV_INPUT].value, params[QUANTIZE_PARAM].value > 0.5f);// all CV outs monitor the CV in (only current channel will have a gate though)
				
				// Gate
				outputs[GATE_OUTPUTS + i].value = ((i == indexChannel) && !muteGate) ? 10.0f : 0.0f;
			}
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			for (int t = 0; t < 5; t++)
				indexStep[t] = 0;
			resetLight = 1.0f;
			pendingPaste = 0;
		}
		else
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime();
		
		// Gate light
		lights[GATE_LIGHT].value = gates[indexChannel][indexStep[indexChannel]] ? 1.0f : 0.0f;			
		
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	
		
		// Write allowed light
		lights[WRITE_LIGHT + 0].value = (canEdit)?1.0f:0.0f;
		lights[WRITE_LIGHT + 1].value = (canEdit)?0.0f:1.0f;
		
		// Pending paste light
		lights[PENDING_LIGHT].value = (pendingPaste == 0 ? 0.0f : 1.0f);
	}
};


struct WriteSeq64Widget : ModuleWidget {

	struct NoteDisplayWidget : TransparentWidget {
		WriteSeq64 *module;
		std::shared_ptr<Font> font;
		char text[7];

		NoteDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		void cvToStr(void) {
			float cvVal = module->cv[module->indexChannel][module->indexStep[module->indexChannel]];
			if (module->params[WriteSeq64::SHARP_PARAM].value > 0.5f) {// show notes
				float cvValOffset = cvVal +10.0f;// to properly handle negative note voltages
				int indexNote =  clamp(  (int)roundf( (cvValOffset-floor(cvValOffset)) * 12.0f ),  0,  11);
				bool sharp = (module->params[WriteSeq64::SHARP_PARAM].value < 1.5f) ? true : false;
				
				text[0] = ' ';
				
				// note letter
				text[1] = sharp ? noteLettersSharp[indexNote] : noteLettersFlat[indexNote];
				
				// octave number
				int octave = (int) roundf(floorf(cvVal)+4.0f);
				if (octave < 0 || octave > 9)
					text[2] = (octave > 9) ? ':' : '_';
				else
					text[2] = (char) ( 0x30 + octave);
				
				// sharp/flat
				text[3] = ' ';
				if (isBlackKey[indexNote] == 1)
					text[3] = (sharp ? '\"' : '^' );
				
				// end of string
				text[4] = 0;
			}
			else  {// show volts
				float cvValPrint = fabs(cvVal);
				cvValPrint = (cvValPrint > 9.999f) ? 9.999f : cvValPrint;
				snprintf(text, 7, " %4.3f", cvValPrint);// Four-wide, three positions after the decimal, left-justified
				text[0] = (cvVal<0.0f) ? '-' : ' ';
				text[2] = ',';
			}
		}

		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box);
			nvgFontFaceId(vg, font->handle);
			nvgTextLetterSpacing(vg, -1.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			nvgText(vg, textPos.x, textPos.y, "~~~~~~", NULL);
			nvgFillColor(vg, textColor);
			cvToStr();
			nvgText(vg, textPos.x, textPos.y, text, NULL);
		}
	};


	struct StepsDisplayWidget : TransparentWidget {
		WriteSeq64 *module;
		std::shared_ptr<Font> font;
		
		StepsDisplayWidget() {
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
			snprintf(displayStr, 3, "%2u", (unsigned) module->indexSteps[module->indexChannel]);
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};	
	
	
	struct StepDisplayWidget : TransparentWidget {
		WriteSeq64 *module;
		std::shared_ptr<Font> font;
		
		StepDisplayWidget() {
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
			snprintf(displayStr, 3, "%2u", (unsigned) module->indexStep[module->indexChannel] + 1);
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};
	
	
	struct ChannelDisplayWidget : TransparentWidget {
		int *indexTrack;
		std::shared_ptr<Font> font;
		
		ChannelDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}

		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box);
			nvgFontFaceId(vg, font->handle);
			//nvgTextLetterSpacing(vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			nvgText(vg, textPos.x, textPos.y, "~", NULL);
			nvgFillColor(vg, textColor);
			char displayStr[2];
			displayStr[0] = 0x30 + (char) (*indexTrack + 1);
			displayStr[1] = 0;
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};

	
	WriteSeq64Widget(WriteSeq64 *module) : ModuleWidget(module) {
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/WriteSeq64.svg")));

		// Screws
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 365)));

		
		// ****** Top portion ******
		
		static const int rowRulerT0 = 56;
		static const int columnRulerT0 = 22;
		static const int columnRulerT1 = columnRulerT0 + 58;
		static const int columnRulerT2 = columnRulerT1 + 50;
		static const int columnRulerT3 = columnRulerT2 + 43;
		static const int columnRulerT4 = columnRulerT3 + 175;
		
		// Channel display
		ChannelDisplayWidget *channelTrack = new ChannelDisplayWidget();
		channelTrack->box.pos = Vec(columnRulerT0+1, rowRulerT0+vOffsetDisplay);
		channelTrack->box.size = Vec(24, 30);// 1 character
		channelTrack->indexTrack = &module->indexChannel;
		addChild(channelTrack);
		// Step display
		StepDisplayWidget *displayStep = new StepDisplayWidget();
		displayStep->box.pos = Vec(columnRulerT1-8, rowRulerT0+vOffsetDisplay);
		displayStep->box.size = Vec(40, 30);// 2 characters
		displayStep->module = module;
		addChild(displayStep);
		// Gate LED
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(columnRulerT2+offsetLEDbutton+offsetLEDbuttonLight, rowRulerT0+offsetLEDbutton+offsetLEDbuttonLight), module, WriteSeq64::GATE_LIGHT));
		// Note display
		NoteDisplayWidget *displayNote = new NoteDisplayWidget();
		displayNote->box.pos = Vec(columnRulerT3, rowRulerT0+vOffsetDisplay);
		displayNote->box.size = Vec(98, 30);// 6 characters (ex.: "-1,234")
		displayNote->module = module;
		addChild(displayNote);
		// Volt/sharp/flat switch
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(columnRulerT3+114+hOffsetCKSS, rowRulerT0+vOffsetCKSSThree), module, WriteSeq64::SHARP_PARAM, 0.0f, 2.0f, 1.0f));
		// Steps display
		StepsDisplayWidget *displaySteps = new StepsDisplayWidget();
		displaySteps->box.pos = Vec(columnRulerT4-7, rowRulerT0+vOffsetDisplay);
		displaySteps->box.size = Vec(40, 30);// 2 characters
		displaySteps->module = module;
		addChild(displaySteps);

		static const int rowRulerT1 = 105;
		
		// Channel button
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerT0+offsetCKD6, rowRulerT1+offsetCKD6), module, WriteSeq64::CHANNEL_PARAM, 0.0f, 1.0f, 0.0f));
		// Step knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(columnRulerT1+offsetDavies1900, rowRulerT1+offsetDavies1900), module, WriteSeq64::STEP_PARAM, -INFINITY, INFINITY, 0.0f));		
		// Gate button
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerT2-1+offsetCKD6, rowRulerT1+offsetCKD6), module, WriteSeq64::GATE_PARAM , 0.0f, 1.0f, 0.0f));
		// Autostep	
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerT2+53+hOffsetCKSS, rowRulerT1+6+vOffsetCKSS), module, WriteSeq64::AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f));
		// Quantize switch
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerT2+110+hOffsetCKSS, rowRulerT1+6+vOffsetCKSS), module, WriteSeq64::QUANTIZE_PARAM, 0.0f, 1.0f, 1.0f));
		// Reset LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerT2+164+offsetLEDbezel, rowRulerT1+6+offsetLEDbezel), module, WriteSeq64::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerT2+164+offsetLEDbezel+offsetLEDbezelLight, rowRulerT1+6+offsetLEDbezel+offsetLEDbezelLight), module, WriteSeq64::RESET_LIGHT));
		// Steps knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(columnRulerT4+offsetDavies1900, rowRulerT1+offsetDavies1900), module, WriteSeq64::STEPS_PARAM, -INFINITY, INFINITY, 0.0f));		
	
		
		// ****** Bottom portion ******
		
		// Column rulers (horizontal positions)
		static const int columnRuler0 = 25;
		static const int columnnRulerStep = 69;
		static const int columnRuler1 = columnRuler0 + columnnRulerStep;
		static const int columnRuler2 = columnRuler1 + columnnRulerStep;
		static const int columnRuler3 = columnRuler2 + columnnRulerStep;
		static const int columnRuler4 = columnRuler3 + columnnRulerStep;
		static const int columnRuler5 = columnRuler4 + columnnRulerStep - 15;
		
		// Row rulers (vertical positions)
		static const int rowRuler0 = 172;
		static const int rowRulerStep = 49;
		static const int rowRuler1 = rowRuler0 + rowRulerStep;
		static const int rowRuler2 = rowRuler1 + rowRulerStep;
		static const int rowRuler3 = rowRuler2 + rowRulerStep;
		
		// Column 0 
		// Copy/paste switches
		addParam(ParamWidget::create<TL1105>(Vec(columnRuler0-10, rowRuler0+offsetTL1105), module, WriteSeq64::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(columnRuler0+20, rowRuler0+offsetTL1105), module, WriteSeq64::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		// Paste sync (and light)
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(columnRuler0+hOffsetCKSS, rowRuler1+vOffsetCKSSThree), module, WriteSeq64::PASTESYNC_PARAM, 0.0f, 2.0f, 0.0f));	
		addChild(ModuleLightWidget::create<SmallLight<RedLight>>(Vec(columnRuler0 + 41, rowRuler1 + 14), module, WriteSeq64::PENDING_LIGHT));
		// Reset
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler0, rowRuler2), Port::INPUT, module, WriteSeq64::RESET_INPUT));		
		// Channel input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler0, rowRuler3), Port::INPUT, module, WriteSeq64::CHANNEL_INPUT));
		
		
		// Column 1
		// Step L button
		addParam(ParamWidget::create<CKD6>(Vec(columnRuler1+offsetCKD6, rowRuler0+offsetCKD6), module, WriteSeq64::STEPL_PARAM, 0.0f, 1.0f, 0.0f));
		// Run LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRuler1+offsetLEDbezel, rowRuler1+offsetLEDbezel), module, WriteSeq64::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRuler1+offsetLEDbezel+offsetLEDbezelLight, rowRuler1+offsetLEDbezel+offsetLEDbezelLight), module, WriteSeq64::RUN_LIGHT));
		// Gate input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler1, rowRuler2), Port::INPUT, module, WriteSeq64::GATE_INPUT));				
		// Step L input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler1, rowRuler3), Port::INPUT, module, WriteSeq64::STEPL_INPUT));
		
		
		// Column 2
		// Step R button
		addParam(ParamWidget::create<CKD6>(Vec(columnRuler2+offsetCKD6, rowRuler0+offsetCKD6), module, WriteSeq64::STEPR_PARAM, 0.0f, 1.0f, 0.0f));	
		// Write button and light
		addParam(ParamWidget::create<CKD6>(Vec(columnRuler2+offsetCKD6, rowRuler1+offsetCKD6), module, WriteSeq64::WRITE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<SmallLight<GreenRedLight>>(Vec(columnRuler2 -12, rowRuler1 - 13), module, WriteSeq64::WRITE_LIGHT));
		// CV input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler2, rowRuler2), Port::INPUT, module, WriteSeq64::CV_INPUT));
		// Step R input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler2, rowRuler3), Port::INPUT, module, WriteSeq64::STEPR_INPUT));
		
		
		// Column 3
		// Clocks
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler3, rowRuler0), Port::INPUT, module, WriteSeq64::CLOCK12_INPUT));		
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler3, rowRuler1), Port::INPUT, module, WriteSeq64::CLOCK34_INPUT));		
		// Monitor (temporary before delete)
		addParam(ParamWidget::create<CKSSH>(Vec(columnRuler3+hOffsetCKSSH, rowRuler2+vOffsetCKSSH), module, WriteSeq64::MONITOR_PARAM, 0.0f, 1.0f, 0.0f));
		// Write input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler3, rowRuler3), Port::INPUT, module, WriteSeq64::WRITE_INPUT));
		
					
		// Column 4 (CVs)
		// Outputs
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler4, rowRuler0), Port::OUTPUT, module, WriteSeq64::CV_OUTPUTS + 0));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler4, rowRuler1), Port::OUTPUT, module, WriteSeq64::CV_OUTPUTS + 1));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler4, rowRuler2), Port::OUTPUT, module, WriteSeq64::CV_OUTPUTS + 2));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler4, rowRuler3), Port::OUTPUT, module, WriteSeq64::CV_OUTPUTS + 3));
		
		
		// Column 5 (Gates)
		// Gates
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler5, rowRuler0), Port::OUTPUT, module, WriteSeq64::GATE_OUTPUTS + 0));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler5, rowRuler1), Port::OUTPUT, module, WriteSeq64::GATE_OUTPUTS + 1));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler5, rowRuler2), Port::OUTPUT, module, WriteSeq64::GATE_OUTPUTS + 2));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler5, rowRuler3), Port::OUTPUT, module, WriteSeq64::GATE_OUTPUTS + 3));
	}
};

Model *modelWriteSeq64 = Model::create<WriteSeq64, WriteSeq64Widget>("Impromptu Modular", "Write-Seq-64", "Write-Seq-64", SEQUENCER_TAG);
