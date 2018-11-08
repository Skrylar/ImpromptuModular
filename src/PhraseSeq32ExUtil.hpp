//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"

using namespace rack;



// General constants

// Run modes
enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_PEN, MODE_BRN, MODE_RND, NUM_MODES};
static const std::string modeLabels[NUM_MODES]={"FWD","REV","PPG","PEN","BRN","RND"};

// Gate types
static const int NUM_GATES = 12;	

// Attributes of a step
typedef unsigned long Attribute;
static const Attribute ATT_MSK_GATE = 0x01;
static const Attribute ATT_MSK_GATEP = 0x02;
static const Attribute ATT_MSK_SLIDE = 0x04;
static const Attribute ATT_MSK_TIED = 0x08;
static const Attribute ATT_MSK_GATETYPE = 0xF0;
static const Attribute gate1TypeShift = 4;
static const Attribute ATT_MSK_GATEP_VAL = 0xFF00;
static const Attribute GatePValShift = 8;
static const Attribute ATT_MSK_SLIDE_VAL = 0xFF0000;
static const Attribute slideValShift = 16;
static const Attribute ATT_MSK_INITSTATE = (ATT_MSK_GATE | (0 << gate1TypeShift) | (50 << GatePValShift) | (10 << slideValShift));

							
				
// Inline functions

inline bool getGateA(Attribute attribute) {return (attribute & ATT_MSK_GATE) != 0;}
inline bool getTiedA(Attribute attribute) {return (attribute & ATT_MSK_TIED) != 0;}
inline bool getGatePa(Attribute attribute) {return (attribute & ATT_MSK_GATEP) != 0;}
inline int getGatePValA(Attribute attribute) {return (int)((attribute & ATT_MSK_GATEP_VAL) >> GatePValShift);}
inline bool getSlideA(Attribute attribute) {return (attribute & ATT_MSK_SLIDE) != 0;}
inline int getSlideValA(Attribute attribute) {return (int)((attribute & ATT_MSK_SLIDE_VAL) >> slideValShift);}
inline int getGateAType(Attribute attribute) {return ((int)(attribute & ATT_MSK_GATETYPE) >> gate1TypeShift);}

inline void setGateA(Attribute *attribute, bool gate1State) {(*attribute) &= ~ATT_MSK_GATE; if (gate1State) (*attribute) |= ATT_MSK_GATE;}
inline void setTiedA(Attribute *attribute, bool tiedState) {(*attribute) &= ~ATT_MSK_TIED; if (tiedState) (*attribute) |= ATT_MSK_TIED;}
inline void setGatePa(Attribute *attribute, bool GatePState) {(*attribute) &= ~ATT_MSK_GATEP; if (GatePState) (*attribute) |= ATT_MSK_GATEP;}
inline void setGatePValA(Attribute *attribute, int gatePval) {(*attribute) &= ~ATT_MSK_GATEP_VAL; (*attribute) |= (((Attribute)gatePval) << GatePValShift);}
inline void setSlideA(Attribute *attribute, bool slideState) {(*attribute) &= ~ATT_MSK_SLIDE; if (slideState) (*attribute) |= ATT_MSK_SLIDE;}
inline void setSlideValA(Attribute *attribute, int slideVal) {(*attribute) &= ~ATT_MSK_SLIDE_VAL; (*attribute) |= (((Attribute)slideVal) << slideValShift);}
inline void setGateTypeA(Attribute *attribute, int gate1Type) {(*attribute) &= ~ATT_MSK_GATETYPE; (*attribute) |= (((Attribute)gate1Type) << gate1TypeShift);}

inline void toggleGateA(Attribute *attribute) {(*attribute) ^= ATT_MSK_GATE;}
inline void toggleTiedA(Attribute *attribute) {(*attribute) ^= ATT_MSK_TIED;}
inline void toggleGatePa(Attribute *attribute) {(*attribute) ^= ATT_MSK_GATEP;}
inline void toggleSlideA(Attribute *attribute) {(*attribute) ^= ATT_MSK_SLIDE;}

// Other functions

int moveIndexEx(int index, int indexNext, int numSteps);
	
	

struct SequencerKernel {
	// General constants
	// ----------------

	// Clock resolution										
	static const int NUM_PPS_VALUES = 33;
	static const int ppsValues[NUM_PPS_VALUES];

	
	// Member data
	// ----------------
	
	int id;
	std::string ids;
	
	// need to save
	int runModeSong;	
	int runModeSeq[32];
	int pulsesPerStepIndex;// 0 to NUM_PPS_VALUES-1; 0 means normal gate mode of 1 pps; this is an index into ppsValues[]
	int phrases;// number of phrases (song steps), min value is 1
	int phraseReps[32];// a rep is 1 to 99
	int phrase[32];// This is the song (series of phases; a phrase is a sequence number)	
	int lengths[32];// number of steps in each sequence, min value is 1
	int transposeOffsets[32];
	
	// no need to save
	int stepIndexRun;
	unsigned long stepIndexRunHistory;
	int phraseIndexRun;
	unsigned long phraseIndexRunHistory;
	unsigned long slideStepsRemain;// 0 when no slide under way, downward step counter when sliding
	float slideCVdelta;// no need to initialize, this is only used when slideStepsRemain is not 0
	int gateCode;
	int ppqnCount;
	
	
	// Get, set, mod
	// ----------------

	
	// get
	inline int getRunModeSong() {return runModeSong;}
	inline int getRunModeSeq(int seqn) {return runModeSeq[seqn];}
	inline int getPhrases() {return phrases;}
	inline int getLength(int seqn) {return lengths[seqn];}
	inline int getPhrase(int phrn) {return phrase[phrn];}
	inline int getPhraseReps(int phrn) {return phraseReps[phrn];}
	inline int getPulsesPerStep() {return ppsValues[pulsesPerStepIndex];}
	inline int getTransposeOffset(int seqn) {return transposeOffsets[seqn];}
	inline int getStepIndexRun() {return stepIndexRun;}
	inline int getPhraseIndexRun() {return phraseIndexRun;}
	
	// set
	inline void setRunModeSeq(int seqn, int runmode) {runModeSeq[seqn] = runmode;}
	inline void setPhrases(int _phrases) {phrases = _phrases;}
	inline void setLength(int seqn, int _length) {lengths[seqn] = _length;}
	inline void setPhrase(int phrn, int seqn) {phrase[phrn] = seqn;}
	inline void resetTransposeOffset(int seqn) {transposeOffsets[seqn] = 0;}
	inline void setSlideStepsRemainAndCVdelta(unsigned long _slideStepsRemain, float cvdelta) {slideStepsRemain = _slideStepsRemain; slideCVdelta = cvdelta/(float)slideStepsRemain;}
	inline void setStepIndexRun(int _stepIndexRun) {stepIndexRun = _stepIndexRun;}
	inline void setPhraseIndexRun(int _phraseIndexRun) {phraseIndexRun = _phraseIndexRun;}
	
	// mod
	inline void modRunModeSong(int delta) {runModeSong += delta; if (runModeSong < 0) runModeSong = 0; if (runModeSong >= NUM_MODES) runModeSong = NUM_MODES - 1;}
	inline void modRunModeSeq(int seqn, int delta) {
		runModeSeq[seqn] += delta;
		if (runModeSeq[seqn] < 0) runModeSeq[seqn] = 0;
		if (runModeSeq[seqn] >= NUM_MODES) runModeSeq[seqn] = NUM_MODES - 1;
	}
	inline void modPhrases(int delta) {phrases += delta; if (phrases > 32) phrases = 32; if (phrases < 1 ) phrases = 1;}
	inline void modLength(int seqn, int delta) {lengths[seqn] += delta; if (lengths[seqn] > 32) lengths[seqn] = 32; if (lengths[seqn] < 1 ) lengths[seqn] = 1;}
	inline void modPhrase(int phrn, int delta) {
		int newPhrase = phrase[phrn] + delta;
		if (newPhrase < 0)
			newPhrase += (1 - newPhrase / 32) * 32;// newPhrase now positive
		newPhrase = newPhrase % 32;
		phrase[phrn] = newPhrase;
	}
	inline void modPhraseReps(int phrn, int delta) {
		int newPhraseReps = phraseReps[phrn] + delta - 1;
		if (newPhraseReps < 0)
			newPhraseReps += (1 - newPhraseReps / 99) * 99;// newPhraseReps now positive
		newPhraseReps = newPhraseReps % 99;
		phraseReps[phrn] = newPhraseReps + 1;
	}		
	inline void modPulsesPerStepIndex(int delta) {
		pulsesPerStepIndex += delta;
		if (pulsesPerStepIndex < 0) pulsesPerStepIndex = 0;
		if (pulsesPerStepIndex >= NUM_PPS_VALUES) pulsesPerStepIndex = NUM_PPS_VALUES - 1;
	}
	inline void modTransposeOffset(int seqn, int delta) {
		transposeOffsets[seqn] += delta;
		if (transposeOffsets[seqn] > 99) transposeOffsets[seqn] = 99;
		if (transposeOffsets[seqn] < -99) transposeOffsets[seqn] = -99;						
	}
	inline void decSlideStepsRemain() {if (slideStepsRemain > 0ul) slideStepsRemain--;}
	
	
	// Main methods
	// ----------------
	
	void setId(int _id) {
		id = _id;
		ids = "id" + std::to_string(id) + "_";
	}
	
	
	void onReset() {
		pulsesPerStepIndex = 0;
		runModeSong = MODE_FWD;
		phrases = 4;
		for (int i = 0; i < 32; i++) {
			runModeSeq[i] = MODE_FWD;
			phrase[i] = 0;
			phraseReps[i] = 1;
			lengths[i] = 32;			
			resetTransposeOffset(i);
		}
		slideStepsRemain = 0ul;
	}
	
	
	void onRandomize() {
		runModeSong = randomu32() % 5;
		phrases = 1 + (randomu32() % 32);
		for (int i = 0; i < 32; i++) {
			runModeSeq[i] = randomu32() % NUM_MODES;
			phrase[i] = randomu32() % 32;
			phraseReps[i] = randomu32() % 4 + 1;
			lengths[i] = 1 + (randomu32() % 32);
			resetTransposeOffset(i);
		}
	}
	
	
	void initRun(bool hard, bool isEditingSequence, int sequence) {
		// NOTE when importing, keep same order as in module's initRun()
	
		if (hard) {
			phraseIndexRun = (runModeSong == MODE_REV ? phrases - 1 : 0);
			phraseIndexRunHistory = 0;
		}
		int seqn = (isEditingSequence ? sequence : phrase[phraseIndexRun]);
		if (hard) {
			stepIndexRun = (runModeSeq[seqn] == MODE_REV ? lengths[seqn] - 1 : 0);
			stepIndexRunHistory = 0;
		}
		ppqnCount = 0;
		//seq[0].calcGateCodeEx(attributes[seqn][stepIndexRun]);
		slideStepsRemain = 0ul;
	}
	
	
	void toJson(json_t *rootJ) {
		// pulsesPerStepIndex
		json_object_set_new(rootJ, (ids + "pulsesPerStepIndex").c_str(), json_integer(pulsesPerStepIndex));

		// runModeSong
		json_object_set_new(rootJ, (ids + "runModeSong").c_str(), json_integer(runModeSong));

		// runModeSeq
		json_t *runModeSeqJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(runModeSeqJ, i, json_integer(runModeSeq[i]));
		json_object_set_new(rootJ, (ids + "runModeSeq").c_str(), runModeSeqJ);

		// phrases
		json_object_set_new(rootJ, (ids + "phrases").c_str(), json_integer(phrases));
		
		// phraseReps 
		json_t *phraseRepsJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(phraseRepsJ, i, json_integer(phraseReps[i]));
		json_object_set_new(rootJ, (ids + "phraseReps").c_str(), phraseRepsJ);

		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, (ids + "phrase").c_str(), phraseJ);

		// lengths
		json_t *lengthsJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(lengthsJ, i, json_integer(lengths[i]));
		json_object_set_new(rootJ, (ids + "lengths").c_str(), lengthsJ);
		
		// transposeOffsets
		json_t *transposeOffsetsJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(transposeOffsetsJ, i, json_integer(transposeOffsets[i]));
		json_object_set_new(rootJ, (ids + "transposeOffsets").c_str(), transposeOffsetsJ);

	}
	
	
	void fromJson(json_t *rootJ) {
		// pulsesPerStepIndex
		json_t *pulsesPerStepIndexJ = json_object_get(rootJ, (ids + "pulsesPerStepIndex").c_str());
		if (pulsesPerStepIndexJ)
			pulsesPerStepIndex = json_integer_value(pulsesPerStepIndexJ);

		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, (ids + "runModeSong").c_str());
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
				
		// runModeSeq
		json_t *runModeSeqJ = json_object_get(rootJ, (ids + "runModeSeq").c_str());
		if (runModeSeqJ) {
			for (int i = 0; i < 32; i++)
			{
				json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
				if (runModeSeqArrayJ)
					runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
			}			
		}		
		
		// phrases	
		json_t *phrasesJ = json_object_get(rootJ, (ids + "phrases").c_str());
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
		
		// phraseReps
		json_t *phraseRepsJ = json_object_get(rootJ, (ids + "phraseReps").c_str());
		if (phraseRepsJ)
			for (int i = 0; i < 32; i++)
			{
				json_t *phraseRepsArrayJ = json_array_get(phraseRepsJ, i);
				if (phraseRepsArrayJ)
					phraseReps[i] = json_integer_value(phraseRepsArrayJ);
			}

		// phrase
		json_t *phraseJ = json_object_get(rootJ, (ids + "phrase").c_str());
		if (phraseJ)
			for (int i = 0; i < 32; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
		
		// lengths
		json_t *lengthsJ = json_object_get(rootJ, (ids + "lengths").c_str());
		if (lengthsJ)
			for (int i = 0; i < 32; i++)
			{
				json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
				if (lengthsArrayJ)
					lengths[i] = json_integer_value(lengthsArrayJ);
			}

		// transposeOffsets
		json_t *transposeOffsetsJ = json_object_get(rootJ, (ids + "transposeOffsets").c_str());
		if (transposeOffsetsJ) {
			for (int i = 0; i < 32; i++)
			{
				json_t *transposeOffsetsArrayJ = json_array_get(transposeOffsetsJ, i);
				if (transposeOffsetsArrayJ)
					transposeOffsets[i] = json_integer_value(transposeOffsetsArrayJ);
			}			
		}		
	}
	
	
	inline float calcSlideOffset() {return (slideStepsRemain > 0ul ? (slideCVdelta * (float)slideStepsRemain) : 0.0f);}
	

	inline bool incPpqnCountAndCmpWithZero() {
		ppqnCount++;
		if (ppqnCount >= ppsValues[pulsesPerStepIndex])
			ppqnCount = 0;
		return ppqnCount == 0;
	}

	
	inline bool calcGate(SchmittTrigger clockTrigger, unsigned long clockStep, float sampleRate) {
		if (gateCode < 2) 
			return gateCode == 1;
		if (gateCode == 2)
			return clockTrigger.isHigh();
		return clockStep < (unsigned long) (sampleRate * 0.001f);
	}
	
	int keyIndexToGateTypeEx(int keyIndex) {
		int pulsesPerStep = ppsValues[pulsesPerStepIndex];
		if (((pulsesPerStep % 6) != 0) && (keyIndex == 1 || keyIndex == 3 || keyIndex == 6 || keyIndex == 8 || keyIndex == 10))// TRIs
			return -1;
		if (((pulsesPerStep % 4) != 0) && (keyIndex == 0 || keyIndex == 4 || keyIndex == 7 || keyIndex == 9))// DOUBLEs
			return -1;
		return keyIndex;// keyLight index now matches gate types, so no mapping table needed anymore
	}
	

	void calcGateCodeEx(Attribute attribute) {
		int gateType;
		static const uint64_t advGateHitMaskLow[NUM_GATES] = 
		{0x0000000000FFFFFF, 0x0000FFFF0000FFFF, 0x0000FFFFFFFFFFFF, 0x0000FFFF00000000, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 
		//				25%					TRI		  			50%					T23		  			75%					FUL		
		 0x000000000000FFFF, 0xFFFF000000FFFFFF, 0x0000FFFF00000000, 0xFFFF000000000000, 0x0000000000000000, 0};
		//  			TR1 				DUO		  			TR2 	     		D2		  			TR3  TRIG		
		static const uint64_t advGateHitMaskHigh[NUM_GATES] = 
		{0x0000000000000000, 0x000000000000FFFF, 0x0000000000000000, 0x000000000000FFFF, 0x00000000000000FF, 0x00000000FFFFFFFF, 
		//				25%					TRI		  			50%					T23		  			75%					FUL		
		 0x0000000000000000, 0x00000000000000FF, 0x0000000000000000, 0x00000000000000FF, 0x000000000000FFFF, 0};
		//  			TR1 				DUO		  			TR2 	     		D2		  			TR3  TRIG		

		if (gateCode != -1 || ppqnCount == 0) {// always calc on first ppqnCount, avoid thereafter if gate will be off for whole step
			// -1 = gate off for whole step, 0 = gate off for current ppqn, 1 = gate on, 2 = clock high, 3 = trigger
			if ( ppqnCount == 0 && getGatePa(attribute) && !(randomUniform() < ((float)getGatePValA(attribute) / 100.0f)) ) {// randomUniform is [0.0, 1.0), see include/util/common.hpp
				gateCode = -1;// must do this first in this method since it will kill rest of step if prob turns off the step
			}
			else if (!getGateA(attribute)) {
				gateCode = 0;
			}
			else if (pulsesPerStepIndex == 0) {
				gateCode = 2;// clock high pulse
			}
			else {
				gateType = getGateAType(attribute);
				if (gateType == 11) {
					gateCode = (ppqnCount == 0 ? 3 : 0);
				}
				else {
					uint64_t shiftAmt = ppqnCount * (96 / ppsValues[pulsesPerStepIndex]);
					if (shiftAmt >= 64)
						gateCode = (int)((advGateHitMaskHigh[gateType] >> (shiftAmt - (uint64_t)64)) & (uint64_t)0x1);
					else
						gateCode = (int)((advGateHitMaskLow[gateType] >> shiftAmt) & (uint64_t)0x1);
				}
			}
		}
	}
	
	
	bool moveIndexRunMode(bool stepPhraseN, int numSteps, int runMode, int reps) {	
		// assert((reps * numSteps) <= 0xFFF); // for BRN and RND run modes, history is not a span count but a step count
		
		int* index;
		unsigned long* history;
		
		if (stepPhraseN) {// true when moving sequence, false when moving song
			index = &stepIndexRun;
			history = &stepIndexRunHistory;
		}
		else {
			index = &phraseIndexRun;
			history = &phraseIndexRunHistory;
		}
		
		bool crossBoundary = false;
		
		switch (runMode) {
		
			// history 0x0000 is reserved for reset
			
			case MODE_REV :// reverse; history base is 0x2000
				if ((*history) < 0x2001 || (*history) > 0x2FFF)
					(*history) = 0x2000 + reps;
				(*index)--;
				if ((*index) < 0) {
					(*index) = numSteps - 1;
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
					if ((*index) >= numSteps) {
						(*index) = numSteps - 1 ;
						(*history)--;
					}
				}
				else {// odd so reverse phase
					(*index)--;
					if ((*index) < 0) {
						(*index) = 0;
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
					if ((*index) >= numSteps) {
						(*index) = numSteps - 2;
						(*history)--;
						if ((*index) < 1) {// if back at 0 after turnaround, then no reverse phase needed
							(*index) = 0;
							(*history)--;
							if ((*history) <= 0x4000)
								crossBoundary = true;
						}
					}
				}
				else {// odd so reverse phase
					(*index)--;
					if ((*index) < 1) {
						(*index) = 0;
						(*history)--;
						if ((*history) <= 0x4000)
							crossBoundary = true;
					}
				}
			break;
			
			case MODE_BRN :// brownian random; history base is 0x5000
				if ((*history) < 0x5001 || (*history) > 0x5FFF) 
					(*history) = 0x5000 + numSteps * reps;
				(*index) += (randomu32() % 3) - 1;
				if ((*index) >= numSteps) {
					(*index) = 0;
				}
				if ((*index) < 0) {
					(*index) = numSteps - 1;
				}
				(*history)--;
				if ((*history) <= 0x5000) {
					crossBoundary = true;
				}
			break;
			
			case MODE_RND :// random; history base is 0x6000
				if ((*history) < 0x6001 || (*history) > 0x6FFF) 
					(*history) = 0x6000 + numSteps * reps;
				(*index) = (randomu32() % numSteps) ;
				(*history)--;
				if ((*history) <= 0x6000) {
					crossBoundary = true;
				}
			break;
			
			default :// MODE_FWD  forward; history base is 0x1000
				if ((*history) < 0x1001 || (*history) > 0x1FFF)
					(*history) = 0x1000 + reps;
				(*index)++;
				if ((*index) >= numSteps) {
					(*index) = 0;
					(*history)--;
					if ((*history) <= 0x1000)
						crossBoundary = true;
				}
		}

		return crossBoundary;
	}
		
};// struct SequencerKernel 




