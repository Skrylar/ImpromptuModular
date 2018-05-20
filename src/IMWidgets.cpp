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


// Dynamic Jack

DynamicJackWidget::DynamicJackWidget() {
    mode = nullptr;;
    oldMode = -1;
	
	shadow = new CircularShadow();
	addChild(shadow);
	// Avoid breakage if plugins fail to call setSVG()
	// In that case, just disable the shadow.
	shadow->box.size = Vec();

    visibleJack = new SVGWidget();
    addChild(visibleJack);
}

void DynamicJackWidget::addJack(std::shared_ptr<SVG> svg) {
    jacks.push_back(svg);
    if(!visibleJack->svg) {
        visibleJack->setSVG(svg);
		box.size = visibleJack->box.size;
		shadow->box.size = visibleJack->box.size;
		shadow->box.pos = Vec(0, visibleJack->box.size.y * 0.1);	
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


// Dynamic Switch

DynamicSwitchWidget::DynamicSwitchWidget() {
    mode = nullptr;;
    oldMode = -1;
    sw = new SVGWidget();
    addChild(sw);
}

void DynamicSwitchWidget::addFrame(std::shared_ptr<SVG> svg) {
    frames.push_back(svg);
    if(!sw->svg) {
        sw->setSVG(svg);
		box.size = sw->box.size;
    }
}

void DynamicSwitchWidget::step() {
    if (isNear(gPixelRatio, 1.0)) {
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        sw->setSVG(frames[(*mode) * 2]);
        oldMode = *mode;
        dirty = true;
    }
}

void DynamicSwitchWidget::onChange(EventChange &e) {
	assert(frames.size() > 0);
	float valueScaled = rescale(value, minValue, maxValue, 0, frames.size()/2 - 1);
	int index = clamp((int) roundf(valueScaled), 0, (int) frames.size() - 1);
	if(mode != nullptr)
		index += (*mode) * 2;
	assert(index < (int)frames.size());
	sw->setSVG(frames[index]);
	dirty = true;
	ParamWidget::onChange(e);
}
