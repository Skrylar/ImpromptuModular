//***********************************************************************************************
//Blank Panel for VCV Rack by Marc Boulé
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "IMWidgets.hpp"


struct BlankPanel : Module {

	int panelTheme = 0;


	BlankPanel() : Module(0, 0, 0, 0) {
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
	}
};


struct BlankPanelWidget : ModuleWidget {

	struct PanelThemeItem : MenuItem {
		BlankPanel *module;
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

		BlankPanel *module = dynamic_cast<BlankPanel*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = "Light";
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = "Dark";
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);

		return menu;
	}	


	BlankPanelWidget(BlankPanel *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicPanelWidget *panel = new DynamicPanelWidget();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/BlankPanel.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/dark/BlankPanel_dark.svg")));
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screw holes (optical illustion makes screws look oval, remove for now)
		/*addChild(new ScrewHole(Vec(15, 0)));
		addChild(new ScrewHole(Vec(box.size.x-30, 0)));
		addChild(new ScrewHole(Vec(15, 365)));
		addChild(new ScrewHole(Vec(box.size.x-30, 365)));*/

		// Screws
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 365)));
		
	}
};

Model *modelBlankPanel = Model::create<BlankPanel, BlankPanelWidget>("Impromptu Modular", "Blank-Panel", "Blank-Panel", BLANK_TAG);
