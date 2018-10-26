//***********************************************************************************************
//Clockable ramp LFO module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé and Nigel Sixsmith
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"


struct ClockedLFO : Module {
	enum ParamIds {
		ENUMS(RATIO_PARAMS, 4),// master is index 0
		ENUMS(SWING_PARAMS, 4),// master is index 0
		ENUMS(PW_PARAMS, 4),// master is index 0
		RESET_PARAM,
		RUN_PARAM,
		ENUMS(DELAY_PARAMS, 4),// index 0 is unused
		// -- 0.6.9 ^^
		BPMMODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(PW_INPUTS, 4),// master is index 0
		RESET_INPUT,
		RUN_INPUT,
		BPM_INPUT,
		ENUMS(SWING_INPUTS, 4),// master is index 0
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
		ENUMS(CLK_LIGHTS, 4),// master is index 0 (not used)
		ENUMS(BPMSYNC_LIGHT, 2),// room for GreenRed
		NUM_LIGHTS
	};
	
	
	// Constants
	static const int bpmMax = 300;
	static const int bpmMin = 30;
	static constexpr float masterLengthMax = 120.0f / bpmMin;// a length is a double period
	static constexpr float masterLengthMin = 120.0f / bpmMax;// a length is a double period
	static constexpr float delayInfoTime = 3.0f;// seconds
	static constexpr float swingInfoTime = 2.0f;// seconds
	
	// Need to save
	int panelTheme = 0;
	int expansion = 0;
	bool displayDelayNoteMode = true;
	bool emitResetOnStopRun = false;
	int ppqn = 4;
	bool running;
	
	// No need to save
	bool syncRatios[4];// 0 index unused
	int extPulseNumber;// 0 to ppqn - 1
	double extIntervalTime;
	double timeoutTime;
	float newMasterLength;
	float masterLength;
	long editingBpmMode;// 0 when no edit bpmMode, downward step counter timer when edit, negative upward when show can't edit ("--") 
	
	bool scheduledReset = false;
	int notifyingSource[4] = {-1, -1, -1, -1};
	long notifyInfo[4] = {0l, 0l, 0l, 0l};// downward step counter when swing to be displayed, 0 when normal display
	long cantRunWarning = 0l;// 0 when no warning, positive downward step counter timer when warning
	unsigned int lightRefreshCounter = 0;
	float resetLight = 0.0f;
	SchmittTrigger resetTrigger;
	SchmittTrigger runTrigger;
	SchmittTrigger bpmDetectTrigger;
	SchmittTrigger bpmModeTrigger;
	PulseGenerator resetPulse;
	PulseGenerator runPulse;

	
	inline float getBpmKnob() {
		float knobBPM = params[RATIO_PARAMS + 0].value;// already integer BPM since using snap knob
		if (knobBPM < (float)bpmMin)// safety in case module step() starts before widget defaults take effect.
			return (float)bpmMin;	
		return knobBPM;
	}
	
	
	// called from the main thread (step() can not be called until all modules created)
	ClockedLFO() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}
	

	void onReset() override {
		running = false;
		editingBpmMode = 0l;
		resetClockedLFO();		
	}
	
	
	void onRandomize() override {
		resetClockedLFO();
	}

	
	void resetClockedLFO() {
		for (int i = 0; i < 4; i++) {
			syncRatios[i] = false;
		}
		extPulseNumber = -1;
		extIntervalTime = 0.0;
		timeoutTime = 2.0 / ppqn + 0.1;
		if (inputs[BPM_INPUT].active) {
			newMasterLength = 1.0f;// 120 BPM
		}
		else
			newMasterLength = 120.0f / getBpmKnob();
		newMasterLength = clamp(newMasterLength, masterLengthMin, masterLengthMax);
		masterLength = newMasterLength;
	}	
	
	
	json_t *toJson() override {
		json_t *rootJ = json_object();
		
		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// expansion
		json_object_set_new(rootJ, "expansion", json_integer(expansion));

		// displayDelayNoteMode
		json_object_set_new(rootJ, "displayDelayNoteMode", json_boolean(displayDelayNoteMode));
		
		// emitResetOnStopRun
		json_object_set_new(rootJ, "emitResetOnStopRun", json_boolean(emitResetOnStopRun));
		
		// ppqn
		json_object_set_new(rootJ, "ppqn", json_integer(ppqn));
		
		return rootJ;
	}


	void fromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// expansion
		json_t *expansionJ = json_object_get(rootJ, "expansion");
		if (expansionJ)
			expansion = json_integer_value(expansionJ);

		// displayDelayNoteMode
		json_t *displayDelayNoteModeJ = json_object_get(rootJ, "displayDelayNoteMode");
		if (displayDelayNoteModeJ)
			displayDelayNoteMode = json_is_true(displayDelayNoteModeJ);

		// emitResetOnStopRun
		json_t *emitResetOnStopRunJ = json_object_get(rootJ, "emitResetOnStopRun");
		if (emitResetOnStopRunJ)
			emitResetOnStopRun = json_is_true(emitResetOnStopRunJ);

		// ppqn
		json_t *ppqnJ = json_object_get(rootJ, "ppqn");
		if (ppqnJ)
			ppqn = clamp(json_integer_value(ppqnJ), 4, 24);

		scheduledReset = true;
	}

	
	void onSampleRateChange() override {
		resetClockedLFO();
	}		
	

	void step() override {		
		double sampleRate = (double)engineGetSampleRate();
		double sampleTime = 1.0 / sampleRate;// do this here since engineGetSampleRate() returns float

		// Scheduled reset
		if (scheduledReset) {
			resetClockedLFO();		
			scheduledReset = false;
		}
		
		// Reset (has to be near top because it sets steps to 0, and 0 not a real step (clock section will move to 1 before reaching outputs)
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			resetLight = 1.0f;
			resetPulse.trigger(0.001f);
			resetClockedLFO();	
		}	

		// BPM mode
		if (bpmModeTrigger.process(params[BPMMODE_PARAM].value)) {// no input refresh here, not worth it just for one button (this is the only potential button to input-refresh optimize)
			if (editingBpmMode != 0ul) {// force active before allow change
				if (ppqn == 4)
					ppqn = 8;
				else if (ppqn == 8)
					ppqn = 12;
				else if (ppqn == 12)
					ppqn = 24;
				else 
					ppqn = 4;
			}
			editingBpmMode = (long) (3.0 * sampleRate / displayRefreshStepSkips);
		}
		
		// BPM input and knob
		newMasterLength = masterLength;
		if (inputs[BPM_INPUT].active) { 
			bool trigBpmInValue = bpmDetectTrigger.process(inputs[BPM_INPUT].value);
			
			// BPM Detection method
			// rising edge detect
			if (trigBpmInValue) {
				if (!running) {
					// this must be the only way to start runnning when in bpmDetectionMode or else
					//   when manually starting, the clock will not know which pulse is the 1st of a ppqn set
					//runPulse.trigger(0.001f); don't need this since slaves will detect the same thing
					running = true;
					runPulse.trigger(0.001f);
					resetClockedLFO();
				}
				if (running) {
					extPulseNumber++;
					if (extPulseNumber >= ppqn * 2)// *2 because working with double_periods
						extPulseNumber = 0;
					if (extPulseNumber == 0)// if first pulse, start interval timer
						extIntervalTime = 0.0;
					else {
						// all other ppqn pulses except the first one. now we have an interval upon which to plan a strecth 
						double timeLeft = extIntervalTime * (double)(ppqn * 2 - extPulseNumber) / ((double)extPulseNumber);
						newMasterLength = clamp(timeLeft, masterLengthMin / 1.5f, masterLengthMax * 1.5f);// extended range for better sync ability (20-450 BPM)
						timeoutTime = extIntervalTime * ((double)(1 + extPulseNumber) / ((double)extPulseNumber)) + 0.1;
					}
				}
			}
			if (running) {
				extIntervalTime += sampleTime;
				if (extIntervalTime > timeoutTime) {
					//info("*** extIntervalTime = %f, timeoutTime = %f",extIntervalTime, timeoutTime);
					running = false;
					runPulse.trigger(0.001f);
					resetClockedLFO();
					if (emitResetOnStopRun) {
						resetPulse.trigger(0.001f);
					}
				}
			}
		}
		else {// BPM_INPUT not active
			newMasterLength = 120.0f; // clamp(120.0f / getBpmKnob(), masterLengthMin, masterLengthMax);
		}
		if (newMasterLength != masterLength) {
			masterLength = newMasterLength;
		}
		
		
		// LFO Output (only a BPM CV for now!!!!!)
		outputs[BPM_OUTPUT].value =  log2f(1.0f / masterLength);
			
		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			// Reset light
			lights[RESET_LIGHT].value =	resetLight;	
			resetLight -= (resetLight / lightLambda) * (float)sampleTime * displayRefreshStepSkips;
			
			// Run light
			lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;
			
			// BPM light
			bool warningFlashState = true;
			if (cantRunWarning > 0l) 
				warningFlashState = calcWarningFlash(cantRunWarning, (long) (0.7 * sampleRate / displayRefreshStepSkips));
			lights[BPMSYNC_LIGHT + 0].value = (warningFlashState && inputs[BPM_INPUT].active) ? 1.0f : 0.0f;
			lights[BPMSYNC_LIGHT + 1].value = (warningFlashState && inputs[BPM_INPUT].active) ? (float)((ppqn - 4)*(ppqn - 4))/400.0f : 0.0f;			
			
			// ratios synched lights
			for (int i = 1; i < 4; i++)
				lights[CLK_LIGHTS + i].value = (syncRatios[i] && running) ? 1.0f: 0.0f;

			// info notification counters
			for (int i = 0; i < 4; i++) {
				notifyInfo[i]--;
				if (notifyInfo[i] < 0l)
					notifyInfo[i] = 0l;
			}
			if (cantRunWarning > 0l)
				cantRunWarning--;
			editingBpmMode--;
			if (editingBpmMode < 0l)
				editingBpmMode = 0l;
		}// lightRefreshCounter
	}// step()
};


struct ClockedLFOWidget : ModuleWidget {
	ClockedLFO *module;
	DynamicSVGPanel *panel;
	int oldExpansion;
	int expWidth = 60;
	IMPort* expPorts[6];


	struct RatioDisplayWidget : TransparentWidget {
		ClockedLFO *module;
		int knobIndex;
		std::shared_ptr<Font> font;
		char displayStr[4];
		const std::string delayLabelsClock[8] = {"D 0", "/16",   "1/8",  "1/4", "1/3",     "1/2", "2/3",     "3/4"};
		const std::string delayLabelsNote[8]  = {"D 0", "/64",   "/32",  "/16", "/8t",     "1/8", "/4t",     "/8d"};

		
		RatioDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box, 18);
			nvgFontFaceId(vg, font->handle);
			//nvgTextLetterSpacing(vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			nvgText(vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(vg, textColor);
			if (module->notifyInfo[knobIndex] > 0l)
			{
				int srcParam = module->notifyingSource[knobIndex];
				if ( (srcParam >= ClockedLFO::SWING_PARAMS + 0) && (srcParam <= ClockedLFO::SWING_PARAMS + 3) ) {
					float swValue = module->params[ClockedLFO::SWING_PARAMS + knobIndex].value;
					int swInt = (int)round(swValue * 99.0f);
					snprintf(displayStr, 4, " %2u", (unsigned) abs(swInt));
					if (swInt < 0)
						displayStr[0] = '-';
					if (swInt >= 0)
						displayStr[0] = '+';
				}
				else if ( (srcParam >= ClockedLFO::DELAY_PARAMS + 1) && (srcParam <= ClockedLFO::DELAY_PARAMS + 3) ) {				
					int delayKnobIndex = (int)(module->params[ClockedLFO::DELAY_PARAMS + knobIndex].value + 0.5f);
					if (module->displayDelayNoteMode)
						snprintf(displayStr, 4, "%s", (delayLabelsNote[delayKnobIndex]).c_str());
					else
						snprintf(displayStr, 4, "%s", (delayLabelsClock[delayKnobIndex]).c_str());
				}					
				else if ( (srcParam >= ClockedLFO::PW_PARAMS + 0) && (srcParam <= ClockedLFO::PW_PARAMS + 3) ) {				
					float pwValue = module->params[ClockedLFO::PW_PARAMS + knobIndex].value;
					int pwInt = ((int)round(pwValue * 98.0f)) + 1;
					snprintf(displayStr, 4, "_%2u", (unsigned) abs(pwInt));
				}					
			}
			else {
				if (module->editingBpmMode != 0l) {
					snprintf(displayStr, 4, "P%2u", (unsigned) module->ppqn);
				}
				else
					snprintf(displayStr, 4, "%3u", (unsigned)((120.0f / module->masterLength) + 0.5f));
			}
			displayStr[3] = 0;// more safety
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	struct PanelThemeItem : MenuItem {
		ClockedLFO *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	struct ExpansionItem : MenuItem {
		ClockedLFO *module;
		void onAction(EventAction &e) override {
			module->expansion = module->expansion == 1 ? 0 : 1;
		}
	};
	struct EmitResetItem : MenuItem {
		ClockedLFO *module;
		void onAction(EventAction &e) override {
			module->emitResetOnStopRun = !module->emitResetOnStopRun;
		}
	};	
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		ClockedLFO *module = dynamic_cast<ClockedLFO*>(this->module);
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

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		EmitResetItem *erItem = MenuItem::create<EmitResetItem>("Emit Reset when Run is Turned Off", CHECKMARK(module->emitResetOnStopRun));
		erItem->module = module;
		menu->addChild(erItem);

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *expansionLabel = new MenuLabel();
		expansionLabel->text = "Expansion module";
		menu->addChild(expansionLabel);

		ExpansionItem *expItem = MenuItem::create<ExpansionItem>(expansionMenuLabel, CHECKMARK(module->expansion != 0));
		expItem->module = module;
		menu->addChild(expItem);

		return menu;
	}	
	
	void step() override {
		if(module->expansion != oldExpansion) {
			if (oldExpansion!= -1 && module->expansion == 0) {// if just removed expansion panel, disconnect wires to those jacks
				for (int i = 0; i < 6; i++)
					gRackWidget->wireContainer->removeAllWires(expPorts[i]);
			}
			oldExpansion = module->expansion;		
		}
		box.size.x = panel->box.size.x - (1 - module->expansion) * expWidth;
		Widget::step();
	}
	
	struct IMSmallKnobNotify : IMSmallKnob {
		IMSmallKnobNotify() {};
		void onDragMove(EventDragMove &e) override {
			int dispIndex = 0;
			if ( (paramId >= ClockedLFO::SWING_PARAMS + 0) && (paramId <= ClockedLFO::SWING_PARAMS + 3) )
				dispIndex = paramId - ClockedLFO::SWING_PARAMS;
			else if ( (paramId >= ClockedLFO::DELAY_PARAMS + 1) && (paramId <= ClockedLFO::DELAY_PARAMS + 3) )
				dispIndex = paramId - ClockedLFO::DELAY_PARAMS;
			else if ( (paramId >= ClockedLFO::PW_PARAMS + 0) && (paramId <= ClockedLFO::PW_PARAMS + 3) )
				dispIndex = paramId - ClockedLFO::PW_PARAMS;
			((ClockedLFO*)(module))->notifyingSource[dispIndex] = paramId;
			((ClockedLFO*)(module))->notifyInfo[dispIndex] = (long) (ClockedLFO::delayInfoTime * (float)engineGetSampleRate() / displayRefreshStepSkips);
			Knob::onDragMove(e);
		}
	};
	struct IMSmallSnapKnobNotify : IMSmallKnobNotify {
		IMSmallSnapKnobNotify() {
			snap = true;
			smooth = false;
		}
	};
	struct IMBigSnapKnobNotify : IMBigSnapKnob {
		IMBigSnapKnobNotify() {};
		void onChange(EventChange &e) override {
			int dispIndex = 0;
			if ( (paramId >= ClockedLFO::RATIO_PARAMS + 1) && (paramId <= ClockedLFO::RATIO_PARAMS + 3) )
				dispIndex = paramId - ClockedLFO::RATIO_PARAMS;
			((ClockedLFO*)(module))->syncRatios[dispIndex] = true;
			((ClockedLFO*)(module))->notifyInfo[dispIndex] = 0l;
			SVGKnob::onChange(e);		
		}
	};

	
	ClockedLFOWidget(ClockedLFO *module) : ModuleWidget(module) {
 		this->module = module;
		oldExpansion = -1;
		
		// Main panel from Inkscape
        panel = new DynamicSVGPanel();
        panel->mode = &module->panelTheme;
		panel->expWidth = &expWidth;
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/Clocked.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/dark/Clocked_dark.svg")));
        box.size = panel->box.size;
		box.size.x = box.size.x - (1 - module->expansion) * expWidth;
        addChild(panel);		
		
		// Screws
		addChild(createDynamicScrew<IMScrew>(Vec(15, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(15, 365), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 365), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30-expWidth, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30-expWidth, 365), &module->panelTheme));


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
		static const int colRulerT3 = colRulerT2 + colRulerSpacingT + 5;// swingMaster knob
		static const int colRulerT4 = colRulerT3 + colRulerSpacingT;// pwMaster knob
		static const int colRulerT5 = colRulerT4 + colRulerSpacingT;// clkMaster output
		// Three clock rows
		static const int colRulerM1 = colRulerL + 60;// ratio displays
		
		RatioDisplayWidget *displayRatios[4];
		
		// Row 0
		// Reset input
		addInput(createDynamicPort<IMPort>(Vec(colRulerL, rowRuler0), Port::INPUT, module, ClockedLFO::RESET_INPUT, &module->panelTheme));
		// Run input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT1, rowRuler0), Port::INPUT, module, ClockedLFO::RUN_INPUT, &module->panelTheme));
		// In input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT2, rowRuler0), Port::INPUT, module, ClockedLFO::BPM_INPUT, &module->panelTheme));
		// Master BPM knob
		addParam(createDynamicParam<IMBigSnapKnob>(Vec(colRulerT3 + 1 + offsetIMBigKnob, rowRuler0 + offsetIMBigKnob), module, ClockedLFO::RATIO_PARAMS + 0, (float)(module->bpmMin), (float)(module->bpmMax), 120.0f, &module->panelTheme));// must be a snap knob, code in step() assumes that a rounded value is read from the knob	(chaining considerations vs BPM detect)
		// BPM display
		displayRatios[0] = new RatioDisplayWidget();
		displayRatios[0]->box.pos = Vec(colRulerT4 + 11, rowRuler0 + vOffsetDisplay);
		displayRatios[0]->box.size = Vec(55, 30);// 3 characters
		displayRatios[0]->module = module;
		displayRatios[0]->knobIndex = 0;
		addChild(displayRatios[0]);
		
		// Row 1
		// Reset LED bezel and light
		addParam(createParam<LEDBezel>(Vec(colRulerL + offsetLEDbezel, rowRuler1 + offsetLEDbezel), module, ClockedLFO::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MuteLight<GreenLight>>(Vec(colRulerL + offsetLEDbezel + offsetLEDbezelLight, rowRuler1 + offsetLEDbezel + offsetLEDbezelLight), module, ClockedLFO::RESET_LIGHT));
		// Run LED bezel and light
		addParam(createParam<LEDBezel>(Vec(colRulerT1 + offsetLEDbezel, rowRuler1 + offsetLEDbezel), module, ClockedLFO::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MuteLight<GreenLight>>(Vec(colRulerT1 + offsetLEDbezel + offsetLEDbezelLight, rowRuler1 + offsetLEDbezel + offsetLEDbezelLight), module, ClockedLFO::RUN_LIGHT));
		// BPM mode and light
		addParam(createDynamicParam<IMPushButton>(Vec(colRulerT2 + offsetTL1105, rowRuler1 + offsetTL1105), module, ClockedLFO::BPMMODE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(colRulerM1 + 62, rowRuler1 + offsetMediumLight), module, ClockedLFO::BPMSYNC_LIGHT));		
		// Swing master knob
		addParam(createDynamicParam<IMSmallKnobNotify>(Vec(colRulerT3 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, ClockedLFO::SWING_PARAMS + 0, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		// PW master knob
		addParam(createDynamicParam<IMSmallKnobNotify>(Vec(colRulerT4 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, ClockedLFO::PW_PARAMS + 0, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		// Clock master out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler1), Port::OUTPUT, module, ClockedLFO::CLK_OUTPUTS + 0, &module->panelTheme));
		
		// Out out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT2, rowRuler5), Port::OUTPUT, module, ClockedLFO::BPM_OUTPUT, &module->panelTheme));
	}
};

Model *modelClockedLFO = Model::create<ClockedLFO, ClockedLFOWidget>("Impromptu Modular", "ClockedLFO", "LFO - ClockedLFO", LFO_TAG);

/*CHANGE LOG

0.6.12:
created

*/
