//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"

using namespace rack;


int moveIndexEx(int index, int indexNext, int numSteps);
	
	
class StepAttributes {
	unsigned long attributes;
	
	public:

	static const unsigned long ATT_MSK_GATE =      0x01000000, gateShift = 24;
	static const unsigned long ATT_MSK_GATEP =     0x02000000;
	static const unsigned long ATT_MSK_SLIDE =     0x04000000;
	static const unsigned long ATT_MSK_TIED =      0x08000000;
	static const unsigned long ATT_MSK_GATETYPE =  0xF0000000, gateTypeShift = 28;
	static const unsigned long ATT_MSK_VELOCITY =  0x000000FF, velocityShift = 0;
	static const unsigned long ATT_MSK_GATEP_VAL = 0x0000FF00, gatePValShift = 8;
	static const unsigned long ATT_MSK_SLIDE_VAL = 0x00FF0000, slideValShift = 16;

	static const unsigned long ATT_MSK_INITSTATE = ((ATT_MSK_GATE) | (128 << velocityShift) | (50 << gatePValShift) | (10 << slideValShift));

	inline void clear() {attributes = 0ul;}
	inline void init() {attributes = ATT_MSK_INITSTATE;}
	inline void randomize() {attributes = ( ((randomu32() & 0xF) << gateShift) | ((randomu32() % 101) << gatePValShift) | ((randomu32() % 101) << slideValShift) | ((randomu32() & 0xFF) << velocityShift) );}
	
	inline bool getGate() {return (attributes & ATT_MSK_GATE) != 0;}
	inline int getGateType() {return (int)((attributes & ATT_MSK_GATETYPE) >> gateTypeShift);}
	inline bool getTied() {return (attributes & ATT_MSK_TIED) != 0;}
	inline bool getGateP() {return (attributes & ATT_MSK_GATEP) != 0;}
	inline int getGatePVal() {return (int)((attributes & ATT_MSK_GATEP_VAL) >> gatePValShift);}
	inline bool getSlide() {return (attributes & ATT_MSK_SLIDE) != 0;}
	inline int getSlideVal() {return (int)((attributes & ATT_MSK_SLIDE_VAL) >> slideValShift);}
	inline int getVelocityVal() {return (int)((attributes & ATT_MSK_VELOCITY) >> velocityShift);}
	inline float getVelocity() {return ((float)getVelocityVal()) / 25.5f;}
	inline unsigned long getAttribute() {return attributes;}

	inline void setGate(bool gate1State) {attributes &= ~ATT_MSK_GATE; if (gate1State) attributes |= ATT_MSK_GATE;}
	inline void setGateType(int gateType) {attributes &= ~ATT_MSK_GATETYPE; attributes |= (((unsigned long)gateType) << gateTypeShift);}
	inline void setTied(bool tiedState) {attributes &= ~ATT_MSK_TIED; if (tiedState) attributes |= ATT_MSK_TIED;}
	inline void setGateP(bool GatePState) {attributes &= ~ATT_MSK_GATEP; if (GatePState) attributes |= ATT_MSK_GATEP;}
	inline void setGatePVal(int gatePval) {attributes &= ~ATT_MSK_GATEP_VAL; attributes |= (((unsigned long)gatePval) << gatePValShift);}
	inline void setSlide(bool slideState) {attributes &= ~ATT_MSK_SLIDE; if (slideState) attributes |= ATT_MSK_SLIDE;}
	inline void setSlideVal(int slideVal) {attributes &= ~ATT_MSK_SLIDE_VAL; attributes |= (((unsigned long)slideVal) << slideValShift);}
	inline void setVelocityVal(int _velocity) {attributes &= ~ATT_MSK_VELOCITY; attributes |= (((unsigned long)_velocity) << velocityShift);}
	inline void setVelocity(float _velocityf) {setVelocityVal((int)(_velocityf * 25.5f + 0.5f));}
	inline void setAttribute(unsigned long _attributes) {attributes = _attributes;}

	inline void toggleGate() {attributes ^= ATT_MSK_GATE;}
	inline void toggleTied() {attributes ^= ATT_MSK_TIED;}
	inline void toggleGateP() {attributes ^= ATT_MSK_GATEP;}
	inline void toggleSlide() {attributes ^= ATT_MSK_SLIDE;}	
	
	inline void applyTied() {attributes &= ~(ATT_MSK_GATE | ATT_MSK_GATEP | ATT_MSK_SLIDE);}// clear other attributes if tied
};


//*****************************************************************************


class Phrase {
	// a phrase is a sequence number and a number of repetitions; it is used to make a song
	unsigned long phrase;
	
	public:

	static const unsigned long PHR_MSK_SEQNUM = 0x00FF;
	static const unsigned long PHR_MSK_REPS =   0xFF00, repShift = 8;// a rep is 1 to 99
	
	inline void init() {phrase = (1 << repShift);}
	inline void randomize(int maxSeqs) {phrase = ((randomu32() % maxSeqs) | ((randomu32() % 4 + 1) << repShift));}
	
	inline int getSeqNum() {return (int)(phrase & PHR_MSK_SEQNUM);}
	inline int getReps() {return (int)((phrase & PHR_MSK_REPS) >> repShift);}
	inline unsigned long getPhraseJson() {return phrase - (1 << repShift);}// compression trick (store 0 instead of 1)
	
	inline void setSeqNum(int seqn) {phrase &= ~PHR_MSK_SEQNUM; phrase |= ((unsigned long)seqn);}
	inline void setReps(int _reps) {phrase &= ~PHR_MSK_REPS; phrase |= (((unsigned long)_reps) << repShift);}
	inline void setPhraseJson(unsigned long _phrase) {phrase = (_phrase + (1 << repShift));}// compression trick (store 0 instead of 1)
};


//*****************************************************************************


class SeqAttributes {
	unsigned long attributes;
	
	public:

	static const unsigned long SEQ_MSK_LENGTH  =   0x0000FF;// number of steps in each sequence, min value is 1
	static const unsigned long SEQ_MSK_RUNMODE =   0x00FF00, runModeShift = 8;
	static const unsigned long SEQ_MSK_TRANSPOSE = 0x7F0000, transposeShift = 16;
	static const unsigned long SEQ_MSK_TRANSIGN  = 0x800000;// manually implement sign bit
	
	inline void init(int length, int runMode) {attributes = ((length) | (((unsigned long)runMode) << runModeShift));}
	inline void randomize(int maxSteps, int numModes) {attributes = ( (1 + (randomu32() % maxSteps)) | (((unsigned long)(randomu32() % numModes) << runModeShift)) );}
	
	inline int getLength() {return (int)(attributes & SEQ_MSK_LENGTH);}
	inline int getRunMode() {return (int)((attributes & SEQ_MSK_RUNMODE) >> runModeShift);}
	inline int getTranspose() {
		int ret = (int)((attributes & SEQ_MSK_TRANSPOSE) >> transposeShift);
		if ( (attributes & SEQ_MSK_TRANSIGN) != 0)// if negative
			ret *= -1;
		return ret;
	}
	inline unsigned long getSeqAttrib() {return attributes;}
	
	inline void setLength(int length) {attributes &= ~SEQ_MSK_LENGTH; attributes |= ((unsigned long)length);}
	inline void setRunMode(int runMode) {attributes &= ~SEQ_MSK_RUNMODE; attributes |= (((unsigned long)runMode) << runModeShift);}
	inline void setTranspose(int transp) {
		attributes &= ~ (SEQ_MSK_TRANSPOSE | SEQ_MSK_TRANSIGN); 
		attributes |= (((unsigned long)abs(transp)) << transposeShift);
		if (transp < 0) 
			attributes |= SEQ_MSK_TRANSIGN;
	}
	inline void setSeqAttrib(unsigned long _attributes) {attributes = _attributes;}
};


//*****************************************************************************


class SequencerKernel {
	public: 
	
	
	// General constants
	// ----------------

	// Sequencer dimensions
	static const int MAX_STEPS = 32;
	static const int MAX_SEQS = 64;
	static const int MAX_PHRASES = 99;// maximum value is 99 (disp will be 1 to 99)

	// Run modes
	enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_PEN, MODE_BRN, MODE_RND, NUM_MODES};
	static const std::string modeLabels[NUM_MODES];
	
	// Gate types
	static const int NUM_GATES = 12;	
	static const uint64_t advGateHitMaskLow[NUM_GATES];		
	static const uint64_t advGateHitMaskHigh[NUM_GATES];

	static constexpr float INIT_CV = 0.0f;


	private:
	

	// Member data
	// ----------------	
	int id;
	std::string ids;
	
	// Need to save
	int pulsesPerStep;// stored range is [1:49] so must ALWAYS read thgouth getPulsesPerStep(). Must do this because of knob
	int delay;
	int runModeSong;	
	int songBeginIndex;
	int songEndIndex;
	Phrase phrases[MAX_PHRASES];// This is the song (series of phases; a phrase is a sequence number and a repetition value)	
	SeqAttributes sequences[MAX_SEQS];
	float cv[MAX_SEQS][MAX_STEPS];// [-3.0 : 3.917]. First index is sequence number, 2nd index is step
	StepAttributes attributes[MAX_SEQS][MAX_STEPS];// First index is sequence number, 2nd index is step
	
	// No need to save
	int stepIndexRun;
	unsigned long stepIndexRunHistory;
	int phraseIndexRun;
	unsigned long phraseIndexRunHistory;
	int ppqnCount;
	int ppqnLeftToSkip;// used in clock delay
	int gateCode;// -1 = Killed for all pulses of step, 0 = Low for current pulse of step, 1 = High for current pulse of step, 2 = Clk high pulse, 3 = 1ms trig
	unsigned long slideStepsRemain;// 0 when no slide under way, downward step counter when sliding
	float slideCVdelta;// no need to initialize, this is only used when slideStepsRemain is not 0
	
	
	public: 
	
	
	// get
	// ----------------
	inline int getRunModeSong() {return runModeSong;}
	inline int getRunModeSeq(int seqn) {return sequences[seqn].getRunMode();}
	inline int getBegin() {return songBeginIndex;}
	inline int getEnd() {return songEndIndex;}
	inline int getLength(int seqn) {return sequences[seqn].getLength();}
	inline int getPhraseSeq(int phrn) {return phrases[phrn].getSeqNum();}
	inline int getPhraseReps(int phrn) {return phrases[phrn].getReps();}
	inline int getPulsesPerStep() {return (pulsesPerStep > 2 ? ((pulsesPerStep - 1) << 1) : pulsesPerStep);}
	inline int getDelay() {return delay;}
	inline int getTransposeOffset(int seqn) {return sequences[seqn].getTranspose();}
	inline int getStepIndexRun() {return stepIndexRun;}
	inline int getPhraseIndexRun() {return phraseIndexRun;}
	inline float getCV(int seqn, int stepn) {return cv[seqn][stepn];}
	inline float getCVRun() {return cv[phrases[phraseIndexRun].getSeqNum()][stepIndexRun];}
	inline StepAttributes getAttribute(int seqn, int stepn) {return attributes[seqn][stepn];}
	inline StepAttributes getAttributeRun() {return attributes[phrases[phraseIndexRun].getSeqNum()][stepIndexRun];}
	inline bool getTied(int seqn, int stepn) {return attributes[seqn][stepn].getTied();}
	inline int getGatePVal(int seqn, int stepn) {return attributes[seqn][stepn].getGatePVal();}
	inline int getSlideVal(int seqn, int stepn) {return attributes[seqn][stepn].getSlideVal();}
	inline int getVelocityVal(int seqn, int stepn) {return attributes[seqn][stepn].getVelocityVal();}
	inline float getVelocity(int seqn, int stepn) {return attributes[seqn][stepn].getVelocity();}
	inline float getVelocityRun() {return getAttributeRun().getVelocity();}
	inline int getGateType(int seqn, int stepn) {return attributes[seqn][stepn].getGateType();}
	
	
	// Set
	// ----------------
	inline void setLength(int seqn, int _length) {sequences[seqn].setLength(_length);}
	inline void setBegin(int phrn) {songBeginIndex = phrn; songEndIndex = max(phrn, songEndIndex);}
	inline void setEnd(int phrn) {songEndIndex = phrn; songBeginIndex = min(phrn, songBeginIndex);}
	inline void setGatePVal(int seqn, int stepn, int gatePval) {attributes[seqn][stepn].setGatePVal(gatePval);}
	inline void setSlideVal(int seqn, int stepn, int slideVal) {attributes[seqn][stepn].setSlideVal(slideVal);}
	inline void setVelocityVal(int seqn, int stepn, int velocity) {attributes[seqn][stepn].setVelocityVal(velocity);}
	inline void setVelocity(int seqn, int stepn, float velocity) {attributes[seqn][stepn].setVelocity(velocity);}
	inline void setGateType(int seqn, int stepn, int gateType) {attributes[seqn][stepn].setGateType(gateType);}

	
	// Mod, inc, dec, toggle, etc
	// ----------------
	inline void modRunModeSong(int delta) {runModeSong += delta; if (runModeSong < 0) runModeSong = 0; if (runModeSong >= NUM_MODES) runModeSong = NUM_MODES - 1;}
	inline void modRunModeSeq(int seqn, int delta) {
		int rVal = sequences[seqn].getRunMode();
		rVal += delta;
		if (rVal < 0) rVal = 0;
		if (rVal >= NUM_MODES) rVal = NUM_MODES - 1;
		sequences[seqn].setRunMode(rVal);
	}
	inline void modLength(int seqn, int delta) {
		int lVal = sequences[seqn].getLength();
		lVal += delta; 
		if (lVal > MAX_STEPS) lVal = MAX_STEPS; 
		if (lVal < 1 ) lVal = 1;
		sequences[seqn].setLength(lVal);
	}
	inline void modPhraseSeqNum(int phrn, int delta) {
		int seqn = phrases[phrn].getSeqNum();
		seqn = moveIndexEx(seqn, seqn + delta, MAX_SEQS);
		phrases[phrn].setSeqNum(seqn);
	}
	inline void modPhraseReps(int phrn, int delta) {
		int rVal = phrases[phrn].getReps();
		rVal += delta; 
		if (rVal > 99) rVal = 99; 
		if (rVal < 1 ) rVal = 1;
		phrases[phrn].setReps(rVal);
	}		
	inline void modPulsesPerStep(int delta) {
		pulsesPerStep += delta;
		if (pulsesPerStep < 1) pulsesPerStep = 1;
		if (pulsesPerStep > 49) pulsesPerStep = 49;
	}
	inline void modDelay(int delta) {
		delay += delta;
		if (delay < 0) delay = 0;
		if (delay > 99) delay = 99;
	}
	inline void modGatePVal(int seqn, int stepn, int delta) {
		int pVal = getGatePVal(seqn, stepn);
		pVal += delta;
		if (pVal > 100) pVal = 100;
		if (pVal < 0) pVal = 0;
		setGatePVal(seqn, stepn, pVal);						
	}		
	inline void modSlideVal(int seqn, int stepn, int delta) {
		int sVal = getSlideVal(seqn, stepn);
		sVal += delta;
		if (sVal > 100) sVal = 100;
		if (sVal < 0) sVal = 0;
		setSlideVal(seqn, stepn, sVal);
	}		
	inline void modVelocityVal(int seqn, int stepn, int delta) {
		int vVal = getVelocityVal(seqn, stepn);
		vVal += delta;
		if (vVal > 255) vVal = 255;
		if (vVal < 0) vVal = 0;
		setVelocityVal(seqn, stepn, vVal);						
	}		
	inline void decSlideStepsRemain() {if (slideStepsRemain > 0ul) slideStepsRemain--;}	
	inline void toggleGate(int seqn, int stepn) {attributes[seqn][stepn].toggleGate();}
	inline void toggleTied(int seqn, int stepn) {// will clear other attribs if new state is on
		attributes[seqn][stepn].toggleTied();
		if (attributes[seqn][stepn].getTied()) {
			attributes[seqn][stepn].setGate(false);
			attributes[seqn][stepn].setGateP(false);
			attributes[seqn][stepn].setSlide(false);
			applyTiedStep(seqn, stepn);
		}
	}
	inline void toggleGateP(int seqn, int step) {attributes[seqn][step].toggleGateP();}
	inline void toggleSlide(int seqn, int step) {attributes[seqn][step].toggleSlide();}	
	inline float applyNewOctave(int seqn, int stepn, int newOct) {
		float newCV = cv[seqn][stepn] + 10.0f;//to properly handle negative note voltages
		newCV = newCV - floor(newCV) + (float) (newOct - 3);
		if (newCV >= -3.0f && newCV < 4.0f) {
			cv[seqn][stepn] = newCV;
			applyTiedStep(seqn, stepn);
		}
		return newCV;
	}
	inline float applyNewKey(int seqn, int stepn, int newKeyIndex) {
		float newCV = floor(cv[seqn][stepn]) + ((float) newKeyIndex) / 12.0f;
		cv[seqn][stepn] = newCV;
		applyTiedStep(seqn, stepn);
		return newCV;
	}
	inline float writeCV(int seqn, int stepn, float newCV) {
		cv[seqn][stepn] = newCV;
		applyTiedStep(seqn, stepn);
		return cv[seqn][stepn];// may have changed with the applyTiedStep so must return
	}

	
	// Calc
	// ----------------
	inline float calcSlideOffset() {return (slideStepsRemain > 0ul ? (slideCVdelta * (float)slideStepsRemain) : 0.0f);}
	inline bool calcGate(SchmittTrigger clockTrigger, unsigned long clockStep, float sampleRate) {
		if (gateCode < 2) 
			return gateCode == 1;
		if (gateCode == 2)
			return clockTrigger.isHigh();
		return clockStep < (unsigned long) (sampleRate * 0.001f);
	}
	
	
	// Init
	// ----------------
	inline void initSequence(int seqn) {
		sequences[seqn].init(MAX_STEPS, MODE_FWD);
		for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
			cv[seqn][stepn] = INIT_CV;
			attributes[seqn][stepn].init();
		}
	}
	inline void initSong() {
		runModeSong = MODE_FWD;
		songBeginIndex = 0;
		songEndIndex = 0;
		for (int phrn = 0; phrn < MAX_PHRASES; phrn++) {
			phrases[phrn].init();
		}
	}

	
	// Randomize and staircase
	// ----------------
	inline void randomizeSequence(int seqn) {
		sequences[seqn].randomize(MAX_STEPS, NUM_MODES);// code below uses lengths so this must be randomized first
		for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
			cv[seqn][stepn] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
			attributes[seqn][stepn].randomize();
			if (attributes[seqn][stepn].getTied()) {
				attributes[seqn][stepn].applyTied();
				applyTiedStep(seqn, stepn);
			}	
		}
	}
	inline void randomizeSong() {
		runModeSong = randomu32() % NUM_MODES;
		songBeginIndex = 0;
		songEndIndex = (randomu32() % MAX_PHRASES);
		for (int phrn = 0; phrn < MAX_PHRASES; phrn++) {
			phrases[phrn].randomize(MAX_SEQS);
		}
	}	

	
	// Copy-paste sequence or song
	// ----------------
	inline void copySequence(float* cvCPbuffer, StepAttributes* attribCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int seqn, int startCP, int countCP) {
		for (int i = 0, stepn = startCP; i < countCP; i++, stepn++) {
			cvCPbuffer[i] = cv[seqn][stepn];
			attribCPbuffer[i] = attributes[seqn][stepn];
		}
		*seqPhraseAttribCPbuffer = sequences[seqn];
		seqPhraseAttribCPbuffer->setTranspose(-1);// so that a cross paste can be detected
	}
	inline void pasteSequence(float* cvCPbuffer, StepAttributes* attribCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int seqn, int startCP, int countCP) {
		for (int i = 0, stepn = startCP; i < countCP; i++, stepn++) {
			cv[seqn][stepn] = cvCPbuffer[i];
			attributes[seqn][stepn] = attribCPbuffer[i];
		}
		if (countCP == MAX_STEPS) {// all
			sequences[seqn] = *seqPhraseAttribCPbuffer;
			sequences[seqn].setTranspose(0);
		}
	}
	inline void copyPhrase(Phrase* phraseCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int startCP, int countCP) {	
		for (int i = 0, phrn = startCP; i < countCP; i++, phrn++) {
			phraseCPbuffer[i] = phrases[phrn];
		}
		seqPhraseAttribCPbuffer->setLength(songBeginIndex);
		seqPhraseAttribCPbuffer->setTranspose(songEndIndex);
		seqPhraseAttribCPbuffer->setRunMode(runModeSong);
	}
	inline void pastePhrase(Phrase* phraseCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int startCP, int countCP) {	
		for (int i = 0, phrn = startCP; i < countCP; i++, phrn++) {
			phrases[phrn] = phraseCPbuffer[i];
		}
		if (countCP == MAX_PHRASES) {// all
			songBeginIndex = seqPhraseAttribCPbuffer->getLength();
			songEndIndex = seqPhraseAttribCPbuffer->getTranspose();
			runModeSong = seqPhraseAttribCPbuffer->getRunMode();
		}
	}
	
	
	// Main methods
	// ----------------
	
	void setId(int _id) {
		id = _id;
		ids = "id" + std::to_string(id) + "_";
	}
	
	
	void reset() {
		pulsesPerStep = 1;
		delay = 0;
		initSong();
		for (int seqn = 0; seqn < MAX_SEQS; seqn++) {
			initSequence(seqn);		
		}
		slideStepsRemain = 0ul;
		// no need to call initRun() here since user of the kernel does it in its onReset() via its initRun()
	}
	
	
	void randomize() {
		randomizeSong();
		for (int seqn = 0; seqn < MAX_SEQS; seqn++) {
			randomizeSequence(seqn);
		}
		// no need to call initRun() here since user of the kernel does it in its onRandomize() via its initRun()
	}
	
	
	void initRun() {
		phraseIndexRun = (runModeSong == MODE_REV ? songEndIndex : songBeginIndex);
		phraseIndexRunHistory = 0;

		int seqn = phrases[phraseIndexRun].getSeqNum();
		stepIndexRun = (sequences[seqn].getRunMode() == MODE_REV ? sequences[seqn].getLength() - 1 : 0);
		stepIndexRunHistory = 0;

		ppqnCount = 0;
		ppqnLeftToSkip = delay;
		calcGateCodeEx(seqn);// uses stepIndexRun as the step
		slideStepsRemain = 0ul;
	}
	
	
	void toJson(json_t *rootJ) {
		// pulsesPerStep
		json_object_set_new(rootJ, (ids + "pulsesPerStep").c_str(), json_integer(pulsesPerStep));

		// delay
		json_object_set_new(rootJ, (ids + "delay").c_str(), json_integer(delay));

		// runModeSong
		json_object_set_new(rootJ, (ids + "runModeSong").c_str(), json_integer(runModeSong));

		// sequences (attributes of a seqs)
		json_t *sequencesJ = json_array();
		for (int i = 0; i < MAX_SEQS; i++)
			json_array_insert_new(sequencesJ, i, json_integer(sequences[i].getSeqAttrib()));
		json_object_set_new(rootJ, (ids + "sequences").c_str(), sequencesJ);

		// phrases 
		json_t *phrasesJ = json_array();
		for (int i = 0; i < MAX_PHRASES; i++)
			json_array_insert_new(phrasesJ, i, json_integer(phrases[i].getPhraseJson()));
		json_object_set_new(rootJ, (ids + "phrases").c_str(), phrasesJ);

		// CV and attributes
		json_t *seqSavedJ = json_array();		
		json_t *cvJ = json_array();
		json_t *attributesJ = json_array();
		for (int seqnRead = 0, seqnWrite = 0; seqnRead < MAX_SEQS; seqnRead++) {
			bool compress = true;
			for (int stepn = 0; stepn < 4; stepn++) {
				if (cv[seqnRead][stepn] != INIT_CV || attributes[seqnRead][stepn].getAttribute() != StepAttributes::ATT_MSK_INITSTATE) {
					compress = false;
					break;
				}
			}
			if (compress) {
				json_array_insert_new(seqSavedJ, seqnRead, json_integer(0));
			}
			else {
				json_array_insert_new(seqSavedJ, seqnRead, json_integer(1));
				for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
					json_array_insert_new(cvJ, stepn + (seqnWrite * MAX_STEPS), json_real(cv[seqnRead][stepn]));
					json_array_insert_new(attributesJ, stepn + (seqnWrite * MAX_STEPS), json_integer(attributes[seqnRead][stepn].getAttribute()));
				}
				seqnWrite++;
			}
		}
		json_object_set_new(rootJ, (ids + "seqSaved").c_str(), seqSavedJ);
		json_object_set_new(rootJ, (ids + "cv").c_str(), cvJ);
		json_object_set_new(rootJ, (ids + "attributes").c_str(), attributesJ);

		// songBeginIndex
		json_object_set_new(rootJ, (ids + "songBeginIndex").c_str(), json_integer(songBeginIndex));

		// songEndIndex
		json_object_set_new(rootJ, (ids + "songEndIndex").c_str(), json_integer(songEndIndex));

	}
	
	
	void fromJson(json_t *rootJ) {
		// pulsesPerStep
		json_t *pulsesPerStepJ = json_object_get(rootJ, (ids + "pulsesPerStep").c_str());
		if (pulsesPerStepJ)
			pulsesPerStep = json_integer_value(pulsesPerStepJ);

		// delay
		json_t *delayJ = json_object_get(rootJ, (ids + "delay").c_str());
		if (delayJ)
			delay = json_integer_value(delayJ);

		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, (ids + "runModeSong").c_str());
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
				
		// sequences (attributes of a seqs)
		json_t *sequencesJ = json_object_get(rootJ, (ids + "sequences").c_str());
		if (sequencesJ) {
			for (int i = 0; i < MAX_SEQS; i++)
			{
				json_t *sequencesArrayJ = json_array_get(sequencesJ, i);
				if (sequencesArrayJ)
					sequences[i].setSeqAttrib(json_integer_value(sequencesArrayJ));
			}			
		}		
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, (ids + "phrases").c_str());
		if (phrasesJ)
			for (int i = 0; i < MAX_PHRASES; i++)
			{
				json_t *phrasesArrayJ = json_array_get(phrasesJ, i);
				if (phrasesArrayJ)
					phrases[i].setPhraseJson(json_integer_value(phrasesArrayJ));
			}
		
		// CV and attributes
		json_t *seqSavedJ = json_object_get(rootJ, (ids + "seqSaved").c_str());
		int seqSaved[MAX_SEQS];
		if (seqSavedJ) {
			int i;
			for (i = 0; i < MAX_SEQS; i++)
			{
				json_t *seqSavedArrayJ = json_array_get(seqSavedJ, i);
				if (seqSavedArrayJ)
					seqSaved[i] = json_integer_value(seqSavedArrayJ);
				else 
					break;
			}	
			if (i == MAX_SEQS) {			
				json_t *cvJ = json_object_get(rootJ, (ids + "cv").c_str());
				json_t *attributesJ = json_object_get(rootJ, (ids + "attributes").c_str());
				if (cvJ && attributesJ) {
					for (int seqnFull = 0, seqnComp = 0; seqnFull < MAX_SEQS; seqnFull++) {
						if (!seqSaved[seqnFull]) {
							continue;
						}
						for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
							json_t *cvArrayJ = json_array_get(cvJ, stepn + (seqnComp * MAX_STEPS));
							if (cvArrayJ)
								cv[seqnFull][stepn] = json_number_value(cvArrayJ);
							json_t *attributesArrayJ = json_array_get(attributesJ, stepn + (seqnComp * MAX_STEPS));
							if (attributesArrayJ)
								attributes[seqnFull][stepn].setAttribute(json_integer_value(attributesArrayJ));
						}
						seqnComp++;
					}
				}
			}
		}		
		
		// songBeginIndex
		json_t *songBeginIndexJ = json_object_get(rootJ, (ids + "songBeginIndex").c_str());
		if (songBeginIndexJ)
			songBeginIndex = json_integer_value(songBeginIndexJ);
				
		// songEndIndex
		json_t *songEndIndexJ = json_object_get(rootJ, (ids + "songEndIndex").c_str());
		if (songEndIndexJ)
			songEndIndex = json_integer_value(songEndIndexJ);
	}
	
	
	void clockStep(unsigned long clockPeriod) {
		if (ppqnLeftToSkip > 0) {
			ppqnLeftToSkip--;
		}
		else {
			ppqnCount++;
			int ppsFiltered = getPulsesPerStep();// must use method
			if (ppqnCount >= ppsFiltered)
				ppqnCount = 0;
			if (ppqnCount == 0) {
				float slideFromCV = getCVRun();
				if (moveIndexRunMode(true)) {// true means seq
					moveIndexRunMode(false); // false means song
					SeqAttributes newSeq = sequences[phrases[phraseIndexRun].getSeqNum()];
					stepIndexRun = (newSeq.getRunMode() == MODE_REV ? newSeq.getLength() - 1 : 0);// must always refresh after phraseIndexRun has changed
				}

				// Slide
				StepAttributes attribRun = getAttributeRun();
				if (attribRun.getSlide()) {
					slideStepsRemain = (unsigned long) (((float)clockPeriod * ppsFiltered) * ((float)attribRun.getSlideVal() / 100.0f));
					float slideToCV = getCVRun();
					slideCVdelta = (slideToCV - slideFromCV)/(float)slideStepsRemain;
				}
			}
			calcGateCodeEx(phrases[phraseIndexRun].getSeqNum());// uses stepIndexRun as the step		
		}
	}
	
	
	int keyIndexToGateTypeEx(int keyIndex) {// return -1 when invalid gate type given current pps setting
		int ppsFiltered = getPulsesPerStep();// must use method
		int ret = keyIndex;
		
		if (keyIndex == 1 || keyIndex == 3 || keyIndex == 6 || keyIndex == 8 || keyIndex == 10) {// black keys
			if ((ppsFiltered % 6) != 0)
				ret = -1;
		}
		else if (keyIndex == 4 || keyIndex == 7 || keyIndex == 9) {// 75%, DUO, DU2 
			if ((ppsFiltered % 4) != 0)
				ret = -1;
		}
		else if (keyIndex == 2) {// 50%
			if ((ppsFiltered % 2) != 0)
				ret = -1;
		}
		else if (keyIndex == 0) {// 25%
			if (ppsFiltered != 1 && (ppsFiltered % 4) != 0)
				ret = -1;
		}
		//else always good: 5 (full) and 11 (trig)
		
		return ret;
	}


	void transposeSeq(int seqn, int delta) {
		int tVal = sequences[seqn].getTranspose();
		int oldTransposeOffset = tVal;
		tVal += delta;
		if (tVal > 99) tVal = 99;
		if (tVal < -99) tVal = -99;						
		sequences[seqn].setTranspose(tVal);
		
		delta = tVal - oldTransposeOffset;
		if (delta != 0) { 
			float offsetCV = ((float)(delta))/12.0f;
			for (int stepn = 0; stepn < MAX_STEPS; stepn++) 
				cv[seqn][stepn] += offsetCV;
		}
	}

	
	void rotateSeq(int* rotateOffset, int seqn, int delta) {
		int oldRotateOffset = *rotateOffset;
		*rotateOffset += delta;
		if (*rotateOffset > 99) *rotateOffset = 99;
		if (*rotateOffset < -99) *rotateOffset = -99;	
		
		delta = *rotateOffset - oldRotateOffset;
		if (delta == 0) 
			return;// if end of range, no transpose to do
		
		if (delta > 0 && delta < 201) {// Rotate right, 201 is safety
			for (int i = delta; i > 0; i--) {
				rotateSeqByOne(seqn, true);
			}
		}
		if (delta < 0 && delta > -201) {// Rotate left, 201 is safety
			for (int i = delta; i < 0; i++) {
				rotateSeqByOne(seqn, false);
			}
		}
	}	

	
	private:
	
	
	void rotateSeqByOne(int seqn, bool directionRight) {
		float rotCV;
		StepAttributes rotAttributes;
		int iStart = 0;
		int iEnd = sequences[seqn].getLength() - 1;
		int iRot = iStart;
		int iDelta = 1;
		if (directionRight) {
			iRot = iEnd;
			iDelta = -1;
		}
		rotCV = cv[seqn][iRot];
		rotAttributes = attributes[seqn][iRot];
		for ( ; ; iRot += iDelta) {
			if (iDelta == 1 && iRot >= iEnd) break;
			if (iDelta == -1 && iRot <= iStart) break;
			cv[seqn][iRot] = cv[seqn][iRot + iDelta];
			attributes[seqn][iRot] = attributes[seqn][iRot + iDelta];
		}
		cv[seqn][iRot] = rotCV;
		attributes[seqn][iRot] = rotAttributes;
	}

	
	void applyTiedStep(int seqn, int indexTied) {
		int seqLength = sequences[seqn].getLength();
		// Start on indexTied and loop until seqLength
		// Called because either:
		//   case A: tied was activated for given step
		//   case B: the given step's CV was modified
		// These cases are mutually exclusive
		
		// copy previous CV and attribute over to current step if tied
		if (attributes[seqn][indexTied].getTied() && (indexTied > 0)) {
			cv[seqn][indexTied] = cv[seqn][indexTied - 1];
			attributes[seqn][indexTied] = attributes[seqn][indexTied - 1];
			attributes[seqn][indexTied].setTied(true);
			attributes[seqn][indexTied].setGate(false);
			attributes[seqn][indexTied].setGateP(false);
			attributes[seqn][indexTied].setSlide(false);
		}
		
		// Affect downstream CVs and attributes of subsequent tied note chain (can be 0 length if next note is not tied)
		for (int i = indexTied + 1; i < seqLength; i++) {
			if (attributes[seqn][i].getTied()) {
				cv[seqn][i] = cv[seqn][indexTied];
			}
			else 
				break;
		}
	}	
	

	void calcGateCodeEx(int seqn) {// uses stepIndexRun as the step
		StepAttributes attribute = attributes[seqn][stepIndexRun];
		int ppsFiltered = getPulsesPerStep();// must use method
		int gateType;

		if (gateCode != -1 || ppqnCount == 0) {// always calc on first ppqnCount, avoid thereafter if gate will be off for whole step
			gateType = attribute.getGateType();
			
			// -1 = gate off for whole step, 0 = gate off for current ppqn, 1 = gate on, 2 = clock high, 3 = trigger
			if ( ppqnCount == 0 && attribute.getGateP() && !(randomUniform() < ((float)attribute.getGatePVal() / 100.0f)) ) {// randomUniform is [0.0, 1.0), see include/util/common.hpp
				gateCode = -1;// must do this first in this method since it will kill all remaining pulses of the step if prob turns off the step
			}
			else if (!attribute.getGate()) {
				gateCode = 0;
			}
			else if (ppsFiltered == 1 && gateType == 0) {
				gateCode = 2;// clock high pulse
			}
			else {
				if (gateType == 11) {
					gateCode = (ppqnCount == 0 ? 3 : 0);// trig on first ppqnCount
				}
				else {
					uint64_t shiftAmt = ppqnCount * (96 / ppsFiltered);
					if (shiftAmt >= 64)
						gateCode = (int)((advGateHitMaskHigh[gateType] >> (shiftAmt - (uint64_t)64)) & (uint64_t)0x1);
					else
						gateCode = (int)((advGateHitMaskLow[gateType] >> shiftAmt) & (uint64_t)0x1);
				}
			}
		}
	}
	

	bool moveIndexRunMode(bool moveSequence) {	
		// assert((reps * numSteps) <= 0xFFF); // for BRN and RND run modes, history is not a span count but a step count
		
		int* index;
		unsigned long* history;
		int reps;
		int runMode;
		int endStep;
		int startStep;
		
		if (moveSequence) {
			index = &stepIndexRun;
			history = &stepIndexRunHistory;
			reps = phrases[phraseIndexRun].getReps();
			runMode = sequences[phrases[phraseIndexRun].getSeqNum()].getRunMode();
			endStep = sequences[phrases[phraseIndexRun].getSeqNum()].getLength() - 1;
			startStep = 0;
		}
		else {// move song
			index = &phraseIndexRun;
			history = &phraseIndexRunHistory;
			reps = 1;// 1 count is enough in song, since the return boundaryCross boolean is ignored (it will loop the song continually)
			runMode = runModeSong;
			endStep = songEndIndex;
			startStep = songBeginIndex;
		}
		
		bool crossBoundary = false;
		
		switch (runMode) {
		
			// history 0x0000 is reserved for reset
			
			case MODE_REV :// reverse; history base is 0x2000
				if ((*history) < 0x2001 || (*history) > 0x2FFF)
					(*history) = 0x2000 + reps;
				(*index)--;
				if ((*index) < startStep) {
					(*index) = endStep;
					(*history)--;
					if ((*history) <= 0x2000)
						crossBoundary = true;
				}
			break;
			
			case MODE_PPG :// forward-reverse; history base is 0x3000
				if ((*history) < 0x3001 || (*history) > 0x3FFF) // even means going forward, odd means going reverse
					(*history) = 0x3000 + reps * 2;
				if (((*history) & 0x1) == 0) {// even so forward phase
					(*index)++;
					if ((*index) > endStep) {
						(*index) = endStep;
						(*history)--;
					}
				}
				else {// odd so reverse phase
					(*index)--;
					if ((*index) < startStep) {
						(*index) = startStep;
						(*history)--;
						if ((*history) <= 0x3000)
							crossBoundary = true;
					}
				}
			break;

			case MODE_PEN :// forward-reverse; history base is 0x4000
				if ((*history) < 0x4001 || (*history) > 0x4FFF) // even means going forward, odd means going reverse
					(*history) = 0x4000 + reps * 2;
				if (((*history) & 0x1) == 0) {// even so forward phase
					(*index)++;
					if ((*index) > endStep) {
						(*index) = endStep - 1;
						(*history)--;
						if ((*index) <= startStep) {// if back at start after turnaround, then no reverse phase needed
							(*index) = startStep;
							(*history)--;
							if ((*history) <= 0x4000)
								crossBoundary = true;
						}
					}
				}
				else {// odd so reverse phase
					(*index)--;
					if ((*index) <= startStep) {
						(*index) = startStep;
						(*history)--;
						if ((*history) <= 0x4000)
							crossBoundary = true;
					}
				}
			break;
			
			case MODE_BRN :// brownian random; history base is 0x5000
				if ((*history) < 0x5001 || (*history) > 0x5FFF) 
					(*history) = 0x5000 + (endStep - startStep + 1) * reps;
				(*index) += (randomu32() % 3) - 1;
				if ((*index) > endStep) {
					(*index) = startStep;
				}
				if ((*index) < startStep) {
					(*index) = endStep;
				}
				(*history)--;
				if ((*history) <= 0x5000) {
					crossBoundary = true;
				}
			break;
			
			case MODE_RND :// random; history base is 0x6000
				if ((*history) < 0x6001 || (*history) > 0x6FFF) 
					(*history) = 0x6000 + (endStep - startStep + 1) * reps;
				(*index) = startStep + (randomu32() % (endStep - startStep + 1)) ;
				(*history)--;
				if ((*history) <= 0x6000) {
					crossBoundary = true;
				}
			break;
			
			default :// MODE_FWD  forward; history base is 0x1000
				if ((*history) < 0x1001 || (*history) > 0x1FFF)
					(*history) = 0x1000 + reps;
				(*index)++;
				if ((*index) > endStep) {
					(*index) = startStep;
					(*history)--;
					if ((*history) <= 0x1000)
						crossBoundary = true;
				}
		}

		return crossBoundary;
	}
		
};// struct SequencerKernel 




