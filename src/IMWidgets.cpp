//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from Valley Rack Free by Dale Johnson
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "IMWidgets.hpp"



// Dynamic SVGScrew


ScrewCircle::ScrewCircle(float _angle) {
	static const float highRadius = 1.4f;// radius for 0 degrees (screw looks like a +)
	static const float lowRadius = 1.1f;// radius for 45 degrees (screw looks like an x)
	angle = _angle;
	_angle = fabs(angle - M_PI/4.0f);
	radius = ((highRadius - lowRadius)/(M_PI/4.0f)) * _angle + lowRadius;
}
void ScrewCircle::draw(NVGcontext *vg) {
	NVGcolor backgroundColor = nvgRGB(0x72, 0x72, 0x72); 
	NVGcolor borderColor = nvgRGB(0x72, 0x72, 0x72);
	nvgBeginPath(vg);
	nvgCircle(vg, box.size.x/2.0f, box.size.y/2.0f, radius);// box, radius
	nvgFillColor(vg, backgroundColor);
	nvgFill(vg);
	nvgStrokeWidth(vg, 1.0);
	nvgStrokeColor(vg, borderColor);
	nvgStroke(vg);
}
DynamicSVGScrew::DynamicSVGScrew() {
    mode = nullptr;
    oldMode = -1;
 
	
	// for random rotated screw used in primary mode (code copied from ImpromptuModular.cpp ScrewSilverRandomRot::ScrewSilverRandomRot())
	// **********
	float angle0_90 = randomUniform()*M_PI/2.0f;
	//float angle0_90 = randomUniform() > 0.5f ? M_PI/4.0f : 0.0f;// for testing
	
	tw = new TransformWidget();
	addChild(tw);
	
	sw = new SVGWidget();
	tw->addChild(sw);
	//sw->setSVG(SVG::load(assetPlugin(plugin, "res/Screw.svg")));
	sw->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/ScrewSilver.svg")));
	
	sc = new ScrewCircle(angle0_90);
	sc->box.size = sw->box.size;
	tw->addChild(sc);
	
	box.size = sw->box.size;
	tw->box.size = sw->box.size; 
	tw->identity();
	// Rotate SVG
	Vec center = sw->box.getCenter();
	tw->translate(center);
	tw->rotate(angle0_90);
	tw->translate(center.neg());	

	// for fixed svg screw used in alternate mode
	// **********
	swAlt = new SVGWidget();
	swAlt->visible = false;
    addChild(swAlt);
}


void DynamicSVGScrew::addSVGalt(std::shared_ptr<SVG> svg) {
    if(!swAlt->svg) {
        swAlt->setSVG(svg);
    }
}

void DynamicSVGScrew::step() { // all code except middle if() from SVGPanel::step() in SVGPanel.cpp
    if (isNear(gPixelRatio, 1.0)) {
		// Small details draw poorly at low DPI, so oversample when drawing to the framebuffer
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        if ((*mode) == 0) {
			sw->visible = true;
			swAlt->visible = false;
		}
		else {
			sw->visible = false;
			swAlt->visible = true;
		}
        oldMode = *mode;
        dirty = true;
    }
	FramebufferWidget::step();
}



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
		onChange(*(new EventChange()));// required because of the way SVGSwitch changes images, we only change the frames above.
		//dirty = true;// dirty is not sufficient when changing via frames assignments above (i.e. onChange() is required)
    }
}



// Dynamic SVGKnob

DynamicSVGKnob::DynamicSVGKnob() {
    mode = nullptr;
    oldMode = -1;
	effect = new SVGWidget();
	//SVGKnob constructor automatically called
}

void DynamicSVGKnob::addFrameAll(std::shared_ptr<SVG> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 1) {
		setSVG(svg);
	}
}

void DynamicSVGKnob::addEffect(std::shared_ptr<SVG> svg) {
    effect->setSVG(svg);
	addChild(effect);
}

void DynamicSVGKnob::step() {
    if (isNear(gPixelRatio, 1.0)) {
		// Small details draw poorly at low DPI, so oversample when drawing to the framebuffer
        oversample = 2.f;
    }
    if(mode != nullptr && *mode != oldMode) {
        if ((*mode) == 0) {
			setSVG(framesAll[0]);
			effect->visible = false;
		}
		else {
			setSVG(framesAll[1]);
			effect->visible = true;
		}
        oldMode = *mode;
		dirty = true;
    }
	SVGKnob::step();
}



// Dynamic IMTactile

DynamicIMTactile::DynamicIMTactile() {
	randomizable = false;
	snap = false;
	
	wider = NULL;
	oldWider = -1.0f;

	box.size = Vec(45,200);
}

void DynamicIMTactile::step() {
   if(wider != nullptr && *wider != oldWider) {
        if ((*wider) > 0.5f) {
			box.size = Vec(130,200);
		}
		else {
			box.size = Vec(45,200);		
		}
        oldWider = *wider;
    }	
	ParamWidget::step();
}

void DynamicIMTactile::onDragStart(EventDragStart &e) {
	dragValue = value;
	dragY = gRackWidget->lastMousePos.y;
}

void DynamicIMTactile::onDragMove(EventDragMove &e) {
	float rangeValue = maxValue - minValue;// infinite not supported (not relevant)
	float newDragY = gRackWidget->lastMousePos.y;
	float delta = -(newDragY - dragY) * rangeValue / box.size.y;
	dragY = newDragY;
	dragValue += delta;
	float dragValueClamped = clamp2(dragValue, minValue, maxValue);
	if (snap)
		setValue(roundf(dragValueClamped));
	else
		setValue(dragValueClamped);
}

void DynamicIMTactile::onMouseDown(EventMouseDown &e) {
	float val = rescale(e.pos.y, box.size.y, 0.0f , minValue, maxValue);
	if (snap)
		setValue(roundf(val));
	else
		setValue(val);
	ParamWidget::onMouseDown(e);
}


