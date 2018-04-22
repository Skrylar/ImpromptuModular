//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************



#include "ImpromptuModular.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);

	p->addModel(modelTwelveKey);
	p->addModel(modelWriteSeq32);
	p->addModel(modelWriteSeq64);
	p->addModel(modelPhraseSeq16);
}


SVGScrewRot::SVGScrewRot() {
	tw = new TransformWidget();
	addChild(tw);
	sw = new SVGWidget();
	tw->addChild(sw);
}
ScrewSilverRandomRot::ScrewSilverRandomRot() {
	float angle0_90 = randomUniform()*M_PI/2.0f;
	if ( angle0_90 > M_PI/6.0f && angle0_90 < 2.0f*M_PI/6.0f )
		sw->setSVG(SVG::load(assetPlugin(plugin, "res/Screw45.svg")));
	else
		sw->setSVG(SVG::load(assetPlugin(plugin, "res/Screw0.svg")));
	//sw->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/ScrewSilver.svg")));
	box.size = sw->box.size;
	tw->box.size = sw->box.size; 
	tw->identity();
	// Rotate SVG
	Vec center = sw->box.getCenter();
	tw->translate(center);
	tw->rotate(angle0_90);
	tw->translate(center.neg());
}


ScrewHole::ScrewHole(Vec posGiven) {
	box.size = Vec(16, 7);
	box.pos = Vec(posGiven.x, posGiven.y + 4);// nudgeX for realism 0 = no nudge
}
void ScrewHole::draw(NVGcontext *vg) {
	NVGcolor backgroundColor = nvgRGB(0x10, 0x10, 0x10); 
	NVGcolor borderColor = nvgRGB(0x20, 0x20, 0x20);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0.0, 0.0, box.size.x, box.size.y, 2.5f);
	nvgFillColor(vg, backgroundColor);
	nvgFill(vg);
	nvgStrokeWidth(vg, 1.0);
	nvgStrokeColor(vg, borderColor);
	nvgStroke(vg);
}


NVGcolor prepareDisplay(NVGcontext *vg, Rect *box) {
	NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38); 
	NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0.0, 0.0, box->size.x, box->size.y, 5.0);
	nvgFillColor(vg, backgroundColor);
	nvgFill(vg);
	nvgStrokeWidth(vg, 1.0);
	nvgStrokeColor(vg, borderColor);
	nvgStroke(vg);
	nvgFontSize(vg, 18);
	NVGcolor textColor = nvgRGB(0xaf, 0xd2, 0x2c);
	return textColor;
}


int moveIndex(int index, int indexNext, int numSteps) {
	if (indexNext < 0)
		index = numSteps - 1;
	else
	{
		if (indexNext - index >= 0) { // if moving right or same place
			if (indexNext >= numSteps)
				index = 0;
			else
				index = indexNext;
		}
		else { // moving left 
			if (indexNext >= numSteps)
				index = numSteps - 1;
			else
				index = indexNext;
		}
	}
	return index;
}


