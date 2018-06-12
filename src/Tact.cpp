//***********************************************************************************************
//Tactile controller module for VCV Rack by Marc Boulé
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
		ENUMS(MAX_PARAMS, 2),// max knobs
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
		ENUMS(TACT_LIGHTS, numLights * 2), // first 11 lights for channel L, other 11 for channel R
		NUM_LIGHTS
	};
	
	// Need to save
	int panelTheme = 0;
	
	// No need to save
	
	

	Tact() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
	}

	void onRandomize() override {
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		
	
		// Outputs
		outputs[CV_OUTPUTS + 0].value = params[TACT_PARAMS + 0].value * params[MAX_PARAMS + 0].value;
		outputs[CV_OUTPUTS + 1].value = params[TACT_PARAMS + 1].value * params[MAX_PARAMS + 0].value;
	
		// Tactile lights
		setTLights(0);
		setTLights(1);
	}
	
	void setTLights(int chan) {
		for (int i = 0; i < numLights; i++) {
			lights[TACT_LIGHTS + chan * numLights + (numLights - 1 - i)].value = (  params[TACT_PARAMS + chan].value > (((float)(i))/10.0f - 0.05f) && 
																					params[TACT_PARAMS + chan].value <= (((float)(i+1))/10.0f - 0.05f)  ) ? 1.0f : 0.0f;// lights are up-side down because of module origin at top-left
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
		addParam(ParamWidget::create<IMTactile>(Vec(colRulerT0, rowRuler0), module, Tact::TACT_PARAMS + 0, 0.0, 1.0, 0.5));// size is in ImpromptuModular.hpp
		addParam(ParamWidget::create<IMTactile>(Vec(colRulerT1, rowRuler0), module, Tact::TACT_PARAMS + 1, 0.0, 1.0, 0.5));// size is in ImpromptuModular.hpp

		// Tactile lights
		for (int i = 0 ; i < Tact::numLights; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerT0 + lightsOffsetX, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + i));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerT1 + lightsOffsetX, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + Tact::numLights + i));
		}


		static const int rowRuler1 = 260;
		static const int maxOffsetX = 2;
		static const int slewOffsetX = 40;

		// Max knobs
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT0 + maxOffsetX + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Tact::MAX_PARAMS + 0, -10.0f, 10.0f, 10.0f, &module->panelTheme));
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT1 + maxOffsetX + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Tact::MAX_PARAMS + 1, -10.0f, 10.0f, 10.0f, &module->panelTheme));
		// Slew knobs
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT0 + slewOffsetX + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Tact::SLEW_PARAMS + 0, 0.0f, 10.0f, 1.0f, &module->panelTheme));// in seconds
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT1 + slewOffsetX + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Tact::SLEW_PARAMS + 1, 0.0f, 10.0f, 1.0f, &module->panelTheme));// in seconds
		
				
		// Outputs
		addOutput(createDynamicPort<IMPort>(Vec(35, 315), Port::OUTPUT, module, Tact::CV_OUTPUTS + 0, &module->panelTheme));		
		addOutput(createDynamicPort<IMPort>(Vec(120, 315), Port::OUTPUT, module, Tact::CV_OUTPUTS + 1, &module->panelTheme));
		// Link switch
		addParam(ParamWidget::create<CKSS>(Vec(80 + hOffsetCKSS, 315 + vOffsetCKSS), module, Tact::LINK_PARAM, 0.0f, 1.0f, 1.0f));		
		
	}
};

Model *modelTact = Model::create<Tact, TactWidget>("Impromptu Modular", "Tact", "Tact", CONTROLLER_TAG);
