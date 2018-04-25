//***********************************************************************************************
//Multi-phrase 16 step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by the SA-100 Stepper Acid sequencer by Transistor Sounds Labs
//
//Acknowledgements: Thanks to Alfredo Santamaria and Nay Seven for suggestions regarding 
//  improvements. A very special thank you to Nigel Sixsmith for the many fruitful discussions 
//  and numerous design improvements that were suggested.
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct PhraseSeq16 : Module {
	enum ParamIds {
		LEFT_PARAM,
		RIGHT_PARAM,
		LENGTH_PARAM,
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
		ROTATEL_PARAM,// no longer used
		ROTATER_PARAM,// no longer used
		PASTESYNC_PARAM,// no longer used
		AUTOSTEP_PARAM,
		ENUMS(KEY_PARAMS, 12),
		TRANSPOSEU_PARAM,// no longer used
		TRANSPOSED_PARAM,// no longer used
		// -- 0.6.2
		RUNMODE_PARAM,
		TRANSPOSE_PARAM,
		ROTATE_PARAM,
		GATE1_KNOB_PARAM,
		GATE2_KNOB_PARAM,// no longer used
		GATE1_PROB_PARAM,
		TIE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		CV_INPUT,
		RESET_INPUT,
		CLOCK_INPUT,
		// -- 0.6.2
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
		// -- 0.6.2
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, 16*2),// room for GreenRed
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12),
		RUN_LIGHT,
		RESET_LIGHT,
		GATE1_LIGHT,
		GATE2_LIGHT,
		SLIDE_LIGHT,
		ATTACH_LIGHT,
		PENDING_LIGHT,// no longer used
		// -- 0.6.2
		GATE1_PROB_LIGHT,
		NUM_LIGHTS
	};
	
	enum DisplayStateIds {DISP_NORMAL, DISP_MODE, DISP_TRANSPOSE, DISP_ROTATE};
	enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_BRN, MODE_RND, NUM_MODES};

	// Need to save
	bool running;
	int runModeSeq; 
	int runModeSong; 
	//
	int sequence;
	int steps;//1 to 16
	//
	int phrase[16] = {};// This is the song (series of phases; a phrase is a patten number)
	int phrases;//1 to 16
	//
	float cv[16][16] = {};// [-3.0 : 3.917]. First index is patten number, 2nd index is step
	bool gate1[16][16] = {};// First index is patten number, 2nd index is step
	bool gate1Prob[16][16] = {};// First index is patten number, 2nd index is step
	bool gate2[16][16] = {};// First index is patten number, 2nd index is step
	bool slide[16][16] = {};// First index is patten number, 2nd index is step
	//
	int sequenceKnob;// save this so no delta triggered when close/open Rack
	float attach;

	// No need to save
	float resetLight = 0.0f;
	int stepIndexEdit;
	int stepIndexRun;
	int phraseIndexEdit;
	int phraseIndexRun;
	int stepIndexPhraseRun;
	unsigned long editingLength;// 0 when not editing length, downward step counter timer when editing length
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	float editingGateCV;// no need to initialize, this is a companion to editingGate
	int stepIndexRunHistory;// no need to initialize
	int stepIndexPhraseRunHistory;// no need to initialize
	int phraseIndexRunHistory;// no need to initialize
	int displayState;
	unsigned long slideStepsRemain;// 0 when no slide under way, downward step counter when sliding
	float slideCVdelta;// no need to initialize, this is a companion to slideStepsRemain
	float cvCPbuffer[16];// copy paste buffer for CVs
	bool gate1CPbuffer[16];// copy paste buffer for gate1
	bool gate1ProbCPbuffer[16];// copy paste buffer for gate1Prob
	bool gate2CPbuffer[16];// copy paste buffer for gate2
	bool slideCPbuffer[16];// copy paste buffer for slide
	bool gate1RandomEnable; 
	int transposeOffset;// no need to initialize, this is companion to displayMode = DISP_TRANSPOSE
	int rotateOffset;// no need to initialize, this is companion to displayMode = DISP_ROTATE


	SchmittTrigger resetTrigger;
	SchmittTrigger leftTrigger;
	SchmittTrigger rightTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger octTriggers[7];
	SchmittTrigger octmTrigger;
	SchmittTrigger gate1Trigger;
	SchmittTrigger gate1ProbTrigger;
	SchmittTrigger gate2Trigger;
	SchmittTrigger slideTrigger;
	SchmittTrigger lengthTrigger;
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
	
		
	PhraseSeq16() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		running = false;
		runModeSeq = 0;
		runModeSong = 0;
		stepIndexEdit = 0;
		stepIndexRun = 0;
		sequence = 0;
		steps = 16;
		phraseIndexEdit = 0;
		phraseIndexRun = 0;
		stepIndexPhraseRun = 0;
		phrases = 4;
		for (int i = 0; i < 16; i++) {
			for (int s = 0; s < 16; s++) {
				cv[i][s] = 0.0f;
				gate1[i][s] = true;
				gate1Prob[i][s] = false;
				gate2[i][s] = true;
				slide[i][s] = false;
			}
			phrase[i] = 0;
			cvCPbuffer[i] = 0.0f;
			gate1CPbuffer[i] = true;
			gate2CPbuffer[i] = true;
			slideCPbuffer[i] = true;
		}
		sequenceKnob = 0;
		editingLength = 0ul;
		editingGate = 0ul;
		infoCopyPaste = 0l;
		displayState = DISP_NORMAL;
		slideStepsRemain = 0ul;
		attach = 1.0f;
		gate1RandomEnable = false;
	}

	void onRandomize() override {
		running = false;
		runModeSeq = randomu32() % 5;
		runModeSong = randomu32() % 5;
		stepIndexEdit = 0;
		stepIndexRun = 0;
		sequence = randomu32() % 16;
		steps = 1 + (randomu32() % 16);
		phraseIndexEdit = 0;
		phraseIndexRun = 0;
		stepIndexPhraseRun = 0;
		phrases = 1 + (randomu32() % 16);
		for (int i = 0; i < 16; i++) {
			for (int s = 0; s < 16; s++) {
				cv[i][s] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
				gate1[i][s] = (randomUniform() > 0.5f);
				gate1Prob[i][s] = (randomUniform() > 0.5f);
				gate2[i][s] = (randomUniform() > 0.5f);
				slide[i][s] = (randomUniform() > 0.5f);
			}
			phrase[i] = randomu32() % 16;
			cvCPbuffer[i] = 0.0f;
			gate1CPbuffer[i] = true;
			gate2CPbuffer[i] = true;
			slideCPbuffer[i] = true;
		}
		sequenceKnob = 0;
		editingLength = 0ul;
		editingGate = 0ul;
		infoCopyPaste = 0l;
		displayState = DISP_NORMAL;
		slideStepsRemain = 0ul;
		attach = 1.0f;
		gate1RandomEnable = false;
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

		// steps
		json_object_set_new(rootJ, "steps", json_integer(steps));

		// phrases
		json_object_set_new(rootJ, "phrases", json_integer(phrases));

		// CV
		json_t *cvJ = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(cvJ, s + (i<<4), json_real(cv[i][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// gate1
		json_t *gate1J = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(gate1J, s + (i<<4), json_integer((int) gate1[i][s]));
			}
		json_object_set_new(rootJ, "gate1", gate1J);

		// gate1Prob
		json_t *gate1ProbJ = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(gate1ProbJ, s + (i<<4), json_integer((int) gate1Prob[i][s]));
			}
		json_object_set_new(rootJ, "gate1Prob", gate1ProbJ);

		// gate2
		json_t *gate2J = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(gate2J, s + (i<<4), json_integer((int) gate2[i][s]));
			}
		json_object_set_new(rootJ, "gate2", gate2J);

		// slide
		json_t *slideJ = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(slideJ, s + (i<<4), json_integer((int) slide[i][s]));
			}
		json_object_set_new(rootJ, "slide", slideJ);

		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, "phrase", phraseJ);

		// sequenceKnob
		json_object_set_new(rootJ, "sequenceKnob", json_integer(sequenceKnob));

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
		
		// steps
		json_t *stepsJ = json_object_get(rootJ, "steps");
		if (stepsJ)
			steps = json_integer_value(stepsJ);
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, "phrases");
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
		
		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *cvArrayJ = json_array_get(cvJ, s + (i<<4));
					if (cvArrayJ)
						cv[i][s] = json_real_value(cvArrayJ);
				}
		}
		
		// gate1
		json_t *gate1J = json_object_get(rootJ, "gate1");
		if (gate1J) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *gate1arrayJ = json_array_get(gate1J, s + (i<<4));
					if (gate1arrayJ)
						gate1[i][s] = !!json_integer_value(gate1arrayJ);
				}
		}
		
		// gate1Prob
		json_t *gate1ProbJ = json_object_get(rootJ, "gate1Prob");
		if (gate1ProbJ) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *gate1ProbarrayJ = json_array_get(gate1ProbJ, s + (i<<4));
					if (gate1ProbarrayJ)
						gate1Prob[i][s] = !!json_integer_value(gate1ProbarrayJ);
				}
		}
		
		// gate2
		json_t *gate2J = json_object_get(rootJ, "gate2");
		if (gate2J) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *gate2arrayJ = json_array_get(gate2J, s + (i<<4));
					if (gate2arrayJ)
						gate2[i][s] = !!json_integer_value(gate2arrayJ);
				}
		}
		
		// slide
		json_t *slideJ = json_object_get(rootJ, "slide");
		if (slideJ) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *slideArrayJ = json_array_get(slideJ, s + (i<<4));
					if (slideArrayJ)
						slide[i][s] = !!json_integer_value(slideArrayJ);
				}
		}
		
		// phrase
		json_t *phraseJ = json_object_get(rootJ, "phrase");
		if (phraseJ)
			for (int i = 0; i < 16; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
			
		// sequenceKnob
		json_t *sequenceKnobJ = json_object_get(rootJ, "sequenceKnob");
		if (sequenceKnobJ)
			sequenceKnob = json_integer_value(sequenceKnobJ);
		
		// attach
		json_t *attachJ = json_object_get(rootJ, "attach");
		if (attachJ)
			attach = json_real_value(attachJ);
	}

	void rotateSeq(int sequenceNum, bool directionRight, int numSteps) {
		float rotCV;
		bool rotGate1, rotGate1Prob, rotGate2, rotSlide;
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
		rotGate1 = gate1[sequenceNum][iRot];
		rotGate1Prob = gate1Prob[sequenceNum][iRot];
		rotGate2 = gate2[sequenceNum][iRot];
		rotSlide = slide[sequenceNum][iRot];
		for ( ; ; iRot += iDelta) {
			if (iDelta == 1 && iRot >= numSteps - 1) break;
			if (iDelta == -1 && iRot <= 0) break;
			cv[sequenceNum][iRot] = cv[sequenceNum][iRot + iDelta];
			gate1[sequenceNum][iRot] = gate1[sequenceNum][iRot + iDelta];
			gate1Prob[sequenceNum][iRot] = gate1Prob[sequenceNum][iRot + iDelta];
			gate2[sequenceNum][iRot] = gate2[sequenceNum][iRot + iDelta];
			slide[sequenceNum][iRot] = slide[sequenceNum][iRot + iDelta];
		}
		cv[sequenceNum][iRot] = rotCV;
		gate1[sequenceNum][iRot] = rotGate1;
		gate1Prob[sequenceNum][iRot] = rotGate1Prob;
		gate2[sequenceNum][iRot] = rotGate2;
		slide[sequenceNum][iRot] = rotSlide;
	}
	
	bool stepIndex(int* index, int numSteps, int runMode, int* stepIndexHistory) {		
		bool crossBoundary = false;
		
		if (runMode == MODE_REV) {// reverse; history base is 1000 (not needed)
			(*stepIndexHistory) = 1000;
			(*index)--;
			if ((*index) < 0) {
				(*index) = numSteps - 1;
				crossBoundary = true;
			}
		}
		else if (runMode == MODE_PPG) {// forward-reverse; history base is 2000
			if ((*stepIndexHistory) != 2000 && (*stepIndexHistory) != 2001) // 2000 means going forward, 2001 means going reverse
				(*stepIndexHistory) = 2000;
			if ((*stepIndexHistory) == 2000) {// forward phase
				(*index)++;
				if ((*index) >= numSteps) {
					(*index) = numSteps - 1;
					(*stepIndexHistory) = 2001;
				}
			}
			else {// it is 2001; reverse phase
				(*index)--;
				if ((*index) < 0) {
					(*index) = 0;
					(*stepIndexHistory) = 2000;
					crossBoundary = true;
				}
			}
		}
		else if (runMode == MODE_BRN) {// brownian random; history base is 3000
			if ( (*stepIndexHistory) < 3000 || ((*stepIndexHistory) > (3000 + numSteps)) ) 
				(*stepIndexHistory) = 3000 + numSteps;
			(*index) += (randomu32() % 3) - 1;
			if ((*index) >= numSteps) {
				(*index) = 0;
			}
			if ((*index) < 0) {
				(*index) = numSteps - 1;
			}
			(*stepIndexHistory)--;
			if ((*stepIndexHistory) <= 3000) {
				(*stepIndexHistory) = 3000 + numSteps;
				crossBoundary = true;
			}
		}
		else if (runMode == MODE_RND) {// random; history base is 4000
			if ( (*stepIndexHistory) < 4000 || ((*stepIndexHistory) > (4000 + numSteps)) ) 
				(*stepIndexHistory) = 4000 + numSteps;
			(*index) = (randomu32() % numSteps) ;
			(*stepIndexHistory)--;
			if ((*stepIndexHistory) <= 4000) {
				(*stepIndexHistory) = 4000 + numSteps;
				crossBoundary = true;
			}
		}
		else {// MODE_FWD  forward; history base is 0 (not needed)
			(*stepIndexHistory) = 0;
			(*index)++;
			if ((*index) >= numSteps) {
				(*index) = 0;
				crossBoundary = true;
			}
		}

		return crossBoundary;
	}
	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		static const float gateTime = 0.3f;// seconds
		static const float copyPasteInfoTime = 0.4f;// seconds
		static const float editLengthTime = 1.6f;// seconds

		bool editingSequence = params[EDIT_PARAM].value > 0.5f;// true = editing sequence, false = editing song
		if ( editTrigger.process(params[EDIT_PARAM].value) || editTriggerInv.process(1.0f - params[EDIT_PARAM].value) )
			displayState = DISP_NORMAL;	
		
		// Seq and Mode CV inputs
		if (inputs[SEQCV_INPUT].active) {
			sequence = (int) clamp( round(inputs[SEQCV_INPUT].value * 15.0f / 10.0f), 0.0f, 15.0f );
		}
		if (inputs[MODECV_INPUT].active) {
			runModeSeq = (int) clamp( round(inputs[MODECV_INPUT].value * 4.0f / 10.0f), 0.0f, 4.0f );
		}
		
		// Run state and light
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
		lights[RUN_LIGHT].value = (running);

		// Attach button and behavior
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
		
		// Copy
		if (copyTrigger.process(params[COPY_PARAM].value)) {
			if (editingSequence) {
				infoCopyPaste = (long) (copyPasteInfoTime * engineGetSampleRate());
				for (int s = 0; s < 16; s++) {
					cvCPbuffer[s] = cv[sequence][s];
					gate1CPbuffer[s] = gate1[sequence][s];
					gate1ProbCPbuffer[s] = gate1Prob[sequence][s];
					gate2CPbuffer[s] = gate2[sequence][s];
					slideCPbuffer[s] = slide[sequence][s];
				}
			}
			displayState = DISP_NORMAL;
		}
		// Paste
		if (pasteTrigger.process(params[PASTE_PARAM].value)) {
			if (editingSequence) {
				infoCopyPaste = (long) (-1 * copyPasteInfoTime * engineGetSampleRate());
				for (int s = 0; s < 16; s++) {
					cv[sequence][s] = cvCPbuffer[s];
					gate1[sequence][s] = gate1CPbuffer[s];
					gate1Prob[sequence][s] = gate1ProbCPbuffer[s];
					gate2[sequence][s] = gate2CPbuffer[s];
					slide[sequence][s] = slideCPbuffer[s];
				}
			}
			displayState = DISP_NORMAL;
		}
		// Tie
		if (pasteTrigger.process(params[TIE_PARAM].value)) {
			if (editingSequence) {
				int prevIndex = stepIndexEdit - 1;
				if (prevIndex < 0)
					prevIndex = steps - 1;
				cv[sequence][stepIndexEdit] = cv[sequence][prevIndex];
				gate1[sequence][stepIndexEdit] = false;
				gate1Prob[sequence][stepIndexEdit] = false;
				gate2[sequence][stepIndexEdit] = false;
				slide[sequence][stepIndexEdit] = false;		
			}
			displayState = DISP_NORMAL;
		}


		// Length button
		if (lengthTrigger.process(params[LENGTH_PARAM].value)) {
			if (editingLength > 0ul)
				editingLength = 0ul;// allow user to quickly leave editing mode when re-press
			else
				editingLength = (unsigned long) (editLengthTime * engineGetSampleRate());
			displayState = DISP_NORMAL;
		}
		
		// Write (must be before Left and Right in case route gate simultaneously to Right and Write for example)
		//  (write must be to correct step)
		bool writeTrig = writeTrigger.process(inputs[WRITE_INPUT].value);
		if (writeTrig) {
			if (editingSequence) {
				editingGate = (unsigned long) (gateTime * engineGetSampleRate());
				editingGateCV = inputs[CV_INPUT].value;
				cv[sequence][stepIndexEdit] = inputs[CV_INPUT].value;
				if (params[AUTOSTEP_PARAM].value > 0.5f)
					stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, steps);
			}
			displayState = DISP_NORMAL;
		}
		// Left and Right buttons
		int delta = 0;
		if (leftTrigger.process(inputs[LEFTCV_INPUT].value + params[LEFT_PARAM].value)) { 
			delta = -1;
			displayState = DISP_NORMAL;
		}
		if (rightTrigger.process(inputs[RIGHTCV_INPUT].value + params[RIGHT_PARAM].value)) {
			delta = +1;
			displayState = DISP_NORMAL;
		}
		if (delta != 0) {
			if (editingLength > 0ul) {
				editingLength = (unsigned long) (editLengthTime * engineGetSampleRate());// restart editing length timer
				if (editingSequence) {
					steps += delta;
					if (steps > 16) steps = 16;
					if (steps < 1 ) steps = 1;
					if (stepIndexEdit >= steps) stepIndexEdit = steps - 1;
				}
				else {
					phrases += delta;
					if (phrases > 16) phrases = 16;
					if (phrases < 1 ) phrases = 1;
					if (phraseIndexEdit >= phrases) phraseIndexEdit = phrases - 1;
				}
			}
			else {
				if (!running || attach < 0.5f) {// don't move heads when attach and running
					if (editingSequence) {
						stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, steps);
						if (!writeTrig)
							editingGateCV = cv[sequence][stepIndexEdit];// don't overwrite when simultaneous writeCV and stepCV
						editingGate = (unsigned long) (gateTime * engineGetSampleRate());
					}
					else
						phraseIndexEdit = moveIndex(phraseIndexEdit, phraseIndexEdit + delta, phrases);
				}
			}
		}
		
		// Mode/Transpose/Rotate buttons
		if (modeTrigger.process(params[RUNMODE_PARAM].value)) {
			if (displayState != DISP_MODE)
				displayState = DISP_MODE;
			else
				displayState = DISP_NORMAL;
		}
		if (transposeTrigger.process(params[TRANSPOSE_PARAM].value)) {
			if (displayState != DISP_TRANSPOSE && editingSequence) {
				displayState = DISP_TRANSPOSE;
				transposeOffset = 0;
			}
			else
				displayState = DISP_NORMAL;
		}
		if (rotateTrigger.process(params[ROTATE_PARAM].value)) {
			if (displayState != DISP_ROTATE  && editingSequence) {
				displayState = DISP_ROTATE;
				rotateOffset = 0;
			}
			else
				displayState = DISP_NORMAL;
		}
		
		// Sequence knob  
		int newSequenceKnob = (int)roundf(params[SEQUENCE_PARAM].value*7.0f);
		int deltaKnob = newSequenceKnob - sequenceKnob;
		if (deltaKnob != 0) {
			if (abs(deltaKnob) <= 1) {// avoid discontinuous step (initialize for example)
				if (displayState == DISP_MODE) {
					if (editingSequence) {
						if (!inputs[MODECV_INPUT].active) {
							runModeSeq += deltaKnob;
							if (runModeSeq < 0) runModeSeq = 0;
							if (runModeSeq > 4) runModeSeq = 4;
						}
					}
					else {
						runModeSong += deltaKnob;
						if (runModeSong < 0) runModeSong = 0;
						if (runModeSong > 4) runModeSong = 4;
					}
				}
				else if (displayState == DISP_TRANSPOSE) {
					if (editingSequence) {
						transposeOffset += deltaKnob;
						if (transposeOffset > 99) transposeOffset = 99;
						if (transposeOffset < -99) transposeOffset = -99;						
						// Tranpose by this number of semi-tones: deltaKnob
						float transposeOffsetCV = ((float)(deltaKnob))/12.0f;
						for (int s = 0; s < 16; s++) {
							cv[sequence][s] += transposeOffsetCV;
						}
					}
				}
				else if (displayState == DISP_ROTATE) {
					if (editingSequence) {
						rotateOffset += deltaKnob;
						if (rotateOffset > 99) rotateOffset = 99;
						if (rotateOffset < -99) rotateOffset = -99;	
						if (deltaKnob > 0 && deltaKnob < 99) {// Rotata right, 99 is safety
							for (int i = deltaKnob; i > 0; i--)
								rotateSeq(sequence, true, steps);
						}
						if (deltaKnob < 0 && deltaKnob > -99) {// Rotata left, 99 is safety
							for (int i = deltaKnob; i < 0; i++)
								rotateSeq(sequence, false, steps);
						}
					}						
				}
				else {// DISP_NORMAL
					if (editingSequence) {
						if (!inputs[SEQCV_INPUT].active) {
							sequence += deltaKnob;
							if (sequence < 0) sequence = 0;
							if (sequence > 15) sequence = 15;
						}
					}
					else {
						phrase[phraseIndexEdit] += deltaKnob;
						if (phrase[phraseIndexEdit] < 0) phrase[phraseIndexEdit] = 0;
						if (phrase[phraseIndexEdit] > 15) phrase[phraseIndexEdit] = 15;				
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
		if (newOct >=0 && newOct <=6) {
			if (editingSequence) {
				float newCV = cv[sequence][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
				newCV = newCV - floor(newCV) + (float) (newOct - 3);
				if (newCV >= -3.0f && newCV < 4.0f)
					cv[sequence][stepIndexEdit] = newCV;
				editingGate = (unsigned long) (gateTime * engineGetSampleRate());
				editingGateCV = cv[sequence][stepIndexEdit];
			}
		}		
		
		// Keyboard
		for (int i = 0; i < 12; i++) {
			if (keyTriggers[i].process(params[KEY_PARAMS + i].value)) {
				if (editingSequence) {
					cv[sequence][stepIndexEdit] = floor(cv[sequence][stepIndexEdit]) + ((float) i) / 12.0f;
					editingGate = (unsigned long) (gateTime * engineGetSampleRate());
					editingGateCV = cv[sequence][stepIndexEdit];	
				}
				displayState = DISP_NORMAL;
			}
		}
				
		// Gate1, Gate1Prob, Gate2 and slide buttons
		if (gate1Trigger.process(params[GATE1_PARAM].value)) {
			if (editingSequence)
				gate1[sequence][stepIndexEdit] = !gate1[sequence][stepIndexEdit];
			displayState = DISP_NORMAL;
		}		
		if (gate1ProbTrigger.process(params[GATE1_PROB_PARAM].value)) {
			if (editingSequence)
				gate1Prob[sequence][stepIndexEdit] = !gate1Prob[sequence][stepIndexEdit];
			displayState = DISP_NORMAL;
		}		
		if (gate2Trigger.process(params[GATE2_PARAM].value)) {
			if (editingSequence)
				gate2[sequence][stepIndexEdit] = !gate2[sequence][stepIndexEdit];
			displayState = DISP_NORMAL;
		}		
		if (slideTrigger.process(params[SLIDE_BTN_PARAM].value)) {
			if (editingSequence)
				slide[sequence][stepIndexEdit] = !slide[sequence][stepIndexEdit];
			displayState = DISP_NORMAL;
		}		
		
		// Clock
		if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			if (running) {
				float slideFromCV = 0.0f;
				float slideToCV = 0.0f;
				if (editingSequence) {
					slideFromCV = cv[sequence][stepIndexRun];
					stepIndex(&stepIndexRun, steps, runModeSeq, &stepIndexRunHistory);
					slideToCV = cv[sequence][stepIndexRun];
					gate1RandomEnable = gate1Prob[sequence][stepIndexRun];// not random yet
				}
				else {
					slideFromCV = cv[phrase[phraseIndexRun]][stepIndexPhraseRun];
					if (stepIndex(&stepIndexPhraseRun, steps, runModeSeq, &stepIndexPhraseRunHistory)) {
						stepIndex(&phraseIndexRun, phrases, runModeSong, &phraseIndexRunHistory);
					}
					slideToCV = cv[phrase[phraseIndexRun]][stepIndexPhraseRun];
					gate1RandomEnable = gate1Prob[phrase[phraseIndexRun]][stepIndexPhraseRun];// not random yet
				}
				
				// Slide
				if ( (editingSequence && slide[sequence][stepIndexRun]) || (!editingSequence && slide[phrase[phraseIndexRun]][stepIndexPhraseRun]) ) {
					slideStepsRemain = (unsigned long) (params[SLIDE_KNOB_PARAM].value * engineGetSampleRate());// avtivate sliding
					slideCVdelta = (slideToCV - slideFromCV)/(float)slideStepsRemain;
				}

				if (gate1RandomEnable)
					gate1RandomEnable = randomUniform() < (params[GATE1_KNOB_PARAM].value);// randomUniform is [0.0, 1.0), see include/util/common.hpp
				else 
					gate1RandomEnable = true;
			}
		}	
		
		// CV and gates outputs
		if (running) {
			float slideOffset = (slideStepsRemain > 0ul ? (slideCVdelta * (float)slideStepsRemain) : 0.0f);
			if (editingSequence) {// editing sequence while running
				outputs[CV_OUTPUT].value = cv[sequence][stepIndexRun] - slideOffset;
				outputs[GATE1_OUTPUT].value = (clockTrigger.isHigh() && gate1RandomEnable && gate1[sequence][stepIndexRun]) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (clockTrigger.isHigh() && gate2[sequence][stepIndexRun]) ? 10.0f : 0.0f;
			}
			else {// editing song while running
				outputs[CV_OUTPUT].value = cv[phrase[phraseIndexRun]][stepIndexPhraseRun] - slideOffset;
				outputs[GATE1_OUTPUT].value = (clockTrigger.isHigh() && gate1RandomEnable && gate1[phrase[phraseIndexRun]][stepIndexPhraseRun]) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (clockTrigger.isHigh() && gate2[phrase[phraseIndexRun]][stepIndexPhraseRun]) ? 10.0f : 0.0f;
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
		}
		else
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime();
	
		// Step/phrase lights
		for (int i = 0; i < 16; i++) {
			if (editingLength > 0ul) {
				// Length (green)
				if (editingSequence)
					lights[STEP_PHRASE_LIGHTS + (i<<1)].value = ((i < steps) ? 0.5f : 0.0f);
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
		if (infoCopyPaste != 0l) {
			if (infoCopyPaste > 0l) {// if copy (light-up black keys)
				for (int i = 0; i < 12; i++) {
					lights[KEY_LIGHTS + i].value = (i == 1 || i == 3 || i == 6 || i == 8 || i == 10) ? 1.0f : 0.0f;
				}	
			}
			else {// paste (light-up white keys)
				for (int i = 0; i < 12; i++) {
					lights[KEY_LIGHTS + i].value = (i == 1 || i == 3 || i == 6 || i == 8 || i == 10) ? 0.0f : 1.0f;
				}	
			}
		}
		else {
			if (editingSequence) 
				cvValOffset = cv[sequence][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
			else	
				cvValOffset = cv[phrase[phraseIndexEdit]][stepIndexPhraseRun] + 10.0f;//to properly handle negative note voltages
			int keyLightIndex = (int) clamp(  roundf( (cvValOffset-floor(cvValOffset)) * 12.0f ),  0.0f,  11.0f);
			for (int i = 0; i < 12; i++) {
				lights[KEY_LIGHTS + i].value = (i == keyLightIndex ? 1.0f : 0.0f);
			}	
		}		
		
		// Gate1, Gate1Prob, Gate2 and Slide lights
		bool gate1Val = true;
		bool gate1ProbVal = true;
		bool gate2Val = true;
		bool slideVal = true;
		if (editingSequence) {
			gate1Val = gate1[sequence][stepIndexEdit];
			gate1ProbVal = gate1Prob[sequence][stepIndexEdit];
			gate2Val = gate2[sequence][stepIndexEdit];
			slideVal = slide[sequence][stepIndexEdit];
		}
		else {
			gate1Val = gate1[phrase[phraseIndexEdit]][stepIndexPhraseRun];
			gate1ProbVal = gate1Prob[phrase[phraseIndexEdit]][stepIndexPhraseRun];
			gate2Val = gate2[phrase[phraseIndexEdit]][stepIndexPhraseRun];
			slideVal = slide[phrase[phraseIndexEdit]][stepIndexPhraseRun];
		}
		lights[GATE1_LIGHT].value = (gate1Val) ? 1.0f : 0.0f;
		lights[GATE1_PROB_LIGHT].value = (gate1ProbVal) ? 1.0f : 0.0f;
		lights[GATE2_LIGHT].value = (gate2Val) ? 1.0f : 0.0f;
		lights[SLIDE_LIGHT].value = (slideVal) ? 1.0f : 0.0f;

		// Attach light
		lights[ATTACH_LIGHT].value = running ? attach : 0.0f;
		
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	

		if (editingLength > 0ul)
			editingLength--;
		if (editingGate > 0ul)
			editingGate--;
		if (infoCopyPaste != 0ul) {
			if (infoCopyPaste > 0l)
				infoCopyPaste --;
			if (infoCopyPaste < 0l)
				infoCopyPaste ++;
		}
		if (slideStepsRemain > 0ul)
			slideStepsRemain--;
	}
};


struct PhraseSeq16Widget : ModuleWidget {

	struct SequenceDisplayWidget : TransparentWidget {
		PhraseSeq16 *module;
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
			if (module->displayState == PhraseSeq16::DISP_MODE) {
				if (module->params[PhraseSeq16::EDIT_PARAM].value > 0.5f)// if editing sequence
					runModeToStr(module->runModeSeq);
				else
					runModeToStr(module->runModeSong);
			}
			else if (module->displayState == PhraseSeq16::DISP_TRANSPOSE) {
				snprintf(displayStr, 4, "+%2u", (unsigned) abs(module->transposeOffset));
				if (module->transposeOffset < 0)
					displayStr[0] = '-';
			}
			else if (module->displayState == PhraseSeq16::DISP_ROTATE) {
				snprintf(displayStr, 4, ">%2u", (unsigned) abs(module->rotateOffset));
				if (module->rotateOffset < 0)
					displayStr[0] = '<';
			}
			else {// DISP_NORMAL
				snprintf(displayStr, 4, " %2u", (unsigned) (module->params[PhraseSeq16::EDIT_PARAM].value > 0.5f ? 
					module->sequence : module->phrase[module->phraseIndexEdit]) + 1 );
			}
			displayStr[3] = 0;// more safety
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	PhraseSeq16Widget(PhraseSeq16 *module) : ModuleWidget(module) {
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/PhraseSeq16.svg")));

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
		static const int columnRulerT0 = 15;// Length button
		static const int columnRulerT1 = columnRulerT0 + 47;// Left/Right buttons
		static const int columnRulerT2 = columnRulerT1 + 75;// Step/Phase lights
		static const int columnRulerT3 = columnRulerT2 + 263;// Attach (also used to align rest of right side of module)

		// Length button
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerT0 + offsetCKD6b, rowRulerT0 + offsetCKD6b), module, PhraseSeq16::LENGTH_PARAM, 0.0f, 1.0f, 0.0f));
		// Left/Right buttons
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerT1 + offsetCKD6b, rowRulerT0 + offsetCKD6b), module, PhraseSeq16::LEFT_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerT1 + 38 + offsetCKD6b, rowRulerT0 + offsetCKD6b), module, PhraseSeq16::RIGHT_PARAM, 0.0f, 1.0f, 0.0f));
		// Step/Phrase lights
		static const int spLightsSpacing = 15;
		for (int i = 0; i < 16; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(columnRulerT2 + spLightsSpacing * i + offsetMediumLight, rowRulerT0 + offsetMediumLight), module, PhraseSeq16::STEP_PHRASE_LIGHTS + (i*2)));
		}
		// Attach button and light
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerT3 - 4, rowRulerT0 + 2 + offsetTL1105), module, PhraseSeq16::ATTACH_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerT3 + 12 + offsetMediumLight, rowRulerT0 + offsetMediumLight), module, PhraseSeq16::ATTACH_LIGHT));		

		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const float octLightsIntY = 21.0f;
		for (int i = 0; i < 7; i++) {
			addParam(ParamWidget::create<LEDButton>(Vec(15 + 3, 82 + 6 + i * octLightsIntY- 4.4f), module, PhraseSeq16::OCTAVE_PARAM + i, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(15 + 3 + 4.4f, 82 + 6 + i * octLightsIntY), module, PhraseSeq16::OCTAVE_LIGHTS + i));
		}
		// Keys and Key lights
		static const int keyNudgeX = 7;
		static const int offsetKeyLEDx = 6;
		static const int offsetKeyLEDy = 28;
		// Black keys and lights
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(65+keyNudgeX, 89), module, PhraseSeq16::KEY_PARAMS + 1, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(65+keyNudgeX+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 1));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(93+keyNudgeX, 89), module, PhraseSeq16::KEY_PARAMS + 3, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(93+keyNudgeX+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 3));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(150+keyNudgeX, 89), module, PhraseSeq16::KEY_PARAMS + 6, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(150+keyNudgeX+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 6));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(178+keyNudgeX, 89), module, PhraseSeq16::KEY_PARAMS + 8, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(178+keyNudgeX+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 8));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(206+keyNudgeX, 89), module, PhraseSeq16::KEY_PARAMS + 10, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(206+keyNudgeX+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 10));
		// White keys and lights
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(51+keyNudgeX, 139), module, PhraseSeq16::KEY_PARAMS + 0, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(51+keyNudgeX+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 0));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(79+keyNudgeX, 139), module, PhraseSeq16::KEY_PARAMS + 2, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(79+keyNudgeX+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 2));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(107+keyNudgeX, 139), module, PhraseSeq16::KEY_PARAMS + 4, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(107+keyNudgeX+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 4));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(136+keyNudgeX, 139), module, PhraseSeq16::KEY_PARAMS + 5, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(136+keyNudgeX+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 5));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(164+keyNudgeX, 139), module, PhraseSeq16::KEY_PARAMS + 7, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(164+keyNudgeX+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 7));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(192+keyNudgeX, 139), module, PhraseSeq16::KEY_PARAMS + 9, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(192+keyNudgeX+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 9));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(220+keyNudgeX, 139), module, PhraseSeq16::KEY_PARAMS + 11, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(220+keyNudgeX+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 11));
		
		
		
		// ****** Right side control area ******

		static const int rowRulerMK0 = 99;// Edit mode row
		static const int rowRulerMK1 = rowRulerMK0 + 56; // Run row
		static const int rowRulerMK2 = rowRulerMK1 + 56; // Reset row
		static const int columnRulerMK0 = 276;// Edit mode column
		static const int columnRulerMK1 = columnRulerMK0 + 59;// Display column
		static const int columnRulerMK2 = columnRulerT3;// Run mode column
		
		// Edit mode switch
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerMK0 + hOffsetCKSS, rowRulerMK0 + vOffsetCKSS), module, PhraseSeq16::EDIT_PARAM, 0.0f, 1.0f, 1.0f));
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(columnRulerMK1-15, rowRulerMK0 + 3 + vOffsetDisplay);
		displaySequence->box.size = Vec(55, 30);// 3 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Run mode button
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK0 + 0 + offsetCKD6b), module, PhraseSeq16::RUNMODE_PARAM, 0.0f, 1.0f, 0.0f));

		// Reset LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerMK0 + offsetLEDbezel, rowRulerMK1 + 7 + offsetLEDbezel), module, PhraseSeq16::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerMK0 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK1 + 7 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq16::RESET_LIGHT));
		// Sequence knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(columnRulerMK1 + 1 + offsetDavies1900, rowRulerMK0 + 55 + offsetDavies1900), module, PhraseSeq16::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f));		
		// Transpose button
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK1 + 0 + offsetCKD6b), module, PhraseSeq16::TRANSPOSE_PARAM, 0.0f, 1.0f, 0.0f));
		
		
		// Run LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerMK0 + offsetLEDbezel, rowRulerMK2 + 5 + offsetLEDbezel), module, PhraseSeq16::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerMK0 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK2 + 5 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq16::RUN_LIGHT));
		// Copy/paste buttons
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMK1 - 10, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq16::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMK1 + 20, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq16::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		// Rotate button
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK2 + 0 + offsetCKD6b), module, PhraseSeq16::ROTATE_PARAM, 0.0f, 1.0f, 0.0f));

		
		
		// ****** Gate and slide section ******
		
		static const int rowRulerMB0 = 214;
		static const int rowRulerMB1 = rowRulerMB0 + 55;
		static const int columnRulerMB0 = 22;
		static const int columnRulerMBspacing = 54;
		static const int columnRulerMB1 = columnRulerMB0 + 54;// Gate1 
		static const int columnRulerMB2 = columnRulerMB1 + columnRulerMBspacing;// Gate2
		static const int columnRulerMB3 = columnRulerMB2 + columnRulerMBspacing;// Slide
		static const int columnRulerMB4 = columnRulerMB3 + columnRulerMBspacing;// Tie
		static const int posLEDvsButton = + 25;
		
		// Gate 1 light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB1 - 24 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq16::GATE1_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB1 - 24 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq16::GATE1_PARAM, 0.0f, 1.0f, 0.0f));
		// Gate 2 light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB2 - 20 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq16::GATE2_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB2 - 20 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq16::GATE2_PARAM, 0.0f, 1.0f, 0.0f));
		// Slide light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB3 - 16 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq16::SLIDE_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB3 - 16 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq16::SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f));
		// Tie step
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB4 - 12 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq16::TIE_PARAM, 0.0f, 1.0f, 0.0f));

		
		// Gate 1 probability light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB1 + posLEDvsButton + offsetMediumLight, rowRulerMB1 + offsetMediumLight), module, PhraseSeq16::GATE1_PROB_LIGHT));		
		addParam(ParamWidget::create<CKD6b>(Vec(columnRulerMB1 + offsetCKD6b, rowRulerMB1 + offsetCKD6b), module, PhraseSeq16::GATE1_PROB_PARAM, 0.0f, 1.0f, 0.0f));
		// Gate 1 probability knob
		addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(columnRulerMB2 + offsetRoundSmallBlackKnob, rowRulerMB1 + offsetRoundSmallBlackKnob), module, PhraseSeq16::GATE1_KNOB_PARAM, 0.0f, 1.0f, 1.0f));
		// Slide knob
		addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(columnRulerMB3 + offsetRoundSmallBlackKnob, rowRulerMB1 + offsetRoundSmallBlackKnob), module, PhraseSeq16::SLIDE_KNOB_PARAM, 0.0f, 2.0f, 0.15f));// slide time in seconds		
		
						
		
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

		addInput(Port::create<PJ301MPortS>(Vec(columnRulerMB0, rowRulerB1), Port::INPUT, module, PhraseSeq16::SEQCV_INPUT));
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerB4 + hOffsetCKSS, rowRulerB1 + vOffsetCKSS), module, PhraseSeq16::AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f));		
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB5, rowRulerB1), Port::INPUT, module, PhraseSeq16::CV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB6, rowRulerB1), Port::INPUT, module, PhraseSeq16::RESET_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB7, rowRulerB1), Port::INPUT, module, PhraseSeq16::CLOCK_INPUT));

		

		// ****** Bottom row (all aligned) ******

	
		// CV control Inputs 
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB0, rowRulerB0), Port::INPUT, module, PhraseSeq16::MODECV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB1, rowRulerB0), Port::INPUT, module, PhraseSeq16::LEFTCV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB2, rowRulerB0), Port::INPUT, module, PhraseSeq16::RIGHTCV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB3, rowRulerB0), Port::INPUT, module, PhraseSeq16::RUNCV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB4, rowRulerB0), Port::INPUT, module, PhraseSeq16::WRITE_INPUT));
		// Outputs
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB5, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::CV_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB6, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::GATE1_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB7, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::GATE2_OUTPUT));

	}
};

Model *modelPhraseSeq16 = Model::create<PhraseSeq16, PhraseSeq16Widget>("Impromptu Modular", "Phrase-Seq-16", "Phrase-Seq-16", SEQUENCER_TAG);
