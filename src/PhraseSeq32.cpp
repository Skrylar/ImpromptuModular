//***********************************************************************************************
//Multi-phrase 32 step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by the SA-100 Stepper Acid sequencer by Transistor Sounds Labs
//
//Acknowledgements: please see README.md
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct PhraseSeq32 : Module {
	enum ParamIds {
		LEFT_PARAM,
		RIGHT_PARAM,
		RIGHT8_PARAM,
		EDIT_PARAM,
		SEQUENCE_PARAM,
		RUN_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		ENUMS(OCTAVE_PARAM, 7),
		GATE1_PARAM,
		GATE2_PARAM,
		SLIDE_BTN_PARAM,
		SLIDE_KNOB_PARAM,
		ATTACH_PARAM,
		AUTOSTEP_PARAM,
		ENUMS(KEY_PARAMS, 12),
		RUNMODE_PARAM,
		TRAN_ROT_PARAM,
		GATE1_KNOB_PARAM,
		GATE1_PROB_PARAM,
		TIE_PARAM,// Legato
		CPMODE_PARAM,
		ENUMS(STEP_PHRASE_PARAMS, 32),
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		CV_INPUT,
		RESET_INPUT,
		CLOCK_INPUT,
		LEFTCV_INPUT,
		RIGHTCV_INPUT,
		RUNCV_INPUT,
		SEQCV_INPUT,
		MODECV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE1_OUTPUT,
		GATE2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, 32 * 2),// room for GreenRed
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12),
		RUN_LIGHT,
		RESET_LIGHT,
		GATE1_LIGHT,
		GATE2_LIGHT,
		SLIDE_LIGHT,
		ATTACH_LIGHT,
		GATE1_PROB_LIGHT,
		TIE_LIGHT,
		NUM_LIGHTS
	};
	
	enum DisplayStateIds {DISP_NORMAL, DISP_MODE, DISP_LENGTH, DISP_TRANSPOSE, DISP_ROTATE};
	enum AttributeBitMasks {ATT_MSK_GATE1 = 0x01, ATT_MSK_GATE1P = 0x02, ATT_MSK_GATE2 = 0x04, ATT_MSK_SLIDE = 0x08, ATT_MSK_TIED = 0x10};

	// Need to save
	bool running;
	int runModeSeq; 
	int runModeSong; 
	//
	int sequence;
	int lengths[32];//1 to 32
	//
	int phrase[32] = {};// This is the song (series of phases; a phrase is a patten number)
	int phrases;//1 to 32
	//
	float cv[32][32] = {};// [-3.0 : 3.917]. First index is patten number, 2nd index is step
	int attributes[32][32] = {};// First index is patten number, 2nd index is step (see enum AttributeBitMasks for details)
	//
	float attach;

	// No need to save
	float resetLight = 0.0f;
	int stepIndexEdit;
	int stepIndexRun;
	int phraseIndexEdit;
	int phraseIndexRun;
	int stepIndexPhraseRun;
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	float editingGateCV;// no need to initialize, this is a companion to editingGate
	int stepIndexRunHistory;// no need to initialize
	int stepIndexPhraseRunHistory;// no need to initialize
	int phraseIndexRunHistory;// no need to initialize
	int displayState;
	unsigned long slideStepsRemain;// 0 when no slide under way, downward step counter when sliding
	float slideCVdelta;// no need to initialize, this is a companion to slideStepsRemain
	float cvCPbuffer[32];// copy paste buffer for CVs
	int attributesCPbuffer[32];// copy paste buffer for attributes
	int lengthCPbuffer;
	int countCP;// number of steps to paste (in case CPMODE_PARAM changes between copy and paste)
	bool gate1RandomEnable; 
	int transposeOffset;// no need to initialize, this is companion to displayMode = DISP_TRANSPOSE
	int rotateOffset;// no need to initialize, this is companion to displayMode = DISP_ROTATE
	long clockIgnoreOnReset;
	const float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
	unsigned long clockPeriod;// counts number of step() calls upward from last clock (reset after clock processed)
	long tiedWarning;// 0 when no warning, positive downward step counter timer when warning
	int sequenceKnob;// INT_MAX when knob not seen yet


	SchmittTrigger resetTrigger;
	SchmittTrigger leftTrigger;
	SchmittTrigger rightTrigger;
	SchmittTrigger right8Trigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger octTriggers[7];
	SchmittTrigger octmTrigger;
	SchmittTrigger gate1Trigger;
	SchmittTrigger gate1ProbTrigger;
	SchmittTrigger gate2Trigger;
	SchmittTrigger slideTrigger;
	SchmittTrigger keyTriggers[12];
	SchmittTrigger writeTrigger;
	SchmittTrigger attachTrigger;
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger modeTrigger;
	SchmittTrigger rotateTrigger;
	SchmittTrigger transposeTrigger;
	SchmittTrigger editTrigger;
	SchmittTrigger editTriggerInv;
	SchmittTrigger tiedTrigger;
	SchmittTrigger stepTriggers[32];
	
		
	PhraseSeq32() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		running = false;
		runModeSeq = 0;
		runModeSong = 0;
		stepIndexEdit = 0;
		stepIndexRun = 0;
		sequence = 0;
		phraseIndexEdit = 0;
		phraseIndexRun = 0;
		stepIndexPhraseRun = 0;
		phrases = 4;
		for (int i = 0; i < 32; i++) {
			for (int s = 0; s < 32; s++) {
				cv[i][s] = 0.0f;
				attributes[i][s] = ATT_MSK_GATE1 | ATT_MSK_GATE2;
			}
			phrase[i] = 0;
			lengths[i] = 32;
			cvCPbuffer[i] = 0.0f;
			attributesCPbuffer[i] = ATT_MSK_GATE1 | ATT_MSK_GATE2;
		}
		lengthCPbuffer = 32;
		countCP = 32;
		sequenceKnob = INT_MAX;
		editingGate = 0ul;
		infoCopyPaste = 0l;
		displayState = DISP_NORMAL;
		slideStepsRemain = 0ul;
		attach = 1.0f;
		gate1RandomEnable = false;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
		clockPeriod = 0ul;
		tiedWarning = 0ul;
	}

	void onRandomize() override {
		running = false;
		runModeSeq = randomu32() % 5;
		runModeSong = randomu32() % 5;
		stepIndexEdit = 0;
		stepIndexRun = 0;
		sequence = randomu32() % 32;
		phraseIndexEdit = 0;
		phraseIndexRun = 0;
		stepIndexPhraseRun = 0;
		phrases = 1 + (randomu32() % 32);
		for (int i = 0; i < 32; i++) {
			for (int s = 0; s < 32; s++) {
				cv[i][s] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
				attributes[i][s] = randomu32() % 32;// 32 because 5 attributes
				if ((attributes[i][s] & ATT_MSK_TIED) != 0)
					attributes[i][s] = ATT_MSK_TIED;// clear other attributes if tied
			}
			phrase[i] = randomu32() % 32;
			lengths[i] = 1 + (randomu32() % 32);
			cvCPbuffer[i] = 0.0f;
			attributesCPbuffer[i] = ATT_MSK_GATE1 | ATT_MSK_GATE2;
		}
		lengthCPbuffer = 32;
		countCP = 32;
		sequenceKnob = INT_MAX;
		editingGate = 0ul;
		infoCopyPaste = 0l;
		displayState = DISP_NORMAL;
		slideStepsRemain = 0ul;
		attach = 1.0f;
		gate1RandomEnable = false;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
		clockPeriod = 0ul;
		tiedWarning = 0ul;
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

		// lengths 
		json_t *lengthsJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(lengthsJ, i, json_integer(lengths[i]));
		json_object_set_new(rootJ, "lengths", lengthsJ);

		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, "phrase", phraseJ);

		// phrases
		json_object_set_new(rootJ, "phrases", json_integer(phrases));

		// CV
		json_t *cvJ = json_array();
		for (int i = 0; i < 32; i++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(cvJ, s + (i * 32), json_real(cv[i][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// attributes
		json_t *attributesJ = json_array();
		for (int i = 0; i < 32; i++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(attributesJ, s + (i * 32), json_integer(attributes[i][s]));
			}
		json_object_set_new(rootJ, "attributes", attributesJ);

		// attach
		json_object_set_new(rootJ, "attach", json_real(attach));

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
		
		// lengths
		json_t *lengthsJ = json_object_get(rootJ, "lengths");
		if (lengthsJ)
			for (int i = 0; i < 32; i++)
			{
				json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
				if (lengthsArrayJ)
					lengths[i] = json_integer_value(lengthsArrayJ);
			}
			
		// phrase
		json_t *phraseJ = json_object_get(rootJ, "phrase");
		if (phraseJ)
			for (int i = 0; i < 32; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, "phrases");
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
		
		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int i = 0; i < 32; i++)
				for (int s = 0; s < 32; s++) {
					json_t *cvArrayJ = json_array_get(cvJ, s + (i * 32));
					if (cvArrayJ)
						cv[i][s] = json_real_value(cvArrayJ);
				}
		}
		
		// attributes
		json_t *attributesJ = json_object_get(rootJ, "attributes");
		if (attributesJ) {
			for (int i = 0; i < 32; i++)
				for (int s = 0; s < 32; s++) {
					json_t *attributesArrayJ = json_array_get(attributesJ, s + (i * 32));
					if (attributesArrayJ)
						attributes[i][s] = json_integer_value(attributesArrayJ);
				}
		}
		
		// attach
		json_t *attachJ = json_object_get(rootJ, "attach");
		if (attachJ)
			attach = json_real_value(attachJ);
	}

	void rotateSeq(int sequenceNum, bool directionRight, int numSteps) {
		float rotCV;
		int rotAttributes;
		int iRot = 0;
		int iDelta = 0;
		if (directionRight) {
			iRot = numSteps - 1;
			iDelta = -1;
		}
		else {
			iDelta = 1;
		}
		rotCV = cv[sequenceNum][iRot];
		rotAttributes = attributes[sequenceNum][iRot];
		for ( ; ; iRot += iDelta) {
			if (iDelta == 1 && iRot >= numSteps - 1) break;
			if (iDelta == -1 && iRot <= 0) break;
			cv[sequenceNum][iRot] = cv[sequenceNum][iRot + iDelta];
			attributes[sequenceNum][iRot] = attributes[sequenceNum][iRot + iDelta];
		}
		cv[sequenceNum][iRot] = rotCV;
		attributes[sequenceNum][iRot] = rotAttributes;
	}
	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		static const float gateTime = 0.3f;// seconds
		static const float copyPasteInfoTime = 0.4f;// seconds
		static const float tiedWarningTime = 0.4f;// seconds
		long tiedWarningInit = (long) (tiedWarningTime * engineGetSampleRate());
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		// Notes: 
		// * a tied step's attributes can not be modified by any of the following: 
		//   write input, oct and keyboard buttons, gate1, gate1Prob, gate2 and slide buttons
		//   however, paste, transpose, rotate obviously can.
		// * Whenever cv[][] is modified or tied[] is activated for a step, call applyTiedStep(sequence,stepIndexEdit,steps)
		
		bool editingSequence = params[EDIT_PARAM].value > 0.5f;// true = editing sequence, false = editing song
		if ( editTrigger.process(params[EDIT_PARAM].value) || editTriggerInv.process(1.0f - params[EDIT_PARAM].value) )
			displayState = DISP_NORMAL;
		
		// Seq CV input
		if (inputs[SEQCV_INPUT].active) {
			sequence = (int) clamp( round(inputs[SEQCV_INPUT].value * (32.0f - 1.0f) / 10.0f), 0.0f, (32.0f - 1.0f) );
		}
		
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUNCV_INPUT].value)) {
			running = !running;
			displayState = DISP_NORMAL;
			if (running) {
				stepIndexEdit = 0;
				stepIndexRun = 0;
				phraseIndexEdit = 0;
				phraseIndexRun = 0;
				stepIndexPhraseRun = 0;
			}
		}

		// Attach button
		if (attachTrigger.process(params[ATTACH_PARAM].value)) {
			if (running) {
				attach = 1.0f - attach;// toggle
			}	
			displayState = DISP_NORMAL;			
		}
		if (running && attach > 0.5f) {
			if (editingSequence)
				stepIndexEdit = stepIndexRun;
			else
				phraseIndexEdit = phraseIndexRun;
		}
		
		// Copy button
		if (copyTrigger.process(params[COPY_PARAM].value)) {
			if (editingSequence) {
				infoCopyPaste = (long) (copyPasteInfoTime * engineGetSampleRate());
				//CPinfo must be set to 0 for copy/paste all, and 0x1ii for copy/paste 4 at pos ii, 0x2ii for copy/paste 8 at 0xii
				int sStart = stepIndexEdit;
				int sCount = 32;
				if (params[CPMODE_PARAM].value > 1.5f)// all
					sStart = 0;
				else if (params[CPMODE_PARAM].value < 0.5f)// 4
					sCount = 4;
				else// 8
					sCount = 8;
				countCP = sCount;
				for (int i = 0, s = sStart; i < countCP; i++, s++) {
					if (s >= 32) s = 0;
					cvCPbuffer[i] = cv[sequence][s];
					attributesCPbuffer[i] = attributes[sequence][s];
					if ((--sCount) <= 0)
						break;
				}
				lengthCPbuffer = lengths[sequence];
			}
			displayState = DISP_NORMAL;
		}
		// Paste button
		if (pasteTrigger.process(params[PASTE_PARAM].value)) {
			if (editingSequence) {
				infoCopyPaste = (long) (-1 * copyPasteInfoTime * engineGetSampleRate());
				int sStart = ((countCP == 32) ? 0 : stepIndexEdit);
				int sCount = countCP;
				for (int i = 0, s = sStart; i < countCP; i++, s++) {
					if (s >= 32) s = 0;
					cv[sequence][s] = cvCPbuffer[i];
					attributes[sequence][s] = attributesCPbuffer[i];
					if ((--sCount) <= 0)
						break;
				}	
				if (params[CPMODE_PARAM].value > 1.5f)// all
					lengths[sequence] = lengthCPbuffer;
			}
			displayState = DISP_NORMAL;
		}

		// Write input (must be before Left and Right in case route gate simultaneously to Right and Write for example)
		//  (write must be to correct step)
		bool writeTrig = writeTrigger.process(inputs[WRITE_INPUT].value);
		if (writeTrig) {
			if (editingSequence) {
				if ((attributes[sequence][stepIndexEdit] & ATT_MSK_TIED) != 0)
					tiedWarning = tiedWarningInit;
				else {			
					editingGate = (unsigned long) (gateTime * engineGetSampleRate());
					editingGateCV = inputs[CV_INPUT].value;
					cv[sequence][stepIndexEdit] = inputs[CV_INPUT].value;
					applyTiedStep(sequence, stepIndexEdit, lengths[sequence]);
					if (params[AUTOSTEP_PARAM].value > 0.5f)
						stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, lengths[sequence]);
				}
			}
			displayState = DISP_NORMAL;
		}
		
		// Step button presses
		int stepPressed = -1;
		for (int i = 0; i < 32; i++) {
			if (stepTriggers[i].process(params[STEP_PHRASE_PARAMS + i].value))
				stepPressed = i;
		}
		if (stepPressed != -1) {
			if (displayState == DISP_LENGTH) {
				if (editingSequence) {
					lengths[sequence] = stepPressed + 1;
					if (lengths[sequence] > 32) lengths[sequence] = 32;
					if (lengths[sequence] < 1 ) lengths[sequence] = 1;
					if (stepIndexEdit >= lengths[sequence]) stepIndexEdit = lengths[sequence] - 1;
				}
				else {
					phrases = stepPressed + 1;
					if (phrases > 32) phrases = 32;
					if (phrases < 1 ) phrases = 1;
					if (phraseIndexEdit >= phrases) phraseIndexEdit = phrases - 1;
				}
			}
			else {
				if (!running || attach < 0.5f) {// don't move heads when attach and running
					if (editingSequence) {
						stepIndexEdit = stepPressed;
						if (stepIndexEdit >= lengths[sequence])
							stepIndexEdit = lengths[sequence] - 1;
						if ((attributes[sequence][stepIndexEdit] & ATT_MSK_TIED) == 0) {// play if non-tied step
							if (!writeTrig)
								editingGateCV = cv[sequence][stepIndexEdit];// don't overwrite when simultaneous writeCV and stepCV
							editingGate = (unsigned long) (gateTime * engineGetSampleRate());
						}
					}
					else {
						phraseIndexEdit = stepPressed;
						if (phraseIndexEdit >= phrases)
							phraseIndexEdit = phrases - 1;
					}
				}
				displayState = DISP_NORMAL;
			}
		} 
		
		// Mode/Length and Transpose/Rotate buttons
		if (modeTrigger.process(params[RUNMODE_PARAM].value)) {
			if (displayState == DISP_NORMAL || displayState == DISP_TRANSPOSE || displayState == DISP_ROTATE)
				displayState = DISP_MODE;
			else if (displayState == DISP_MODE)
				displayState = DISP_LENGTH;
			else
				displayState = DISP_NORMAL;
		}
		if (transposeTrigger.process(params[TRAN_ROT_PARAM].value)) {
			if (editingSequence) {
				if (displayState == DISP_NORMAL || displayState == DISP_MODE || displayState == DISP_LENGTH) {
					displayState = DISP_TRANSPOSE;
					transposeOffset = 0;
				}
				else if (displayState == DISP_TRANSPOSE) {
					displayState = DISP_ROTATE;
					rotateOffset = 0;
				}
				else 
					displayState = DISP_NORMAL;
			}
		}			
		
		// Sequence knob  
		int newSequenceKnob = (int)roundf(params[SEQUENCE_PARAM].value*7.0f);
		if (sequenceKnob == INT_MAX)
			sequenceKnob = newSequenceKnob;
		int deltaKnob = newSequenceKnob - sequenceKnob;
		if (deltaKnob != 0) {
			if (abs(deltaKnob) <= 3) {// avoid discontinuous step (initialize for example)
				if (displayState == DISP_MODE) {
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
						if (lengths[sequence] > 32) lengths[sequence] = 32;
						if (lengths[sequence] < 1 ) lengths[sequence] = 1;
						if (stepIndexEdit >= lengths[sequence]) stepIndexEdit = lengths[sequence] - 1;
					}
					else {
						phrases += deltaKnob;
						if (phrases > 32) phrases = 32;
						if (phrases < 1 ) phrases = 1;
						if (phraseIndexEdit >= phrases) phraseIndexEdit = phrases - 1;
					}
				}
				else if (displayState == DISP_TRANSPOSE) {
					if (editingSequence) {
						transposeOffset += deltaKnob;
						if (transposeOffset > 99) transposeOffset = 99;
						if (transposeOffset < -99) transposeOffset = -99;						
						// Tranpose by this number of semi-tones: deltaKnob
						float transposeOffsetCV = ((float)(deltaKnob))/12.0f;
						for (int s = 0; s < 32; s++) {
							cv[sequence][s] += transposeOffsetCV;
						}
					}
				}
				else if (displayState == DISP_ROTATE) {
					if (editingSequence) {
						rotateOffset += deltaKnob;
						if (rotateOffset > 99) rotateOffset = 99;
						if (rotateOffset < -99) rotateOffset = -99;	
						if (deltaKnob > 0 && deltaKnob < 99) {// Rotate right, 99 is safety
							for (int i = deltaKnob; i > 0; i--)
								rotateSeq(sequence, true, lengths[sequence]);
						}
						if (deltaKnob < 0 && deltaKnob > -99) {// Rotate left, 99 is safety
							for (int i = deltaKnob; i < 0; i++)
								rotateSeq(sequence, false, lengths[sequence]);
						}
					}						
				}
				else {// DISP_NORMAL
					if (editingSequence) {
						if (!inputs[SEQCV_INPUT].active) {
							sequence += deltaKnob;
							if (sequence < 0) sequence = 0;
							if (sequence >= 32) sequence = (32 - 1);
						}
					}
					else {
						phrase[phraseIndexEdit] += deltaKnob;
						if (phrase[phraseIndexEdit] < 0) phrase[phraseIndexEdit] = 0;
						if (phrase[phraseIndexEdit] >= 32) phrase[phraseIndexEdit] = (32 - 1);				
					}
				}
			}
			sequenceKnob = newSequenceKnob;
		}	
		
		// Octave buttons
		int newOct = -1;
		for (int i = 0; i < 7; i++) {
			if (octTriggers[i].process(params[OCTAVE_PARAM + i].value)) {
				newOct = 6 - i;
				displayState = DISP_NORMAL;
			}
		}
		if (newOct >= 0 && newOct <= 6) {
			if (editingSequence) {
				if ( (attributes[sequence][stepIndexEdit] & ATT_MSK_TIED) != 0 )// if tied
					tiedWarning = tiedWarningInit;
				else {			
					float newCV = cv[sequence][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
					newCV = newCV - floor(newCV) + (float) (newOct - 3);
					if (newCV >= -3.0f && newCV < 4.0f) {
						cv[sequence][stepIndexEdit] = newCV;
						applyTiedStep(sequence, stepIndexEdit, lengths[sequence]);
					}
					editingGate = (unsigned long) (gateTime * engineGetSampleRate());
					editingGateCV = cv[sequence][stepIndexEdit];
				}
			}
		}		
		
		// Keyboard buttons
		for (int i = 0; i < 12; i++) {
			if (keyTriggers[i].process(params[KEY_PARAMS + i].value)) {
				if (editingSequence) {
					if ( (attributes[sequence][stepIndexEdit] & ATT_MSK_TIED) != 0 )// if tied
						tiedWarning = tiedWarningInit;
					else {			
						cv[sequence][stepIndexEdit] = floor(cv[sequence][stepIndexEdit]) + ((float) i) / 12.0f;
						applyTiedStep(sequence, stepIndexEdit, lengths[sequence]);
						editingGate = (unsigned long) (gateTime * engineGetSampleRate());
						editingGateCV = cv[sequence][stepIndexEdit];
					}						
				}
				displayState = DISP_NORMAL;
			}
		}
				
		// Gate1, Gate1Prob, Gate2, Slide and Tied buttons
		if (gate1Trigger.process(params[GATE1_PARAM].value)) {
			if (editingSequence) {
				if ( (attributes[sequence][stepIndexEdit] & ATT_MSK_TIED) != 0 )// if tied
					tiedWarning = tiedWarningInit;
				else
					attributes[sequence][stepIndexEdit] ^= ATT_MSK_GATE1;
			}
			displayState = DISP_NORMAL;
		}		
		if (gate1ProbTrigger.process(params[GATE1_PROB_PARAM].value)) {
			if (editingSequence) {
				if ( (attributes[sequence][stepIndexEdit] & ATT_MSK_TIED) != 0 )// if tied
					tiedWarning = tiedWarningInit;
				else
					attributes[sequence][stepIndexEdit] ^= ATT_MSK_GATE1P;
			}
			displayState = DISP_NORMAL;
		}		
		if (gate2Trigger.process(params[GATE2_PARAM].value)) {
			if (editingSequence) {
				if ( (attributes[sequence][stepIndexEdit] & ATT_MSK_TIED) != 0 )// if tied
					tiedWarning = tiedWarningInit;
				else
					attributes[sequence][stepIndexEdit] ^= ATT_MSK_GATE2;
			}
			displayState = DISP_NORMAL;
		}		
		if (slideTrigger.process(params[SLIDE_BTN_PARAM].value)) {
			if (editingSequence) {
				if ( (attributes[sequence][stepIndexEdit] & ATT_MSK_TIED) != 0 )// if tied
					tiedWarning = tiedWarningInit;
				else
					attributes[sequence][stepIndexEdit] ^= ATT_MSK_SLIDE;
			}
			displayState = DISP_NORMAL;
		}		
		if (tiedTrigger.process(params[TIE_PARAM].value)) {
			if (editingSequence) {
				attributes[sequence][stepIndexEdit] ^= ATT_MSK_TIED;
				if ( (attributes[sequence][stepIndexEdit] & ATT_MSK_TIED) != 0 ) {// if tied
					attributes[sequence][stepIndexEdit] &= ~(ATT_MSK_GATE1 | ATT_MSK_GATE1P | ATT_MSK_GATE2 | ATT_MSK_SLIDE);
					applyTiedStep(sequence, stepIndexEdit, lengths[sequence]);
				}
				else
					attributes[sequence][stepIndexEdit] |= ATT_MSK_GATE1;
			}
			displayState = DISP_NORMAL;
		}		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			if (running && clockIgnoreOnReset == 0l) {
				float slideFromCV = 0.0f;
				float slideToCV = 0.0f;
				if (editingSequence) {
					slideFromCV = cv[sequence][stepIndexRun];
					moveIndexRunMode(&stepIndexRun, lengths[sequence], runModeSeq, &stepIndexRunHistory);
					slideToCV = cv[sequence][stepIndexRun];
					gate1RandomEnable = ( (attributes[sequence][stepIndexRun] & ATT_MSK_GATE1P) != 0 );// not random yet
				}
				else {
					slideFromCV = cv[phrase[phraseIndexRun]][stepIndexPhraseRun];
					if (moveIndexRunMode(&stepIndexPhraseRun, lengths[phrase[phraseIndexRun]], runModeSeq, &stepIndexPhraseRunHistory)) {
						moveIndexRunMode(&phraseIndexRun, phrases, runModeSong, &phraseIndexRunHistory);
					}
					slideToCV = cv[phrase[phraseIndexRun]][stepIndexPhraseRun];
					gate1RandomEnable = ( (attributes[phrase[phraseIndexRun]][stepIndexPhraseRun] & ATT_MSK_GATE1P) != 0 );// not random yet
				}
				
				// Slide
				if ( (editingSequence && ((attributes[sequence][stepIndexRun] & ATT_MSK_SLIDE) != 0) ) || 
				    (!editingSequence && ((attributes[phrase[phraseIndexRun]][stepIndexPhraseRun] & ATT_MSK_SLIDE) != 0) ) ) {
					// avtivate sliding (slideStepsRemain can be reset, else runs down to 0, either way, no need to reinit)
					slideStepsRemain =   (unsigned long) (((float)clockPeriod)   * params[SLIDE_KNOB_PARAM].value / 2.0f);// 0-T slide, where T is clock period (can be too long when user does clock gating)
					//slideStepsRemain = (unsigned long)  (engineGetSampleRate() * params[SLIDE_KNOB_PARAM].value );// 0-2s slide
					slideCVdelta = (slideToCV - slideFromCV)/(float)slideStepsRemain;
				}

				if (gate1RandomEnable)
					gate1RandomEnable = randomUniform() < (params[GATE1_KNOB_PARAM].value);// randomUniform is [0.0, 1.0), see include/util/common.hpp
				else 
					gate1RandomEnable = true;
			}
			clockPeriod = 0ul;
		}	
		clockPeriod++;
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			stepIndexEdit = 0;
			stepIndexRun = 0;
			sequence = 0;
			phraseIndexEdit = 0;
			phraseIndexRun = 0;
			stepIndexPhraseRun = 0;
			resetLight = 1.0f;
			displayState = DISP_NORMAL;
			clockTrigger.reset();
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
		}
		else
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime();
		
		
		//********** Outputs and lights **********
				
		// CV and gates outputs
		if (running) {
			float slideOffset = (slideStepsRemain > 0ul ? (slideCVdelta * (float)slideStepsRemain) : 0.0f);
			if (editingSequence) {// editing sequence while running
				outputs[CV_OUTPUT].value = cv[sequence][stepIndexRun] - slideOffset;
				outputs[GATE1_OUTPUT].value = (clockTrigger.isHigh() && gate1RandomEnable && 
												((attributes[sequence][stepIndexRun] & ATT_MSK_GATE1) != 0)) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (clockTrigger.isHigh() && 
												((attributes[sequence][stepIndexRun] & ATT_MSK_GATE2) != 0)) ? 10.0f : 0.0f;
			}
			else {// editing song while running
				outputs[CV_OUTPUT].value = cv[phrase[phraseIndexRun]][stepIndexPhraseRun] - slideOffset;
				outputs[GATE1_OUTPUT].value = (clockTrigger.isHigh() && gate1RandomEnable && 
												((attributes[phrase[phraseIndexRun]][stepIndexPhraseRun] & ATT_MSK_GATE1) != 0)) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (clockTrigger.isHigh() && 
												((attributes[phrase[phraseIndexRun]][stepIndexPhraseRun] & ATT_MSK_GATE2) != 0)) ? 10.0f : 0.0f;
			}
		}
		else {// not running 
			if (editingSequence) {// editing sequence while not running
				outputs[CV_OUTPUT].value = editingGateCV;//cv[sequence][stepIndexEdit];
				outputs[GATE1_OUTPUT].value = (editingGate > 0ul) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (editingGate > 0ul) ? 10.0f : 0.0f;
			}
			else {// editing song while not running
				outputs[CV_OUTPUT].value = 0.0f;
				outputs[GATE1_OUTPUT].value = 0.0f;
				outputs[GATE2_OUTPUT].value = 0.0f;
			}
		}
		
		// Step/phrase lights
		if (infoCopyPaste != 0l) {
			for (int i = 0; i < 32; i++) {
				if ( (i >= stepIndexEdit && i < (stepIndexEdit + countCP)) || 
				     i < ((stepIndexEdit + countCP) - 32) || (countCP == 32)  )
					lights[STEP_PHRASE_LIGHTS + (i<<1)].value = 0.5f;// Green when copy interval
				else
					lights[STEP_PHRASE_LIGHTS + (i<<1)].value = 0.0f; // Green (nothing)
				lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = 0.0f;// Red (nothing)
			}
		}
		else {
			for (int i = 0; i < 32; i++) {
				if (displayState == DISP_LENGTH) {
					// Length (green)
					if (editingSequence)
						lights[STEP_PHRASE_LIGHTS + (i<<1)].value = ((i < lengths[sequence]) ? 0.5f : 0.0f);
					else
						lights[STEP_PHRASE_LIGHTS + (i<<1)].value = ((i < phrases) ? 0.5f : 0.0f);
					// Nothing (red)
					lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = 0.0f;
				}
				else {
					// Run cursor (green)
					if (editingSequence)
						lights[STEP_PHRASE_LIGHTS + (i<<1)].value = ((running && (i == stepIndexRun)) ? 1.0f : 0.0f);
					else {
						float green = ((running && (i == phraseIndexRun)) ? 1.0f : 0.0f);
						green += ((running && (i == stepIndexPhraseRun) && i != phraseIndexEdit) ? 0.1f : 0.0f);
						lights[STEP_PHRASE_LIGHTS + (i<<1)].value = clamp(green, 0.0f, 1.0f);
					}
					// Edit cursor (red)
					if (editingSequence)
						lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = (i == stepIndexEdit ? 1.0f : 0.0f);
					else
						lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = (i == phraseIndexEdit ? 1.0f : 0.0f);
				}
			}
		}
	
		// Octave lights
		float octCV = 0.0f;
		if (editingSequence)
			octCV = cv[sequence][stepIndexEdit];
		else
			octCV = cv[phrase[phraseIndexEdit]][stepIndexPhraseRun];
		int octLightIndex = (int) floor(octCV + 3.0f);
		for (int i = 0; i < 7; i++) {
			lights[OCTAVE_LIGHTS + i].value = (i == (6 - octLightIndex) ? 1.0f : 0.0f);
		}
		
		// Keyboard lights
		float cvValOffset;
		if (editingSequence) 
			cvValOffset = cv[sequence][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
		else	
			cvValOffset = cv[phrase[phraseIndexEdit]][stepIndexPhraseRun] + 10.0f;//to properly handle negative note voltages
		int keyLightIndex = (int) clamp(  roundf( (cvValOffset-floor(cvValOffset)) * 12.0f ),  0.0f,  11.0f);
		for (int i = 0; i < 12; i++) {
			lights[KEY_LIGHTS + i].value = (i == keyLightIndex ? 1.0f : 0.0f);
		}			
		
		// Gate1, Gate1Prob, Gate2, Slide and Tied lights
		int attributesVal = attributes[sequence][stepIndexEdit];
		if (!editingSequence)
			attributesVal = attributes[phrase[phraseIndexEdit]][stepIndexPhraseRun];
		//
		lights[GATE1_LIGHT].value = ((attributesVal & ATT_MSK_GATE1) != 0) ? 1.0f : 0.0f;
		lights[GATE1_PROB_LIGHT].value = ((attributesVal & ATT_MSK_GATE1P) != 0) ? 1.0f : 0.0f;
		lights[GATE2_LIGHT].value = ((attributesVal & ATT_MSK_GATE2) != 0) ? 1.0f : 0.0f;
		lights[SLIDE_LIGHT].value = ((attributesVal & ATT_MSK_SLIDE) != 0) ? 1.0f : 0.0f;
		if (tiedWarning > 0l) {
			bool warningFlashState = calcTiedWarning(tiedWarning, tiedWarningInit);
			lights[TIE_LIGHT].value = (warningFlashState) ? 1.0f : 0.0f;
		}
		else
			lights[TIE_LIGHT].value = ((attributesVal & ATT_MSK_TIED) != 0) ? 1.0f : 0.0f;

		// Attach light
		lights[ATTACH_LIGHT].value = running ? attach : 0.0f;
		
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	
		
		// Run light
		lights[RUN_LIGHT].value = running;

		if (editingGate > 0ul)
			editingGate--;
		if (infoCopyPaste != 0l) {
			if (infoCopyPaste > 0l)
				infoCopyPaste --;
			if (infoCopyPaste < 0l)
				infoCopyPaste ++;
		}
		if (slideStepsRemain > 0ul)
			slideStepsRemain--;
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
		if (tiedWarning > 0l)
			tiedWarning--;

	}// step()
	
	bool calcTiedWarning(long tiedWarning, long tiedWarningInit) {
		bool warningFlashState = true;
		if (tiedWarning > (tiedWarningInit * 2l / 4l) && tiedWarning < (tiedWarningInit * 3l / 4l))
			warningFlashState = false;
		else if (tiedWarning < (tiedWarningInit * 1l / 4l))
			warningFlashState = false;
		return warningFlashState;
	}	
	
	void applyTiedStep(int seqNum, int indexTied, int numSteps) {
		// Called because either:
		//   case A: tied was activated for given step
		//   case B: the give step's CV was modified
		// These cases are mutually exclusive
		
		if ((attributes[seqNum][indexTied] & ATT_MSK_TIED) != 0) {
			// copy previous CV over to current step
			int iSrc = indexTied - 1;
			if (iSrc < 0) 
				iSrc = numSteps -1;
			cv[seqNum][indexTied] = cv[seqNum][iSrc];
		}
		// Affect downstream CVs of subsequent tied note chain (can be 0 length if next note is not tied)
		int iDest = indexTied + 1;
		for (int i = 0; i < 16; i++) {// i is a safety counter in case all notes are tied (avoir infinite loop)
			if (iDest > numSteps - 1) 
				iDest = 0;
			if ((attributes[seqNum][iDest] & ATT_MSK_TIED) == 0)
				break;
			// iDest is tied, so set its CV
			cv[seqNum][iDest] = cv[seqNum][indexTied];
			iDest++;
		}
	}
};



struct PhraseSeq32Widget : ModuleWidget {

	struct SequenceDisplayWidget : TransparentWidget {
		PhraseSeq32 *module;
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
			if (module->infoCopyPaste != 0l) {
				if (module->infoCopyPaste > 0l) {// if copy display "CPY"
					snprintf(displayStr, 4, "CPY");
				}
				else {// if paste display "PST"
					snprintf(displayStr, 4, "PST");
				}
			}
			else {
				if (module->displayState == PhraseSeq32::DISP_MODE) {
					if (module->params[PhraseSeq32::EDIT_PARAM].value > 0.5f)// if editing sequence
						runModeToStr(module->runModeSeq);
					else
						runModeToStr(module->runModeSong);
				}
				else if (module->displayState == PhraseSeq32::DISP_LENGTH) {
					if (module->params[PhraseSeq32::EDIT_PARAM].value > 0.5f)// if editing sequence
						snprintf(displayStr, 4, "L%2u", module->lengths[module->sequence]);
					else
						snprintf(displayStr, 4, "L%2u", module->phrases);
				}
				else if (module->displayState == PhraseSeq32::DISP_TRANSPOSE) {
					snprintf(displayStr, 4, "+%2u", (unsigned) abs(module->transposeOffset));
					if (module->transposeOffset < 0)
						displayStr[0] = '-';
				}
				else if (module->displayState == PhraseSeq32::DISP_ROTATE) {
					snprintf(displayStr, 4, ")%2u", (unsigned) abs(module->rotateOffset));
					if (module->rotateOffset < 0)
						displayStr[0] = '(';
				}
				else {// DISP_NORMAL
					snprintf(displayStr, 4, " %2u", (unsigned) (module->params[PhraseSeq32::EDIT_PARAM].value > 0.5f ? 
						module->sequence : module->phrase[module->phraseIndexEdit]) + 1 );
				}
			}
			displayStr[3] = 0;// more safety
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	PhraseSeq32Widget(PhraseSeq32 *module) : ModuleWidget(module) {
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/PhraseSeq32.svg")));

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

		
		
		// ****** Top row ******
		
		static const int rowRulerT0 = 48;
		static const int columnRulerT0 = 40;// Step/Phase LED buttons
		static const int columnRulerT3 = 400;// Attach (also used to align rest of right side of module)

		// Step/Phrase LED buttons
		int posX = columnRulerT0;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		for (int x = 0; x < 16; x++) {
			// First row
			addParam(ParamWidget::create<LEDButton>(Vec(posX, rowRulerT0 - 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_PARAMS + x, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(posX + 4.4f, rowRulerT0 - 10 + 3), module, PhraseSeq32::STEP_PHRASE_LIGHTS + (x * 2)));
			// Second row
			addParam(ParamWidget::create<LEDButton>(Vec(posX, rowRulerT0 + 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_PARAMS + x + 16, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(posX + 4.4f, rowRulerT0 + 10 + 3), module, PhraseSeq32::STEP_PHRASE_LIGHTS + ((x + 16) * 2)));
			// step position to next location and handle groups of four
			posX += spacingSteps;
			if ((x + 1) % 4 == 0)
				posX += spacingSteps4;
		}
		// Attach button and light
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerT3 - 4, rowRulerT0 - 6 + 2 + offsetTL1105), module, PhraseSeq32::ATTACH_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerT3 + 12 + offsetMediumLight, rowRulerT0 - 6 + offsetMediumLight), module, PhraseSeq32::ATTACH_LIGHT));		

		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const float octLightsIntY = 20.0f;
		for (int i = 0; i < 7; i++) {
			addParam(ParamWidget::create<LEDButton>(Vec(15 + 3, 82 + 24 + i * octLightsIntY- 4.4f), module, PhraseSeq32::OCTAVE_PARAM + i, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(15 + 3 + 4.4f, 82 + 24 + i * octLightsIntY), module, PhraseSeq32::OCTAVE_LIGHTS + i));
		}
		// Keys and Key lights
		static const int keyNudgeX = 7;
		static const int keyNudgeY = 2;
		static const int offsetKeyLEDx = 6;
		static const int offsetKeyLEDy = 28;
		// Black keys and lights
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(65+keyNudgeX, 89+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 1, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(65+keyNudgeX+offsetKeyLEDx, 89+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 1));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(93+keyNudgeX, 89+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 3, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(93+keyNudgeX+offsetKeyLEDx, 89+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 3));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(150+keyNudgeX, 89+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 6, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(150+keyNudgeX+offsetKeyLEDx, 89+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 6));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(178+keyNudgeX, 89+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 8, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(178+keyNudgeX+offsetKeyLEDx, 89+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 8));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(206+keyNudgeX, 89+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 10, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(206+keyNudgeX+offsetKeyLEDx, 89+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 10));
		// White keys and lights
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(51+keyNudgeX, 139+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 0, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(51+keyNudgeX+offsetKeyLEDx, 139+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 0));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(79+keyNudgeX, 139+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 2, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(79+keyNudgeX+offsetKeyLEDx, 139+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 2));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(107+keyNudgeX, 139+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 4, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(107+keyNudgeX+offsetKeyLEDx, 139+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 4));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(136+keyNudgeX, 139+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 5, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(136+keyNudgeX+offsetKeyLEDx, 139+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 5));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(164+keyNudgeX, 139+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 7, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(164+keyNudgeX+offsetKeyLEDx, 139+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 7));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(192+keyNudgeX, 139+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 9, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(192+keyNudgeX+offsetKeyLEDx, 139+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 9));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(220+keyNudgeX, 139+keyNudgeY), module, PhraseSeq32::KEY_PARAMS + 11, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(220+keyNudgeX+offsetKeyLEDx, 139+keyNudgeY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 11));
		
		
		
		// ****** Right side control area ******

		static const int rowRulerMK0 = 101;// Edit mode row
		static const int rowRulerMK1 = rowRulerMK0 + 56; // Run row
		static const int rowRulerMK2 = rowRulerMK1 + 54; // Copy-paste Tran/rot row
		static const int columnRulerMK0 = 276;// Edit mode column
		static const int columnRulerMK1 = columnRulerMK0 + 59;// Display column
		static const int columnRulerMK2 = columnRulerT3;// Run mode column
		
		// Edit mode switch
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerMK0 + hOffsetCKSS, rowRulerMK0 + vOffsetCKSS), module, PhraseSeq32::EDIT_PARAM, 0.0f, 1.0f, 1.0f));
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(columnRulerMK1-15, rowRulerMK0 + 3 + vOffsetDisplay);
		displaySequence->box.size = Vec(55, 30);// 3 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Run mode button
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK0 + 0 + offsetCKD6b), module, PhraseSeq32::RUNMODE_PARAM, 0.0f, 1.0f, 0.0f));

		// Run LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerMK0 + offsetLEDbezel, rowRulerMK1 + 7 + offsetLEDbezel), module, PhraseSeq32::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerMK0 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK1 + 7 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq32::RUN_LIGHT));
		// Sequence knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(columnRulerMK1 + 1 + offsetDavies1900, rowRulerMK0 + 55 + offsetDavies1900), module, PhraseSeq32::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f));		
		// Transpose/rotate button
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK1 + 4 + offsetCKD6b), module, PhraseSeq32::TRAN_ROT_PARAM, 0.0f, 1.0f, 0.0f));
		
		
		// Copy/paste buttons
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMK1 - 10, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq32::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMK1 + 20, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq32::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		// Copy-paste mode switch (3 position)
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(columnRulerMK2 - 3 + hOffsetCKSS + 1, rowRulerMK2 - 1 + vOffsetCKSSThree), module, PhraseSeq32::CPMODE_PARAM, 0.0f, 2.0f, 2.0f));	// 0.0f is top position

		
		
		// ****** Gate and slide section ******
		
		static const int rowRulerMB0 = 214;
		static const int rowRulerMB1 = rowRulerMB0 + 55;
		static const int columnRulerMB0 = 22;
		static const int columnRulerMBspacing = 62;
		static const int columnRulerMB1 = columnRulerMB0 + 54;// Gate1 
		static const int columnRulerMB2 = columnRulerMB1 + columnRulerMBspacing;// Gate2
		static const int columnRulerMB3 = columnRulerMB2 + columnRulerMBspacing;// Slide
		static const int columnRulerMB4 = columnRulerMB3 + columnRulerMBspacing;// Tie
		static const int posLEDvsButton = + 25;
		
		// Gate 1 light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB1 - 24 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::GATE1_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB1 - 24 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::GATE1_PARAM, 0.0f, 1.0f, 0.0f));
		// Gate 2 light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB2 - 20 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::GATE2_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB2 - 20 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::GATE2_PARAM, 0.0f, 1.0f, 0.0f));
		// Slide light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB3 - 16 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::SLIDE_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB3 - 16 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f));
		// Tie light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB4 - 12 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::TIE_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB4 - 12 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::TIE_PARAM, 0.0f, 1.0f, 0.0f));

		
		// Gate 1 probability light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB1 + posLEDvsButton + offsetMediumLight, rowRulerMB1 + offsetMediumLight), module, PhraseSeq32::GATE1_PROB_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB1 + offsetCKD6b, rowRulerMB1 + offsetCKD6b), module, PhraseSeq32::GATE1_PROB_PARAM, 0.0f, 1.0f, 0.0f));
		// Gate 1 probability knob
		addParam(ParamWidget::create<RoundSmallBlackKnobB>(Vec(columnRulerMB2 - 8 + offsetRoundSmallBlackKnob, rowRulerMB1 + offsetRoundSmallBlackKnob), module, PhraseSeq32::GATE1_KNOB_PARAM, 0.0f, 1.0f, 1.0f));
		// Slide knob
		addParam(ParamWidget::create<RoundSmallBlackKnobB>(Vec(columnRulerMB3 - 16 + offsetRoundSmallBlackKnob, rowRulerMB1 + offsetRoundSmallBlackKnob), module, PhraseSeq32::SLIDE_KNOB_PARAM, 0.0f, 2.0f, 0.15f));
		
						
		
		// ****** All jacks and autostep ******
		
		static const int outputJackSpacingX = 54;
		static const int rowRulerB0 = 323;
		static const int rowRulerB1 = 269;
		static const int columnRulerB0 = columnRulerMB0;
		static const int columnRulerB1 = columnRulerB0 + outputJackSpacingX;
		static const int columnRulerB2 = columnRulerB1 + outputJackSpacingX;
		static const int columnRulerB3 = columnRulerB2 + outputJackSpacingX;
		static const int columnRulerB4 = columnRulerB3 + outputJackSpacingX;
		static const int columnRulerB7 = columnRulerMK2 + 1;
		static const int columnRulerB6 = columnRulerB7 - outputJackSpacingX;
		static const int columnRulerB5 = columnRulerB6 - outputJackSpacingX;

		// Reset LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerMB0 + offsetLEDbezel, rowRulerB1 + 1 + offsetLEDbezel), module, PhraseSeq32::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerMB0 + offsetLEDbezel + offsetLEDbezelLight, rowRulerB1 + 1 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq32::RESET_LIGHT));
		// Autostep and penultimate row jacks
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerB4 + hOffsetCKSS, rowRulerB1 + vOffsetCKSS), module, PhraseSeq32::AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f));		
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB5, rowRulerB1), Port::INPUT, module, PhraseSeq32::CV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB7, rowRulerB1), Port::INPUT, module, PhraseSeq32::RESET_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB6, rowRulerB1), Port::INPUT, module, PhraseSeq32::CLOCK_INPUT));

		

		// ****** Bottom row (all aligned) ******

	
		// CV control Inputs 
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB0, rowRulerB0), Port::INPUT, module, PhraseSeq32::SEQCV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB1, rowRulerB0), Port::INPUT, module, PhraseSeq32::LEFTCV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB2, rowRulerB0), Port::INPUT, module, PhraseSeq32::RIGHTCV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB3, rowRulerB0), Port::INPUT, module, PhraseSeq32::RUNCV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB4, rowRulerB0), Port::INPUT, module, PhraseSeq32::WRITE_INPUT));
		// Outputs
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB5, rowRulerB0), Port::OUTPUT, module, PhraseSeq32::CV_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB6, rowRulerB0), Port::OUTPUT, module, PhraseSeq32::GATE1_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB7, rowRulerB0), Port::OUTPUT, module, PhraseSeq32::GATE2_OUTPUT));

	}
};

Model *modelPhraseSeq32 = Model::create<PhraseSeq32, PhraseSeq32Widget>("Impromptu Modular", "Phrase-Seq-32", "Phrase-Seq-32", SEQUENCER_TAG);

/*CHANGE LOG

0.6.4:
initial release of PS32
*/
