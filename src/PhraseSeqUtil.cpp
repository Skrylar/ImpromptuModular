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


int gateModeToKeyLightIndex(int attribute, bool isGate1) {
	int gateMode = isGate1 ? getGate1aMode(attribute) : getGate2aMode(attribute);
	switch (gateMode) {
		case (0) : return 2; break;
		case (1) : return 4; break;
		case (2) : return 5; break;
		case (3) : return 7; break;
		case (4) : return 0; break;
		case (5) : return 9; break;
		case (6) : return 1; break;
		case (7) : return 3; break;
		case (8) : return 11; break;
		case (9) : return 6; break;
		case (10) : return 8; break;
		default: return 10;
	}
	return 2;
}

int keyIndexToGateMode(int keyIndex) {
	switch (keyIndex) {
		case (2) : return 0; break;
		case (4) : return 1; break;
		case (5) : return 2; break;
		case (7) : return 3; break;
		case (0) : return 4; break;
		case (9) : return 5; break;
		case (1) : return 6; break;
		case (3) : return 7; break;
		case (11) : return 8; break;
		case (6) : return 9; break;
		case (8) : return 10; break;
		default: return 11;
	}
	return 0;
}
