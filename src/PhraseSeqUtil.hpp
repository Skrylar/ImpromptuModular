//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "dsp/digital.hpp"

using namespace rack;


// General constants

enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_BRN, MODE_RND, MODE_FW2, MODE_FW3, MODE_FW4, NUM_MODES};
static const std::string modeLabels[NUM_MODES]={"FWD","REV","PPG","BRN","RND","FW2","FW3","FW4"};
enum GateModeIds {GATE_24, GATE_34, GATE_44, GATE_14, GATE_TRIG, GATE_DUO, GATE_DU1, GATE_DU2, 
				  GATE_TRIPLET, GATE_TRIP1, GATE_TRIP2, GATE_TRIP3, GATE_TRIP4, GATE_TRIP5, GATE_TRIP6, NUM_GATES};
static const std::string gateLabels[NUM_GATES]={"2/4","3/4","4/4","1/4","TRG","DUO","DU1","DU2",
												"TRP","TR1","TR2","TR3","TR4","TR5","TR6"};
enum AttributeBitMasks {ATT_MSK_GATE1 = 0x01, ATT_MSK_GATE1P = 0x02, ATT_MSK_GATE2 = 0x04, ATT_MSK_SLIDE = 0x08, ATT_MSK_TIED = 0x10};// 5 bits
static const int ATT_MSK_GATE1MODE = 0x01E0;// 4 bits
static const int gate1ModeShift = 5;
static const int ATT_MSK_GATE2MODE = 0x1E00;// 4 bits
static const int gate2ModeShift = 9;
static const uint32_t advGateHitMask[NUM_GATES] = {0xFFF000, 0xFFFFC0, 0xFFFFFF, 0xFC0000, 0, 0xFC0FC0, 0xFC0000, 0x000FC0,
							0xF0F0F0, 0xF00000, 0x00F000, 0x0000F0, 0xF0F000, 0xF000F0, 0x00F0F0};

				
// Inline methods
inline bool getGate1a(int attribute) {return (attribute & ATT_MSK_GATE1) != 0;}
inline bool getGate2a(int attribute) {return (attribute & ATT_MSK_GATE2) != 0;}
inline bool getGate1Pa(int attribute) {return (attribute & ATT_MSK_GATE1P) != 0;}
inline int getGate1aMode(int attribute) {return (attribute & ATT_MSK_GATE1MODE) >> gate1ModeShift;}
inline int getGate2aMode(int attribute) {return (attribute & ATT_MSK_GATE2MODE) >> gate2ModeShift;}
inline int ppsToIndex(int pulsesPerStep) {return (pulsesPerStep == 24 ? 3 : (pulsesPerStep == 12 ? 2 : (pulsesPerStep == 4 ? 1 : 0)));}// map 1,4,12,24, to 0,1,2,3
inline int indexToPps(int index) {return (index == 3 ? 24 : (index == 2 ? 12 : (index == 1 ? 4 : 1)));}// inverse map of above

	
// Other methods (code in PhraseSeqUtil.cpp)	
												
int moveIndex(int index, int indexNext, int numSteps);
bool moveIndexRunMode(int* index, int numSteps, int runMode, int* history);
int calcGates(int attribute, SchmittTrigger clockTrigger, int ppqnCount, int pulsesPerStep);// TODO delete this when finished code below


inline bool calcGate(int gateCode, SchmittTrigger clockTrigger, unsigned long clockStep, float sampleRate) {
	if (gateCode < 2) 
		return gateCode == 1;
	if (gateCode == 2)
		return clockTrigger.isHigh();
	return clockStep < (unsigned long) (sampleRate * 0.010f);// home made trigger (TODO: set to 1ms after debug)
}

inline int getAdvGate(int ppqnCount, int pulsesPerStep, int gateMode) { 
	if (gateMode == GATE_TRIG)
		return ppqnCount == 0 ? 3 : 0;
	uint32_t shiftAmt = (24 - ppqnCount) - 1;
	return (int)((advGateHitMask[gateMode] >> shiftAmt) & (uint32_t)0x1);// TODO handle 4 and 12 ppqn
}

inline int calcGate1Code(int attribute, int ppqnCount, int pulsesPerStep, float randKnob) {// 0 = gate off, 1 = gate on, 2 = clock high, 3 = trigger
	if (!getGate1a(attribute))
		return 0;
	if (getGate1Pa(attribute) && !(randomUniform() < randKnob))// randomUniform is [0.0, 1.0), see include/util/common.hpp
		return 0;
	if (pulsesPerStep == 1)
		return 2;// clock high
	return getAdvGate(ppqnCount, pulsesPerStep, getGate1aMode(attribute));
}
inline int calcGate2Code(int attribute, int ppqnCount, int pulsesPerStep) {// 0 = gate off, 1 = clock high, 2 = trigger, 3 = gate on
	if (!getGate2a(attribute))
		return 0;
	if (pulsesPerStep == 1)
		return 2;// clock high
	return getAdvGate(ppqnCount, pulsesPerStep, getGate2aMode(attribute));
}

