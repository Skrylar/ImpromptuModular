//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "dsp/digital.hpp"

using namespace rack;


// General constants

static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_PEN, MODE_BRN, MODE_RND, NUM_MODES};
static const std::string modeLabels[NUM_MODES]={"FWD","REV","PPG","PEN","BRN","RND"};

static const int NUM_GATES = 12;												
static const uint32_t advGateHitMask[NUM_GATES] = 
{0x00003F, 0x0F0F0F, 0x000FFF, 0x0F0F00, 0x03FFFF, 0xFFFFFF, 0x00000F, 0x03F03F, 0x000F00, 0x03F000, 0x0F0000, 0};
//	  25%		TRI		  50%		T23		  75%		FUL		  TR1 		DUO		  TR2 	     D2		  TR3  TRIG		

enum AttributeBitMasks {ATT_MSK_GATE1 = 0x01, ATT_MSK_GATE1P = 0x02, ATT_MSK_SLIDE = 0x04, ATT_MSK_TIED = 0x08};// 4 bits
static const int ATT_MSK_GATE1MODE = 0x00F0;// 4 bits
static const int gate1ModeShift = 4;

							
				
// Inline methods
inline bool getGate1a(int attribute) {return (attribute & ATT_MSK_GATE1) != 0;}
inline bool getGate1Pa(int attribute) {return (attribute & ATT_MSK_GATE1P) != 0;}
inline bool getSlideA(int attribute) {return (attribute & ATT_MSK_SLIDE) != 0;}
inline bool getTiedA(int attribute) {return (attribute & ATT_MSK_TIED) != 0;}
inline int getGate1aMode(int attribute) {return (attribute & ATT_MSK_GATE1MODE) >> gate1ModeShift;}

inline void setGate1a(int *attribute, bool gate1State) {(*attribute) &= ~ATT_MSK_GATE1; if (gate1State) (*attribute) |= ATT_MSK_GATE1;}
inline void setGate1Pa(int *attribute, bool gate1PState) {(*attribute) &= ~ATT_MSK_GATE1P; if (gate1PState) (*attribute) |= ATT_MSK_GATE1P;}
inline void setSlideA(int *attribute, bool slideState) {(*attribute) &= ~ATT_MSK_SLIDE; if (slideState) (*attribute) |= ATT_MSK_SLIDE;}
inline void setTiedA(int *attribute, bool tiedState) {(*attribute) &= ~ATT_MSK_TIED; if (tiedState) (*attribute) |= ATT_MSK_TIED;}

inline void toggleGate1a(int *attribute) {(*attribute) ^= ATT_MSK_GATE1;}
inline void toggleGate1Pa(int *attribute) {(*attribute) ^= ATT_MSK_GATE1P;}
inline void toggleSlideA(int *attribute) {(*attribute) ^= ATT_MSK_SLIDE;}
inline void toggleTiedA(int *attribute) {(*attribute) ^= ATT_MSK_TIED;}


inline int ppsToIndex(int pulsesPerStep) {// map 1,4,6,12,24, to 0,1,2,3,4
	if (pulsesPerStep == 1) return 0;
	if (pulsesPerStep == 4) return 1; 
	if (pulsesPerStep == 6) return 2;
	if (pulsesPerStep == 12) return 3; 
	return 4; 
}
inline int indexToPps(int index) {// inverse map of ppsToIndex()
	index = clamp(index, 0, 4); 
	if (index == 0) return 1;
	if (index == 1) return 4; 
	if (index == 2) return 6;
	if (index == 3) return 12; 
	return 24; 
}

inline bool calcGate(int gateCode, SchmittTrigger clockTrigger, unsigned long clockStep, float sampleRate) {
	if (gateCode < 2) 
		return gateCode == 1;
	if (gateCode == 2)
		return clockTrigger.isHigh();
	return clockStep < (unsigned long) (sampleRate * 0.001f);
}

inline int getAdvGate(int ppqnCount, int pulsesPerStep, int gateMode) { 
	if (gateMode == 11)
		return ppqnCount == 0 ? 3 : 0;
	uint32_t shiftAmt = ppqnCount * (24 / pulsesPerStep);
	return (int)((advGateHitMask[gateMode] >> shiftAmt) & (uint32_t)0x1);
}

inline int calcGate1Code(int attribute, int ppqnCount, int pulsesPerStep, float randKnob) {
	// -1 = gate off for whole step, 0 = gate off for current ppqn, 1 = gate on, 2 = clock high, 3 = trigger
	if (ppqnCount == 0 && getGate1Pa(attribute) && !(randomUniform() < randKnob))// randomUniform is [0.0, 1.0), see include/util/common.hpp
		return -1;// must do this first in this method since it will kill rest of step if prob turns off the step
	if (!getGate1a(attribute))
		return 0;
	if (pulsesPerStep == 1)
		return 2;// clock high
	return getAdvGate(ppqnCount, pulsesPerStep, getGate1aMode(attribute));
}



// Other methods 

int moveIndexEx(int index, int indexNext, int numSteps) {
	if (indexNext < 0)
		index = numSteps - 1;
	else
	{
		if (indexNext - index >= 0) { // if moving right or same place
			if (indexNext >= numSteps)
				index = 0;
			else
				index = indexNext;
		}
		else { // moving left 
			if (indexNext >= numSteps)
				index = numSteps - 1;
			else
				index = indexNext;
		}
	}
	return index;
}


bool moveIndexRunModeEx(int* index, int numSteps, int runMode, unsigned int* history, int reps) {	
	// assert((reps * numSteps) <= 0xFFF); // for BRN and RND run modes, history is not a span count but a step count
	
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


int keyIndexToGateModeEx(int keyIndex, int pulsesPerStep) {
	if (pulsesPerStep == 4 && (keyIndex == 1 || keyIndex == 3 || keyIndex == 6 || keyIndex == 8 || keyIndex == 10))
		return -1;
	if (pulsesPerStep == 6 && (keyIndex == 0 || keyIndex == 4 || keyIndex == 7 || keyIndex == 9))
		return -1;
	return keyIndex;// keyLight index now matches gate modes, so no mapping table needed anymore
}


