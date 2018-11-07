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

							
				
// Inline methods

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


inline bool calcGate(int gateCode, SchmittTrigger clockTrigger, unsigned long clockStep, float sampleRate) {
	if (gateCode < 2) 
		return gateCode == 1;
	if (gateCode == 2)
		return clockTrigger.isHigh();
	return clockStep < (unsigned long) (sampleRate * 0.001f);
}


struct SequencerKernel {
	// General constants

	// Clock resolution										
	static const int NUM_PPS_VALUES = 33;
	static const int ppsValues[NUM_PPS_VALUES];

	
	// Member data
	
	int id;
	std::string ids;
	int runModeSong;	
	int runModeSeq[32];
	int pulsesPerStepIndex;// 0 to NUM_PPS_VALUES-1; 0 means normal gate mode of 1 pps; this is an index into ppsValues[]
	int phrases;// number of phrases (song steps), min value is 1
	int phraseReps[32];// a rep is 1 to 99
	int phrase[32];// This is the song (series of phases; a phrase is a sequence number)	
	int lengths[32];// number of steps in each sequence, min value is 1
	int transposeOffsets[32];
	
	// get
	inline int getRunModeSong() {return runModeSong;}
	inline int getRunModeSeq(int seqn) {return runModeSeq[seqn];}
	inline int getPhrases() {return phrases;}
	inline int getLength(int seqn) {return lengths[seqn];}
	inline int getPhrase(int phrn) {return phrase[phrn];}
	inline int getPhraseReps(int phrn) {return phraseReps[phrn];}
	inline int getPulsesPerStep() {return ppsValues[pulsesPerStepIndex];}
	inline int getTransposeOffset(int seqn) {return transposeOffsets[seqn];}
	
	// set
	inline void setRunModeSeq(int seqn, int runmode) {runModeSeq[seqn] = runmode;}
	inline void setPhrases(int _phrases) {phrases = _phrases;}
	inline void setLength(int seqn, int _length) {lengths[seqn] = _length;}
	inline void setPhrase(int phrn, int seqn) {phrase[phrn] = seqn;}
	inline void resetTransposeOffset(int seqn) {transposeOffsets[seqn] = 0;}
	
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
	
	// main
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
	
	
	int keyIndexToGateTypeEx(int keyIndex) {
		int pulsesPerStep = ppsValues[pulsesPerStepIndex];
		if (((pulsesPerStep % 6) != 0) && (keyIndex == 1 || keyIndex == 3 || keyIndex == 6 || keyIndex == 8 || keyIndex == 10))// TRIs
			return -1;
		if (((pulsesPerStep % 4) != 0) && (keyIndex == 0 || keyIndex == 4 || keyIndex == 7 || keyIndex == 9))// DOUBLEs
			return -1;
		return keyIndex;// keyLight index now matches gate types, so no mapping table needed anymore
	}
	

	int calcGateCodeEx(Attribute attribute, int ppqnCount) {
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

		// -1 = gate off for whole step, 0 = gate off for current ppqn, 1 = gate on, 2 = clock high, 3 = trigger
		if ( ppqnCount == 0 && getGatePa(attribute) && !(randomUniform() < ((float)getGatePValA(attribute) / 100.0f)) )// randomUniform is [0.0, 1.0), see include/util/common.hpp
			return -1;// must do this first in this method since it will kill rest of step if prob turns off the step
		if (!getGateA(attribute))
			return 0;
		if (pulsesPerStepIndex == 0)
			return 2;// clock high pulse
		int gateType = getGateAType(attribute);
		if (gateType == 11)
			return ppqnCount == 0 ? 3 : 0;
		uint64_t shiftAmt = ppqnCount * (96 / ppsValues[pulsesPerStepIndex]);
		if (shiftAmt >= 64)
			return (int)((advGateHitMaskHigh[gateType] >> (shiftAmt - (uint64_t)64)) & (uint64_t)0x1);
		return (int)((advGateHitMaskLow[gateType] >> shiftAmt) & (uint64_t)0x1);
	}
		
};// struct SequencerKernel 


// Other methods 

	







//#endif


