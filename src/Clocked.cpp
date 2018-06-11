//***********************************************************************************************
//Chain-able clock module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé, Nigel Sixsmith and Xavier Belmont
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct Clocked : Module {
	enum ParamIds {
		ENUMS(RATIO_PARAMS, 4),// master is index 0
		ENUMS(SWING_PARAMS, 4),// master is index 0
		ENUMS(PW_PARAMS, 4),// master is index 0
		RESET_PARAM,
		RUN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(PW_INPUTS, 4),// master is index 0
		RESET_INPUT,
		RUN_INPUT,
		BPM_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CLK_OUTPUTS, 4),// master is index 0
		RESET_OUTPUT,
		RUN_OUTPUT,
		BPM_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RESET_LIGHT,
		RUN_LIGHT,
		ENUMS(CLK_LIGHTS, 4),// master is index 0
		NUM_LIGHTS
	};
	
	
	// Ratio info
	static const int numRatios = 33;
	float ratioValues[numRatios] = {1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 
									17, 19, 23, 29, 31, 32, 37, 41, 43, 47, 48, 53, 59, 61, 64};

	// BPM info
	static const int bpmMax = 350;
	static const int bpmMin = 10;
	static const int bpmRange = bpmMax - bpmMin;
		
	// Knob utilities
	float getBeatsPerMinute(void) {
		float bpm = 0.0f;
		if (inputs[BPM_INPUT].active)
			bpm = round( (inputs[BPM_INPUT].value / 10.0f) * bpmRange + bpmMin );
		else
			bpm = round( params[RATIO_PARAMS + 0].value );
		return bpm;
	}
	float getRatio(int ratioKnobIndex) {
		// ratioKnobIndex is 1 for first ratio knob, 0 is master BPM ratio, which is implicitly 1.0f
		float ret = 1.0f;
		if (ratioKnobIndex > 0) {
			bool isDivision = false;
			int i = (int) round( params[RATIO_PARAMS + ratioKnobIndex].value );// [ -(numRatios-1) ; (numRatios-1) ]
			if (i < 0) {
				i *= -1;
				isDivision = true;
			}
			if (i >= numRatios) {
				i = numRatios - 1;
			}
			ret = ratioValues[i];
			if (isDivision) 
				ret = 1.0f / ret;
		}
		return ret;
	}
	
	// Need to save
	int panelTheme = 0;
	bool running;
	
	// No need to save
	float resetLight = 0.0f;
	float bpm;
	long steps[4];// 0 when stopped, [1 to 2*sampleRate] for clock steps (*2 is because of swing, so we do groups of 2 periods)
	long lengths[4];// dependant of steps[], irrelevant values when a step is 0
	float ratios[4];// dependant of steps[], irrelevant outside of step()
	float newRatios[4];// dependant of steps[], irrelevant outside of step()
	
	
	SchmittTrigger resetTrigger;
	SchmittTrigger runTrigger;
	PulseGenerator resetPulse;
	PulseGenerator runPulse;
	

	Clocked() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}
	

	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		running = false;
		bpm = -1.0f;// unseen
		for (int i = 0; i < 4; i++) {
			steps[i] = 0;
			lengths[i] = 0;
			ratios[i] = 0.0f;
			newRatios[i] = 0.0f;
		}
	}
	
	
	// widgets randomized before onRandomize() is called
	void onRandomize() override {
		onReset();
	}

	
	json_t *toJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		return rootJ;
	}


	// widgets loaded before this fromJson() is called
	void fromJson(json_t *rootJ) override {
		onReset();
		
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		float sampleRate = engineGetSampleRate();
		float sampleTime = engineGetSampleTime();
		

		//********** Buttons, knobs, switches and inputs **********
		
		// Run button
		if (runTrigger.process(params[RUN_PARAM].value + inputs[RUN_INPUT].value)) {
			running = !running;
			if (!running) {
				onReset();
			}
			runPulse.trigger(0.001f);
		}

		// BPM input and knob
		float newBpm = getBeatsPerMinute();
		if (newBpm != bpm) {
			bpm = newBpm; 
			for (int i = 0; i < 4; i++) {
				steps[i] = 0;// retrigger everything
			}
		}
		float bps = bpm / 60.0f;

		// Ratio knobs
		bool syncRatios[4] = {false, false, false, false};// 0 index unused
		for (int i = 1; i < 4; i++) {
			newRatios[i] = getRatio(i);
			if (newRatios[i] != ratios[i])
				syncRatios[i] = true;
		}
		
		
		//********** Clock and reset **********
		
		// Clock
		if (running) {
			// master clock
			if (steps[0] == 0) {
				lengths[0] = (long) (2.0f * sampleRate / (bps));
				steps[0] = 1;// the 0 step does not actually to consume a sample step, it is used to reset every double-period so that lengths can be re-computed
				for (int i = 1; i < 4; i++) {// see if ratio knobs uninitialized or changed
					if (syncRatios[i]) {
						steps[i] = 0;// force refresh of that sub-clock
						ratios[i] = newRatios[i];
						syncRatios[i] = false;
					}
				}
			}
			// three sub-clocks
			for (int i = 1; i < 4; i++) {
				if (steps[i] == 0) {
					lengths[i] = (long) (2.0f * sampleRate / (bps * ratios[i]));
					steps[i] = 1;// the 0 step does not actually to consume a sample step, it is used to reset every double-period so that lengths can be re-computed
				}
			}
		}
		
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			resetLight = 1.0f;
			resetPulse.trigger(0.001f);
			for (int i = 0; i < 4; i++) {
				steps[i] = 0;
			}
		}
		else
			resetLight -= (resetLight / lightLambda) * sampleTime;		
		
		
		//********** Outputs and lights **********
			
		// Clock outputs
		for (int i = 0; i < 4; i++) {
			outputs[CLK_OUTPUTS + i].value = calcClock(i, sampleRate);
		}
		
		outputs[RESET_OUTPUT].value = (resetPulse.process(sampleTime) ? 10.0f : 0.0f);
		outputs[RUN_OUTPUT].value = (runPulse.process(sampleTime) ? 10.0f : 0.0f);
		outputs[BPM_OUTPUT].value =  (inputs[BPM_INPUT].active ? inputs[BPM_INPUT].value : ( (bpm - bpmMin) / bpmRange ) * 10.0f );
			
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	
		
		// Run light
		lights[RUN_LIGHT].value = running;
		
		// Sync lights
		for (int i = 1; i < 4; i++) {
			lights[CLK_LIGHTS + i].value = (syncRatios[i] && running) ? 1.0f: 0.0f;
		}

		for (int i = 0; i < 4; i++) {
			if (steps[i] != 0) {
				if (steps[i] >= lengths[i])
					steps[i] = 0;
				else 
					steps[i]++;
			}
		}
	}
	
	float calcClock(int clkIndex, float sampleRate) {
		float ret = 0.0f;
		
		if (steps[clkIndex] > 0) {
			float swParam = params[SWING_PARAMS + clkIndex].value;
			swParam *= (swParam * (swParam > 0.0f ? 1.0f : -1.0f));// non-linear behavior for more sensitivity at center: f(x) = x^2 * sign(x)
			
			// all following values are in samples numbers, whether long or float
			float onems = sampleRate / 1000.0f;
			float period = ((float)lengths[clkIndex]) / 2.0f;
			float swing = (period - 2.0f * onems) * swParam;
			float p2min = onems;
			float p2max = period - onems - fabs(swing);
			if (p2max < p2min) {
				p2max = p2min;
			}
			
			//long p1 = 1;// implicit, no need 
			long p2 = (long)((p2max - p2min) * params[PW_PARAMS + clkIndex].value + p2min + 0.5f);
			long p3 = (long)(period + swing);
			long p4 = ((long)(period + swing)) + p2;
			
			ret = ( ((steps[clkIndex] < p2)) || ((steps[clkIndex] < p4) && (steps[clkIndex] > p3)) ) ? 10.0f : 0.0f;
		}
		return ret;
	}
};


struct ClockedWidget : ModuleWidget {

	struct RatioDisplayWidget : TransparentWidget {
		Clocked *module;
		int knobIndex;
		std::shared_ptr<Font> font;
		char displayStr[4];
		
		RatioDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box);
			nvgFontFaceId(vg, font->handle);
			//nvgTextLetterSpacing(vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			nvgText(vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(vg, textColor);
			if (knobIndex > 0) {// ratio to display
				bool isDivision = false;
				float ratio = module->getRatio(knobIndex);
				if (ratio < 1.0f) {
					ratio = 1.0f / ratio;
					isDivision = true;
				}
				int ratioDoubled = (int) ((2.0f * ratio) + 0.5f);
				if ( (ratioDoubled % 2) == 1 )
					snprintf(displayStr, 4, "%c,5", 0x30 + ratioDoubled / 2);
				else {
					snprintf(displayStr, 4, "X%2u", ratioDoubled / 2);
					if (isDivision)
						displayStr[0] = '/';
				}
			}
			else {// BPM to display
				snprintf(displayStr, 4, "%3u", (unsigned) (fabs(module->getBeatsPerMinute()) + 0.5f));
			}
			displayStr[3] = 0;// more safety
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	struct PanelThemeItem : MenuItem {
		Clocked *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Clocked *module = dynamic_cast<Clocked*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = lightPanelID;// ImpromptuModular.hpp
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = darkPanelID;// ImpromptuModular.hpp
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);

		return menu;
	}	
	
	
	ClockedWidget(Clocked *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/Clocked.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/dark/Clocked_dark.svg")));
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws
		addChild(createDynamicScrew<IMScrew>(Vec(15, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(box.size.x-30, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(15, 365), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(box.size.x-30, 365), &module->panelTheme));


		static const int rowRuler0 = 50;//reset,run inputs, master knob and bpm display
		static const int rowRuler1 = rowRuler0 + 55;// reset,run switches
		//
		static const int rowRuler2 = rowRuler1 + 55;// clock 1
		static const int rowSpacingClks = 50;
		static const int rowRuler5 = rowRuler2 + rowSpacingClks * 2 + 55;// reset,run outputs, pw inputs
		
		
		static const int colRulerL = 18;// reset input and button, ratio knobs
		// First two rows and last row
		static const int colRulerSpacingT = 47;
		static const int colRulerT1 = colRulerL + colRulerSpacingT;// run input and button
		static const int colRulerT2 = colRulerT1 + colRulerSpacingT;// in and pwMaster inputs
		static const int colRulerT3 = colRulerT2 + colRulerSpacingT;// swingMaster knob
		static const int colRulerT4 = colRulerT3 + colRulerSpacingT;// pwMaster knob
		static const int colRulerT5 = colRulerT4 + colRulerSpacingT;// clkMaster output
		// Three clock rows
		static const int colRulerM0 = colRulerL + 5;// ratio knobs
		static const int colRulerM1 = colRulerL + 60;// ratio displays
		static const int colRulerM2 = colRulerT3;// swingX knobs
		static const int colRulerM3 = colRulerT4;// pwX knobs
		static const int colRulerM4 = colRulerT5;// clkX outputs
		
		RatioDisplayWidget *displayRatios[4];
		
		// Row 0
		// Reset input
		addInput(createDynamicPort<IMPort>(Vec(colRulerL, rowRuler0), Port::INPUT, module, Clocked::RESET_INPUT, &module->panelTheme));
		// Run input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT1, rowRuler0), Port::INPUT, module, Clocked::RUN_INPUT, &module->panelTheme));
		// In input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT2, rowRuler0), Port::INPUT, module, Clocked::BPM_INPUT, &module->panelTheme));
		// Master knob
		addParam(ParamWidget::create<IMBigSnapKnob>(Vec(colRulerT3 + 6 + offsetIMBigKnob, rowRuler0 + offsetIMBigKnob), module, Clocked::RATIO_PARAMS + 0, (float)(module->bpmMin), (float)(module->bpmMax), 120.0f));		
		// BPM display
		displayRatios[0] = new RatioDisplayWidget();
		displayRatios[0]->box.pos = Vec(colRulerT4 + 16, rowRuler0 + vOffsetDisplay);
		displayRatios[0]->box.size = Vec(55, 30);// 3 characters
		displayRatios[0]->module = module;
		displayRatios[0]->knobIndex = 0;
		addChild(displayRatios[0]);
		
		// Row 1
		// Reset LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(colRulerL + offsetLEDbezel, rowRuler1 + offsetLEDbezel), module, Clocked::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRulerL + offsetLEDbezel + offsetLEDbezelLight, rowRuler1 + offsetLEDbezel + offsetLEDbezelLight), module, Clocked::RESET_LIGHT));
		// Run LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(colRulerT1 + offsetLEDbezel, rowRuler1 + offsetLEDbezel), module, Clocked::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRulerT1 + offsetLEDbezel + offsetLEDbezelLight, rowRuler1 + offsetLEDbezel + offsetLEDbezelLight), module, Clocked::RUN_LIGHT));
		// PW master input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT2, rowRuler1), Port::INPUT, module, Clocked::PW_INPUTS + 0, &module->panelTheme));
		// Swing master knob
		addParam(ParamWidget::create<IMSmallKnob>(Vec(colRulerT3 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Clocked::SWING_PARAMS + 0, -1.0f, 1.0f, 0.0f));
		// PW master knob
		addParam(ParamWidget::create<IMSmallKnob>(Vec(colRulerT4 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Clocked::PW_PARAMS + 0, 0.0f, 1.0f, 0.5f));
		// Clock master out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler1), Port::OUTPUT, module, Clocked::CLK_OUTPUTS + 0, &module->panelTheme));
		
		
		// Row 2-4 (sub clocks)		
		for (int i = 0; i < 3; i++) {
			// Ratio1 knob
			addParam(ParamWidget::create<IMBigSnapKnob>(Vec(colRulerM0 + offsetIMBigKnob, rowRuler2 + i * rowSpacingClks + offsetIMBigKnob), module, Clocked::RATIO_PARAMS + 1 + i, ((float)(module->numRatios - 1))*-1.0f, ((float)(module->numRatios - 1)), 0.0f));		
			// Ratio display
			displayRatios[i + 1] = new RatioDisplayWidget();
			displayRatios[i + 1]->box.pos = Vec(colRulerM1, rowRuler2 + i * rowSpacingClks + vOffsetDisplay);
			displayRatios[i + 1]->box.size = Vec(55, 30);// 3 characters
			displayRatios[i + 1]->module = module;
			displayRatios[i + 1]->knobIndex = i + 1;
			addChild(displayRatios[i + 1]);
			// Sync light
			addChild(ModuleLightWidget::create<SmallLight<RedLight>>(Vec(colRulerM1 + 62, rowRuler2 + i * rowSpacingClks + 10), module, Clocked::CLK_LIGHTS + i + 1));		
			// Swing knobs
			addParam(ParamWidget::create<IMSmallKnob>(Vec(colRulerM2 + offsetIMSmallKnob, rowRuler2 + i * rowSpacingClks + offsetIMSmallKnob), module, Clocked::SWING_PARAMS + 1 + i, -1.0f, 1.0f, 0.0f));
			// PW knobs
			addParam(ParamWidget::create<IMSmallKnob>(Vec(colRulerM3 + offsetIMSmallKnob, rowRuler2 + i * rowSpacingClks + offsetIMSmallKnob), module, Clocked::PW_PARAMS + 1 + i, 0.0f, 1.0f, 0.5f));
			// Clock outs
			addOutput(createDynamicPort<IMPort>(Vec(colRulerM4, rowRuler2 + i * rowSpacingClks), Port::OUTPUT, module, Clocked::CLK_OUTPUTS + 1 + i, &module->panelTheme));
		}

		// Last row
		// Reset out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerL, rowRuler5), Port::OUTPUT, module, Clocked::RESET_OUTPUT, &module->panelTheme));
		// Run out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT1, rowRuler5), Port::OUTPUT, module, Clocked::RUN_OUTPUT, &module->panelTheme));
		// Out out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT2, rowRuler5), Port::OUTPUT, module, Clocked::BPM_OUTPUT, &module->panelTheme));
		// PW 1 input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT3, rowRuler5), Port::INPUT, module, Clocked::PW_INPUTS + 1, &module->panelTheme));
		// PW 2 input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT4, rowRuler5), Port::INPUT, module, Clocked::PW_INPUTS + 2, &module->panelTheme));
		// PW 3 input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler5), Port::INPUT, module, Clocked::PW_INPUTS + 3, &module->panelTheme));
	}
};

Model *modelClocked = Model::create<Clocked, ClockedWidget>("Impromptu Modular", "Clocked", "Clocked", CLOCK_TAG);
