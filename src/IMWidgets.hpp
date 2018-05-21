//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from Valley Rack Free by Dale Johnson
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#ifndef IM_WIDGETS_HPP
#define IM_WIDGETS_HPP

#include "rack.hpp"
#include "window.hpp"

using namespace rack;


// Dynamic Panel (From Dale Johnson)

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


// Dynamic SVGPort (started from SVGPort in app.hpp and SVGPort.cpp)

struct DynamicSVGPort : Port, FramebufferWidget {
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<SVG>> frames;
    SVGWidget* sw;
	CircularShadow *shadow;

    DynamicSVGPort();
    void addJack(std::shared_ptr<SVG> svg);
	void draw(NVGcontext *vg) override {Port::draw(vg); FramebufferWidget::draw(vg);}
    void step() override;
};

template <class TDynamicPort>
DynamicSVGPort* createDynamicJackWidget(Vec pos, Port::PortType type, Module *module, int portId,
                                               int* mode, Plugin* plugin) {
	DynamicSVGPort *dynJack = new TDynamicPort();
	dynJack->box.pos = pos;
	dynJack->module = module;
	dynJack->type = type;
	dynJack->portId = portId;
	
	dynJack->mode = mode;
	dynJack->addJack(SVG::load(assetGlobal("res/ComponentLibrary/PJ301M.svg")));
    dynJack->addJack(SVG::load(assetPlugin(plugin, "res/comp/CL1362.svg")));
	
	return dynJack;
}


// Dynamic SVGSwitch (started from SVGSwitch in app.hpp and SVGSwitch.cpp)

struct DynamicSVGSwitch : virtual ParamWidget, FramebufferWidget {
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<SVG>> frames;
    SVGWidget* sw;

    DynamicSVGSwitch();
    void addFrame(std::shared_ptr<SVG> svg);
    void step() override;
	void onChange(EventChange &e) override;
};

template <class TDynamicSwitch>
DynamicSVGSwitch* createDynamicSwitchWidget(Vec pos, Module *module, int paramId, float minValue, float maxValue, float defaultValue,
                                               int* mode, Plugin* plugin) {
	DynamicSVGSwitch *dynSwitch = new TDynamicSwitch();
	dynSwitch->box.pos = pos;
	dynSwitch->module = module;
	dynSwitch->paramId = paramId;
	dynSwitch->mode = mode;
	dynSwitch->setLimits(minValue, maxValue);
	dynSwitch->setDefaultValue(defaultValue);
	
	return dynSwitch;
}

#endif