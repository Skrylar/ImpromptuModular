//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "dsp/digital.hpp"

using namespace rack;


// General constants

static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_PEN, MODE_BRN, MODE_RND, MODE_FW2, NUM_MODES};
static const std::string modeLabels[NUM_MODES]={"FWD","REV","PPG","PEN","BRN","RND","FW2"};// PS16 and SMS16 use NUM_MODES - 1 since no RN2!!!

static const int NUM_GATES = 12;												
static const uint32_t advGateHitMask[NUM_GATES] = 
{0x00003F, 0x0F0F0F, 0x000FFF, 0x0F0F00, 0x03FFFF, 0xFFFFFF, 0x00000F, 0x03F03F, 0x000F00, 0x03F000, 0x0F0000, 0};
//	  25%		TRI		  50%		T23		  75%		FUL		  TR1 		DUO		  TR2 	     D2		  TR3  TRIG		

enum AttributeBitMasks {ATT_MSK_GATE1 = 0x01, ATT_MSK_GATE1P = 0x02, ATT_MSK_GATE2 = 0x04, ATT_MSK_SLIDE = 0x08, ATT_MSK_TIED = 0x10};// 5 bits
static const int ATT_MSK_GATE1MODE = 0x01E0;// 4 bits
static const int gate1ModeShift = 5;
static const int ATT_MSK_GATE2MODE = 0x1E00;// 4 bits
static const int gate2ModeShift = 9;

							
				
// Inline methods
inline int calcNewGateMode(int currentGateMode, int deltaKnob) {return clamp(currentGateMode + deltaKnob, 0, NUM_GATES - 1);}

inline bool getGate1a(int attribute) {return (attribute & ATT_MSK_GATE1) != 0;}
inline bool getGate1Pa(int attribute) {return (attribute & ATT_MSK_GATE1P) != 0;}
inline bool getGate2a(int attribute) {return (attribute & ATT_MSK_GATE2) != 0;}
inline bool getSlideA(int attribute) {return (attribute & ATT_MSK_SLIDE) != 0;}
inline bool getTiedA(int attribute) {return (attribute & ATT_MSK_TIED) != 0;}
inline int getGate1aMode(int attribute) {return (attribute & ATT_MSK_GATE1MODE) >> gate1ModeShift;}
inline int getGate2aMode(int attribute) {return (attribute & ATT_MSK_GATE2MODE) >> gate2ModeShift;}

inline void setGate1a(int *attribute, bool gate1State) {(*attribute) &= ~ATT_MSK_GATE1; if (gate1State) (*attribute) |= ATT_MSK_GATE1;}
inline void setGate1Pa(int *attribute, bool gate1PState) {(*attribute) &= ~ATT_MSK_GATE1P; if (gate1PState) (*attribute) |= ATT_MSK_GATE1P;}
inline void setGate2a(int *attribute, bool gate2State) {(*attribute) &= ~ATT_MSK_GATE2; if (gate2State) (*attribute) |= ATT_MSK_GATE2;}
inline void setSlideA(int *attribute, bool slideState) {(*attribute) &= ~ATT_MSK_SLIDE; if (slideState) (*attribute) |= ATT_MSK_SLIDE;}
inline void setTiedA(int *attribute, bool tiedState) {(*attribute) &= ~ATT_MSK_TIED; if (tiedState) (*attribute) |= ATT_MSK_TIED;}

inline void toggleGate1a(int *attribute) {(*attribute) ^= ATT_MSK_GATE1;}
inline void toggleGate1Pa(int *attribute) {(*attribute) ^= ATT_MSK_GATE1P;}
inline void toggleGate2a(int *attribute) {(*attribute) ^= ATT_MSK_GATE2;}
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
inline int calcGate2Code(int attribute, int ppqnCount, int pulsesPerStep) {
	// 0 = gate off, 1 = clock high, 2 = trigger, 3 = gate on
	if (!getGate2a(attribute))
		return 0;
	if (pulsesPerStep == 1)
		return 2;// clock high
	return getAdvGate(ppqnCount, pulsesPerStep, getGate2aMode(attribute));
}

inline int gateModeToKeyLightIndex(int attribute, bool isGate1) {// keyLight index now matches gate modes, so no mapping table needed anymore
	return isGate1 ? getGate1aMode(attribute) : getGate2aMode(attribute);
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


bool moveIndexRunModeEx(int* index, int numSteps, int runMode, int* history) {		
	bool crossBoundary = false;
	int numRuns;// for FWx
	
	switch (runMode) {
	
		case MODE_REV :// reverse; history base is 1000 (not needed)
			(*history) = 1000;
			(*index)--;
			if ((*index) < 0) {
				(*index) = numSteps - 1;
				crossBoundary = true;
			}
		break;
		

		case MODE_PPG :// forward-reverse; history base is 2000
			if ((*history) != 2000 && (*history) != 2001) // 2000 means going forward, 2001 means going reverse
				(*history) = 2000;
			if ((*history) == 2000) {// forward phase
				(*index)++;
				if ((*index) >= numSteps) {
					(*index) = numSteps - 1 ;//- (numSteps > 1 ? 1 : 0);// last term was absent in former PPG method
					(*history) = 2001;
				}
			}
			else {// it is 2001; reverse phase
				(*index)--;
				if ((*index) < 0) {// was 0 in former PPG method
					(*index) = 0;
					(*history) = 2000;
					crossBoundary = true;
				}
			}
		break;

		case MODE_PEN :// forward-reverse; history base is 6000
			if ((*history) != 6000 && (*history) != 6001) // 6000 means going forward, 6001 means going reverse
				(*history) = 6000;
			if ((*history) == 6000) {// forward phase
				(*index)++;
				if ((*index) >= numSteps) {
					(*index) = numSteps - 2;
					if ((*index) < 1) {// if back at 0 after turnaround, then no reverse phase needed
						crossBoundary = true;
						(*index) = 0;
					}
					else
						(*history) = 6001;
				}
			}
			else {// it is 6001; reverse phase
				(*index)--;
				if ((*index) < 1) {// was 0 in former PPG method
					(*index) = 0;
					(*history) = 6000;
					crossBoundary = true;
				}
			}
		break;
		
		case MODE_BRN :// brownian random; history base is 3000
			if ( (*history) < 3000 || ((*history) > (3000 + numSteps)) ) 
				(*history) = 3000 + numSteps;
			(*index) += (randomu32() % 3) - 1;
			if ((*index) >= numSteps) {
				(*index) = 0;
			}
			if ((*index) < 0) {
				(*index) = numSteps - 1;
			}
			(*history)--;
			if ((*history) <= 3000) {
				(*history) = 3000 + numSteps;
				crossBoundary = true;
			}
		break;
		
		case MODE_RND :// random; history base is 4000
			if ( (*history) < 4000 || ((*history) > (4000 + numSteps)) ) 
				(*history) = 4000 + numSteps;
			(*index) = (randomu32() % numSteps) ;
			(*history)--;
			if ((*history) <= 4000) {
				(*history) = 4000 + numSteps;
				crossBoundary = true;
			}
		break;
		
		case MODE_FW2 :// forward twice
			numRuns = 5002 + runMode - MODE_FW2;
			if ( (*history) < 5000 || (*history) >= numRuns ) // 5000 means first pass, 5001 means 2nd pass, etc...
				(*history) = 5000;
			(*index)++;
			if ((*index) >= numSteps) {
				(*index) = 0;
				(*history)++;
				if ((*history) >= numRuns) {
					(*history) = 5000;
					crossBoundary = true;
				}				
			}
		break;

		default :// MODE_FWD  forward; history base is 0 (not needed)
			(*history) = 0;
			(*index)++;
			if ((*index) >= numSteps) {
				(*index) = 0;
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


