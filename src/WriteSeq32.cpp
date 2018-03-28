//***********************************************************************************************
//Three channel 32-step writable sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct WriteSeq32 : Module {
	enum ParamIds {
		SHARP_PARAM,
		ENUMS(WINDOW_PARAM, 4),
		QUANTIZE_PARAM,
		ENUMS(GATE_PARAM, 8),
		CHANNEL_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RUN_PARAM,
		WRITE_PARAM,
		STEPL_PARAM,
		MONITOR_PARAM,
		STEPR_PARAM,
		STEPS_PARAM,
		AUTOSTEP_PARAM,
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
		CLOCK_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 3),
		ENUMS(GATE_OUTPUTS, 3),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(WINDOW_LIGHTS, 4),
		ENUMS(STEP_LIGHTS, 8),
		ENUMS(GATE_LIGHTS, 8),
		ENUMS(CHANNEL_LIGHTS, 4 * 3),// room for GreenRedBlue
		RUN_LIGHT,
		ENUMS(WRITE_LIGHT, 2),// room for GreenRed
		NUM_LIGHTS
	};

	// Need to save
	bool running;
	int indexStep;
	int indexStepStage;
	int indexChannel;
	float cv[4][32] = {};
	bool gates[4][32] = {};

	// No need to save
	int notesPos[8]; // used for rendering notes in LCD_24, 8 gate and 8 step LEDs 
	float cvCPbuffer[32];// copy paste buffer for CVs
	bool gateCPbuffer[32];// copy paste buffer for gates
	int pendingPaste;// 0 = nothing to paste, 1 = paste on clk, 2 = paste on seq

	SchmittTrigger clockTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger channelTrigger;
	SchmittTrigger stepLTrigger;
	SchmittTrigger stepRTrigger;
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger writeTrigger;
	SchmittTrigger gateTriggers[8];
	SchmittTrigger windowTriggers[4];

	
	WriteSeq32() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		running = true;
		indexStep = 0;
		indexStepStage = 0;
		indexChannel = 0;
		for (int s = 0; s < 32; s++) {
			for (int c = 0; c < 4; c++) {
				cv[c][s] = 0.0f;
				gates[c][s] = true;
			}
			cvCPbuffer[s] = 0.0f;
			gateCPbuffer[s] = true;
		}
		pendingPaste = 0;
	}

	void onRandomize() override {
		running = true;
		indexStep = 0;
		indexStepStage = 0;
		indexChannel = 0;
		for (int s = 0; s < 32; s++) {
			for (int c = 0; c < 4; c++) {
				cv[c][s] = (randomUniform() *10.0f);
				gates[c][s] = (randomUniform() > 0.5f);
			}
			cvCPbuffer[s] = 0.0f;
			gateCPbuffer[s] = true;
		}
		pendingPaste = 0;
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// indexStep, indexStepStage, indexChannel
		json_object_set_new(rootJ, "indexStep", json_integer(indexStep));
		json_object_set_new(rootJ, "indexStepStage", json_integer(indexStepStage));
		json_object_set_new(rootJ, "indexChannel", json_integer(indexChannel));

		// CV
		json_t *cvJ = json_array();
		for (int c = 0; c < 4; c++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(cvJ, s + (c<<5), json_real(cv[c][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// Gates
		json_t *gatesJ = json_array();
		for (int c = 0; c < 4; c++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(gatesJ, s + (c<<5), json_integer((int) gates[c][s]));
			}
		json_object_set_new(rootJ, "gates", gatesJ);

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
		
		// indexStep, indexStepStage, indexChannel
		json_t *indexStepJ = json_object_get(rootJ, "indexStep");
		if (indexStepJ)
			indexStep = json_integer_value(indexStepJ);
		json_t *indexStepStageJ = json_object_get(rootJ, "indexStepStage");
		if (indexStepStageJ)
			indexStepStage = json_integer_value(indexStepStageJ);
		json_t *indexChannelJ = json_object_get(rootJ, "indexChannel");
		if (indexChannelJ)
			indexChannel = json_integer_value(indexChannelJ);

		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int c = 0; c < 4; c++)
				for (int s = 0; s < 32; s++) {
					json_t *cvArrayJ = json_array_get(cvJ, s + (c<<5));
					if (cvArrayJ)
						cv[c][s] = json_real_value(cvArrayJ);
				}
		}
		
		// Gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int c = 0; c < 4; c++)
				for (int s = 0; s < 32; s++) {
					json_t *gateJ = json_array_get(gatesJ, s + (c<<5));
					if (gateJ)
						gates[c][s] = !!json_integer_value(gateJ);
				}
		}
	}

	
	inline float quantize(float cv, bool enable) {
		return enable ? (roundf(cv * 12.0f) / 12.0f) : cv;
	}
	
	int moveIndex(int index, int indexNext, int numSteps) {
		if (indexNext < 0)
			index = numSteps - 1;
		else
		{
			if (indexNext - index >= 0) { // if moving right or same place
				if (indexNext >= numSteps)
					index = 0;
				else
					index = indexNext;
			}
			else { // moving left 
				if (indexNext >= numSteps)
					index = numSteps - 1;
				else
					index = indexNext;
			}
		}
		return index;
	}
	
	void step() override {
		int numSteps = (int) clamp(roundf(params[STEPS_PARAM].value), 1.0f, 32.0f);	
		
		// Run state and light
		if (runningTrigger.process(params[RUN_PARAM].value)) {
			running = !running;
			pendingPaste = 0;// no pending pastes across run state toggles
		}
		lights[RUN_LIGHT].value = (running);
		
		// Copy
		if (copyTrigger.process(params[COPY_PARAM].value)) {
			for (int s = 0; s < 32; s++) {
				cvCPbuffer[s] = cv[indexChannel][s];
				gateCPbuffer[s] = gates[indexChannel][s];
			}
		}
		// Paste
		if (pasteTrigger.process(params[PASTE_PARAM].value)) {
			if (params[PASTESYNC_PARAM].value < 0.5f || indexChannel == 3) {
				// Paste realtime, no pending to schedule
				for (int s = 0; s < 32; s++) {
					cv[indexChannel][s] = cvCPbuffer[s];
					gates[indexChannel][s] = gateCPbuffer[s];
				}
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
			if (indexChannel >= 4)
				indexChannel = 0;
		}
		
		// Gate buttons
		for (int i =0, iGate = 0; i < 8; i++) {
			if (gateTriggers[i].process(params[GATE_PARAM + i].value)) {
				iGate = ( (indexChannel == 3 ? indexStepStage : indexStep) & 0x18) | i;
				gates[indexChannel][iGate] = !gates[indexChannel][iGate];
			}
		}

		bool canEdit = !running || (indexChannel == 3);
			
		// Steps knob will not trigger anything in step(), and if user goes lower than current step, lower the index accordingly
		if (indexStep >= numSteps)
			indexStep = numSteps - 1;
		if (indexStepStage >= numSteps)
			indexStepStage = numSteps - 1;
		
		if (running)
		{
			// Clock
			if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
				indexStep = moveIndex(indexStep, indexStep + 1, numSteps);
				
				// Pending paste on clock or end of seq
				if ( ((pendingPaste&0x3) == 1) || ((pendingPaste&0x3) == 2 && indexStep == 0) ) {
					int pasteChannel = pendingPaste>>2;
					for (int s = 0; s < 32; s++) {
						cv[pasteChannel][s] = cvCPbuffer[s];
						gates[pasteChannel][s] = gateCPbuffer[s];
					}
					pendingPaste = 0;
				}
			}
		}
		
		if (canEdit)
		{		
			// Step L
			if (stepLTrigger.process(params[STEPL_PARAM].value + inputs[STEPL_INPUT].value)) {
				if (indexChannel == 3)
					indexStepStage = moveIndex(indexStepStage, indexStepStage - 1, numSteps);
				else 
					indexStep = moveIndex(indexStep, indexStep - 1, numSteps);
			}
			// Step R
			if (stepRTrigger.process(params[STEPR_PARAM].value + inputs[STEPR_INPUT].value)) {
				if (indexChannel == 3)
					indexStepStage = moveIndex(indexStepStage, indexStepStage + 1, numSteps);
				else 
					indexStep = moveIndex(indexStep, indexStep + 1, numSteps);
			}
			// Window
			for (int i = 0; i < 4; i++) {
				if (windowTriggers[i].process(params[WINDOW_PARAM+i].value)) {
					if (indexChannel == 3)
						indexStepStage = (i<<3) | (indexStepStage&0x7);
					else 
						indexStep = (i<<3) | (indexStep&0x7);
				}
			}
			// Write
			if (writeTrigger.process(params[WRITE_PARAM].value + inputs[WRITE_INPUT].value)) {
				int index = (indexChannel == 3 ? indexStepStage : indexStep);
				// CV
				cv[indexChannel][index] = quantize(inputs[CV_INPUT].value, params[QUANTIZE_PARAM].value > 0.5f);
				// Gate
				if (inputs[GATE_INPUT].active)
					gates[indexChannel][index] = (inputs[GATE_INPUT].value >= 1.0f) ? true : false;
				// Autostep
				if (params[AUTOSTEP_PARAM].value > 0.5f) {
					if (indexChannel == 3)
						indexStepStage = moveIndex(indexStepStage, indexStepStage + 1, numSteps);
					else 
						indexStep = moveIndex(indexStep, indexStep + 1, numSteps);
				}
			}
		}
		
		// CV and gate outputs (staging area not used)
		if (running) {
			for (int i = 0; i < 3; i++) {
				outputs[CV_OUTPUTS + i].value = cv[i][indexStep];
				outputs[GATE_OUTPUTS + i].value = (clockTrigger.isHigh() && gates[i][indexStep]) ? 10.0f : 0.0f;
			}
		}
		else {			
			bool muteGate = false;// (params[WRITE_PARAM].value + params[STEPL_PARAM].value + params[STEPR_PARAM].value) > 0.5f; // set to false if don't want mute gate on button push
			for (int i = 0; i < 3; i++) {
				// CV
				if (params[MONITOR_PARAM].value > 0.5f)
					outputs[CV_OUTPUTS + i].value = cv[i][indexStep];// each CV out monitors the current step CV of that channel
				else
					outputs[CV_OUTPUTS + i].value = quantize(inputs[CV_INPUT].value, params[QUANTIZE_PARAM].value > 0.5f);// all CV outs monitor the CV in (only current channel will have a gate though)
				
				// Gate
				outputs[GATE_OUTPUTS + i].value = ((i == indexChannel) && !muteGate) ? 10.0f : 0.0f;
			}
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value)) {
			indexStep = 0;
			indexStepStage = 0;	
		}

		int index = (indexChannel == 3 ? indexStepStage : indexStep);
		// Window lights
		for (int i = 0; i < 4; i++) {
			lights[WINDOW_LIGHTS + i].value = ((i == (index >> 3))?1.0f:0.0f);
		}
		// Step and gate lights
		for (int i = 0; i < 8; i++) {
			lights[STEP_LIGHTS + i].value = (i == (index&0x7)) ? 1.0f : 0.0f;
			lights[GATE_LIGHTS + i].value = gates[indexChannel][(index&0x18) | i] ? 1.0f : 0.0f;
		}
			
		// Channel lights (with pendingPaste state)
		setRGBLight(CHANNEL_LIGHTS + 0, 0.0f, 1.0f, 0.0f, (indexChannel == 0));// green
		setRGBLight(CHANNEL_LIGHTS + 3, 0.4f, 0.5f, 0.0f, (indexChannel == 1));// orange
		setRGBLight(CHANNEL_LIGHTS + 6, 0.0f, 0.5f, 0.4f, (indexChannel == 2));// turquoise
		setRGBLight(CHANNEL_LIGHTS + 9, 0.0f, 0.0f, 0.8f, (indexChannel == 3));// blue
		if (pendingPaste != 0)
			setRGBLight(CHANNEL_LIGHTS + (pendingPaste>>2)*3, 1.0f, 0.0f, 0.0f, true);
		
		// Write allowed light
		lights[WRITE_LIGHT + 0].value = (canEdit)?1.0f:0.0f;
		lights[WRITE_LIGHT + 1].value = (canEdit)?0.0f:1.0f;
	}
	
	void setRGBLight(int id, float red, float green, float blue, bool enable) {
		lights[id + 0].value = enable? red : 0.0f;
		lights[id + 1].value = enable? green : 0.0f;
		lights[id + 2].value = enable? blue : 0.0f;
	}
};


struct WriteSeq32Widget : ModuleWidget {

	struct NotesDisplayWidget : TransparentWidget {
		WriteSeq32 *module;
		std::shared_ptr<Font> font;
		char text[4];

		NotesDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		void cvToStr(int index8) {
			int index = (module->indexChannel == 3 ? module->indexStepStage : module->indexStep);
			float cvVal = module->cv[module->indexChannel][index8|(index&0x18)];
			//if (module->params[WriteSeq32::QUANTIZE_PARAM].value) {
				float cvValOffset = cvVal +10.0f;//to properly handle negative note voltages
				int indexNote = (int) clamp(  roundf( (cvValOffset-floor(cvValOffset)) * 12.0f ),  0.0f,  11.0f);
				bool sharp = (module->params[WriteSeq32::SHARP_PARAM].value > 0.5f) ? true : false;
				
				// note letter
				text[0] = sharp ? noteLettersSharp[indexNote] : noteLettersFlat[indexNote];
				
				// octave number
				int octave = (int) roundf(floorf(cvVal)+4.0f);
				if (octave < 0 || octave > 9)
					text[1] = (octave > 9) ? ':' : '_';
				else
					text[1] = (char) ( 0x30 + octave);
				
				// sharp/flat
				text[2] = ' ';
				if (isBlackKey[indexNote] == 1)
					text[2] = (sharp ? '\"' : '^' );
				
				// end of string
				text[3] = 0;
			//}
			//else  // show first two decimals of fractional part
			//	snprintf(text, 4, "%2u ", (unsigned)((cvVal-floorf(cvVal))*100.0f));
		}

		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box);
			nvgFontFaceId(vg, font->handle);
			nvgTextLetterSpacing(vg, -1.5);

			for (int i = 0; i < 8; i++) {
				Vec textPos = Vec(module->notesPos[i], 24);
				nvgFillColor(vg, nvgTransRGBA(textColor, 16));
				nvgText(vg, textPos.x, textPos.y, "~~~", NULL);
				nvgFillColor(vg, textColor);
				cvToStr(i);
				nvgText(vg, textPos.x, textPos.y, text, NULL);
			}
		}
	};


	struct StepsDisplayWidget : TransparentWidget {
		float *valueKnob;
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
			snprintf(displayStr, 3, "%2u", (unsigned) clamp(roundf(*valueKnob), 1.0f, 32.0f) );
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};

	
	WriteSeq32Widget(WriteSeq32 *module) : ModuleWidget(module) {

		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/WriteSeq32.svg")));

		// Screws
		addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));

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
		static const int rowRuler3 = rowRuler2 + rowRulerStep + 4;


		// ****** Top portion ******
		
		static const int yRulerTopLEDs = 42;
		static const int yRulerTopSwitches = yRulerTopLEDs-11;
		
		// Autostep, sharp/flat and quantize switches
		// Autostep	
		addParam(ParamWidget::create<CKSS>(Vec(columnRuler0+3+hOffsetCKSS, yRulerTopSwitches+vOffsetCKSS), module, WriteSeq32::AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f));
		// Sharp/flat
		addParam(ParamWidget::create<CKSS>(Vec(columnRuler4+hOffsetCKSS, yRulerTopSwitches+vOffsetCKSS), module, WriteSeq32::SHARP_PARAM, 0.0f, 1.0f, 1.0f));
		// Quantize
		addParam(ParamWidget::create<CKSS>(Vec(columnRuler5+hOffsetCKSS, yRulerTopSwitches+vOffsetCKSS), module, WriteSeq32::QUANTIZE_PARAM, 0.0f, 1.0f, 1.0f));

		// Window LED buttons
		static const float wLightsPosX = 140.0f;
		static const float wLightsIntX = 35.0f;
		for (int i = 0; i < 4; i++) {
			addParam(ParamWidget::create<LEDButton>(Vec(wLightsPosX + i * wLightsIntX, yRulerTopLEDs - 4.4f), module, WriteSeq32::WINDOW_PARAM + i, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(wLightsPosX + 4.4f + i * wLightsIntX, yRulerTopLEDs), module, WriteSeq32::WINDOW_LIGHTS + i));
		}
		
		// Prepare 8 positions for step lights, gate lights and notes display
		module->notesPos[0] = 9;// this is also used to help line up LCD digits with LEDbuttons and avoid bad horizontal scaling with long str in display  
		for (int i = 1; i < 8; i++) {
			module->notesPos[i] = module->notesPos[i-1] + 46;
		}

		// Notes display
		NotesDisplayWidget *displayNotes = new NotesDisplayWidget();
		displayNotes->box.pos = Vec(12, 76);
		displayNotes->box.size = Vec(381, 30);
		displayNotes->module = module;
		addChild(displayNotes);

		// Step LEDs (must be done after Notes display such that LED glow will overlay the notes display
		static const int yRulerStepLEDs = 65;
		for (int i = 0; i < 8; i++) {
			addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(module->notesPos[i]+25.0f+1.5f, yRulerStepLEDs), module, WriteSeq32::STEP_LIGHTS + i));
		}

		// Gates LED buttons
		static const int yRulerT2 = 119.0f;
		for (int i = 0; i < 8; i++) {
			addParam(ParamWidget::create<LEDButton>(Vec(module->notesPos[i]+25.0f-4.4f, yRulerT2-4.4f), module, WriteSeq32::GATE_PARAM + i, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(module->notesPos[i]+25.0f, yRulerT2), module, WriteSeq32::GATE_LIGHTS + i));
		}
		
		
		// ****** Bottom portion ******
		
		// Column 0
		// Channel button
		addParam(ParamWidget::create<CKD6>(Vec(columnRuler0+offsetCKD6, rowRuler0+offsetCKD6), module, WriteSeq32::CHANNEL_PARAM, 0.0f, 1.0f, 0.0f));
		// Channel LEDS
		static const int chanLEDoffsetX = 25;
		static const int chanLEDoffsetY[4] = {-20, -8, 4, 16};
		for (int i = 0; i < 4; i++) {
			addChild(ModuleLightWidget::create<MediumLight<RedGreenBlueLight>>(Vec(columnRuler0 + chanLEDoffsetX + offsetMediumLight, rowRuler0 + chanLEDoffsetY[i] + offsetMediumLight), module, WriteSeq32::CHANNEL_LIGHTS + (i*3)));
		}
		// Copy/paste switches
		addParam(ParamWidget::create<TL1105>(Vec(columnRuler0-10, rowRuler1+offsetTL1105), module, WriteSeq32::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(columnRuler0+20, rowRuler1+offsetTL1105), module, WriteSeq32::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		// Paste sync
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(columnRuler0+hOffsetCKSS, rowRuler2+vOffsetCKSSThree), module, WriteSeq32::PASTESYNC_PARAM, 0.0f, 2.0f, 0.0f));		
		// Channel input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler0, rowRuler3), Port::INPUT, module, WriteSeq32::CHANNEL_INPUT));
		
		
		// Column 1
		// Step L button
		addParam(ParamWidget::create<CKD6>(Vec(columnRuler1+offsetCKD6, rowRuler0+offsetCKD6), module, WriteSeq32::STEPL_PARAM, 0.0f, 1.0f, 0.0f));
		// Run LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRuler1+offsetLEDbezel, rowRuler1+offsetLEDbezel), module, WriteSeq32::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRuler1+offsetLEDbezel+offsetLEDbezelLight, rowRuler1+offsetLEDbezel+offsetLEDbezelLight), module, WriteSeq32::RUN_LIGHT));
		// Gate input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler1, rowRuler2), Port::INPUT, module, WriteSeq32::GATE_INPUT));		
		// Step L input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler1, rowRuler3), Port::INPUT, module, WriteSeq32::STEPL_INPUT));
		
		
		// Column 2
		// Step R button
		addParam(ParamWidget::create<CKD6>(Vec(columnRuler2+offsetCKD6, rowRuler0+offsetCKD6), module, WriteSeq32::STEPR_PARAM, 0.0f, 1.0f, 0.0f));	
		// Write button and light
		addParam(ParamWidget::create<CKD6>(Vec(columnRuler2+offsetCKD6, rowRuler1+offsetCKD6), module, WriteSeq32::WRITE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<SmallLight<GreenRedLight>>(Vec(columnRuler2 -12, rowRuler1 - 13), module, WriteSeq32::WRITE_LIGHT));
		// CV input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler2, rowRuler2), Port::INPUT, module, WriteSeq32::CV_INPUT));		
		// Step R input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler2, rowRuler3), Port::INPUT, module, WriteSeq32::STEPR_INPUT));
		
		
		// Column 3
		// Steps display
		StepsDisplayWidget *displaySteps = new StepsDisplayWidget();
		displaySteps->box.pos = Vec(columnRuler3-7, rowRuler0+vOffsetDisplay);
		displaySteps->box.size = Vec(40, 30);// 2 characters
		displaySteps->valueKnob = &module->params[WriteSeq32::STEPS_PARAM].value;
		addChild(displaySteps);
		// Steps knob and cv input
		addParam(ParamWidget::create<Davies1900hBlackSnapKnob>(Vec(columnRuler3+offsetDavies1900, rowRuler1+offsetDavies1900), module, WriteSeq32::STEPS_PARAM, 1.0f, 32.0f, 32.0f));		
		// Monitor
		addParam(ParamWidget::create<CKSSH>(Vec(columnRuler3+hOffsetCKSSH, rowRuler2+vOffsetCKSSH), module, WriteSeq32::MONITOR_PARAM, 0.0f, 1.0f, 0.0f));		
		// Write input
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler3, rowRuler3), Port::INPUT, module, WriteSeq32::WRITE_INPUT));
		
		
		// Column 4
		// Outputs
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler4, rowRuler0), Port::OUTPUT, module, WriteSeq32::CV_OUTPUTS + 0));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler4, rowRuler1), Port::OUTPUT, module, WriteSeq32::CV_OUTPUTS + 1));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler4, rowRuler2), Port::OUTPUT, module, WriteSeq32::CV_OUTPUTS + 2));
		// Reset
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler4, rowRuler3), Port::INPUT, module, WriteSeq32::RESET_INPUT));		

		
		// Column 5
		// Gates
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler5, rowRuler0), Port::OUTPUT, module, WriteSeq32::GATE_OUTPUTS + 0));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler5, rowRuler1), Port::OUTPUT, module, WriteSeq32::GATE_OUTPUTS + 1));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRuler5, rowRuler2), Port::OUTPUT, module, WriteSeq32::GATE_OUTPUTS + 2));
		// Clock
		addInput(Port::create<PJ301MPortS>(Vec(columnRuler5, rowRuler3), Port::INPUT, module, WriteSeq32::CLOCK_INPUT));			
	}
};

Model *modelWriteSeq32 = Model::create<WriteSeq32, WriteSeq32Widget>("Impromptu Modular", "Write-Seq-32", "Write-Seq-32", SEQUENCER_TAG);
