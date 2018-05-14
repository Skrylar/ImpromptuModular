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
		MODES_PARAM,
		RUN_PARAM,
		CONFIG_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		PROB_KNOB_PARAM,
		EDIT_PARAM,
		SEQUENCE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		RUNCV_INPUT,
		SEQCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_LIGHTS, 64 * 2),// room for GreenRed
		P_LIGHT,
		RUN_LIGHT,
		RESET_LIGHT,
		NUM_LIGHTS
	};
	
	enum DisplayStateIds {DISP_GATE, DISP_LENGTH, DISP_MODES, DISP_ROW_SEL};// TODO remove DISP_ROW_SEL and make copy paste like in PSxx
	enum AttributeBitMasks {ATT_MSK_PROB = 0xFF, ATT_MSK_GATEP = 0x100, ATT_MSK_GATE = 0x200};
	
	// Need to save
	bool running;
	int runModeSeq;
	int runModeSong;
	//
	int sequence;
	int lengths[16] = {};// values are 1 to 16
	//
	int phrase[16] = {};// This is the song (series of phases; a phrase is a patten number)
	int phrases;//1 to 16
	//	
	bool trig[4] = {};
	int attributes[16][64] = {};

	// No need to save
	int displayState;
	int stepIndexRun;
	int phraseIndexEdit;	
	int phraseIndexRun;
	int stepIndexPhraseRun;
	int stepIndexRunHistory;// no need to initialize
	int stepIndexPhraseRunHistory;// no need to initialize
	int phraseIndexRunHistory;// no need to initialize
	int cpBufAttributes[16] = {};// copy-paste only one row
	int cpBufLength;// copy-paste only one row
	long feedbackCP;// downward step counter for CP feedback
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	float resetLight = 0.0f;
	long feedbackCPinit;// no need to initialize
	int cpInfo;// copy = 1, paste = 2
	long clockIgnoreOnReset;
	const float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
	int displayProb;// -1 when prob can not be modified, 0 to 63 when prob can be changed.
	int probKnob;// INT_MAX when knob not seen yet
	int sequenceKnob;// INT_MAX when knob not seen yet

	
	SchmittTrigger modesTrigger;
	SchmittTrigger stepTriggers[64];
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger configTrigger;
	SchmittTrigger configTrigger2;
	SchmittTrigger configTrigger3;
	SchmittTrigger editTrigger;
	SchmittTrigger editTriggerInv;

	
	inline bool getGate(int seq, int step) {return (attributes[seq][step] & ATT_MSK_GATE) != 0;}
	inline bool getGateP(int seq, int step) {return (attributes[seq][step] & ATT_MSK_GATEP) != 0;}
	inline int getGatePVal(int seq, int step) {return attributes[seq][step] & ATT_MSK_PROB;}
	inline bool isEditingSequence(void) {return params[EDIT_PARAM].value > 0.5f;}
		
	GateSeq64() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		displayState = DISP_GATE;
		running = false;
		runModeSeq = MODE_FWD;
		runModeSong = MODE_FWD;
		stepIndexRun = 0;
		sequence = 0;
		phrases = 4;
		phraseIndexEdit = 0;
		phraseIndexRun = 0;
		stepIndexPhraseRun = 0;
		for (int r = 0; r < 4; r++) {
			trig[r] = false;
		}
		for (int i = 0; i < 16; i++) {
			for (int s = 0; s < 64; s++) {
				attributes[i][s] = 50;
			}
			phrase[i] = 0;
			lengths[i] = 16;
			cpBufAttributes[i] = 50;
		}
		cpBufLength = 16;
		feedbackCP = 0l;
		displayProb = -1;
		infoCopyPaste = 0l;
		probKnob = INT_MAX;
		sequenceKnob = INT_MAX;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}

	void onRandomize() override {
		int stepConfig = 1;// 4x16
		if (params[CONFIG_PARAM].value > 1.5f)// 1x64
			stepConfig = 4;
		else if (params[CONFIG_PARAM].value > 0.5f)// 2x32
			stepConfig = 2;
		//
		displayState = DISP_GATE;
		running = (randomUniform() > 0.5f);
		runModeSeq = randomu32() % 5;
		runModeSong = randomu32() % 5;
		sequence = randomu32() % 16;
		phrases = 1 + (randomu32() % 16);
		phraseIndexEdit = 0;
		phraseIndexRun = 0;
		stepIndexPhraseRun = 0;
		configTrigger.reset();
		configTrigger2.reset();
		configTrigger3.reset();
		for (int i = 0; i < 16; i++) {
			for (int s = 0; s < 64; s++) {
				attributes[i][s] = (randomu32() % 101) | (randomu32() & (ATT_MSK_GATEP | ATT_MSK_GATE));
			}
			phrase[i] = randomu32() % 16;
			lengths[i] = 1 + (randomu32() % (16 * stepConfig));
			cpBufAttributes[i] = 50;
		}
		stepIndexRun = (runModeSeq == MODE_REV ? lengths[sequence] - 1 : 0);
		cpBufLength = 16;
		feedbackCP = 0l;
		displayProb = -1;
		infoCopyPaste = 0l;
		probKnob = INT_MAX;
		sequenceKnob = INT_MAX;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// runModeSeq
		json_object_set_new(rootJ, "runModeSeq", json_integer(runModeSeq));

		// runModeSong
		json_object_set_new(rootJ, "runModeSong", json_integer(runModeSong));

		// sequence
		json_object_set_new(rootJ, "sequence", json_integer(sequence));

		// attributes
		json_t *attributesJ = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 64; s++) {
				json_array_insert_new(attributesJ, s + (i * 64), json_integer(attributes[i][s]));
			}
		json_object_set_new(rootJ, "attributes", attributesJ);
		
		// lengths
		json_t *lengthsJ = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(lengthsJ, i, json_integer(lengths[i]));
		json_object_set_new(rootJ, "lengths", lengthsJ);
	
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
		
		// runModeSeq
		json_t *runModeSeqJ = json_object_get(rootJ, "runModeSeq");
		if (runModeSeqJ)
			runModeSeq = json_integer_value(runModeSeqJ);
		
		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, "runModeSong");
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
		
		// sequence
		json_t *sequenceJ = json_object_get(rootJ, "sequence");
		if (sequenceJ)
			sequence = json_integer_value(sequenceJ);
		
		// attributes
		json_t *attributesJ = json_object_get(rootJ, "attributes");
		if (attributesJ) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 64; s++) {
					json_t *attributesArrayJ = json_array_get(attributesJ, s + (i * 64));
					if (attributesArrayJ)
						attributes[i][s] = json_integer_value(attributesArrayJ);
				}
		}
		
		// lengths
		json_t *lengthsJ = json_object_get(rootJ, "lengths");
		if (lengthsJ) {
			for (int i = 0; i < 16; i++)
			{
				json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
				if (lengthsArrayJ)
					lengths[i] = json_integer_value(lengthsArrayJ);
			}			
		}
		stepIndexRun = (runModeSeq == MODE_REV ? lengths[sequence] - 1 : 0);
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		static const float copyPasteInfoTime = 3.0f;// seconds
		float engineSampleRate = engineGetSampleRate();
		feedbackCPinit = (long) (copyPasteInfoTime * engineSampleRate);
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		bool editingSequence = isEditingSequence();// true = editing sequence, false = editing song
		if ( editTrigger.process(params[EDIT_PARAM].value) || editTriggerInv.process(1.0f - params[EDIT_PARAM].value) ) {
			displayState = DISP_GATE;
			displayProb = -1;
		}

		// Seq CV input
		if (inputs[SEQCV_INPUT].active) {
			sequence = (int) clamp( round(inputs[SEQCV_INPUT].value * 15.0f / 10.0f), 0.0f, 15.0f );
		}

		// Config switch
		int stepConfig = 1;// 4x16
		if (params[CONFIG_PARAM].value > 1.5f)// 1x64
			stepConfig = 4;
		else if (params[CONFIG_PARAM].value > 0.5f)// 2x32
			stepConfig = 2;
		// Config: cap lengths to their new max when move switch
		bool configTrigged = configTrigger.process(params[CONFIG_PARAM].value*10.0f - 10.0f) || 
			configTrigger2.process(params[CONFIG_PARAM].value*10.0f) ||
			configTrigger3.process(params[CONFIG_PARAM].value*-5.0f + 10.0f);
		if (configTrigged) {
			for (int i = 0; i < 16; i++)
				if (lengths[i] > 16 * stepConfig)
					lengths[i] = 16 * stepConfig;
			displayProb = -1;
		}
		
		// Run state button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUNCV_INPUT].value)) {
			running = !running;
			if (running) {
				stepIndexRun = (runModeSeq == MODE_REV ? lengths[sequence] - 1 : 0);				
				phraseIndexEdit = 0;				
				phraseIndexRun = 0;
				stepIndexPhraseRun = 0;
			}
			displayState = DISP_GATE;
			displayProb = -1;
		}
		
		// Modes button
		if (modesTrigger.process(params[MODES_PARAM].value)) {
			if (displayState == DISP_GATE || displayState == DISP_ROW_SEL)
				displayState = DISP_MODES;
			else if (displayState == DISP_MODES)
				displayState = DISP_LENGTH;
			else
				displayState = DISP_GATE;
			displayProb = -1;
		}
				
		// Prob knob
		int newProbKnob = (int)roundf(params[PROB_KNOB_PARAM].value * 14.0f);
		if (probKnob == INT_MAX)
			probKnob = newProbKnob;
		if (newProbKnob != probKnob) {
			if (editingSequence) {
				if ((abs(newProbKnob - probKnob) <= 3) && displayProb != -1 ) {// avoid discontinuous step (initialize for example)
					int pval = getGatePVal(sequence, displayProb);
					pval += (newProbKnob - probKnob) * 2;
					if (pval > 100)
						pval = 100;
					if (pval < 0)
						pval = 0;
					attributes[sequence][displayProb] = pval | (attributes[sequence][displayProb] & (ATT_MSK_GATE | ATT_MSK_GATEP));
				}
			}
			probKnob = newProbKnob;// must do this step whether running or not
		}	
		
		// Sequence knob  
		int newSequenceKnob = (int)roundf(params[SEQUENCE_PARAM].value*7.0f);
		if (sequenceKnob == INT_MAX)
			sequenceKnob = newSequenceKnob;
		int deltaKnob = newSequenceKnob - sequenceKnob;
		if (deltaKnob != 0) {
			if (abs(deltaKnob) <= 3) {// avoid discontinuous step (initialize for example)
				if (displayState == DISP_MODES) {
					if (editingSequence) {
						runModeSeq += deltaKnob;
						if (runModeSeq < 0) runModeSeq = 0;
						if (runModeSeq > 4) runModeSeq = 4;
					}
					else {
						runModeSong += deltaKnob;
						if (runModeSong < 0) runModeSong = 0;
						if (runModeSong > 4) runModeSong = 4;
					}
				}
				else if (displayState == DISP_LENGTH) {
					if (editingSequence) {
						lengths[sequence] += deltaKnob;
						if (lengths[sequence] > (16 * stepConfig)) lengths[sequence] = (16 * stepConfig);
						if (lengths[sequence] < 1 ) lengths[sequence] = 1;
					}
					else {
						phrases += deltaKnob;
						if (phrases > 16) phrases = 16;
						if (phrases < 1 ) phrases = 1;
						if (phraseIndexEdit >= phrases) phraseIndexEdit = phrases - 1;
					}
					
				}
				else {
					if (editingSequence) {
						if (!inputs[SEQCV_INPUT].active) {
							sequence += deltaKnob;
							if (sequence < 0) sequence = 0;
							if (sequence > 15) sequence = 15;
						}
					}
					else {
						if (!running) {
							phrase[phraseIndexEdit] += deltaKnob;
							if (phrase[phraseIndexEdit] < 0) phrase[phraseIndexEdit] = 0;
							if (phrase[phraseIndexEdit] > 15) phrase[phraseIndexEdit] = 15;
						}						
					}	
				}					
			}
			sequenceKnob = newSequenceKnob;
			displayProb = -1;
		}

		// Copy, paste buttons
		bool copyTrigged = copyTrigger.process(params[COPY_PARAM].value);
		bool pasteTrigged = pasteTrigger.process(params[PASTE_PARAM].value);
		if (editingSequence) {
			if (copyTrigged || pasteTrigged) {
				if (displayState == DISP_GATE) {
					cpInfo = 0;
					if (copyTrigged) cpInfo = 1;
					if (pasteTrigged) cpInfo = 2;
					displayState = DISP_ROW_SEL;
					feedbackCP = feedbackCPinit;
				}
				else if (displayState == DISP_ROW_SEL) {// abort copy or paste
					displayState = DISP_GATE;
				}
				displayProb = -1;
			}
		}
		
		
		// Step LED button presses
		int row = -1;
		int col = -1;
		int stepPressed = -1;
		for (int i = 0; i < 64; i++) {
			if (stepTriggers[i].process(params[STEP_PARAMS + i].value))
				stepPressed = i;
		}		
		if (stepPressed != -1) {
			if (editingSequence) {
				if (displayState == DISP_LENGTH) {
					col = stepPressed % (16 * stepConfig);
					lengths[sequence] = col + 1;
				}
				else if (displayState == DISP_ROW_SEL) {
					row = stepPressed / 16;// copy-paste done on blocks of 16 even when in 2x32 or 1x64 config
					if (cpInfo == 1) {// copy
						for (int i = 0; i < 16; i++) {
							cpBufAttributes[i] = attributes[sequence][row * 16 + i];
							cpBufLength = lengths[sequence];
						}
					}					
					else if (cpInfo == 2) {// paste
						for (int i = 0; i < 16; i++) {
							attributes[sequence][row * 16 + i] = cpBufAttributes[i];
							lengths[sequence] = cpBufLength;
						}
					}			
					displayState = DISP_GATE;
				}
				else {
					if (!getGate(sequence, stepPressed)) {// clicked inactive, so turn gate on
						attributes[sequence][stepPressed] |= ATT_MSK_GATE;
						attributes[sequence][stepPressed] &= ~ATT_MSK_GATEP;
						displayProb = -1;
					}
					else {
						if (!getGateP(sequence, stepPressed)) {// clicked active, but not in prob mode
							displayProb = stepPressed;
							attributes[sequence][stepPressed] |= ATT_MSK_GATEP;
						}
						else {// clicked active, and in prob mode
							if (displayProb != stepPressed)// coming from elsewhere, so don't change any states, just show its prob
								displayProb = stepPressed;
							else {// coming from current step, so turn off
								attributes[sequence][stepPressed] &= ~(ATT_MSK_GATEP | ATT_MSK_GATE);
								displayProb = -1;
							}
						}
					}
				}
			}
			else {// editing song
				row = stepPressed / 16;
				if (row == 3) {
					col = stepPressed % 16;
					if (displayState == DISP_LENGTH) {
						phrases = col + 1;
						if (phrases > 16) phrases = 16;
						if (phrases < 1 ) phrases = 1;
						if (phraseIndexEdit >= phrases) phraseIndexEdit = phrases - 1;
					}
					else if (displayState == DISP_MODES) {
						if (col >= 11 && col <= 15)
							runModeSong = col - 11;
					}
					else {
						if (!running) {
							phraseIndexEdit = stepPressed - 48;
							if (phraseIndexEdit >= phrases)
								phraseIndexEdit = phrases - 1;
						}
					}
				}
			}
		}
		
		
		//********** Clock and reset **********
		
		if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			if (running && clockIgnoreOnReset == 0l) {
				if (editingSequence) {
					moveIndexRunMode(&stepIndexRun, lengths[sequence], runModeSeq, &stepIndexRunHistory);
				}
				else {
					if (moveIndexRunMode(&stepIndexPhraseRun, lengths[phrase[phraseIndexRun]], runModeSeq, &stepIndexPhraseRunHistory)) {
						moveIndexRunMode(&phraseIndexRun, phrases, runModeSong, &phraseIndexRunHistory);
					}
				}
			}
		}	
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			sequence = 0;
			stepIndexRun = (runModeSeq == MODE_REV ? lengths[sequence] - 1 : 0);
			phraseIndexEdit = 0;
			phraseIndexRun = 0;
			stepIndexPhraseRun = 0;
			resetLight = 1.0f;
			displayState = DISP_GATE;
			clockTrigger.reset();
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
		}
		else
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime();
	
		
		//********** Outputs and lights **********
				
		// Gate outputs
		for (int i = 0; i < 4; i += stepConfig) {
			if (running) {
				if (editingSequence) {// editing sequence while running
					bool gateRandomEnable = randomUniform() < ((float)(getGatePVal(sequence, i * 16 + stepIndexRun)))/100.0f;// randomUniform is [0.0, 1.0), see include/util/common.hpp
					gateRandomEnable |= !getGateP(sequence, i * 16 + stepIndexRun);
					bool gateOut = clockTrigger.isHigh() && getGate(sequence, i * 16 + stepIndexRun) && gateRandomEnable;
					outputs[GATE_OUTPUTS + i + (stepConfig - 1)].value = gateOut ? 10.0f : 0.0f;
				}
				else {// editing song while running
					bool gateRandomEnable = randomUniform() < ((float)(getGatePVal(phrase[phraseIndexRun], i * 16 + stepIndexPhraseRun)))/100.0f;// randomUniform is [0.0, 1.0), see include/util/common.hpp
					gateRandomEnable |= !getGateP(phrase[phraseIndexRun], i * 16 + stepIndexPhraseRun);
					bool gateOut = clockTrigger.isHigh() && getGate(phrase[phraseIndexRun], i * 16 + stepIndexPhraseRun) && gateRandomEnable;
					outputs[GATE_OUTPUTS + i + (stepConfig - 1)].value = gateOut ? 10.0f : 0.0f;
				}
			}
			else {// not running (no gates, no need to hear anything)
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
			if (editingSequence) {
				if (displayState == DISP_LENGTH) {
					if (col < (lengths[sequence] - 1))
						setGreenRed(STEP_LIGHTS + i * 2, 0.1f, 0.0f);
					else if (col == (lengths[sequence] - 1))
						setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
					else 
						setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
				}
				else if (displayState == DISP_ROW_SEL) {
					if ((i / 16) == rowToLight)
						setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
					else
						setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
				}
				else {
					float stepHereOffset =  ((stepIndexRun == col) && running) ? 0.5f : 0.0f;
					if (getGate(sequence, i)) {
						if (i == displayProb && getGateP(sequence, i)) 
							setGreenRed(STEP_LIGHTS + i * 2, 0.4f, 1.0f - stepHereOffset);
						else
							setGreenRed(STEP_LIGHTS + i * 2, 1.0f - stepHereOffset, getGateP(sequence, i) ? (1.0f - stepHereOffset) : 0.0f);
					}
					else {
						setGreenRed(STEP_LIGHTS + i * 2, stepHereOffset / 5.0f, 0.0f);
					}				
				}
			}
			else {// editing Song
				if (displayState == DISP_LENGTH) {
					row = i / 16;
					col = i % 16;
					if (row == 3 && col < (phrases - 1))
						setGreenRed(STEP_LIGHTS + i * 2, 0.1f, 0.0f);
					else if (row == 3 && col == (phrases - 1))
						setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
					else 
						setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
				}
				else {
					// Run cursor (green)
					float green;
					if (running) 
						green = (i == (phraseIndexRun + 48)) ? 1.0f : 0.0f;
					else
						green = (i == (phraseIndexEdit + 48)) ? 1.0f : 0.0f;
					green += ((running && (col == stepIndexPhraseRun) && i != (phraseIndexEdit + 48)) ? 0.1f : 0.0f);
					lights[STEP_LIGHTS + i * 2].value = clamp(green, 0.0f, 1.0f);
					// Edit cursor (red)
					lights[STEP_LIGHTS + i * 2 + 1].value = (/*i == (phraseIndexEdit + 48) ? 1.0f :*/ 0.0f);					
				}				
			}
		}
		
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
		char displayStr[4];
		std::string modeLabels[5]={"FWD","REV","PPG","BRN","RND"};
		
		SequenceDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		void runModeToStr(int num) {
			if (num >= 0 && num < 5)
				snprintf(displayStr, 4, "%s", modeLabels[num].c_str());
		}

		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box);
			nvgFontFaceId(vg, font->handle);
			//nvgTextLetterSpacing(vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			nvgText(vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(vg, textColor);				
			if (module->displayState == GateSeq64::DISP_LENGTH)
				if (module->isEditingSequence())
					snprintf(displayStr, 4, "L%2u", module->lengths[module->sequence]);
				else
					snprintf(displayStr, 4, "L%2u", module->phrases);
			else if (module->displayState == GateSeq64::DISP_MODES) {
				if (module->isEditingSequence())
					runModeToStr(module->runModeSeq);
				else
					runModeToStr(module->runModeSong);
			}
			else {
				int dispVal = 0;
				if (module->isEditingSequence())
					dispVal = module->sequence;
				else {
					if (module->running)
						dispVal = module->phrase[module->phraseIndexRun];
					else 
						dispVal = module->phrase[module->phraseIndexEdit];
				}
				snprintf(displayStr, 4, " %2u", (unsigned)(dispVal) + 1 );
			}
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
			
			if (module->displayProb != -1) {
				int prob = module->getGatePVal(module->sequence, module->displayProb);
				if ( prob>= 100)
					snprintf(displayStr, 3, "1*");
				else if (prob >= 1)
					snprintf(displayStr, 3, "%02d", prob);
				else
					snprintf(displayStr, 3, " 0");
			}
			else
				snprintf(displayStr, 3, "--");
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

		
		
		// ****** Top portion (2 switches and LED button array ******
		
		static const int colRuler0 = 20;
		static const int rowRuler0 = 36;
		static const int colRulerSteps = 60;
		static const int spacingSteps = 20;
		static const int spacingSteps4 = 4;
		static const int spacingRows = 40;
		
		
		// Config switch (3 position)
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(colRuler0 + hOffsetCKSS, rowRuler0 + 43 + vOffsetCKSSThree), module, GateSeq64::CONFIG_PARAM, 0.0f, 2.0f, 0.0f));// 0.0f is top position
		// Seq/Song selector
		addParam(ParamWidget::create<CKSS>(Vec(colRuler0 + hOffsetCKSS, rowRuler0 + 100 + vOffsetCKSS), module, GateSeq64::EDIT_PARAM, 0.0f, 1.0f, 1.0f));
		
		
		for (int y = 0; y < 4; y++) {
			int posX = colRulerSteps;
			for (int x = 0; x < 16; x++) {
				addParam(ParamWidget::create<LEDButton>(Vec(posX, rowRuler0 + 8 + y * spacingRows - 4.4f), module, GateSeq64::STEP_PARAMS + y * 16 + x, 0.0f, 1.0f, 0.0f));
				addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(posX + 4.4f, rowRuler0 + 8 + y * spacingRows), module, GateSeq64::STEP_LIGHTS + (y * 16 + x) * 2));
				posX += spacingSteps;
				if ((x + 1) % 4 == 0)
					posX += spacingSteps4;
			}
		}
			
		
		
		// ****** 5x3 Main bottom half Control section ******
		
		static int colRulerC0 = 25;
		static int colRulerSpacing = 68;
		static int colRulerC1 = colRulerC0 + colRulerSpacing;
		static int colRulerC2 = colRulerC1 + colRulerSpacing;
		static int colRulerC3 = colRulerC2 + colRulerSpacing;
		static int colRulerC4 = colRulerC3 + colRulerSpacing;
		static int rowRulerC0 = 217;
		static int rowRulerSpacing = 54;
		static int rowRulerC1 = rowRulerC0 + rowRulerSpacing;
		static int rowRulerC2 = rowRulerC1 + rowRulerSpacing;
		
		
		// Clock input
		addInput(Port::create<PJ301MPortS>(Vec(colRulerC0, rowRulerC0), Port::INPUT, module, GateSeq64::CLOCK_INPUT));
		// Reset CV
		addInput(Port::create<PJ301MPortS>(Vec(colRulerC0, rowRulerC1), Port::INPUT, module, GateSeq64::RESET_INPUT));
		// Seq CV
		addInput(Port::create<PJ301MPortS>(Vec(colRulerC0, rowRulerC2), Port::INPUT, module, GateSeq64::SEQCV_INPUT));
		
		
		// Run LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(colRulerC1 + offsetLEDbezel, rowRulerC0 + offsetLEDbezel), module, GateSeq64::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRulerC1 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC0 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RUN_LIGHT));
		// Reset LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(colRulerC1 + offsetLEDbezel, rowRulerC1 + offsetLEDbezel), module, GateSeq64::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRulerC2 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC1 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RESET_LIGHT));
		// Run CV
		addInput(Port::create<PJ301MPortS>(Vec(colRulerC1, rowRulerC2), Port::INPUT, module, GateSeq64::RUNCV_INPUT));

		
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(colRulerC2 - 15, rowRulerC0 + vOffsetDisplay);
		displaySequence->box.size = Vec(55, 30);// 3 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Sequence knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(colRulerC2 + 1 + offsetDavies1900, rowRulerC0 + 50 + offsetDavies1900), module, GateSeq64::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f));		


		// Modes button and light
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC3 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::MODES_PARAM, 0.0f, 1.0f, 0.0f));
		// Copy/paste buttons
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC3 - 10, rowRulerC1 + offsetTL1105), module, GateSeq64::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC3 + 20, rowRulerC1 + offsetTL1105), module, GateSeq64::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		
		
		// Probability display
		ProbDisplayWidget *probDisplay = new ProbDisplayWidget();
		probDisplay->box.pos = Vec(colRulerC4 - 7, rowRulerC0 + vOffsetDisplay);
		probDisplay->box.size = Vec(40, 30);// 3 characters
		probDisplay->module = module;
		addChild(probDisplay);
		// Probability knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(colRulerC4 + 1 + offsetDavies1900, rowRulerC0 + 50 + offsetDavies1900), module, GateSeq64::PROB_KNOB_PARAM, -INFINITY, INFINITY, 0.0f));	

		
		// Outputs
		for (int iSides = 0; iSides < 4; iSides++)
			addOutput(Port::create<PJ301MPortS>(Vec(356, 210 + iSides * 40), Port::OUTPUT, module, GateSeq64::GATE_OUTPUTS + iSides));
		
	}
};

Model *modelGateSeq64 = Model::create<GateSeq64, GateSeq64Widget>("Impromptu Modular", "Gate-Seq-64", "Gate-Seq-64", SEQUENCER_TAG);
