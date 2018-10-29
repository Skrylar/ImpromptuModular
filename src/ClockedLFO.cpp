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
#include "FundamentalUtil.hpp"


class Clock {
	// The -1.0 step is used as a reset state every period so that 
	//   lengths can be re-computed; it will stay at -1.0 when a clock is inactive.

	
	double step;// -1.0 when stopped, [0 to period[ for clock steps
	double length;// period
	double sampleTime;
	
	public:
	
	Clock() {
		reset();
	}
	
	inline void reset() {
		step = -1.0;
	}
	inline bool isReset() {
		return step == -1.0;
	}
	inline double getStep() {
		return step;
	}
	inline void start() {
		step = 0.0;
	}
	
	inline void setup(double lengthGiven, double sampleTimeGiven) {
		length = lengthGiven;
		sampleTime = sampleTimeGiven;
	}

	void stepClock() {// here the clock was output on step "step", this function is called at end of module::step()
		if (step >= 0.0) {// if active clock
			step += sampleTime;
			if (step >= length) {// reached end iteration
				step -= length;
				reset();// frame done
			}
		}
	}
	
	void applyNewLength(double lengthStretchFactor) {
		if (step != -1.0)
			step *= lengthStretchFactor;
		length *= lengthStretchFactor;
	}
};


//*****************************************************************************


struct ClockedLFO : Module {
	enum ParamIds {
		ENUMS(SWING_PARAMS, 4),// master is index 0
		ENUMS(PW_PARAMS, 4),// master is index 0
		RESET_PARAM,
		BPMMODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(PW_INPUTS, 4),// master is index 0
		RESET_INPUT,
		CLK_INPUT,
		ENUMS(SWING_INPUTS, 4),// master is index 0
		NUM_INPUTS
	};
	enum OutputIds {
		LFO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RESET_LIGHT,
		ENUMS(BPMSYNC_LIGHT, 2),// room for GreenRed
		NUM_LIGHTS
	};
	
	
	// Constants
	static const int bpmMax = 300;
	static const int bpmMin = 30;
	static constexpr float masterLengthMax = 60.0f / bpmMin;// a length is a double period
	static constexpr float masterLengthMin = 60.0f / bpmMax;// a length is a double period
	static constexpr float delayInfoTime = 3.0f;// seconds
	static constexpr float swingInfoTime = 2.0f;// seconds
	
	// Need to save
	int panelTheme = 0;
	int ppqn = 4;
	
	// No need to save
	bool running;
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
	Clock clk;
	LowFrequencyOscillator lfo;
	

	// called from the main thread (step() can not be called until all modules created)
	ClockedLFO() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}
	

	void onReset() override {
		running = false;
		editingBpmMode = 0l;
		resetClockedLFO(true);		
		lfo.setPulseWidth(0.5f);//params[PW_PARAM].value + params[PWM_PARAM].value * inputs[PW_INPUT].value / 10.0f);
		lfo.offset = false;//(params[OFFSET_PARAM].value > 0.0f);
		lfo.invert = false;//(params[INVERT_PARAM].value <= 0.0f);
		lfo.setReset(inputs[RESET_INPUT].value + params[RESET_PARAM].value);
	}
	
	
	void onRandomize() override {
		resetClockedLFO(false);
	}

	
	void resetClockedLFO(bool hardReset) {// set hardReset to true to revert learned BPM to 120 in sync mode, or else when false, learned bmp will stay persistent
		clk.reset();
		extPulseNumber = -1;
		extIntervalTime = 0.0;
		timeoutTime = 2.0 / ppqn + 0.1;// worst case. This is a double period at 30 BPM (2s), divided by the expected number of edges in the double period 
									   //   which is ppqn, plus epsilon. This timeoutTime is only used for timingout the 2nd clock edge
		if (hardReset)
			newMasterLength = 0.5f;// 120 BPM
		newMasterLength = clamp(newMasterLength, masterLengthMin, masterLengthMax);
		masterLength = newMasterLength;
	}	
	
	
	json_t *toJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// ppqn
		json_object_set_new(rootJ, "ppqn", json_integer(ppqn));
		
		return rootJ;
	}


	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// ppqn
		json_t *ppqnJ = json_object_get(rootJ, "ppqn");
		if (ppqnJ)
			ppqn = clamp(json_integer_value(ppqnJ), 4, 24);

		scheduledReset = true;
	}

	
	void onSampleRateChange() override {
		resetClockedLFO(false);
	}		
	

	void step() override {		
		double sampleRate = (double)engineGetSampleRate();
		double sampleTime = 1.0 / sampleRate;// do this here since engineGetSampleRate() returns float

		// Scheduled reset
		if (scheduledReset) {
			resetClockedLFO(false);		
			scheduledReset = false;
		}
		
		// Reset (has to be near top because it sets steps to 0, and 0 not a real step (clock section will move to 1 before reaching outputs)
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			resetLight = 1.0f;
			resetPulse.trigger(0.001f);
			resetClockedLFO(false);	
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
		
		newMasterLength = masterLength;
		// rising edge detect
		if (bpmDetectTrigger.process(inputs[CLK_INPUT].value)) {
			if (!running) {
				// this must be the only way to start runnning when in bpmDetectionMode or else
				//   when manually starting, the clock will not know which pulse is the 1st of a ppqn set
				running = true;
				runPulse.trigger(0.001f);
				resetClockedLFO(false);
			}
			if (running) {
				extPulseNumber++;
				if (extPulseNumber >= ppqn)
					extPulseNumber = 0;
				if (extPulseNumber == 0)// if first pulse, start interval timer
					extIntervalTime = 0.0;
				else {
					// all other ppqn pulses except the first one. now we have an interval upon which to plan a strecth 
					double timeLeft = extIntervalTime * (double)(ppqn - extPulseNumber) / ((double)extPulseNumber);
					newMasterLength = clamp(clk.getStep() + timeLeft, masterLengthMin / 1.5f, masterLengthMax * 1.5f);// extended range for better sync ability (20-450 BPM)
					timeoutTime = extIntervalTime * ((double)(1 + extPulseNumber) / ((double)extPulseNumber)) + 0.1; // when a second or higher clock edge is received, 
					//  the timeout is the predicted next edge (whici is extIntervalTime + extIntervalTime / extPulseNumber) plus epsilon
				}
			}
		}
		if (running) {
			extIntervalTime += sampleTime;
			if (extIntervalTime > timeoutTime) {
				running = false;
				runPulse.trigger(0.001f);
				resetClockedLFO(false);
			}
		}
		if (newMasterLength != masterLength) {
			double lengthStretchFactor = ((double)newMasterLength) / ((double)masterLength);
			clk.applyNewLength(lengthStretchFactor);
			masterLength = newMasterLength;
		}
		
		
		// main clock engine
		if (running) {
			// See if clocks finished their prescribed number of iteratios of double periods (and syncWait for sub) or 
			//    if they were forced reset and if so, recalc and restart them
			
			// Master clock
			if (clk.isReset()) {
				clk.setup(masterLength, sampleTime);// must call setup before start. length = double_period
				clk.start();
			}
		}
		clk.stepClock();

		

		lfo.setPitch(log2f(1.0f / masterLength));
		lfo.step(sampleTime);
		outputs[LFO_OUTPUT].value = 5.0f * lfo.tri();
		
		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			// Reset light
			lights[RESET_LIGHT].value =	resetLight;	
			resetLight -= (resetLight / lightLambda) * (float)sampleTime * displayRefreshStepSkips;
			
			// BPM light
			bool warningFlashState = true;
			if (cantRunWarning > 0l) 
				warningFlashState = calcWarningFlash(cantRunWarning, (long) (0.7 * sampleRate / displayRefreshStepSkips));
			lights[BPMSYNC_LIGHT + 0].value = (warningFlashState) ? 1.0f : 0.0f;
			lights[BPMSYNC_LIGHT + 1].value = (warningFlashState) ? (float)((ppqn - 4)*(ppqn - 4))/400.0f : 0.0f;			
			
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

	struct RatioDisplayWidget : TransparentWidget {
		ClockedLFO *module;
		int knobIndex;
		std::shared_ptr<Font> font;
		char displayStr[4];
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
					snprintf(displayStr, 4, "%3u", (unsigned)((60.0f / module->masterLength) + 0.5f));
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

		//menu->addChild(new MenuLabel());// empty line
		
		//MenuLabel *settingsLabel = new MenuLabel();
		//settingsLabel->text = "Settings";
		//menu->addChild(settingsLabel);
		
		return menu;
	}		
	struct IMSmallKnobNotify : IMSmallKnob {
		IMSmallKnobNotify() {};
		void onDragMove(EventDragMove &e) override {
			int dispIndex = 0;
			if ( (paramId >= ClockedLFO::SWING_PARAMS + 0) && (paramId <= ClockedLFO::SWING_PARAMS + 3) )
				dispIndex = paramId - ClockedLFO::SWING_PARAMS;
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

	
	ClockedLFOWidget(ClockedLFO *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/ClockedLFO.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/dark/ClockedLFO.svg")));
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);		
		
		// Screws
		addChild(createDynamicScrew<IMScrew>(Vec(15, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(15, 365), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 365), &module->panelTheme));


		static const int rowRuler0 = 50;//reset,run inputs, master knob and bpm display
		static const int rowRuler1 = rowRuler0 + 55;// reset,run switches
		//		
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
		// Clk input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT2, rowRuler0), Port::INPUT, module, ClockedLFO::CLK_INPUT, &module->panelTheme));
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
		// BPM mode and light
		addParam(createDynamicParam<IMPushButton>(Vec(colRulerT2 + offsetTL1105, rowRuler1 + offsetTL1105), module, ClockedLFO::BPMMODE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(colRulerM1 + 62, rowRuler1 + offsetMediumLight), module, ClockedLFO::BPMSYNC_LIGHT));		
		// Swing master knob
		addParam(createDynamicParam<IMSmallKnobNotify>(Vec(colRulerT3 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, ClockedLFO::SWING_PARAMS + 0, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		// PW master knob
		addParam(createDynamicParam<IMSmallKnobNotify>(Vec(colRulerT4 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, ClockedLFO::PW_PARAMS + 0, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		
		// Out out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler1), Port::OUTPUT, module, ClockedLFO::LFO_OUTPUT, &module->panelTheme));
	}
};

Model *modelClockedLFO = Model::create<ClockedLFO, ClockedLFOWidget>("Impromptu Modular", "ClockedLFO", "LFO - ClockedLFO", LFO_TAG);

/*CHANGE LOG

0.6.12:
created

*/
