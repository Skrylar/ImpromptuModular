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

				
// Inline methods
				
inline int ppsToIndex(int pulsesPerStep) {return (pulsesPerStep == 24 ? 3 : (pulsesPerStep == 12 ? 2 : (pulsesPerStep == 4 ? 1 : 0)));}// map 1,4,12,24, to 0,1,2,3
inline int indexToPps(int index) {return (index == 3 ? 24 : (index == 2 ? 12 : (index == 1 ? 4 : 1)));}// inverse map of above

	
// Other methods (code in PhraseSeqUtil.cpp)	
												
int moveIndex(int index, int indexNext, int numSteps);
bool moveIndexRunMode(int* index, int numSteps, int runMode, int* history);
bool calcGate(bool gateOn, SchmittTrigger clockTrigger, int ppqnCount, int pulsesPerStep);
