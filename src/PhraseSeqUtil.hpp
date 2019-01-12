//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "dsp/digital.hpp"

using namespace rack;


// General constants

enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_PEN, MODE_BRN, MODE_RND, MODE_FW2, MODE_FW3, MODE_FW4, MODE_RN2, NUM_MODES};
static const std::string modeLabels[NUM_MODES] = {"FWD","REV","PPG","PEN","BRN","RND","FW2","FW3","FW4","RN2"};// PS16 and SMS16 use NUM_MODES - 1 since no RN2!!!

static const int NUM_GATES = 12;												
static const uint32_t advGateHitMask[NUM_GATES] = 
{0x00003F, 0x0F0F0F, 0x000FFF, 0x0F0F00, 0x03FFFF, 0xFFFFFF, 0x00000F, 0x03F03F, 0x000F00, 0x03F000, 0x0F0000, 0};
//	  25%		TRI		  50%		T23		  75%		FUL		  TR1 		DUO		  TR2 	     D2		  TR3  TRIG		


class StepAttributes {
	unsigned short attributes;
	
	public:

	static const unsigned short ATT_MSK_GATE1 = 0x01;
	static const unsigned short ATT_MSK_GATE1P = 0x02;
	static const unsigned short ATT_MSK_GATE2 = 0x04;
	static const unsigned short ATT_MSK_SLIDE = 0x08;
	static const unsigned short ATT_MSK_TIED = 0x10;
	static const unsigned short ATT_MSK_GATE1MODE = 0x01E0, gate1ModeShift = 5;
	static const unsigned short ATT_MSK_GATE2MODE = 0x1E00, gate2ModeShift = 9;
	
	static const unsigned short ATT_MSK_INITSTATE =  ATT_MSK_GATE1;
	
	inline void clear() {attributes = 0u;}
	inline void init() {attributes = ATT_MSK_INITSTATE;}
	inline void randomize() {attributes = (randomu32() & (ATT_MSK_GATE1 | ATT_MSK_GATE1P | ATT_MSK_GATE2 | ATT_MSK_SLIDE | ATT_MSK_TIED | ATT_MSK_GATE1MODE | ATT_MSK_GATE2MODE));}
	
	inline bool getGate1() {return (attributes & ATT_MSK_GATE1) != 0;}
	inline bool getGate1P() {return (attributes & ATT_MSK_GATE1P) != 0;}
	inline bool getGate2() {return (attributes & ATT_MSK_GATE2) != 0;}
	inline bool getSlide() {return (attributes & ATT_MSK_SLIDE) != 0;}
	inline bool getTied() {return (attributes & ATT_MSK_TIED) != 0;}
	inline int getGate1Mode() {return (attributes & ATT_MSK_GATE1MODE) >> gate1ModeShift;}
	inline int getGate2Mode() {return (attributes & ATT_MSK_GATE2MODE) >> gate2ModeShift;}
	inline unsigned short getAttribute() {return attributes;}

	inline void setGate1(bool gate1State) {attributes &= ~ATT_MSK_GATE1; if (gate1State) attributes |= ATT_MSK_GATE1;}
	inline void setGate1P(bool gate1PState) {attributes &= ~ATT_MSK_GATE1P; if (gate1PState) attributes |= ATT_MSK_GATE1P;}
	inline void setGate2(bool gate2State) {attributes &= ~ATT_MSK_GATE2; if (gate2State) attributes |= ATT_MSK_GATE2;}
	inline void setSlide(bool slideState) {attributes &= ~ATT_MSK_SLIDE; if (slideState) attributes |= ATT_MSK_SLIDE;}
	inline void setTied(bool tiedState) {
		attributes &= ~ATT_MSK_TIED; 
		if (tiedState) {
			attributes |= ATT_MSK_TIED;
			attributes &= ~(ATT_MSK_GATE1 | ATT_MSK_GATE1P | ATT_MSK_GATE2 | ATT_MSK_SLIDE);// clear other attributes if tied
		}
	}
	inline void setGate1Mode(int gateMode) {attributes &= ~ATT_MSK_GATE1MODE; attributes |= (gateMode << gate1ModeShift);}
	inline void setGate2Mode(int gateMode) {attributes &= ~ATT_MSK_GATE2MODE; attributes |= (gateMode << gate2ModeShift);}
	inline void setAttribute(unsigned short _attributes) {attributes = _attributes;}

	inline void toggleGate1() {attributes ^= ATT_MSK_GATE1;}
	inline void toggleGate1P() {attributes ^= ATT_MSK_GATE1P;}
	inline void toggleGate2() {attributes ^= ATT_MSK_GATE2;}
	inline void toggleSlide() {attributes ^= ATT_MSK_SLIDE;}
};// class StepAttributes
			

inline int ppsToIndex(int pulsesPerStep) {// map 1,2,4,6,8,10,12...24, to 0,1,2,3,4,5,6...12
	if (pulsesPerStep == 1) return 0;
	return pulsesPerStep >> 1;
}
inline int indexToPps(int index) {// inverse map of ppsToIndex()
	index = clamp(index, 0, 12);
	if (index == 0) return 1;
	return index <<	1;
}

inline float applyNewOct(float cvVal, int newOct) {
	float newCV = cvVal + 10.0f;//to properly handle negative note voltages
	return newCV - floor(newCV) + (float) (newOct - 3);
}

inline bool calcGate(int gateCode, SchmittTrigger clockTrigger, unsigned long clockStep, float sampleRate) {
	if (gateCode < 2) 
		return gateCode == 1;
	if (gateCode == 2)
		return clockTrigger.isHigh();
	return clockStep < (unsigned long) (sampleRate * 0.01f);
}

inline int getAdvGate(int ppqnCount, int pulsesPerStep, int gateMode) { 
	if (gateMode == 11)
		return ppqnCount == 0 ? 3 : 0;
	uint32_t shiftAmt = ppqnCount * (24 / pulsesPerStep);
	return (int)((advGateHitMask[gateMode] >> shiftAmt) & (uint32_t)0x1);
}

inline int calcGate1Code(StepAttributes attribute, int ppqnCount, int pulsesPerStep, float randKnob) {
	// -1 = gate off for whole step, 0 = gate off for current ppqn, 1 = gate on, 2 = clock high, 3 = trigger
	if (ppqnCount == 0 && attribute.getGate1P() && !(randomUniform() < randKnob))// randomUniform is [0.0, 1.0), see include/util/common.hpp
		return -1;// must do this first in this method since it will kill rest of step if prob turns off the step
	if (!attribute.getGate1())
		return 0;
	int gateType = attribute.getGate1Mode();
	if (pulsesPerStep == 1 && gateType == 0)
		return 2;// clock high
	if (gateType == 11)
		return (ppqnCount == 0 ? 3 : 0);
	return getAdvGate(ppqnCount, pulsesPerStep, gateType);
}
inline int calcGate2Code(StepAttributes attribute, int ppqnCount, int pulsesPerStep) {
	// 0 = gate off, 1 = clock high, 2 = trigger, 3 = gate on
	if (!attribute.getGate2())
		return 0;
	int gateType = attribute.getGate2Mode();
	if (pulsesPerStep == 1 && gateType == 0)
		return 2;// clock high
	if (gateType == 11)
		return (ppqnCount == 0 ? 3 : 0);
	return getAdvGate(ppqnCount, pulsesPerStep, gateType);
}

inline int gateModeToKeyLightIndex(StepAttributes attribute, bool isGate1) {// keyLight index now matches gate modes, so no mapping table needed anymore
	return isGate1 ? attribute.getGate1Mode() : attribute.getGate2Mode();
}



// Other methods (code in PhraseSeqUtil.cpp)	
												
bool moveIndexRunMode(int* index, int numSteps, int runMode, unsigned long* history);
int keyIndexToGateMode(int keyIndex, int pulsesPerStep);


