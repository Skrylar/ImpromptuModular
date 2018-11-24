//***********************************************************************************************
//Multi-track multi-phrase 32 step sequencer module for VCV Rack by Marc Boulé
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
#include "PhraseSeq32ExUtil.hpp"


struct PhraseSeq32Ex : Module {
	enum ParamIds {
		ATTACH_PARAM,
		PHRASE_PARAM,
		SEQUENCE_PARAM,
		RUN_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		ENUMS(OCTAVE_PARAM, 7),
		GATE_PARAM,
		SLIDE_BTN_PARAM,
		AUTOSTEP_PARAM,
		ENUMS(KEY_PARAMS, 12),
		MODE_PARAM,
		CLKRES_PARAM,
		TRAN_ROT_PARAM,
		GATE_PROB_PARAM,
		TIE_PARAM,// Legato
		CPMODE_PARAM,
		ENUMS(STEP_PHRASE_PARAMS, SequencerKernel::MAX_STEPS),
		KEYNOTE_PARAM,
		KEYGATE_PARAM,
		TRACKDOWN_PARAM,
		TRACKUP_PARAM,
		VEL_KNOB_PARAM,
		ALLSTEPS_PARAM,
		ALLTRACKS_PARAM,
		REPS_PARAM,
		VELMODE_PARAM,
		BEGIN_PARAM,
		END_PARAM,
		LEN_PARAM,
		OVERVIEW_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		ENUMS(CV_INPUTS, 4),
		RESET_INPUT,
		CLOCK_INPUT,
		LEFTCV_INPUT,
		RIGHTCV_INPUT,
		RUNCV_INPUT,
		GATECV_INPUT,
		GATEPCV_INPUT,
		TIEDCV_INPUT,
		SLIDECV_INPUT,
		VEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),
		ENUMS(VEL_OUTPUTS, 4),
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ATTACH_LIGHT,
		ENUMS(STEP_PHRASE_LIGHTS, SequencerKernel::MAX_STEPS * 2),// room for GreenRed
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12 * 2),// room for GreenRed
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GATE_LIGHT, 2),// room for GreenRed
		SLIDE_LIGHT,
		GATE_PROB_LIGHT,
		TIE_LIGHT,
		KEYNOTE_LIGHT,
		ENUMS(KEYGATE_LIGHT, 2),// room for GreenRed
		ENUMS(OVERVIEW_LIGHT, 2),// room for GreenRed
		NUM_LIGHTS
	};
	
	// Constants
	enum EditPSDisplayStateIds {DISP_NORMAL, DISP_MODE_SEQ, DISP_MODE_SONG, DISP_LENGTH_SEQ, DISP_LENGTH_SONG, DISP_REPS, DISP_TRANSPOSE, DISP_ROTATE, DISP_PPQN};
	static const int NUM_TRACKS = 4;

	// Need to save
	int panelTheme = 0;
	int expansion = 0;
	bool running;
	bool resetOnRun;
	bool attached;
	int stepIndexEdit;
	int phraseIndexEdit;
	int trackIndexEdit;
	SequencerKernel sek[NUM_TRACKS];

	// No need to save
	long infoCopyPasteSeq;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	long infoCopyPasteSong;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	unsigned long editingGate[NUM_TRACKS];// 0 when no edit gate, downward step counter timer when edit gate
	float editingGateCV[NUM_TRACKS];// no need to initialize, this goes with editingGate (output this only when editingGate > 0)
	int editingGateKeyLight;// no need to initialize, this goes with editingGate (use this only when editingGate > 0)
	int displayState;
	float cvCPbuffer[SequencerKernel::MAX_STEPS];// copy paste buffer for CVs
	Attribute attribCPbuffer[SequencerKernel::MAX_STEPS];
	int phraseCPbuffer[SequencerKernel::MAX_PHRASES];
	int repCPbuffer[SequencerKernel::MAX_PHRASES];
	int lengthCPbuffer;// is -1 when song was copied, >0 when seq was copied
	int modeCPbuffer;
	int countCP;// number of steps to paste (in case CPMODE_PARAM changes between copy and paste)
	int startCP;
	bool seqFocusCP;// focus is on Seq (true), else Focus is on song. Used for copy paste
	int rotateOffset;// no need to initialize, this goes with displayMode = DISP_ROTATE
	long clockIgnoreOnReset;
	unsigned long clockPeriod;// counts number of step() calls upward from last clock (reset after clock processed)
	long tiedWarning;// 0 when no warning, positive downward step counter timer when warning
	long revertDisplay;
	bool keyboardEditingGates;// 0 when no info, positive when gate1
	

	unsigned int lightRefreshCounter = 0;
	float resetLight = 0.0f;
	int sequenceKnob = 0;
	int velocityKnob = 0;
	int phraseKnob = 0;
	SchmittTrigger resetTrigger;
	SchmittTrigger leftTrigger;
	SchmittTrigger rightTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger octTriggers[7];
	SchmittTrigger octmTrigger;
	SchmittTrigger gate1Trigger;
	SchmittTrigger gateProbTrigger;
	SchmittTrigger slideTrigger;
	SchmittTrigger keyTriggers[12];
	SchmittTrigger writeTrigger;
	SchmittTrigger attachedTrigger;
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger modeTrigger;
	SchmittTrigger rotateTrigger;
	SchmittTrigger transposeTrigger;
	SchmittTrigger tiedTrigger;
	SchmittTrigger stepTriggers[SequencerKernel::MAX_STEPS];
	SchmittTrigger keyNoteTrigger;
	SchmittTrigger keyGateTrigger;
	SchmittTrigger repsTrigger;
	SchmittTrigger clkResTrigger;
	SchmittTrigger trackIncTrigger;
	SchmittTrigger trackDeccTrigger;	
	SchmittTrigger beginTrigger;
	SchmittTrigger endTrigger;
	SchmittTrigger lenTrigger;
	SchmittTrigger overviewTrigger;	


	
	PhraseSeq32Ex() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].setId(trkn);
		onReset();
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		running = false;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		trackIndexEdit = 0;
		for (int phrn = 0; phrn < SequencerKernel::MAX_PHRASES; phrn++) {
			phraseCPbuffer[phrn] = 0;
			repCPbuffer[phrn] = 1;
		}
			
		for (int stepn = 0; stepn < SequencerKernel::MAX_STEPS; stepn++) {
			cvCPbuffer[stepn] = 0.0f;
			attribCPbuffer[stepn].init();
		}
		lengthCPbuffer = SequencerKernel::MAX_STEPS;
		modeCPbuffer = SequencerKernel::MODE_FWD;
		countCP = SequencerKernel::MAX_STEPS;
		startCP = 0;
		seqFocusCP = true;
		infoCopyPasteSeq = 0l;
		infoCopyPasteSong = 0l;
		displayState = DISP_NORMAL;
		attached = false;
		clockPeriod = 0ul;
		tiedWarning = 0ul;
		revertDisplay = 0l;
		resetOnRun = false;
		keyboardEditingGates = false;
		
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
			editingGate[trkn] = 0ul;
			sek[trkn].reset();
		}
		initRun(true);
	}
	
	
	void onRandomize() override {
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].randomize();	
		initRun(true);
	}
	
	
	void initRun(bool hard) {// run button activated or run edge in run input jack
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].initRun(hard);
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}	

	
	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// expansion
		json_object_set_new(rootJ, "expansion", json_integer(expansion));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// attached
		json_object_set_new(rootJ, "attached", json_boolean(attached));

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));
	
		// phraseIndexEdit
		json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

		// trackIndexEdit
		json_object_set_new(rootJ, "trackIndexEdit", json_integer(trackIndexEdit));

		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].toJson(rootJ);
		
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

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
		
		// attached
		json_t *attachedJ = json_object_get(rootJ, "attached");
		if (attachedJ)
			attached = json_is_true(attachedJ);
		
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
		
		// trackIndexEdit
		json_t *trackIndexEditJ = json_object_get(rootJ, "trackIndexEdit");
		if (trackIndexEditJ)
			trackIndexEdit = json_integer_value(trackIndexEditJ);
		
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].fromJson(rootJ);
		
		// Initialize dependants after everything loaded
		initRun(true);
	}


	void step() override {
		const float sampleRate = engineGetSampleRate();
		static const float gateTime = 0.4f;// seconds
		static const float copyPasteInfoTime = 0.5f;// seconds
		static const float revertDisplayTime = 0.7f;// seconds
		static const float tiedWarningTime = 0.7f;// seconds
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUNCV_INPUT].value)) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running)
				initRun(resetOnRun);
			displayState = DISP_NORMAL;
		}

		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {
									
			// Attach button
			if (attachedTrigger.process(params[ATTACH_PARAM].value)) {
				attached = !attached;
				displayState = DISP_NORMAL;			
			}
			if (attached) {
				stepIndexEdit = sek[trackIndexEdit].getStepIndexRun();
				phraseIndexEdit = sek[trackIndexEdit].getPhraseIndexRun();
			}
			
			// Copy 
			if (copyTrigger.process(params[COPY_PARAM].value)) {
				if (!attached) {
					infoCopyPasteSeq = (long) (copyPasteInfoTime * sampleRate / displayRefreshStepSkips);
					if (seqFocusCP) {// copying sequence steps
						startCP = stepIndexEdit;
						countCP = SequencerKernel::MAX_STEPS;
						if (params[CPMODE_PARAM].value > 1.5f)// all
							startCP = 0;
						else if (params[CPMODE_PARAM].value < 0.5f)// 4
							countCP = min(4, countCP - startCP);
						else// 8
							countCP = min(8, countCP - startCP);
						sek[trackIndexEdit].copySequence(cvCPbuffer, attribCPbuffer, &lengthCPbuffer, &modeCPbuffer, phraseIndexEdit, startCP, countCP);						
					}
					else {// copying song phrases
						startCP = phraseIndexEdit;
						countCP = SequencerKernel::MAX_PHRASES;
						if (params[CPMODE_PARAM].value > 1.5f)// all
							startCP = 0;
						else if (params[CPMODE_PARAM].value < 0.5f)// 4
							countCP = min(4, countCP - startCP);
						else// 8
							countCP = min(8, countCP - startCP);
						sek[trackIndexEdit].copyPhrase(phraseCPbuffer, repCPbuffer, &lengthCPbuffer, startCP, countCP);
					}
					displayState = DISP_NORMAL;
				}
			}
			// Paste 
			if (pasteTrigger.process(params[PASTE_PARAM].value)) {
				if (!attached) {
					infoCopyPasteSeq = -1l * (long) (copyPasteInfoTime * sampleRate / displayRefreshStepSkips);
					if (seqFocusCP) {// pasting sequence steps
						startCP = 0;
						if (countCP <= 8) {
							startCP = stepIndexEdit;
							countCP = min(countCP, SequencerKernel::MAX_STEPS - startCP);
						}
						sek[trackIndexEdit].pasteSequence(cvCPbuffer, attribCPbuffer, &lengthCPbuffer, &modeCPbuffer, phraseIndexEdit, startCP, countCP);
					}
					else {// pasting song phrases
						startCP = 0;
						if (countCP <= 8) {
							startCP = phraseIndexEdit;
							countCP = min(countCP, SequencerKernel::MAX_PHRASES - startCP);
						}
						sek[trackIndexEdit].pastePhrase(phraseCPbuffer, repCPbuffer, startCP, countCP);
					}
					displayState = DISP_NORMAL;
				}
			}			
			

			// Write input (must be before Left and Right in case route gate simultaneously to Right and Write for example)
			//  (write must be to correct step)
			bool writeTrig = writeTrigger.process(inputs[WRITE_INPUT].value);
			if (writeTrig) {
				if (!attached) {
					for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
						if (inputs[CV_INPUTS + trkn].active) {
							editingGateCV[trkn] = sek[trkn].writeCV(phraseIndexEdit, stepIndexEdit, inputs[CV_INPUTS + trkn].value);
							editingGate[trkn] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							if (inputs[VEL_INPUT].active)
								sek[trkn].setVelocity(phraseIndexEdit, stepIndexEdit, (int)(inputs[VEL_INPUT].value));
						}
					}
					editingGateKeyLight = -1;
					// Autostep (after grab all active inputs)
					if (params[AUTOSTEP_PARAM].value > 0.5f) {
						stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + 1, SequencerKernel::MAX_STEPS);
					}
				}
				displayState = DISP_NORMAL;
			}
			// Left and right CV inputs
			int delta = 0;
			if (leftTrigger.process(inputs[LEFTCV_INPUT].value)) { 
				delta = -1;
			}
			if (rightTrigger.process(inputs[RIGHTCV_INPUT].value)) {
				delta = +1;
			}
			if (!attached && delta != 0) {
				if (displayState == DISP_NORMAL) {
					stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + delta, SequencerKernel::MAX_STEPS);
					for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
						if (!sek[trkn].getTied(phraseIndexEdit, stepIndexEdit)) {// play if non-tied step
							if (!writeTrig) {// in case autostep when simultaneous writeCV and stepCV (keep what was done in Write Input block above)
								editingGate[trkn] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
								editingGateCV[trkn] = sek[trkn].getCV(phraseIndexEdit, stepIndexEdit);
								editingGateKeyLight = -1;
							}
						}
					}
				}
			}

			// Step button presses
			int stepPressed = -1;
			for (int i = 0; i < SequencerKernel::MAX_STEPS; i++) {
				if (stepTriggers[i].process(params[STEP_PHRASE_PARAMS + i].value))
					stepPressed = i;
			}
			if (!attached && stepPressed != -1) {
				if (displayState == DISP_LENGTH_SEQ) {
					sek[trackIndexEdit].setLength(phraseIndexEdit, stepPressed + 1);
					revertDisplay = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
				}
				else {
					stepIndexEdit = stepPressed;
					if (!sek[trackIndexEdit].getTied(phraseIndexEdit,stepIndexEdit)) {// play if non-tied step
						editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
						editingGateCV[trackIndexEdit] = sek[trackIndexEdit].getCV(phraseIndexEdit, stepIndexEdit);
						editingGateKeyLight = -1;
					}
					displayState = DISP_NORMAL;
				}
			} 
			
			// Mode button
			if (modeTrigger.process(params[MODE_PARAM].value)) {
				if (!attached) {
					if (displayState != DISP_MODE_SEQ && displayState != DISP_MODE_SONG)
						displayState = DISP_MODE_SONG;
					else if (displayState == DISP_MODE_SONG)
						displayState = DISP_MODE_SEQ;
					else
						displayState = DISP_NORMAL;
				}
			}
			
			// Clk res button
			if (clkResTrigger.process(params[CLKRES_PARAM].value)) {
				if (!attached) {
					if (displayState != DISP_PPQN)	
						displayState = DISP_PPQN;
					else
						displayState = DISP_NORMAL;
				}
			}
			
			// Transpose/Rotate button
			if (transposeTrigger.process(params[TRAN_ROT_PARAM].value)) {
				if (!attached) {
					if (displayState != DISP_TRANSPOSE && displayState != DISP_ROTATE) {
						displayState = DISP_TRANSPOSE;
					}
					else if (displayState == DISP_TRANSPOSE) {
						displayState = DISP_ROTATE;
						rotateOffset = 0;
					}
					else 
						displayState = DISP_NORMAL;
				}
			}			

			// Begin button
			if (beginTrigger.process(params[BEGIN_PARAM].value)) {
				if (!attached) {
					sek[trackIndexEdit].setBegin(phraseIndexEdit);
				}
			}	
			// End button
			if (endTrigger.process(params[END_PARAM].value)) {
				if (!attached) {
					sek[trackIndexEdit].setEnd(phraseIndexEdit);
				}
			}	

			// Reps button
			if (repsTrigger.process(params[REPS_PARAM].value)) {
				if (!attached) {
					if (displayState != DISP_REPS)
						displayState = DISP_REPS;
					else 
						displayState = DISP_NORMAL;
				}
			}	
			// Len button
			if (lenTrigger.process(params[LEN_PARAM].value)) {
				if (!attached) {
					if (displayState != DISP_LENGTH_SEQ && displayState != DISP_LENGTH_SONG)
						displayState = DISP_LENGTH_SEQ;
					else if (displayState == DISP_LENGTH_SEQ)
						displayState = DISP_LENGTH_SONG;
					else
						displayState = DISP_NORMAL;
				}
			}	

			// Track Inc/Dec triggers
			if (trackIncTrigger.process(params[TRACKUP_PARAM].value)) {
				if (trackIndexEdit < (NUM_TRACKS - 1)) 
					trackIndexEdit++;
			}
			if (trackDeccTrigger.process(params[TRACKDOWN_PARAM].value)) {
				if (trackIndexEdit > 0) 
					trackIndexEdit--;
			}
			
			// Velocity knob 
			float velParamValue = params[VEL_KNOB_PARAM].value;
			int newVelocityKnob = (int)roundf(velParamValue * 30.0f);
			if (velParamValue == 0.0f)// true when constructor or fromJson() occured
				velocityKnob = newVelocityKnob;
			int deltaVelKnob = newVelocityKnob - velocityKnob;
			if (deltaVelKnob != 0) {
				if (abs(deltaVelKnob) <= 3) {// avoid discontinuous step (initialize for example)
					if (!attached) {
						if (params[VELMODE_PARAM].value > 1.5f) {
							sek[trackIndexEdit].modSlideVal(phraseIndexEdit, stepIndexEdit, deltaVelKnob);
						}
						else if (params[VELMODE_PARAM].value > 0.5f) {
							sek[trackIndexEdit].modGatePVal(phraseIndexEdit, stepIndexEdit, deltaVelKnob);
						}
						else {
							sek[trackIndexEdit].modVelocityVal(phraseIndexEdit, stepIndexEdit, deltaVelKnob);
						}
					}
				}
				velocityKnob = newVelocityKnob;
			}	

						
			// Phrase edit knob 
			float phraseParamValue = params[PHRASE_PARAM].value;
			int newPhraseKnob = (int)roundf(phraseParamValue * 7.0f);
			if (phraseParamValue == 0.0f)// true when constructor or fromJson() occured
				phraseKnob = newPhraseKnob;
			int deltaPhrKnob = newPhraseKnob - phraseKnob;
			if (deltaPhrKnob != 0) {
				if (abs(deltaPhrKnob) <= 3) {// avoid discontinuous step (initialize for example)
					if (!attached) {
						if (displayState == DISP_PPQN) {
							sek[trackIndexEdit].modPulsesPerStepIndex(deltaPhrKnob);
							if (sek[trackIndexEdit].getPulsesPerStep() == 1)
								keyboardEditingGates = false;
						}
						else if (displayState == DISP_MODE_SONG) {
							sek[trackIndexEdit].modRunModeSong(deltaPhrKnob);
						}
						else if (displayState == DISP_LENGTH_SONG) {
							sek[trackIndexEdit].modPhrases(deltaPhrKnob);
						}
						else {
							phraseIndexEdit += deltaPhrKnob;
							if (phraseIndexEdit < 0) phraseIndexEdit = 0;
							if (phraseIndexEdit >= SequencerKernel::MAX_PHRASES) phraseIndexEdit = (SequencerKernel::MAX_PHRASES - 1);
						}
					}
				}
				phraseKnob = newPhraseKnob;
			}	
				
				
			// Sequence edit knob 
			float seqParamValue = params[SEQUENCE_PARAM].value;
			int newSequenceKnob = (int)roundf(seqParamValue * 7.0f);
			if (seqParamValue == 0.0f)// true when constructor or fromJson() occured
				sequenceKnob = newSequenceKnob;
			int deltaSeqKnob = newSequenceKnob - sequenceKnob;
			if (deltaSeqKnob != 0) {
				if (abs(deltaSeqKnob) <= 3) {// avoid discontinuous step (initialize for example)
					if (!attached) {
						if (displayState == DISP_MODE_SEQ) {
							sek[trackIndexEdit].modRunModeSeq(phraseIndexEdit, deltaSeqKnob);
						}
						else if (displayState == DISP_LENGTH_SEQ) {
							sek[trackIndexEdit].modLength(phraseIndexEdit, deltaSeqKnob);
						}
						else if (displayState == DISP_TRANSPOSE) {
							sek[trackIndexEdit].transposeSeq(phraseIndexEdit, deltaSeqKnob);
						}
						else if (displayState == DISP_ROTATE) {
							sek[trackIndexEdit].rotateSeq(&rotateOffset, phraseIndexEdit, deltaSeqKnob);
						}					
						else if (displayState == DISP_REPS) {
							sek[trackIndexEdit].modPhraseReps(phraseIndexEdit, deltaSeqKnob);
						}
						else {// DISP_NORMAL
							sek[trackIndexEdit].modPhrase(phraseIndexEdit, deltaSeqKnob);
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
				if (!attached) {
					if (sek[trackIndexEdit].getTied(phraseIndexEdit, stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else {			
						editingGateCV[trackIndexEdit] = sek[trackIndexEdit].applyNewOctave(phraseIndexEdit, stepIndexEdit, newOct);
						editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
						editingGateKeyLight = -1;
					}
				}
				displayState = DISP_NORMAL;
			}		
			
			// Keyboard buttons
			for (int i = 0; i < 12; i++) {
				if (keyTriggers[i].process(params[KEY_PARAMS + i].value)) {
					if (!attached) {
						if (keyboardEditingGates) {
							int newMode = sek[trackIndexEdit].keyIndexToGateTypeEx(i);
							if (newMode != -1)
								sek[trackIndexEdit].setGateType(phraseIndexEdit, stepIndexEdit, newMode);
						}
						else if (sek[trackIndexEdit].getTied(phraseIndexEdit, stepIndexEdit)) {
							if (params[KEY_PARAMS + i].value > 1.5f)
								stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + 1, SequencerKernel::MAX_STEPS);
							else
								tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
						}
						else {	
							editingGateCV[trackIndexEdit] = sek[trackIndexEdit].applyNewKey(phraseIndexEdit, stepIndexEdit, i);
							editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							editingGateKeyLight = -1;
							if (params[KEY_PARAMS + i].value > 1.5f) {// if right-click then move to next step
								stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + 1, SequencerKernel::MAX_STEPS);
								editingGateKeyLight = i;
							}
						}						
					}
					displayState = DISP_NORMAL;
				}
			}
			
			// Keyboard mode (note or gate type)
			if (keyNoteTrigger.process(params[KEYNOTE_PARAM].value)) {
				if (!attached) {
					keyboardEditingGates = false;
				}
				displayState = DISP_NORMAL;
			}
			if (keyGateTrigger.process(params[KEYGATE_PARAM].value)) {
				if (!attached) {
					if (sek[trackIndexEdit].getPulsesPerStep() != 1) {
						keyboardEditingGates = true;
					}
				}
				displayState = DISP_NORMAL;
			}

			// Gate, GateProb, Slide and Tied buttons
			if (gate1Trigger.process(params[GATE_PARAM].value + inputs[GATECV_INPUT].value)) {
				if (!attached) {
					sek[trackIndexEdit].toggleGate(phraseIndexEdit, stepIndexEdit);
				}
				displayState = DISP_NORMAL;
			}		
			if (gateProbTrigger.process(params[GATE_PROB_PARAM].value + inputs[GATEPCV_INPUT].value)) {
				if (!attached) {
					if (sek[trackIndexEdit].getTied(phraseIndexEdit,stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else {
						sek[trackIndexEdit].toggleGateP(phraseIndexEdit, stepIndexEdit);
					}
				}
				displayState = DISP_NORMAL;
			}		
			if (slideTrigger.process(params[SLIDE_BTN_PARAM].value + inputs[SLIDECV_INPUT].value)) {
				if (!attached) {
					if (sek[trackIndexEdit].getTied(phraseIndexEdit,stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else {
						sek[trackIndexEdit].toggleSlide(phraseIndexEdit, stepIndexEdit);
					}
				}
				displayState = DISP_NORMAL;
			}		
			if (tiedTrigger.process(params[TIE_PARAM].value + inputs[TIEDCV_INPUT].value)) {
				if (!attached) {
					sek[trackIndexEdit].toggleTied(phraseIndexEdit, stepIndexEdit);// will clear other attribs if new state is on
				}
				displayState = DISP_NORMAL;
			}		
			
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			if (running && clockIgnoreOnReset == 0l) {
				for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
					sek[trkn].clockStep(clockPeriod);
			}
			clockPeriod = 0ul;
		}
		clockPeriod++;
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			initRun(true);
			resetLight = 1.0f;
			displayState = DISP_NORMAL;
		}
		
		
		//********** Outputs and lights **********
				
		
		
		// CV and gates outputs
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
			if (running) {
				int phrn = sek[trkn].getPhraseIndexRun();
				int stepn = sek[trkn].getStepIndexRun();
				outputs[CV_OUTPUTS + trkn].value = sek[trkn].getCV(phrn, stepn) - sek[trkn].calcSlideOffset();
				outputs[GATE_OUTPUTS + trkn].value = (sek[trkn].calcGate(clockTrigger, clockPeriod, sampleRate) ? 10.0f : 0.0f);
				outputs[VEL_OUTPUTS + trkn].value = sek[trkn].getVelocity(phrn, stepn);
			}
			else {// not running 
				outputs[CV_OUTPUTS + trkn].value = (editingGate[trkn] > 0ul) ? editingGateCV[trkn] : sek[trkn].getCV(phraseIndexEdit, stepIndexEdit);
				outputs[GATE_OUTPUTS + trkn].value = (editingGate[trkn] > 0ul) ? 10.0f : 0.0f;
				outputs[VEL_OUTPUTS + trkn].value = (editingGate[trkn] > 0ul) ? 10.0f : sek[trkn].getVelocity(phraseIndexEdit, stepIndexEdit);
			}
			sek[trkn].decSlideStepsRemain();
		}

		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;
		
			// Step/phrase lights
			if (infoCopyPasteSeq != 0l) {
				for (int i = 0; i < SequencerKernel::MAX_STEPS; i++) {
					if (i >= startCP && i < (startCP + countCP))
						lights[STEP_PHRASE_LIGHTS + (i<<1)].value = 0.5f;// Green when copy interval
					else
						lights[STEP_PHRASE_LIGHTS + (i<<1)].value = 0.0f; // Green (nothing)
					lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = 0.0f;// Red (nothing)
				}
			}
			else {
				for (int i = 0; i < SequencerKernel::MAX_STEPS; i++) {
					if (displayState == DISP_LENGTH_SEQ) {
						int seqEnd = sek[trackIndexEdit].getLength(phraseIndexEdit) - 1;
						if (i < seqEnd)
							setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 0.1f, 0.0f);
						else if (i == seqEnd)
							setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 1.0f, 0.0f);
						else 
							setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 0.0f, 0.0f);
					}
					else {// normal led display (i.e. not length)
						float red = 0.0f;
						float green = 0.0f;
						// Run cursor (green)
						// if (running)
							// green = (i == sek[trackIndexEdit].getStepIndexRun() ? 0.1f : 0.0f);
						// Edit cursor (red)
						if (i == stepIndexEdit) {
							red = 1.0f;
							// green = 0.0f;
						}
						setGreenRed(STEP_PHRASE_LIGHTS + i * 2, green, red);
					}
				}
			}
		
			// Octave lights
			float octKeyCV = sek[trackIndexEdit].getCV(phraseIndexEdit, stepIndexEdit);// used for keyboard lights also
			int octLightIndex = (int) floor(octKeyCV + 3.0f);
			for (int i = 0; i < 7; i++) {
				float red = 0.0f;
				if (tiedWarning > 0l) {
					bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
					red = (warningFlashState && (i == (6 - octLightIndex))) ? 1.0f : 0.0f;
				}
				else				
					red = (i == (6 - octLightIndex) ? 1.0f : 0.0f);// no lights when outside of range
				lights[OCTAVE_LIGHTS + i].value = red;
			}
			
			// Keyboard lights
			octKeyCV += 10.0f;// to properly handle negative note voltages
			int keyLightIndex = (int) clamp(  roundf( (octKeyCV-floor(octKeyCV)) * 12.0f ),  0.0f,  11.0f);
			if (keyboardEditingGates) {
				int modeLightIndex = sek[trackIndexEdit].getGateType(phraseIndexEdit, stepIndexEdit);
				for (int i = 0; i < 12; i++) {
					if (i == modeLightIndex)
						setGreenRed(KEY_LIGHTS + i * 2, 1.0f, 1.0f);
					else
						setGreenRed(KEY_LIGHTS + i * 2, 0.0f, i == keyLightIndex ? 0.1f : 0.0f);
				}
			}
			else {
				for (int i = 0; i < 12; i++) {
					float red = 0.0f;
					float green = 0.0f;
					if (tiedWarning > 0l) {
						bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
						red = (warningFlashState && i == keyLightIndex) ? 1.0f : 0.0f;
					}
					else {
						if (editingGate[trackIndexEdit] > 0ul && editingGateKeyLight != -1)
							red = (i == editingGateKeyLight ? ((float) editingGate[trackIndexEdit] / (float)(gateTime * sampleRate / displayRefreshStepSkips)) : 0.0f);
						else
							red = (i == keyLightIndex ? 1.0f : 0.0f);
					}
					setGreenRed(KEY_LIGHTS + i * 2, green, red);
				}
			}		

			// Gate, GateProb, Slide and Tied lights 
			Attribute attributesVal = sek[trackIndexEdit].getAttribute(phraseIndexEdit, stepIndexEdit);
			if (!attributesVal.getGate()) 
				setGreenRed(GATE_LIGHT, 0.0f, 0.0f);
			else if (sek[trackIndexEdit].getPulsesPerStep() == 1) 
				setGreenRed(GATE_LIGHT, 0.0f, 1.0f);
			else 
				setGreenRed(GATE_LIGHT, 1.0f, 1.0f);
			lights[GATE_PROB_LIGHT].value = attributesVal.getGateP() ? 1.0f : 0.0f;
			lights[SLIDE_LIGHT].value = attributesVal.getSlide() ? 1.0f : 0.0f;
			if (tiedWarning > 0l) {
				bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
				lights[TIE_LIGHT].value = (warningFlashState) ? 1.0f : 0.0f;
			}
			else
				lights[TIE_LIGHT].value = attributesVal.getTied() ? 1.0f : 0.0f;			
			
			// Key mode light (note or gate type)
			lights[KEYNOTE_LIGHT].value = (!keyboardEditingGates) ? 10.0f : 0.0f;
			if (!keyboardEditingGates)
				setGreenRed(KEYGATE_LIGHT, 0.0f, 0.0f);
			else
				setGreenRed(KEYGATE_LIGHT, 1.0f, 1.0f);
			
			// Attach light
			lights[ATTACH_LIGHT].value = (attached ? 1.0f : 0.0f);		

			// Reset light
			lights[RESET_LIGHT].value =	resetLight;
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime() * displayRefreshStepSkips;
			
			// Run light
			lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;

			for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
				if (editingGate[trkn] > 0ul)
					editingGate[trkn]--;
			}
			if (infoCopyPasteSeq != 0l) {
				if (infoCopyPasteSeq > 0l) infoCopyPasteSeq --;
				if (infoCopyPasteSeq < 0l) infoCopyPasteSeq ++;
			}
			if (infoCopyPasteSong != 0l) {
				if (infoCopyPasteSong > 0l) infoCopyPasteSong --;
				if (infoCopyPasteSong < 0l) infoCopyPasteSong ++;
			}
			if (tiedWarning > 0l)
				tiedWarning--;
			if (revertDisplay > 0l) {
				if (revertDisplay == 1)
					displayState = DISP_NORMAL;
				revertDisplay--;
			}
		}// lightRefreshCounter
				
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
		
	}// step()
	

	inline void setGreenRed(int id, float green, float red) {
		lights[id + 0].value = green;
		lights[id + 1].value = red;
	}
};



struct PhraseSeq32ExWidget : ModuleWidget {
	PhraseSeq32Ex *module;
	DynamicSVGPanel *panel;
	int oldExpansion;
	int expWidth = 60;
	IMPort* expPorts[5];
	
	template <int NUMCHAR>
	struct DisplayWidget : TransparentWidget {// a centered display, must derive from this
		PhraseSeq32Ex *module;
		std::shared_ptr<Font> font;
		char displayStr[NUMCHAR+1];
		static const int textFontSize = 14;
		static constexpr float textOffsetY = 18.2f;
		
		void runModeToStr(int num) {
			if (num >= 0 && num < SequencerKernel::NUM_MODES)
				snprintf(displayStr, 4, "%s", SequencerKernel::modeLabels[num].c_str());
		}

		DisplayWidget(Vec _pos, Vec _size, PhraseSeq32Ex *_module) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box, textFontSize);
			nvgFontFaceId(vg, font->handle);
			nvgTextLetterSpacing(vg, -0.5);

			Vec textPos = Vec(4.5f, textOffsetY);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			std::string initString(NUMCHAR,'~');
			nvgText(vg, textPos.x, textPos.y, initString.c_str(), NULL);
			nvgFillColor(vg, textColor);
			printText();
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
		
		virtual void printText() = 0;
	};
	
	struct VelocityDisplayWidget : DisplayWidget<3> {
		VelocityDisplayWidget(Vec _pos, Vec _size, PhraseSeq32Ex *_module) : DisplayWidget(_pos, _size, _module) {};
		void printText() override {
			if (module->params[PhraseSeq32Ex::VELMODE_PARAM].value > 1.5f) {
				int slide = module->sek[module->trackIndexEdit].getSlideVal(module->phraseIndexEdit, module->stepIndexEdit);
				if ( slide>= 100)
					snprintf(displayStr, 4, "1,0");
				else if (slide >= 10)
					snprintf(displayStr, 4, ",%2u", (unsigned) slide);
				else if (slide >= 1)
					snprintf(displayStr, 4, " ,%1u", (unsigned) slide);
				else
					snprintf(displayStr, 4, "  0");
			}
			else if (module->params[PhraseSeq32Ex::VELMODE_PARAM].value > 0.5f) {
				int prob = module->sek[module->trackIndexEdit].getGatePVal(module->phraseIndexEdit, module->stepIndexEdit);
				if ( prob>= 100)
					snprintf(displayStr, 4, "1,0");
				else if (prob >= 10)
					snprintf(displayStr, 4, ",%2u", (unsigned) prob);
				else if (prob >= 1)
					snprintf(displayStr, 4, " ,%1u", (unsigned) prob);
				else
					snprintf(displayStr, 4, "  0");
			}
			else {
				Attribute attributesVal = module->sek[module->trackIndexEdit].getAttribute(module->phraseIndexEdit, module->stepIndexEdit);
				snprintf(displayStr, 4, "%3u", (unsigned)(attributesVal.getVelocityVal()));
			}
		}
	};
	
	struct TrackDisplayWidget : DisplayWidget<3> {
		TrackDisplayWidget(Vec _pos, Vec _size, PhraseSeq32Ex *_module) : DisplayWidget(_pos, _size, _module) {};
		void printText() override {
			snprintf(displayStr, 4, " %2u", (unsigned)(module->trackIndexEdit + 1));
		}
	};
	

	struct PhrEditDisplayWidget : DisplayWidget<3> {
		PhrEditDisplayWidget(Vec _pos, Vec _size, PhraseSeq32Ex *_module) : DisplayWidget(_pos, _size, _module) {};

		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box, textFontSize);
			nvgFontFaceId(vg, font->handle);
			nvgTextLetterSpacing(vg, -0.5);

			Vec textPos = Vec(4.5f, textOffsetY);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			std::string initString(3,'~');
			nvgText(vg, textPos.x, textPos.y, initString.c_str(), NULL);
			nvgFillColor(vg, textColor);
			bool beginEnd = printTextBE();
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
			if (beginEnd)
				nvgText(vg, textPos.x, textPos.y, ")", NULL);
		}

		void printText() override {};
		bool printTextBE() {
			bool beginEnd = false;// special return code to print an end symbols overlaped (begin done in here)
			if (module->infoCopyPasteSong != 0l) {
				if (module->infoCopyPasteSong > 0l)
					snprintf(displayStr, 4, "CPY");
				else {
					snprintf(displayStr, 4, "PST");
				}
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_PPQN) {
				snprintf(displayStr, 4, "x%2u", (unsigned) module->sek[module->trackIndexEdit].getPulsesPerStep());
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_MODE_SONG) {
				runModeToStr(module->sek[module->trackIndexEdit].getRunModeSong());
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_LENGTH_SONG) {
				snprintf(displayStr, 4, "L%2u", (unsigned) module->sek[module->trackIndexEdit].getPhrases());
			}
			else {// DISP_NORMAL
				int phrn = module->phraseIndexEdit;
				snprintf(displayStr, 4, " %2u", (unsigned)(phrn + 1));
				bool begHere = (phrn == module->sek[module->trackIndexEdit].getBegin());
				bool endHere = (phrn == module->sek[module->trackIndexEdit].getEnd());
				if (begHere) {
					displayStr[0] = '(';
					if (endHere)
						beginEnd = true;
				}
				else if (endHere)
					displayStr[0] = ')';
			}
			return beginEnd;
		}
	};
	
	
	struct SeqEditDisplayWidget : DisplayWidget<3> {
		SeqEditDisplayWidget(Vec _pos, Vec _size, PhraseSeq32Ex *_module) : DisplayWidget(_pos, _size, _module) {};
		void printText() override {
			if (module->infoCopyPasteSeq != 0l) {
				if (module->infoCopyPasteSeq > 0l)
					snprintf(displayStr, 4, "CPY");
				else {
					snprintf(displayStr, 4, "PST");
				}
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_MODE_SEQ) {
				runModeToStr(module->sek[module->trackIndexEdit].getRunModeSeq(module->phraseIndexEdit));
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_LENGTH_SEQ) {
				snprintf(displayStr, 4, "L%2u", (unsigned) module->sek[module->trackIndexEdit].getLength(module->phraseIndexEdit));
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_TRANSPOSE) {
				int tranOffset = module->sek[module->trackIndexEdit].getTransposeOffset(module->phraseIndexEdit);
				snprintf(displayStr, 4, "+%2u", (unsigned) abs(tranOffset));
				if (tranOffset < 0)
					displayStr[0] = '-';
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_ROTATE) {
				snprintf(displayStr, 4, ")%2u", (unsigned) abs(module->rotateOffset));
				if (module->rotateOffset < 0)
					displayStr[0] = '(';
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_REPS) {
				snprintf(displayStr, 4, "R%2u", (unsigned) abs(module->sek[module->trackIndexEdit].getPhraseReps(module->phraseIndexEdit)));
			}
			else {// DISP_NORMAL
				int seqn = module->sek[module->trackIndexEdit].getPhrase(module->phraseIndexEdit);
				snprintf(displayStr, 4, " %2u", (unsigned)(seqn + 1) );
			}
		}
	};
	

	struct PanelThemeItem : MenuItem {
		PhraseSeq32Ex *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	struct ExpansionItem : MenuItem {
		PhraseSeq32Ex *module;
		void onAction(EventAction &e) override {
			module->expansion = module->expansion == 1 ? 0 : 1;
		}
	};
	struct ResetOnRunItem : MenuItem {
		PhraseSeq32Ex *module;
		void onAction(EventAction &e) override {
			module->resetOnRun = !module->resetOnRun;
		}
	};
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		PhraseSeq32Ex *module = dynamic_cast<PhraseSeq32Ex*>(this->module);
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
				for (int i = 0; i < 5; i++)
					gRackWidget->wireContainer->removeAllWires(expPorts[i]);
			}
			oldExpansion = module->expansion;		
		}
		box.size.x = panel->box.size.x - (1 - module->expansion) * expWidth;
		Widget::step();
	}
		
	PhraseSeq32ExWidget(PhraseSeq32Ex *module) : ModuleWidget(module) {
		this->module = module;
		oldExpansion = -1;
		
		// Main panel from Inkscape
        panel = new DynamicSVGPanel();
        panel->mode = &module->panelTheme;
		panel->expWidth = &expWidth;
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/PhraseSeq32Ex.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/PhraseSeq32Ex.svg")));
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

		
		
		// ****** Top row ******
		
		static const int rowRulerT0 = 56;
		static const int columnRulerT0 = 25;// Step/Phase LED buttons
		static const int columnRulerT1 = 373;// All 
		static const int columnRulerT4 = 492;// Copy-paste
		static const int columnRulerT5 = 536;// Attach
		static const int stepsOffsetY = 10;

		// Step/Phrase LED buttons
		int posX = columnRulerT0;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		const int numX = SequencerKernel::MAX_STEPS / 2;
		for (int x = 0; x < numX; x++) {
			// First row
			addParam(createParamCentered<LEDButton>(Vec(posX, rowRulerT0 - stepsOffsetY), module, PhraseSeq32Ex::STEP_PHRASE_PARAMS + x, 0.0f, 1.0f, 0.0f));
			addChild(createLightCentered<MediumLight<GreenRedLight>>(Vec(posX, rowRulerT0 - stepsOffsetY), module, PhraseSeq32Ex::STEP_PHRASE_LIGHTS + (x * 2)));
			// Second row
			addParam(createParamCentered<LEDButton>(Vec(posX, rowRulerT0 + stepsOffsetY), module, PhraseSeq32Ex::STEP_PHRASE_PARAMS + x + numX, 0.0f, 1.0f, 0.0f));
			addChild(createLightCentered<MediumLight<GreenRedLight>>(Vec(posX, rowRulerT0 + stepsOffsetY), module, PhraseSeq32Ex::STEP_PHRASE_LIGHTS + ((x + numX) * 2)));
			// step position to next location and handle groups of four
			posX += spacingSteps;
			if ((x + 1) % 4 == 0)
				posX += spacingSteps4;
		}
		// AllSteps button
		addParam(createDynamicParamCentered<IMPushButton>(Vec(columnRulerT1, rowRulerT0 - stepsOffsetY), module, PhraseSeq32Ex::ALLSTEPS_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// AllTracks button
		addParam(createDynamicParamCentered<IMPushButton>(Vec(columnRulerT1, rowRulerT0 + stepsOffsetY), module, PhraseSeq32Ex::ALLTRACKS_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Copy/paste buttons
		static const int cpButtonsOffsetX = 66;
		addParam(createDynamicParamCentered<IMPushButton>(Vec(columnRulerT4 - cpButtonsOffsetX - 10, rowRulerT0), module, PhraseSeq32Ex::COPY_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(columnRulerT4 - cpButtonsOffsetX + 20, rowRulerT0), module, PhraseSeq32Ex::PASTE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Copy-paste mode switch (3 position)
		addParam(createParamCentered<CKSSThreeInv>(Vec(columnRulerT4, rowRulerT0 - 5), module, PhraseSeq32Ex::CPMODE_PARAM, 0.0f, 2.0f, 2.0f));	// 0.0f is top position
		// Attach button and light
		addParam(createDynamicParamCentered<IMPushButton>(Vec(columnRulerT5 - 8, rowRulerT0), module, PhraseSeq32Ex::ATTACH_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerT5 + 12, rowRulerT0), module, PhraseSeq32Ex::ATTACH_LIGHT));		
		
		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const int octLightsIntY = 20;
		static const int rowRulerOct = 111;
		for (int i = 0; i < 7; i++) {
			addParam(createParamCentered<LEDButton>(Vec(columnRulerT0, rowRulerOct + i * octLightsIntY), module, PhraseSeq32Ex::OCTAVE_PARAM + i, 0.0f, 1.0f, 0.0f));
			addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerT0, rowRulerOct + i * octLightsIntY), module, PhraseSeq32Ex::OCTAVE_LIGHTS + i));
		}
		
		// Keys and Key lights
		static const int keyNudgeX = 2;
		static const int KeyBlackY = 103;
		static const int KeyWhiteY = 141;
		static const int offsetKeyLEDx = 6;
		static const int offsetKeyLEDy = 16;
		// Black keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(65+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 1, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(65+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 1 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(93+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 3, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(93+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 3 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(150+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 6, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(150+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 6 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(178+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 8, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(178+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 8 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(206+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 10, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(206+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 10 * 2));
		// White keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(51+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 0, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(51+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 0 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(79+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 2, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(79+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 2 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(107+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 4, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(107+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 4 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(136+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 5, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(136+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 5 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(164+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 7, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(164+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 7 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(192+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 9, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(192+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 9 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(220+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 11, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(220+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 11 * 2));

		// Key mode LED buttons	
		static const int rowRulerKM = rowRulerOct + 5 * octLightsIntY;
		static const int colRulerKM = 57;
		addParam(createParamCentered<LEDButton>(Vec(colRulerKM, rowRulerKM), module, PhraseSeq32Ex::KEYNOTE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(colRulerKM, rowRulerKM), module, PhraseSeq32Ex::KEYNOTE_LIGHT));
		addParam(createParamCentered<LEDButton>(Vec(colRulerKM, rowRulerKM + 24), module, PhraseSeq32Ex::KEYGATE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(Vec(colRulerKM, rowRulerKM + 24), module, PhraseSeq32Ex::KEYGATE_LIGHT));
		
		
		// ****** Gate and slide section ******
		
		static const int rowRulerMB0 = rowRulerOct + 6 * octLightsIntY;
		static const int columnRulerMB1 = 115;// Gate 
		static const int columnRulerMBspacing = 67;
		static const int columnRulerMB2 = columnRulerMB1 + columnRulerMBspacing;// Tie
		static const int columnRulerMB3 = columnRulerMB2 + columnRulerMBspacing;// GateP
		static const int columnRulerMB4 = columnRulerMB3 + columnRulerMBspacing;// Slide
		static const int columnRulerMB5 = columnRulerMB4 + columnRulerMBspacing;// Tran/Rot
		static const int columnRulerMB6 = columnRulerMB5 + columnRulerMBspacing - 8;// RunModes
		static const int columnRulerMB7 = columnRulerMB6 + columnRulerMBspacing - 8;// Overview

		static const int posLEDvsButton = + 26;
		
		// Gate 1 light and button
		addChild(createLightCentered<MediumLight<GreenRedLight>>(Vec(columnRulerMB1 + posLEDvsButton, rowRulerMB0), module, PhraseSeq32Ex::GATE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB1, rowRulerMB0), module, PhraseSeq32Ex::GATE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Tie light and button
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerMB2 + posLEDvsButton, rowRulerMB0), module, PhraseSeq32Ex::TIE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB2, rowRulerMB0), module, PhraseSeq32Ex::TIE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Gate 1 probability light and button
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerMB3 + posLEDvsButton, rowRulerMB0), module, PhraseSeq32Ex::GATE_PROB_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB3, rowRulerMB0), module, PhraseSeq32Ex::GATE_PROB_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Slide light and button
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerMB4 + posLEDvsButton, rowRulerMB0), module, PhraseSeq32Ex::SLIDE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB4, rowRulerMB0), module, PhraseSeq32Ex::SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Transpose/rotate button
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB5, rowRulerMB0), module, PhraseSeq32Ex::TRAN_ROT_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Mode button
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB6, rowRulerMB0), module, PhraseSeq32Ex::MODE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Overview
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerMB7 + posLEDvsButton, rowRulerMB0), module, PhraseSeq32Ex::OVERVIEW_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB7, rowRulerMB0), module, PhraseSeq32Ex::OVERVIEW_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));

	
	
		// ****** Right side control area ******
		
		static const int rowRulerDisp = 110;
		static const int rowRulerKnobs = 145;
		static const int rowRulerSmallButtons = 189;
		static const int displayWidths = 43;
		static const int displayHeights = 22;
		static const int displaySpacingX = 61;

		// Velocity display
		static const int colRulerVel = 288;
		addChild(new VelocityDisplayWidget(Vec(colRulerVel, rowRulerDisp), Vec(displayWidths, displayHeights), module));// 3 characters
		// Velocity knob
		addParam(createDynamicParamCentered<IMMediumKnobInf>(Vec(colRulerVel, rowRulerKnobs), module, PhraseSeq32Ex::VEL_KNOB_PARAM, -INFINITY, INFINITY, 0.0f, &module->panelTheme));	
		// Veocity mode switch (3 position)
		addParam(createParamCentered<CKSSHThree>(Vec(colRulerVel, rowRulerSmallButtons), module, PhraseSeq32Ex::VELMODE_PARAM, 0.0f, 2.0f, 0.0f));	// 0.0f is top position
		

		// Track display
		static const int colRulerTrk = colRulerVel + displaySpacingX;
		static const int trkButtonsOffsetX = 12;
		addChild(new TrackDisplayWidget(Vec(colRulerTrk, rowRulerDisp), Vec(displayWidths, displayHeights), module));// 2 characters
		// Track buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk + trkButtonsOffsetX, rowRulerKnobs), module, PhraseSeq32Ex::TRACKUP_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk - trkButtonsOffsetX, rowRulerKnobs), module, PhraseSeq32Ex::TRACKDOWN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Clk res
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk, rowRulerSmallButtons), module, PhraseSeq32Ex::CLKRES_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
			
			
		// Phrase edit display 
		static const int colRulerEditPhr = colRulerTrk + displaySpacingX;
		addChild(new PhrEditDisplayWidget(Vec(colRulerEditPhr, rowRulerDisp), Vec(displayWidths, displayHeights), module));// 5 characters
		// Phrase knob
		addParam(createDynamicParamCentered<IMMediumKnobInf>(Vec(colRulerEditPhr, rowRulerKnobs), module, PhraseSeq32Ex::PHRASE_PARAM, -INFINITY, INFINITY, 0.0f, &module->panelTheme));		
		// Begin/end buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditPhr - trkButtonsOffsetX, rowRulerSmallButtons), module, PhraseSeq32Ex::BEGIN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditPhr + trkButtonsOffsetX, rowRulerSmallButtons), module, PhraseSeq32Ex::END_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));

				
		// Seq edit display 
		static const int colRulerEditSeq = colRulerEditPhr + displaySpacingX;
		addChild(new SeqEditDisplayWidget(Vec(colRulerEditSeq, rowRulerDisp), Vec(displayWidths, displayHeights), module));// 5 characters
		// Sequence-edit knob
		addParam(createDynamicParamCentered<IMMediumKnobInf>(Vec(colRulerEditSeq, rowRulerKnobs), module, PhraseSeq32Ex::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f, &module->panelTheme));		
		// Len and Reps button
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditSeq - trkButtonsOffsetX, rowRulerSmallButtons), module, PhraseSeq32Ex::REPS_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditSeq + trkButtonsOffsetX, rowRulerSmallButtons), module, PhraseSeq32Ex::LEN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
	
			
		// Reset and run LED buttons
		static const int colRulerResetRun = columnRulerT5;
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerResetRun, rowRulerDisp + 10), module, PhraseSeq32Ex::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerResetRun, rowRulerDisp + 10), module, PhraseSeq32Ex::RUN_LIGHT));
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerResetRun, rowRulerKnobs + 23), module, PhraseSeq32Ex::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerResetRun, rowRulerKnobs + 23), module, PhraseSeq32Ex::RESET_LIGHT));

	
						
		
		// ****** Bottom two rows ******
		
		static const int rowRulerBLow = 335;
		static const int rowRulerBHigh = 286;
		
		static const int inputJackSpacingX = 50;
		static const int columnRulerB0 = 28;
		static const int columnRulerB1 = columnRulerB0 + inputJackSpacingX;
		static const int columnRulerB2 = columnRulerB1 + inputJackSpacingX;
		static const int columnRulerB3 = columnRulerB2 + inputJackSpacingX;
		static const int columnRulerB4 = columnRulerB3 + inputJackSpacingX;
		static const int columnRulerB5 = columnRulerB4 + inputJackSpacingX;// clock and reset
		
		static const int outputJackSpacingX = 42;
		static const int columnRulerB6 = columnRulerB5 + inputJackSpacingX - 2;// outputs
		static const int columnRulerB7 = columnRulerB6 + outputJackSpacingX;// outputs
		static const int columnRulerB8 = columnRulerB7 + outputJackSpacingX;// outputs
		static const int columnRulerB9 = columnRulerB8 + outputJackSpacingX + 3;// outputs
		static const int columnRulerB10 = columnRulerB9 + outputJackSpacingX;// outputs
		static const int columnRulerB11 = columnRulerB10 + outputJackSpacingX;// outputs
		
		// Step arrow CV inputs
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB0, rowRulerBHigh), Port::INPUT, module, PhraseSeq32Ex::LEFTCV_INPUT, &module->panelTheme));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB0, rowRulerBLow), Port::INPUT, module, PhraseSeq32Ex::RIGHTCV_INPUT, &module->panelTheme));

		// Autostep and write
		addParam(createParamCentered<CKSS>(Vec(columnRulerB1, rowRulerBHigh), module, PhraseSeq32Ex::AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f));		
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB1, rowRulerBLow), Port::INPUT, module, PhraseSeq32Ex::WRITE_INPUT, &module->panelTheme));
	
		// CV IN inputs
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB2, rowRulerBHigh), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB3, rowRulerBHigh), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 1, &module->panelTheme));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB2, rowRulerBLow), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 3, &module->panelTheme));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB3, rowRulerBLow), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 4, &module->panelTheme));
		
		// Velocity input and run CV input
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB4, rowRulerBHigh), Port::INPUT, module, PhraseSeq32Ex::VEL_INPUT, &module->panelTheme));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB4, rowRulerBLow), Port::INPUT, module, PhraseSeq32Ex::RUNCV_INPUT, &module->panelTheme));
		
		
		// Clock and reset inputs
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB5, rowRulerBHigh), Port::INPUT, module, PhraseSeq32Ex::CLOCK_INPUT, &module->panelTheme));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB5, rowRulerBLow), Port::INPUT, module, PhraseSeq32Ex::RESET_INPUT, &module->panelTheme));

		// CV+Gate+Vel outputs
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB6, rowRulerBHigh), Port::OUTPUT, module, PhraseSeq32Ex::CV_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB7, rowRulerBHigh), Port::OUTPUT, module, PhraseSeq32Ex::GATE_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB8, rowRulerBHigh), Port::OUTPUT, module, PhraseSeq32Ex::VEL_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB9, rowRulerBHigh), Port::OUTPUT, module, PhraseSeq32Ex::CV_OUTPUTS + 1, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB10, rowRulerBHigh), Port::OUTPUT, module, PhraseSeq32Ex::GATE_OUTPUTS + 1, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB11, rowRulerBHigh), Port::OUTPUT, module, PhraseSeq32Ex::VEL_OUTPUTS + 1, &module->panelTheme));
		//
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB6, rowRulerBLow), Port::OUTPUT, module, PhraseSeq32Ex::CV_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB7, rowRulerBLow), Port::OUTPUT, module, PhraseSeq32Ex::GATE_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB8, rowRulerBLow), Port::OUTPUT, module, PhraseSeq32Ex::VEL_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB9, rowRulerBLow), Port::OUTPUT, module, PhraseSeq32Ex::CV_OUTPUTS + 3, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB10, rowRulerBLow), Port::OUTPUT, module, PhraseSeq32Ex::GATE_OUTPUTS + 3, &module->panelTheme));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB11, rowRulerBLow), Port::OUTPUT, module, PhraseSeq32Ex::VEL_OUTPUTS + 3, &module->panelTheme));


		
		
		// Expansion module
		static const int rowRulerExpTop = 78;
		static const int rowSpacingExp = 60;
		static const int colRulerExp = panel->box.size.x - expWidth / 2;
		addInput(expPorts[0] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 0), Port::INPUT, module, PhraseSeq32Ex::GATECV_INPUT, &module->panelTheme));
		addInput(expPorts[1] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 1), Port::INPUT, module, PhraseSeq32Ex::GATEPCV_INPUT, &module->panelTheme));
		addInput(expPorts[2] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 2), Port::INPUT, module, PhraseSeq32Ex::TIEDCV_INPUT, &module->panelTheme));
		addInput(expPorts[3] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 3), Port::INPUT, module, PhraseSeq32Ex::SLIDECV_INPUT, &module->panelTheme));
		//addInput(expPorts[4] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 4), Port::INPUT, module, PhraseSeq32Ex::SEQCV_INPUT, &module->panelTheme)); // no longer used
	}
};

Model *modelPhraseSeq32Ex = Model::create<PhraseSeq32Ex, PhraseSeq32ExWidget>("Impromptu Modular", "Phrase-Seq-32Ex", "SEQ - Phrase-Seq-32Ex", SEQUENCER_TAG);

/*CHANGE LOG

0.6.13:
created

*/
