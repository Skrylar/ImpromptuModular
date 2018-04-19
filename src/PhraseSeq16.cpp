//***********************************************************************************************
//Multi-phrase 16 step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by the SA-100 Stepper Acid sequencer by Transistor Sounds Labs
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
		PLAYMODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		CV_INPUT,
		RESET_INPUT,
		CLOCK_INPUT,
		LEFTCV_INPUT,
		RIGHTCV_INPUT,
		SEQUENCECV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE1_OUTPUT,
		GATE2_OUTPUT,
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
		NUM_LIGHTS
	};

	// Need to save
	bool running;
	int playModeSeq; // 0 = forward, 1 = reverse, 2 = ping-pong, 3 = random walk, 4 = random
	int playModeSong;// 0 = forward, 1 = reverse, 2 = ping-pong, 3 = random walk, 4 = random
	//
	int stepIndexEdit;
	int stepIndexRun;
	int sequence;
	int steps;//1 to 16
	//
	int phraseIndexEdit;
	int phraseIndexRun;
	int stepIndexPhraseRun;
	int phrase[16] = {};// This is the song (series of phases; a phrase is a patten number)
	int phrases;//1 to 16
	//
	float cv[16][16] = {}; // [-3.0 : 3.917]. First index is patten number, 2nd index is step
	bool gate1[16][16] = {}; // First index is patten number, 2nd index is step
	bool gate2[16][16] = {}; // First index is patten number, 2nd index is step
	bool slide[16][16] = {}; // First index is patten number, 2nd index is step
	//
	int sequenceKnob;// save this so no delta triggered when close/open Rack
	float attach;

	// No need to save
	float resetLight = 0.0f;
	unsigned long editingLength;// 0 when not editing length, downward step counter timer when editing length
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate
	unsigned long editingPlayMode;// 0 when not editing play mode, downward step counter timer when editing play mode
	unsigned long slideStepsRemain;// 0 when no slide under way, downward step counter when sliding
	float slideCVdelta;// no need to initialize, this is a companion to slideStepsRemain
	float cvCPbuffer[16];// copy paste buffer for CVs
	bool gate1CPbuffer[16];// copy paste buffer for gate1
	bool gate2CPbuffer[16];// copy paste buffer for gate2
	bool slideCPbuffer[16];// copy paste buffer for slide
	//int pendingPaste;// 0 = nothing to paste, 1 = paste on clk, 2 = paste on seq, destination seq in next msbits

	SchmittTrigger resetTrigger;
	SchmittTrigger leftTrigger;
	SchmittTrigger rightTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger octTriggers[7];
	SchmittTrigger octmTrigger;
	SchmittTrigger gate1Trigger;
	SchmittTrigger gate2Trigger;
	SchmittTrigger slideTrigger;
	SchmittTrigger lengthTrigger;
	SchmittTrigger keyTriggers[12];
	SchmittTrigger writeTrigger;
	SchmittTrigger attachTrigger;
	//SchmittTrigger rotateLeftTrigger;
	//SchmittTrigger rotateRightTrigger;
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	//SchmittTrigger transposeDTrigger;
	//SchmittTrigger transposeUTrigger;
	SchmittTrigger playModeTrigger;
	
		
	PhraseSeq16() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		running = false;
		playModeSeq = 0;
		playModeSong = 0;
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
				gate2[i][s] = true;
				slide[i][s] = true;
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
		editingPlayMode = 0ul;
		slideStepsRemain = 0ul;
		attach = 1.0f;
		//pendingPaste = 0;
	}

	void onRandomize() override {
		running = false;
		playModeSeq = randomu32() % 5;
		playModeSong = randomu32() % 5;
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
		editingPlayMode = 0ul;
		slideStepsRemain = 0ul;
		attach = 1.0f;
		//pendingPaste = 0;
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// playModeSeq
		json_object_set_new(rootJ, "playModeSeq", json_integer(playModeSeq));

		// playModeSong
		json_object_set_new(rootJ, "playModeSong", json_integer(playModeSong));
		
		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));

		// stepIndexRun
		json_object_set_new(rootJ, "stepIndexRun", json_integer(stepIndexRun));

		// sequence
		json_object_set_new(rootJ, "sequence", json_integer(sequence));

		// steps
		json_object_set_new(rootJ, "steps", json_integer(steps));

		// phraseIndexEdit
		json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

		// phraseIndexRun
		json_object_set_new(rootJ, "phraseIndexRun", json_integer(phraseIndexRun));

		// stepIndexPhraseRun
		json_object_set_new(rootJ, "stepIndexPhraseRun", json_integer(stepIndexPhraseRun));

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

		// playModeSeq
		json_t *playModeSeqJ = json_object_get(rootJ, "playModeSeq");
		if (playModeSeqJ)
			playModeSeq = json_integer_value(playModeSeqJ);
		
		// playModeSong
		json_t *playModeSongJ = json_object_get(rootJ, "playModeSong");
		if (playModeSongJ)
			playModeSong = json_integer_value(playModeSongJ);

		// stepIndexEdit
		json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
		if (stepIndexEditJ)
			stepIndexEdit = json_integer_value(stepIndexEditJ);
		
		// stepIndexRun
		json_t *stepIndexRunJ = json_object_get(rootJ, "stepIndexRun");
		if (stepIndexRunJ)
			stepIndexRun = json_integer_value(stepIndexRunJ);
		
		// sequence
		json_t *sequenceJ = json_object_get(rootJ, "sequence");
		if (sequenceJ)
			sequence = json_integer_value(sequenceJ);
		
		// steps
		json_t *stepsJ = json_object_get(rootJ, "steps");
		if (stepsJ)
			steps = json_integer_value(stepsJ);
		
		// phraseIndexEdit
		json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
		if (phraseIndexEditJ)
			phraseIndexEdit = json_integer_value(phraseIndexEditJ);
		
		// phraseIndexRun
		json_t *phraseIndexRunJ = json_object_get(rootJ, "phraseIndexRun");
		if (phraseIndexRunJ)
			phraseIndexRun = json_integer_value(phraseIndexRunJ);
		
		// stepIndexPhraseRun
		json_t *stepIndexPhraseRunJ = json_object_get(rootJ, "stepIndexPhraseRun");
		if (stepIndexPhraseRunJ)
			stepIndexPhraseRun = json_integer_value(stepIndexPhraseRunJ);
		
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

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		static const float gateTime = 0.3f;// seconds
		static const float editLengthTime = 1.6f;// seconds
		static const float editPlayModeTime = 1.6f;// seconds

		bool editingSequence = params[EDIT_PARAM].value > 0.5f;// true = editing sequence, false = editing song
		
		// Run state and light
		if (runningTrigger.process(params[RUN_PARAM].value)) {
			running = !running;
			//pendingPaste = 0;// no pending pastes across run state toggles
		}
		lights[RUN_LIGHT].value = (running);

		// Attach button and behavior
		if (attachTrigger.process(params[ATTACH_PARAM].value)) {
			if (running) {
				attach = 1.0f - attach;// toggle
			}			
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
				for (int s = 0; s < 16; s++) {
					cvCPbuffer[s] = cv[sequence][s];
					gate1CPbuffer[s] = gate1[sequence][s];
					gate2CPbuffer[s] = gate2[sequence][s];
					slideCPbuffer[s] = slide[sequence][s];
				}
			}
		}
		// Paste
		if (pasteTrigger.process(params[PASTE_PARAM].value)) {
			if (editingSequence) {
				//if (params[PASTESYNC_PARAM].value < 0.5f) {
					// Paste realtime, no pending to schedule
					for (int s = 0; s < 16; s++) {
						cv[sequence][s] = cvCPbuffer[s];
						gate1[sequence][s] = gate1CPbuffer[s];
						gate2[sequence][s] = gate2CPbuffer[s];
						slide[sequence][s] = slideCPbuffer[s];
					}
					//pendingPaste = 0;
				/*}
				else {
					pendingPaste = params[PASTESYNC_PARAM].value > 1.5f ? 2 : 1;
					pendingPaste |= sequence<<2; // add paste destination channel into pendingPaste				
				}*/
			}
		}

		// Length button
		if (lengthTrigger.process(params[LENGTH_PARAM].value)) {
			if (editingLength > 0ul)
				editingLength = 0ul;// allow user to quickly leave editing mode when re-press
			else
				editingLength = (unsigned long) (editLengthTime * engineGetSampleRate());
		}
		
		// Left and right buttons
		int delta = 0;
		if (leftTrigger.process(inputs[LEFTCV_INPUT].value + params[LEFT_PARAM].value)) 
			delta = -1;
		if (rightTrigger.process(inputs[RIGHTCV_INPUT].value + params[RIGHT_PARAM].value))
			delta = +1;
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
						editingGate = (unsigned long) (gateTime * engineGetSampleRate());
					}
					else
						phraseIndexEdit = moveIndex(phraseIndexEdit, phraseIndexEdit + delta, phrases);
				}
			}
		}
		
		// PlayMode button
		if (playModeTrigger.process(params[PLAYMODE_PARAM].value)) {
			if (editingPlayMode > 0ul)
				editingPlayMode = 0ul;// allow user to quickly leave editing mode when re-press
			else
				editingPlayMode = (unsigned long) (editPlayModeTime * engineGetSampleRate());
		}
		
		// Sequence knob  
		//TODO implement SEQUENCECV_INPUT behavior (likely wont work properly here since changing SEQ<->SONG will destroy SONG/SEQ stored values)
		//     should probably use SEQUENCECV_INPUT >= or < 1 V to change LEFTCV_INPUT and RIGHTCV_INPUT behaviors to instead inc/dec SEQ# knob
		int newSequenceKnob = (int)roundf(params[SEQUENCE_PARAM].value*7.0f);
		if (newSequenceKnob != sequenceKnob) {
			if (abs(newSequenceKnob - sequenceKnob) <= 1) {// avoid discontinuous step (initialize for example)
				if (editingPlayMode > 0ul) {
					editingPlayMode = (unsigned long) (editPlayModeTime * engineGetSampleRate());// restart editing play mode timer
					if (editingSequence) {
						playModeSeq += newSequenceKnob - sequenceKnob;
						if (playModeSeq < 0) playModeSeq = 0;
						if (playModeSeq > 4) playModeSeq = 4;
					}
					else {
						playModeSong += newSequenceKnob - sequenceKnob;
						if (playModeSong < 0) playModeSong = 0;
						if (playModeSong > 4) playModeSong = 4;
					}
					
				}
				else {
					if (editingSequence) {
						sequence += newSequenceKnob - sequenceKnob;
						if (sequence < 0) sequence = 0;
						if (sequence > 15) sequence = 15;
					}
					else {
						phrase[phraseIndexEdit] += newSequenceKnob - sequenceKnob;
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
			if (octTriggers[i].process(params[OCTAVE_PARAM + i].value))
				newOct = 6 - i;
		}
		if (newOct >=0 && newOct <=6) {
			if (editingSequence) {
				float newCV = cv[sequence][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
				newCV = newCV - floor(newCV) + (float) (newOct - 3);
				if (newCV >= -3.0f && newCV < 4.0f)
					cv[sequence][stepIndexEdit] = newCV;
				editingGate = (unsigned long) (gateTime * engineGetSampleRate());
			}
		}		
		
		// Keyboard and cv input 
		for (int i = 0; i < 12; i++) {
			if (keyTriggers[i].process(params[KEY_PARAMS + i].value)) {
				if (editingSequence) {
					cv[sequence][stepIndexEdit] = floor(cv[sequence][stepIndexEdit]) + ((float) i) / 12.0f;
					editingGate = (unsigned long) (gateTime * engineGetSampleRate());
				}
			}
		}
		if (writeTrigger.process(inputs[WRITE_INPUT].value)) {//TODO place this so that if wire into RIGHTCV_INPUT and WRITE_INPUT, the write goes to proper place
			if (editingSequence) {
				cv[sequence][stepIndexEdit] = inputs[CV_INPUT].value;
				editingGate = (unsigned long) (gateTime * engineGetSampleRate());
				if (params[AUTOSTEP_PARAM].value > 0.5f)
					stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, steps);
			}
		}

		// Rotate left, right buttons
		/* float rotCV;
		bool rotGate1, rotGate2, rotSlide;
		int iRot = 0;
		int iDelta = 0;
		if (rotateLeftTrigger.process(params[ROTATEL_PARAM].value)) {
			iDelta = 1;
		}
		if (rotateRightTrigger.process(params[ROTATER_PARAM].value)) {
			iRot = steps - 1;
			iDelta = -1;
		}
		if (iDelta != 0 && editingSequence) {	
			rotCV = cv[sequence][iRot];
			rotGate1 = gate1[sequence][iRot];
			rotGate2 = gate2[sequence][iRot];
			rotSlide = slide[sequence][iRot];		
			for ( ; ; iRot += iDelta) {
				if (iDelta == 1 && iRot >= steps - 1) break;
				if (iDelta == -1 && iRot <= 0) break;				
				cv[sequence][iRot] = cv[sequence][iRot + iDelta];
				gate1[sequence][iRot] = gate1[sequence][iRot + iDelta];
				gate2[sequence][iRot] = gate2[sequence][iRot + iDelta];
				slide[sequence][iRot] = slide[sequence][iRot + iDelta];
			}
			cv[sequence][iRot] = rotCV;
			gate1[sequence][iRot] = rotGate1;
			gate2[sequence][iRot] = rotGate2;
			slide[sequence][iRot] = rotSlide;				
		} */
		
		// Transpose
		/* float transposeOffset = 0.0f;
		if (transposeDTrigger.process(params[TRANSPOSED_PARAM].value)) {
			transposeOffset = -1.0f/12.0f;
		}
		if (transposeUTrigger.process(params[TRANSPOSEU_PARAM].value)) {
			transposeOffset = 1.0f/12.0f;
		}
		if (transposeOffset != 0.0f && editingSequence) {
			for (int s = 0; s < 16; s++) {
				cv[sequence][s] += transposeOffset;
			}
		} */

		// Gate1, Gate2 and slide buttons
		if (gate1Trigger.process(params[GATE1_PARAM].value)) {
			if (editingSequence)
				gate1[sequence][stepIndexEdit] = !gate1[sequence][stepIndexEdit];
		}		
		if (gate2Trigger.process(params[GATE2_PARAM].value)) {
			if (editingSequence)
				gate2[sequence][stepIndexEdit] = !gate2[sequence][stepIndexEdit];
		}		
		if (slideTrigger.process(params[SLIDE_BTN_PARAM].value)) {
			if (editingSequence)
				slide[sequence][stepIndexEdit] = !slide[sequence][stepIndexEdit];
		}		
		
		// Clock
		if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			if (running) {
				float slideFromCV = 0.0f;
				float slideToCV = 0.0f;
				if (editingSequence) {
					slideFromCV = cv[sequence][stepIndexRun];
					stepIndexRun++;//TODO step to the next proper step according to playModeSeq
					if (stepIndexRun >= steps) stepIndexRun = 0;
					slideToCV = cv[sequence][stepIndexRun];
				}
				else {
					slideFromCV = cv[phrase[phraseIndexRun]][stepIndexPhraseRun];
					stepIndexPhraseRun++;//TODO step to the next proper step according to playModeSeq
					if (stepIndexPhraseRun >= steps) {
						stepIndexPhraseRun = 0;
						phraseIndexRun++;//TODO step to the next proper step according to playModeSong
						if (phraseIndexRun >= phrases) 
							phraseIndexRun = 0;
					}
					slideToCV = cv[phrase[phraseIndexRun]][stepIndexPhraseRun];
				}
				
				// Slide
				if ( (editingSequence && slide[sequence][stepIndexRun]) || (!editingSequence && slide[phrase[phraseIndexRun]][stepIndexPhraseRun]) ) {
					slideStepsRemain = (unsigned long) (params[SLIDE_KNOB_PARAM].value * engineGetSampleRate());// avtivate sliding
					slideCVdelta = (slideToCV - slideFromCV)/(float)slideStepsRemain;
				}
			
				// Pending paste on clock or end of seq
				/*if ( ((pendingPaste&0x3) == 1) || ((pendingPaste&0x3) == 2 && stepIndexRun == 0) ) {
					if (editingSequence) {
						int pasteSeq = pendingPaste>>2;
						for (int s = 0; s < 16; s++) {
							cv[pasteSeq][s] = cvCPbuffer[s];
							gate1[pasteSeq][s] = gate1CPbuffer[s];
							gate2[pasteSeq][s] = gate2CPbuffer[s];
							slide[pasteSeq][s] = slideCPbuffer[s];
						}
						pendingPaste = 0;
					}
				}*/
			}
		}	
		
		// CV and gates outputs
		if (running) {
			float slideOffset = (slideStepsRemain > 0ul ? (slideCVdelta * (float)slideStepsRemain) : 0.0f);
			if (editingSequence) {// editing sequence while running
				outputs[CV_OUTPUT].value = cv[sequence][stepIndexRun] - slideOffset;
				outputs[GATE1_OUTPUT].value = (clockTrigger.isHigh() && gate1[sequence][stepIndexRun]) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (clockTrigger.isHigh() && gate2[sequence][stepIndexRun]) ? 10.0f : 0.0f;
			}
			else {// editing song while running
				outputs[CV_OUTPUT].value = cv[phrase[phraseIndexRun]][stepIndexPhraseRun] - slideOffset;
				outputs[GATE1_OUTPUT].value = (clockTrigger.isHigh() && gate1[phrase[phraseIndexRun]][stepIndexPhraseRun]) ? 10.0f : 0.0f;
				outputs[GATE2_OUTPUT].value = (clockTrigger.isHigh() && gate2[phrase[phraseIndexRun]][stepIndexPhraseRun]) ? 10.0f : 0.0f;
			}
		}
		else {// not running 
			if (editingSequence) {// editing sequence while not running
				outputs[CV_OUTPUT].value = cv[sequence][stepIndexEdit];
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
			phraseIndexEdit = 0;
			phraseIndexRun = 0;
			stepIndexPhraseRun = 0;
			resetLight = 1.0f;
			//pendingPaste = 0;
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
				else
					lights[STEP_PHRASE_LIGHTS + (i<<1)].value = ((running && (i == phraseIndexRun)) ? 1.0f : 0.0f);
				// Edit cursor (red)
				if (editingSequence)
					lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = (i == stepIndexEdit ? 1.0f : 0.0f);
				else
					lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = (i == phraseIndexEdit ? 1.0f : 0.0f);
			}
		}
	
		// Octave lights
		int octLightIndex = -1;
		if (editingSequence)
			octLightIndex = (int) floor(cv[sequence][stepIndexEdit] + 3.0f);
		for (int i = 0; i < 7; i++) {
			lights[OCTAVE_LIGHTS + i].value = (i == (6 - octLightIndex) ? 1.0f : 0.0f);
		}
		
		// Keyboard lights
		int keyLightIndex = -1;
		if (editingSequence) {
			float cvValOffset = cv[sequence][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
			keyLightIndex = (int) clamp(  roundf( (cvValOffset-floor(cvValOffset)) * 12.0f ),  0.0f,  11.0f);
		}
		for (int i = 0; i < 12; i++) {
			lights[KEY_LIGHTS + i].value = (i == keyLightIndex ? 1.0f : 0.0f);
		}		
		
		// Gate1, Gate2 and Slide lights
		lights[GATE1_LIGHT].value = (editingSequence && gate1[sequence][stepIndexEdit]) ? 1.0f : 0.0f;
		lights[GATE2_LIGHT].value = (editingSequence && gate2[sequence][stepIndexEdit]) ? 1.0f : 0.0f;
		lights[SLIDE_LIGHT].value = (editingSequence && slide[sequence][stepIndexEdit]) ? 1.0f : 0.0f;

		// Attach light
		lights[ATTACH_LIGHT].value = running ? attach : 0.0f;
		
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	

		// Pending paste light
		//lights[PENDING_LIGHT].value = (pendingPaste == 0 ? 0.0f : 1.0f);
		
		if (editingGate > 0ul)
			editingGate--;
		if (editingLength > 0ul)
			editingLength--;
		if (editingPlayMode > 0ul)
			editingPlayMode--;
		if (slideStepsRemain > 0ul)
			slideStepsRemain--;
	}
};


struct PhraseSeq16Widget : ModuleWidget {

	struct SequenceDisplayWidget : TransparentWidget {
		PhraseSeq16 *module;
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
			if (module->editingPlayMode > 0ul) {
				if (module->params[PhraseSeq16::EDIT_PARAM].value > 0.5f) { // if editing sequence
					displayStr[0] = 'E';
					displayStr[1] = (char) module->playModeSeq + 0x30;// TODO change this to the proper string
				}
				else {
					displayStr[0] = 'O';
					displayStr[1] = (char) module->playModeSong + 0x30;// TODO change this to the proper string
				}
				displayStr[2] = 0;
			}
			else {
				snprintf(displayStr, 3, "%2u", (unsigned) (module->params[PhraseSeq16::EDIT_PARAM].value > 0.5f ? 
					module->sequence : module->phrase[module->phraseIndexEdit]) + 1 );
			}
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	PhraseSeq16Widget(PhraseSeq16 *module) : ModuleWidget(module) {
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/PhraseSeq16.svg")));

		// Screw holes
		addChild(new ScrewHole(Vec(15, 0)));
		addChild(new ScrewHole(Vec(box.size.x-30, 0)));
		addChild(new ScrewHole(Vec(15, 365)));
		addChild(new ScrewHole(Vec(box.size.x-30, 365)));
		
		// Screws
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 365)));

		
		// ****** Top portion ******
		
		static const int rowRulerT0 = 48;
		static const int columnRulerT0 = 15;// Length button
		static const int columnRulerT1 = columnRulerT0 + 47;// Left/Right buttons
		static const int columnRulerT2 = columnRulerT1 + 79;// Step/Phase lights

		// Length button
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerT0 + offsetCKD6, rowRulerT0 + offsetCKD6), module, PhraseSeq16::LENGTH_PARAM, 0.0f, 1.0f, 0.0f));
		// Left/Right buttons
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerT1 + offsetCKD6, rowRulerT0 + offsetCKD6), module, PhraseSeq16::LEFT_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerT1 + 38 + offsetCKD6, rowRulerT0 + offsetCKD6), module, PhraseSeq16::RIGHT_PARAM, 0.0f, 1.0f, 0.0f));
		// Step/Phrase lights
		static const int spLightsSpacing = 15;
		for (int i = 0; i < 16; i++) {
			addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(columnRulerT2 + spLightsSpacing * i + offsetMediumLight, rowRulerT0 + offsetMediumLight), module, PhraseSeq16::STEP_PHRASE_LIGHTS + (i*2)));
		}
		
		
		// ****** Middle keyboard portion ******
		
		static const int rowRulerMK0 = 82;
		static const int rowRulerMK1 = rowRulerMK0 + 82;
		static const int columnRulerMK0 = 15;// Octave lights
		static const int columnRulerMK1 = columnRulerMK0 + 264;// Edit mode and run/reset switches
		static const int columnRulerMK2 = 350;// Sequence display and knob



		// Octave LED buttons
		static const float octLightsIntY = 21.0f;
		for (int i = 0; i < 7; i++) {
			addParam(ParamWidget::create<LEDButton>(Vec(columnRulerMK0 + 3, rowRulerMK0 + 6 + i * octLightsIntY- 4.4f), module, PhraseSeq16::OCTAVE_PARAM + i, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMK0 + 3 + 4.4f, rowRulerMK0 + 6 + i * octLightsIntY), module, PhraseSeq16::OCTAVE_LIGHTS + i));
		}
		// Keys and Key lights
		static const int offsetKeyLEDx = 6;
		static const int offsetKeyLEDy = 28;
		// Black keys and lights
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(65, 89), module, PhraseSeq16::KEY_PARAMS + 1, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(65+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 1));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(93, 89), module, PhraseSeq16::KEY_PARAMS + 3, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(93+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 3));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(150, 89), module, PhraseSeq16::KEY_PARAMS + 6, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(150+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 6));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(178, 89), module, PhraseSeq16::KEY_PARAMS + 8, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(178+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 8));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(206, 89), module, PhraseSeq16::KEY_PARAMS + 10, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(206+offsetKeyLEDx, 89+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 10));
		// White keys and lights
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(51, 139), module, PhraseSeq16::KEY_PARAMS + 0, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(51+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 0));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(79, 139), module, PhraseSeq16::KEY_PARAMS + 2, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(79+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 2));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(107, 139), module, PhraseSeq16::KEY_PARAMS + 4, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(107+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 4));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(136, 139), module, PhraseSeq16::KEY_PARAMS + 5, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(136+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 5));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(164, 139), module, PhraseSeq16::KEY_PARAMS + 7, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(164+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 7));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(192, 139), module, PhraseSeq16::KEY_PARAMS + 9, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(192+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 9));
		addParam(ParamWidget::create<InvisibleKeySmall>(			Vec(220, 139), module, PhraseSeq16::KEY_PARAMS + 11, 0.0, 1.0, 0.0));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(220+offsetKeyLEDx, 139+offsetKeyLEDy), module, PhraseSeq16::KEY_LIGHTS + 11));
		// Edit mode switch
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerMK1 + hOffsetCKSS, rowRulerMK0 + 17 + vOffsetCKSS), module, PhraseSeq16::EDIT_PARAM, 0.0f, 1.0f, 1.0f));
		// Run LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerMK1 - 17 + offsetLEDbezel, rowRulerMK1 + offsetLEDbezel), module, PhraseSeq16::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerMK1 - 17 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK1 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq16::RUN_LIGHT));
		// Reset LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(columnRulerMK1 + 17 + offsetLEDbezel, rowRulerMK1 + offsetLEDbezel), module, PhraseSeq16::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(columnRulerMK1 + 17 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK1 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq16::RESET_LIGHT));
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(columnRulerMK2-7, rowRulerMK0 + 20 + vOffsetDisplay);
		displaySequence->box.size = Vec(40, 30);// 2 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Sequence knob
		addParam(ParamWidget::create<Davies1900hBlackKnobNoTick>(Vec(columnRulerMK2 + 1 + offsetDavies1900, rowRulerMK0 + 72 + offsetDavies1900), module, PhraseSeq16::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f));		
		
		
		// ****** Middle Buttons portion ******
		
		static const int rowRulerMB0 = 214;
		static const int rowRulerMB1 = rowRulerMB0 + 46;
		static const int columnRulerMB0 = 22;// Oct
		static const int columnRulerMBspacing = 62;
		static const int columnRulerMB1 = columnRulerMB0 + 54;// Gate1
		static const int columnRulerMB2 = columnRulerMB1 + columnRulerMBspacing;// Gate2
		static const int columnRulerMB3 = columnRulerMB2 + columnRulerMBspacing;// Slide
		static const int columnRulerMB4 = columnRulerMK1 - 2;// Attach and rotate
		static const int columnRulerMB5 = columnRulerMK2;// Copy-paste and paste sync
		
		// Gate 1 light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB1 + 25 + offsetMediumLight, rowRulerMB0 + 5 + offsetMediumLight), module, PhraseSeq16::GATE1_LIGHT));		
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerMB1 + offsetCKD6, rowRulerMB0 + 5 + offsetCKD6), module, PhraseSeq16::GATE1_PARAM, 0.0f, 1.0f, 0.0f));
		// Gate 2 light and button
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB2 + 25 + offsetMediumLight, rowRulerMB0 + 5 + offsetMediumLight), module, PhraseSeq16::GATE2_LIGHT));		
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerMB2 + offsetCKD6, rowRulerMB0 + 5 + offsetCKD6), module, PhraseSeq16::GATE2_PARAM, 0.0f, 1.0f, 0.0f));
		// Slide light, button and knob
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB3 + 25 + offsetMediumLight, rowRulerMB0 + 5 + offsetMediumLight), module, PhraseSeq16::SLIDE_LIGHT));		
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerMB3 + offsetCKD6, rowRulerMB0 + 5 + offsetCKD6), module, PhraseSeq16::SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(columnRulerMB4 + offsetRoundSmallBlackKnob, rowRulerMB0 + offsetRoundSmallBlackKnob), module, PhraseSeq16::SLIDE_KNOB_PARAM, 0.0f, 2.0f, 0.25f));// slide time in seconds		
		// Attach button and light
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB4 - 10, rowRulerMB1 + offsetTL1105), module, PhraseSeq16::ATTACH_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(columnRulerMB4 + 14 + offsetMediumLight, rowRulerMB1 - 1 + offsetMediumLight), module, PhraseSeq16::ATTACH_LIGHT));		
		// Copy/paste buttons
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB5 - 10, rowRulerMB1 + offsetTL1105), module, PhraseSeq16::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB5 + 20, rowRulerMB1 + offsetTL1105), module, PhraseSeq16::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		// Autostep
		addParam(ParamWidget::create<CKSS>(Vec(columnRulerMB0 + 2 + hOffsetCKSS, rowRulerMB1 + 3 + vOffsetCKSS), module, PhraseSeq16::AUTOSTEP_PARAM, 0.0f, 1.0f, 0.0f));		
		// Play mode	
		addParam(ParamWidget::create<CKD6>(Vec(columnRulerMB5 + offsetCKD6, rowRulerMB0 + offsetCKD6), module, PhraseSeq16::PLAYMODE_PARAM, 0.0f, 1.0f, 0.0f));

		
		// Transpose
		//addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB1 - 3 + offsetTL1105, rowRulerMB1 + 1 + offsetTL1105), module, PhraseSeq16::TRANSPOSED_PARAM, 0.0f, 1.0f, 0.0f));
		//addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB2 + 22 + offsetTL1105, rowRulerMB1 + 1 + offsetTL1105), module, PhraseSeq16::TRANSPOSEU_PARAM, 0.0f, 1.0f, 0.0f));
		// Rotate buttons
		//addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB4 - 10, rowRulerMB1 - 1 + offsetTL1105), module, PhraseSeq16::ROTATEL_PARAM, 0.0f, 1.0f, 0.0f));
		//addParam(ParamWidget::create<TL1105>(Vec(columnRulerMB4 + 20, rowRulerMB1 - 1 + offsetTL1105), module, PhraseSeq16::ROTATER_PARAM, 0.0f, 1.0f, 0.0f));
		// Paste sync (and light)
		//addParam(ParamWidget::create<CKSSThreeInv>(Vec(columnRulerMB5 - 6 + hOffsetCKSS, rowRulerMB1 - 1 + vOffsetCKSSThree), module, PhraseSeq16::PASTESYNC_PARAM, 0.0f, 2.0f, 0.0f));	
		//addChild(ModuleLightWidget::create<SmallLight<RedLight>>(Vec(columnRulerMB5 - 6 + 41, rowRulerMB1 - 1 + 14), module, PhraseSeq16::PENDING_LIGHT));
		
						
		// ****** Bottom portion ******
		
		static const int rowRulerB0 = 315;
		static const int columnRulerB6 = columnRulerMB5;
		static const int outputJackSpacingX = 54;
		static const int columnRulerB5 = columnRulerB6 - outputJackSpacingX;
		static const int columnRulerB4 = columnRulerB5 - outputJackSpacingX;
		static const int columnRulerB0 = 22;
		static const int columnRulerB1 = columnRulerB0 + outputJackSpacingX;
		static const int columnRulerB2 = columnRulerB1 + outputJackSpacingX;
		static const int columnRulerB3 = columnRulerB2 + outputJackSpacingX;

		
		
		// Inputs first row
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB1, rowRulerMB1 + 5), Port::INPUT, module, PhraseSeq16::LEFTCV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB2, rowRulerMB1 + 5), Port::INPUT, module, PhraseSeq16::RIGHTCV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB3, rowRulerMB1 + 5), Port::INPUT, module, PhraseSeq16::SEQUENCECV_INPUT));
		// Inputs 2nd row (bottom)
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB0, rowRulerB0), Port::INPUT, module, PhraseSeq16::WRITE_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB1, rowRulerB0), Port::INPUT, module, PhraseSeq16::CV_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB2, rowRulerB0), Port::INPUT, module, PhraseSeq16::RESET_INPUT));
		addInput(Port::create<PJ301MPortS>(Vec(columnRulerB3, rowRulerB0), Port::INPUT, module, PhraseSeq16::CLOCK_INPUT));
		// Outputs
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB4, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::CV_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB5, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::GATE1_OUTPUT));
		addOutput(Port::create<PJ301MPortS>(Vec(columnRulerB6, rowRulerB0), Port::OUTPUT, module, PhraseSeq16::GATE2_OUTPUT));

	}
};

Model *modelPhraseSeq16 = Model::create<PhraseSeq16, PhraseSeq16Widget>("Impromptu Modular", "Phrase-Seq-16", "Phrase-Seq-16", SEQUENCER_TAG);
