//***********************************************************************************************
//Gate sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept by Nigel Sixsmith and Marc Boulé
//
//Acknowledgements: please see README.md
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct GateSeq64 : Module {
	enum ParamIds {
		ENUMS(STEP_PARAMS, 64),
		GATE_PARAM,
		LENGTH_PARAM,
		GATEP_PARAM,
		MODES_PARAM,
		RUN_PARAM,
		CONFIG_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		PROB_KNOB_PARAM,
		EDIT_PARAM,
		SEQUENCE_PARAM,
		CPMODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(CLOCK_INPUTS, 4),
		RESET_INPUT,
		RUNCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_LIGHTS, 64 * 2),// room for GreenRed
		GATE_LIGHT,
		LENGTH_LIGHT,
		GATEP_LIGHT,
		P_LIGHT,
		MODES_LIGHT,
		RUN_LIGHT,
		RESET_LIGHT,
		NUM_LIGHTS
	};
	
	enum DisplayStateIds {DISP_GATE, DISP_LENGTH, DISP_GATEP, DISP_MODES, DISP_ROW_SEL};
	
	// Need to save
	bool running;
	int length[4] = {};// values are 1 to 16
	int mode[4] = {};
	bool trig[4] = {};
	bool gate[64] = {};
	bool gatep[64] = {};
	int gatepval[64] = {};// values are 0 to 100

	// No need to save
	int displayState;
	int indexStep[4] = {};
	int stepIndexRunHistory[4] = {};// no need to initialize
	bool cpBufGate[16] = {};// copy-paste only one row
	bool cpBufGateP[16] = {};// copy-paste only one row
	int cpBufGatePVal[16] = {};// copy-paste only one row
	long feedbackCP;// downward step counter for CP feedback
	bool gateRandomEnable[4];// no need to initialize
	float resetLight = 0.0f;
	long feedbackCPinit;// no need to initialize
	bool gatePulsesValue[4] = {};
	int cpcfInfo;// 4 LSbits are: copy = 1, paste = 2, clear = 3, fill = 4; next 2 LS bits indicate gate, gatep, gatepval
	long clockIgnoreOnReset;
	const float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
	int displayProb;// -1 when no step was clicked (when moving to DISP_GATEP mode), 0 to 63 when a step was selected and its prob can be changed.
	int probKnob;// INT_MAX when knob not seen yet

	
	SchmittTrigger gateTrigger;
	SchmittTrigger lengthTrigger;
	SchmittTrigger gatePTrigger;
	SchmittTrigger modesTrigger;
	SchmittTrigger stepTriggers[64];
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTriggers[4];
	SchmittTrigger resetTrigger;
	SchmittTrigger linkedTrigger;
	SchmittTrigger configTrigger;
	SchmittTrigger configTrigger2;
	SchmittTrigger configTrigger3;

	PulseGenerator gatePulses[4];

		
	GateSeq64() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		displayState = DISP_GATE;
		running = false;
		for (int i = 0; i < 64; i++) {
			gate[i] = false;
			gatep[i] = false;
			gatepval[i] = 50;
		}
		for (int i = 0; i < 4; i++) {
			indexStep[i] = 0;
			length[i] = 16;
			mode[i] = MODE_FWD;
			trig[i] = false;
			gateRandomEnable[i] = true;
		}
		for (int i = 0; i < 16; i++) {
			cpBufGate[i] = false;
			cpBufGateP[i] = false;
			cpBufGatePVal[i] = 50;
		}
		feedbackCP = 0l;
		displayProb = -1;
		probKnob = INT_MAX;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}

	void onRandomize() override {
		displayState = DISP_GATE;
		running = (randomUniform() > 0.5f);
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
			indexStep[i] = 0;
			length[i] = 1 + (randomu32() % (16 * stepConfig));
			mode[i] = randomu32() % 5;
			trig[i] = (randomUniform() > 0.5f);
		}
		for (int i = 0; i < 16; i++) {
			cpBufGate[i] = false;
			cpBufGateP[i] = false;
			cpBufGatePVal[i] = 50;
		}
		feedbackCP = 0l;
		displayProb = -1;
		probKnob = INT_MAX;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
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
		if (runningJ)
			running = json_is_true(runningJ);
		
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
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		// Config switch
		int stepConfig = 1;// 4x16
		if (params[CONFIG_PARAM].value > 1.5f)// 1x64
			stepConfig = 4;
		else if (params[CONFIG_PARAM].value > 0.5f)// 2x32
			stepConfig = 2;
		// Config: set lengths to their max when move switch, and set switches same when linked
		bool configTrigged = configTrigger.process(params[CONFIG_PARAM].value*10.0f - 10.0f) || 
			configTrigger2.process(params[CONFIG_PARAM].value*10.0f) ||
			configTrigger3.process(params[CONFIG_PARAM].value*-5.0f + 10.0f);
		if (configTrigged) {
			for (int i = 0; i < 4; i += stepConfig)
				length[i] = 16 * stepConfig;
		}
		

		// Run state button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUNCV_INPUT].value)) {
			running = !running;
			if (running) {
				for (int i = 0; i < 4; i++)
					indexStep[i] = 0;			
			}
			displayState = DISP_GATE;
		}
		
		// Length button
		if (lengthTrigger.process(params[LENGTH_PARAM].value)) {
			if (displayState != DISP_LENGTH)
				displayState = DISP_LENGTH;
			else
				displayState = DISP_GATE;
		}
		
		// Modes button
		if (modesTrigger.process(params[MODES_PARAM].value)) {
			if (displayState != DISP_MODES)
				displayState = DISP_MODES;
			else
				displayState = DISP_GATE;
		}
		
		// Gate button
		if (gateTrigger.process(params[GATE_PARAM].value)) {
			displayState = DISP_GATE;
		}	
		
		// GateP button
		if (gatePTrigger.process(params[GATEP_PARAM].value)) {
			if (displayState != DISP_GATEP) {
				displayState = DISP_GATEP;
				displayProb = -1;
			}
			else
				displayState = DISP_GATE;
		}	
		
		// Prob knob
		int newProbKnob = (int)roundf(params[PROB_KNOB_PARAM].value * 14.0f);
		if (probKnob == INT_MAX)
			probKnob = newProbKnob;
		if (newProbKnob != probKnob) {
			if (displayState == DISP_GATEP && (abs(newProbKnob - probKnob) <= 3) && displayProb != -1 ) {// avoid discontinuous step (initialize for example)
				gatepval[displayProb] += (newProbKnob - probKnob) * 2;
				if (gatepval[displayProb] > 100)
					gatepval[displayProb] = 100;
				if (gatepval[displayProb] < 0)
					gatepval[displayProb] = 0;
			}
			probKnob = newProbKnob;// must do this step whether running or not
		}	

		// Copy, paste, clear, fill (cpcf) buttons
		bool copyTrigged = copyTrigger.process(params[COPY_PARAM].value);
		bool pasteTrigged = pasteTrigger.process(params[PASTE_PARAM].value);
		if (copyTrigged || pasteTrigged) {
			if (displayState == DISP_GATE || displayState == DISP_GATEP) {
				cpcfInfo = 0;
				if (copyTrigged) cpcfInfo = 1;
				if (pasteTrigged) cpcfInfo = 2;
				if (displayState == DISP_GATEP) cpcfInfo += 16;
				displayState = DISP_ROW_SEL;
				feedbackCP = feedbackCPinit;
			}
			else if (displayState == DISP_ROW_SEL) {// abort copy
				if (cpcfInfo >= 16)
					displayState = DISP_GATEP;
				else 
					displayState = DISP_GATE;
			}
		}
		
		
		// Step LED buttons
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
					if (displayProb == -1 || (displayProb != -1 && i != displayProb)) {// Click new step
						displayProb = i;// Display its prob and allow adjust
						gatep[i] = true;// Turn on (or keep on) when click new step
					}
					else {
						if (i == displayProb) {// Click step again
							gatep[i] = !gatep[i];
							if (!gatep[i])
								displayProb = -1;							
						}
					}
				}
				if (displayState == DISP_MODES) {
					row = i / (16 * stepConfig);
					if (stepConfig == 2 && row == 1) 
						row++;
					col = i % (16 * stepConfig);
					if (col >= 0 && col <= 4)
						mode[row] = col - 0;
					if (col == 12)
						trig[row] = false;
					else if (col == 13)
						trig[row] = true;
				}
				if (displayState == DISP_ROW_SEL) {
					row = i / 16;// copy-paste done on blocks of 16 even when in 2x32 or 1x64 config
					int button = (cpcfInfo & 0xF);
					if (button == 1) {// copy
						for (int j = 0; j < 16; j++) {
							// copy all attributes, in case params[CP_ALL_PARAM].value changes in between copy-paste ops
							cpBufGatePVal[j] = gatepval[row * 16 + j];
							cpBufGateP[j] = gatep[row * 16 + j];
							cpBufGate[j] = gate[row * 16 + j];
						}
					}					
					else if (button == 2) {// paste
						for (int j = 0; j < 16; j++) {
							if ((cpcfInfo >= 32) /*|| (params[CP_ALL_PARAM].value > 0.5f)*/)// gatepval
								gatepval[row * 16 + j] = cpBufGatePVal[j];
							if ((cpcfInfo >= 16 && cpcfInfo < 32) /*|| (params[CP_ALL_PARAM].value > 0.5f)*/)// gatep
								gatep[row * 16 + j] = cpBufGateP[j];
							if ((cpcfInfo < 16) /*|| (params[CP_ALL_PARAM].value > 0.5f)*/)// gate
								gate[row * 16 + j] = cpBufGate[j];
						}
					}			
					else if (button == 3 || button == 4) {// clear or fill
						for (int j = 0; j < 16; j++) {
							if (cpcfInfo >= 32)// gatepval
								gatepval[row * 16 + j] = (button == 3 ? 0 : 100);
							else if (cpcfInfo >= 16)// gatep
								gatep[row * 16 + j] = (button == 3 ? false : true);
							else// gate
								gate[row * 16 + j] = (button == 3 ? false : true);
						}
					}
					if (cpcfInfo >= 16)
						displayState = DISP_GATEP;
					else 
						displayState = DISP_GATE;
				}
			}
		}
		
		
		//********** Clock and reset **********
		
		// Clock
		bool clkTrig[4] = {};
		bool clkActive[4] = {};
		for (int i = 0; i < 4; i++) {
			clkTrig[i] = clockTriggers[i].process(inputs[CLOCK_INPUTS + i].value);
			clkActive[i] = inputs[CLOCK_INPUTS + i].active;
		}
		if (running) {
			for (int i = 0; i < 4; i += stepConfig) {
				if (clockIgnoreOnReset == 0l && clkTrig[findClock(i, clkActive)]) {
					moveIndexRunMode(&indexStep[i], length[i], mode[i], &stepIndexRunHistory[i]);
					gatePulses[i].trigger(0.001f);
					gateRandomEnable[i] = !gatep[i * 16 + indexStep[i]];// not random yet
					gateRandomEnable[i] |= randomUniform() < ((float)(gatepval[i * 16 + indexStep[i]]))/100.0f;// randomUniform is [0.0, 1.0), see include/util/common.hpp
				}
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
		
		
		//********** Outputs and lights **********
		
		// Process PulseGenerators (even if may not use)
		for (int i = 0; i < 4; i++)
			gatePulsesValue[i] = gatePulses[i].process(1.0 / engineSampleRate);
		
		// Gate outputs
		for (int i = 0; i < 4; i += stepConfig) {
			if (running) {
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
				float stepHereOffset =  (indexStep[row] == col && running) ? 0.5f : 0.0f;
				if (gate[i]) {
					setGreenRed(STEP_LIGHTS + i * 2, 1.0f - stepHereOffset, gatep[i] ? (1.0f /* * gatepval[i]/100.0f */- stepHereOffset) : 0.0f);
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
			else if (displayState == DISP_MODES) {
				if (col >= 0 && col <= 4) { 
					if (col - 0 == mode[row])
						setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
					else
						setGreenRed(STEP_LIGHTS + i * 2, 0.1f, 0.0f);
				}
				else if (col >= 12 && col <= 13) {
					if (col - 12 == (trig[row] ? 1 : 0))
						setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
					else
						setGreenRed(STEP_LIGHTS + i * 2, 0.1f, 0.0f);
				}
				else {
					setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
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
		lights[GATE_LIGHT].value = displayState == DISP_GATE ? 1.0f : 0.0f;
		lights[LENGTH_LIGHT].value = displayState == DISP_LENGTH ? 1.0f : 0.0f;
		lights[GATEP_LIGHT].value = displayState == DISP_GATEP ? 1.0f : 0.0f;
		lights[MODES_LIGHT].value = displayState == DISP_MODES ? 1.0f : 0.0f;		

		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	

		// Run lights
		lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;
		
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
		
		struct SequenceDisplayWidget : TransparentWidget {
			GateSeq64 *module;
			std::shared_ptr<Font> font;
			
			SequenceDisplayWidget() {
				font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
			}

			void draw(NVGcontext *vg) override {
				NVGcolor textColor = prepareDisplay(vg, &box);
				nvgFontFaceId(vg, font->handle);
				//nvgTextLetterSpacing(vg, 2.5);

				Vec textPos = Vec(6, 24);
				nvgFillColor(vg, nvgTransRGBA(textColor, 16));
				nvgText(vg, textPos.x, textPos.y, "~~", NULL);
				nvgFillColor(vg, textColor);
				char displayStr[3];
				//snprintf(displayStr, 3, "%2u", (unsigned) (module->params[PhraseSeq16::EDIT_PARAM].value > 0.5f ? module->sequence : module->phrase[module->phraseIndexEdit]) + 1 );
				snprintf(displayStr, 3, "TD");// TODO use line above instead
				nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
			}
		};	
		
		
		struct ProbDisplayWidget : TransparentWidget {
		GateSeq64 *module;
		std::shared_ptr<Font> font;
		char displayStr[3];
		
		ProbDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box);
			nvgFontFaceId(vg, font->handle);
			//nvgTextLetterSpacing(vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			nvgText(vg, textPos.x, textPos.y, "~~", NULL);
			nvgFillColor(vg, textColor);
			
			if (module->displayState == GateSeq64::DISP_GATEP) {
				if (module->displayProb != -1) {
					int prob = module->gatepval[module->displayProb];
					if ( prob>= 100)
						snprintf(displayStr, 3, "1*");
					else if (prob >= 1)
						snprintf(displayStr, 3, "%02d", prob);
					else
						snprintf(displayStr, 3, " 0");
				}
				else
					snprintf(displayStr, 3, "--");
			}
			else
				displayStr[0] = 0;
			displayStr[2] = 0;// more safety
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
		
		// Clock inputs
		for (int iSides = 0; iSides < 4; iSides++) {
			addInput(Port::create<PJ301MPortS>(Vec(colRuler0, rowRuler0 + iSides * rowSpacingSides), Port::INPUT, module, GateSeq64::CLOCK_INPUTS + iSides));
		}

		// Outputs
		for (int iSides = 0; iSides < 4; iSides++) {
			addOutput(Port::create<PJ301MPortS>(Vec(colRuler6, rowRuler0 + iSides * rowSpacingSides), Port::OUTPUT, module, GateSeq64::GATE_OUTPUTS + iSides));
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
			
		
		
		// ****** 5x3 Main bottom half Control section ******
		
		static int colRulerCm1 = 30;// seq/song switch and seq display
		static int colRulerC0 = 112;
		static int colRulerSpacing = 70;
		static int colRulerC1 = colRulerC0 + colRulerSpacing;
		static int colRulerC2 = colRulerC1 + colRulerSpacing;
		static int colRulerC3 = colRulerC2 + colRulerSpacing;
		//static int colRulerC4 = colRulerC3 + colRulerSpacing;
		static int rowRulerC0 = 217;
		static int rowRulerSpacing = 48;
		static int rowRulerC1 = rowRulerC0 + rowRulerSpacing;
		static int rowRulerC2 = rowRulerC1 + rowRulerSpacing;
		static const int posLEDvsButton = + 25;
		
		// *** First row
		
		// Monitor
		addParam(ParamWidget::create<CKSSH>(Vec(colRulerCm1 + hOffsetCKSSH, rowRulerC0 + vOffsetCKSSH), module, GateSeq64::EDIT_PARAM, 0.0f, 1.0f, 1.0f));		
		// Length light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC0 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::LENGTH_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerC0 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq64::LENGTH_LIGHT));		
		// Modes light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC1 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::MODES_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerC1 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq64::MODES_LIGHT));		
		// Gate light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC2 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::GATE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerC2 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq64::GATE_LIGHT));		
		// Gate p light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC3 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::GATEP_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(colRulerC3 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq64::GATEP_LIGHT));		
	
		
		// *** Second row
		// Steps display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(colRulerCm1 - 7, rowRulerC1 + vOffsetDisplay);
		displaySequence->box.size = Vec(40, 30);// 2 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Config switch (3 position)
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(colRulerC0 - 30 + hOffsetCKSS + 1, rowRulerC1 + 25 + vOffsetCKSSThree), module, GateSeq64::CONFIG_PARAM, 0.0f, 2.0f, 0.0f));	// 0.0f is top position
		// Run CV and LED bezel and light
		addInput(Port::create<PJ301MPortS>(Vec(colRulerC1 + 7, rowRulerC1 ), Port::INPUT, module, GateSeq64::RUNCV_INPUT));
		addParam(ParamWidget::create<LEDBezel>(Vec(colRulerC1 - 37 + offsetLEDbezel, rowRulerC1 + offsetLEDbezel), module, GateSeq64::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRulerC1 - 37 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC1 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RUN_LIGHT));
		// Copy/paste buttons
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2 - 10, rowRulerC1 + offsetTL1105), module, GateSeq64::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2 + 20, rowRulerC1 + offsetTL1105), module, GateSeq64::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		// Probability display
		ProbDisplayWidget *probDisplay = new ProbDisplayWidget();
		probDisplay->box.pos = Vec(colRulerC3 - 7, rowRulerC1 + vOffsetDisplay);
		probDisplay->box.size = Vec(40, 30);// 3 characters
		probDisplay->module = module;
		addChild(probDisplay);
		

		// *** Third row

		// Sequence knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(colRulerCm1 + 1 + offsetDavies1900, rowRulerC1 + 50 + offsetDavies1900), module, GateSeq64::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f));		
		// Reset CV and LED bezel and light
		addInput(Port::create<PJ301MPortS>(Vec(colRulerC1 + 7, rowRulerC2 ), Port::INPUT, module, GateSeq64::RESET_INPUT));
		addParam(ParamWidget::create<LEDBezel>(Vec(colRulerC1 - 37 + offsetLEDbezel, rowRulerC2 + offsetLEDbezel), module, GateSeq64::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRulerC1 - 37 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC2 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RESET_LIGHT));
		// Copy-paste mode switch (3 position)
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(colRulerC2 + hOffsetCKSS + 1, rowRulerC2 - 1 + vOffsetCKSSThree), module, GateSeq64::CPMODE_PARAM, 0.0f, 2.0f, 2.0f));	// 0.0f is top position
		// Probability knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(colRulerC3 + 1 + offsetDavies1900, rowRulerC1 + 50 + offsetDavies1900), module, GateSeq64::PROB_KNOB_PARAM, -INFINITY, INFINITY, 0.0f));		
	}
};

Model *modelGateSeq64 = Model::create<GateSeq64, GateSeq64Widget>("Impromptu Modular", "Gate-Seq-64", "Gate-Seq-64", SEQUENCER_TAG);
