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
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(TOP_INPUTS, 2),
		ENUMS(BOT_INPUTS, 2),
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
	
	// No need to save
	double cv[2];// actual Tact CV since Tactknob can be different than these when transitioning
	unsigned long transitionStepsRemain[2];// 0 when no transition under way, downward step counter when transitioning
	double transitionCVdelta[2];// no need to initialize, this is a companion to slideStepsRemain

	static constexpr float TACT_INIT_VALUE = 5.0f;// so that module constructor is coherent with widget initialization, since module created before widget
	double tactLast[2];
		
	IMTactile* tactWidgets[2];		
	
	
	SchmittTrigger topTriggers[2];
	SchmittTrigger botTriggers[2];


	inline bool isLinked(void) {return params[LINK_PARAM].value > 0.5f;}

	
	Tact() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
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
		}
	}

	
	// widgets randomized before onRandomize() is called
	void onRandomize() override {
		for (int i = 0; i < 2; i++) {
			cv[i] = clamp(params[TACT_PARAMS + i].value, 0.0f, 10.0f);
			tactLast[i] = cv[i];
			transitionStepsRemain[i] = 0;
		}
	}

	
	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}

	
	// widgets loaded before this fromJson() is called
	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		for (int i = 0; i < 2; i++) {
			cv[i] = clamp(params[TACT_PARAMS + i].value, 0.0f, 10.0f);
			tactLast[i] = cv[i];
			transitionStepsRemain[i] = 0;
		}
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		
		// top/bot inputs
		for (int i = 0; i < 2; i++) {
			if (topTriggers[i].process(inputs[TOP_INPUTS + i].value)) {
				if (tactWidgets[i] != nullptr) {
					tactWidgets[i]->changeValue(10.0f);
				}
			}
			if (botTriggers[i].process(inputs[BOT_INPUTS + i].value)) {
				if (tactWidgets[i] != nullptr) {
					tactWidgets[i]->changeValue(0.0f);
				}				
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
		setTLights(0);
		setTLights(1);
		
		for (int i = 0; i < 2; i++) {
			if (transitionStepsRemain[i] > 0)
				transitionStepsRemain[i]--;
		}
		if (isLinked()) {
			transitionStepsRemain[1] = 0;
			cv[1] = clamp(params[TACT_PARAMS + 1].value, 0.0f, 10.0f);
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
		static const int colRulerT0 = 50;
		static const int colRulerT1 = 115;
		static const int lightsOffsetXL = -20;
		static const int lightsOffsetXR = 56;
		static const int lightsOffsetY = 19;
		static const int lightsSpacingY = 17;
		
		
		// Tactile touch pads
		// Right (no dynamic width, but must do first so that left will get mouse events when wider overlaps)
		tactR = createDynamicParam2<IMTactile>(Vec(colRulerT1, rowRuler0), module, Tact::TACT_PARAMS + 1, -1.0f, 11.0f, Tact::TACT_INIT_VALUE, nullptr);
		addParam(tactR);
		// Left (with width dependant on Link value)	
		tactL = createDynamicParam2<IMTactile>(Vec(colRulerT0, rowRuler0), module, Tact::TACT_PARAMS + 0, -1.0f, 11.0f, Tact::TACT_INIT_VALUE,  &module->params[Tact::LINK_PARAM].value);
		addParam(tactL);
		
		module->tactWidgets[0] = tactL;
		module->tactWidgets[1] = tactR;
				

		// Tactile lights
		for (int i = 0 ; i < Tact::numLights; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(colRulerT0 + lightsOffsetXL, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + i * 2));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(colRulerT1 + lightsOffsetXR, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + (Tact::numLights + i) * 2));
		}

		
		static const int colRulerM0 = colRulerT0 + lightsOffsetXL - 15;
		static const int colRulerM1 = colRulerT1 + lightsOffsetXR - 1;
		static const int rowRuler1 = 228;
		static const int rowRuler2 = rowRuler1 + 37;// outputs and link
		
		// CV Inputs
		addInput(createDynamicPort<IMPort>(Vec(colRulerM0, rowRuler1), Port::INPUT, module, Tact::TOP_INPUTS + 0, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerM1, rowRuler1), Port::INPUT, module, Tact::TOP_INPUTS + 1, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerM0, rowRuler2), Port::INPUT, module, Tact::BOT_INPUTS + 0, &module->panelTheme));		
		addInput(createDynamicPort<IMPort>(Vec(colRulerM1, rowRuler2), Port::INPUT, module, Tact::BOT_INPUTS + 1, &module->panelTheme));		
		// Outputs
		addOutput(createDynamicPort<IMPort>(Vec(colRulerM0 + 40, rowRuler2), Port::OUTPUT, module, Tact::CV_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(colRulerM1 - 40, rowRuler2), Port::OUTPUT, module, Tact::CV_OUTPUTS + 1, &module->panelTheme));
		// Link switch
		addParam(ParamWidget::create<CKSS>(Vec(93 + hOffsetCKSS, rowRuler2 + 34 + vOffsetCKSS), module, Tact::LINK_PARAM, 0.0f, 1.0f, 0.0f));		

		
		static const int rowRuler3 = rowRuler2 + 49;// knobs
		static const int rateOffsetX = 40;

		// Left channel knobs
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerM0 + offsetIMSmallKnob, rowRuler3 + offsetIMSmallKnob), module, Tact::ATTV_PARAMS + 0, -1.0f, 1.0f, 1.0f, &module->panelTheme));
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerM0 + rateOffsetX + offsetIMSmallKnob, rowRuler3 + offsetIMSmallKnob), module, Tact::RATE_PARAMS + 0, 0.0f, 2.0f, 0.2f, &module->panelTheme));
		// Right channel knobs
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerM1 - rateOffsetX + offsetIMSmallKnob, rowRuler3 + offsetIMSmallKnob), module, Tact::ATTV_PARAMS + 1, -1.0f, 1.0f, 1.0f, &module->panelTheme));
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerM1 + offsetIMSmallKnob, rowRuler3 + offsetIMSmallKnob), module, Tact::RATE_PARAMS + 1, 0.0f, 2.0f, 0.2f, &module->panelTheme));
		
				
		
	}
};

Model *modelTact = Model::create<Tact, TactWidget>("Impromptu Modular", "Tact", "CTRL - Tact", CONTROLLER_TAG);
