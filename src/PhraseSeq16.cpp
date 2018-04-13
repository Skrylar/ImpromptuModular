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
		NUM_PARAMS
	};
	enum InputIds {
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
		RUN_LIGHT,
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

	
	/* Advances the module by 1 audio frame with duration 1.0 / gSampleRate */
	void step() override {

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
			snprintf(displayStr, 3, "%2u", (unsigned) 1);//module->indexSteps[module->indexChannel]);
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
		
		static const int rowRulerT0 = 52;
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
		
		static const int rowRulerMK0 = 86;
		static const int rowRulerMK1 = rowRulerMK0 + 82;
		static const int columnRulerMK0 = 15;// Octave lights
		static const int columnRulerMK1 = columnRulerMK0 + 262;// Edit mode and run switch
		static const int columnRulerMK2 = columnRulerT2;// Pattern display and knob

		// Octave lights
		static const int octLightsSpacing = 15;
		for (int i = 0; i < 7; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(columnRulerMK0 + offsetMediumLight, rowRulerMK0 - 2 + octLightsSpacing * i + offsetMediumLight), module, PhraseSeq16::OCTAVE_LIGHTS));
		}
		// Keys and Key lights
		//InvisibleKeySmall (12x) and <MediumLight<GreenLight>> (12x)
		// Edit mode
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerMK1 + hOffsetCKSS, rowRulerMK0 + 17 + vOffsetCKSS), module, PhraseSeq16::EDIT_PARAM, 0.0f, 1.0f, 1.0f));
		// Run LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerMK1 + offsetLEDbezel, rowRulerMK1 + offsetLEDbezel), module, PhraseSeq16::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerMK1 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK1 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq16::RUN_LIGHT));
		// Pattern display
		PatternDisplayWidget *displayPattern = new PatternDisplayWidget();
		displayPattern->box.pos = Vec(columnRulerMK2-7, rowRulerMK0 + 20 + vOffsetDisplay);
		displayPattern->box.size = Vec(40, 30);// 2 characters
		displayPattern->module = module;
		addChild(displayPattern);
		// Pattern knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(columnRulerMK2 + offsetDavies1900, rowRulerMK0 + 72 + offsetDavies1900), module, PhraseSeq16::PATTERN_PARAM, -INFINITY, INFINITY, 0.0f));		
		
		
		// ****** Middle Buttons portion ******
		static const int rowRulerMB0 = 222;
		//static const int columnRulerMB0 = 15;// Oct
		//static const int columnRulerMB1 = columnRulerMB0 + 50;// Gate
		//static const int columnRulerMB2 = columnRulerMB1 + 50;// Accent
		//static const int columnRulerMB3 = columnRulerMB2 + 50;// Slide
		//static const int columnRulerMB4 = columnRulerMK1;// Run
		static const int columnRulerMB5 = columnRulerMK2;// Copy-paste
		
		
		// Copy/paste switches
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB5-10, rowRulerMB0+offsetTL1105), module, PhraseSeq16::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB5+20, rowRulerMB0+offsetTL1105), module, PhraseSeq16::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		
		
		
		
		// ****** Bottom portion ******
		static const int rowRulerB0 = 319;
		static const int columnRulerB5 = columnRulerMB5;
		static const int outputJackSpacingX = 54;
		static const int columnRulerB4 = columnRulerB5 - outputJackSpacingX;
		static const int columnRulerB3 = columnRulerB4 - outputJackSpacingX;

		
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB3, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::CV_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB4, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::GATE1_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB5, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::GATE2_OUTPUT));

	}
};

Model *modelPhraseSeq16 = Model::create<PhraseSeq16, PhraseSeq16Widget>("Impromptu Modular", "Phrase-Seq-16", "Phrase-Seq-16", SEQUENCER_TAG);
