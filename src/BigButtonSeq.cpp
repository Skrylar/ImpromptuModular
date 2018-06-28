//***********************************************************************************************
//Six channel 32-step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Based on the BigButton sequencer by Look-Mum-No-Computer
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct BigButtonSeq : Module {
	enum ParamIds {
		CHAN_PARAM,
		LEN_PARAM,
		RND_PARAM,
		RESET_PARAM,
		CLEAR_PARAM,
		BANK_PARAM,
		DEL_PARAM,
		FILL_PARAM,
		BIG_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLK_INPUT,
		CHAN_INPUT,
		BIG_INPUT,
		LEN_INPUT,
		RND_INPUT,
		RESET_INPUT,
		CLEAR_INPUT,
		BANK_INPUT,
		DEL_INPUT,
		FILL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CHAN_OUTPUTS, 6),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHAN_LIGHTS, 6),
		BIG_LIGHT,
		NUM_LIGHTS
	};

	// Need to save
	int panelTheme = 0;
	int bank[6];
	uint32_t gates[6][2];// chan , bank

	// No need to save
	int indexStep;
	long clockIgnoreOnReset;
	const float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
	
	SchmittTrigger clockTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger bankTrigger;
	SchmittTrigger bigTrigger;
	
	

	inline void toggleGate(int chan) {gates[chan][bank[chan]] ^= (1<<indexStep);}
	inline void setGate(int chan) {gates[chan][bank[chan]] |= (1<<indexStep);}
	inline void clearGate(int chan) {gates[chan][bank[chan]] &= ~(1<<indexStep);}
	inline bool getGate(int chan) {return !((gates[chan][bank[chan]] & (1<<indexStep)) == 0);}
	
	
	BigButtonSeq() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		indexStep = 0;
		for (int c = 0; c < 6; c++) {
			bank[c] = 0;
			gates[c][0] = 0;
			gates[c][1] = 0;
		}
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}

	void onRandomize() override {
		indexStep = randomu32() % 32;
		for (int c = 0; c < 6; c++) {
			bank[c] = randomu32() % 2;
			gates[c][0] = randomu32();
			gates[c][1] = randomu32();
		}
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// bank
		json_t *bankJ = json_array();
		for (int c = 0; c < 6; c++)
			json_array_insert_new(bankJ, c, json_integer(bank[c]));
		json_object_set_new(rootJ, "bank", bankJ);

		// Gates
		// TODO: break into two int since min sizeof int is 16!!! *********
		json_t *gatesJ = json_array();
		for (int c = 0; c < 6; c++)
			for (int b = 0; b < 2; b++) {
				json_array_insert_new(gatesJ, b + (c<<1), json_integer((int) gates[c][b]));
			}
		json_object_set_new(rootJ, "gates", gatesJ);

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// bank
		json_t *bankJ = json_object_get(rootJ, "bank");
		if (bankJ)
			for (int c = 0; c < 6; c++)
			{
				json_t *bankArrayJ = json_array_get(bankJ, c);
				if (bankArrayJ)
					bank[c] = json_integer_value(bankArrayJ);
			}

		// Gates
		// TODO: break into two int since min sizeof int is 16!!! *********
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int c = 0; c < 6; c++)
				for (int b = 0; b < 2; b++) {
					json_t *gateJ = json_array_get(gatesJ, b + (c<<1));
					if (gateJ)
						gates[c][b] = json_integer_value(gateJ);
				}
		}
	}

	
	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		
		//********** Buttons, knobs, switches and inputs **********
		
		// Length
		int len = 0; 
		if (inputs[LEN_INPUT].active)
			len = (int) clamp(roundf(inputs[LEN_INPUT].value / 10.0f * 32.0f), 1.0f, 32.0f);
		else
			len = (int) clamp(roundf(params[LEN_PARAM].value), 1.0f, 32.0f);

		// Chan
		int chan = 0;
		if (inputs[LEN_INPUT].active)
			chan = (int) clamp(roundf(inputs[CHAN_INPUT].value / 10.0f * 6.0f), 1.0f, 6.0f) - 1;
		else
			chan = (int) clamp(roundf(params[CHAN_PARAM].value), 1.0f, 6.0f) - 1;	
		
		// Big button
		if (bigTrigger.process(params[BIG_PARAM].value + inputs[BIG_INPUT].value)) {
			toggleGate(chan);// bank and indexStep are global
		}

		// Bank button
		if (bankTrigger.process(params[BANK_PARAM].value + inputs[BANK_INPUT].value)) {
			if (bank[chan] == 1) bank[chan] = 0;
			else bank[chan] = 1;
		}
		
		// Clear button
		if (params[CLEAR_PARAM].value + inputs[CLEAR_INPUT].value > 0.5f)
			gates[chan][bank[chan]] = 0;
		
		// Del button
		if (params[DEL_PARAM].value + inputs[DEL_INPUT].value > 0.5f)
			clearGate(chan);// bank and indexStep are global

		// Fill button
		if (params[FILL_PARAM].value + inputs[FILL_INPUT].value > 0.5f)
			setGate(chan);// bank and indexStep are global

		
		
		//********** Clock and reset **********
		
		// Clock
		if (clockTrigger.process(inputs[CLK_INPUT].value)) {
			if (clockIgnoreOnReset == 0l) {
				indexStep = moveIndex(indexStep, indexStep + 1, len);
			}
		}
		
		// Reset
		if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
			indexStep = 0;
			clockTrigger.reset();
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
		}		
		
		
		
		//********** Outputs and lights **********
		
		// Gate and light outputs
		for (int i = 0; i < 6; i++) {
			outputs[CHAN_OUTPUTS + i].value = (getGate(i) && clockTrigger.isHigh()) ? 10.0f : 0.0f;
			lights[CHAN_LIGHTS + i].setBrightnessSmooth(outputs[CHAN_OUTPUTS + i].value > 1.0f ? 1.0f : 0.0f);
		}
		lights[BIG_LIGHT].value = bank[chan] == 1 ? 1.0f : 0.0f;
		
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}
};


struct BigButtonSeqWidget : ModuleWidget {

	struct PanelThemeItem : MenuItem {
		BigButtonSeq *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		BigButtonSeq *module = dynamic_cast<BigButtonSeq*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = lightPanelID;// ImpromptuModular.hpp
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = darkPanelID;// ImpromptuModular.hpp
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);

		return menu;
	}	
	
	
	BigButtonSeqWidget(BigButtonSeq *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/BigButtonSeq.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/dark/BigButtonSeq_dark.svg")));
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws
		addChild(createDynamicScrew<IMScrew>(Vec(15, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(box.size.x-30, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(15, 365), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(box.size.x-30, 365), &module->panelTheme));

		
		
		// Column rulers (horizontal positions)
		static const int rowRuler0 = 45;
		static const int colRulerCenter = 115;// not real center, but pos so that a jack would be centered
		static const int offsetChanOutX = 20;
		static const int colRulerT0 = colRulerCenter - offsetChanOutX * 5;
		static const int colRulerT1 = colRulerCenter - offsetChanOutX * 3;
		static const int colRulerT2 = colRulerCenter - offsetChanOutX * 1;
		static const int colRulerT3 = colRulerCenter + offsetChanOutX * 1;
		static const int colRulerT4 = colRulerCenter + offsetChanOutX * 3;
		static const int colRulerT5 = colRulerCenter + offsetChanOutX * 5;
		static const int ledOffsetY = 24;
		
		// Outputs
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT0, rowRuler0), Port::OUTPUT, module, BigButtonSeq::CHAN_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT1, rowRuler0), Port::OUTPUT, module, BigButtonSeq::CHAN_OUTPUTS + 1, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT2, rowRuler0), Port::OUTPUT, module, BigButtonSeq::CHAN_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT3, rowRuler0), Port::OUTPUT, module, BigButtonSeq::CHAN_OUTPUTS + 3, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT4, rowRuler0), Port::OUTPUT, module, BigButtonSeq::CHAN_OUTPUTS + 4, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler0), Port::OUTPUT, module, BigButtonSeq::CHAN_OUTPUTS + 5, &module->panelTheme));
		// LEDs
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerT0 + offsetMediumLight, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 0));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerT1 + offsetMediumLight, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 1));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerT2 + offsetMediumLight, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 2));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerT3 + offsetMediumLight, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 3));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerT4 + offsetMediumLight, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 4));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerT5 + offsetMediumLight, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 5));

		
		
		static const int rowRuler1 = rowRuler0 + 70;// clk, chan and big CV
		static const int knobCVjackOffsetX = 52;
		
		// Clock input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT0, rowRuler1), Port::INPUT, module, BigButtonSeq::CLK_INPUT, &module->panelTheme));
		// Chan knob and jack
		addParam(createDynamicParam<IMSixPosBigKnob>(Vec(colRulerCenter + offsetIMBigKnob, rowRuler1 + offsetIMBigKnob), module, BigButtonSeq::CHAN_PARAM, 1.0f, 6.0f, 1.0f, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerCenter - knobCVjackOffsetX, rowRuler1), Port::INPUT, module, BigButtonSeq::CHAN_INPUT, &module->panelTheme));
		// Big input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler1), Port::INPUT, module, BigButtonSeq::BIG_INPUT, &module->panelTheme));


		
		static const int rowRuler2 = rowRuler1 + 50;// len and rnd
		static const int lenAndRndKnobOffsetX = 90;
		
		// Len knob and jack
		addParam(createDynamicParam<IMBigSnapKnob>(Vec(colRulerCenter - lenAndRndKnobOffsetX + offsetIMBigKnob, rowRuler2 + offsetIMBigKnob), module, BigButtonSeq::LEN_PARAM, 1.0f, 32.0f, 32.0f, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerCenter - lenAndRndKnobOffsetX + knobCVjackOffsetX, rowRuler2), Port::INPUT, module, BigButtonSeq::LEN_INPUT, &module->panelTheme));
		// Rnd knob and jack
		addParam(createDynamicParam<IMBigSnapKnob>(Vec(colRulerCenter + lenAndRndKnobOffsetX + offsetIMBigKnob, rowRuler2 + offsetIMBigKnob), module, BigButtonSeq::RND_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerCenter + lenAndRndKnobOffsetX - knobCVjackOffsetX, rowRuler2), Port::INPUT, module, BigButtonSeq::RND_INPUT, &module->panelTheme));


		
		static const int rowRuler3 = rowRuler2 + 35;// bank
		static const int rowRuler4 = rowRuler3 + 22;// clear and del
		static const int rowRuler5 = rowRuler4 + 52;// reset and fill
		static const int clearAndDelButtonOffsetX = (colRulerCenter - colRulerT0) / 2 + 8;
		static const int knobCVjackOffsetY = 40;
		
		// Bank button and jack
		addParam(createDynamicParam<IMBigPushButton>(Vec(colRulerCenter + offsetCKD6b, rowRuler3 + offsetCKD6b), module, BigButtonSeq::BANK_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addInput(createDynamicPort<IMPort>(Vec(colRulerCenter, rowRuler3 + knobCVjackOffsetY), Port::INPUT, module, BigButtonSeq::BANK_INPUT, &module->panelTheme));
		// Clear button and jack
		addParam(createDynamicParam<IMBigPushButton>(Vec(colRulerCenter - clearAndDelButtonOffsetX + offsetCKD6b, rowRuler4 + offsetCKD6b), module, BigButtonSeq::CLEAR_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addInput(createDynamicPort<IMPort>(Vec(colRulerCenter - clearAndDelButtonOffsetX, rowRuler4 + knobCVjackOffsetY), Port::INPUT, module, BigButtonSeq::CLEAR_INPUT, &module->panelTheme));
		// Del button and jack
		addParam(createDynamicParam<IMBigPushButton>(Vec(colRulerCenter + clearAndDelButtonOffsetX + offsetCKD6b, rowRuler4 + offsetCKD6b), module, BigButtonSeq::DEL_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addInput(createDynamicPort<IMPort>(Vec(colRulerCenter + clearAndDelButtonOffsetX, rowRuler4 + knobCVjackOffsetY), Port::INPUT, module, BigButtonSeq::DEL_INPUT, &module->panelTheme));
		// Reset button and jack
		addParam(createDynamicParam<IMBigPushButton>(Vec(colRulerT0 + offsetCKD6b, rowRuler5 + offsetCKD6b), module, BigButtonSeq::RESET_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addInput(createDynamicPort<IMPort>(Vec(colRulerT0, rowRuler5 + knobCVjackOffsetY), Port::INPUT, module, BigButtonSeq::RESET_INPUT, &module->panelTheme));
		// Fill button and jack
		addParam(createDynamicParam<IMBigPushButton>(Vec(colRulerT5 + offsetCKD6b, rowRuler5 + offsetCKD6b), module, BigButtonSeq::FILL_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addInput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler5 + knobCVjackOffsetY), Port::INPUT, module, BigButtonSeq::FILL_INPUT, &module->panelTheme));


		// And now time for... BIG BUTTON!
		addChild(ModuleLightWidget::create<GiantLight<GreenLight>>(Vec(colRulerCenter + offsetLEDbezel + offsetLEDbezelLight, 320 + offsetLEDbezel + offsetLEDbezelLight), module, BigButtonSeq::BIG_LIGHT));
		addParam(ParamWidget::create<LEDBezel>(Vec(colRulerCenter + offsetLEDbezel, 320 + offsetLEDbezel), module, BigButtonSeq::BIG_PARAM, 0.0f, 1.0f, 0.0f));
	}
};

Model *modelBigButtonSeq = Model::create<BigButtonSeq, BigButtonSeqWidget>("Impromptu Modular", "Big-Button-Seq", "SEQ - Big-Button-Seq", SEQUENCER_TAG);

/*CHANGE LOG

0.6.8:
created

*/