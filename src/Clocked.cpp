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


struct Clock {
	// the 0.0d step does not actually to consume a sample step, it is used to reset every double-period so that lengths can be re-computed
	// a clock frame is defined as "length * iterations + syncWait";
	// for master, syncWait does not apply and iterations = 1
	
	double step;// 0.0d when stopped, [sampleTime to 2*period] for clock steps (*2 is because of swing, so we do groups of 2 periods)
	double length;// double period. Dependant of step, irrelevant values when a step is 0
	int iterations;// run this many double periods before going into sync 
	
	/*float calcClock(Clock* clk, int clkIndex, float sampleRate) {
		// uses steps[], lengths[], swingVal[], params[PW_PARAMS + i].value
		float ret = 0.0f;
		
		if (clk[clkIndex].steps > 0) {
			float swParam = swingVal[clkIndex];
			swParam *= (swParam * (swParam > 0.0f ? 1.0f : -1.0f));// non-linear behavior for more sensitivity at center: f(x) = x^2 * sign(x)
			
			// all following values are in samples numbers, whether long or float
			float onems = sampleRate / 1000.0f;
			float period = ((float)clk[clkIndex].lengths) / 2.0f;
			float swing = (period - 2.0f * onems) * swParam;
			float p2min = onems;
			float p2max = period - onems - fabs(swing);
			if (p2max < p2min) {
				p2max = p2min;
			}
			
			//long p1 = 1;// implicit, no need 
			long p2 = (long)((p2max - p2min) * params[PW_PARAMS + clkIndex].value + p2min);
			long p3 = (long)(period + swing);
			long p4 = ((long)(period + swing)) + p2;
			
			ret = ( ((clk[clkIndex].steps < p2)) || ((clk[clkIndex].steps < p4) && (clk[clkIndex].steps > p3)) ) ? 10.0f : 0.0f;
		}
		return ret;
	}*/
	bool isClockHigh(float swing, float pulseWidth) {
		bool high = false;
		if ( (step > 0.0d && step <= (length * 0.25d)) || (step > (length * 0.5d) && step <= (length * 0.75d)) )
			high = true;
		return high;
	}
	
	inline void resetClock() {
		step = 0.0d;
	}
	inline bool isClockReset() {
		return step == 0.0d;
	}
	
	inline void setup(double lengthGiven, int iterationsGiven) {
		length = lengthGiven;
		iterations = iterationsGiven;
	}
	
	inline void startClock(double sampleTime) {
		step = sampleTime;
	}
	
	void stepClock(double sampleTime) {// here the clock was output on step "step", this function is called at end of module::step()
		if (step > 0.0d) {// if active clock
			if (iterations > 0) {
				if (step >= length) {// reached end iteration
					iterations--;
					if (iterations > 0) {
						startClock(sampleTime);
					}
					else {
						/*if (remainder > 0l) {
							remainder--;
							step++;
						}
						else*/ 
							resetClock();// frame done
					}
				}
				else
					step += sampleTime;
			}
			else {// iterations == 0
				/*if (remainder > 0l) {
					remainder--;
					step++;
				}
				else */
					resetClock();// frame done
			}
		}
	}
	
	void applyNewLength(double lengthStretchFactor) {// delta length as calculated for the main clock (in step steps)
		// scale all, naive version for now
		step *= lengthStretchFactor;
		length *= lengthStretchFactor;
	}
};


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
	int ratioValuesDoubled[numRatios];// calculated only once in constructor

	// BPM info
	static const int bpmMax = 350;
	static const int bpmMin = 10;
	static const int bpmRange = bpmMax - bpmMin;
		
	// Knob utilities
	int getBeatsPerMinute(void) {
		int bpm = 0;
		if (inputs[BPM_INPUT].active) {
			bpm = (int) round( (inputs[BPM_INPUT].value / 10.0f) * (float)bpmRange + (float)bpmMin );// use round() in case negative voltage input
			if (bpm < bpmMin)
				bpm = bpmMin;
		}
		else {
			bpm = (int)(params[RATIO_PARAMS + 0].value + 0.5f);
		}
		return bpm;
	}
	long getRatioDoubled(int ratioKnobIndex) {
		// ratioKnobIndex is 0 for master BPM's ratio (mplicitly 1.0f), and 1 to 3 for other ratio knobs
		// returns a positive ratio for mult, negative ratio for div (0 never returned)
		int ret = 1;
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
			ret = ratioValuesDoubled[i];
			if (isDivision) 
				ret = -1l * ret;
		}
		return ret;
	}
	
	// Need to save
	int panelTheme = 0;
	bool running;
	
	// No need to save
	float resetLight = 0.0f;
	static constexpr float BPM_PARAM_INIT_VALUE = 120.0f;// so that module constructor is coherent with widget initialization, since module created before widget
	int bpm;
	Clock clk[4];
	int ratiosDoubled[4];
	int newRatiosDoubled[4];
	
	static constexpr float SWING_PARAM_INIT_VALUE = 0.0f;// so that module constructor is coherent with widget initialization, since module created before widget
	float swingVal[4];
	float swingLast[4];
	long swingInfo[4];// downward step counter when swing to be displayed, 0 when normal display

	
	SchmittTrigger resetTrigger;
	SchmittTrigger runTrigger;
	PulseGenerator resetPulse;
	PulseGenerator runPulse;
	

	Clocked() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		for (int i = 0; i < numRatios; i++)
			ratioValuesDoubled[i] = (int) (ratioValues[i] * 2.0f + 0.5f);
		onReset();
	}
	

	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		running = false;
		bpm = (int)(BPM_PARAM_INIT_VALUE + 0.5f);
		for (int i = 0; i < 4; i++) {
			clk[i].resetClock();
			ratiosDoubled[i] = 0;
			newRatiosDoubled[i] = 0;
			swingVal[i] = SWING_PARAM_INIT_VALUE;
			swingLast[i] = swingVal[i];
			swingInfo[i] = 0l;
		}
	}
	
	
	// widgets randomized before onRandomize() is called
	void onRandomize() override {
		onReset();
		for (int i = 0; i < 4; i++) {
			swingVal[i] = params[SWING_PARAMS + i].value;// redo this since SWING_PARAM_INIT_VALUE of onReset() not valid
			swingLast[i] = swingVal[i];
		}
		bpm = getBeatsPerMinute();// redo this since BPM_PARAM_INIT_VALUE of onReset() not valid
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

		for (int i = 0; i < 4; i++) {
			swingVal[i] = params[SWING_PARAMS + i].value;
			swingLast[i] = swingVal[i];
		}
		
		bpm = getBeatsPerMinute();
	}

	inline double calcMasterLength(int givenBpm) {// length in seconds
		return 120.0d / (double)givenBpm;// same as: (2 * 1 / bps)   =   (2 * 1 / ( bpm / 60 ))
	}
	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		double sampleRate = (double)engineGetSampleRate();
		double sampleTime = 1.0d / sampleRate;// do this here since engineGetSampleRate() returns float
		
		static const float swingInfoTime = 1.7f;// seconds
		long swingInfoInit = (long) (swingInfoTime * (float)sampleRate);
		
		// Run button
		if (runTrigger.process(params[RUN_PARAM].value + inputs[RUN_INPUT].value)) {
			running = !running;
			// reset on any change of run state (will not relaunch if not running, thus clock railed low)
			for (int i = 0; i < 4; i++) {
				clk[i].resetClock();
			}
			runPulse.trigger(0.001f);
		}

		// Reset (has to be near to because sets steps to 0, and 0 not a real step (clock section will move to 1 before reaching outputs)
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {// TODO add sampleRate change as condition for reset
			resetLight = 1.0f;
			resetPulse.trigger(0.001f);
			for (int i = 0; i < 4; i++) {
				clk[i].resetClock();
			}
		}
		else
			resetLight -= (resetLight / lightLambda) * (float)sampleTime;	

		// BPM input and knob
		long newBpm = getBeatsPerMinute();
		if (newBpm != bpm) {
			double lengthStretchFactor = ((double)bpm) / ((double)newBpm);
			for (int i = 0; i < 4; i++) {
				clk[i].applyNewLength(lengthStretchFactor);
			}
			bpm = newBpm;// bpm was changed
		}

		// Ratio knobs
		bool syncRatios[4] = {false, false, false, false};// 0 index unused
		for (int i = 1; i < 4; i++) {
			newRatiosDoubled[i] = getRatioDoubled(i);
			if (newRatiosDoubled[i] != ratiosDoubled[i]) {
				syncRatios[i] = true;// 0 index not used, but loop must start at i = 0
			}
		}
		
		// Swing changed (for swing info)
		for (int i = 0; i < 4; i++) {
			swingVal[i] = params[SWING_PARAMS + i].value;
			if (swingLast[i] != swingVal[i]) {
				swingInfo[i] = swingInfoInit;// trigger swing info on channel i
				swingLast[i] = swingVal[i];
			}
		}			

		
		
		//********** Clock **********
		
		// Clock
		if (running) {
			
			//if (clk[0].isClockReset() && clk[1].isClockReset()) info("**** SYNC 0 vs 1 ****");// TODO remove this line
			
			// Note: the 0.0d step does not consume a sample step when runnnig, it's used to force length (re)computation
			//       and will stay at 0.0d when a clock is inactive.
			
			// See if ratio knobs changed (or unitinialized)
			if (clk[0].isClockReset()) {
				for (int i = 1; i < 4; i++) {
					if (syncRatios[i]) {// always false for master
						clk[i].resetClock();// force reset (thus refresh) of that sub-clock
						ratiosDoubled[i] = newRatiosDoubled[i];
						syncRatios[i] = false;
					}
				}
			}
			
			// See if clocks finished their prescribed number of iteratios of double periods (and syncWait for sub) or were forced reset
			//    and if so, recalc and restart them
			//Master clock
			if (clk[0].isClockReset()) {
				clk[0].setup(calcMasterLength(bpm), 1);
				clk[0].startClock(sampleTime);
			}
			// Sub clocks
			for (int i = 1; i < 4; i++) {
				if (clk[i].isClockReset()) {
					double length;
					int iterations;
					int ratioDoubled = ratiosDoubled[i];
					if (ratioDoubled < 0) { // if div 
						ratioDoubled *= -1;
						length = (clk[0].length * ((double)ratioDoubled)) / 2.0d;
						iterations = 1l + (ratioDoubled % 2);		
						clk[i].setup(length, iterations);
					}
					else {// mult 
						length = (clk[0].length * 2.0d) / ((double)ratioDoubled);
						iterations = ratioDoubled / (2l - (ratioDoubled % 2l));							
						clk[i].setup(length, iterations);
					}
					clk[i].startClock(sampleTime);
				}
			}
		}
			
		
		
		//********** Outputs and lights **********
			
		// Clock outputs
		for (int i = 0; i < 4; i++) {
			outputs[CLK_OUTPUTS + i].value = clk[i].isClockHigh(swingVal[i], params[PW_PARAMS + i].value) ? 10.0f : 0.0f;
		}
		
		outputs[RESET_OUTPUT].value = (resetPulse.process((float)sampleTime) ? 10.0f : 0.0f);
		outputs[RUN_OUTPUT].value = (runPulse.process((float)sampleTime) ? 10.0f : 0.0f);
		outputs[BPM_OUTPUT].value =  (inputs[BPM_INPUT].active ? inputs[BPM_INPUT].value : ( (float)((bpm - bpmMin) * 10l) / ((float)bpmRange) ) );
			
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	
		
		// Run light
		lights[RUN_LIGHT].value = running;
		
		// Sync lights
		for (int i = 1; i < 4; i++) {
			lights[CLK_LIGHTS + i].value = (syncRatios[i] && running) ? 1.0f: 0.0f;
		}

		// incr/decr all counters related to step()
		for (int i = 0; i < 4; i++) {
			// step clocks
			clk[i].stepClock(sampleTime);
			// swing info
			if (swingInfo[i] > 0)
				swingInfo[i]--;
		}
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
			if (module->swingInfo[knobIndex] > 0)
			{
				float swValue = module->swingVal[knobIndex];
				int swInt = (int)(swValue * 99.0f + 0.5f);
				snprintf(displayStr, 4, " %2u", (unsigned) abs(swInt));
				if (swInt < 0)
					displayStr[0] = '-';
				if (swInt > 0)
					displayStr[0] = '+';
			}
			else {
				if (knobIndex > 0) {// ratio to display
					bool isDivision = false;
					long ratioDoubled = module->getRatioDoubled(knobIndex);
					if (ratioDoubled < 0l) {
						ratioDoubled = -1l * ratioDoubled;
						isDivision = true;
					}
					if ( (ratioDoubled % 2) == 1 )
						snprintf(displayStr, 4, "%c,5", 0x30 + (char)(ratioDoubled / 2l));
					else {
						snprintf(displayStr, 4, "X%2u", (unsigned)(ratioDoubled / 2l));
						if (isDivision)
							displayStr[0] = '/';
					}
				}
				else {// BPM to display
					snprintf(displayStr, 4, "%3u", (unsigned) (fabs(module->getBeatsPerMinute()) + 0.5f));
				}
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
		// Master BPM knob
		addParam(createDynamicParam<IMBigSnapKnob>(Vec(colRulerT3 + 6 + offsetIMBigKnob, rowRuler0 + offsetIMBigKnob), module, Clocked::RATIO_PARAMS + 0, (float)(module->bpmMin), (float)(module->bpmMax), Clocked::BPM_PARAM_INIT_VALUE, &module->panelTheme));		
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
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT3 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Clocked::SWING_PARAMS + 0, -1.0f, 1.0f, Clocked::SWING_PARAM_INIT_VALUE, &module->panelTheme));
		// PW master knob
		addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerT4 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Clocked::PW_PARAMS + 0, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		// Clock master out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler1), Port::OUTPUT, module, Clocked::CLK_OUTPUTS + 0, &module->panelTheme));
		
		
		// Row 2-4 (sub clocks)		
		for (int i = 0; i < 3; i++) {
			// Ratio1 knob
			addParam(createDynamicParam<IMBigSnapKnob>(Vec(colRulerM0 + offsetIMBigKnob, rowRuler2 + i * rowSpacingClks + offsetIMBigKnob), module, Clocked::RATIO_PARAMS + 1 + i, ((float)(module->numRatios - 1))*-1.0f, ((float)(module->numRatios - 1)), 0.0f, &module->panelTheme));		
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
			addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerM2 + offsetIMSmallKnob, rowRuler2 + i * rowSpacingClks + offsetIMSmallKnob), module, Clocked::SWING_PARAMS + 1 + i, -1.0f, 1.0f, Clocked::SWING_PARAM_INIT_VALUE, &module->panelTheme));
			// PW knobs
			addParam(createDynamicParam<IMSmallKnob>(Vec(colRulerM3 + offsetIMSmallKnob, rowRuler2 + i * rowSpacingClks + offsetIMSmallKnob), module, Clocked::PW_PARAMS + 1 + i, 0.0f, 1.0f, 0.5f, &module->panelTheme));
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
