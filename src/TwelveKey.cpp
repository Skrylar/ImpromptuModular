//***********************************************************************************************
//Chain-able keyboard module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by:
//  * the Autodafe keyboard by Antonio Grazioli 
//  * the cf mixer by Clément Foulc
//  * Twisted Electrons' KeyChain 
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct TwelveKey : Module {
	enum ParamIds {
		OCTINC_PARAM,
		OCTDEC_PARAM,
		ENUMS(KEY_PARAMS, 12),
		NUM_PARAMS
	};
	enum InputIds {
		GATE_INPUT,
		CV_INPUT,	
		OCT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		CV_OUTPUT,	
		OCT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		PRESS_LIGHT,
		NUM_LIGHTS
	};
	
	// Need to save
	int octaveNum;// 0 to 9
	float cv;
	bool stateInternal;// false when pass through CV and Gate, true when CV and gate from this module
	
	// No need to save
	float gateLight = 0.0f;
	
	
	SchmittTrigger keyTriggers[12];
	SchmittTrigger gateInputTrigger;
	SchmittTrigger octIncTrigger;
	SchmittTrigger octDecTrigger;
	

	TwelveKey() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		octaveNum = 4;
		cv = 0.0f;
		stateInternal = inputs[GATE_INPUT].active ? false : true;
	}

	void onRandomize() override {
		octaveNum = randomu32() % 10;
		cv = ((float)(octaveNum - 4)) + ((float)(randomu32() % 12)) / 12.0f;
		stateInternal = inputs[GATE_INPUT].active ? false : true;
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();
		
		// cv
		json_object_set_new(rootJ, "cv", json_real(cv));
		
		// octave
		json_object_set_new(rootJ, "octave", json_integer(octaveNum));
		
		// stateInternal
		json_object_set_new(rootJ, "stateInternal", json_boolean(stateInternal));

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		
		// cv
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ)
			cv = json_real_value(cvJ);
		
		// octave
		json_t *octaveJ = json_object_get(rootJ, "octave");
		if (octaveJ)
			octaveNum = json_integer_value(octaveJ);

		// stateInternal
		json_t *stateInternalJ = json_object_get(rootJ, "stateInternal");
		if (stateInternalJ)
			stateInternal = json_is_true(stateInternalJ);
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		

		// set octaveNum
		if (octIncTrigger.process(params[OCTINC_PARAM].value))
			octaveNum++;
		if (octDecTrigger.process(params[OCTDEC_PARAM].value))
			octaveNum--;
		if (inputs[OCT_INPUT].active)
			octaveNum = ((int) floor(inputs[OCT_INPUT].value));
		if (octaveNum > 9) octaveNum = 9;
		if (octaveNum < 0) octaveNum = 0;
		
		// set stateInternal and memorize cv 
		for (int i = 0; i < 12; i++) {
			if (keyTriggers[i].process(params[KEY_PARAMS + i].value)) {
				cv = ((float)(octaveNum - 4)) + ((float) i) / 12.0f;
				stateInternal = true;
			}
		}
		if (gateInputTrigger.process(inputs[GATE_INPUT].value)) {
			cv = inputs[CV_INPUT].value;			
			stateInternal = false;
		}
			
		
		// Gate keypress LED (with fade)
		int pressed = 0;
		for (int i = 0; i < 12; i++)
			if (params[KEY_PARAMS + i].value > 0.5f)
				pressed++;
		if (pressed != 0)
			gateLight = 1.0f;
		else
			gateLight -= (gateLight / lightLambda) * engineGetSampleTime();
		lights[PRESS_LIGHT].value = gateLight;
		
		
		// cv output
		outputs[CV_OUTPUT].value = cv;
		
		// gate output
		if (stateInternal == false) {// if receiving a key from left chain
			outputs[GATE_OUTPUT].value = inputs[GATE_INPUT].value;
		}
		else {// key from this
			outputs[GATE_OUTPUT].value = (pressed != 0 ? 10.0f : 0.0f);
		}
		
		// Octave output
		outputs[OCT_OUTPUT].value = round( (float)(octaveNum + 1) );
	}
};


struct TwelveKeyWidget : ModuleWidget {

	struct OctaveNumDisplayWidget : TransparentWidget {
		int *octaveNum;
		std::shared_ptr<Font> font;
		
		OctaveNumDisplayWidget() {
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
			displayStr[0] = 0x30 + (char) *octaveNum;
			displayStr[1] = 0;
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};

	TwelveKeyWidget(TwelveKey *module) : ModuleWidget(module) {
		
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/TwelveKey.svg")));

		// Screws
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 365)));


		// ****** Top portion (keys) ******
		
		// Black keys
		addParam(ParamWidget::create<InvisibleKey>(Vec(30, 40), module, TwelveKey::KEY_PARAMS + 1, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(71, 40), module, TwelveKey::KEY_PARAMS + 3, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(154, 40), module, TwelveKey::KEY_PARAMS + 6, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(195, 40), module, TwelveKey::KEY_PARAMS + 8, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(236, 40), module, TwelveKey::KEY_PARAMS + 10, 0.0, 1.0, 0.0));

		// White keys
		addParam(ParamWidget::create<InvisibleKey>(Vec(10, 112), module, TwelveKey::KEY_PARAMS + 0, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(51, 112), module, TwelveKey::KEY_PARAMS + 2, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(92, 112), module, TwelveKey::KEY_PARAMS + 4, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(133, 112), module, TwelveKey::KEY_PARAMS + 5, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(174, 112), module, TwelveKey::KEY_PARAMS + 7, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(215, 112), module, TwelveKey::KEY_PARAMS + 9, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(256, 112), module, TwelveKey::KEY_PARAMS + 11, 0.0, 1.0, 0.0));
		
		
		// ****** Bottom portion ******

		// Column rulers (horizontal positions)
		static const int columnRulerL = 30;
		static const int columnRulerR = box.size.x - 25 - columnRulerL;
		static const int columnRulerM = box.size.x / 2 - 14;
		
		// Row rulers (vertical positions)
		static const int rowRuler0 = 220;
		static const int rowRulerStep = 49;
		static const int rowRuler1 = rowRuler0 + rowRulerStep;
		static const int rowRuler2 = rowRuler1 + rowRulerStep;
		
		// Left side inputs
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerL, rowRuler0), Port::INPUT, module, TwelveKey::CV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerL, rowRuler1), Port::INPUT, module, TwelveKey::GATE_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerL, rowRuler2), Port::INPUT, module, TwelveKey::OCT_INPUT));

		// Middle
		// Press LED
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(columnRulerM + offsetMediumLight, rowRuler0 - 15 + offsetMediumLight), module, TwelveKey::PRESS_LIGHT));
		// Octave display
		OctaveNumDisplayWidget *octaveNumDisplay = new OctaveNumDisplayWidget();
		octaveNumDisplay->box.pos = Vec(columnRulerM + 2, rowRuler1 - 11 + vOffsetDisplay);
		octaveNumDisplay->box.size = Vec(24, 30);// 1 character
		octaveNumDisplay->octaveNum = &module->octaveNum;
		addChild(octaveNumDisplay);
		// Octave buttons
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerM - 20 + offsetCKD6, rowRuler2 - 10 + offsetCKD6), module, TwelveKey::OCTDEC_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerM + 22 + offsetCKD6, rowRuler2 - 10 + offsetCKD6), module, TwelveKey::OCTINC_PARAM, 0.0f, 1.0f, 0.0f));
		
		// Right side outputs
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerR, rowRuler0), Port::OUTPUT, module, TwelveKey::CV_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerR, rowRuler1), Port::OUTPUT, module, TwelveKey::GATE_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerR, rowRuler2), Port::OUTPUT, module, TwelveKey::OCT_OUTPUT));
	}
};

Model *modelTwelveKey = Model::create<TwelveKey, TwelveKeyWidget>("Impromptu Modular", "Twelve-Key", "Twelve-Key", CONTROLLER_TAG);
