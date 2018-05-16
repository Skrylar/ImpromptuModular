//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//This is code from Valley Rack Free by Dale Johnson
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "IMWidgets.hpp"


// Dynamic Panel

void PanelBorderWidget::draw(NVGcontext *vg) {
    NVGcolor borderColor = nvgRGBAf(0.5, 0.5, 0.5, 0.5);
    nvgBeginPath(vg);
    nvgRect(vg, 0.5, 0.5, box.size.x - 1.0, box.size.y - 1.0);
    nvgStrokeColor(vg, borderColor);
    nvgStrokeWidth(vg, 1.0);
    nvgStroke(vg);
}

DynamicPanelWidget::DynamicPanelWidget() {
    mode = nullptr;
    oldMode = -1;
    visiblePanel = new SVGWidget();
    addChild(visiblePanel);
    border = new PanelBorderWidget();
    addChild(border);
}

void DynamicPanelWidget::addPanel(std::shared_ptr<SVG> svg) {
    panels.push_back(svg);
    if(!visiblePanel->svg) {
        visiblePanel->setSVG(svg);
        box.size = visiblePanel->box.size.div(RACK_GRID_SIZE).round().mult(RACK_GRID_SIZE);
        border->box.size = box.size;
    }
}

void DynamicPanelWidget::step() {
    if (isNear(gPixelRatio, 1.0)) {
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        visiblePanel->setSVG(panels[*mode]);
        oldMode = *mode;
        dirty = true;
    }
}


// Dynamic Jack (WIP)

DynamicJackWidget::DynamicJackWidget() {
    mode = nullptr;
    oldMode = -1;
    visibleJack = new SVGWidget();
    addChild(visibleJack);
}

void DynamicJackWidget::addJack(std::shared_ptr<SVG> svg) {
    jacks.push_back(svg);
    if(!visibleJack->svg) {
        visibleJack->setSVG(svg);
        box.size = visibleJack->box.size.div(RACK_GRID_SIZE).round().mult(RACK_GRID_SIZE);
    }
}

void DynamicJackWidget::step() {
    if (isNear(gPixelRatio, 1.0)) {
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        visibleJack->setSVG(jacks[*mode]);
        oldMode = *mode;
        dirty = true;
    }
}

