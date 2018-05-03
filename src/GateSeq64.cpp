//***********************************************************************************************
//Gate sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept by Nigel Sixsmith and Marc Boulé
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct GateSeq64 : Module {
	enum ParamIds {
		ENUMS(STEP_PARAMS, 64),
		GATE_PARAM,
		LENGTH_PARAM,
		GATEP_PARAM,
		MODE_PARAM,
		GATETRIG_PARAM,
		ENUMS(RUN_PARAMS, 4),
		LINKED_PARAM,
		CONFIG_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		CLEAR_PARAM,
		FILL_PARAM,
		RESET_PARAM,
		PROB_KNOB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(CLOCK_INPUTS, 4),
		RESET_INPUT,
		ENUMS(RUNCV_INPUTS, 4),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_LIGHTS, 64 * 2),// room for GreenRed
		ENUMS(GATE_LIGHT, 1),
		ENUMS(LENGTH_LIGHT, 1),
		ENUMS(GATEP_LIGHT, 1),
		ENUMS(MODE_LIGHT, 1),
		ENUMS(GATETRIG_LIGHT, 1),
		ENUMS(RUN_LIGHTS, 4),
		ENUMS(CLOCKIN_LIGHTS, 4),
		ENUMS(GATEOUT_LIGHTS, 4),
		RESET_LIGHT,
		NUM_LIGHTS
	};
	
	enum DisplayStateIds {DISP_GATE, DISP_LENGTH, DISP_GATEP, DISP_MODE, DISP_GATETRIG, DISP_ROW_SEL};
	
	// Need to save
	bool running[4] = {};
	bool gate[64] = {};
	int length[4] = {};// values are 1 to 16
	bool gatep[64] = {};
	int gatepval[64] = {};// values are 0 to 100
	int mode[4] = {};
	bool trig[4] = {};

	// No need to save
	int displayState;
	int indexStep[4] = {};
	int stepIndexRunHistory[4] = {};// no need to initialize
	bool copyPasteBuf[16] = {};// copy-paste only one row
	int copyPasteBufProb[16] = {};// copy-paste only one row
	long feedbackCP;// downward step counter for CP feedback
	bool gateRandomEnable[4];// no need to initialize
	float resetLight = 0.0f;
	long feedbackCPinit;// no need to initialize
	bool gatePulsesValue[4] = {};
	int cpcfInfo;// copy = 1, paste = 2, clear = 3, fill = 4; on gate (positive) or gatep (negative)
	long clockIgnoreOnReset;
	const float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
	int displayProb;

	
	SchmittTrigger gateTrigger;
	SchmittTrigger lengthTrigger;
	SchmittTrigger gatePTrigger;
	SchmittTrigger modeTrigger;
	SchmittTrigger gateTrigTrigger;
	SchmittTrigger stepTriggers[64];
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger runningTriggers[4];
	SchmittTrigger clockTriggers[4];
	SchmittTrigger resetTrigger;
	SchmittTrigger linkedTrigger;
	SchmittTrigger clearTrigger;
	SchmittTrigger fillTrigger;
	SchmittTrigger configTrigger;
	SchmittTrigger configTrigger2;
	SchmittTrigger configTrigger3;

	PulseGenerator gatePulses[4];

		
	GateSeq64() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		displayState = DISP_GATE;
		for (int i = 0; i < 64; i++) {
			gate[i] = false;
			gatep[i] = false;
			gatepval[i] = 100;
		}
		for (int i = 0; i < 4; i++) {
			running[i] = false;
			indexStep[i] = 0;
			length[i] = 16;
			mode[i] = MODE_FWD;
			trig[i] = false;
			gateRandomEnable[i] = true;
		}
		for (int i = 0; i < 16; i++) {
			copyPasteBuf[i] = 0;
			copyPasteBufProb[i] = 100;			
		}
		feedbackCP = 0l;
		displayProb = 100;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}

	void onRandomize() override {
		displayState = DISP_GATE;
		configTrigger.reset();
		configTrigger2.reset();
		configTrigger3.reset();
		linkedTrigger.reset();
		int stepConfig = 1;// 4x16
		if (params[CONFIG_PARAM].value > 1.5f)// 1x64
			stepConfig = 4;
		else if (params[CONFIG_PARAM].value > 0.5f)// 2x32
			stepConfig = 2;
		for (int i = 0; i < 64; i++) {
			gate[i] = (randomUniform() > 0.5f);
			gatep[i] = (randomUniform() > 0.5f);
			gatepval[i] = (randomu32() % 101);
		}
		for (int i = 0; i < 4; i += stepConfig) {
			running[i] = (randomUniform() > 0.5f);
			indexStep[i] = 0;
			length[i] = 1 + (randomu32() % (16 * stepConfig));
			mode[i] = randomu32() % 5;
			trig[i] = (randomUniform() > 0.5f);
		}
		if (params[LINKED_PARAM].value > 0.5f) {
			running[1] = running[0];
			running[2] = running[0];
			running[3] = running[0];
		}	
		for (int i = 0; i < 16; i++) {
			copyPasteBuf[i] = 0;
			copyPasteBufProb[i] = 100;			
		}
		feedbackCP = 0l;
		displayProb = (randomu32() % 101);
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// running
		json_t *runningJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(runningJ, i, json_integer((int) running[i]));
		json_object_set_new(rootJ, "running", runningJ);
		
		// gate
		json_t *gateJ = json_array();
		for (int i = 0; i < 64; i++)
			json_array_insert_new(gateJ, i, json_integer((int) gate[i]));
		json_object_set_new(rootJ, "gate", gateJ);
		
		// length
		json_t *lengthJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(lengthJ, i, json_integer(length[i]));
		json_object_set_new(rootJ, "length", lengthJ);
		
		// gatep
		json_t *gatepJ = json_array();
		for (int i = 0; i < 64; i++)
			json_array_insert_new(gatepJ, i, json_integer((int) gatep[i]));
		json_object_set_new(rootJ, "gatep", gatepJ);
		
		// gatepval
		json_t *gatepvalJ = json_array();
		for (int i = 0; i < 64; i++)
			json_array_insert_new(gatepvalJ, i, json_integer(gatepval[i]));
		json_object_set_new(rootJ, "gatepval", gatepvalJ);
		
		// mode
		json_t *modeJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(modeJ, i, json_integer(mode[i]));
		json_object_set_new(rootJ, "mode", modeJ);
		
		// trig
		json_t *trigJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(trigJ, i, json_integer((int) trig[i]));
		json_object_set_new(rootJ, "trig", trigJ);
		
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ) {
			for (int i = 0; i < 4; i++) {
				json_t *runningArrayJ = json_array_get(runningJ, i);
				if (runningArrayJ)
					running[i] = !!json_integer_value(runningArrayJ);
			}
		}
		
		// gate
		json_t *gateJ = json_object_get(rootJ, "gate");
		if (gateJ) {
			for (int i = 0; i < 64; i++) {
				json_t *gateArrayJ = json_array_get(gateJ, i);
				if (gateArrayJ)
					gate[i] = !!json_integer_value(gateArrayJ);
			}
		}
		
		// length
		json_t *lengthJ = json_object_get(rootJ, "length");
		if (lengthJ)
			for (int i = 0; i < 4; i++)
			{
				json_t *lengthArrayJ = json_array_get(lengthJ, i);
				if (lengthArrayJ)
					length[i] = json_integer_value(lengthArrayJ);
			}
		
		// gatep
		json_t *gatepJ = json_object_get(rootJ, "gatep");
		if (gatepJ) {
			for (int i = 0; i < 64; i++) {
				json_t *gatepArrayJ = json_array_get(gatepJ, i);
				if (gatepArrayJ)
					gatep[i] = !!json_integer_value(gatepArrayJ);
			}
		}
		
		// gatepval
		json_t *gatepvalJ = json_object_get(rootJ, "gatepval");
		if (gatepvalJ) {
			for (int i = 0; i < 64; i++) {
				json_t *gatepvalArrayJ = json_array_get(gatepvalJ, i);
				if (gatepvalArrayJ)
					gatepval[i] = json_integer_value(gatepvalArrayJ);
			}
		}
		
		// mode
		json_t *modeJ = json_object_get(rootJ, "mode");
		if (modeJ)
			for (int i = 0; i < 4; i++)
			{
				json_t *modeArrayJ = json_array_get(modeJ, i);
				if (modeArrayJ)
					mode[i] = json_integer_value(modeArrayJ);
			}
		
		// trig
		json_t *trigJ = json_object_get(rootJ, "trig");
		if (trigJ) {
			for (int i = 0; i < 4; i++) {
				json_t *trigArrayJ = json_array_get(trigJ, i);
				if (trigArrayJ)
					trig[i] = !!json_integer_value(trigArrayJ);
			}
		}
	}

	
	// Find lowest active clock (takes given index to start from, retruns the index found)
	// Returns 0 if not found (use clock 0 by default, do not return an error code)
	int findClock(int clkIndex, bool* clkActive) {
		for (int i = clkIndex; i > 0; i--)
			if (clkActive[i])
				return i;
		return 0;
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		static const float copyPasteInfoTime = 3.0f;// seconds
		float engineSampleRate = engineGetSampleRate();
		feedbackCPinit = (long) (copyPasteInfoTime * engineSampleRate);
		
		// Config
		int stepConfig = 1;// 4x16
		if (params[CONFIG_PARAM].value > 1.5f)// 1x64
			stepConfig = 4;
		else if (params[CONFIG_PARAM].value > 0.5f)// 2x32
			stepConfig = 2;
		// Config: set lengths to their max when move switch
		if (configTrigger.process(params[CONFIG_PARAM].value*10.0f - 10.0f) || 
			configTrigger2.process(params[CONFIG_PARAM].value*10.0f) ||
			configTrigger3.process(params[CONFIG_PARAM].value*-5.0f + 10.0f)) {
			for (int i = 0; i < 4; i += stepConfig)
				length[i] = 16 * stepConfig;
		}
		
		// Run state
		bool runTrig[5] = {};
		for (int i = 0; i < 4; i++) {
			runTrig[i] = runningTriggers[i].process(params[RUN_PARAMS + i].value + inputs[RUNCV_INPUTS + i].value);
			if (runTrig[i])
				displayState = DISP_GATE;
		}
		runTrig[4] = linkedTrigger.process(params[LINKED_PARAM].value);
		if (params[LINKED_PARAM].value > 0.5f) {// Linked run states
			if (stepConfig >= 2) {// 2x32
				runTrig[1] = false;
				runTrig[3] = false;
			}
			if (stepConfig == 4) // 1x64
				runTrig[2] = false;
			if (runTrig[0] || runTrig[1] || runTrig[2] || runTrig[3] || runTrig[4]) {
				running[0] = runTrig[4] ? running[0] : !running[0];
				running[1] = running[0];
				running[2] = running[0];
				running[3] = running[0];
				if (running[0])
					for (int i = 0; i < 4; i++)
						indexStep[i] = 0;
			}
		}
		else {// Not linked
			for (int i = 0; i < 4; i += stepConfig) {
				if (runTrig[i]) {
					running[i] = !running[i];
					if (running[i])
						indexStep[i] = 0;
				}
			}
		}
		// Run state: turn off running for unused buttons when 2x32 and 1x64
		if (stepConfig >= 2) {// 2x32
			running[1] = 0;
			running[3] = 0;
		}
		if (stepConfig == 4) // 1x64
			running[2] = 0;
		
		// Gate button
		if (gateTrigger.process(params[GATE_PARAM].value)) {
			displayState = DISP_GATE;
		}		
		// Length button
		if (lengthTrigger.process(params[LENGTH_PARAM].value)) {
			if (displayState != DISP_LENGTH)
				displayState = DISP_LENGTH;
			else
				displayState = DISP_GATE;
		}
		// GateP button
		if (gatePTrigger.process(params[GATEP_PARAM].value)) {
			if (displayState != DISP_GATEP)
				displayState = DISP_GATEP;
			else
				displayState = DISP_GATE;
		}
		// Mode button
		if (modeTrigger.process(params[MODE_PARAM].value)) {
			if (displayState != DISP_MODE)
				displayState = DISP_MODE;
			else
				displayState = DISP_GATE;
		}
		// GateTrig button
		if (gateTrigTrigger.process(params[GATETRIG_PARAM].value)) {
			if (displayState != DISP_GATETRIG)
				displayState = DISP_GATETRIG;
			else
				displayState = DISP_GATE;
		}
		// Copy, paste, clear, fill (cpcf) buttons
		bool copyTrigged = copyTrigger.process(params[COPY_PARAM].value);
		bool pasteTrigged = pasteTrigger.process(params[PASTE_PARAM].value);
		bool clearTrigged = clearTrigger.process(params[CLEAR_PARAM].value);
		bool fillTrigged = fillTrigger.process(params[FILL_PARAM].value);
		if (copyTrigged || pasteTrigged || clearTrigged || fillTrigged) {
			if (displayState == DISP_GATE || displayState == DISP_GATEP) {
				cpcfInfo = 0;
				if (copyTrigged) cpcfInfo = 1;
				if (pasteTrigged) cpcfInfo = 2;
				if (clearTrigged) cpcfInfo = 3;
				if (fillTrigged) cpcfInfo = 4;
				if (displayState == DISP_GATEP) cpcfInfo *= -1;
				displayState = DISP_ROW_SEL;
				feedbackCP = feedbackCPinit;
			}
			else if (displayState == DISP_ROW_SEL)// abort copy
				displayState = cpcfInfo > 0 ? DISP_GATE : DISP_GATEP;
		}
		
		// Step LED button presses
		int row = -1;
		int col = -1;
		for (int i = 0; i < 64; i++) {
			if (stepTriggers[i].process(params[STEP_PARAMS + i].value)) {
				if (displayState == DISP_GATE) {
					gate[i] = !gate[i];
				}
				if (displayState == DISP_LENGTH) {
					row = i / (16 * stepConfig);
					if (stepConfig == 2 && row == 1) 
						row++;
					col = i % (16 * stepConfig);
					length[row] = col + 1;
				}
				if (displayState == DISP_GATEP) {
					gatep[i] = !gatep[i];
				}
				if (displayState == DISP_MODE) {
					row = i / (16 * stepConfig);
					if (stepConfig == 2 && row == 1) 
						row++;
					col = i % (16 * stepConfig);
					if (col >= 4 && col <= 8)
						mode[row] = col - 4;
				}
				if (displayState == DISP_GATETRIG) {
					row = i / (16 * stepConfig);
					if (stepConfig == 2 && row == 1) 
						row++;
					col = i % (16 * stepConfig);
					if (col == 10)
						trig[row] = false;
					else if (col == 11)
						trig[row] = true;
				}
				if (displayState == DISP_ROW_SEL) {
					row = i / 16;// copy-paste done on blocks of 16 even when in 2x32 or 1x64 config
					if (abs(cpcfInfo) == 1) {// copy
						for (int j = 0; j < 16; j++) {
							copyPasteBuf[j] = cpcfInfo > 0 ? gate[row * 16 + j] : gatep[row * 16 + j];
							if (cpcfInfo < 0)
								copyPasteBufProb[j] = gatepval[row * 16 + j];
						}
					}					
					else if (abs(cpcfInfo) == 2) {// paste
						for (int j = 0; j < 16; j++) {
							if (cpcfInfo > 0) // paste gate
								gate[row * 16 + j] = copyPasteBuf[j];			
							else {// fill gatep and gatepprob
								gatep[row * 16 + j] = copyPasteBuf[j];
								gatepval[row * 16 + j] = copyPasteBufProb[j];
							}
						}
					}					
					else if (abs(cpcfInfo) == 3 || abs(cpcfInfo) == 4) {// clear or fill (no effect on gatepval)
						for (int j = 0; j < 16; j++) {
							if (cpcfInfo > 0) // clear gate
								gate[row * 16 + j] = (cpcfInfo == 3 ? false : true);
							else // clear gatep
								gatep[row * 16 + j] = (cpcfInfo == -3 ? false : true);
						}
					}
					displayState = (cpcfInfo >= 0 ? DISP_GATE : DISP_GATEP);
				}
			}
		}
		
		// Process PulseGenerators (even if may not use)
		for (int i = 0; i < 4; i++)
			gatePulsesValue[i] = gatePulses[i].process(1.0 / engineSampleRate);
		
		// Clock
		bool clkTrig[4] = {};
		bool clkActive[4] = {};
		for (int i = 0; i < 4; i++) {
			clkTrig[i] = clockTriggers[i].process(inputs[CLOCK_INPUTS + i].value);
			clkActive[i] = inputs[CLOCK_INPUTS + i].active;
		}
		for (int i = 0; i < 4; i += stepConfig) {
			if (running[i] && clockIgnoreOnReset == 0l && clkTrig[findClock(i, clkActive)]) {
				moveIndexRunMode(&indexStep[i], length[i], mode[i], &stepIndexRunHistory[i]);
				gatePulses[i].trigger(0.001f);
				gateRandomEnable[i] = !gatep[i * 16 + indexStep[i]];// not random yet
				gateRandomEnable[i] |= randomUniform() < ((float)(gatepval[i * 16 + indexStep[i]]))/100.0f;// randomUniform is [0.0, 1.0), see include/util/common.hpp
			}
		}
		
		// Gate outputs
		for (int i = 0; i < 4; i += stepConfig) {
			if (running[i]) {
				bool gateOut = gate[i * 16 + indexStep[i]] && gateRandomEnable[i];
				if (trig[i])
					gateOut &= gatePulsesValue[i];
				else
					gateOut &= clockTriggers[findClock(i, clkActive)].isHigh();
				outputs[GATE_OUTPUTS + i + (stepConfig - 1)].value = gateOut ? 10.0f : 0.0f;
			}
			else {// not running 
				outputs[GATE_OUTPUTS + i + (stepConfig - 1)].value = 0.0f;
			}		
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			for (int i = 0; i < 4; i++) {
				indexStep[i] = 0;
				clockTriggers[i].reset();
				gateRandomEnable[i] = true;
			}
			resetLight = 1.0f;
			displayState = DISP_GATE;
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
		}
		else
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime();		
		
		// Step LED button lights
		int rowToLight = -1;
		if (displayState == DISP_ROW_SEL) 
			rowToLight = CalcRowToLight(feedbackCP, feedbackCPinit);
		for (int i = 0; i < 64; i++) {
			row = i / (16 * stepConfig);
			if (stepConfig == 2 && row == 1) 
				row++;
			col = i % (16 * stepConfig);
			if (displayState == DISP_GATE) {
				float stepHereOffset =  (indexStep[row] == col && running[row]) ? 0.5f : 0.0f;
				if (gate[i]) {
					setGreenRed(STEP_LIGHTS + i * 2, 1.0f - stepHereOffset, gatep[i] ? (1.0f - stepHereOffset) : 0.0f);
				}
				else {
					setGreenRed(STEP_LIGHTS + i * 2, stepHereOffset / 5.0f, 0.0f);
				}
			}
			else if (displayState == DISP_LENGTH) {
				if (col < (length[row] - 1))
					setGreenRed(STEP_LIGHTS + i * 2, 0.1f, 0.0f);
				else if (col == (length[row] - 1))
					setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
				else 
					setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
			}
			else if (displayState == DISP_GATEP) {
				if (gatep[i])
					setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 1.0f);
				else 
					setGreenRed(STEP_LIGHTS + i * 2, gate[i] ? 0.1f : 0.0f, 0.0f);
			}
			else if (displayState == DISP_MODE) {
				if (col < 4 || col > 8) {
					setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
				}
				else { 
					if (col - 4 == mode[row])
						setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
					else
						setGreenRed(STEP_LIGHTS + i * 2, 0.1f, 0.0f);
				}
			}
			else if (displayState == DISP_GATETRIG) {
				if (col < 10 || col > 11) {
					setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
				}
				else {
					if (col - 10 == (trig[row] ? 1 : 0))
						setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
					else
						setGreenRed(STEP_LIGHTS + i * 2, 0.1f, 0.0f);
				}
			}
			else if (displayState == DISP_ROW_SEL) {
				if ((i / 16) == rowToLight)
					setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
				else
					setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
			}
			else {
				setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);// should never happen
			}
		}
		
		// Main button lights
		lights[GATE_LIGHT].value = displayState == DISP_GATE ? 1.0f : 0.0f;// green
		lights[LENGTH_LIGHT].value = displayState == DISP_LENGTH ? 1.0f : 0.0f;// green
		lights[GATEP_LIGHT].value = displayState == DISP_GATEP ? 1.0f : 0.0f;// red
		lights[MODE_LIGHT].value = displayState == DISP_MODE ? 1.0f : 0.0f;// blue		
		lights[GATETRIG_LIGHT].value = displayState == DISP_GATETRIG ? 1.0f : 0.0f;// blue

		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	

		// Run lights
		for (int i = 0; i < 4; i++)
			lights[RUN_LIGHTS + i].value = running[i] ? 1.0f : 0.0f;
		
		// ClockIn and GateOut tiny lights
		for (int i = 0; i < 4; i++) {
			lights[CLOCKIN_LIGHTS + i].value = ((i % stepConfig) == 0 ? 1.0f : 0.0f);
			lights[GATEOUT_LIGHTS + 3 - i].value = ((i % stepConfig) == 0 ? 1.0f : 0.0f);
		}
		
		if (feedbackCP > 0l)			
			feedbackCP--;	
		else
			feedbackCP = feedbackCPinit;// roll over
		
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// step()
	
	
	void setGreenRed(int id, float green, float red) {
		lights[id + 0].value = green;
		lights[id + 1].value = red;
	}

	int CalcRowToLight(long feedbackCP, long feedbackCPinit) {
		int rowToLight = -1;
		long onDelta = feedbackCPinit / 14;
		long onThreshold;// top based
		
		onThreshold = feedbackCPinit;
		if (feedbackCP < onThreshold && feedbackCP > (onThreshold - onDelta))
			rowToLight = 0;
		else {
			onThreshold = feedbackCPinit * 3 / 4;
			if (feedbackCP < onThreshold && feedbackCP > (onThreshold - onDelta))
				rowToLight = 1;
			else {
				onThreshold = feedbackCPinit * 2 / 4;
				if (feedbackCP < onThreshold && feedbackCP > (onThreshold - onDelta))
					rowToLight = 2;
				else {
					onThreshold = feedbackCPinit * 1 / 4;
					if (feedbackCP < onThreshold && feedbackCP > (onThreshold - onDelta))
						rowToLight = 3;
				}
			}
		}
		return rowToLight;
	}
};// GateSeq64 : module

struct GateSeq64Widget : ModuleWidget {
		
		struct ProbDisplayWidget : TransparentWidget {
		GateSeq64 *module;
		std::shared_ptr<Font> font;
		char displayStr[4];
		
		ProbDisplayWidget() {
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
			
			// display module->displayProb;
			displayStr[3] = 0;// more safety
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	GateSeq64Widget(GateSeq64 *module) : ModuleWidget(module) {
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/GateSeq64.svg")));

		// Screw holes (optical illustion makes screws look oval, remove for now)
		/*addChild(new ScrewHole(Vec(15, 0)));
		addChild(new ScrewHole(Vec(box.size.x-30, 0)));
		addChild(new ScrewHole(Vec(15, 365)));
		addChild(new ScrewHole(Vec(box.size.x-30, 365)));*/
		
		// Screws
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 365)));

		
		
		// ****** Sides (8 rows) ******
		
		static const int rowRuler0 = 40;
		static const int colRuler0 = 20;
		static const int colRuler6 = 406;
		static const int rowSpacingSides = 40;
		static int offsetTinyLight = 22;
		
		// Clock inputs
		int iSides = 0;
		for (; iSides < 4; iSides++) {
			addInput(Port::create<PJ301MPortS>(Vec(colRuler0, rowRuler0 + iSides * rowSpacingSides), Port::INPUT, module, GateSeq64::CLOCK_INPUTS + iSides));
			addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(Vec(colRuler0 + offsetTinyLight, rowRuler0 + iSides * rowSpacingSides), module, GateSeq64::CLOCKIN_LIGHTS + iSides));
		}
		// Run CVs
		for (; iSides < 8; iSides++) {
			addInput(Port::create<PJ301MPortS>(Vec(colRuler0, rowRuler0 + 5 + iSides * rowSpacingSides), Port::INPUT, module, GateSeq64::RUNCV_INPUTS + iSides - 4));
			
		}
		// Run LED bezel and light, four times
		for (iSides = 4; iSides < 8; iSides++) {
			addParam(ParamWidget::create<LEDBezel>(Vec(colRuler0 + 43 + offsetLEDbezel, rowRuler0 + 5 + iSides * rowSpacingSides + offsetLEDbezel), module, GateSeq64::RUN_PARAMS + iSides - 4, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRuler0 + 43 + offsetLEDbezel + offsetLEDbezelLight, rowRuler0 + 5 + iSides * rowSpacingSides + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RUN_LIGHTS + iSides - 4));
		}

		// Outputs
		iSides = 0;
		for (; iSides < 4; iSides++) {
			addOutput(Port::create<PJ301MPortS>(Vec(colRuler6, rowRuler0 + iSides * rowSpacingSides), Port::OUTPUT, module, GateSeq64::GATE_OUTPUTS + iSides));
			addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(Vec(colRuler6 + offsetTinyLight, rowRuler0 + iSides * rowSpacingSides), module, GateSeq64::GATEOUT_LIGHTS + iSides));
		}
		
		
		
		// ****** Steps LED buttons ******
		
		static int colRulerSteps = 60;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		
		for (int y = 0; y < 4; y++) {
			int posX = colRulerSteps;
			for (int x = 0; x < 16; x++) {
				addParam(ParamWidget::create<LEDButton>(Vec(posX, rowRuler0 + 8 + y * rowSpacingSides - 4.4f), module, GateSeq64::STEP_PARAMS + y * 16 + x, 0.0f, 1.0f, 0.0f));
				addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(posX + 4.4f, rowRuler0 + 8 + y * rowSpacingSides), module, GateSeq64::STEP_LIGHTS + (y * 16 + x) * 2));
				posX += spacingSteps;
				if ((x + 1) % 4 == 0)
					posX += spacingSteps4;
			}
		}
			
		
		
		// ****** 4x3 Main center bottom half Control section ******
		
		static int colRulerC0 = 112;
		static int colRulerSpacing = 70;
		static int colRulerC1 = colRulerC0 + colRulerSpacing;
		static int colRulerC2 = colRulerC1 + colRulerSpacing;
		static int colRulerC3 = colRulerC2 + colRulerSpacing;
		static int colRulerC4 = colRulerC3 + colRulerSpacing;
		static int rowRulerC0 = 217;
		static int rowRulerSpacing = 48;
		static int rowRulerC1 = rowRulerC0 + rowRulerSpacing;
		static int rowRulerC2 = rowRulerC1 + rowRulerSpacing;
		static const int posLEDvsButton = + 25;
		
		// Length light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC0 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::LENGTH_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerC0 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq64::LENGTH_LIGHT));		
		// Mode light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC1 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::MODE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerC1 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq64::MODE_LIGHT));		
		// GateTrig light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC2 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::GATETRIG_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerC2 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq64::GATETRIG_LIGHT));		
		// Gate light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC3 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::GATE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerC3 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq64::GATE_LIGHT));		
		// Gate p light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC4 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::GATEP_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(colRulerC4 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq64::GATEP_LIGHT));		
		
		// Linked button
		addParam(ParamWidget::create<CKSS>(Vec(colRulerC0 + hOffsetCKSS, rowRulerC1 + vOffsetCKSS), module, GateSeq64::LINKED_PARAM, 0.0f, 1.0f, 0.0f)); // 1.0f is top position
		// Reset LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(colRulerC1 + offsetLEDbezel, rowRulerC1 + offsetLEDbezel), module, GateSeq64::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRulerC1 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC1 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RESET_LIGHT));
		// Copy/paste switches
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2 - 10, rowRulerC1 + offsetTL1105), module, GateSeq64::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2 + 20, rowRulerC1 + offsetTL1105), module, GateSeq64::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		
		// Sequence display
		ProbDisplayWidget *probDisplay = new ProbDisplayWidget();
		probDisplay->box.pos = Vec(colRulerC4-15, rowRulerC1 + 3 + vOffsetDisplay);
		probDisplay->box.size = Vec(55, 30);// 3 characters
		probDisplay->module = module;
		addChild(probDisplay);
		// Run mode button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC4 + offsetCKD6b, rowRulerC1 + 0 + offsetCKD6b), module, GateSeq64::PROB_KNOB_PARAM, 0.0f, 1.0f, 0.0f));
		
		// Config switch (3 position)
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(colRulerC0 + hOffsetCKSS, rowRulerC2 + vOffsetCKSSThree), module, GateSeq64::CONFIG_PARAM, 0.0f, 2.0f, 0.0f));	// 0.0f is top position
		// Reset
		addInput(Port::create<PJ301MPortS>(Vec(colRulerC1, rowRulerC2 ), Port::INPUT, module, GateSeq64::RESET_INPUT));
		// Clear/fill switches
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2 - 10, rowRulerC2 + offsetTL1105), module, GateSeq64::CLEAR_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2 + 20, rowRulerC2 + offsetTL1105), module, GateSeq64::FILL_PARAM, 0.0f, 1.0f, 0.0f));
	}
};

Model *modelGateSeq64 = Model::create<GateSeq64, GateSeq64Widget>("Impromptu Modular", "Gate-Seq-64", "Gate-Seq-64", SEQUENCER_TAG);
