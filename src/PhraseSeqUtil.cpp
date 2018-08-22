//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************


#include "PhraseSeqUtil.hpp"


int moveIndex(int index, int indexNext, int numSteps) {
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


bool moveIndexRunMode(int* index, int numSteps, int runMode, int* history) {		
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
					(*index) = numSteps - 1;
					(*history) = 2001;
				}
			}
			else {// it is 2001; reverse phase
				(*index)--;
				if ((*index) < 0) {
					(*index) = 0;
					(*history) = 2000;
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
		case MODE_FW3 :// forward three times
		case MODE_FW4 :// forward four times
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

static const int modeToLightTable[NUM_GATES] = {2, 4, 5, 7, 0, 9, 1, 3, 11, 6, 8, 10};
int gateModeToKeyLightIndex(int attribute, bool isGate1) {
	int gateMode = isGate1 ? getGate1aMode(attribute) : getGate2aMode(attribute);
	return modeToLightTable[gateMode];
}

static const int keyToModeTable[NUM_GATES] = {4, 6, 0, 7,  1, 2, 9, 3,  10, 5, 11, 8};
int keyIndexToGateMode(int keyIndex, int pulsesPerStep) {
	if (pulsesPerStep == 4 && (keyIndex == 6 || keyIndex == 8 || keyIndex >= 10))
		return -1;
	if (pulsesPerStep == 6 && (keyIndex == 1 || keyIndex == 2 || keyIndex == 3 || keyIndex == 5 || keyIndex == 9))
		return -1;
	return keyToModeTable[keyIndex];
}
