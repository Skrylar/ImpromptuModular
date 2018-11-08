//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "dsp/digital.hpp"
#include "PhraseSeq32ExUtil.hpp"

using namespace rack;


// TODO: move non trivial methods here

const int SequencerKernel::ppsValues[NUM_PPS_VALUES] = {1, 4, 6, 8, 12, 16, 18, 20, 24, 28, 30, 32, 36, 40, 42, 44, 48, 52, 54, 56, 60, 64, 66, 68, 72, 76, 78, 80, 84, 88, 90, 92, 96};



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


