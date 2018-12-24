//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "dsp/digital.hpp"
#include "PhraseSeq32ExUtil.hpp"

using namespace rack;



//*****************************************************************************
// SequencerKernel
//*****************************************************************************


const std::string SequencerKernel::modeLabels[NUM_MODES] = {"FWD", "REV", "PPG", "PEN", "BRN", "RND", "ARN"};


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


void SequencerKernel::construct(int _id, uint32_t* seqPtr, uint32_t* songPtr, bool* _holdTiedNotesPtr) {// don't want regaular constructor mechanism
	id = _id;
	ids = "id" + std::to_string(id) + "_";
	
	slaveSeqRndLast = seqPtr; 
	slaveSongRndLast = songPtr;
	
	holdTiedNotesPtr = _holdTiedNotesPtr;
}


void SequencerKernel::toggleGate(int seqn, int stepn, int count) {
	attributes[seqn][stepn].toggleGate();
	bool newGate = attributes[seqn][stepn].getGate();
	int starti = (count == MAX_STEPS ? 0 : (stepn + 1));
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = starti; i < endi; i++)
		attributes[seqn][i].setGate(newGate);
}
void SequencerKernel::toggleGateP(int seqn, int stepn, int count) {
	attributes[seqn][stepn].toggleGateP();
	bool newGateP = attributes[seqn][stepn].getGateP();
	int starti = (count == MAX_STEPS ? 0 : (stepn + 1));
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = starti; i < endi; i++)
		attributes[seqn][i].setGateP(newGateP);
}
void SequencerKernel::toggleSlide(int seqn, int stepn, int count) {
	attributes[seqn][stepn].toggleSlide();
	bool newSlide = attributes[seqn][stepn].getSlide();
	int starti = (count == MAX_STEPS ? 0 : (stepn + 1));
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = starti; i < endi; i++)
		attributes[seqn][i].setSlide(newSlide);
}	
void SequencerKernel::toggleTied(int seqn, int stepn, int count) {
	int starti = (count == MAX_STEPS ? 0 : (stepn + 1));
	int endi = min(MAX_STEPS, stepn + count);
	if (attributes[seqn][stepn].getTied()) {
		deactivateTiedStep(seqn, stepn);
		for (int i = starti; i < endi; i++)
			deactivateTiedStep(seqn, i);
	}
	else {
		activateTiedStep(seqn, stepn);
		for (int i = starti; i < endi; i++)
			activateTiedStep(seqn, i);
	}
}
float SequencerKernel::applyNewOctave(int seqn, int stepn, int newOct, int count) {// does not overwrite tied steps
	float newCV = cv[seqn][stepn] + 10.0f;//to properly handle negative note voltages
	newCV = newCV - floor(newCV) + (float) (newOct - 3);
	
	int starti = (count == MAX_STEPS ? 0 : stepn);
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = starti; i < endi; i++) {
		if (!attributes[seqn][i].getTied()) {
			cv[seqn][i] = newCV;
			propagateCVtoTied(seqn, i);		
		}
	}
	
	return newCV;
}
float SequencerKernel::applyNewKey(int seqn, int stepn, int newKeyIndex, int count) {// does not overwrite tied steps
	float newCV = floor(cv[seqn][stepn]) + ((float) newKeyIndex) / 12.0f;
	
	int starti = (count == MAX_STEPS ? 0 : stepn);
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = starti; i < endi; i++) {
		if (!attributes[seqn][i].getTied()) {
			cv[seqn][i] = newCV;
			propagateCVtoTied(seqn, i);		
		}
	}
	
	return newCV;
}
float SequencerKernel::writeCV(int seqn, int stepn, float newCV, int count) {// does not overwrite tied steps
	int starti = (count == MAX_STEPS ? 0 : stepn);
	int endi = min(MAX_STEPS, stepn + count);
	for (int i = starti; i < endi; i++) {
		if (!attributes[seqn][i].getTied()) {
			cv[seqn][i] = newCV;
			propagateCVtoTied(seqn, i);
		}
	}
	return newCV;
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


void SequencerKernel::copySequence(float* cvCPbuffer, StepAttributes* attribCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int seqn, int startCP, int countCP) {
	for (int i = 0, stepn = startCP; i < countCP; i++, stepn++) {
		cvCPbuffer[i] = cv[seqn][stepn];
		attribCPbuffer[i] = attributes[seqn][stepn];
	}
	*seqPhraseAttribCPbuffer = sequences[seqn];
	seqPhraseAttribCPbuffer->setTranspose(-1);// so that a cross paste can be detected
}
void SequencerKernel::pasteSequence(float* cvCPbuffer, StepAttributes* attribCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int seqn, int startCP, int countCP) {
	for (int i = 0, stepn = startCP; i < countCP; i++, stepn++) {
		cv[seqn][stepn] = cvCPbuffer[i];
		attributes[seqn][stepn] = attribCPbuffer[i];
	}
	if (countCP == MAX_STEPS) {// all
		sequences[seqn] = *seqPhraseAttribCPbuffer;
		sequences[seqn].setTranspose(0);
	}
}
void SequencerKernel::copyPhrase(Phrase* phraseCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int startCP, int countCP) {	
	for (int i = 0, phrn = startCP; i < countCP; i++, phrn++) {
		phraseCPbuffer[i] = phrases[phrn];
	}
	seqPhraseAttribCPbuffer->setLength(songBeginIndex);
	seqPhraseAttribCPbuffer->setTranspose(songEndIndex);
	seqPhraseAttribCPbuffer->setRunMode(runModeSong);
}
void SequencerKernel::pastePhrase(Phrase* phraseCPbuffer, SeqAttributes* seqPhraseAttribCPbuffer, int startCP, int countCP) {	
	for (int i = 0, phrn = startCP; i < countCP; i++, phrn++) {
		phrases[phrn] = phraseCPbuffer[i];
	}
	if (countCP == MAX_PHRASES) {// all
		songBeginIndex = seqPhraseAttribCPbuffer->getLength();
		songEndIndex = seqPhraseAttribCPbuffer->getTranspose();
		runModeSong = seqPhraseAttribCPbuffer->getRunMode();
	}
}


void SequencerKernel::reset() {
	pulsesPerStep = 1;
	delay = 0;
	initSong();
	for (int seqn = 0; seqn < MAX_SEQS; seqn++) {
		initSequence(seqn);		
	}
	slideStepsRemain = 0ul;
	// no need to call initRun() here since user of the kernel does it in its onReset() via its initRun()
}


void SequencerKernel::randomize() {
	randomizeSong();
	for (int seqn = 0; seqn < MAX_SEQS; seqn++) {
		randomizeSequence(seqn);
	}
	// no need to call initRun() here since user of the kernel does it in its onRandomize() via its initRun()
}
	

void SequencerKernel::initRun() {
	phraseIndexRun = (runModeSong == MODE_REV ? songEndIndex : songBeginIndex);
	phraseIndexRunHistory = 0;

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


void SequencerKernel::clockStep(unsigned long clockPeriod) {
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
			if (moveIndexRunMode(true)) {// true means seq
				moveIndexRunMode(false); // false means song
				SeqAttributes newSeq = sequences[phrases[phraseIndexRun].getSeqNum()];
				stepIndexRun = (newSeq.getRunMode() == MODE_REV ? newSeq.getLength() - 1 : 0);// must always refresh after phraseIndexRun has changed
			}

			// Slide
			StepAttributes attribRun = getAttributeRun();
			if (attribRun.getSlide()) {
				slideStepsRemain = (unsigned long) (((float)clockPeriod * ppsFiltered) * ((float)attribRun.getSlideVal() / 100.0f));
				float slideToCV = getCVRun();
				slideCVdelta = (slideToCV - slideFromCV)/(float)slideStepsRemain;
			}
		}
		calcGateCodeEx(phrases[phraseIndexRun].getSeqNum());// uses stepIndexRun as the step		
	}
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
	

bool SequencerKernel::moveIndexRunMode(bool moveSequence) {	
	// assert((reps * MAX_STEPS) <= 0xFFF); // for BRN and RND run modes, history is not a span count but a step count
	
	int* index;
	unsigned long* history;
	int reps;
	int runMode;
	int endStep;
	int startStep;
	
	if (moveSequence) {
		index = &stepIndexRun;
		history = &stepIndexRunHistory;
		reps = phrases[phraseIndexRun].getReps();
		runMode = sequences[phrases[phraseIndexRun].getSeqNum()].getRunMode();
		endStep = sequences[phrases[phraseIndexRun].getSeqNum()].getLength() - 1;
		startStep = 0;
	}
	else {// move song
		index = &phraseIndexRun;
		history = &phraseIndexRunHistory;
		reps = 1;// 1 count is enough in song, since the return boundaryCross boolean is ignored (it will loop the song continually)
		runMode = runModeSong;
		endStep = songEndIndex;
		startStep = songBeginIndex;
	}
	
	bool crossBoundary = false;
	
	switch (runMode) {
	
		// history 0x0000 is reserved for reset
		
		case MODE_REV :// reverse; history base is 0x2000
			if ((*history) < 0x2001 || (*history) > 0x2FFF)
				(*history) = 0x2000 + reps;
			(*index)--;
			if ((*index) < startStep) {
				(*index) = endStep;
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
				if ((*index) > endStep) {
					(*index) = endStep;
					(*history)--;
				}
			}
			else {// odd so reverse phase
				(*index)--;
				if ((*index) < startStep) {
					(*index) = startStep;
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
				if ((*index) > endStep) {
					(*index) = endStep - 1;
					(*history)--;
					if ((*index) <= startStep) {// if back at start after turnaround, then no reverse phase needed
						(*index) = startStep;
						(*history)--;
						if ((*history) <= 0x4000)
							crossBoundary = true;
					}
				}
			}
			else {// odd so reverse phase
				(*index)--;
				if ((*index) <= startStep) {
					(*index) = startStep;
					(*history)--;
					if ((*history) <= 0x4000)
						crossBoundary = true;
				}
			}
		break;
		
		case MODE_BRN :// brownian random; history base is 0x5000
			if ((*history) < 0x5001 || (*history) > 0x5FFF) 
				(*history) = 0x5000 + (endStep - startStep + 1) * reps;
			(*index) += (randomu32() % 3) - 1;
			if ((*index) > endStep) {
				(*index) = startStep;
			}
			if ((*index) < startStep) {
				(*index) = endStep;
			}
			(*history)--;
			if ((*history) <= 0x5000) {
				crossBoundary = true;
			}
		break;
		
		case MODE_RND :// random; history base is 0x6000
		case MODE_ARN :// use track A's random number for seq/song depending on moveSequence bool value
			if ((*history) < 0x6001 || (*history) > 0x6FFF) {
				(*history) = 0x6000 + (endStep - startStep + 1) * reps;
			}

			{
				uint32_t randomToUseHere = 0ul;
				if (runMode == MODE_ARN && slaveSeqRndLast != nullptr) {// must test both since MODE_ARN allowed for track 0 (defaults to RND)
					if (moveSequence)
						randomToUseHere = *slaveSeqRndLast;
					else
						randomToUseHere = *slaveSongRndLast;
				}
				else {
					randomToUseHere = randomu32();
					if (moveSequence)
						seqRndLast = randomToUseHere;
					else
						songRndLast = randomToUseHere;
				}
				
				(*index) = startStep + (randomToUseHere % (endStep - startStep + 1)) ;
			}
			(*history)--;
			if ((*history) <= 0x6000) {
				crossBoundary = true;
			}
		break;
		
		default :// MODE_FWD  forward; history base is 0x1000
			if ((*history) < 0x1001 || (*history) > 0x1FFF)
				(*history) = 0x1000 + reps;
			(*index)++;
			if ((*index) > endStep) {
				(*index) = startStep;
				(*history)--;
				if ((*history) <= 0x1000)
					crossBoundary = true;
			}
	}

	return crossBoundary;
}

