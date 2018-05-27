//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from Valley Rack Free by Dale Johnson
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "IMWidgets.hpp"



// Dynamic SVGPanel

void PanelBorderWidget::draw(NVGcontext *vg) {  // carbon copy from SVGPanel.cpp
    NVGcolor borderColor = nvgRGBAf(0.5, 0.5, 0.5, 0.5);
    nvgBeginPath(vg);
    nvgRect(vg, 0.5, 0.5, box.size.x - 1.0, box.size.y - 1.0);
    nvgStrokeColor(vg, borderColor);
    nvgStrokeWidth(vg, 1.0);
    nvgStroke(vg);
}

DynamicSVGPanel::DynamicSVGPanel() {
    mode = nullptr;
    oldMode = -1;
    visiblePanel = new SVGWidget();
    addChild(visiblePanel);
    border = new PanelBorderWidget();
    addChild(border);
}

void DynamicSVGPanel::addPanel(std::shared_ptr<SVG> svg) {
    panels.push_back(svg);
    if(!visiblePanel->svg) {
        visiblePanel->setSVG(svg);
        box.size = visiblePanel->box.size.div(RACK_GRID_SIZE).round().mult(RACK_GRID_SIZE);
        border->box.size = box.size;
    }
}

void DynamicSVGPanel::step() { // all code except middle if() from SVGPanel::step() in SVGPanel.cpp
    if (isNear(gPixelRatio, 1.0)) {
		// Small details draw poorly at low DPI, so oversample when drawing to the framebuffer
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        visiblePanel->setSVG(panels[*mode]);
        oldMode = *mode;
        dirty = true;
    }
	FramebufferWidget::step();
}



// Dynamic SVGPanel with expansion

DynamicSVGPanelEx::DynamicSVGPanelEx() {
    mode = nullptr;
    oldMode = -1;
    expansion = nullptr;
    oldExpansion = -1;
    visiblePanel = new SVGWidget();
    addChild(visiblePanel);
    border = new PanelBorderWidget();
    addChild(border);
    visiblePanelEx = new SVGWidget();
	visiblePanelEx->visible = false;
    addChild(visiblePanelEx);
    borderEx = new PanelBorderWidget();
	borderEx->visible = false;
    addChild(borderEx);
}

void DynamicSVGPanelEx::addPanel(std::shared_ptr<SVG> svg) {// must add these before adding expansion panels
    panels.push_back(svg);
    if(!visiblePanel->svg) {
        visiblePanel->setSVG(svg);
        box.size = visiblePanel->box.size.div(RACK_GRID_SIZE).round().mult(RACK_GRID_SIZE);
        border->box.size = box.size;
    }
}

void DynamicSVGPanelEx::addPanelEx(std::shared_ptr<SVG> svg) {
    panelsEx.push_back(svg);
    if(!visiblePanelEx->svg) {
        visiblePanelEx->setSVG(svg);
		visiblePanelEx->box.pos.x = visiblePanel->box.size.x;
		borderEx->box.pos.x = visiblePanel->box.size.x;
		borderEx->box.size = visiblePanelEx->box.size.div(RACK_GRID_SIZE).round().mult(RACK_GRID_SIZE);
		if (expansion != nullptr && *expansion != 0)
			box.size.x = border->box.size.x + borderEx->box.size.x;
    }
}

void DynamicSVGPanelEx::step() { // all code except two middle if() from SVGPanel::step() in SVGPanel.cpp
    if (isNear(gPixelRatio, 1.0)) {
		// Small details draw poorly at low DPI, so oversample when drawing to the framebuffer
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        visiblePanel->setSVG(panels[*mode]);
		visiblePanelEx->setSVG(panelsEx[*mode]);
        oldMode = *mode;
        dirty = true;
    }
    if(expansion != nullptr && *expansion != oldExpansion) {
		if (*expansion == 0) {
			if (oldExpansion != -1) {
				box.size.x = border->box.size.x;
				borderEx->visible = false;
				visiblePanelEx->visible = false;
			}
		}
		else {
			box.size.x = border->box.size.x + borderEx->box.size.x;
			borderEx->visible = true;
			visiblePanelEx->visible = true;
		}
        oldExpansion = *expansion;
        dirty = true;
    }
	FramebufferWidget::step();
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
		// Small details draw poorly at low DPI, so oversample when drawing to the framebuffer
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        background->setSVG(frames[*mode]);
        oldMode = *mode;
        dirty = true;
    }
	Port::step();
}



// Dynamic SVGSwitch

DynamicSVGSwitch::DynamicSVGSwitch() {
    mode = nullptr;
    oldMode = -1;
	//SVGSwitch constructor automatically called
}

void DynamicSVGSwitch::addFrameAll(std::shared_ptr<SVG> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 2) {
		addFrame(framesAll[0]);
		addFrame(framesAll[1]);
	}
}

void DynamicSVGSwitch::step() {
    if (isNear(gPixelRatio, 1.0)) {
		// Small details draw poorly at low DPI, so oversample when drawing to the framebuffer
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        if ((*mode) == 0) {
			frames[0]=framesAll[0];
			frames[1]=framesAll[1];
		}
		else {
			frames[0]=framesAll[2];
			frames[1]=framesAll[3];
		}
        oldMode = *mode;
		onChange(*(new EventChange()));
    }
}


