//***********************************************************************************************
//Chain-able keyboard module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt and on concepts from the Autodafe
//keyboard by Antonio Grazioli and the cf mixer by Clément Foulc, and Twisted Electrons' TwelveKey
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct TwelveKey : Module {
	enum ParamIds {
		OCT_PARAM,
		ENUMS(KEY_PARAM, 12),
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
	float cv;
	bool stateInternal;// false when pass through CV and Gate, true when CV and gate from this module
	
	// No need to save
	float gateLight = 0.0f;
	
	
	SchmittTrigger keyTriggers[12];
	SchmittTrigger gateInputTrigger;

	TwelveKey() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		cv = 0.0f;
		stateInternal = inputs[GATE_INPUT].active ? false : true;
	}

	void onRandomize() override {
		cv = quantize(randomUniform() * 10.0f - 4.0f);
		stateInternal = inputs[GATE_INPUT].active ? false : true;
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();
		
		// cv
		json_object_set_new(rootJ, "cv", json_real(cv));
		
		// stateInternal
		json_object_set_new(rootJ, "stateInternal", json_boolean(stateInternal));

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		
		// cv
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ)
			cv = json_real_value(cvJ);

		// stateInternal
		json_t *stateInternalJ = json_object_get(rootJ, "stateInternal");
		if (stateInternalJ)
			stateInternal = json_is_true(stateInternalJ);
	}

		
	inline float quantize(float cv) {
		return (roundf(cv * 12.0f) / 12.0f);
	}

	void step() override {		
		
		// set stateInternal and memorize cv 
		for (int i = 0; i < 12; i++) {
			if (keyTriggers[i].process(params[KEY_PARAM + i].value)) {
				cv = inputs[OCT_INPUT].active ? inputs[OCT_INPUT].value : (params[OCT_PARAM].value - 4.0f);
				cv += ((float) i) / 12.0f;
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
			if (params[KEY_PARAM + i].value > 0.5f)
				pressed++;
		if (pressed != 0)
			gateLight = 1.0f;
		else
			gateLight -= gateLight / lightLambda / engineGetSampleRate();
		lights[PRESS_LIGHT].value = gateLight;
		
		
		// cv and gate outputs
		outputs[CV_OUTPUT].value = cv;
		if (stateInternal == false) {// if receiving a key from left chain
			outputs[GATE_OUTPUT].value = inputs[GATE_INPUT].value;
		}
		else {// key from this
			outputs[GATE_OUTPUT].value = (pressed != 0 ? 10.0f : 0.0f);
		}
		
		// Octave output
		if (inputs[OCT_INPUT].active) {
			outputs[OCT_OUTPUT].value = inputs[OCT_INPUT].value + 1.0f;
		}
		else {
			outputs[OCT_OUTPUT].value = params[OCT_PARAM].value - 4.0f + 1.0f;
		}
	}
};


struct TwelveKeyWidget : ModuleWidget {

	TwelveKeyWidget(TwelveKey *module) : ModuleWidget(module) {

		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/TwelveKey.svg")));

		// Screws
		addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));


		// ****** Top portion (keys) ******
		
		// Black keys
		addParam(ParamWidget::create<InvisibleKey>(Vec(30, 40), module, TwelveKey::KEY_PARAM + 1, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(71, 40), module, TwelveKey::KEY_PARAM + 3, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(154, 40), module, TwelveKey::KEY_PARAM + 6, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(195, 40), module, TwelveKey::KEY_PARAM + 8, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(236, 40), module, TwelveKey::KEY_PARAM + 10, 0.0, 1.0, 0.0));

		// White keys
		addParam(ParamWidget::create<InvisibleKey>(Vec(10, 112), module, TwelveKey::KEY_PARAM + 0, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(51, 112), module, TwelveKey::KEY_PARAM + 2, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(92, 112), module, TwelveKey::KEY_PARAM + 4, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(133, 112), module, TwelveKey::KEY_PARAM + 5, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(174, 112), module, TwelveKey::KEY_PARAM + 7, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(215, 112), module, TwelveKey::KEY_PARAM + 9, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<InvisibleKey>(Vec(256, 112), module, TwelveKey::KEY_PARAM + 11, 0.0, 1.0, 0.0));

		
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
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(columnRulerM + offsetMediumLight, rowRuler0 + offsetMediumLight), module, TwelveKey::PRESS_LIGHT));
		// Octave knob
		addParam(ParamWidget::create<Davies1900hBlackSnapKnob>(Vec(columnRulerM + offsetDavies1900, rowRuler1 + 20 + offsetDavies1900), module, TwelveKey::OCT_PARAM, 0.0f, 9.0f, 4.0f));		
		
		// Right side outputs
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerR, rowRuler0), Port::OUTPUT, module, TwelveKey::CV_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerR, rowRuler1), Port::OUTPUT, module, TwelveKey::GATE_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerR, rowRuler2), Port::OUTPUT, module, TwelveKey::OCT_OUTPUT));
	}
};



Model *modelTwelveKey = Model::create<TwelveKey, TwelveKeyWidget>("Impromptu Modular", "Twelve-Key", "Twelve-Key", CONTROLLER_TAG);
