//***********************************************************************************************
//Multi-phrase 16 step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by Transistor Sounds Labs' SA-100 Stepper Acid Sequencer 
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct PhraseSeq16 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, 16*2),// room for GreenRed
		ENUMS(OCTAVE_LIGHTS, 8),// octaves 1 to 8
		NUM_LIGHTS
	};

	// Need to save

	// No need to save
	
	
	PhraseSeq16() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
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

	}
};


struct PhraseSeq16Widget : ModuleWidget {

	
	PhraseSeq16Widget(PhraseSeq16 *module) : ModuleWidget(module) {
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/PhraseSeq16.svg")));

		// Screws
		addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));

		
		// ****** Top portion ******
		
		static const int rowRulerT0 = 56;
		static const int columnRulerT0 = 22;
		//static const int columnRulerT1 = columnRulerT0 + 258;

		
		// Step/Phrase lights
		static const int spLightsSpacing = 15;
		for (int i = 0; i < 16; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(columnRulerT0 + spLightsSpacing * i + offsetMediumLight, rowRulerT0 + offsetMediumLight), module, PhraseSeq16::STEP_PHRASE_LIGHTS + (i*2)));
		}
		
		
		// ****** Left portion ******
		
		static const int rowRulerL0 = 86;
		static const int columnRulerL0 = 22;

		// Octave lights
		static const int octLightsSpacing = 15;
		for (int i = 0; i < 8; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(columnRulerL0 + offsetMediumLight, rowRulerL0 + octLightsSpacing * i + offsetMediumLight), module, PhraseSeq16::OCTAVE_LIGHTS));
		}
		
	}
};

Model *modelPhraseSeq16 = Model::create<PhraseSeq16, PhraseSeq16Widget>("Impromptu Modular", "Phrase-Seq-16", "Phrase-Seq-16", SEQUENCER_TAG);
