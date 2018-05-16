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


// Dynamic Jack (WIP, started from ValleyWidgets DynamicSwitch)

struct DynamicJackWidget : SVGPort {
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<SVG>> jacks;
    SVGWidget* visibleJack;

    DynamicJackWidget();
    void addJack(std::shared_ptr<SVG> svg);
    void step() override;
};

template <class TDynamicJack>
DynamicJackWidget* createDynamicJackWidget(Vec pos, Port::PortType type, Module *module, int portId,
                                               int* mode, Plugin* plugin) {
	DynamicJackWidget *dynJack = new TDynamicJack();
	dynJack->box.pos = pos;
	dynJack->type = type;
	dynJack->module = module;
	dynJack->portId = portId;
	
    dynJack->addJack(SVG::load(assetPlugin(plugin, "res/light/GateSeq64.svg")));
    dynJack->addJack(SVG::load(assetPlugin(plugin, "res/light/GateSeq64.svg")));
	//dynJack->box.size = dynJack->box.size;
	dynJack->mode = mode;
	
	return dynJack;
}


