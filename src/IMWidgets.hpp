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

struct PanelBorderWidget : TransparentWidget { // from SVGPanel.cpp
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


// ******** Dynamic Ports ********

// General Dynamic Port creation
template <class TDynamicPort>
TDynamicPort* createDynamicPort(Vec pos, Port::PortType type, Module *module, int portId,
                                               int* mode) {
	TDynamicPort *dynPort = Port::create<TDynamicPort>(pos, type, module, portId);
	dynPort->mode = mode;
	return dynPort;
}

// Dynamic SVGPort (see SVGPort in app.hpp and SVGPort.cpp)
struct DynamicSVGPort : SVGPort {
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<SVG>> frames;

    DynamicSVGPort();
    void addFrame(std::shared_ptr<SVG> svg);
    void step() override;
};



// ******** Dynamic Params ********

// General Dynamic Param creation
template <class TDynamicParam>
TDynamicParam* createDynamicParam(Vec pos, Module *module, int paramId, float minValue, float maxValue, float defaultValue,
                                               int* mode) {
	TDynamicParam *dynParam = ParamWidget::create<TDynamicParam>(pos, module, paramId, minValue, maxValue, defaultValue);
	dynParam->mode = mode;
	return dynParam;
}

// Dynamic SVGSwitch (see SVGSwitch in app.hpp and SVGSwitch.cpp)
struct DynamicSVGSwitch : SVGSwitch {
    int* mode;
    int oldMode;
	std::vector<std::shared_ptr<SVG>> framesAll;
	
    DynamicSVGSwitch();
	void addFrameAll(std::shared_ptr<SVG> svg);
    void step() override;
};



#endif