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
	p->addModel(modelPhraseSeq16);
	p->addModel(modelPhraseSeq32);
	p->addModel(modelGateSeq64);
	p->addModel(modelSemiModularSynth);
	p->addModel(modelWriteSeq32);
	p->addModel(modelWriteSeq64);
	p->addModel(modelBlankPanel);
}


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
ScrewSilverRandomRot::ScrewSilverRandomRot() {
	float angle0_90 = randomUniform()*M_PI/2.0f;
	//float angle0_90 = randomUniform() > 0.5f ? M_PI/4.0f : 0.0f;// for testing
	
	tw = new TransformWidget();
	addChild(tw);
	
	sw = new SVGWidget();
	tw->addChild(sw);
	//sw->setSVG(SVG::load(assetPlugin(plugin, "res/Screw0.svg")));
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
}


ScrewHole::ScrewHole(Vec posGiven) {
	box.size = Vec(16, 7);
	box.pos = Vec(posGiven.x, posGiven.y + 4);// nudgeX for realism, 0 = no nudge
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


bool moveIndexRunMode(int* index, int numSteps, int runMode, int* history) {		
	bool crossBoundary = false;
	
	if (runMode == MODE_REV) {// reverse; history base is 1000 (not needed)
		(*history) = 1000;
		(*index)--;
		if ((*index) < 0) {
			(*index) = numSteps - 1;
			crossBoundary = true;
		}
	}
	else if (runMode == MODE_PPG) {// forward-reverse; history base is 2000
		if ((*history) != 2000 && (*history) != 2001) // 2000 means going forward, 2001 means going reverse
			(*history) = 2000;
		if ((*history) == 2000) {// forward phase
			(*index)++;
			if ((*index) >= numSteps) {
				(*index) = numSteps - 1;
				(*history) = 2001;
			}
		}
		else {// it is 2001; reverse phase
			(*index)--;
			if ((*index) < 0) {
				(*index) = 0;
				(*history) = 2000;
				crossBoundary = true;
			}
		}
	}
	else if (runMode == MODE_BRN) {// brownian random; history base is 3000
		if ( (*history) < 3000 || ((*history) > (3000 + numSteps)) ) 
			(*history) = 3000 + numSteps;
		(*index) += (randomu32() % 3) - 1;
		if ((*index) >= numSteps) {
			(*index) = 0;
		}
		if ((*index) < 0) {
			(*index) = numSteps - 1;
		}
		(*history)--;
		if ((*history) <= 3000) {
			(*history) = 3000 + numSteps;
			crossBoundary = true;
		}
	}
	else if (runMode == MODE_RND) {// random; history base is 4000
		if ( (*history) < 4000 || ((*history) > (4000 + numSteps)) ) 
			(*history) = 4000 + numSteps;
		(*index) = (randomu32() % numSteps) ;
		(*history)--;
		if ((*history) <= 4000) {
			(*history) = 4000 + numSteps;
			crossBoundary = true;
		}
	}
	else {// MODE_FWD  forward; history base is 0 (not needed)
		(*history) = 0;
		(*index)++;
		if ((*index) >= numSteps) {
			(*index) = 0;
			crossBoundary = true;
		}
	}

	return crossBoundary;
}
