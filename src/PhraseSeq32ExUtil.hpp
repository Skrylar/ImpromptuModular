//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "dsp/digital.hpp"

using namespace rack;



// General constants

static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)

// Run modes
enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_PEN, MODE_BRN, MODE_RND, NUM_MODES};
static const std::string modeLabels[NUM_MODES]={"FWD","REV","PPG","PEN","BRN","RND"};

// Gate types
static const int NUM_GATES = 12;	

// Clock resolution										
static const int NUM_PPS_VALUES = 33;
static const int ppsValues[NUM_PPS_VALUES] = {1, 4, 6, 8, 12, 16, 18, 20, 24, 28, 30, 32, 36, 40, 42, 44, 48, 52, 54, 56, 60, 64, 66, 68, 72, 76, 78, 80, 84, 88, 90, 92, 96};

// Attributes of a step
typedef unsigned long TAttribute;
static const TAttribute ATT_MSK_GATE = 0x01;
static const TAttribute ATT_MSK_GATEP = 0x02;
static const TAttribute ATT_MSK_SLIDE = 0x04;
static const TAttribute ATT_MSK_TIED = 0x08;
static const TAttribute ATT_MSK_GATETYPE = 0xF0;
static const TAttribute gate1TypeShift = 4;
static const TAttribute ATT_MSK_GATEP_VAL = 0xFF00;
static const TAttribute GatePValShift = 8;
static const TAttribute ATT_MSK_SLIDE_VAL = 0xFF0000;
static const TAttribute slideValShift = 16;
static const TAttribute ATT_MSK_INITSTATE = (ATT_MSK_GATE | (0 << gate1TypeShift) | (50 << GatePValShift) | (10 << slideValShift));

							
				
// Inline methods

inline bool getGateA(TAttribute attribute) {return (attribute & ATT_MSK_GATE) != 0;}
inline bool getTiedA(TAttribute attribute) {return (attribute & ATT_MSK_TIED) != 0;}
inline bool getGatePa(TAttribute attribute) {return (attribute & ATT_MSK_GATEP) != 0;}
inline int getGatePValA(TAttribute attribute) {return (int)((attribute & ATT_MSK_GATEP_VAL) >> GatePValShift);}
inline bool getSlideA(TAttribute attribute) {return (attribute & ATT_MSK_SLIDE) != 0;}
inline int getSlideValA(TAttribute attribute) {return (int)((attribute & ATT_MSK_SLIDE_VAL) >> slideValShift);}
inline int getGateAType(TAttribute attribute) {return ((int)(attribute & ATT_MSK_GATETYPE) >> gate1TypeShift);}

inline void setGateA(TAttribute *attribute, bool gate1State) {(*attribute) &= ~ATT_MSK_GATE; if (gate1State) (*attribute) |= ATT_MSK_GATE;}
inline void setTiedA(TAttribute *attribute, bool tiedState) {(*attribute) &= ~ATT_MSK_TIED; if (tiedState) (*attribute) |= ATT_MSK_TIED;}
inline void setGatePa(TAttribute *attribute, bool GatePState) {(*attribute) &= ~ATT_MSK_GATEP; if (GatePState) (*attribute) |= ATT_MSK_GATEP;}
inline void setGatePValA(TAttribute *attribute, int gatePval) {(*attribute) &= ~ATT_MSK_GATEP_VAL; (*attribute) |= (((TAttribute)gatePval) << GatePValShift);}
inline void setSlideA(TAttribute *attribute, bool slideState) {(*attribute) &= ~ATT_MSK_SLIDE; if (slideState) (*attribute) |= ATT_MSK_SLIDE;}
inline void setSlideValA(TAttribute *attribute, int slideVal) {(*attribute) &= ~ATT_MSK_SLIDE_VAL; (*attribute) |= (((TAttribute)slideVal) << slideValShift);}
inline void setGateTypeA(TAttribute *attribute, int gate1Type) {(*attribute) &= ~ATT_MSK_GATETYPE; (*attribute) |= (((TAttribute)gate1Type) << gate1TypeShift);}

inline void toggleGateA(TAttribute *attribute) {(*attribute) ^= ATT_MSK_GATE;}
inline void toggleTiedA(TAttribute *attribute) {(*attribute) ^= ATT_MSK_TIED;}
inline void toggleGatePa(TAttribute *attribute) {(*attribute) ^= ATT_MSK_GATEP;}
inline void toggleSlideA(TAttribute *attribute) {(*attribute) ^= ATT_MSK_SLIDE;}


inline bool calcGate(int gateCode, SchmittTrigger clockTrigger, unsigned long clockStep, float sampleRate) {
	if (gateCode < 2) 
		return gateCode == 1;
	if (gateCode == 2)
		return clockTrigger.isHigh();
	return clockStep < (unsigned long) (sampleRate * 0.001f);
}



// Other methods 

int calcGateCodeEx(TAttribute attribute, int ppqnCount, int pulsesPerStepIndex) {
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

int keyIndexToGateTypeEx(int keyIndex, int pulsesPerStepIndex) {
	int pulsesPerStep = ppsValues[pulsesPerStepIndex];
	if (((pulsesPerStep % 6) != 0) && (keyIndex == 1 || keyIndex == 3 || keyIndex == 6 || keyIndex == 8 || keyIndex == 10))// TRIs
		return -1;
	if (((pulsesPerStep % 4) != 0) && (keyIndex == 0 || keyIndex == 4 || keyIndex == 7 || keyIndex == 9))// DOUBLEs
		return -1;
	return keyIndex;// keyLight index now matches gate types, so no mapping table needed anymore
}

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


bool moveIndexRunModeEx(int* index, int numSteps, int runMode, unsigned long* history, int reps) {	
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





