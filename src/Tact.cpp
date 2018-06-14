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


struct Tact : Module {
	static const int numLights = 11;// number of lights per channel

	enum ParamIds {
		ENUMS(TACT_PARAMS, 2),// touch pads
		ENUMS(ATTV_PARAMS, 2),// max knobs
		ENUMS(SLEW_PARAMS, 2),// slew knobs
		LINK_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 2),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(TACT_LIGHTS, numLights * 2 + numLights * 2), // first 11 lights for channel L, other 11 for channel R (*2 for GreenRed)
		NUM_LIGHTS
	};
	
	// Need to save
	int panelTheme = 0;
	
	// No need to save
	float cv[2];// actual Tact CV since Tactknob can be different than these when slewing
	unsigned long slewStepsRemain[2];// 0 when no slew under way, downward step counter when slewing
	float slewCVdelta[2];// no need to initialize, this is a companion to slideStepsRemain

	static constexpr float TACT_INIT_VALUE = 5.0f;// so that module constructor is coherent with widget initialization, since module created before widget
	float tactLast[2];
	
	
	inline bool isLinked(void) {return params[LINK_PARAM].value > 0.5f;}

	
	Tact() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		for (int i = 0; i < 2; i++) {
			cv[i] = TACT_INIT_VALUE;
			tactLast[i] = cv[i];
			slewStepsRemain[i] = 0;
		}
	}

	
	// widgets randomized before onRandomize() is called
	void onRandomize() override {
		for (int i = 0; i < 2; i++) {
			cv[i] = params[TACT_PARAMS + i].value;
			tactLast[i] = cv[i];
			slewStepsRemain[i] = 0;
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
			cv[i] = params[TACT_PARAMS + i].value;
			tactLast[i] = cv[i];
			slewStepsRemain[i] = 0;
		}
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		
		// cv
		for (int i = 0; i < 2; i++) {
			if (tactLast[i] != params[TACT_PARAMS + i].value) {
				tactLast[i] = params[TACT_PARAMS + i].value;
				slewStepsRemain[i] = (unsigned long) (params[SLEW_PARAMS + i].value * engineGetSampleRate()) + 1ul;
				slewCVdelta[i] = (tactLast[i] - cv[i])/((float)slewStepsRemain[i]);
			}
			if (slewStepsRemain[i] > 0) {
				cv[i] += slewCVdelta[i];
			}
		}
		
	
		// Outputs
		for (int i = 0; i < 2; i++) {
			int readChan = isLinked() ? 0 : i;
			outputs[CV_OUTPUTS + i].value = cv[readChan] * params[ATTV_PARAMS + readChan].value;
		}
		
		// Tactile lights
		setTLights2(0);
		setTLights2(1);
		
		for (int i = 0; i < 2; i++) {
			if (slewStepsRemain[i] > 0)
				slewStepsRemain[i]--;
		}
		if (isLinked())
			slewStepsRemain[1] = 0;
	}
	
	void setTLights1(int chan) {// single-LED cursor with dual color: red is true value, green is target
		// TODO change color on top, mid and bot lights when exactly 10V, 5V, 0V respectively, with micro epsilon (account for attv effect obviously)	
		int readChan = isLinked() ? 0 : chan;
		for (int i = 0; i < numLights; i++) {
			// Green
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 0].value = (  params[TACT_PARAMS + readChan].value > (((float)(i)) - 0.5f) && 
																					params[TACT_PARAMS + readChan].value <= (((float)(i+1)) - 0.5f)  ) ? 1.0f : 0.0f;// lights are up-side down because of module origin at top-left
			// Red
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 1].value = (  cv[readChan] > (((float)(i)) - 0.5f) && 
																					cv[readChan] <= (((float)(i+1)) - 0.5f)  ) ? 1.0f : 0.0f;// lights are up-side down because of module origin at top-left
		}
	}
	void setTLights2(int chan) {// single color bar-LED true value pale, target bright 
		// TODO change color on top, mid and bot lights when exactly 10V, 5V, 0V respectively, with micro epsilon (account for attv effect obviously)
		int readChan = isLinked() ? 0 : chan;
		for (int i = 0; i < numLights; i++) {
			// Green
			float light = (  cv[readChan] > (((float)(i)) - 0.5f) /*&& cv[readChan] <= (((float)(i+1)) - 0.5f)*/  ) ? 0.1f : 0.0f;// lights are up-side down because of module origin at top-left
			float bright = (  params[TACT_PARAMS + readChan].value > (((float)(i)) - 0.5f) && 
							  params[TACT_PARAMS + readChan].value <= (((float)(i+1)) - 0.5f)  ) ? 1.0f : 0.0f;
			
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 0].value = clamp(light + bright, 0.0f, 1.0f);
			// Red
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 1].value = bright/1.0f;
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
		static const int colRulerT0 = 15;
		static const int colRulerT1 = 100;
		static const int lightsOffsetX = 55;
		static const int lightsOffsetY = 5;
		static const int lightsSpacingY = 18;
		
		
		// Tactile touch pads
		// Right (no dynamic width, but must do first so that left will get mouse events when wider overlaps)
		
		
		/*IMTactile *tactR = new IMTactile();
		tactR->box.pos = Vec(colRulerT1, rowRuler0);
		tactR->module = module;
		tactR->paramId = (Tact::TACT_PARAMS + 1);
		tactR->setLimits(0.0f, 10.0f);
		tactR->setDefaultValue(Tact::TACT_INIT_VALUE);
		addChild(tactR);
		// Left (with width dependant on Link value)
		IMTactile *tactL = new IMTactile();
		tactL->box.pos = Vec(colRulerT0, rowRuler0);
		tactL->module = module;
		tactL->paramId = (Tact::TACT_PARAMS + 0);
		tactL->setLimits(0.0f, 10.0f);
		tactL->setDefaultValue(Tact::TACT_INIT_VALUE);
		//tactL->wider = &module->params[Tact::LINK_PARAM].value;
		addChild(tactL);*/
		
		addParam(createDynamicParam2<IMTactile>(Vec(colRulerT1, rowRuler0), module, Tact::TACT_PARAMS + 1, 0.0f, 10.0f, Tact::TACT_INIT_VALUE, &module->params[Tact::LINK_PARAM].value));
		addParam(createDynamicParam2<IMTactile>(Vec(colRulerT0, rowRuler0), module, Tact::TACT_PARAMS + 0, 0.0f, 10.0f, Tact::TACT_INIT_VALUE, nullptr));
		
		
		// Tactile lights
		for (int i = 0 ; i < Tact::numLights; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(colRulerT0 + lightsOffsetX, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + i * 2));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(colRulerT1 + lightsOffsetX, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + (Tact::numLights + i) * 2));
		}


		static const int rowRuler1 = 260;
		static const int maxOffsetX = 2;
		static const int slewOffsetX = 40;

		// Attv knobs
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT0 + maxOffsetX + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Tact::ATTV_PARAMS + 0, -1.0f, 1.0f, 1.0f, &module->panelTheme));
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT1 + maxOffsetX + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Tact::ATTV_PARAMS + 1, -1.0f, 1.0f, 1.0f, &module->panelTheme));
		// Slew knobs
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT0 + slewOffsetX + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Tact::SLEW_PARAMS + 0, 0.0f, 10.0f, 1.0f, &module->panelTheme));// in seconds
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT1 + slewOffsetX + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Tact::SLEW_PARAMS + 1, 0.0f, 10.0f, 1.0f, &module->panelTheme));// in seconds
		
				
		// Outputs
		addOutput(createDynamicPort<IMPort>(Vec(33, 315), Port::OUTPUT, module, Tact::CV_OUTPUTS + 0, &module->panelTheme));		
		addOutput(createDynamicPort<IMPort>(Vec(121, 315), Port::OUTPUT, module, Tact::CV_OUTPUTS + 1, &module->panelTheme));
		// Link switch
		addParam(ParamWidget::create<CKSS>(Vec(78 + hOffsetCKSS, 315 + vOffsetCKSS), module, Tact::LINK_PARAM, 0.0f, 1.0f, 0.0f));		
		
	}
};

Model *modelTact = Model::create<Tact, TactWidget>("Impromptu Modular", "Tact", "Tact", CONTROLLER_TAG);
