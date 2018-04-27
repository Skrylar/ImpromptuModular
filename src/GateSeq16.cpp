//***********************************************************************************************
//Gate sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept by Nigel Sixsmith and Marc Boulé
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct GateSeq16 : Module {
	enum ParamIds {
		ENUMS(STEP_PARAMS, 64),
		GATE_PARAM,
		LENGTH_PARAM,
		GATEP_PARAM,
		MODE_PARAM,
		GATETRIG_PARAM,
		ENUMS(RUN_PARAMS, 4),
		PROB_PARAM,
		CONFIG_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RUNCV_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(CLOCK_INPUTS, 4),
		RESET_INPUT,
		ENUMS(RUNCV_INPUTS, 4),
		PROBCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(GATE_OUTPUTS, 4),
		ENUMS(EOC_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_LIGHTS, 64 * 3),// room for GreenRedBlue
		ENUMS(GATE_LIGHT, 3),// room for GreenRedBlue
		ENUMS(LENGTH_LIGHT, 3),// room for GreenRedBlue
		ENUMS(GATEP_LIGHT, 3),// room for GreenRedBlue
		ENUMS(MODE_LIGHT, 3),// room for GreenRedBlue
		ENUMS(GATETRIG_LIGHT, 3),// room for GreenRedBlue
		ENUMS(RUN_LIGHTS, 4),
		NUM_LIGHTS
	};
	
	enum DisplayStateIds {DISP_GATE, DISP_LENGTH, DISP_GATEP, DISP_MODE, DISP_GATETRIG, DISP_COPY, DISP_PASTE};
	enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_BRN, MODE_RND, NUM_MODES};
	
	// Need to save
	bool gate[64] = {};
	int length[4] = {};// values are 1 to 16
	bool gatep[64] = {};
	int mode[4] = {};
	bool trig[4] = {};

	// No need to save
	int displayState;


	SchmittTrigger gateTrigger;
	SchmittTrigger lengthTrigger;
	SchmittTrigger gatePTrigger;
	SchmittTrigger modeTrigger;
	SchmittTrigger gateTrigTrigger;
	SchmittTrigger stepTriggers[64];
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;

		
	GateSeq16() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		displayState = DISP_GATE;
		for (int i = 0; i < 4; i++) {
			length[i] = 16;
			mode[i] = MODE_FWD;
			trig[i] = false;
		}
		for (int i = 0; i < 64; i++) {
			gate[i] = false;
			gatep[i] = false;
		}
	}

	void onRandomize() override {
		// TODO
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// TODO
		
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// TODO
	}


	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		
		// Gate button
		if (gateTrigger.process(params[GATE_PARAM].value)) {
			displayState = DISP_GATE;
		}
		
		// Length button
		if (lengthTrigger.process(params[LENGTH_PARAM].value)) {
			if (displayState != DISP_LENGTH)
				displayState = DISP_LENGTH;
			else
				displayState = DISP_GATE;
		}
		// GateP button
		if (gatePTrigger.process(params[GATEP_PARAM].value)) {
			if (displayState != DISP_GATEP)
				displayState = DISP_GATEP;
			else
				displayState = DISP_GATE;
		}
		// Mode button
		if (modeTrigger.process(params[MODE_PARAM].value)) {
			if (displayState != DISP_MODE)
				displayState = DISP_MODE;
			else
				displayState = DISP_GATE;
		}
		// GateTrig button
		if (gateTrigTrigger.process(params[GATETRIG_PARAM].value)) {
			if (displayState != DISP_GATETRIG)
				displayState = DISP_GATETRIG;
			else
				displayState = DISP_GATE;
		}
		// Copy button
		if (copyTrigger.process(params[COPY_PARAM].value)) {
			if (displayState != DISP_COPY)
				displayState = DISP_COPY;
			else
				displayState = DISP_GATE;
		}
		// Paste button
		if (pasteTrigger.process(params[PASTE_PARAM].value)) {
			if (displayState != DISP_PASTE)
				displayState = DISP_PASTE;
			else
				displayState = DISP_GATE;
		}
	
		// Step LED buttons
		for (int i = 0; i < 64; i++) {
			if (stepTriggers[i].process(params[STEP_PARAMS + i].value)) {
				if (displayState == DISP_GATE) {
					gate[i] = !gate[i];
				}
				if (displayState == DISP_LENGTH) {
					length[i / 16] = (i % 16) + 1;
				}
				if (displayState == DISP_GATEP) {
					gatep[i] = !gatep[i];
				}
				if (displayState == DISP_MODE) {
					// TODO
				}
				if (displayState == DISP_GATETRIG) {
					// TODO
				}
				if (displayState == DISP_COPY) {
					// TODO
				}
				if (displayState == DISP_PASTE) {
					// TODO
				}

			}
		}
		
		
		// Step lights
		if (displayState == DISP_GATE) {
			for (int i = 0; i < 64; i++) {
				setRGBLight(STEP_LIGHTS + i * 3, 1.0f, 0.0f, 0.0f, gate[i]);// red
			}
		}
		else if (displayState == DISP_LENGTH) {
			for (int i = 0; i < 64; i++) {
				int row = i / 16;
				int col = i % 16;
				if (col < (length[row] - 1))
					setRGBLight(STEP_LIGHTS + i * 3, 0.0f, 0.1f, 0.0f, true);// pale green
				else if (col == (length[row] - 1))
					setRGBLight(STEP_LIGHTS + i * 3, 0.0f, 1.0f, 0.0f, true);// green
				else 
					setRGBLight(STEP_LIGHTS + i * 3, 0.0f, 0.0f, 0.0f, true);// off
			}
		}
		else if (displayState == DISP_GATEP) {
			for (int i = 0; i < 64; i++) {
				setRGBLight(STEP_LIGHTS + i * 3, 0.0f, 1.0f, 0.0f, gatep[i]);// green
			}
		}
		else if (displayState == DISP_MODE) {
			for (int i = 0; i < 64; i++) {
				// TODO
			}
		}
		else if (displayState == DISP_GATETRIG) {
			for (int i = 0; i < 64; i++) {
				// TODO
			}
		}
		else if (displayState == DISP_COPY) {
			for (int i = 0; i < 64; i++) {
				// TODO
			}
		}
		else if (displayState == DISP_PASTE) {
			for (int i = 0; i < 64; i++) {
				// TODO
			}
		}
		else {
			setRGBLight(GATE_LIGHT,     0.0f, 0.0f, 0.0f, true);// should never happen
		}
		
		
		// Main button lights
		setRGBLight(GATE_LIGHT,     0.0f, 1.0f, 0.0f, displayState == DISP_GATE);
		setRGBLight(LENGTH_LIGHT,   0.0f, 1.0f, 0.0f, displayState == DISP_LENGTH);
		setRGBLight(GATEP_LIGHT,    1.0f, 0.0f, 0.0f, displayState == DISP_GATEP);
		setRGBLight(MODE_LIGHT,     0.0f, 0.0f, 0.8f, displayState == DISP_MODE);// blue		
		setRGBLight(GATETRIG_LIGHT, 0.0f, 0.5f, 0.4f, displayState == DISP_GATETRIG);// turquoise
		
	}
	
	void setRGBLight(int id, float red, float green, float blue, bool enable) {
		lights[id + 0].value = enable? red : 0.0f;
		lights[id + 1].value = enable? green : 0.0f;
		lights[id + 2].value = enable? blue : 0.0f;
	}

};


struct GateSeq16Widget : ModuleWidget {
		
	GateSeq16Widget(GateSeq16 *module) : ModuleWidget(module) {
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/GateSeq16.svg")));

		// Screw holes (optical illustion makes screws look oval, remove for now)
		/*addChild(new ScrewHole(Vec(15, 0)));
		addChild(new ScrewHole(Vec(box.size.x-30, 0)));
		addChild(new ScrewHole(Vec(15, 365)));
		addChild(new ScrewHole(Vec(box.size.x-30, 365)));*/
		
		// Screws
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 365)));

		
		
		// ****** Outputs and clock inputs ******
		
		static const int rowRuler0 = 43;
		static const int colRuler0 = 20;
		static const int colRuler6 = 406;
		static const int rowSpacingOutputs = 40;
		
		// Outputs
		int iOutputs = 0;
		for (; iOutputs < 4; iOutputs++) {
			addOutput(Port::create<PJ301MPortS>(Vec(colRuler6, rowRuler0 + iOutputs * rowSpacingOutputs), Port::OUTPUT, module, GateSeq16::GATE_OUTPUTS + iOutputs));
		}
		for (; iOutputs < 8; iOutputs++) {
			addOutput(Port::create<PJ301MPortS>(Vec(colRuler6, rowRuler0 + iOutputs * rowSpacingOutputs + 5), Port::OUTPUT, module, GateSeq16::EOC_OUTPUTS + iOutputs));
		}
		
		// Clock inputs
		for (int i = 0; i < 4; i++) {
			addInput(Port::create<PJ301MPortS>(Vec(colRuler0, rowRuler0 + i * rowSpacingOutputs), Port::INPUT, module, GateSeq16::CLOCK_INPUTS + i));
		}
		
		
		
		// ****** Steps LED buttons ******
		
		static int colRulerSteps = 60;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		
		for (int y = 0; y < 4; y++) {
			int posX = colRulerSteps;
			for (int x = 0; x < 16; x++) {
				addParam(ParamWidget::create<LEDButton>(Vec(posX, rowRuler0 + 8 + y * rowSpacingOutputs - 4.4f), module, GateSeq16::STEP_PARAMS + y * 16 + x, 0.0f, 1.0f, 0.0f));
				addChild(ModuleLightWidget::create<MediumLight<RedGreenBlueLight>>(Vec(posX + 4.4f, rowRuler0 + 8 + y * rowSpacingOutputs), module, GateSeq16::STEP_LIGHTS + (y * 16 + x) * 3));
				posX += spacingSteps;
				if ((x + 1) % 4 == 0)
					posX += spacingSteps4;
			}
		}
		
		
		// ****** Reset and 5 main control buttons ******
		
		static const int rowRuler4 = 220;
		static const int controlSpacingX = 66;
		static const int colRuler1 = colRuler0 + controlSpacingX - 14;
		static const int colRuler2 = colRuler1 + controlSpacingX;
		static const int colRuler3 = colRuler2 + controlSpacingX;
		static const int colRuler4 = colRuler3 + controlSpacingX;
		static const int colRuler5 = colRuler4 + controlSpacingX;
		static const int posLEDvsButton = + 25;

		// Reset
		addInput(Port::create<PJ301MPortS>(Vec(colRuler0, rowRuler4 ), Port::INPUT, module, GateSeq16::RESET_INPUT));
		
		// Gate light and button
		addChild(ModuleLightWidget::create<MediumLight<RedGreenBlueLight>>(Vec(colRuler1 + posLEDvsButton + offsetMediumLight, rowRuler4 + offsetMediumLight), module, GateSeq16::GATE_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(colRuler1 + offsetCKD6b, rowRuler4 + offsetCKD6b), module, GateSeq16::GATE_PARAM, 0.0f, 1.0f, 0.0f));
		
		// Length light and button
		addChild(ModuleLightWidget::create<MediumLight<RedGreenBlueLight>>(Vec(colRuler2 + posLEDvsButton + offsetMediumLight, rowRuler4 + offsetMediumLight), module, GateSeq16::LENGTH_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(colRuler2 + offsetCKD6b, rowRuler4 + offsetCKD6b), module, GateSeq16::LENGTH_PARAM, 0.0f, 1.0f, 0.0f));

		// Gate p light and button
		addChild(ModuleLightWidget::create<MediumLight<RedGreenBlueLight>>(Vec(colRuler3 + posLEDvsButton + offsetMediumLight, rowRuler4 + offsetMediumLight), module, GateSeq16::GATEP_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(colRuler3 + offsetCKD6b, rowRuler4 + offsetCKD6b), module, GateSeq16::GATEP_PARAM, 0.0f, 1.0f, 0.0f));

		// Mode light and button
		addChild(ModuleLightWidget::create<MediumLight<RedGreenBlueLight>>(Vec(colRuler4 + posLEDvsButton + offsetMediumLight, rowRuler4 + offsetMediumLight), module, GateSeq16::MODE_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(colRuler4 + offsetCKD6b, rowRuler4 + offsetCKD6b), module, GateSeq16::MODE_PARAM, 0.0f, 1.0f, 0.0f));

		// GateTrig light and button
		addChild(ModuleLightWidget::create<MediumLight<RedGreenBlueLight>>(Vec(colRuler5 + posLEDvsButton + offsetMediumLight, rowRuler4 + offsetMediumLight), module, GateSeq16::GATETRIG_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(colRuler5 + offsetCKD6b, rowRuler4 + offsetCKD6b), module, GateSeq16::GATETRIG_PARAM, 0.0f, 1.0f, 0.0f));
		
		
		
		// ****** Penultimate row ******
		
		static const int rowRuler5 = rowRuler4 + 50;
		static const int runSpacingX = 42;
		
		// Run LED bezel and light, four times
		for (int i = 0; i < 4; i++) {
			addParam(ParamWidget::create<LEDBezel>(Vec(colRuler0 + i * runSpacingX + offsetLEDbezel, rowRuler5 + 5 + offsetLEDbezel), module, GateSeq16::RUN_PARAMS + i, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRuler0 + i * runSpacingX + offsetLEDbezel + offsetLEDbezelLight, rowRuler5 + 5 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq16::RUN_LIGHTS + i));
		}
		// Prob knob
		addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(colRuler3 + offsetRoundSmallBlackKnob, rowRuler5 + offsetRoundSmallBlackKnob), module, GateSeq16::GATEP_PARAM, 0.0f, 1.0f, 1.0f));
		// Config switch (3 position)
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(colRuler4 + hOffsetCKSS, rowRuler5 + vOffsetCKSSThree), module, GateSeq16::CONFIG_PARAM, 0.0f, 2.0f, 0.0f));	// 0.0f is top position
		// Copy button
		addParam(ParamWidget::create<TL1105>(Vec(colRuler5 + offsetTL1105, rowRuler5 + offsetTL1105), module, GateSeq16::COPY_PARAM, 0.0f, 1.0f, 0.0f));
	
		
		// ****** Last row ******
		
		static const int rowRuler6 = rowRuler5 + 50;
		
		// Run CVs
		for (int i = 0; i < 4; i++) {
			addInput(Port::create<PJ301MPortS>(Vec(colRuler0 + i * runSpacingX, rowRuler6), Port::INPUT, module, GateSeq16::RUNCV_INPUTS + i));
		}
		// Prob CV 
		addInput(Port::create<PJ301MPortS>(Vec(colRuler3, rowRuler6), Port::INPUT, module, GateSeq16::PROBCV_INPUT));
		// Run CV mode switch
		addParam(ParamWidget::create<CKSS>(Vec(colRuler4 + hOffsetCKSS, rowRuler6 + vOffsetCKSS), module, GateSeq16::RUNCV_MODE_PARAM, 0.0f, 1.0f, 1.0f)); // 1.0f is top position
		// Paste button
		addParam(ParamWidget::create<TL1105>(Vec(colRuler5 + offsetTL1105, rowRuler6 + offsetTL1105), module, GateSeq16::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		
	}
};

Model *modelGateSeq16 = Model::create<GateSeq16, GateSeq16Widget>("Impromptu Modular", "Gate-Seq-16", "Gate-Seq-16", SEQUENCER_TAG);
