//***********************************************************************************************
//Tactile CV controller module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct Tact : Module {
	static const int numLights = 10;// number of lights per channel

	enum ParamIds {
		ENUMS(TACT_PARAMS, 2),// touch pads
		ENUMS(ATTV_PARAMS, 2),// max knobs
		ENUMS(RATE_PARAMS, 2),// rate knobs
		LINK_PARAM,
		ENUMS(SLIDE_PARAMS, 2),// slide switches
		ENUMS(STORE_PARAMS, 2),// store buttons
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(TOP_INPUTS, 2),
		ENUMS(BOT_INPUTS, 2),
		ENUMS(RECALL_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 2),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(TACT_LIGHTS, numLights * 2 + numLights * 2), // first N lights for channel L, other N for channel R (*2 for GreenRed)
		NUM_LIGHTS
	};
	
	// Need to save
	int panelTheme = 0;
	float storeCV[2];
	
	// No need to save
	double cv[2];// actual Tact CV since Tactknob can be different than these when transitioning
	unsigned long transitionStepsRemain[2];// 0 when no transition under way, downward step counter when transitioning
	double transitionCVdelta[2];// no need to initialize, this is a companion to slideStepsRemain
	long infoStore;// 0 when no info, positive downward step counter timer when store left channel, negative upward for right channel
	long initInfoStore;// set in constructor

	static constexpr float TACT_INIT_VALUE = 5.0f;// so that module constructor is coherent with widget initialization, since module created before widget
	double tactLast[2];
		
	IMTactile* tactWidgets[2];		
	
	
	SchmittTrigger topTriggers[2];
	SchmittTrigger botTriggers[2];
	SchmittTrigger storeTriggers[2];
	SchmittTrigger recallTriggers[2];


	inline bool isLinked(void) {return params[LINK_PARAM].value > 0.5f;}

	
	Tact() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		static const float storeInfoTime = 0.5f;// seconds
		initInfoStore = (long) (storeInfoTime * engineGetSampleRate());

		tactWidgets[0] = nullptr;
		tactWidgets[1] = nullptr;

		onReset();
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		for (int i = 0; i < 2; i++) {
			cv[i] = TACT_INIT_VALUE;
			tactLast[i] = cv[i];
			transitionStepsRemain[i] = 0;
			storeCV[i] = 0.0f;
		}
		infoStore = 0l;
	}

	
	// widgets randomized before onRandomize() is called
	void onRandomize() override {
		for (int i = 0; i < 2; i++) {
			cv[i] = clamp(params[TACT_PARAMS + i].value, 0.0f, 10.0f);
			tactLast[i] = cv[i];
			transitionStepsRemain[i] = 0;
		}
		infoStore = 0l;
	}

	
	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// storeCV[0]
		json_object_set_new(rootJ, "storeCV0", json_real(storeCV[0]));
		// storeCV[1]
		json_object_set_new(rootJ, "storeCV1", json_real(storeCV[1]));
		
		return rootJ;
	}

	
	// widgets loaded before this fromJson() is called
	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// storeCV[0]
		json_t *storeCV0J = json_object_get(rootJ, "storeCV0");
		if (storeCV0J)
			storeCV[0] = json_real_value(storeCV0J);

		// storeCV[1]
		json_t *storeCV1J = json_object_get(rootJ, "storeCV1");
		if (storeCV1J)
			storeCV[1] = json_real_value(storeCV1J);


		for (int i = 0; i < 2; i++) {
			cv[i] = clamp(params[TACT_PARAMS + i].value, 0.0f, 10.0f);
			tactLast[i] = cv[i];
			transitionStepsRemain[i] = 0;
		}
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		
		// store buttons
		for (int i = 0; i < 2; i++) {
			if (storeTriggers[i].process(params[STORE_PARAMS + i].value)) {
				storeCV[i] = cv[i];
				if (i == 1 && isLinked())
					storeCV[0] = cv[1];
				infoStore = initInfoStore * (i == 0 ? 1l : -1l);
			}
		}
		
		// top/bot/recall CV inputs
		for (int i = 0; i < 2; i++) {
			if (topTriggers[i].process(inputs[TOP_INPUTS + i].value)) {
				if (tactWidgets[i] != nullptr) {
					tactWidgets[i]->changeValue(10.0f);
				}
				transitionStepsRemain[i] = 0l;
			}
			if (botTriggers[i].process(inputs[BOT_INPUTS + i].value)) {
				if (tactWidgets[i] != nullptr) {
					tactWidgets[i]->changeValue(0.0f);
				}				
				transitionStepsRemain[i] = 0l;
			}
			if (recallTriggers[i].process(inputs[RECALL_INPUTS + i].value)) {
				if (tactWidgets[i] != nullptr) {
					tactWidgets[i]->changeValue(storeCV[i]);
					if (params[SLIDE_PARAMS + i].value < 0.5f) //if no slide
						cv[i]=storeCV[i];
				}				
				transitionStepsRemain[i] = 0l;
			}
		}
		
		
		// cv
		for (int i = 0; i < 2; i++) {
			float newParamValue = clamp(params[TACT_PARAMS + i].value, 0.0f, 10.0f);
			if (tactLast[i] != newParamValue) {
				tactLast[i] = newParamValue;
				double transitionRate = params[RATE_PARAMS + i].value; // s/V
				double dV = tactLast[i] - cv[i];
				double numSamples = engineGetSampleRate() * transitionRate * fabs(dV);
				transitionStepsRemain[i] = ((unsigned long) (numSamples + 0.5f)) + 1ul;
				transitionCVdelta[i] = (dV / (double)transitionStepsRemain[i]);
			}
			if (transitionStepsRemain[i] > 0) {
				cv[i] += transitionCVdelta[i];
			}
		}
		
	
		// Outputs
		for (int i = 0; i < 2; i++) {
			int readChan = isLinked() ? 0 : i;
			outputs[CV_OUTPUTS + i].value = (float)cv[readChan] * params[ATTV_PARAMS + readChan].value;
		}
		
		// Tactile lights
		if (infoStore > 0l)
			setTLightsStore(0, infoStore);
		else
			setTLights(0);
		if (infoStore < 0l)
			setTLightsStore(1, infoStore * -1l);
		else
			setTLights(1);
		
		for (int i = 0; i < 2; i++) {
			if (transitionStepsRemain[i] > 0)
				transitionStepsRemain[i]--;
		}
		if (isLinked()) {
			transitionStepsRemain[1] = 0;
			cv[1] = clamp(params[TACT_PARAMS + 1].value, 0.0f, 10.0f);
		}
		if (infoStore != 0l) {
			if (infoStore > 0l)
				infoStore --;
			if (infoStore < 0l)
				infoStore ++;
		}
	}
	
	void setTLights(int chan) {
		int readChan = isLinked() ? 0 : chan;
		//float paramClamped = clamp(params[TACT_PARAMS + readChan].value, 0.0f, 10.0f);
		float cvValue = (float)cv[readChan];
		for (int i = 0; i < numLights; i++) {
			float level = clamp( cvValue - ((float)(i)), 0.0f, 1.0f);
			//float cursor = (  paramClamped > (((float)(i)) - 0.5f) && paramClamped <= (((float)(i+1)) - 0.5f)  ) ? 1.0f : 0.0f;
			//float stable = clamp(fabs(paramClamped - cvValue) / 10.0f, 0.0f, level);
			//if (fabs(paramClamped - cvValue) > 0.01f && paramClamped >= ((float)i) && paramClamped < ((float)i + 1.0f)) {
			//	level = 0.0f;
			//	stable = 1.0f;
			//}
			
			// lights are up-side down because of module origin at top-left
			// Green diode
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 0].value = level;
			// Red diode
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 1].value = 0.0f;
		}
	}
	void setTLightsStore(int chan, long infoCount) {
		for (int i = 0; i < numLights; i++) {
			float level = (i == (int) round((float(infoCount)) / ((float)initInfoStore) * (float)(numLights - 1)) ? 1.0f : 0.0f);
			// Green diode
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 0].value = level;
			// Red diode
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 1].value = 0.0f;
		}	
	}
};


struct TactWidget : ModuleWidget {


	struct PanelThemeItem : MenuItem {
		Tact *module;
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

		Tact *module = dynamic_cast<Tact*>(this->module);
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
	
	
	TactWidget(Tact *module) : ModuleWidget(module) {
		IMTactile* tactL;
		IMTactile* tactR;		
		
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/Tact.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/dark/Tact_dark.svg")));
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws
		addChild(createDynamicScrew<IMScrew>(Vec(15, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(box.size.x-30, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(15, 365), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(box.size.x-30, 365), &module->panelTheme));

		
		
		static const int rowRuler0 = 34;
		static const int colRulerPadL = 80;
		static const int colRulerPadR = 145;
		
		// Tactile touch pads
		// Right (no dynamic width, but must do first so that left will get mouse events when wider overlaps)
		tactR = createDynamicParam2<IMTactile>(Vec(colRulerPadR, rowRuler0), module, Tact::TACT_PARAMS + 1, -1.0f, 11.0f, Tact::TACT_INIT_VALUE, nullptr);
		addParam(tactR);
		module->tactWidgets[1] = tactR;
		// Left (with width dependant on Link value)	
		tactL = createDynamicParam2<IMTactile>(Vec(colRulerPadL, rowRuler0), module, Tact::TACT_PARAMS + 0, -1.0f, 11.0f, Tact::TACT_INIT_VALUE,  &module->params[Tact::LINK_PARAM].value);
		addParam(tactL);
		module->tactWidgets[0] = tactL;
			

			
		static const int colRulerLedL = colRulerPadL - 20;
		static const int colRulerLedR = colRulerPadR + 56;
		static const int lightsOffsetY = 19;
		static const int lightsSpacingY = 17;
				
		// Tactile lights
		for (int i = 0 ; i < Tact::numLights; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(colRulerLedL, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + i * 2));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(colRulerLedR, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + (Tact::numLights + i) * 2));
		}

		
		static const int colRulerCenter = 123;// not real center, but pos so that a jack would be centered
		static const int offsetOutputX = 49;
		static const int colRulerC1L = colRulerCenter - offsetOutputX - 1;
		static const int colRulerC1R = colRulerCenter + offsetOutputX; 
		static const int rowRuler2 = 265;// outputs and link
		
		// Link switch
		addParam(ParamWidget::create<CKSS>(Vec(colRulerCenter + hOffsetCKSS, rowRuler2 + vOffsetCKSS), module, Tact::LINK_PARAM, 0.0f, 1.0f, 0.0f));		
		// Outputs
		addOutput(createDynamicPort<IMPort>(Vec(colRulerC1L, rowRuler2), Port::OUTPUT, module, Tact::CV_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(colRulerC1R, rowRuler2), Port::OUTPUT, module, Tact::CV_OUTPUTS + 1, &module->panelTheme));
		

		static const int offsetRecallX = 54;
		static const int colRulerC2L = colRulerC1L - offsetRecallX;
		static const int colRulerC2R = colRulerC1R + offsetRecallX;
		
		// Recall CV inputs
		addInput(createDynamicPort<IMPort>(Vec(colRulerC2L, rowRuler2), Port::INPUT, module, Tact::RECALL_INPUTS + 0, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerC2R, rowRuler2), Port::INPUT, module, Tact::RECALL_INPUTS + 1, &module->panelTheme));		
		
		
		
		static const int rowRuler1d = rowRuler2 - 54;
		
		// Slide switches
		addParam(ParamWidget::create<CKSS>(Vec(colRulerC2L + hOffsetCKSS, rowRuler1d + vOffsetCKSS), module, Tact::SLIDE_PARAMS + 0, 0.0f, 1.0f, 0.0f));		
		addParam(ParamWidget::create<CKSS>(Vec(colRulerC2R + hOffsetCKSS, rowRuler1d + vOffsetCKSS), module, Tact::SLIDE_PARAMS + 1, 0.0f, 1.0f, 0.0f));		

		

		static const int rowRuler1c = rowRuler1d - 46;

		// Store buttons
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2L + offsetTL1105, rowRuler1c + offsetTL1105), module, Tact::STORE_PARAMS + 0, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2R + offsetTL1105, rowRuler1c + offsetTL1105), module, Tact::STORE_PARAMS + 1, 0.0f, 1.0f, 0.0f));
		
		
		static const int rowRuler1b = rowRuler1c - 59;
		
		// Attv knobs
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerC2L + offsetIMSmallKnob, rowRuler1b + offsetIMSmallKnob), module, Tact::ATTV_PARAMS + 0, -1.0f, 1.0f, 1.0f, &module->panelTheme));
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerC2R + offsetIMSmallKnob, rowRuler1b + offsetIMSmallKnob), module, Tact::ATTV_PARAMS + 1, -1.0f, 1.0f, 1.0f, &module->panelTheme));

		
		static const int rowRuler1a = rowRuler1b - 59;
		
		// Rate knobs
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerC2L + offsetIMSmallKnob, rowRuler1a + offsetIMSmallKnob), module, Tact::RATE_PARAMS + 0, 0.0f, 4.0f, 0.2f, &module->panelTheme));
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerC2R + offsetIMSmallKnob, rowRuler1a + offsetIMSmallKnob), module, Tact::RATE_PARAMS + 1, 0.0f, 4.0f, 0.2f, &module->panelTheme));
		

		static const int bottomOffset2ndJack = 22;
		static const int colRulerB1 = colRulerC1L + bottomOffset2ndJack;
		static const int colRulerB2 = colRulerC1R - bottomOffset2ndJack;
		static const int bottomSpacingX = colRulerB2 - colRulerB1;
		static const int colRulerB0 = colRulerB1 - bottomSpacingX;
		static const int colRulerB3 = colRulerB2 + bottomSpacingX;
		
		
		static const int rowRuler3 = rowRuler2 + 54;

		// Top/bot CV Inputs
		addInput(createDynamicPort<IMPort>(Vec(colRulerB0, rowRuler3), Port::INPUT, module, Tact::TOP_INPUTS + 0, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerB1, rowRuler3), Port::INPUT, module, Tact::TOP_INPUTS + 1, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerB2, rowRuler3), Port::INPUT, module, Tact::BOT_INPUTS + 0, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerB3, rowRuler3), Port::INPUT, module, Tact::BOT_INPUTS + 1, &module->panelTheme));		
		
		
		
				
		
	}
};

Model *modelTact = Model::create<Tact, TactWidget>("Impromptu Modular", "Tact", "CTRL - Tact", CONTROLLER_TAG);
