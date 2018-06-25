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



// Dynamic SVGScrew

// General Dynamic Screw creation
template <class TWidget>
TWidget* createDynamicScrew(Vec pos, int* mode) {
	TWidget *dynScrew = Widget::create<TWidget>(pos);
	dynScrew->mode = mode;
	return dynScrew;
}

struct ScrewCircle : TransparentWidget {
	float angle = 0.0f;
	float radius = 2.0f;
	ScrewCircle(float _angle);
	void draw(NVGcontext *vg) override;
};
struct DynamicSVGScrew : FramebufferWidget {
    int* mode;
    int oldMode;
	// for random rotated screw used in primary mode
	SVGWidget *sw;
	TransformWidget *tw;
	ScrewCircle *sc;
	// for fixed svg screw used in alternate mode
    SVGWidget* swAlt;
	
    DynamicSVGScrew();
    void addSVGalt(std::shared_ptr<SVG> svg);
    void step() override;
};



// Dynamic SVGPanel

struct PanelBorderWidget : TransparentWidget { // from SVGPanel.cpp
	void draw(NVGcontext *vg) override;
};

struct DynamicSVGPanel : FramebufferWidget { // like SVGPanel (in app.hpp and SVGPanel.cpp) but with dynmically assignable panel
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<SVG>> panels;
    SVGWidget* visiblePanel;
    PanelBorderWidget* border;

    DynamicSVGPanel();
    void addPanel(std::shared_ptr<SVG> svg);
    void step() override;
};



// Dynamic SVGPanel with expansion module

struct DynamicSVGPanelEx : FramebufferWidget { // like SVGPanel (in app.hpp and SVGPanel.cpp) but with dynmically assignable panel
    int* mode;
    int oldMode;
    int* expansion;
	int oldExpansion;
    std::vector<std::shared_ptr<SVG>> panels;// must add these before adding expansion panels
    std::vector<std::shared_ptr<SVG>> panelsEx;
    SVGWidget* visiblePanel;
    SVGWidget* visiblePanelEx;
	
    PanelBorderWidget* border;
    PanelBorderWidget* borderEx;

    DynamicSVGPanelEx();
    void addPanel(std::shared_ptr<SVG> svg);// must add these before adding expansion panels
    void addPanelEx(std::shared_ptr<SVG> svg);
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

// Dynamic SVGKnob (see SVGKnob in app.hpp and SVGKnob.cpp)
struct DynamicSVGKnob : SVGKnob {
    int* mode;
    int oldMode;
	std::vector<std::shared_ptr<SVG>> framesAll;
	SVGWidget* effect;
	
    DynamicSVGKnob();
	void addFrameAll(std::shared_ptr<SVG> svg);
	void addEffect(std::shared_ptr<SVG> svg);// do this last
    void step() override;
};



// General Dynamic Param creation version 2 (must use this or else param not registered properly and reset() won't work (initialize)
template <class TDynamicParam>
TDynamicParam* createDynamicParam2(Vec pos, Module *module, int paramId, float minValue, float maxValue, float defaultValue,
                                               float* wider) {
	TDynamicParam *dynParam = ParamWidget::create<TDynamicParam>(pos, module, paramId, minValue, maxValue, defaultValue);
	dynParam->wider = wider;
	return dynParam;
}

// Dynamic Tactile pad (see Knob in app.hpp and Knob.cpp, and see SVGSlider in SVGSlider.cpp and app.hpp)
struct DynamicIMTactile : ParamWidget, FramebufferWidget {
	float* wider;// > 0.5f = true
	float oldWider;
	float dragY;
	float dragValue;
	bool snap;
	static const int padWidth = 45;
	static const int padHeight = 200;
	static const int padInterSpace = 20;
	static const int padWidthWide = padWidth * 2 + padInterSpace;
	
	DynamicIMTactile();
	void step() override;
	void onDragStart(EventDragStart &e) override;
	void onDragMove(EventDragMove &e) override;	
	void onMouseDown(EventMouseDown &e) override;
	void changeValue(float newVal);
};


#endif