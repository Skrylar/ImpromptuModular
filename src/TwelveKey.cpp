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
	
	
	// No need to save
	float gateLight = 0.0f;
	

	TwelveKey() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
	}

	void onRandomize() override {
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
	}

		
	void step() override {		
		
		// CV and gate outputs
		if (inputs[GATE_INPUT].value > 0.5f) {// if receiving a key from left chain
			outputs[CV_OUTPUT].value = inputs[GATE_INPUT].value;
			outputs[GATE_OUTPUT].value = inputs[GATE_INPUT].value;
		}
		else {// key from this
			float cv = inputs[OCT_INPUT].active ? inputs[OCT_INPUT].value : (params[OCT_PARAM].value - 4.0f);
			
			for (int i = 0; i < 12 ; i++) {
				if (params[KEY_PARAM + i].value > 0.5f) {
					cv += ((float) i) / 12.0f;
				}
			}
			
			outputs[CV_OUTPUT].value = cv;
			outputs[GATE_OUTPUT].value = 10.0f;
		}
		
		// Octave output
		if (inputs[OCT_INPUT].active) {
			outputs[OCT_OUTPUT].value = inputs[OCT_INPUT].value + 1.0f;
		}
		else {
			outputs[OCT_OUTPUT].value = params[OCT_PARAM].value - 4.0f + 1.0f;
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
