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
		EDIT_PARAM,
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
		TRACKDOWN_PARAM,
		TRACKUP_PARAM,
		VEL_KNOB_PARAM,
		ALLSTEPS_PARAM,
		ALLTRACKS_PARAM,
		REP_LEN_PARAM,
		VELMODE_PARAM,
		BEGIN_PARAM,
		END_PARAM,
		KEY_GATE_PARAM,
		ATTACH_PARAM,
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
		SEQCV_INPUT,
		TRKCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),
		ENUMS(VEL_OUTPUTS, 4),
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, SequencerKernel::MAX_STEPS * 2),// room for GreenRed
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12 * 2),// room for GreenRed
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GATE_LIGHT, 2),// room for GreenRed
		SLIDE_LIGHT,
		GATE_PROB_LIGHT,
		TIE_LIGHT,
		ATTACH_LIGHT,
		NUM_LIGHTS
	};
	
	// Constants
	enum EditPSDisplayStateIds {DISP_NORMAL, DISP_MODE_SEQ, DISP_MODE_SONG, DISP_LEN, DISP_REPS, DISP_TRANSPOSE, DISP_ROTATE, DISP_PPQN, DISP_DELAY, DISP_COPY_SEQ, DISP_PASTE_SEQ, DISP_COPY_SONG, DISP_PASTE_SONG};
	enum MainSwitchIds {MAIN_EDIT_SEQ, MAIN_EDIT_SONG, MAIN_SHOW_RUN};
	static const int NUM_TRACKS = 4;

	// Need to save
	int panelTheme = 0;
	int expansion = 0;
	bool autoseq;
	bool showSharp = true;
	int seqCVmethod = 0;// 0 is 0-10V, 1 is C2-D7#, 2 is TrigIncr
	bool running;
	bool resetOnRun;
	bool attached;
	int stepIndexEdit;
	int seqIndexEdit;// used in edit Seq mode only
	int phraseIndexEdit;// used in edit Song mode only
	int trackIndexEdit;
	SequencerKernel sek[NUM_TRACKS];

	// No need to save
	unsigned long editingGate[NUM_TRACKS];// 0 when no edit gate, downward step counter timer when edit gate
	float editingGateCV[NUM_TRACKS];// no need to initialize, this goes with editingGate (output this only when editingGate > 0)
	int editingGateKeyLight;// no need to initialize, this goes with editingGate (use this only when editingGate > 0)
	int displayState;
	float cvCPbuffer[SequencerKernel::MAX_STEPS];// copy paste buffer for CVs
	StepAttributes attribCPbuffer[SequencerKernel::MAX_STEPS];
	Phrase phraseCPbuffer[SequencerKernel::MAX_PHRASES];
	int repCPbuffer[SequencerKernel::MAX_PHRASES];
	SeqAttributes seqPhraseAttribCPbuffer;// transpose is -1 when seq was copied, holds songEnd when song was copied; len holds songBeg when song
	int countCP;// number of steps to paste (in case CPMODE_PARAM changes between copy and paste)
	int startCP;
	int rotateOffset;// no need to initialize, this goes with displayMode = DISP_ROTATE
	long clockIgnoreOnReset;
	unsigned long clockPeriod;// counts number of step() calls upward from last clock (reset after clock processed)
	long tiedWarning;// 0 when no warning, positive downward step counter timer when warning
	long revertDisplay;
	long showLenInSteps;
	

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
	SchmittTrigger keyTriggers[12];
	SchmittTrigger octTriggers[7];
	SchmittTrigger gate1Trigger;
	SchmittTrigger tiedTrigger;
	SchmittTrigger gateProbTrigger;
	SchmittTrigger slideTrigger;
	SchmittTrigger writeTrigger;
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger modeTrigger;
	SchmittTrigger rotateTrigger;
	SchmittTrigger transposeTrigger;
	SchmittTrigger stepTriggers[SequencerKernel::MAX_STEPS];
	SchmittTrigger clkResTrigger;
	SchmittTrigger trackIncTrigger;
	SchmittTrigger trackDeccTrigger;	
	SchmittTrigger beginTrigger;
	SchmittTrigger endTrigger;
	SchmittTrigger repLenTrigger;
	SchmittTrigger attachedTrigger;
	SchmittTrigger seqCVTrigger;

	
	inline bool isEditingSequence(void) {return params[EDIT_PARAM].value > 0.5f;}
	inline bool isEditingGates(void) {return params[KEY_GATE_PARAM].value < 0.5f;}

	
	PhraseSeq32Ex() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		sek[0].setSlaveRndLastPtrs(nullptr, nullptr);
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
			sek[trkn].setId(trkn);
			if (trkn > 0)
				sek[trkn].setSlaveRndLastPtrs(sek[0].getSeqRndLast(), sek[0].getSongRndLast());
		}
		onReset();
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		autoseq = false;
		running = false;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		seqIndexEdit = 0;
		trackIndexEdit = 0;
		for (int phrn = 0; phrn < SequencerKernel::MAX_PHRASES; phrn++) {
			phraseCPbuffer[phrn].init();
		}
			
		for (int stepn = 0; stepn < SequencerKernel::MAX_STEPS; stepn++) {
			cvCPbuffer[stepn] = 0.0f;
			attribCPbuffer[stepn].init();
		}
		seqPhraseAttribCPbuffer.init(SequencerKernel::MAX_STEPS, SequencerKernel::MODE_FWD);
		seqPhraseAttribCPbuffer.setTranspose(-1);
		countCP = SequencerKernel::MAX_STEPS;
		startCP = 0;
		displayState = DISP_NORMAL;
		clockPeriod = 0ul;
		tiedWarning = 0ul;
		revertDisplay = 0l;
		showLenInSteps = 0l;
		resetOnRun = false;
		attached = false;
		
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
			editingGate[trkn] = 0ul;
			sek[trkn].reset();
		}
		initRun();
	}
	
	
	void onRandomize() override {
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].randomize();	
		initRun();
	}
	
	
	void initRun() {// run button activated or run edge in run input jack
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].initRun();
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
		
		// showSharp
		json_object_set_new(rootJ, "showSharp", json_boolean(showSharp));
		
		// seqCVmethod
		json_object_set_new(rootJ, "seqCVmethod", json_integer(seqCVmethod));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// attached
		json_object_set_new(rootJ, "attached", json_boolean(attached));

		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));
	
		// seqIndexEdit
		json_object_set_new(rootJ, "seqIndexEdit", json_integer(seqIndexEdit));

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

		// autoseq
		json_t *autoseqJ = json_object_get(rootJ, "autoseq");
		if (autoseqJ)
			autoseq = json_is_true(autoseqJ);

		// showSharp
		json_t *showSharpJ = json_object_get(rootJ, "showSharp");
		if (showSharpJ)
			showSharp = json_is_true(showSharpJ);
		
		// seqCVmethod
		json_t *seqCVmethodJ = json_object_get(rootJ, "seqCVmethod");
		if (seqCVmethodJ)
			seqCVmethod = json_integer_value(seqCVmethodJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
		
		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// attached
		json_t *attachedJ = json_object_get(rootJ, "attached");
		if (attachedJ)
			attached = json_is_true(attachedJ);
		
		// stepIndexEdit
		json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
		if (stepIndexEditJ)
			stepIndexEdit = json_integer_value(stepIndexEditJ);
		
		// phraseIndexEdit
		json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
		if (phraseIndexEditJ)
			phraseIndexEdit = json_integer_value(phraseIndexEditJ);
		
		// seqIndexEdit
		json_t *seqIndexEditJ = json_object_get(rootJ, "seqIndexEdit");
		if (seqIndexEditJ)
			seqIndexEdit = json_integer_value(seqIndexEditJ);
		
		// trackIndexEdit
		json_t *trackIndexEditJ = json_object_get(rootJ, "trackIndexEdit");
		if (trackIndexEditJ)
			trackIndexEdit = json_integer_value(trackIndexEditJ);
		
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].fromJson(rootJ);
		
		// Initialize dependants after everything loaded
		initRun();
	}


	void step() override {
		const float sampleRate = engineGetSampleRate();
		static const float gateTime = 0.4f;// seconds
		static const float revertDisplayTime = 0.7f;// seconds
		static const float tiedWarningTime = 0.7f;// seconds
		static const float showLenInStepsTime = 2.0f;// seconds
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		bool editingSequence = isEditingSequence();
		
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUNCV_INPUT].value)) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running && resetOnRun)
				initRun();
			displayState = DISP_NORMAL;
		}

		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {
			
			// Seq CV input
			if (inputs[SEQCV_INPUT].active) {
				if (seqCVmethod == 0) {// 0-10 V
					int newSeq = (int)( inputs[SEQCV_INPUT].value * ((float)SequencerKernel::MAX_SEQS - 1.0f) / 10.0f + 0.5f );
					seqIndexEdit = clamp(newSeq, 0, SequencerKernel::MAX_SEQS - 1);
				}
				else if (seqCVmethod == 1) {// C2-D7#
					int newSeq = (int)( (inputs[SEQCV_INPUT].value + 2.0f) * 12.0f + 0.5f );
					seqIndexEdit = clamp(newSeq, 0, SequencerKernel::MAX_SEQS - 1);
				}
				else {// TrigIncr
					if (seqCVTrigger.process(inputs[SEQCV_INPUT].value))
						seqIndexEdit = clamp(seqIndexEdit + 1, 0, SequencerKernel::MAX_SEQS - 1);
				}	
			}
			
			// Track CV input
			if (inputs[TRKCV_INPUT].active) {
				int newTrk = (int)( inputs[TRKCV_INPUT].value * ((float)NUM_TRACKS - 1.0f) / 10.0f + 0.5f );
				trackIndexEdit = clamp(newTrk, 0, NUM_TRACKS - 1);
			}
			
			// Attach button
			if (attachedTrigger.process(params[ATTACH_PARAM].value)) {
				attached = !attached;
				displayState = DISP_NORMAL;			
			}
			if (running && attached) {
				phraseIndexEdit = sek[trackIndexEdit].getPhraseIndexRun();
				seqIndexEdit = sek[trackIndexEdit].getPhraseSeq(phraseIndexEdit);
				stepIndexEdit = sek[trackIndexEdit].getStepIndexRun();
			}
			
	
			// Copy 
			if (copyTrigger.process(params[COPY_PARAM].value)) {
				if (!attached) {
					revertDisplay = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
					if (editingSequence) {// copying sequence steps
						startCP = stepIndexEdit;
						countCP = SequencerKernel::MAX_STEPS;
						if (params[CPMODE_PARAM].value > 1.5f)// all
							startCP = 0;
						else if (params[CPMODE_PARAM].value < 0.5f)// 4
							countCP = min(4, countCP - startCP);
						else// 8
							countCP = min(8, countCP - startCP);
						sek[trackIndexEdit].copySequence(cvCPbuffer, attribCPbuffer, &seqPhraseAttribCPbuffer, seqIndexEdit, startCP, countCP);						
						displayState = DISP_COPY_SEQ;
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
						sek[trackIndexEdit].copyPhrase(phraseCPbuffer, &seqPhraseAttribCPbuffer, startCP, countCP);
						displayState = DISP_COPY_SONG;
					}
				}
			}
			// Paste 
			if (pasteTrigger.process(params[PASTE_PARAM].value)) {
				if (!attached) {
					if (editingSequence && seqPhraseAttribCPbuffer.getTranspose() == -1) {// pasting sequence steps
						startCP = 0;
						if (countCP <= 8) {
							startCP = stepIndexEdit;
							countCP = min(countCP, SequencerKernel::MAX_STEPS - startCP);
						}
						sek[trackIndexEdit].pasteSequence(cvCPbuffer, attribCPbuffer, &seqPhraseAttribCPbuffer, seqIndexEdit, startCP, countCP);
						displayState = DISP_PASTE_SEQ;
						revertDisplay = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
					}
					else if (!editingSequence && seqPhraseAttribCPbuffer.getTranspose() != -1) {// pasting song phrases
						startCP = 0;
						if (countCP <= 8) {
							startCP = phraseIndexEdit;
							countCP = min(countCP, SequencerKernel::MAX_PHRASES - startCP);
						}
						sek[trackIndexEdit].pastePhrase(phraseCPbuffer, &seqPhraseAttribCPbuffer, startCP, countCP);
						displayState = DISP_PASTE_SONG;
						revertDisplay = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
					}
				}
			}			
			

			// Write input (must be before Left and Right in case route gate simultaneously to Right and Write for example)
			//  (write must be to correct step)
			bool writeTrig = writeTrigger.process(inputs[WRITE_INPUT].value);
			if (writeTrig) {
				if (editingSequence && ! attached) {
					for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
						if (inputs[CV_INPUTS + trkn].active) {
							editingGateCV[trkn] = sek[trkn].writeCV(seqIndexEdit, stepIndexEdit, inputs[CV_INPUTS + trkn].value);
							editingGate[trkn] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							if (inputs[VEL_INPUT].active)
								sek[trkn].setVelocity(seqIndexEdit, stepIndexEdit, (int)(inputs[VEL_INPUT].value));
						}
					}
					editingGateKeyLight = -1;
					// Autostep (after grab all active inputs)
					if (params[AUTOSTEP_PARAM].value > 0.5f) {
						stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + 1, SequencerKernel::MAX_STEPS);
						if (stepIndexEdit == 0 && autoseq && !inputs[SEQCV_INPUT].active)
							seqIndexEdit = moveIndexEx(seqIndexEdit, seqIndexEdit + 1, SequencerKernel::MAX_SEQS);			
					}
				}
			}
			// Left and right CV inputs
			int delta = 0;
			if (leftTrigger.process(inputs[LEFTCV_INPUT].value)) { 
				delta = -1;
			}
			if (rightTrigger.process(inputs[RIGHTCV_INPUT].value)) {
				delta = +1;
			}
			if (delta != 0) {
				if (editingSequence && !attached) {
					if (displayState == DISP_NORMAL) {
						stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + delta, SequencerKernel::MAX_STEPS);
						for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
							if (!sek[trkn].getTied(seqIndexEdit, stepIndexEdit)) {// play if non-tied step
								if (!writeTrig) {// in case autostep when simultaneous writeCV and stepCV (keep what was done in Write Input block above)
									editingGate[trkn] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
									editingGateCV[trkn] = sek[trkn].getCV(seqIndexEdit, stepIndexEdit);
									editingGateKeyLight = -1;
								}
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
			if (stepPressed != -1) {
				if (editingSequence && !attached) {
					if (displayState == DISP_LEN) {
						sek[trackIndexEdit].setLength(seqIndexEdit, stepPressed + 1);
						revertDisplay = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
					}
					else {
						showLenInSteps = (long) (showLenInStepsTime * sampleRate / displayRefreshStepSkips);
						stepIndexEdit = stepPressed;
						if (!sek[trackIndexEdit].getTied(seqIndexEdit,stepIndexEdit)) {// play if non-tied step
							editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							editingGateCV[trackIndexEdit] = sek[trackIndexEdit].getCV(seqIndexEdit, stepIndexEdit);
							editingGateKeyLight = -1;
						}
					}
					displayState = DISP_NORMAL;
				}
			} 
			
			// Mode button
			if (modeTrigger.process(params[MODE_PARAM].value)) {
				if (!attached) {
					if (displayState != DISP_MODE_SEQ && displayState != DISP_MODE_SONG)
						displayState = editingSequence ? DISP_MODE_SEQ : DISP_MODE_SONG;
					else
						displayState = DISP_NORMAL;
				}
			}
			
			// Clk res/delay button
			if (clkResTrigger.process(params[CLKRES_PARAM].value)) {
				if (!attached) {
					if (displayState != DISP_PPQN && displayState != DISP_DELAY)	
						displayState = DISP_PPQN;
					else if (displayState == DISP_PPQN)
						displayState = DISP_DELAY;
					else
						displayState = DISP_NORMAL;
				}
			}
			
			// Transpose/Rotate button
			if (transposeTrigger.process(params[TRAN_ROT_PARAM].value)) {
				if (editingSequence && !attached) {
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

			// Begin/End buttons
			if (beginTrigger.process(params[BEGIN_PARAM].value)) {
				if (!editingSequence && !attached) {
					sek[trackIndexEdit].setBegin(phraseIndexEdit);
					displayState = DISP_NORMAL;
				}
			}	
			if (endTrigger.process(params[END_PARAM].value)) {
				if (!editingSequence && !attached) {
					sek[trackIndexEdit].setEnd(phraseIndexEdit);
					displayState = DISP_NORMAL;
				}
			}	

			// Rep/Len button
			if (repLenTrigger.process(params[REP_LEN_PARAM].value)) {
				if (!attached) {
					if (displayState != DISP_LEN && displayState != DISP_REPS)
						displayState = editingSequence ? DISP_LEN : DISP_REPS;
					else
						displayState = DISP_NORMAL;
				}
			}	

			// Track Inc/Dec buttons
			if (trackIncTrigger.process(params[TRACKUP_PARAM].value)) {
				if (!inputs[TRKCV_INPUT].active) {
					if (trackIndexEdit < (NUM_TRACKS - 1)) 
						trackIndexEdit++;
					else
						trackIndexEdit = 0;
				}
			}
			if (trackDeccTrigger.process(params[TRACKDOWN_PARAM].value)) {
				if (!inputs[TRKCV_INPUT].active) {
					if (trackIndexEdit > 0) 
						trackIndexEdit--;
					else
						trackIndexEdit = NUM_TRACKS - 1;
				}
			}
			
		
			// Velocity knob 
			float velParamValue = params[VEL_KNOB_PARAM].value;
			int newVelocityKnob = (int)roundf(velParamValue * 30.0f);
			if (velParamValue == 0.0f)// true when constructor or fromJson() occured
				velocityKnob = newVelocityKnob;
			int deltaVelKnob = newVelocityKnob - velocityKnob;
			if (deltaVelKnob != 0) {
				if (abs(deltaVelKnob) <= 3) {// avoid discontinuous step (initialize for example)
					if (editingSequence && !attached) {
						if (params[VELMODE_PARAM].value > 1.5f) {
							sek[trackIndexEdit].modSlideVal(seqIndexEdit, stepIndexEdit, deltaVelKnob);
						}
						else if (params[VELMODE_PARAM].value > 0.5f) {
							sek[trackIndexEdit].modGatePVal(seqIndexEdit, stepIndexEdit, deltaVelKnob);
						}
						else {
							sek[trackIndexEdit].modVelocityVal(seqIndexEdit, stepIndexEdit, deltaVelKnob);
						}
						displayState = DISP_NORMAL;
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
					if (displayState == DISP_PPQN) {
						sek[trackIndexEdit].modPulsesPerStep(deltaPhrKnob);
					}
					else if (displayState == DISP_DELAY) {
						sek[trackIndexEdit].modDelay(deltaPhrKnob);
					}
					else if (displayState == DISP_MODE_SONG) {
						sek[trackIndexEdit].modRunModeSong(deltaPhrKnob);
					}
					else if (!editingSequence && !attached) {
						phraseIndexEdit = moveIndexEx(phraseIndexEdit, phraseIndexEdit + deltaPhrKnob, SequencerKernel::MAX_PHRASES);
						displayState = DISP_NORMAL;
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
					if (displayState == DISP_MODE_SEQ) {
						sek[trackIndexEdit].modRunModeSeq(seqIndexEdit, deltaSeqKnob);
					}
					else if (displayState == DISP_LEN) {
						sek[trackIndexEdit].modLength(seqIndexEdit, deltaSeqKnob);
					}
					else if (displayState == DISP_TRANSPOSE) {
						sek[trackIndexEdit].transposeSeq(seqIndexEdit, deltaSeqKnob);
					}
					else if (displayState == DISP_ROTATE) {
						sek[trackIndexEdit].rotateSeq(&rotateOffset, seqIndexEdit, deltaSeqKnob);
					}							
					else if (displayState == DISP_REPS) {
						sek[trackIndexEdit].modPhraseReps(phraseIndexEdit, deltaSeqKnob);
					}
					else if (!attached) {
						if (editingSequence) {
							if (!inputs[SEQCV_INPUT].active)
								seqIndexEdit = moveIndexEx(seqIndexEdit, seqIndexEdit + deltaSeqKnob, SequencerKernel::MAX_SEQS);
						}
						else {// editing song
							sek[trackIndexEdit].modPhraseSeqNum(phraseIndexEdit, deltaSeqKnob);
						}
						displayState = DISP_NORMAL;
					}
				}
				sequenceKnob = newSequenceKnob;
			}	
			
			// Octave buttons
			for (int i = 0; i < 7; i++) {
				if (octTriggers[i].process(params[OCTAVE_PARAM + i].value)) {
					if (editingSequence && !attached && displayState != DISP_PPQN) {
						if (sek[trackIndexEdit].getTied(seqIndexEdit, stepIndexEdit)) {
							tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
						}
						else {			
							editingGateCV[trackIndexEdit] = sek[trackIndexEdit].applyNewOctave(seqIndexEdit, stepIndexEdit, 6 - i);
							editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							editingGateKeyLight = -1;
						}
					}
					displayState = DISP_NORMAL;
				}
			}
			
			// Keyboard buttons
			for (int i = 0; i < 12; i++) {
				if (keyTriggers[i].process(params[KEY_PARAMS + i].value)) {
					displayState = DISP_NORMAL;
					if (editingSequence && !attached && displayState != DISP_PPQN) {
						if (isEditingGates()) {
							int newMode = sek[trackIndexEdit].keyIndexToGateTypeEx(i);
							if (newMode != -1)
								sek[trackIndexEdit].setGateType(seqIndexEdit, stepIndexEdit, newMode);
							else
								displayState = DISP_PPQN;
						}
						else if (sek[trackIndexEdit].getTied(seqIndexEdit, stepIndexEdit)) {
							if (params[KEY_PARAMS + i].value > 1.5f)
								stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + 1, SequencerKernel::MAX_STEPS);
							else
								tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
						}
						else {	
							editingGateCV[trackIndexEdit] = sek[trackIndexEdit].applyNewKey(seqIndexEdit, stepIndexEdit, i);
							editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							editingGateKeyLight = -1;
							if (params[KEY_PARAMS + i].value > 1.5f) {// if right-click then move to next step
								stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + 1, SequencerKernel::MAX_STEPS);
								editingGateKeyLight = i;
							}
						}						
					}
				}
			}
			
			// Gate, GateProb, Slide and Tied buttons
			if (gate1Trigger.process(params[GATE_PARAM].value + inputs[GATECV_INPUT].value)) {
				if (editingSequence && !attached ) {
					sek[trackIndexEdit].toggleGate(seqIndexEdit, stepIndexEdit);
				}
				displayState = DISP_NORMAL;
			}		
			if (gateProbTrigger.process(params[GATE_PROB_PARAM].value + inputs[GATEPCV_INPUT].value)) {
				if (editingSequence && !attached ) {
					if (sek[trackIndexEdit].getTied(seqIndexEdit,stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else {
						sek[trackIndexEdit].toggleGateP(seqIndexEdit, stepIndexEdit);
					}
				}
				displayState = DISP_NORMAL;
			}		
			if (slideTrigger.process(params[SLIDE_BTN_PARAM].value + inputs[SLIDECV_INPUT].value)) {
				if (editingSequence && !attached ) {
					if (sek[trackIndexEdit].getTied(seqIndexEdit,stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else {
						sek[trackIndexEdit].toggleSlide(seqIndexEdit, stepIndexEdit);
					}
				}
				displayState = DISP_NORMAL;
			}		
			if (tiedTrigger.process(params[TIE_PARAM].value + inputs[TIEDCV_INPUT].value)) {
				if (editingSequence && !attached ) {
					sek[trackIndexEdit].toggleTied(seqIndexEdit, stepIndexEdit);// will clear other attribs if new state is on
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
			initRun();
			resetLight = 1.0f;
			displayState = DISP_NORMAL;
			if (inputs[SEQCV_INPUT].active && seqCVmethod == 2)
				seqIndexEdit = 0;
		}
		
		
		//********** Outputs and lights **********
				
		
		
		// CV and gates outputs
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
			if (running) {
				outputs[CV_OUTPUTS + trkn].value = sek[trkn].getCVRun() - sek[trkn].calcSlideOffset();
				outputs[GATE_OUTPUTS + trkn].value = (sek[trkn].calcGate(clockTrigger, clockPeriod, sampleRate) ? 10.0f : 0.0f);
				outputs[VEL_OUTPUTS + trkn].value = sek[trkn].getVelocityRun();
			}
			else {// not running 
				outputs[CV_OUTPUTS + trkn].value = (editingGate[trkn] > 0ul) ? editingGateCV[trkn] : sek[trkn].getCV(seqIndexEdit, stepIndexEdit);
				outputs[GATE_OUTPUTS + trkn].value = (editingGate[trkn] > 0ul) ? 10.0f : 0.0f;
				outputs[VEL_OUTPUTS + trkn].value = (editingGate[trkn] > 0ul) ? 10.0f : sek[trkn].getVelocity(seqIndexEdit, stepIndexEdit);
			}
			sek[trkn].decSlideStepsRemain();
		}

		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;
		
			// Step/phrase lights
			for (int stepn = 0; stepn < SequencerKernel::MAX_STEPS; stepn++) {
				float red = 0.0f;
				float green = 0.0f;		
				if ((displayState == DISP_COPY_SEQ) || (displayState == DISP_PASTE_SEQ)) {
					if (stepn >= startCP && stepn < (startCP + countCP))
						green = 0.5f;// Green when copy interval
				}
				else if (displayState == DISP_LEN) {
					int seqEnd = sek[trackIndexEdit].getLength(seqIndexEdit) - 1;
					if (stepn < seqEnd)
						green = 0.1f;
					else if (stepn == seqEnd)
						green =  1.0f;
				}				
				else if (editingSequence && !attached) {
					if (stepn == stepIndexEdit) {
						red = 1.0f;
					}
					else if (!attached && showLenInSteps > 0l && stepn < sek[trackIndexEdit].getLength(seqIndexEdit)) {
						green = 0.01f;
					}
				}
				else if (attached) {
					// all active light green, current track is bright yellow
					for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
						if (stepn == sek[trkn].getStepIndexRun()) 
							green = 0.05f;	
					}
					if (green > 0.1f) 
						green = 0.1f;
					if (stepn == stepIndexEdit) {
						green = 1.0f;// this makes it yellow since already red in code above
						red = 1.0f;
					}
				}
				setGreenRed(STEP_PHRASE_LIGHTS + stepn * 2, green, red);
			}
			
			
			// Prepare values to visualize
			StepAttributes attributesVisual;
			float cvVisual;
			if (editingSequence || attached) {
				attributesVisual = sek[trackIndexEdit].getAttribute(seqIndexEdit, stepIndexEdit);
				cvVisual = sek[trackIndexEdit].getCV(seqIndexEdit, stepIndexEdit);
			}
			else {
				attributesVisual.clear();// clears everything, but just buttons used below
				cvVisual = 0.0f;// not used
			}

			
			// Octave lights
			int octLightIndex = (int) floor(cvVisual + 3.0f);
			for (int i = 0; i < 7; i++) {
				float red = 0.0f;
				if (editingSequence || attached) {
					if (tiedWarning > 0l) {
						bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
						red = (warningFlashState && (i == (6 - octLightIndex))) ? 1.0f : 0.0f;
					}
					else				
						red = (i == (6 - octLightIndex) ? 1.0f : 0.0f);// no lights when outside of range
				}
				lights[OCTAVE_LIGHTS + i].value = red;
			}
			
			// Keyboard lights
			float keyCV = cvVisual + 10.0f;// to properly handle negative note voltages
			int keyLightIndex = clamp( (int)((keyCV - floor(keyCV)) * 12.0f + 0.5f),  0,  11);
			for (int i = 0; i < 12; i++) {
				float red = 0.0f;
				float green = 0.0f;
				if (displayState == DISP_PPQN) {
					if (sek[trackIndexEdit].keyIndexToGateTypeEx(i) != -1) {
						red = 1.0f;
						green =	1.0f;
					}
				}
				else if (editingSequence || attached) {			
					if (isEditingGates()) {
						int modeLightIndex = sek[trackIndexEdit].getGateType(seqIndexEdit, stepIndexEdit);
						if (i == modeLightIndex) {
							red = 1.0f;
							green =	1.0f;
						}
						else
							red = (i == keyLightIndex ? 0.1f : 0.0f);
					}
					else {
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
					}
				}
				setGreenRed(KEY_LIGHTS + i * 2, green, red);
			}

			// Gate, GateProb, Slide and Tied lights 
			if (attributesVisual.getGate())
				setGreenRed(GATE_LIGHT, (sek[trackIndexEdit].getPulsesPerStep() == 1 ? 0.0f : 1.0f), 1.0f);
			else 
				setGreenRed(GATE_LIGHT, 0.0f, 0.0f);
			lights[GATE_PROB_LIGHT].value = attributesVisual.getGateP() ? 1.0f : 0.0f;
			lights[SLIDE_LIGHT].value = attributesVisual.getSlide() ? 1.0f : 0.0f;
			if (tiedWarning > 0l) {
				bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
				lights[TIE_LIGHT].value = (warningFlashState) ? 1.0f : 0.0f;
			}
			else
				lights[TIE_LIGHT].value = attributesVisual.getTied() ? 1.0f : 0.0f;			
			
			// Reset light
			lights[RESET_LIGHT].value =	resetLight;
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime() * displayRefreshStepSkips;
			
			// Run light
			lights[RUN_LIGHT].value = (running ? 1.0f : 0.0f);

			// Attach light
			lights[ATTACH_LIGHT].value = (attached ? 1.0f : 0.0f);
			
			for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
				if (editingGate[trkn] > 0ul)
					editingGate[trkn]--;
			}
			if (tiedWarning > 0l)
				tiedWarning--;
			if (showLenInSteps > 0l)
				showLenInSteps--;
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
	IMPort* expPorts[6];
	
	template <int NUMCHAR>
	struct DisplayWidget : TransparentWidget {// a centered display, must derive from this
		PhraseSeq32Ex *module;
		std::shared_ptr<Font> font;
		char displayStr[NUMCHAR+1];
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.7f; // 18.2f for 14 pt, 19.7f for 15pt
		
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
			nvgTextLetterSpacing(vg, -0.4);

			Vec textPos = Vec(4.7f, textOffsetY);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			std::string initString(NUMCHAR,'~');
			nvgText(vg, textPos.x, textPos.y, initString.c_str(), NULL);
			nvgFillColor(vg, textColor);
			char overlayChar = printText();
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
			if (overlayChar != 0) {
				displayStr[0] = overlayChar;
				displayStr[1] = 0;
				nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
		
		virtual char printText() = 0;
	};
	
	struct VelocityDisplayWidget : DisplayWidget<3> {
		VelocityDisplayWidget(Vec _pos, Vec _size, PhraseSeq32Ex *_module) : DisplayWidget(_pos, _size, _module) {};
		char printText() override {
			if (module->isEditingSequence() || module->attached) {
				StepAttributes attributesVal = module->sek[module->trackIndexEdit].getAttribute(module->seqIndexEdit, module->stepIndexEdit);
				if (module->params[PhraseSeq32Ex::VELMODE_PARAM].value > 1.5f) {
					int slide = attributesVal.getSlideVal();						
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
					int prob = attributesVal.getGatePVal();
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
					snprintf(displayStr, 4, "%3u", (unsigned)(attributesVal.getVelocityVal()));
				}
			}
			else 				
				snprintf(displayStr, 4, " - ");
			return 0;
		}
	};
	
	struct TrackDisplayWidget : DisplayWidget<2> {
		TrackDisplayWidget(Vec _pos, Vec _size, PhraseSeq32Ex *_module) : DisplayWidget(_pos, _size, _module) {};
		char printText() override {
			snprintf(displayStr, 3, " %c", (unsigned)(module->trackIndexEdit + 0x41));
			return 0;
		}
	};
	

	struct PhrEditDisplayWidget : DisplayWidget<3> {
		PhrEditDisplayWidget(Vec _pos, Vec _size, PhraseSeq32Ex *_module) : DisplayWidget(_pos, _size, _module) {};

		char printText() override {
			char overlayChar = 0;// extra char to print an end symbol overlaped (begin symbol done in here)

			if (module->displayState == PhraseSeq32Ex::DISP_PPQN) {
				snprintf(displayStr, 4, "x%2u", (unsigned) module->sek[module->trackIndexEdit].getPulsesPerStep());
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_DELAY) {
				snprintf(displayStr, 4, "D%2u", (unsigned) module->sek[module->trackIndexEdit].getDelay());
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_COPY_SONG) {
				snprintf(displayStr, 4, "CPY");
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_PASTE_SONG) {
				snprintf(displayStr, 4, "PST");
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_MODE_SONG) {
				runModeToStr(module->sek[module->trackIndexEdit].getRunModeSong());
			}
			else { 
				if (module->isEditingSequence()) {
					snprintf(displayStr, 4, " - ");
				}
				else { // editing song
					int phrn = module->phraseIndexEdit; // good whether attached or not
					snprintf(displayStr, 4, " %2u", (unsigned)(phrn + 1));
					bool begHere = (phrn == module->sek[module->trackIndexEdit].getBegin());
					bool endHere = (phrn == module->sek[module->trackIndexEdit].getEnd());
					if (begHere) {
						displayStr[0] = '{';
						if (endHere)
							overlayChar = '}';
					}
					else if (endHere) {
						displayStr[0] = '}';
						overlayChar = '_';
					}
				}
			}
			return overlayChar;
		}
	};
	
	
	struct SeqEditDisplayWidget : DisplayWidget<3> {
		SeqEditDisplayWidget(Vec _pos, Vec _size, PhraseSeq32Ex *_module) : DisplayWidget(_pos, _size, _module) {};
		char printText() override {
			int trkn = module->trackIndexEdit;

			if (module->displayState == PhraseSeq32Ex::DISP_REPS) {
				snprintf(displayStr, 4, "R%2u", (unsigned) abs(module->sek[trkn].getPhraseReps(module->phraseIndexEdit)));
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_COPY_SEQ) {
				snprintf(displayStr, 4, "CPY");
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_PASTE_SEQ) {
				snprintf(displayStr, 4, "PST");
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_MODE_SEQ) {
				runModeToStr(module->sek[trkn].getRunModeSeq(module->seqIndexEdit));
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_LEN) {
				snprintf(displayStr, 4, "L%2u", (unsigned) module->sek[trkn].getLength(module->seqIndexEdit));
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_TRANSPOSE) {
				int tranOffset = module->sek[trkn].getTransposeOffset(module->seqIndexEdit);
				snprintf(displayStr, 4, "+%2u", (unsigned) abs(tranOffset));
				if (tranOffset < 0)
					displayStr[0] = '-';
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_ROTATE) {
				snprintf(displayStr, 4, ")%2u", (unsigned) abs(module->rotateOffset));
				if (module->rotateOffset < 0)
					displayStr[0] = '(';
			}
			else {
				// two paths below are equivalent when attached, so no need to check attached
				if (module->isEditingSequence())
					snprintf(displayStr, 4, " %2u", (unsigned)(module->seqIndexEdit + 1) );
				else {
					int seqn = module->sek[trkn].getPhraseSeq(module->phraseIndexEdit);
					snprintf(displayStr, 4, " %2u", (unsigned)(seqn + 1) );
				}
			}
			return 0;
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
	struct AutoseqItem : MenuItem {
		PhraseSeq32Ex *module;
		void onAction(EventAction &e) override {
			module->autoseq = !module->autoseq;
		}
	};
	struct SeqCVmethodItem : MenuItem {
		PhraseSeq32Ex *module;
		void onAction(EventAction &e) override {
			module->seqCVmethod++;
			if (module->seqCVmethod > 2)
				module->seqCVmethod = 0;
		}
		void step() override {
			if (module->seqCVmethod == 0)
				text = "Seq CV in: <0-10V>,  C2-D7#,  Trig-Incr";
			else if (module->seqCVmethod == 1)
				text = "Seq CV in: 0-10V,  <C2-D7#>,  Trig-Incr";
			else
				text = "Seq CV in: 0-10V,  C2-D7#,  <Trig-Incr>";
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

		AutoseqItem *aseqItem = MenuItem::create<AutoseqItem>("AutoSeq when writing via CV inputs", CHECKMARK(module->autoseq));
		aseqItem->module = module;
		menu->addChild(aseqItem);

		SeqCVmethodItem *seqcvItem = MenuItem::create<SeqCVmethodItem>("Seq CV in: ", "");
		seqcvItem->module = module;
		menu->addChild(seqcvItem);
		
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
	
	struct CKSSNotify : CKSS {
		CKSSNotify() {};
		void onChange(EventChange &e) override {
			((PhraseSeq32Ex*)(module))->displayState = PhraseSeq32Ex::DISP_NORMAL;
			SVGSwitch::onChange(e);		
		}
	};
	struct CKSSHThreeNotify : CKSSHThree {
		CKSSHThreeNotify() {};
		void onChange(EventChange &e) override {
			((PhraseSeq32Ex*)(module))->displayState = PhraseSeq32Ex::DISP_NORMAL;
			SVGSwitch::onChange(e);		
		}
	};
	struct Velocityknob : IMMediumKnobInf {
		Velocityknob() {};
		void onMouseDown(EventMouseDown &e) override {// from ParamWidget.cpp
			if (e.button == 1) {
				float vparam = ((PhraseSeq32Ex*)(module))->params[PhraseSeq32Ex::VELMODE_PARAM].value;
				int trkn = ((PhraseSeq32Ex*)(module))->trackIndexEdit;
				int seqn = ((PhraseSeq32Ex*)(module))->seqIndexEdit;
				int stepn = ((PhraseSeq32Ex*)(module))->stepIndexEdit;
				if (vparam > 1.5f) {
					((PhraseSeq32Ex*)(module))->sek[trkn].setSlideVal(seqn, stepn, StepAttributes::INIT_SLIDE);
				}
				else if (vparam > 0.5f) {
					((PhraseSeq32Ex*)(module))->sek[trkn].setGatePVal(seqn, stepn, StepAttributes::INIT_PROB);
				}
				else {
					((PhraseSeq32Ex*)(module))->sek[trkn].setVelocityVal(seqn, stepn, StepAttributes::INIT_VELOCITY);
				}
			}
			ParamWidget::onMouseDown(e);
		}
	};
		
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
		static const int columnRulerT1 = 373;// Select (steps) 
		static const int columnRulerT2 = 422;// Copy paste and select mode switch
		//static const int columnRulerT3 = 463;// Copy paste buttons (not needed when align to track display)
		static const int columnRulerT5 = 539;// Edit mode switch (and overview switch also)
		static const int stepsOffsetY = 10;
		static const int posLEDvsButton = 26;

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
		addParam(createDynamicParamCentered<IMPushButton>(Vec(columnRulerT1, rowRulerT0), module, PhraseSeq32Ex::ALLSTEPS_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// Copy-paste and select mode switch (3 position)
		addParam(createParamCentered<CKSSThreeInv>(Vec(columnRulerT2, rowRulerT0), module, PhraseSeq32Ex::CPMODE_PARAM, 0.0f, 2.0f, 2.0f));	// 0.0f is top position
		
		// Copy/paste buttons
		// see under Track display
		
		// Main switch
		addParam(createParamCentered<CKSSNotify>(Vec(columnRulerT5, rowRulerT0), module, PhraseSeq32Ex::EDIT_PARAM, 0.0f, 1.0f, 1.0f));// 1.0f is top position

		
		
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



		// ****** Right side control area ******
		
		static const int rowRulerDisp = 110;
		static const int rowRulerKnobs = 145;
		static const int rowRulerSmallButtons = 189;
		static const int displayWidths = 46; // 43 for 14pt, 46 for 15pt
		static const int displayHeights = 24; // 22 for 14pt, 24 for 15pt
		static const int displaySpacingX = 62;

		// Velocity display
		static const int colRulerVel = 288;
		addChild(new VelocityDisplayWidget(Vec(colRulerVel, rowRulerDisp), Vec(displayWidths, displayHeights), module));// 3 characters
		// Velocity knob
		addParam(createDynamicParamCentered<Velocityknob>(Vec(colRulerVel, rowRulerKnobs), module, PhraseSeq32Ex::VEL_KNOB_PARAM, -INFINITY, INFINITY, 0.0f, &module->panelTheme));	
		// Veocity mode switch (3 position)
		addParam(createParamCentered<CKSSHThreeNotify>(Vec(colRulerVel, rowRulerSmallButtons), module, PhraseSeq32Ex::VELMODE_PARAM, 0.0f, 2.0f, 0.0f));	// 0.0f is top position
		

		// Phrase edit display 
		static const int colRulerEditPhr = colRulerVel + displaySpacingX + 1;
		static const int trkButtonsOffsetX = 14;
		addChild(new PhrEditDisplayWidget(Vec(colRulerEditPhr, rowRulerDisp), Vec(displayWidths, displayHeights), module));// 5 characters
		// Phrase knob
		addParam(createDynamicParamCentered<IMMediumKnobInf>(Vec(colRulerEditPhr, rowRulerKnobs), module, PhraseSeq32Ex::PHRASE_PARAM, -INFINITY, INFINITY, 0.0f, &module->panelTheme));		
		// Begin/end buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditPhr - trkButtonsOffsetX, rowRulerSmallButtons), module, PhraseSeq32Ex::BEGIN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditPhr + trkButtonsOffsetX, rowRulerSmallButtons), module, PhraseSeq32Ex::END_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));

				
		// Seq edit display 
		static const int colRulerEditSeq = colRulerEditPhr + displaySpacingX + 1;
		addChild(new SeqEditDisplayWidget(Vec(colRulerEditSeq, rowRulerDisp), Vec(displayWidths, displayHeights), module));// 5 characters
		// Sequence-edit knob
		addParam(createDynamicParamCentered<IMMediumKnobInf>(Vec(colRulerEditSeq, rowRulerKnobs), module, PhraseSeq32Ex::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f, &module->panelTheme));		
		// Transpose/rotate button
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditSeq, rowRulerSmallButtons), module, PhraseSeq32Ex::TRAN_ROT_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
	
			
		// Track display
		static const int colRulerTrk = colRulerEditSeq + displaySpacingX + 1;
		addChild(new TrackDisplayWidget(Vec(colRulerTrk, rowRulerDisp), Vec(displayWidths - 13, displayHeights), module));// 2 characters
		// Track buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk + trkButtonsOffsetX, rowRulerKnobs), module, PhraseSeq32Ex::TRACKUP_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk - trkButtonsOffsetX, rowRulerKnobs), module, PhraseSeq32Ex::TRACKDOWN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// AllTracks button
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk, rowRulerSmallButtons - 10), module, PhraseSeq32Ex::ALLTRACKS_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Copy/paste buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk - trkButtonsOffsetX, rowRulerT0), module, PhraseSeq32Ex::COPY_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk + trkButtonsOffsetX, rowRulerT0), module, PhraseSeq32Ex::PASTE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
	
	
		// Attach button and light
		addParam(createDynamicParamCentered<IMPushButton>(Vec(columnRulerT5 - 10, rowRulerDisp + 4), module, PhraseSeq32Ex::ATTACH_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerT5 + 10, rowRulerDisp + 4), module, PhraseSeq32Ex::ATTACH_LIGHT));		
	
	
		// ****** Gate and slide section ******
		
		static const int rowRulerMB0 = rowRulerOct + 6 * octLightsIntY;
		static const int columnRulerMB3 = colRulerVel - displaySpacingX;
		static const int columnRulerMB2 = colRulerVel - 2 * displaySpacingX;
		static const int columnRulerMB1 = colRulerVel - 3 * displaySpacingX;
		
		// Key mode LED buttons	
		static const int colRulerKM = 61;
		addParam(createParamCentered<CKSSNotify>(Vec(colRulerKM, rowRulerMB0), module, PhraseSeq32Ex::KEY_GATE_PARAM, 0.0f, 1.0f, 1.0f));
		
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
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(colRulerVel + posLEDvsButton, rowRulerMB0), module, PhraseSeq32Ex::SLIDE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(colRulerVel, rowRulerMB0), module, PhraseSeq32Ex::SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Mode button
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(colRulerEditPhr, rowRulerMB0), module, PhraseSeq32Ex::MODE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Rep/Len button
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(colRulerEditSeq, rowRulerMB0), module, PhraseSeq32Ex::REP_LEN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Clk res
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(colRulerTrk, rowRulerMB0), module, PhraseSeq32Ex::CLKRES_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// Reset and run LED buttons
		static const int colRulerResetRun = columnRulerT5;
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerResetRun, rowRulerSmallButtons - 6), module, PhraseSeq32Ex::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerResetRun, rowRulerSmallButtons - 6), module, PhraseSeq32Ex::RUN_LIGHT));
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerResetRun, rowRulerMB0), module, PhraseSeq32Ex::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerResetRun, rowRulerMB0), module, PhraseSeq32Ex::RESET_LIGHT));
		


		
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
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB2, rowRulerBLow), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 2, &module->panelTheme));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB3, rowRulerBLow), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 3, &module->panelTheme));
		
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
		static const int rowRulerExpTop = 73.55;//78;
		static const int rowSpacingExp = 50;//60;
		static const int colRulerExp = panel->box.size.x - expWidth / 2;
		addInput(expPorts[0] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 0), Port::INPUT, module, PhraseSeq32Ex::GATECV_INPUT, &module->panelTheme));
		addInput(expPorts[1] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 1), Port::INPUT, module, PhraseSeq32Ex::GATEPCV_INPUT, &module->panelTheme));
		addInput(expPorts[2] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 2), Port::INPUT, module, PhraseSeq32Ex::TIEDCV_INPUT, &module->panelTheme));
		addInput(expPorts[3] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 3), Port::INPUT, module, PhraseSeq32Ex::SLIDECV_INPUT, &module->panelTheme));
		addInput(expPorts[4] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 4), Port::INPUT, module, PhraseSeq32Ex::SEQCV_INPUT, &module->panelTheme));
		addInput(expPorts[5] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 5), Port::INPUT, module, PhraseSeq32Ex::TRKCV_INPUT, &module->panelTheme));
	}
};

Model *modelPhraseSeq32Ex = Model::create<PhraseSeq32Ex, PhraseSeq32ExWidget>("Impromptu Modular", "Phrase-Seq-32Ex", "SEQ - Phrase-Seq-32Ex", SEQUENCER_TAG);

/*CHANGE LOG

0.6.13:
created

*/
