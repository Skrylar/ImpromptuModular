//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from Valley Rack Free by Dale Johnson
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "IMWidgets.hpp"


// Dynamic Panel (From Dale Johnson)

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


// Dynamic SVGPort

DynamicSVGPort::DynamicSVGPort() {
    mode = nullptr;
    oldMode = -1;
	//SVGPort constructor automatically called
}

void DynamicSVGPort::addFrame(std::shared_ptr<SVG> svg) {
    frames.push_back(svg);
    if(!background->svg)
        SVGPort::setSVG(svg);
}

void DynamicSVGPort::step() {
    if (isNear(gPixelRatio, 1.0)) {
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        background->setSVG(frames[*mode]);
        oldMode = *mode;
        dirty = true;
    }
}


// Dynamic Switch

DynamicSVGSwitch::DynamicSVGSwitch() {
    mode = nullptr;;
    oldMode = -1;
    sw = new SVGWidget();
    addChild(sw);
}

void DynamicSVGSwitch::addFrame(std::shared_ptr<SVG> svg) {
    frames.push_back(svg);
    if(!sw->svg) {
        sw->setSVG(svg);
		box.size = sw->box.size;
    }
}

void DynamicSVGSwitch::step() {
    if (isNear(gPixelRatio, 1.0)) {
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        sw->setSVG(frames[(*mode) * 2]);
        oldMode = *mode;
        dirty = true;
    }
}

void DynamicSVGSwitch::onChange(EventChange &e) {
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
