//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//This is code from Valley Rack Free by Dale Johnson
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "rack.hpp"
#include "window.hpp"

using namespace rack;


// Dynamic Panel

struct PanelBorderWidget : TransparentWidget {
	void draw(NVGcontext *vg) override;
};

struct DynamicPanelWidget : FramebufferWidget {
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<SVG>> panels;
    SVGWidget* visiblePanel;
    PanelBorderWidget* border;

    DynamicPanelWidget();
    void addPanel(std::shared_ptr<SVG> svg);
    void step() override;
};


// Dynamic Jack (started from ValleyWidgets DynamicSwitch and DynamicKnob)

struct DynamicJackWidget : Port, FramebufferWidget {
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<SVG>> jacks;
    SVGWidget* visibleJack;

    DynamicJackWidget();
    void addJack(std::shared_ptr<SVG> svg);
	void draw(NVGcontext *vg) override {Port::draw(vg); FramebufferWidget::draw(vg);}
    void step() override;
};

template <class TDynamicJack>
DynamicJackWidget* createDynamicJackWidget(Vec pos, Port::PortType type, Module *module, int portId,
                                               int* mode, Plugin* plugin) {
	DynamicJackWidget *dynJack = new DynamicJackWidget();//Port::create(pos, type, module, portId);
	dynJack->box.pos = pos;
	dynJack->module = module;
	dynJack->type = type;
	dynJack->portId = portId;
	
	dynJack->mode = mode;
	dynJack->addJack(SVG::load(assetGlobal("res/ComponentLibrary/PJ301M.svg")));
    dynJack->addJack(SVG::load(assetPlugin(plugin, "res/comp/CL1362.svg")));
	
	return dynJack;
}


// Dynamic Switch (started from SVGSwitch in app.hpp and SVGSwitch.cpp)

struct DynamicSwitchWidget : virtual ParamWidget, FramebufferWidget, MomentarySwitch {
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<SVG>> frames;
    SVGWidget* sw;

    DynamicSwitchWidget();
    void addFrame(std::shared_ptr<SVG> svg);
    void step() override;
	void onChange(EventChange &e) override;
};

template <class TDynamicSwitch>
DynamicSwitchWidget* createDynamicSwitchWidget(Vec pos, Module *module, int paramId, float minValue, float maxValue, float defaultValue,
                                               int* mode, Plugin* plugin) {
	DynamicSwitchWidget *dynSwitch = new DynamicSwitchWidget();
	dynSwitch->box.pos = pos;
	dynSwitch->module = module;
	dynSwitch->paramId = paramId;
	dynSwitch->mode = mode;
	dynSwitch->addFrame(SVG::load(assetPlugin(plugin, "res/comp/CKD6b_0.svg")));
	dynSwitch->addFrame(SVG::load(assetPlugin(plugin, "res/comp/CKD6b_1.svg")));
	dynSwitch->addFrame(SVG::load(assetPlugin(plugin, "res/comp/CKD6b_0.svg")));
	dynSwitch->addFrame(SVG::load(assetPlugin(plugin, "res/comp/CKD6b_1.svg")));	
	dynSwitch->setLimits(minValue, maxValue);
	dynSwitch->setDefaultValue(defaultValue);
	
	return dynSwitch;
}
