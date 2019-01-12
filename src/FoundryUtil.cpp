//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "dsp/digital.hpp"
#include "FoundryUtil.hpp"

using namespace rack;



//*****************************************************************************
// SequencerKernel
//*****************************************************************************


const std::string SequencerKernel::modeLabels[NUM_MODES] = {"FWD", "REV", "PPG", "PEN", "BRN", "RND", "TKA"};


const uint64_t SequencerKernel::advGateHitMaskLow[NUM_GATES] = 
{0x0000000000FFFFFF, 0x0000FFFF0000FFFF, 0x0000FFFFFFFFFFFF, 0x0000FFFF00000000, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 
//				25%					TRI		  			50%					T23		  			75%					FUL		
 0x000000000000FFFF, 0xFFFF000000FFFFFF, 0x0000FFFF00000000, 0xFFFF000000000000, 0x0000000000000000, 0};
//  			TR1 				DUO		  			TR2 	     		D2		  			TR3  TRIG		
const uint64_t SequencerKernel::advGateHitMaskHigh[NUM_GATES] = 
{0x0000000000000000, 0x000000000000FFFF, 0x0000000000000000, 0x000000000000FFFF, 0x00000000000000FF, 0x00000000FFFFFFFF, 
//				25%					TRI		  			50%					T23		  			75%					FUL		
 0x0000000000000000, 0x00000000000000FF, 0x0000000000000000, 0x00000000000000FF, 0x000000000000FFFF, 0};
//  			TR1 				DUO		  			TR2 	     		D2		  			TR3  TRIG		


void SequencerKernel::construct(int _id, SequencerKernel *_masterKernel, bool* _holdTiedNotesPtr) {// don't want regaular constructor mechanism
	id = _id;
	ids = "id" + std::to_string(id) + "_";
	masterKernel = _masterKernel;
	holdTiedNotesPtr = _holdTiedNotesPtr;
}



void SequencerKernel::setGate(int seqn, int stepn, bool newGate, int count) {
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = stepn; i < endi; i++)
		attributes[seqn][i].setGate(newGate);
}
void SequencerKernel::setGateP(int seqn, int stepn, bool newGateP, int count) {
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = stepn; i < endi; i++)
		attributes[seqn][i].setGateP(newGateP);
}
void SequencerKernel::setSlide(int seqn, int stepn, bool newSlide, int count) {
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = stepn; i < endi; i++)
		attributes[seqn][i].setSlide(newSlide);
}
void SequencerKernel::setTied(int seqn, int stepn, bool newTied, int count) {
	int endi = min(MAX_STEPS, stepn + count);
	if (!newTied) {
		for (int i = stepn; i < endi; i++)
			deactivateTiedStep(seqn, i);
	}
	else {
		for (int i = stepn; i < endi; i++)
			activateTiedStep(seqn, i);
	}
}

void SequencerKernel::setGatePVal(int seqn, int stepn, int gatePval, int count) {
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = stepn; i < endi; i++)
		attributes[seqn][i].setGatePVal(gatePval);
}
void SequencerKernel::setSlideVal(int seqn, int stepn, int slideVal, int count) {
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = stepn; i < endi; i++)
		attributes[seqn][i].setSlideVal(slideVal);
}
void SequencerKernel::setVelocityVal(int seqn, int stepn, int velocity, int count) {
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = stepn; i < endi; i++)
		attributes[seqn][i].setVelocityVal(velocity);
}
void SequencerKernel::setGateType(int seqn, int stepn, int gateType, int count) {
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = stepn; i < endi; i++)
		attributes[seqn][i].setGateType(gateType);
}


float SequencerKernel::applyNewOctave(int seqn, int stepn, int newOct, int count) {// does not overwrite tied steps
	float newCV = cv[seqn][stepn] + 10.0f;//to properly handle negative note voltages
	newCV = newCV - floor(newCV) + (float) (newOct - 3);
	
	writeCV(seqn, stepn, newCV, count);
	return newCV;
}
float SequencerKernel::applyNewKey(int seqn, int stepn, int newKeyIndex, int count) {// does not overwrite tied steps
	float newCV = floor(cv[seqn][stepn]) + ((float) newKeyIndex) / 12.0f;
	
	writeCV(seqn, stepn, newCV, count);
	return newCV;
}
void SequencerKernel::writeCV(int seqn, int stepn, float newCV, int count) {// does not overwrite tied steps
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = stepn; i < endi; i++) {
		if (!attributes[seqn][i].getTied()) {
			cv[seqn][i] = newCV;
			propagateCVtoTied(seqn, i);
		}
	}
}


void SequencerKernel::initSequence(int seqn) {
	sequences[seqn].init(MAX_STEPS, MODE_FWD);
	for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
		cv[seqn][stepn] = INIT_CV;
		attributes[seqn][stepn].init();
	}
}
void SequencerKernel::initSong() {
	runModeSong = MODE_FWD;
	songBeginIndex = 0;
	songEndIndex = 0;
	for (int phrn = 0; phrn < MAX_PHRASES; phrn++) {
		phrases[phrn].init();
	}
}


void SequencerKernel::randomizeSequence(int seqn) {
	sequences[seqn].randomize(MAX_STEPS, NUM_MODES);// code below uses lengths so this must be randomized first
	for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
		cv[seqn][stepn] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
		attributes[seqn][stepn].randomize();
		if (attributes[seqn][stepn].getTied()) {
			activateTiedStep(seqn, stepn);
		}	
	}
}
void SequencerKernel::randomizeSong() {
	runModeSong = randomu32() % NUM_MODES;
	songBeginIndex = 0;
	songEndIndex = (randomu32() % MAX_PHRASES);
	for (int phrn = 0; phrn < MAX_PHRASES; phrn++) {
		phrases[phrn].randomize(MAX_SEQS);
	}
}	


void SequencerKernel::copySequence(SeqCPbuffer* seqCPbuf, int seqn, int startCP, int countCP) {
	countCP = min(countCP, MAX_STEPS - startCP);
	for (int i = 0, stepn = startCP; i < countCP; i++, stepn++) {
		seqCPbuf->cvCPbuffer[i] = cv[seqn][stepn];
		seqCPbuf->attribCPbuffer[i] = attributes[seqn][stepn];
	}
	seqCPbuf->seqAttribCPbuffer = sequences[seqn];
	seqCPbuf->storedLength = countCP;
}
void SequencerKernel::pasteSequence(SeqCPbuffer* seqCPbuf, int seqn, int startCP) {
	int countCP = min(seqCPbuf->storedLength, MAX_STEPS - startCP);
	for (int i = 0, stepn = startCP; i < countCP; i++, stepn++) {
		cv[seqn][stepn] = seqCPbuf->cvCPbuffer[i];
		attributes[seqn][stepn] = seqCPbuf->attribCPbuffer[i];
	}
	if (startCP == 0 && countCP == MAX_STEPS)
		sequences[seqn] = seqCPbuf->seqAttribCPbuffer;
}
void SequencerKernel::copySong(SongCPbuffer* songCPbuf, int startCP, int countCP) {	
	countCP = min(countCP, MAX_PHRASES - startCP);
	for (int i = 0, phrn = startCP; i < countCP; i++, phrn++) {
		songCPbuf->phraseCPbuffer[i] = phrases[phrn];
	}
	songCPbuf->beginIndex = songBeginIndex;
	songCPbuf->endIndex = songEndIndex;
	songCPbuf->runModeSong = runModeSong;
	songCPbuf->storedLength = countCP;
}
void SequencerKernel::pasteSong(SongCPbuffer* songCPbuf, int startCP) {	
	int countCP = min(songCPbuf->storedLength, MAX_PHRASES - startCP);
	for (int i = 0, phrn = startCP; i < countCP; i++, phrn++) {
		phrases[phrn] = songCPbuf->phraseCPbuffer[i];
	}
	if (startCP == 0 && countCP == MAX_PHRASES) {
		songBeginIndex = songCPbuf->beginIndex;
		songEndIndex = songCPbuf->endIndex;
		runModeSong = songCPbuf->runModeSong;
	}
}


void SequencerKernel::reset() {
	initPulsesPerStep();
	initDelay();
	initSong();
	for (int seqn = 0; seqn < MAX_SEQS; seqn++) {
		initSequence(seqn);		
	}
	clockPeriod = 0ul;
	initRun();
}


void SequencerKernel::randomize() {
	randomizeSong();
	for (int seqn = 0; seqn < MAX_SEQS; seqn++) {
		randomizeSequence(seqn);
	}
	initRun();
}
	

void SequencerKernel::initRun() {
	movePhraseIndexRun(true);// true means init 

	int seqn = phrases[phraseIndexRun].getSeqNum();
	stepIndexRun = (sequences[seqn].getRunMode() == MODE_REV ? sequences[seqn].getLength() - 1 : 0);
	stepIndexRunHistory = 0;

	ppqnCount = 0;
	ppqnLeftToSkip = delay;
	calcGateCodeEx(seqn);// uses stepIndexRun as the step
	slideStepsRemain = 0ul;
}


void SequencerKernel::toJson(json_t *rootJ) {
	// pulsesPerStep
	json_object_set_new(rootJ, (ids + "pulsesPerStep").c_str(), json_integer(pulsesPerStep));

	// delay
	json_object_set_new(rootJ, (ids + "delay").c_str(), json_integer(delay));

	// runModeSong
	json_object_set_new(rootJ, (ids + "runModeSong").c_str(), json_integer(runModeSong));

	// sequences (attributes of a seqs)
	json_t *sequencesJ = json_array();
	for (int i = 0; i < MAX_SEQS; i++)
		json_array_insert_new(sequencesJ, i, json_integer(sequences[i].getSeqAttrib()));
	json_object_set_new(rootJ, (ids + "sequences").c_str(), sequencesJ);

	// phrases 
	json_t *phrasesJ = json_array();
	for (int i = 0; i < MAX_PHRASES; i++)
		json_array_insert_new(phrasesJ, i, json_integer(phrases[i].getPhraseJson()));
	json_object_set_new(rootJ, (ids + "phrases").c_str(), phrasesJ);

	// CV and attributes
	json_t *seqSavedJ = json_array();		
	json_t *cvJ = json_array();
	json_t *attributesJ = json_array();
	for (int seqnRead = 0, seqnWrite = 0; seqnRead < MAX_SEQS; seqnRead++) {
		bool compress = true;
		for (int stepn = 0; stepn < 5; stepn++) {
			if (cv[seqnRead][stepn] != INIT_CV || attributes[seqnRead][stepn].getAttribute() != StepAttributes::ATT_MSK_INITSTATE) {
				compress = false;
				break;
			}
		}
		if (compress) {
			json_array_insert_new(seqSavedJ, seqnRead, json_integer(0));
		}
		else {
			json_array_insert_new(seqSavedJ, seqnRead, json_integer(1));
			for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
				json_array_insert_new(cvJ, stepn + (seqnWrite * MAX_STEPS), json_real(cv[seqnRead][stepn]));
				json_array_insert_new(attributesJ, stepn + (seqnWrite * MAX_STEPS), json_integer(attributes[seqnRead][stepn].getAttribute()));
			}
			seqnWrite++;
		}
	}
	json_object_set_new(rootJ, (ids + "seqSaved").c_str(), seqSavedJ);
	json_object_set_new(rootJ, (ids + "cv").c_str(), cvJ);
	json_object_set_new(rootJ, (ids + "attributes").c_str(), attributesJ);

	// songBeginIndex
	json_object_set_new(rootJ, (ids + "songBeginIndex").c_str(), json_integer(songBeginIndex));

	// songEndIndex
	json_object_set_new(rootJ, (ids + "songEndIndex").c_str(), json_integer(songEndIndex));

}


void SequencerKernel::fromJson(json_t *rootJ) {
	// pulsesPerStep
	json_t *pulsesPerStepJ = json_object_get(rootJ, (ids + "pulsesPerStep").c_str());
	if (pulsesPerStepJ)
		pulsesPerStep = json_integer_value(pulsesPerStepJ);

	// delay
	json_t *delayJ = json_object_get(rootJ, (ids + "delay").c_str());
	if (delayJ)
		delay = json_integer_value(delayJ);

	// runModeSong
	json_t *runModeSongJ = json_object_get(rootJ, (ids + "runModeSong").c_str());
	if (runModeSongJ)
		runModeSong = json_integer_value(runModeSongJ);
			
	// sequences (attributes of a seqs)
	json_t *sequencesJ = json_object_get(rootJ, (ids + "sequences").c_str());
	if (sequencesJ) {
		for (int i = 0; i < MAX_SEQS; i++)
		{
			json_t *sequencesArrayJ = json_array_get(sequencesJ, i);
			if (sequencesArrayJ)
				sequences[i].setSeqAttrib(json_integer_value(sequencesArrayJ));
		}			
	}		
	
	// phrases
	json_t *phrasesJ = json_object_get(rootJ, (ids + "phrases").c_str());
	if (phrasesJ)
		for (int i = 0; i < MAX_PHRASES; i++)
		{
			json_t *phrasesArrayJ = json_array_get(phrasesJ, i);
			if (phrasesArrayJ)
				phrases[i].setPhraseJson(json_integer_value(phrasesArrayJ));
		}
	
	// CV and attributes
	json_t *seqSavedJ = json_object_get(rootJ, (ids + "seqSaved").c_str());
	int seqSaved[MAX_SEQS];
	if (seqSavedJ) {
		int i;
		for (i = 0; i < MAX_SEQS; i++)
		{
			json_t *seqSavedArrayJ = json_array_get(seqSavedJ, i);
			if (seqSavedArrayJ)
				seqSaved[i] = json_integer_value(seqSavedArrayJ);
			else 
				break;
		}	
		if (i == MAX_SEQS) {			
			json_t *cvJ = json_object_get(rootJ, (ids + "cv").c_str());
			json_t *attributesJ = json_object_get(rootJ, (ids + "attributes").c_str());
			if (cvJ && attributesJ) {
				for (int seqnFull = 0, seqnComp = 0; seqnFull < MAX_SEQS; seqnFull++) {
					if (!seqSaved[seqnFull]) {
						continue;
					}
					for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
						json_t *cvArrayJ = json_array_get(cvJ, stepn + (seqnComp * MAX_STEPS));
						if (cvArrayJ)
							cv[seqnFull][stepn] = json_number_value(cvArrayJ);
						json_t *attributesArrayJ = json_array_get(attributesJ, stepn + (seqnComp * MAX_STEPS));
						if (attributesArrayJ)
							attributes[seqnFull][stepn].setAttribute(json_integer_value(attributesArrayJ));
					}
					seqnComp++;
				}
			}
		}
	}		
	
	// songBeginIndex
	json_t *songBeginIndexJ = json_object_get(rootJ, (ids + "songBeginIndex").c_str());
	if (songBeginIndexJ)
		songBeginIndex = json_integer_value(songBeginIndexJ);
			
	// songEndIndex
	json_t *songEndIndexJ = json_object_get(rootJ, (ids + "songEndIndex").c_str());
	if (songEndIndexJ)
		songEndIndex = json_integer_value(songEndIndexJ);
}


void SequencerKernel::clockStep(bool realClockEdgeToHandle) {
	if (realClockEdgeToHandle) {
		if (ppqnLeftToSkip > 0) {
			ppqnLeftToSkip--;
		}
		else {
			ppqnCount++;
			int ppsFiltered = getPulsesPerStep();// must use method
			if (ppqnCount >= ppsFiltered)
				ppqnCount = 0;
			if (ppqnCount == 0) {
				float slideFromCV = getCVRun();
				if (moveStepIndexRun()) {
					movePhraseIndexRun(false);// false means normal (not init)
					SeqAttributes newSeq = sequences[phrases[phraseIndexRun].getSeqNum()];
					stepIndexRun = (newSeq.getRunMode() == MODE_REV ? newSeq.getLength() - 1 : 0);// must always refresh after phraseIndexRun has changed
				}

				// Slide
				StepAttributes attribRun = getAttributeRun();
				if (attribRun.getSlide()) {
					slideStepsRemain = (unsigned long) (((float)clockPeriod * ppsFiltered) * ((float)attribRun.getSlideVal() / 100.0f));
					if (slideStepsRemain != 0ul) {
						float slideToCV = getCVRun();
						slideCVdelta = (slideToCV - slideFromCV)/(float)slideStepsRemain;
					}
				}
				else
					slideStepsRemain = 0ul;
			}
			calcGateCodeEx(phrases[phraseIndexRun].getSeqNum());// uses stepIndexRun as the step		
		}
	}
	clockPeriod = 0ul;
}


int SequencerKernel::keyIndexToGateTypeEx(int keyIndex) {// return -1 when invalid gate type given current pps setting
	int ppsFiltered = getPulsesPerStep();// must use method
	int ret = keyIndex;
	
	if (keyIndex == 1 || keyIndex == 3 || keyIndex == 6 || keyIndex == 8 || keyIndex == 10) {// black keys
		if ((ppsFiltered % 6) != 0)
			ret = -1;
	}
	else if (keyIndex == 4 || keyIndex == 7 || keyIndex == 9) {// 75%, DUO, DU2 
		if ((ppsFiltered % 4) != 0)
			ret = -1;
	}
	else if (keyIndex == 2) {// 50%
		if ((ppsFiltered % 2) != 0)
			ret = -1;
	}
	else if (keyIndex == 0) {// 25%
		if (ppsFiltered != 1 && (ppsFiltered % 4) != 0)
			ret = -1;
	}
	//else always good: 5 (full) and 11 (trig)
	
	return ret;
}


void SequencerKernel::transposeSeq(int seqn, int delta) {
	int tVal = sequences[seqn].getTranspose();
	int oldTransposeOffset = tVal;
	tVal = clamp(tVal + delta, -99, 99);
	sequences[seqn].setTranspose(tVal);
	
	delta = tVal - oldTransposeOffset;
	if (delta != 0) { 
		float offsetCV = ((float)(delta))/12.0f;
		for (int stepn = 0; stepn < MAX_STEPS; stepn++) 
			cv[seqn][stepn] += offsetCV;
	}
}


void SequencerKernel::rotateSeq(int* rotateOffset, int seqn, int delta) {
	int oldRotateOffset = *rotateOffset;
	*rotateOffset = clamp(*rotateOffset + delta, -99, 99);
	
	delta = *rotateOffset - oldRotateOffset;
	if (delta == 0) 
		return;// if end of range, no transpose to do
	
	if (delta > 0 && delta < 201) {// Rotate right, 201 is safety
		for (int i = delta; i > 0; i--) {
			rotateSeqByOne(seqn, true);
		}
	}
	if (delta < 0 && delta > -201) {// Rotate left, 201 is safety
		for (int i = delta; i < 0; i++) {
			rotateSeqByOne(seqn, false);
		}
	}
}	


void SequencerKernel::rotateSeqByOne(int seqn, bool directionRight) {
	float rotCV;
	StepAttributes rotAttributes;
	int iStart = 0;
	int iEnd = sequences[seqn].getLength() - 1;
	int iRot = iStart;
	int iDelta = 1;
	if (directionRight) {
		iRot = iEnd;
		iDelta = -1;
	}
	rotCV = cv[seqn][iRot];
	rotAttributes = attributes[seqn][iRot];
	for ( ; ; iRot += iDelta) {
		if (iDelta == 1 && iRot >= iEnd) break;
		if (iDelta == -1 && iRot <= iStart) break;
		cv[seqn][iRot] = cv[seqn][iRot + iDelta];
		attributes[seqn][iRot] = attributes[seqn][iRot + iDelta];
	}
	cv[seqn][iRot] = rotCV;
	attributes[seqn][iRot] = rotAttributes;
}


void SequencerKernel::activateTiedStep(int seqn, int stepn) {
	attributes[seqn][stepn].setTied(true);
	if (stepn > 0) 
		propagateCVtoTied(seqn, stepn - 1);
	
	if (*holdTiedNotesPtr) {// new method
		attributes[seqn][stepn].setGate(true);
		for (int i = max(stepn, 1); i < MAX_STEPS && attributes[seqn][i].getTied(); i++) {
			attributes[seqn][i].setGateType(attributes[seqn][i - 1].getGateType());
			attributes[seqn][i - 1].setGateType(5);
			attributes[seqn][i - 1].setGate(true);
		}
	}
	else {// old method
		if (stepn > 0) {
			attributes[seqn][stepn] = attributes[seqn][stepn - 1];
			attributes[seqn][stepn].setTied(true);
		}
	}
}


void SequencerKernel::deactivateTiedStep(int seqn, int stepn) {
	attributes[seqn][stepn].setTied(false);
	if (*holdTiedNotesPtr) {// new method
		int lastGateType = attributes[seqn][stepn].getGateType();
		for (int i = stepn + 1; i < MAX_STEPS && attributes[seqn][i].getTied(); i++)
			lastGateType = attributes[seqn][i].getGateType();
		if (stepn > 0)
			attributes[seqn][stepn - 1].setGateType(lastGateType);
	}
	//else old method, nothing to do
}


void SequencerKernel::calcGateCodeEx(int seqn) {// uses stepIndexRun as the step
	StepAttributes attribute = attributes[seqn][stepIndexRun];
	int ppsFiltered = getPulsesPerStep();// must use method
	int gateType;

	if (gateCode != -1 || ppqnCount == 0) {// always calc on first ppqnCount, avoid thereafter if gate will be off for whole step
		gateType = attribute.getGateType();
		
		// -1 = gate off for whole step, 0 = gate off for current ppqn, 1 = gate on, 2 = clock high, 3 = trigger
		if ( ppqnCount == 0 && attribute.getGateP() && !(randomUniform() < ((float)attribute.getGatePVal() / 100.0f)) ) {// randomUniform is [0.0, 1.0), see include/util/common.hpp
			gateCode = -1;// must do this first in this method since it will kill all remaining pulses of the step if prob turns off the step
		}
		else if (!attribute.getGate()) {
			gateCode = 0;
		}
		else if (ppsFiltered == 1 && gateType == 0) {
			gateCode = 2;// clock high pulse
		}
		else {
			if (gateType == 11) {
				gateCode = (ppqnCount == 0 ? 3 : 0);// trig on first ppqnCount
			}
			else {
				uint64_t shiftAmt = ppqnCount * (96 / ppsFiltered);
				if (shiftAmt >= 64)
					gateCode = (int)((advGateHitMaskHigh[gateType] >> (shiftAmt - (uint64_t)64)) & (uint64_t)0x1);
				else
					gateCode = (int)((advGateHitMaskLow[gateType] >> shiftAmt) & (uint64_t)0x1);
			}
		}
	}
}
	

bool SequencerKernel::moveStepIndexRun() {	
	int reps = phrases[phraseIndexRun].getReps();// 0-rep seqs should be filtered elsewhere and should never happen here. If they do, they will be played (this can be the case when all of the song has 0-rep seqs, or the song is started (reset) into a first phrase that has 0 reps)
	// assert((reps * MAX_STEPS) <= 0xFFF); // for BRN and RND run modes, history is not a span count but a step count
	int runMode = sequences[phrases[phraseIndexRun].getSeqNum()].getRunMode();
	int endStep = sequences[phrases[phraseIndexRun].getSeqNum()].getLength() - 1;
	
	bool crossBoundary = false;
	
	switch (runMode) {
	
		// history 0x0000 is reserved for reset
		
		case MODE_REV :// reverse; history base is 0x2000
			if (stepIndexRunHistory < 0x2001 || stepIndexRunHistory > 0x2FFF)
				stepIndexRunHistory = 0x2000 + reps;
			stepIndexRun--;
			if (stepIndexRun < 0) {
				stepIndexRun = endStep;
				stepIndexRunHistory--;
				if (stepIndexRunHistory <= 0x2000)
					crossBoundary = true;
			}
		break;
		
		case MODE_PPG :// forward-reverse; history base is 0x3000
			if (stepIndexRunHistory < 0x3001 || stepIndexRunHistory > 0x3FFF) // even means going forward, odd means going reverse
				stepIndexRunHistory = 0x3000 + reps * 2;
			if ((stepIndexRunHistory & 0x1) == 0) {// even so forward phase
				stepIndexRun++;
				if (stepIndexRun > endStep) {
					stepIndexRun = endStep;
					stepIndexRunHistory--;
				}
			}
			else {// odd so reverse phase
				stepIndexRun--;
				if (stepIndexRun < 0) {
					stepIndexRun = 0;
					stepIndexRunHistory--;
					if (stepIndexRunHistory <= 0x3000)
						crossBoundary = true;
				}
			}
		break;

		case MODE_PEN :// forward-reverse; history base is 0x4000
			if (stepIndexRunHistory < 0x4001 || stepIndexRunHistory > 0x4FFF) // even means going forward, odd means going reverse
				stepIndexRunHistory = 0x4000 + reps * 2;
			if ((stepIndexRunHistory & 0x1) == 0) {// even so forward phase
				stepIndexRun++;
				if (stepIndexRun > endStep) {
					stepIndexRun = endStep - 1;
					stepIndexRunHistory--;
					if (stepIndexRun <= 0) {// if back at start after turnaround, then no reverse phase needed
						stepIndexRun = 0;
						stepIndexRunHistory--;
						if (stepIndexRunHistory <= 0x4000)
							crossBoundary = true;
					}
				}
			}
			else {// odd so reverse phase
				stepIndexRun--;
				if (stepIndexRun > endStep)// handle song jumped
					stepIndexRun = endStep;
				if (stepIndexRun <= 0) {
					stepIndexRun = 0;
					stepIndexRunHistory--;
					if (stepIndexRunHistory <= 0x4000)
						crossBoundary = true;
				}
			}
		break;
		
		case MODE_BRN :// brownian random; history base is 0x5000
			if (stepIndexRunHistory < 0x5001 || stepIndexRunHistory > 0x5FFF) 
				stepIndexRunHistory = 0x5000 + (endStep + 1) * reps;			
			stepIndexRun += (randomu32() % 3) - 1;
			if (stepIndexRun > endStep)
				stepIndexRun = 0;
			if (stepIndexRun < 0)
				stepIndexRun = endStep;
			stepIndexRunHistory--;
			if (stepIndexRunHistory <= 0x5000)
				crossBoundary = true;
		break;
		
		case MODE_RND :// random; history base is 0x6000
			if (stepIndexRunHistory < 0x6001 || stepIndexRunHistory > 0x6FFF)
				stepIndexRunHistory = 0x6000 + (endStep + 1) * reps;
			stepIndexRun = (randomu32() % (endStep + 1));
			stepIndexRunHistory--;
			if (stepIndexRunHistory <= 0x6000)
				crossBoundary = true;
		break;
		
		case MODE_TKA :// use track A's stepIndexRun; base is 0x7000
			if (masterKernel != nullptr) {
				stepIndexRunHistory = 0x7000;
				stepIndexRun = masterKernel->getStepIndexRun();
				break;
			}
			[[fallthrough]];
		default :// MODE_FWD  forward; history base is 0x1000
			if (stepIndexRunHistory < 0x1001 || stepIndexRunHistory > 0x1FFF)
				stepIndexRunHistory = 0x1000 + reps;
			stepIndexRun++;
			if (stepIndexRun > endStep) {
				stepIndexRun = 0;
				stepIndexRunHistory--;
				if (stepIndexRunHistory <= 0x1000)
					crossBoundary = true;
			}
	}

	return crossBoundary;
}


void SequencerKernel::moveSongIndexBackward(bool init, bool rollover) {
	int phrn = 0;

	// search backward for next non 0-rep seq, ends up in same phrase if all reps in the song are 0
	if (init) {
		phraseIndexRun = songEndIndex;
		phrn = phraseIndexRun;
	}
	else
		phrn = min(phraseIndexRun - 1, songEndIndex);// handle song jumped
	for (; phrn >= songBeginIndex && phrases[phrn].getReps() == 0; phrn--);
	if (phrn < songBeginIndex) {
		if (rollover)
			for (phrn = songEndIndex; phrn > phraseIndexRun && phrases[phrn].getReps() == 0; phrn--);
		else
			phrn = phraseIndexRun;
		phraseIndexRunHistory--;
	}
	phraseIndexRun = phrn;
}


void SequencerKernel::moveSongIndexForeward(bool init, bool rollover) {
	int phrn = 0;
	
	// search fowrard for next non 0-rep seq, ends up in same phrase if all reps in the song are 0
	if (init) {
		phraseIndexRun = songBeginIndex;
		phrn = phraseIndexRun;
	}
	else
		phrn = max(phraseIndexRun + 1, songBeginIndex);// handle song jumped
	for (; phrn <= songEndIndex && phrases[phrn].getReps() == 0; phrn++);
	if (phrn > songEndIndex) {
		if (rollover)
			for (phrn = songBeginIndex; phrn < phraseIndexRun && phrases[phrn].getReps() == 0; phrn++);
		else
			phrn = phraseIndexRun;
		phraseIndexRunHistory--;
	}
	phraseIndexRun = phrn;
}


void SequencerKernel::moveSongIndexRandom(bool init, uint32_t randomValue) {
	int phrn = songBeginIndex;
	int tpi = 0;
	
	for (;phrn <= songEndIndex; phrn++) {
		if (phrases[phrn].getReps() != 0) {
			tempPhraseIndexes[tpi] = phrn;
			tpi++;
			if (init) break;
		}
	}
	
	if (init) {
		phraseIndexRun = (tpi == 0 ? songBeginIndex : tempPhraseIndexes[0]);
	}
	else {
		phraseIndexRun = tempPhraseIndexes[randomValue % tpi];
	}
}


void SequencerKernel::moveSongIndexBrownian(bool init, uint32_t randomValue) {	
	randomValue = randomValue % 3;// 0 = left, 1 = stay, 2 = right
	
	if (init) {
		moveSongIndexForeward(init, true);
	}
	else if (randomValue == 1) {// stay
		if (phraseIndexRun > songEndIndex || phraseIndexRun < songBeginIndex)
			moveSongIndexForeward(false, true);	
	}
	else if (randomValue == 0) {// left
		moveSongIndexBackward(false, true);
	}
	else {// right
		moveSongIndexForeward(false, true);
	}
}


void SequencerKernel::movePhraseIndexRun(bool init) {	
	if (init)
		phraseIndexRunHistory = 0;
	
	switch (runModeSong) {
	
		// history 0x0000 is reserved for reset
		
		case MODE_REV :// reverse; history base is 0x2000
			phraseIndexRunHistory = 0x2000;
			moveSongIndexBackward(init, true);
		break;
		
		case MODE_PPG :// forward-reverse; history base is 0x3000
			if (phraseIndexRunHistory < 0x3001 || phraseIndexRunHistory > 0x3002) // even means going forward, odd means going reverse
				phraseIndexRunHistory = 0x3002;
			if (phraseIndexRunHistory == 0x3002) {// even so forward phase
				moveSongIndexForeward(init, false);
			}
			else {// odd so reverse phase
				moveSongIndexBackward(false, false);
			}
		break;

		case MODE_PEN :// forward-reverse; history base is 0x4000
			if (phraseIndexRunHistory < 0x4001 || phraseIndexRunHistory > 0x4002) // even means going forward, odd means going reverse
				phraseIndexRunHistory = 0x4002;
			if (phraseIndexRunHistory == 0x4002) {// even so forward phase	
				moveSongIndexForeward(init, false);
				if (phraseIndexRunHistory == 0x4001)
					moveSongIndexBackward(false, false);
			}
			else {// odd so reverse phase
				moveSongIndexBackward(false, false);
				if (phraseIndexRunHistory == 0x4000)
					moveSongIndexForeward(false, false);
			}			
		break;
		
		case MODE_BRN :// brownian random; history base is 0x5000
			phraseIndexRunHistory = 0x5000;
			moveSongIndexBrownian(init, randomu32());
		break;
		
		case MODE_RND :// random; history base is 0x6000
			phraseIndexRunHistory = 0x6000;
			moveSongIndexRandom(init, randomu32());
		break;
		
		case MODE_TKA:// use track A's phraseIndexRun; base is 0x7000
			if (masterKernel != nullptr) {
				phraseIndexRunHistory = 0x7000;
				phraseIndexRun = masterKernel->getPhraseIndexRun();
				break;
			}
			[[fallthrough]];
		default :// MODE_FWD  forward; history base is 0x1000
			phraseIndexRunHistory = 0x1000;
			moveSongIndexForeward(init, true);
	}
}


 
void SeqCPbuffer::reset() {		
	for (int stepn = 0; stepn < SequencerKernel::MAX_STEPS; stepn++) {
		cvCPbuffer[stepn] = 0.0f;
		attribCPbuffer[stepn].init();
	}
	seqAttribCPbuffer.init(SequencerKernel::MAX_STEPS, SequencerKernel::MODE_FWD);
	storedLength = SequencerKernel::MAX_STEPS;// number of steps that contain actual cp data
}

void SongCPbuffer::reset() {
	for (int phrn = 0; phrn < SequencerKernel::MAX_PHRASES; phrn++)
		phraseCPbuffer[phrn].init();
	beginIndex = 0;
	endIndex = 0;
	runModeSong = SequencerKernel::MODE_FWD;
	storedLength = SequencerKernel::MAX_PHRASES;
}


//*****************************************************************************
// Sequencer
//*****************************************************************************


void Sequencer::construct(bool* _holdTiedNotesPtr, int* _velocityModePtr) {// don't want regaular constructor mechanism
	velocityModePtr = _velocityModePtr;
	sek[0].construct(0, nullptr, _holdTiedNotesPtr);
	for (int trkn = 1; trkn < NUM_TRACKS; trkn++)
		sek[trkn].construct(trkn, &sek[0], _holdTiedNotesPtr);
}


void Sequencer::setVelocityVal(int trkn, int intVel, int multiStepsCount, bool multiTracks) {
	sek[trkn].setVelocityVal(seqIndexEdit, stepIndexEdit, intVel, multiStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trkn) continue;
			sek[i].setVelocityVal(seqIndexEdit, stepIndexEdit, intVel, multiStepsCount);
		}
	}
}
void Sequencer::setLength(int length, bool multiTracks) {
	sek[trackIndexEdit].setLength(seqIndexEdit, length);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setLength(seqIndexEdit, length);
		}
	}
}
void Sequencer::setBegin(bool multiTracks) {
	sek[trackIndexEdit].setBegin(phraseIndexEdit);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setBegin(phraseIndexEdit);
		}
	}
}
void Sequencer::setEnd(bool multiTracks) {
	sek[trackIndexEdit].setEnd(phraseIndexEdit);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setEnd(phraseIndexEdit);
		}
	}
}
bool Sequencer::setGateType(int keyn, int multiSteps, bool autostepClick, bool multiTracks) {// Third param is for right-click autostep. Returns success
	int newMode = keyIndexToGateTypeEx(keyn);
	if (newMode == -1) 
		return false;
	sek[trackIndexEdit].setGateType(seqIndexEdit, stepIndexEdit, newMode, multiSteps);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setGateType(seqIndexEdit, stepIndexEdit, newMode, multiSteps);
		}
	}
	if (autostepClick) // if right-click then move to next step
		moveStepIndexEdit(1);
	return true;
}


void Sequencer::initSlideVal(int multiStepsCount, bool multiTracks) {
	sek[trackIndexEdit].setSlideVal(seqIndexEdit, stepIndexEdit, StepAttributes::INIT_SLIDE, multiStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setSlideVal(seqIndexEdit, stepIndexEdit, StepAttributes::INIT_SLIDE, multiStepsCount);
		}
	}		
}
void Sequencer::initGatePVal(int multiStepsCount, bool multiTracks) {
	sek[trackIndexEdit].setGatePVal(seqIndexEdit, stepIndexEdit, StepAttributes::INIT_PROB, multiStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setGatePVal(seqIndexEdit, stepIndexEdit, StepAttributes::INIT_PROB, multiStepsCount);
		}
	}		
}
void Sequencer::initVelocityVal(int multiStepsCount, bool multiTracks) {
	sek[trackIndexEdit].setVelocityVal(seqIndexEdit, stepIndexEdit, StepAttributes::INIT_VELOCITY, multiStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setVelocityVal(seqIndexEdit, stepIndexEdit, StepAttributes::INIT_VELOCITY, multiStepsCount);
		}
	}		
}
void Sequencer::initPulsesPerStep(bool multiTracks) {
	sek[trackIndexEdit].initPulsesPerStep();
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].initPulsesPerStep();
		}
	}		
}
void Sequencer::initDelay(bool multiTracks) {
	sek[trackIndexEdit].initDelay();
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].initDelay();
		}
	}		
}
void Sequencer::initRunModeSong(bool multiTracks) {
	sek[trackIndexEdit].setRunModeSong(SequencerKernel::MODE_FWD);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setRunModeSong(SequencerKernel::MODE_FWD);
		}
	}		
}
void Sequencer::initRunModeSeq(bool multiTracks) {
	sek[trackIndexEdit].setRunModeSeq(seqIndexEdit, SequencerKernel::MODE_FWD);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setRunModeSeq(seqIndexEdit, SequencerKernel::MODE_FWD);
		}
	}		
}
void Sequencer::initLength(bool multiTracks) {
	sek[trackIndexEdit].setLength(seqIndexEdit, SequencerKernel::MAX_STEPS);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setLength(seqIndexEdit, SequencerKernel::MAX_STEPS);
		}
	}		
}
void Sequencer::initPhraseReps(bool multiTracks) {
	sek[trackIndexEdit].setPhraseReps(phraseIndexEdit, 1);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseReps(phraseIndexEdit, 1);
		}
	}		
}
void Sequencer::initPhraseSeqNum(bool multiTracks) {
	sek[trackIndexEdit].setPhraseSeqNum(phraseIndexEdit, 0);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseSeqNum(phraseIndexEdit, 0);
		}
	}		
}

void Sequencer::copySequence(int countCP) {
	int startCP = stepIndexEdit;
	sek[trackIndexEdit].copySequence(&seqCPbuf, seqIndexEdit, startCP, countCP);
}
void Sequencer::pasteSequence(bool multiTracks) {
	int startCP = stepIndexEdit;
	sek[trackIndexEdit].pasteSequence(&seqCPbuf, seqIndexEdit, startCP);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].pasteSequence(&seqCPbuf, seqIndexEdit, startCP);
		}
	}
}
void Sequencer::copySong(int countCP) {
	sek[trackIndexEdit].copySong(&songCPbuf, phraseIndexEdit, countCP);
}
void Sequencer::pasteSong(bool multiTracks) {
	sek[trackIndexEdit].pasteSong(&songCPbuf, phraseIndexEdit);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].pasteSong(&songCPbuf, phraseIndexEdit);
		}
	}
}


void Sequencer::writeCV(int trkn, float cvVal, int multiStepsCount, float sampleRate, bool multiTracks) {
	sek[trkn].writeCV(seqIndexEdit, stepIndexEdit, cvVal, multiStepsCount);
	editingGateCV[trkn] = cvVal;
	editingGate[trkn] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trkn) continue;
			sek[i].writeCV(seqIndexEdit, stepIndexEdit, cvVal, multiStepsCount);
		}
	}
}
void Sequencer::autostep(bool autoseq) {
	moveStepIndexEdit(1);
	if (stepIndexEdit == 0 && autoseq)
		seqIndexEdit = moveIndex(seqIndexEdit, seqIndexEdit + 1, SequencerKernel::MAX_SEQS);	
}	

bool Sequencer::applyNewOctave(int octn, int multiSteps, float sampleRate, bool multiTracks) { // returns true if tied
	if (sek[trackIndexEdit].getTied(seqIndexEdit, stepIndexEdit))
		return true;
	editingGateCV[trackIndexEdit] = sek[trackIndexEdit].applyNewOctave(seqIndexEdit, stepIndexEdit, octn, multiSteps);
	editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
	editingGateKeyLight = -1;
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].applyNewOctave(seqIndexEdit, stepIndexEdit, octn, multiSteps);
		}
	}
	return false;
}
bool Sequencer::applyNewKey(int keyn, int multiSteps, float sampleRate, bool autostepClick, bool multiTracks) { // returns true if tied
	bool ret = false;
	if (sek[trackIndexEdit].getTied(seqIndexEdit, stepIndexEdit)) {
		if (autostepClick)
			moveStepIndexEdit(1);
		else
			ret = true;
	}
	else {
		editingGateCV[trackIndexEdit] = sek[trackIndexEdit].applyNewKey(seqIndexEdit, stepIndexEdit, keyn, multiSteps);
		editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
		editingGateKeyLight = -1;
		if (multiTracks) {
			for (int i = 0; i < NUM_TRACKS; i++) {
				if (i == trackIndexEdit) continue;
				sek[i].applyNewKey(seqIndexEdit, stepIndexEdit, keyn, multiSteps);
			}
		}
		if (autostepClick) {// if right-click then move to next step
			moveStepIndexEdit(1);
			editingGateKeyLight = keyn;
		}
	}
	return ret;
}

void Sequencer::moveStepIndexEditWithEditingGate(int delta, bool writeTrig, float sampleRate) {
	moveStepIndexEdit(delta);
	for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
		if (!sek[trkn].getTied(seqIndexEdit, stepIndexEdit)) {// play if non-tied step
			if (!writeTrig) {// in case autostep when simultaneous writeCV and stepCV (keep what was done in Write Input block above)
				editingGate[trkn] = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
				editingGateCV[trkn] = sek[trkn].getCV(seqIndexEdit, stepIndexEdit);
				editingGateKeyLight = -1;
			}
		}
	}
}



void Sequencer::modSlideVal(int deltaVelKnob, int mutliStepsCount, bool multiTracks) {
	int sVal = sek[trackIndexEdit].modSlideVal(seqIndexEdit, stepIndexEdit, deltaVelKnob, mutliStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setSlideVal(seqIndexEdit, stepIndexEdit, sVal, mutliStepsCount);
		}
	}		
}
void Sequencer::modGatePVal(int deltaVelKnob, int mutliStepsCount, bool multiTracks) {
	int gpVal = sek[trackIndexEdit].modGatePVal(seqIndexEdit, stepIndexEdit, deltaVelKnob, mutliStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setGatePVal(seqIndexEdit, stepIndexEdit, gpVal, mutliStepsCount);
		}
	}		
}
void Sequencer::modVelocityVal(int deltaVelKnob, int mutliStepsCount, bool multiTracks) {
	int upperLimit = ((*velocityModePtr) == 0 ? 200 : 127);
	int vVal = sek[trackIndexEdit].modVelocityVal(seqIndexEdit, stepIndexEdit, deltaVelKnob, upperLimit, mutliStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setVelocityVal(seqIndexEdit, stepIndexEdit, vVal, mutliStepsCount);
		}
	}		
}
void Sequencer::modRunModeSong(int deltaPhrKnob, bool multiTracks) {
	int newRunMode = sek[trackIndexEdit].modRunModeSong(deltaPhrKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setRunModeSong(newRunMode);
		}
	}		
}
void Sequencer::modPulsesPerStep(int deltaSeqKnob, bool multiTracks) {
	int newPPS = sek[trackIndexEdit].modPulsesPerStep(deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPulsesPerStep(newPPS);
		}
	}		
}
void Sequencer::modDelay(int deltaSeqKnob, bool multiTracks) {
	int newDelay = sek[trackIndexEdit].modDelay(deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setDelay(newDelay);
		}
	}		
}
void Sequencer::modRunModeSeq(int deltaSeqKnob, bool multiTracks) {
	int newRunMode = sek[trackIndexEdit].modRunModeSeq(seqIndexEdit, deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setRunModeSeq(seqIndexEdit, newRunMode);
		}
	}		
}
void Sequencer::modLength(int deltaSeqKnob, bool multiTracks) {
	int newLength = sek[trackIndexEdit].modLength(seqIndexEdit, deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setLength(seqIndexEdit, newLength);
		}
	}		
}
void Sequencer::modPhraseReps(int deltaSeqKnob, bool multiTracks) {
	int newReps = sek[trackIndexEdit].modPhraseReps(phraseIndexEdit, deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseReps(phraseIndexEdit, newReps);
		}
	}		
}
void Sequencer::modPhraseSeqNum(int deltaSeqKnob, bool multiTracks) {
	int newSeqn = sek[trackIndexEdit].modPhraseSeqNum(phraseIndexEdit, deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseSeqNum(phraseIndexEdit, newSeqn);
		}
	}		
}
void Sequencer::transposeSeq(int deltaSeqKnob, bool multiTracks) {
	sek[trackIndexEdit].transposeSeq(seqIndexEdit, deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].transposeSeq(seqIndexEdit, deltaSeqKnob);
		}
	}		
}
void Sequencer::unTransposeSeq(bool multiTracks) {
	sek[trackIndexEdit].unTransposeSeq(seqIndexEdit);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].unTransposeSeq(seqIndexEdit);
		}
	}		
}
void Sequencer::rotateSeq(int *rotateOffsetPtr, int deltaSeqKnob, bool multiTracks) {
	sek[trackIndexEdit].rotateSeq(rotateOffsetPtr, seqIndexEdit, deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			int rotateOffset = *rotateOffsetPtr;// dummy so not to affect the original pointer
			if (i == trackIndexEdit) continue;
			sek[i].rotateSeq(&rotateOffset, seqIndexEdit, deltaSeqKnob);
		}
	}		
}

void Sequencer::toggleGate(int multiSteps, bool multiTracks) {
	bool newGate = sek[trackIndexEdit].toggleGate(seqIndexEdit, stepIndexEdit, multiSteps);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;			
			sek[i].setGate(seqIndexEdit, stepIndexEdit, newGate, multiSteps);
		}
	}		
}
bool Sequencer::toggleGateP(int multiSteps, bool multiTracks) { // returns true if tied
	if (sek[trackIndexEdit].getTied(seqIndexEdit,stepIndexEdit))
		return true;
	bool newGateP = sek[trackIndexEdit].toggleGateP(seqIndexEdit, stepIndexEdit, multiSteps);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;			
			sek[i].setGateP(seqIndexEdit, stepIndexEdit, newGateP, multiSteps);
		}
	}				
	return false;
}
bool Sequencer::toggleSlide(int multiSteps, bool multiTracks) { // returns true if tied
	if (sek[trackIndexEdit].getTied(seqIndexEdit,stepIndexEdit))
		return true;
	bool newSlide = sek[trackIndexEdit].toggleSlide(seqIndexEdit, stepIndexEdit, multiSteps);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;			
			sek[i].setSlide(seqIndexEdit, stepIndexEdit, newSlide, multiSteps);
		}
	}				
	return false;
}
void Sequencer::toggleTied(int multiSteps, bool multiTracks) {
	bool newTied = sek[trackIndexEdit].toggleTied(seqIndexEdit, stepIndexEdit, multiSteps);// will clear other attribs if new state is on
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;			
			sek[i].setTied(seqIndexEdit, stepIndexEdit, newTied, multiSteps);
		}
	}						
}



void Sequencer::reset() {
	stepIndexEdit = 0;
	phraseIndexEdit = 0;
	seqIndexEdit = 0;
	trackIndexEdit = 0;
	seqCPbuf.reset();
	songCPbuf.reset();

	for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
		editingGate[trkn] = 0ul;
		sek[trkn].reset();
	}
}

void Sequencer::toJson(json_t *rootJ) {
	// stepIndexEdit
	json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));

	// seqIndexEdit
	json_object_set_new(rootJ, "seqIndexEdit", json_integer(seqIndexEdit));

	// phraseIndexEdit
	json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

	// trackIndexEdit
	json_object_set_new(rootJ, "trackIndexEdit", json_integer(trackIndexEdit));

	for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
		sek[trkn].toJson(rootJ);
}

void Sequencer::fromJson(json_t *rootJ) {
	// stepIndexEdit
	json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
	if (stepIndexEditJ)
		stepIndexEdit = json_integer_value(stepIndexEditJ);
	
	// phraseIndexEdit
	json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
	if (phraseIndexEditJ)
		phraseIndexEdit = json_integer_value(phraseIndexEditJ);
	
	// seqIndexEdit
	json_t *seqIndexEditJ = json_object_get(rootJ, "seqIndexEdit");
	if (seqIndexEditJ)
		seqIndexEdit = json_integer_value(seqIndexEditJ);
	
	// trackIndexEdit
	json_t *trackIndexEditJ = json_object_get(rootJ, "trackIndexEdit");
	if (trackIndexEditJ)
		trackIndexEdit = json_integer_value(trackIndexEditJ);
	
	for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
		sek[trkn].fromJson(rootJ);
}


