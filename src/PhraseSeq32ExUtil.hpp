//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"

using namespace rack;


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

	static const int INIT_VELOCITY = 100;
	static const int MAX_VELOCITY = 0x7F;// also used as a bit mask
	static const int INIT_PROB = 50;// range is 0 to 100
	static const int INIT_SLIDE = 10;// range is 0 to 100
	
	static const unsigned long ATT_MSK_INITSTATE = ((ATT_MSK_GATE) | (INIT_VELOCITY << velocityShift) | (INIT_PROB << gatePValShift) | (INIT_SLIDE << slideValShift));

	inline void clear() {attributes = 0ul;}
	inline void init() {attributes = ATT_MSK_INITSTATE;}
	inline void randomize() {attributes = ( ((randomu32() & (ATT_MSK_GATE | ATT_MSK_GATEP | ATT_MSK_SLIDE | ATT_MSK_TIED)) << gateShift) | ((randomu32() % 101) << gatePValShift) | ((randomu32() % 101) << slideValShift) | ((randomu32() & MAX_VELOCITY) << velocityShift) );}
	
	inline bool getGate() {return (attributes & ATT_MSK_GATE) != 0;}
	inline int getGateType() {return (int)((attributes & ATT_MSK_GATETYPE) >> gateTypeShift);}
	inline bool getTied() {return (attributes & ATT_MSK_TIED) != 0;}
	inline bool getGateP() {return (attributes & ATT_MSK_GATEP) != 0;}
	inline int getGatePVal() {return (int)((attributes & ATT_MSK_GATEP_VAL) >> gatePValShift);}
	inline bool getSlide() {return (attributes & ATT_MSK_SLIDE) != 0;}
	inline int getSlideVal() {return (int)((attributes & ATT_MSK_SLIDE_VAL) >> slideValShift);}
	inline int getVelocityVal() {return (int)((attributes & ATT_MSK_VELOCITY) >> velocityShift);}
	inline unsigned long getAttribute() {return attributes;}

	inline void setGate(bool gate1State) {attributes &= ~ATT_MSK_GATE; if (gate1State) attributes |= ATT_MSK_GATE;}
	inline void setGateType(int gateType) {attributes &= ~ATT_MSK_GATETYPE; attributes |= (((unsigned long)gateType) << gateTypeShift);}
	inline void setTied(bool tiedState) {
		attributes &= ~ATT_MSK_TIED; 
		if (tiedState) {
			attributes |= ATT_MSK_TIED;
			attributes &= ~(ATT_MSK_GATE | ATT_MSK_GATEP | ATT_MSK_SLIDE);// clear other attributes if tied
		}
	}
	inline void setGateP(bool GatePState) {attributes &= ~ATT_MSK_GATEP; if (GatePState) attributes |= ATT_MSK_GATEP;}
	inline void setGatePVal(int gatePval) {attributes &= ~ATT_MSK_GATEP_VAL; attributes |= (((unsigned long)gatePval) << gatePValShift);}
	inline void setSlide(bool slideState) {attributes &= ~ATT_MSK_SLIDE; if (slideState) attributes |= ATT_MSK_SLIDE;}
	inline void setSlideVal(int slideVal) {attributes &= ~ATT_MSK_SLIDE_VAL; attributes |= (((unsigned long)slideVal) << slideValShift);}
	inline void setVelocityVal(int _velocity) {attributes &= ~ATT_MSK_VELOCITY; attributes |= (((unsigned long)_velocity) << velocityShift);}
	inline void setAttribute(unsigned long _attributes) {attributes = _attributes;}

	inline void toggleGate() {attributes ^= ATT_MSK_GATE;}
	inline void toggleGateP() {attributes ^= ATT_MSK_GATEP;}
	inline void toggleSlide() {attributes ^= ATT_MSK_SLIDE;}	
};// class StepAttributes


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
};// class Phrase


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
};// class SeqAttributes


//*****************************************************************************


class SequencerKernel {
	public: 
	
	
	// General constants
	// ----------------

	// Sequencer dimensions
	static const int MAX_STEPS = 32;// must be a power of two (some multi select loops have bitwise "& (MAX_STEPS - 1)")
	static const int MAX_SEQS = 64;
	static const int MAX_PHRASES = 99;// maximum value is 99 (disp will be 1 to 99)

	// Run modes
	enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_PEN, MODE_BRN, MODE_RND, MODE_ARN, NUM_MODES};
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
	float cv[MAX_SEQS][MAX_STEPS];// [-3.0 : 3.917].
	StepAttributes attributes[MAX_SEQS][MAX_STEPS];
	
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
	uint32_t* slaveSeqRndLast;// nullprt for track 0
	uint32_t* slaveSongRndLast;// nullprt for track 0
	uint32_t seqRndLast;// slaved random seq on tracks 2-4
	uint32_t songRndLast;// slaved random song on tracks 2-4
	bool* holdTiedNotesPtr;
	
	
	public: 
	
	
	void construct(int _id, uint32_t* seqPtr, uint32_t* songPtr, bool* _holdTiedNotesPtr); // don't want regaular constructor mechanism
	
	
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
	inline bool getGate(int seqn, int stepn) {return attributes[seqn][stepn].getGate();}
	inline bool getGateP(int seqn, int stepn) {return attributes[seqn][stepn].getGateP();}
	inline bool getSlide(int seqn, int stepn) {return attributes[seqn][stepn].getSlide();}
	inline bool getTied(int seqn, int stepn) {return attributes[seqn][stepn].getTied();}
	inline int getGatePVal(int seqn, int stepn) {return attributes[seqn][stepn].getGatePVal();}
	inline int getSlideVal(int seqn, int stepn) {return attributes[seqn][stepn].getSlideVal();}
	inline int getVelocityVal(int seqn, int stepn) {return attributes[seqn][stepn].getVelocityVal();}
	inline int getVelocityValRun() {return getAttributeRun().getVelocityVal();}
	inline int getGateType(int seqn, int stepn) {return attributes[seqn][stepn].getGateType();}
	inline uint32_t* getSeqRndLast() {return &seqRndLast;}
	inline uint32_t* getSongRndLast() {return &songRndLast;}
	
	
	inline void setLength(int seqn, int _length) {sequences[seqn].setLength(_length);}
	inline void setBegin(int phrn) {songBeginIndex = phrn; songEndIndex = max(phrn, songEndIndex);}
	inline void setEnd(int phrn) {songEndIndex = phrn; songBeginIndex = min(phrn, songBeginIndex);}
	inline void setGate(int seqn, int stepn, bool gate) {attributes[seqn][stepn].setGate(gate);}
	inline void setGateP(int seqn, int stepn, bool gateP) {attributes[seqn][stepn].setGateP(gateP);}
	inline void setSlide(int seqn, int stepn, bool slide) {attributes[seqn][stepn].setSlide(slide);}
	inline void setTied(int seqn, int stepn, bool tied) {attributes[seqn][stepn].setTied(tied);}// gate, gateP and slide will get cleared if true
	void setGatePVal(int seqn, int stepn, int gatePval, int count) {
		int starti = (count == MAX_STEPS ? 0 : stepn);
		int endi = min(MAX_STEPS, stepn + count);
		for (int i = starti; i < endi; i++)
			attributes[seqn][i].setGatePVal(gatePval);
	}
	void setSlideVal(int seqn, int stepn, int slideVal, int count) {
		int starti = (count == MAX_STEPS ? 0 : stepn);
		int endi = min(MAX_STEPS, stepn + count);
		for (int i = starti; i < endi; i++)
			attributes[seqn][i].setSlideVal(slideVal);
	}
	void setVelocityVal(int seqn, int stepn, int velocity, int count) {
		int starti = (count == MAX_STEPS ? 0 : stepn);
		int endi = min(MAX_STEPS, stepn + count);
		for (int i = starti; i < endi; i++)
			attributes[seqn][i].setVelocityVal(velocity);
	}
	void setGateType(int seqn, int stepn, int gateType, int count) {
		int starti = (count == MAX_STEPS ? 0 : stepn);
		int endi = min(MAX_STEPS, stepn + count);
		for (int i = starti; i < endi; i++)
			attributes[seqn][i].setGateType(gateType);
	}
	
	
	inline void modRunModeSong(int delta) {
		runModeSong = clamp(runModeSong += delta, 0, NUM_MODES - 1);
	}
	inline void modRunModeSeq(int seqn, int delta) {
		int rVal = sequences[seqn].getRunMode();
		rVal = clamp(rVal + delta, 0, NUM_MODES - 1);
		sequences[seqn].setRunMode(rVal);
	}
	inline void modLength(int seqn, int delta) {
		int lVal = sequences[seqn].getLength();
		lVal = clamp(lVal + delta, 1, MAX_STEPS);
		sequences[seqn].setLength(lVal);
	}
	inline void modPhraseSeqNum(int phrn, int delta) {
		int seqn = phrases[phrn].getSeqNum();
		seqn = moveIndex(seqn, seqn + delta, MAX_SEQS);
		phrases[phrn].setSeqNum(seqn);
	}
	inline void modPhraseReps(int phrn, int delta) {
		int rVal = phrases[phrn].getReps();
		rVal = clamp(rVal + delta, 1, 99);
		phrases[phrn].setReps(rVal);
	}		
	inline void modPulsesPerStep(int delta) {
		pulsesPerStep += delta;
		if (pulsesPerStep < 1) pulsesPerStep = 1;
		if (pulsesPerStep > 49) pulsesPerStep = 49;
	}
	inline void modDelay(int delta) {
		delay = clamp(delay + delta, 0, 99);
	}
	inline void modGatePVal(int seqn, int stepn, int delta, int count) {
		int pVal = getGatePVal(seqn, stepn);
		pVal = clamp(pVal + delta, 0, 100);
		setGatePVal(seqn, stepn, pVal, count);						
	}		
	inline void modSlideVal(int seqn, int stepn, int delta, int count) {
		int sVal = getSlideVal(seqn, stepn);
		sVal = clamp(sVal + delta, 0, 100);
		setSlideVal(seqn, stepn, sVal, count);
	}		
	inline void modVelocityVal(int seqn, int stepn, int delta, int count) {
		int vVal = getVelocityVal(seqn, stepn);
		vVal = clamp(vVal + delta, 0, 127);
		setVelocityVal(seqn, stepn, vVal, count);						
	}		
	inline void decSlideStepsRemain() {if (slideStepsRemain > 0ul) slideStepsRemain--;}	
	void toggleGate(int seqn, int stepn, int count);
	void toggleGateP(int seqn, int stepn, int count);
	void toggleSlide(int seqn, int stepn, int count);	
	void toggleTied(int seqn, int stepn, int count);
	float applyNewOctave(int seqn, int stepn, int newOct, int count);
	float applyNewKey(int seqn, int stepn, int newKeyIndex, int count);
	float writeCV(int seqn, int stepn, float newCV, int count);
	
	
	inline float calcSlideOffset() {return (slideStepsRemain > 0ul ? (slideCVdelta * (float)slideStepsRemain) : 0.0f);}
	inline bool calcGate(SchmittTrigger clockTrigger, unsigned long clockStep, float sampleRate) {
		if (gateCode < 2) 
			return gateCode == 1;
		if (gateCode == 2)
			return clockTrigger.isHigh();
		return clockStep < (unsigned long) (sampleRate * 0.001f);
	}
	
	
	void initSequence(int seqn);
	void initSong();
	void randomizeSequence(int seqn);
	void randomizeSong();	

	void copySequence(float* cvCPbuffer, StepAttributes* attribCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int seqn, int startCP, int countCP);
	void pasteSequence(float* cvCPbuffer, StepAttributes* attribCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int seqn, int startCP, int countCP);
	void copyPhrase(Phrase* phraseCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int startCP, int countCP);
	void pastePhrase(Phrase* phraseCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int startCP, int countCP);
	
	
	// Main methods
	// ----------------
		
	
	void reset();
	void randomize();
	void initRun();
	void toJson(json_t *rootJ);
	void fromJson(json_t *rootJ);
	void clockStep(unsigned long clockPeriod);
	int keyIndexToGateTypeEx(int keyIndex);
	void transposeSeq(int seqn, int delta);
	void rotateSeq(int* rotateOffset, int seqn, int delta);	

	
	private:
	
	
	void rotateSeqByOne(int seqn, bool directionRight);
	inline void propagateCVtoTied(int seqn, int stepn) {
		for (int i = stepn + 1; i < MAX_STEPS && attributes[seqn][i].getTied(); i++)
			cv[seqn][i] = cv[seqn][i - 1];	
	}
	void activateTiedStep(int seqn, int stepn);
	void deactivateTiedStep(int seqn, int stepn);
	void calcGateCodeEx(int seqn);
	bool moveIndexRunMode(bool moveSequence);
	
};// class SequencerKernel 



//*****************************************************************************


class Sequencer {
	//static const int NUM_TRACKS;
	
	//SequencerKernel sek[NUM_TRACKS];
	
};// class Sequencer 

