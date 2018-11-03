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
#include "PhraseSeqUtil.hpp"


struct GateSeq64 : Module {
	enum ParamIds {
		ENUMS(STEP_PARAMS, 64),
		MODES_PARAM,
		RUN_PARAM,
		CONFIG_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		PROB_PARAM,
		EDIT_PARAM,
		SEQUENCE_PARAM,
		CPMODE_PARAM,
		// -- 0.6.9 ^^
		GMODELEFT_PARAM,// no longer used
		GMODERIGHT_PARAM,// no longer used
		// -- 0.6.11 ^^
		ENUMS(GMODE_PARAMS, 8),
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		RUNCV_INPUT,
		SEQCV_INPUT,
		// -- 0.6.7 ^^
		WRITE_INPUT,
		GATE_INPUT,
		PROB_INPUT,
		WRITE1_INPUT,
		WRITE0_INPUT,
		// -- 0.6.10 ^^
		STEPL_INPUT,
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
		ENUMS(GMODE_LIGHTS, 8 * 2),// room for GreenRed
		RES_LIGHT,
		NUM_LIGHTS
	};
	
	// Constants
	enum DisplayStateIds {DISP_GATE, DISP_LENGTH, DISP_MODES};
	enum AttributeBitMasksGS {ATT_MSK_PROB = 0xFF, ATT_MSK_GATEP = 0x100, ATT_MSK_GATE = 0x200};
	static const int ATT_MSK_GATEMODE = 0x1C00;// 3 bits
	static const int gateModeShift = 10;
	//										1/4		DUO			D2			TR1		TR2		TR3 		TR23	   TRI
	const uint32_t advGateHitMaskGS[8] = {0x00003F, 0x03F03F, 0x03F000, 0x00000F, 0x000F00, 0x0F0000, 0x0F0F00, 0x0F0F0F};
	static const int blinkNumInit = 15;
	static constexpr float CONFIG_PARAM_INIT_VALUE = 0.0f;// so that module constructor is coherent with widget initialization, since module created before widget

	// Need to save
	int panelTheme = 0;
	int expansion = 0;
	bool autoseq;
	int pulsesPerStep;// 1 means normal gate mode, alt choices are 4, 6, 12, 24 PPS (Pulses per step)
	bool running;
	int runModeSeq[16];
	int runModeSong;
	int sequence;
	int lengths[16];// values are 1 to 16
	int phrase[64];// This is the song (series of phases; a phrase is a patten number)
	int phrases;//1 to 64
	int attributes[16][64];
	bool resetOnRun;

	// No need to save
	int displayState;
	int stepIndexEdit;
	int stepIndexRun[4];
	int phraseIndexEdit;	
	int phraseIndexRun;
	int stepIndexRunHistory;// no need to initialize
	int phraseIndexRunHistory;// no need to initialize
	int attribOrPhraseCPbuffer[64];
	int lengthCPbuffer;
	int modeCPbuffer;
	int countCP;// number of steps to paste (in case CPMODE_PARAM changes between copy and paste)
	int startCP;
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	long clockIgnoreOnReset;
	long displayProbInfo;// downward step counter for displayProb feedback
	int gateCode[4];
	long revertDisplay;
	long editingPpqn;// 0 when no info, positive downward step counter timer when editing ppqn
	int ppqnCount;
	long blinkCount;// positive upward counter, reset to 0 when max reached
	int blinkNum;// number of blink cycles to do, downward counter
	int stepConfig;
	long editingPhraseSongRunning;// downward step counter


	int stepConfigSync = 0;// 0 means no sync requested, 1 means soft sync (no reset lengths), 2 means hard (reset lengths)
	unsigned int lightRefreshCounter = 0;
	float resetLight = 0.0f;
	int sequenceKnob = 0;
	SchmittTrigger modesTrigger;
	SchmittTrigger stepTriggers[64];
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger writeTrigger;
	SchmittTrigger write0Trigger;
	SchmittTrigger write1Trigger;
	SchmittTrigger stepLTrigger;
	SchmittTrigger gModeTriggers[8];
	SchmittTrigger probTrigger;
	BooleanTrigger editingSequenceTrigger;
	HoldDetect modeHoldDetect;
	int lengthsBuffer[16];// buffer from Json for thread safety

	
	inline bool isEditingSequence(void) {return params[EDIT_PARAM].value > 0.5f;}
	inline int getStepConfig(float paramValue) {// 1 = 4x16 = 0.0f,  2 = 2x32 = 1.0f,  4 = 1x64 = 2.0f
		if (paramValue < 0.5f) return 1;
		else if (paramValue < 1.5f) return 2;
		return 4;
	}
	
	inline void initAttrib(int seq, int step) {attributes[seq][step] = 50;}
	inline bool getGateA(int attribute) {return (attribute & ATT_MSK_GATE) != 0;}
	inline bool getGate(int seq, int step) {return getGateA(attributes[seq][step]);}
	inline bool getGatePa(int attribute) {return (attribute & ATT_MSK_GATEP) != 0;}
	inline bool getGateP(int seq, int step) {return getGatePa(attributes[seq][step]);}
	inline int getGatePValA(int attribute) {return attribute & ATT_MSK_PROB;}
	inline int getGatePVal(int seq, int step) {return getGatePValA(attributes[seq][step]);}
	inline int getGateAMode(int attribute) {return (attribute & ATT_MSK_GATEMODE) >> gateModeShift;}
	inline int getGateMode(int seq, int step) {return getGateAMode(attributes[seq][step]);}

	inline void setGate(int seq, int step, bool gateState) {attributes[seq][step] &= ~ATT_MSK_GATE; if (gateState) attributes[seq][step] |= ATT_MSK_GATE;}
	inline void setGateP(int seq, int step, bool gatePState) {attributes[seq][step] &= ~ATT_MSK_GATEP; if (gatePState) attributes[seq][step] |= ATT_MSK_GATEP;}
	inline void setGatePVal(int seq, int step, int pVal) {attributes[seq][step] &= ~ATT_MSK_PROB; attributes[seq][step] |= (pVal & ATT_MSK_PROB);}
	inline void setGateMode(int seq, int step, int gateMode) {attributes[seq][step] &= ~ATT_MSK_GATEMODE; attributes[seq][step] |= (gateMode << gateModeShift);}
	inline void toggleGate(int seq, int step) {attributes[seq][step] ^= ATT_MSK_GATE;}

	inline int getAdvGateGS(int ppqnCount, int pulsesPerStep, int gateMode) { 
		uint32_t shiftAmt = ppqnCount * (24 / pulsesPerStep);
		return (int)((advGateHitMaskGS[gateMode] >> shiftAmt) & (uint32_t)0x1);
	}	
	inline int calcGateCode(int attribute, int ppqnCount, int pulsesPerStep) {
		// -1 = gate off for whole step, 0 = gate off for current ppqn, 1 = gate on, 2 = clock high
		if (ppqnCount == 0 && getGatePa(attribute) && !(randomUniform() < ((float)(getGatePValA(attribute))/100.0f)))// randomUniform is [0.0, 1.0), see include/util/common.hpp
			return -1;
		if (!getGateA(attribute))
			return 0;
		if (pulsesPerStep == 1)
			return 2;// clock high
		return getAdvGateGS(ppqnCount, pulsesPerStep, getGateAMode(attribute));
	}		
	inline bool calcGate(int gateCode, SchmittTrigger clockTrigger) {
		if (gateCode < 2) 
			return gateCode == 1;
		return clockTrigger.isHigh();
	}		
	inline void fillStepIndexRunVector(int runMode, int len) {
		if (runMode != MODE_RN2) {
			stepIndexRun[1] = stepIndexRun[0];
			stepIndexRun[2] = stepIndexRun[0];
			stepIndexRun[3] = stepIndexRun[0];
		}
		else {
			stepIndexRun[1] = randomu32() % len;
			stepIndexRun[2] = randomu32() % len;
			stepIndexRun[3] = randomu32() % len;
		}
	}
		
		
	GateSeq64() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		for (int i = 0; i < 16; i++)
			lengthsBuffer[i] = 16;
		onReset();
	}

	
	void onReset() override {
		stepConfig = getStepConfig(CONFIG_PARAM_INIT_VALUE);
		autoseq = false;
		pulsesPerStep = 1;
		running = false;
		runModeSong = MODE_FWD;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		sequence = 0;
		phrases = 4;
		for (int i = 0; i < 16; i++) {
			for (int s = 0; s < 64; s++) {
				initAttrib(i, s);
			}
			runModeSeq[i] = MODE_FWD;
			lengths[i] = 16 * stepConfig;
		}
		for (int i = 0; i < 64; i++) {
			phrase[i] = 0;
			attribOrPhraseCPbuffer[i] = 50;
		}
		initRun(true);
		lengthCPbuffer = 64;
		modeCPbuffer = MODE_FWD;
		countCP = 64;
		startCP = 0;
		displayState = DISP_GATE;
		displayProbInfo = 0l;
		infoCopyPaste = 0l;
		revertDisplay = 0l;
		resetOnRun = false;
		editingPpqn = 0l;
		blinkCount = 0l;
		blinkNum = blinkNumInit;
		editingPhraseSongRunning = 0l;
	}

	
	void onRandomize() override {
		stepConfig = getStepConfig(params[CONFIG_PARAM].value);
		runModeSong = randomu32() % 5;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		sequence = randomu32() % 16;
		phrases = 1 + (randomu32() % 64);
		for (int i = 0; i < 16; i++) {
			for (int s = 0; s < 64; s++) {
				attributes[i][s] = (randomu32() % 101) | (randomu32() & (ATT_MSK_GATEP | ATT_MSK_GATE | ATT_MSK_GATEMODE));
			}
			runModeSeq[i] = randomu32() % NUM_MODES;
			lengths[i] = 1 + (randomu32() % (16 * stepConfig));
		}
		for (int i = 0; i < 64; i++)
			phrase[i] = randomu32() % 16;
		initRun(true);
	}


	void initRun(bool hard) {// run button activated or run edge in run input jack
		if (hard) {
			phraseIndexRun = (runModeSong == MODE_REV ? phrases - 1 : 0);
			phraseIndexRunHistory = 0;
		}
		int seq = (isEditingSequence() ? sequence : phrase[phraseIndexRun]);
		if (hard) {	
			stepIndexRun[0] = (runModeSeq[seq] == MODE_REV ? lengths[seq] - 1 : 0);
			fillStepIndexRunVector(runModeSeq[seq], lengths[seq]);
			stepIndexRunHistory = 0;
		}
		ppqnCount = 0;
		for (int i = 0; i < 4; i += stepConfig)
			gateCode[i] = calcGateCode(attributes[seq][(i * 16) + stepIndexRun[i]], 0, pulsesPerStep);
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}
	
	
	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// expansion
		json_object_set_new(rootJ, "expansion", json_integer(expansion));

		// autoseq
		json_object_set_new(rootJ, "autoseq", json_boolean(autoseq));
		
		// pulsesPerStep
		json_object_set_new(rootJ, "pulsesPerStep", json_integer(pulsesPerStep));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// runModeSeq
		json_t *runModeSeqJ = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(runModeSeqJ, i, json_integer(runModeSeq[i]));
		json_object_set_new(rootJ, "runModeSeq3", runModeSeqJ);

		// runModeSong
		json_object_set_new(rootJ, "runModeSong3", json_integer(runModeSong));

		// sequence
		json_object_set_new(rootJ, "sequence", json_integer(sequence));

		// lengths
		json_t *lengthsJ = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(lengthsJ, i, json_integer(lengths[i]));
		json_object_set_new(rootJ, "lengths", lengthsJ);
	
		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 64; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, "phrase2", phraseJ);// "2" appended so no break patches

		// phrases
		json_object_set_new(rootJ, "phrases", json_integer(phrases));

		// attributes
		json_t *attributesJ = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 64; s++) {
				json_array_insert_new(attributesJ, s + (i * 64), json_integer(attributes[i][s]));
			}
		json_object_set_new(rootJ, "attributes", attributesJ);
		
		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));
	
		// phraseIndexEdit
		json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

		return rootJ;
	}

	
	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);
		
		// expansion
		json_t *expansionJ = json_object_get(rootJ, "expansion");
		if (expansionJ)
			expansion = json_integer_value(expansionJ);

		// autoseq
		json_t *autoseqJ = json_object_get(rootJ, "autoseq");
		if (autoseqJ)
			autoseq = json_is_true(autoseqJ);

		// pulsesPerStep
		json_t *pulsesPerStepJ = json_object_get(rootJ, "pulsesPerStep");
		if (pulsesPerStepJ)
			pulsesPerStep = json_integer_value(pulsesPerStepJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
		
		// runModeSeq
		json_t *runModeSeqJ = json_object_get(rootJ, "runModeSeq3");
		if (runModeSeqJ) {
			for (int i = 0; i < 16; i++)
			{
				json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
				if (runModeSeqArrayJ)
					runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
			}			
		}		
		else {// legacy
			runModeSeqJ = json_object_get(rootJ, "runModeSeq2");
			if (runModeSeqJ) {
				for (int i = 0; i < 16; i++)
				{
					json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
					if (runModeSeqArrayJ) {
						runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
						if (runModeSeq[i] >= MODE_PEN)// this mode was not present in version runModeSeq2
							runModeSeq[i]++;
					}
				}			
			}		
		}
		
		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, "runModeSong3");
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
		else {// legacy
			runModeSongJ = json_object_get(rootJ, "runModeSong");
			if (runModeSongJ) {
				runModeSong = json_integer_value(runModeSongJ);
				if (runModeSong >= MODE_PEN)// this mode was not present in original version
					runModeSong++;
			}
		}
		
		// sequence
		json_t *sequenceJ = json_object_get(rootJ, "sequence");
		if (sequenceJ)
			sequence = json_integer_value(sequenceJ);
		
		// lengths
		json_t *lengthsJ = json_object_get(rootJ, "lengths");
		if (lengthsJ) {
			for (int i = 0; i < 16; i++)
			{
				json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
				if (lengthsArrayJ)
					lengthsBuffer[i] = json_integer_value(lengthsArrayJ);
			}			
		}
		
		// phrase
		json_t *phraseJ = json_object_get(rootJ, "phrase2");// "2" appended so no break patches
		if (phraseJ) {
			for (int i = 0; i < 64; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
		}
		else {// legacy
			phraseJ = json_object_get(rootJ, "phrase");
			if (phraseJ) {
				for (int i = 0; i < 16; i++)
				{
					json_t *phraseArrayJ = json_array_get(phraseJ, i);
					if (phraseArrayJ)
						phrase[i] = json_integer_value(phraseArrayJ);
				}
				for (int i = 16; i < 64; i++)
					phrase[i] = 0;
			}
		}
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, "phrases");
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
	
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
		
		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// stepIndexEdit
		json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
		if (stepIndexEditJ)
			stepIndexEdit = json_integer_value(stepIndexEditJ);
		
		// phraseIndexEdit
		json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
		if (phraseIndexEditJ)
			phraseIndexEdit = json_integer_value(phraseIndexEditJ);
		
		stepConfigSync = 1;// signal a sync from fromJson so that step will get lengths from lengthsBuffer
	}

	
	void step() override {
		static const float copyPasteInfoTime = 0.5f;// seconds
		static const float displayProbInfoTime = 3.0f;// seconds
		static const float revertDisplayTime = 0.5f;// seconds
		static const float holdDetectTime = 2.0f;// seconds
		static const float editingPhraseSongRunningTime = 4.0f;// seconds
		static const float editingPpqnTime = 3.5f;// seconds
		float sampleRate = engineGetSampleRate();
		

		
		//********** Buttons, knobs, switches and inputs **********

		// Edit mode		
		bool editingSequence = isEditingSequence();// true = editing sequence, false = editing song
		
		// Run state button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUNCV_INPUT].value)) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running)
				initRun(resetOnRun);
			else
				blinkNum = blinkNumInit;
			displayState = DISP_GATE;
		}
		
		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {
			
			// Edit mode blink when change
			if (editingSequenceTrigger.process(editingSequence))
				blinkNum = blinkNumInit;

			// Config switch
			if (stepConfigSync != 0) {
				stepConfig = getStepConfig(params[CONFIG_PARAM].value);
				if (stepConfigSync == 1) {// sync from fromJson, so read lengths from lengthsBuffer
					for (int i = 0; i < 16; i++)
						lengths[i] = lengthsBuffer[i];
				}
				else if (stepConfigSync == 2) {// sync from a real mouse drag event on the switch itself, so init lengths
					for (int i = 0; i < 16; i++)
						lengths[i] = 16 * stepConfig;
				}
				initRun(true);	
				stepConfigSync = 0;
			}
			
			// Seq CV input
			if (inputs[SEQCV_INPUT].active) {
				sequence = (int) clamp( round(inputs[SEQCV_INPUT].value * 15.0f / 10.0f), 0.0f, 15.0f );
			}
			
			// Copy button
			if (copyTrigger.process(params[COPY_PARAM].value)) {
				startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
				countCP = 64;
				if (params[CPMODE_PARAM].value > 1.5f)// ALL
					startCP = 0;			
				else if (params[CPMODE_PARAM].value < 0.5f)// 4
					countCP = min(4, 64 - startCP);
				else// 8
					countCP = min(8, 64 - startCP);
				if (editingSequence) {	
					for (int i = 0, s = startCP; i < countCP; i++, s++)
						attribOrPhraseCPbuffer[i] = attributes[sequence][s];
					lengthCPbuffer = lengths[sequence];
					modeCPbuffer = runModeSeq[sequence];		
				}
				else {
					for (int i = 0, p = startCP; i < countCP; i++, p++)
						attribOrPhraseCPbuffer[i] = phrase[p];
					lengthCPbuffer = -1;// so that a cross paste can be detected
				}
				infoCopyPaste = (long) (copyPasteInfoTime * sampleRate / displayRefreshStepSkips);
				displayState = DISP_GATE;
				blinkNum = blinkNumInit;
			}
			// Paste button
			if (pasteTrigger.process(params[PASTE_PARAM].value)) {
				infoCopyPaste = (long) (-1 * copyPasteInfoTime * sampleRate / displayRefreshStepSkips);
				startCP = 0;
				if (countCP <= 8) {
					startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
					countCP = min(countCP, 64 - startCP);
				}
				// else nothing to do for ALL
					
				if (editingSequence) {
					if (lengthCPbuffer >= 0) {// non-crossed paste (seq vs song)
						for (int i = 0, s = startCP; i < countCP; i++, s++)
							attributes[sequence][s] = attribOrPhraseCPbuffer[i];
						if (params[CPMODE_PARAM].value > 1.5f) {// all
							lengths[sequence] = lengthCPbuffer;
							if (lengths[sequence] > 16 * stepConfig)
								lengths[sequence] = 16 * stepConfig;
							runModeSeq[sequence] = modeCPbuffer;
						}
					}
					else {// crossed paste to seq (seq vs song)
						if (params[CPMODE_PARAM].value > 1.5f) { // ALL (init steps)
							for (int s = 0; s < 64; s++)
								initAttrib(sequence, s);
						}
						else if (params[CPMODE_PARAM].value < 0.5f) {// 4 (randomize gates)
							for (int s = 0; s < 64; s++)
								if ( (randomu32() & 0x1) != 0)
									toggleGate(sequence, s);
						}
						else {// 8 (randomize probs)
							for (int s = 0; s < 64; s++) {
								setGateP(sequence, s, (randomu32() & 0x1) != 0);
								setGatePVal(sequence, s, randomu32() % 101);
							}
						}
						startCP = 0;
						countCP = 64;
						infoCopyPaste *= 2l;
					}
				}
				else {// song
					if (lengthCPbuffer < 0) {// non-crossed paste (seq vs song)
						for (int i = 0, p = startCP; i < countCP; i++, p++)
							phrase[p] = attribOrPhraseCPbuffer[i] & 0xF;
					}
					else {// crossed paste to song (seq vs song)
						if (params[CPMODE_PARAM].value > 1.5f) { // ALL (init phrases)
							for (int p = 0; p < 64; p++)
								phrase[p] = 0;
						}
						else if (params[CPMODE_PARAM].value < 0.5f) {// 4 (phrases increase from 1 to 64)
							for (int p = 0; p < 64; p++)
								phrase[p] = p;						
						}
						else {// 8 (randomize phrases)
							for (int p = 0; p < 64; p++)
								phrase[p] = randomu32() % 64;
						}
						startCP = 0;
						countCP = 64;
						infoCopyPaste *= 2l;
					}
				}
				displayState = DISP_GATE;
				blinkNum = blinkNumInit;
			}
			
			// Write CV inputs 
			bool writeTrig = writeTrigger.process(inputs[WRITE_INPUT].value);
			bool write0Trig = write0Trigger.process(inputs[WRITE0_INPUT].value);
			bool write1Trig = write1Trigger.process(inputs[WRITE1_INPUT].value);
			if (writeTrig || write0Trig || write1Trig) {
				if (editingSequence) {
					blinkNum = blinkNumInit;
					if (writeTrig) {// higher priority than write0 and write1
						if (inputs[PROB_INPUT].active) {
							setGatePVal(sequence, stepIndexEdit, clamp( (int)round(inputs[PROB_INPUT].value * 10.0f), 0, 100) );
							setGateP(sequence, stepIndexEdit, true);
						}
						else{
							//setGatePVal(sequence, stepIndexEdit, 50);
							setGateP(sequence, stepIndexEdit, false);
						}
						if (inputs[GATE_INPUT].active)
							setGate(sequence, stepIndexEdit, inputs[GATE_INPUT].value >= 1.0f);
					}
					else {// write1 or write0			
						setGate(sequence, stepIndexEdit, write1Trig);
					}
					// Autostep (after grab all active inputs)
					stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 64);
					if (stepIndexEdit == 0 && autoseq)
						sequence = moveIndex(sequence, sequence + 1, 16);			
				}
			}

			// Step left CV input
			if (stepLTrigger.process(inputs[STEPL_INPUT].value)) {
				if (editingSequence) {
					blinkNum = blinkNumInit;
					stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit - 1, 64);					
				}
			}

			// Step LED button presses
			int stepPressed = -1;
			for (int i = 0; i < 64; i++) {
				if (stepTriggers[i].process(params[STEP_PARAMS + i].value))
					stepPressed = i;
			}		
			if (stepPressed != -1) {
				if (editingSequence) {
					if (displayState == DISP_LENGTH) {
						lengths[sequence] = stepPressed % (16 * stepConfig) + 1;
						revertDisplay = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODES) {
					}
					else {
						if (!getGate(sequence, stepPressed)) {// clicked inactive, so turn gate on
							setGate(sequence, stepPressed, true);
							if (getGateP(sequence, stepPressed))
								displayProbInfo = (long) (displayProbInfoTime * sampleRate / displayRefreshStepSkips);
							else
								displayProbInfo = 0l;
						}
						else {// clicked active
							if (stepIndexEdit == stepPressed && blinkNum != 0) {// only if coming from current step, turn off
								setGate(sequence, stepPressed, false);
								displayProbInfo = 0l;
							}
							else {
								if (getGateP(sequence, stepPressed))
									displayProbInfo = (long) (displayProbInfoTime * sampleRate / displayRefreshStepSkips);
								else
									displayProbInfo = 0l;
							}
						}
						stepIndexEdit = stepPressed;
					}
					blinkNum = blinkNumInit;
				}
				else {// editing song
					if (displayState == DISP_LENGTH) {
						phrases = stepPressed + 1;
						if (phrases > 64) phrases = 64;
						if (phrases < 1 ) phrases = 1;
						revertDisplay = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODES) {
					}
					else {
						phraseIndexEdit = stepPressed;
						if (running)
							editingPhraseSongRunning = (long) (editingPhraseSongRunningTime * sampleRate / displayRefreshStepSkips);
						else
							phraseIndexRun = stepPressed;
					}
				}
			}

			// Mode/Length button
			if (modesTrigger.process(params[MODES_PARAM].value)) {
				blinkNum = blinkNumInit;
				if (editingPpqn != 0l)
					editingPpqn = 0l;			
				if (displayState == DISP_GATE)
					displayState = DISP_LENGTH;
				else if (displayState == DISP_LENGTH)
					displayState = DISP_MODES;
				else
					displayState = DISP_GATE;
				modeHoldDetect.start((long) (holdDetectTime * sampleRate / displayRefreshStepSkips));
			}

			// Prob button
			if (probTrigger.process(params[PROB_PARAM].value)) {
				blinkNum = blinkNumInit;
				if (editingSequence && getGate(sequence, stepIndexEdit)) {
					if (getGateP(sequence, stepIndexEdit)) {
						displayProbInfo = 0l;
						setGateP(sequence, stepIndexEdit, false);
					}
					else {
						displayProbInfo = (long) (displayProbInfoTime * sampleRate / displayRefreshStepSkips);
						setGateP(sequence, stepIndexEdit, true);
					}
				}
			}
			
			// GateMode buttons
			for (int i = 0; i < 8; i++) {
				if (gModeTriggers[i].process(params[GMODE_PARAMS + i].value)) {
					blinkNum = blinkNumInit;
					if (editingSequence && getGate(sequence, stepIndexEdit)) {
						if ( (pulsesPerStep < 2) || (pulsesPerStep == 4 && i > 2) || (pulsesPerStep == 6 && i <= 2) ) // pps requirement not met
							editingPpqn = (long) (editingPpqnTime * sampleRate / displayRefreshStepSkips);
						else
							setGateMode(sequence, stepIndexEdit, i);
					}
				}
			}

			
			// Sequence knob (Main knob)
			float seqParamValue = params[SEQUENCE_PARAM].value;
			int newSequenceKnob = (int)roundf(seqParamValue * 7.0f);
			if (seqParamValue == 0.0f)// true when constructor or fromJson() occured
				sequenceKnob = newSequenceKnob;
			int deltaKnob = newSequenceKnob - sequenceKnob;
			if (deltaKnob != 0) {
				if (abs(deltaKnob) <= 3) {// avoid discontinuous step (initialize for example)
					if (displayProbInfo != 0l && editingSequence) {
						blinkNum = blinkNumInit;
						int pval = getGatePVal(sequence, stepIndexEdit);
						pval += deltaKnob * 2;
						if (pval > 100)
							pval = 100;
						if (pval < 0)
							pval = 0;
						setGatePVal(sequence, stepIndexEdit, pval);
						displayProbInfo = (long) (displayProbInfoTime * sampleRate / displayRefreshStepSkips);
					}
					else if (editingPpqn != 0) {
						pulsesPerStep = indexToPps(ppsToIndex(pulsesPerStep) + deltaKnob);// indexToPps() does clamping
						editingPpqn = (long) (editingPpqnTime * sampleRate / displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODES) {
						if (editingSequence) {
							runModeSeq[sequence] += deltaKnob;
							if (runModeSeq[sequence] < 0) runModeSeq[sequence] = 0;
							if (runModeSeq[sequence] >= NUM_MODES) runModeSeq[sequence] = NUM_MODES - 1;
						}
						else {
							runModeSong += deltaKnob;
							if (runModeSong < 0) runModeSong = 0;
							if (runModeSong >= 6) runModeSong = 6 - 1;
						}
					}
					else if (displayState == DISP_LENGTH) {
						if (editingSequence) {
							lengths[sequence] += deltaKnob;
							if (lengths[sequence] > (16 * stepConfig)) 
								lengths[sequence] = (16 * stepConfig);
							if (lengths[sequence] < 1 ) lengths[sequence] = 1;
						}
						else {
							phrases += deltaKnob;
							if (phrases > 64) phrases = 64;
							if (phrases < 1 ) phrases = 1;
						}
					}
					else {
						if (editingSequence) {
							blinkNum = blinkNumInit;
							if (!inputs[SEQCV_INPUT].active) {
								sequence += deltaKnob;
								if (sequence < 0) sequence = 0;
								if (sequence >= 16) sequence = (16 - 1);
							}
						}
						else {
							if (editingPhraseSongRunning > 0l || !running) {
								phrase[phraseIndexEdit] += deltaKnob;
								if (phrase[phraseIndexEdit] < 0) phrase[phraseIndexEdit] = 0;
								if (phrase[phraseIndexEdit] >= 16) phrase[phraseIndexEdit] = (16 - 1);
								if (running)
									editingPhraseSongRunning = (long) (editingPhraseSongRunningTime * sampleRate / displayRefreshStepSkips);
							}
						}	
					}					
				}
				sequenceKnob = newSequenceKnob;
			}		
		
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			if (running && clockIgnoreOnReset == 0l) {
				ppqnCount++;
				if (ppqnCount >= pulsesPerStep)
					ppqnCount = 0;
				
				int newSeq = sequence;// good value when editingSequence, overwrite if not editingSequence
				if (ppqnCount == 0) {
					if (editingSequence) {
						moveIndexRunMode(&stepIndexRun[0], lengths[sequence], runModeSeq[sequence], &stepIndexRunHistory);
					}
					else {
						if (moveIndexRunMode(&stepIndexRun[0], lengths[phrase[phraseIndexRun]], runModeSeq[phrase[phraseIndexRun]], &stepIndexRunHistory)) {
							moveIndexRunMode(&phraseIndexRun, phrases, runModeSong, &phraseIndexRunHistory);
							stepIndexRun[0] = (runModeSeq[phrase[phraseIndexRun]] == MODE_REV ? lengths[phrase[phraseIndexRun]] - 1 : 0);// must always refresh after phraseIndexRun has changed
						}
						newSeq = phrase[phraseIndexRun];
					}
					fillStepIndexRunVector(runModeSeq[newSeq], lengths[newSeq]);
				}
				else {
					if (!editingSequence)
						newSeq = phrase[phraseIndexRun];
				}
				for (int i = 0; i < 4; i += stepConfig) { 
					if (gateCode[i] != -1 || ppqnCount == 0)
						gateCode[i] = calcGateCode(attributes[newSeq][(i * 16) + stepIndexRun[i]], ppqnCount, pulsesPerStep);
				}
			}
		}	
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			initRun(true);// must be after sequence reset
			resetLight = 1.0f;
			displayState = DISP_GATE;
		}
	
		
		//********** Outputs and lights **********
				
		// Gate outputs
		if (running) {
			for (int i = 0; i < 4; i++)
				outputs[GATE_OUTPUTS + i].value = calcGate(gateCode[i], clockTrigger) ? 10.0f : 0.0f;
		}
		else {// not running (no gates, no need to hear anything)
			for (int i = 0; i < 4; i++)
				outputs[GATE_OUTPUTS + i].value = 0.0f;	
		}

		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			// Step LED button lights
			if (infoCopyPaste != 0l) {
				for (int i = 0; i < 64; i++) {
					if (i >= startCP && i < (startCP + countCP))
						setGreenRed(STEP_LIGHTS + i * 2, 0.5f, 0.0f);
					else
						setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
				}
			}
			else {
				int row = -1;
				int col = -1;
				for (int i = 0; i < 64; i++) {
					row = i >> (3 + stepConfig);//i / (16 * stepConfig);// optimized (not equivalent code, but in this case has same effect)
					if (stepConfig == 2 && row == 1) 
						row++;
					col = (((stepConfig - 1) << 4) | 0xF) & i;//i % (16 * stepConfig);// optimized			
					if (editingSequence) {
						if (displayState == DISP_LENGTH) {
							if (col < (lengths[sequence] - 1))
								setGreenRed(STEP_LIGHTS + i * 2, 0.1f, 0.0f);
							else if (col == (lengths[sequence] - 1))
								setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
							else 
								setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
						}
						else {
							float stepHereOffset = ((stepIndexRun[row] == col) && running) ? 0.5f : 1.0f;
							long blinkCountMarker = (long) (0.67f * sampleRate / displayRefreshStepSkips);							
							if (getGate(sequence, i)) {
								bool blinkEnableOn = (displayState != DISP_MODES) && (blinkCount < blinkCountMarker);
								if (getGateP(sequence, i)) {
									if (i == stepIndexEdit)// more orange than yellow
										setGreenRed(STEP_LIGHTS + i * 2, blinkEnableOn ? 1.0f : 0.0f, blinkEnableOn ? 1.0f : 0.0f);
									else// more yellow
										setGreenRed(STEP_LIGHTS + i * 2, stepHereOffset, stepHereOffset);
								}
								else {
									if (i == stepIndexEdit)
										setGreenRed(STEP_LIGHTS + i * 2, blinkEnableOn ? 1.0f : 0.0f, 0.0f);
									else
										setGreenRed(STEP_LIGHTS + i * 2, stepHereOffset, 0.0f);
								}
							}
							else {
								if (i == stepIndexEdit && blinkCount > blinkCountMarker && displayState != DISP_MODES)
									setGreenRed(STEP_LIGHTS + i * 2, 0.05f, 0.0f);
								else
									setGreenRed(STEP_LIGHTS + i * 2, ((stepIndexRun[row] == col) && running) ? 0.1f : 0.0f, 0.0f);
							}
						}
					}
					else {// editing Song
						if (displayState == DISP_LENGTH) {
							col = i & 0xF;//i % 16;// optimized
							if (i < (phrases - 1))
								setGreenRed(STEP_LIGHTS + i * 2, 0.1f, 0.0f);
							else if (i == (phrases - 1))
								setGreenRed(STEP_LIGHTS + i * 2, 1.0f, 0.0f);
							else 
								setGreenRed(STEP_LIGHTS + i * 2, 0.0f, 0.0f);
						}
						else {
							float green = (i == (phraseIndexRun) && running) ? 1.0f : 0.0f;
							float red = (i == (phraseIndexEdit) && ((editingPhraseSongRunning > 0l) || !running)) ? 1.0f : 0.0f;
							green += ((running && (col == stepIndexRun[row]) && i != (phraseIndexEdit)) ? 0.1f : 0.0f);
							setGreenRed(STEP_LIGHTS + i * 2, clamp(green, 0.0f, 1.0f), red);
						}				
					}
				}
			}
			
			// GateType lights
			if (pulsesPerStep != 1 && editingSequence && getGate(sequence, stepIndexEdit)) {
				int gmode = getGateMode(sequence, stepIndexEdit);
				for (int i = 0; i < 8; i++) {
					if (i == gmode) {
						if ( (pulsesPerStep == 4 && i > 2) || (pulsesPerStep == 6 && i <= 2) ) // pps requirement not met
							setGreenRed(GMODE_LIGHTS + i * 2, 0.0f, 1.0f);
						else
							setGreenRed(GMODE_LIGHTS + i * 2, 1.0f, 0.0f);
					}
					else
						setGreenRed(GMODE_LIGHTS + i * 2, 0.0f, 0.0f);
				}
			}
			else {
				for (int i = 0; i < 8; i++) 
					setGreenRed(GMODE_LIGHTS + i * 2, 0.0f, 0.0f);
			}
		
			// Res light
			long editingPpqnInit = (long) (editingPpqnTime * sampleRate / displayRefreshStepSkips);
			if ( ((editingPpqn > 0l) && (editingPpqn < (editingPpqnInit / 6l))) ||
				 ((editingPpqn > (editingPpqnInit * 2l / 6l)) && (editingPpqn < (editingPpqnInit * 3l / 6l))) ||
				 ((editingPpqn > (editingPpqnInit * 4l / 6l)) && (editingPpqn < (editingPpqnInit * 5l / 6l))) )
				lights[RES_LIGHT].value = 1.0f;
			else 
				lights[RES_LIGHT].value = 0.0f;

			// Reset light
			lights[RESET_LIGHT].value =	resetLight;	
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime() * displayRefreshStepSkips;

			// Run lights
			lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;
		
			if (infoCopyPaste != 0l) {
				if (infoCopyPaste > 0l)
					infoCopyPaste --;
				if (infoCopyPaste < 0l)
					infoCopyPaste ++;
			}
			if (displayProbInfo > 0l)
				displayProbInfo--;
			if (modeHoldDetect.process(params[MODES_PARAM].value)) {
				displayState = DISP_GATE;
				editingPpqn = (long) (editingPpqnTime * sampleRate / displayRefreshStepSkips);
			}
			if (editingPpqn > 0l)
				editingPpqn--;
			if (editingPhraseSongRunning > 0l)
				editingPhraseSongRunning--;
			if (revertDisplay > 0l) {
				if (revertDisplay == 1)
					displayState = DISP_GATE;
				revertDisplay--;
			}
			if (blinkNum > 0) {
				blinkCount++;
				if (blinkCount >= (long) (1.0f * sampleRate / displayRefreshStepSkips)) {
					blinkCount = 0l;
					blinkNum--;
				}
			}
		}// lightRefreshCounter

		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;

	}// step()
	
	inline void setGreenRed(int id, float green, float red) {
		lights[id + 0].value = green;
		lights[id + 1].value = red;
	}

};// GateSeq64 : module

struct GateSeq64Widget : ModuleWidget {
	GateSeq64 *module;
	DynamicSVGPanel *panel;
	int oldExpansion;
	int expWidth = 60;
	IMPort* expPorts[6];
		
	struct SequenceDisplayWidget : TransparentWidget {
		GateSeq64 *module;
		std::shared_ptr<Font> font;
		char displayStr[4];
		
		SequenceDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		void runModeToStr(int num) {
			if (num >= 0 && num < NUM_MODES)
				snprintf(displayStr, 4, "%s", modeLabels[num].c_str());
		}

		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box, 18);
			nvgFontFaceId(vg, font->handle);
			bool editingSequence = module->isEditingSequence();

			Vec textPos = Vec(6, 24);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			nvgText(vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(vg, textColor);				
			if (module->infoCopyPaste != 0l) {
				if (module->infoCopyPaste > 0l)// if copy display "CPY"
					snprintf(displayStr, 4, "CPY");
				else {
					int lenCP = module->lengthCPbuffer;
					float cpMode = module->params[GateSeq64::CPMODE_PARAM].value;
					if (editingSequence && lenCP == -1) {// cross paste to seq
						if (cpMode > 1.5f)// All = init
							snprintf(displayStr, 4, "CLR");
						else if (cpMode < 0.5f)// 4 = random gate
							snprintf(displayStr, 4, "RGT");
						else// 8 = random probs
							snprintf(displayStr, 4, "RPR");
					}
					else if (!editingSequence && lenCP != -1) {// cross paste to song
						if (cpMode > 1.5f)// All = init
							snprintf(displayStr, 4, "CLR");
						else if (cpMode < 0.5f)// 4 = increase by 1
							snprintf(displayStr, 4, "INC");
						else// 8 = random phrases
							snprintf(displayStr, 4, "RPH");
					}
					else
						snprintf(displayStr, 4, "PST");
				}
			}
			else if (module->displayProbInfo != 0l) {
				int prob = module->getGatePVal(module->sequence, module->stepIndexEdit);
				if ( prob>= 100)
					snprintf(displayStr, 4, "1,0");
				else if (prob >= 10)
					snprintf(displayStr, 4, ",%2u", (unsigned) prob);
				else if (prob >= 1)
					snprintf(displayStr, 4, " ,%1u", (unsigned) prob);
				else
					snprintf(displayStr, 4, "  0");
			}
			else if (module->editingPpqn != 0ul) {
				snprintf(displayStr, 4, "x%2u", (unsigned) module->pulsesPerStep);
			}
			else if (module->displayState == GateSeq64::DISP_LENGTH) {
				if (editingSequence)
					snprintf(displayStr, 4, "L%2u", (unsigned) module->lengths[module->sequence]);
				else
					snprintf(displayStr, 4, "L%2u", (unsigned) module->phrases);
			}
			else if (module->displayState == GateSeq64::DISP_MODES) {
				if (editingSequence)
					runModeToStr(module->runModeSeq[module->sequence]);
				else
					runModeToStr(module->runModeSong);
			}
			else {
				int dispVal = 0;
				char specialCode = ' ';
				if (editingSequence)
					dispVal = module->sequence;
				else {
					if (module->editingPhraseSongRunning > 0l || !module->running) {
						dispVal = module->phrase[module->phraseIndexEdit];
						if (module->editingPhraseSongRunning > 0l)
							specialCode = '*';
					}
					else
						dispVal = module->phrase[module->phraseIndexRun];
				}
				snprintf(displayStr, 4, "%c%2u", specialCode, (unsigned)(dispVal) + 1 );
			}
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};	
		
	struct PanelThemeItem : MenuItem {
		GateSeq64 *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	struct ExpansionItem : MenuItem {
		GateSeq64 *module;
		void onAction(EventAction &e) override {
			module->expansion = module->expansion == 1 ? 0 : 1;
		}
	};
	struct ResetOnRunItem : MenuItem {
		GateSeq64 *module;
		void onAction(EventAction &e) override {
			module->resetOnRun = !module->resetOnRun;
		}
	};
	struct AutoseqItem : MenuItem {
		GateSeq64 *module;
		void onAction(EventAction &e) override {
			module->autoseq = !module->autoseq;
		}
	};
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		GateSeq64 *module = dynamic_cast<GateSeq64*>(this->module);
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
		
		ResetOnRunItem *rorItem = MenuItem::create<ResetOnRunItem>("Reset on Run", CHECKMARK(module->resetOnRun));
		rorItem->module = module;
		menu->addChild(rorItem);
		
		AutoseqItem *aseqItem = MenuItem::create<AutoseqItem>("AutoSeq when writing via CV inputs", CHECKMARK(module->autoseq));
		aseqItem->module = module;
		menu->addChild(aseqItem);

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

	struct CKSSThreeInvNotify : CKSSThreeInv {
		CKSSThreeInvNotify() {};
		void onDragStart(EventDragStart &e) override {
			ToggleSwitch::onDragStart(e);
			((GateSeq64*)(module))->stepConfigSync = 2;// signal a sync from switch so that steps get initialized
		}	
	};

	GateSeq64Widget(GateSeq64 *module) : ModuleWidget(module) {		
		this->module = module;
		oldExpansion = -1;
		
		// Main panel from Inkscape
        panel = new DynamicSVGPanel();
        panel->mode = &module->panelTheme;
		panel->expWidth = &expWidth;
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/GateSeq64.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/dark/GateSeq64_dark.svg")));
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
		
		
		// ****** Top portion (LED button array and gate type LED buttons) ******
		
		static const int rowRuler0 = 32;
		static const int spacingRows = 32;
		static const int colRulerSteps = 15;
		static const int spacingSteps = 20;
		static const int spacingSteps4 = 4;
		
		
		// Step LED buttons and GateMode lights
		for (int y = 0; y < 4; y++) {
			int posX = colRulerSteps;
			for (int x = 0; x < 16; x++) {
				addParam(createParam<LEDButton>(Vec(posX, rowRuler0 + 8 + y * spacingRows - 4.4f), module, GateSeq64::STEP_PARAMS + y * 16 + x, 0.0f, 1.0f, 0.0f));
				addChild(createLight<MediumLight<GreenRedLight>>(Vec(posX + 4.4f, rowRuler0 + 8 + y * spacingRows), module, GateSeq64::STEP_LIGHTS + (y * 16 + x) * 2));
				posX += spacingSteps;
				if ((x + 1) % 4 == 0)
					posX += spacingSteps4;
			}
		}
					
		// Gate type LED buttons (bottom left to top left to top right)
		static const int rowRulerG0 = 166;
		static const int rowSpacingG = 26;
		static const int colSpacingG = 56;
		static const int colRulerG0 = 15 + 28;
		
		addParam(createParam<LEDButton>(Vec(colRulerG0, rowRulerG0 + rowSpacingG * 2 - 4.4f), module, GateSeq64::GMODE_PARAMS + 2, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerG0 + 4.4f, rowRulerG0 + rowSpacingG * 2), module, GateSeq64::GMODE_LIGHTS + 2 * 2));
		addParam(createParam<LEDButton>(Vec(colRulerG0, rowRulerG0 + rowSpacingG - 4.4f), module, GateSeq64::GMODE_PARAMS + 1, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerG0 + 4.4f, rowRulerG0 + rowSpacingG), module, GateSeq64::GMODE_LIGHTS + 1 * 2));
		addParam(createParam<LEDButton>(Vec(colRulerG0, rowRulerG0 - 4.4f), module, GateSeq64::GMODE_PARAMS + 0, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerG0 + 4.4f, rowRulerG0), module, GateSeq64::GMODE_LIGHTS + 0 * 2));		
		for (int x = 1; x < 6; x++) {
			addParam(createParam<LEDButton>(Vec(colRulerG0 + colSpacingG * x, rowRulerG0 - 4.4f), module, GateSeq64::GMODE_PARAMS + 2 + x, 0.0f, 1.0f, 0.0f));
			addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerG0 + colSpacingG * x + 4.4f, rowRulerG0), module, GateSeq64::GMODE_LIGHTS + (2 + x) * 2));
		}
		
		
		
		
		// ****** 5x3 Main bottom half Control section ******
		
		static const int colRulerC0 = 25;
		static const int colRulerC1 = 78;
		static const int colRulerC2 = 126;
		static const int colRulerC3 = 189;
		static const int colRulerC4 = 241;
		static const int rowRulerC0 = 206; 
		static const int rowRulerSpacing = 58;
		static const int rowRulerC1 = rowRulerC0 + rowRulerSpacing;
		static const int rowRulerC2 = rowRulerC1 + rowRulerSpacing;
				
		
		// Clock input
		addInput(createDynamicPort<IMPort>(Vec(colRulerC0, rowRulerC1), Port::INPUT, module, GateSeq64::CLOCK_INPUT, &module->panelTheme));
		// Reset CV
		addInput(createDynamicPort<IMPort>(Vec(colRulerC0, rowRulerC2), Port::INPUT, module, GateSeq64::RESET_INPUT, &module->panelTheme));
		
				
		// Prob button
		addParam(createDynamicParam<IMBigPushButton>(Vec(colRulerC1 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::PROB_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Reset LED bezel and light
		addParam(createParam<LEDBezel>(Vec(colRulerC1 + offsetLEDbezel, rowRulerC1 + offsetLEDbezel), module, GateSeq64::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MuteLight<GreenLight>>(Vec(colRulerC1 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC1 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RESET_LIGHT));
		// Seq CV
		addInput(createDynamicPort<IMPort>(Vec(colRulerC1, rowRulerC2), Port::INPUT, module, GateSeq64::SEQCV_INPUT, &module->panelTheme));
		
		// Sequence knob
		addParam(createDynamicParam<IMBigKnobInf>(Vec(colRulerC2 + 1 + offsetIMBigKnob, rowRulerC0 + offsetIMBigKnob), module, GateSeq64::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f, &module->panelTheme));		
		// Run LED bezel and light
		addParam(createParam<LEDBezel>(Vec(colRulerC2 + offsetLEDbezel, rowRulerC1 + offsetLEDbezel), module, GateSeq64::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MuteLight<GreenLight>>(Vec(colRulerC2 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC1 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RUN_LIGHT));
		// Run CV
		addInput(createDynamicPort<IMPort>(Vec(colRulerC2, rowRulerC2), Port::INPUT, module, GateSeq64::RUNCV_INPUT, &module->panelTheme));

		
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(colRulerC3 - 15, rowRulerC0 + vOffsetDisplay);
		displaySequence->box.size = Vec(55, 30);// 3 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Modes button
		addParam(createDynamicParam<IMBigPushButton>(Vec(colRulerC3 + offsetCKD6b, rowRulerC1 + offsetCKD6b), module, GateSeq64::MODES_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLight<SmallLight<RedLight>>(Vec(colRulerC3 + offsetCKD6b + 24, rowRulerC1 + 0 + offsetCKD6b + 31), module, GateSeq64::RES_LIGHT));
		// Copy/paste buttons
		addParam(createDynamicParam<IMPushButton>(Vec(colRulerC3 - 10, rowRulerC2 + offsetTL1105), module, GateSeq64::COPY_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<IMPushButton>(Vec(colRulerC3 + 20, rowRulerC2 + offsetTL1105), module, GateSeq64::PASTE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		

		// Seq/Song selector
		addParam(createParam<CKSS>(Vec(colRulerC4 + 2 + hOffsetCKSS, rowRulerC0 + vOffsetCKSS), module, GateSeq64::EDIT_PARAM, 0.0f, 1.0f, 1.0f));
		// Config switch (3 position)
		addParam(createParam<CKSSThreeInvNotify>(Vec(colRulerC4 + 2 + hOffsetCKSS, rowRulerC1 - 2 + vOffsetCKSSThree), module, GateSeq64::CONFIG_PARAM, 0.0f, 2.0f, GateSeq64::CONFIG_PARAM_INIT_VALUE));// 0.0f is top position
		// Copy paste mode
		addParam(createParam<CKSSThreeInv>(Vec(colRulerC4 + 2 + hOffsetCKSS, rowRulerC2 + vOffsetCKSSThree), module, GateSeq64::CPMODE_PARAM, 0.0f, 2.0f, 2.0f));

		// Outputs
		for (int iSides = 0; iSides < 4; iSides++)
			addOutput(createDynamicPort<IMPort>(Vec(311, rowRulerC0 + iSides * 40), Port::OUTPUT, module, GateSeq64::GATE_OUTPUTS + iSides, &module->panelTheme));
		
		// Expansion module
		static const int rowRulerExpTop = 60;
		static const int rowSpacingExp = 50;
		static const int colRulerExp = 497 - 30 - 90;// GS64 is (2+6)HP less than PS32
		addInput(expPorts[0] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 0), Port::INPUT, module, GateSeq64::WRITE_INPUT, &module->panelTheme));
		addInput(expPorts[1] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 1), Port::INPUT, module, GateSeq64::GATE_INPUT, &module->panelTheme));
		addInput(expPorts[2] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 2), Port::INPUT, module, GateSeq64::PROB_INPUT, &module->panelTheme));
		addInput(expPorts[3] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 3), Port::INPUT, module, GateSeq64::WRITE0_INPUT, &module->panelTheme));
		addInput(expPorts[4] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 4), Port::INPUT, module, GateSeq64::WRITE1_INPUT, &module->panelTheme));
		addInput(expPorts[5] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 5), Port::INPUT, module, GateSeq64::STEPL_INPUT, &module->panelTheme));

	}
};


Model *modelGateSeq64 = Model::create<GateSeq64, GateSeq64Widget>("Impromptu Modular", "Gate-Seq-64", "SEQ - Gate-Seq-64", SEQUENCER_TAG);

/*CHANGE LOG

0.6.13:
fix run mode bug (history not reset when hard reset)

0.6.12:
input refresh optimization
add separate buttons for each advanced-gate (remove left right buttons)
change behavior of write CV input in exp pannel (prob not reset when ProbIn unconnected, and gate not written when GateIn unconnected)

0.6.11:
step optimization of lights refresh
add RN2 run mode
add step-left CV input in expansion panel
implement copy-paste in song mode and change 4/ROW/ALL to 4/8/ALL
implement cross paste trick for init and randomize seq/song
add AutoSeq option when writing via CV inputs 
make song mode 64 sequences long

0.6.10:
add advanced gate mode

0.6.9:
add FW2, FW3 and FW4 run modes for sequences (but not for song)

0.6.7:
add expansion panel with extra CVs for writing steps into the module
allow full edit capabilities in song mode
no reset on run by default, with switch added in context menu
reset does not revert seq or song number to 1

0.6.6:
config and knob bug fixes when loading patch

0.6.5:
swap MODE/LEN so that length happens first (update manual)

0.6.4:
initial release of GS64
*/