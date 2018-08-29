//***********************************************************************************************
//MidiFile module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental, Core and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//Also based on Midifile, a C++ MIDI file parsing library by Craig Stuart Sapp
//  https://github.com/craigsapp/midifile
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept by Marc Boulé
//***********************************************************************************************


/* temporary notes
https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message
https://www.midi.org/specifications-old/item/table-3-control-change-messages-data-bytes-2

Dekstop (callback mechanism and file opening):
https://github.com/dekstop/vcvrackplugins_dekstop/blob/master/src/Recorder.cpp

VCVRack-Simple (file opening):
https://github.com/IohannRabeson/VCVRack-Simple/commit/2d33e97d2e344d2926548a0b9f11f1c15ee4ca3c

*/


#include "ImpromptuModular.hpp"
#include "midifile/MidiFile.h"
#include "osdialog.h"
#include <iostream>
#include <algorithm>


using namespace std;
using namespace smf;


//*****************************************************************************


struct MidiFileModule : Module {
	enum ParamIds {
		LOADMIDI_PARAM,
		RESET_PARAM,
		RUN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLK_INPUT,
		RESET_INPUT,
		RUNCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),
		ENUMS(GATE_OUTPUTS, 4),
		ENUMS(VELOCITY_OUTPUTS, 4),
		ENUMS(AFTERTOUCH_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		RESET_LIGHT,
		RUN_LIGHT,
		ENUMS(LOADMIDI_LIGHT, 2),
		NUM_LIGHTS
	};
	

	struct NoteData {
		uint8_t velocity = 0;
		uint8_t aftertouch = 0;
	};
	
	// Constants
	enum PolyMode {ROTATE_MODE, REUSE_MODE, RESET_MODE, REASSIGN_MODE, UNISON_MODE, NUM_MODES};
	
	// Need to save, with reset
	// none
	
	// Need to save, no reset
	int panelTheme;
	string lastPath;
	PolyMode polyMode;// From QuadMIDIToCVInterface.cpp
	
	// No need to save, with reset
	bool running;
	double time;
	long event;
	//
	//--- START From QuadMIDIToCVInterface.cpp
	NoteData noteData[128];
	// cachedNotes : UNISON_MODE and REASSIGN_MODE cache all played notes. The other polyModes cache stolen notes (after the 4th one).
	std::vector<uint8_t> cachedNotes;
	uint8_t notes[4];
	bool gates[4];
	// gates set to TRUE by pedal and current gate. FALSE by pedal.
	bool pedalgates[4];
	bool pedal;
	int rotateIndex;
	int stealIndex;
	//--- END From QuadMIDIToCVInterface.cpp
	
	
	// No need to save, no reset
	MidiFile midifile;
	bool fileLoaded;
	SchmittTrigger runningTrigger;
	SchmittTrigger resetTrigger;
	float resetLight;
	
	
	MidiFileModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS), cachedNotes(128) {
		// Need to save, no reset
		panelTheme = 0;
		lastPath = "";
		polyMode = RESET_MODE;
		
		// No need to save, no reset
		fileLoaded = false;
		runningTrigger.reset();
		resetTrigger.reset();
		resetLight = 0.0f;
		
		onReset();
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		// Need to save, with reset
		// none
		
		// No need to save, with reset
		running = false;
		time = 0.0;
		event = 0;
		//
		cachedNotes.clear();
		for (int i = 0; i < 4; i++) {
			notes[i] = 60;
			gates[i] = false;
			pedalgates[i] = false;
		}
		pedal = false;
		rotateIndex = -1;
		stealIndex = 0;
	}
	
	// widgets randomized before onRandomize() is called
	void onRandomize() override {

	}


	json_t *toJson() override {
		json_t *rootJ = json_object();
		// TODO // Need to save (reset or not)
		return rootJ;
	}

	
	// widgets loaded before this fromJson() is called
	void fromJson(json_t *rootJ) override {
		// TODO // Need to save (reset or not)

		
		// No need to save, with reset
		// none		
	}
	
	
	//------------ START QuadMIDIToCVInterface.cpp (with slight modifications) ------------------
	
	int getPolyIndex(int nowIndex) {
		for (int i = 0; i < 4; i++) {
			nowIndex++;
			if (nowIndex > 3)
				nowIndex = 0;
			if (!(gates[nowIndex] || pedalgates[nowIndex])) {
				stealIndex = nowIndex;
				return nowIndex;
			}
		}
		// All taken = steal (stealIndex always rotates)
		stealIndex++;
		if (stealIndex > 3)
			stealIndex = 0;
		if ((polyMode < REASSIGN_MODE) && (gates[stealIndex]))
			cachedNotes.push_back(notes[stealIndex]);
		return stealIndex;
	}

	void pressNote(uint8_t note) {
		// Set notes and gates
		switch (polyMode) {
			case ROTATE_MODE: {
				rotateIndex = getPolyIndex(rotateIndex);
			} break;

			case REUSE_MODE: {
				bool reuse = false;
				for (int i = 0; i < 4; i++) {
					if (notes[i] == note) {
						rotateIndex = i;
						reuse = true;
						break;
					}
				}
				if (!reuse)
					rotateIndex = getPolyIndex(rotateIndex);
			} break;

			case RESET_MODE: {
				rotateIndex = getPolyIndex(-1);
			} break;

			case REASSIGN_MODE: {
				cachedNotes.push_back(note);
				rotateIndex = getPolyIndex(-1);
			} break;

			case UNISON_MODE: {
				cachedNotes.push_back(note);
				for (int i = 0; i < 4; i++) {
					notes[i] = note;
					gates[i] = true;
					pedalgates[i] = pedal;
					// reTrigger[i].trigger(1e-3);
				}
				return;
			} break;

			default: break;
		}
		// Set notes and gates
		// if (gates[rotateIndex] || pedalgates[rotateIndex])
		// 	reTrigger[rotateIndex].trigger(1e-3);
		notes[rotateIndex] = note;
		gates[rotateIndex] = true;
		pedalgates[rotateIndex] = pedal;
	}

	void releaseNote(uint8_t note) {
		// Remove the note
		auto it = find(cachedNotes.begin(), cachedNotes.end(), note);
		if (it != cachedNotes.end())
			cachedNotes.erase(it);

		switch (polyMode) {
			case REASSIGN_MODE: {
				for (int i = 0; i < 4; i++) {
					if (i < (int) cachedNotes.size()) {
						if (!pedalgates[i])
							notes[i] = cachedNotes[i];
						pedalgates[i] = pedal;
					}
					else {
						gates[i] = false;
					}
				}
			} break;

			case UNISON_MODE: {
				if (!cachedNotes.empty()) {
					uint8_t backnote = cachedNotes.back();
					for (int i = 0; i < 4; i++) {
						notes[i] = backnote;
						gates[i] = true;
					}
				}
				else {
					for (int i = 0; i < 4; i++) {
						gates[i] = false;
					}
				}
			} break;

			// default ROTATE_MODE REUSE_MODE RESET_MODE
			default: {
				for (int i = 0; i < 4; i++) {
					if (notes[i] == note) {
						if (pedalgates[i]) {
							gates[i] = false;
						}
						else if (!cachedNotes.empty()) {
							notes[i] = cachedNotes.back();
							cachedNotes.pop_back();
						}
						else {
							gates[i] = false;
						}
					}
				}
			} break;
		}
	}

	void pressPedal() {
		pedal = true;
		for (int i = 0; i < 4; i++) {
			pedalgates[i] = gates[i];
		}
	}

	void releasePedal() {
		pedal = false;
		// When pedal is off, recover notes for pressed keys (if any) after they were already being "cycled" out by pedal-sustained notes.
		for (int i = 0; i < 4; i++) {
			pedalgates[i] = false;
			if (!cachedNotes.empty()) {
				if (polyMode < REASSIGN_MODE) {
					notes[i] = cachedNotes.back();
					cachedNotes.pop_back();
					gates[i] = true;
				}
			}
		}
		if (polyMode == REASSIGN_MODE) {
			for (int i = 0; i < 4; i++) {
				if (i < (int) cachedNotes.size()) {
					notes[i] = cachedNotes[i];
					gates[i] = true;
				}
				else {
					gates[i] = false;
				}
			}
		}
	}	
	
	void processMessage(MidiMessage *msg) {
		switch (msg->getCommandByte() >> 4) {//status()
			// note off
			case 0x8: {
				releaseNote(msg->getKeyNumber());//note()
			} break;
			// note on
			case 0x9: {
				if (msg->getVelocity() > 0) {//value()
					noteData[msg->getKeyNumber()].velocity = msg->getVelocity();//note(),  value()
					pressNote(msg->getKeyNumber());//note()
				}
				else {
					releaseNote(msg->getKeyNumber());//note()
				}
			} break;
			// channel aftertouch
			case 0xa: {
				noteData[msg->getKeyNumber()].aftertouch = msg->getP2();//note(),  value()
			} break;
			// cc
			case 0xb: {
				processCC(msg);
			} break;
			default: break;
		}
	}

	void processCC(MidiMessage *msg) {
		switch (msg->getControllerNumber()) {//note()
			// sustain
			case 0x40: {
				if (msg->getControllerValue() >= 64)//value()
					pressPedal();
				else
					releasePedal();
			} break;
			default: break;
		}
	}
	
	//------------ END QuadMIDIToCVInterface.cpp ------------------
	
	

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		double sampleTime = engineGetSampleTime();
		
		
		
		//********** Buttons, knobs, switches and inputs **********

		// Run state button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUNCV_INPUT].value)) {
			running = !running;
		}		
		

		
		//********** Clock and reset **********
		
		int track = 0;// midifile was flattened when loaded
		double readTime = 0.0;
		if (running) {
			for (int ii = 0; ii < 100; ii++) {// assumes max N events at the same time
				if (event >= midifile[track].size()) {
					running = false;// TODO implement loop switch to optionally loop the song
					event = 0;
					time = 0.0;
					break;
				}
				
				readTime = midifile[track][event].seconds;
				if (readTime > time)
					break;

				processMessage(&midifile[track][event]);
				event++;
			}	
			time += sampleTime;
		}
		
		
		// Reset
		if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
			//clockTrigger.reset();
			//clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
			resetLight = 1.0f;
			time = 0.0;
			event = 0;
		}				
		
		
		
		//********** Outputs and lights **********
		
		for (int i = 0; i < 4; i++) {
			uint8_t lastNote = notes[i];
			uint8_t lastGate = (gates[i] || pedalgates[i]);
			outputs[CV_OUTPUTS + i].value = (lastNote - 60) / 12.f;
			outputs[GATE_OUTPUTS + i].value = (lastGate && running) ? 10.f : 0.f;
			outputs[VELOCITY_OUTPUTS + i].value = rescale(noteData[lastNote].velocity, 0, 127, 0.f, 10.f);
			outputs[AFTERTOUCH_OUTPUTS + i].value = rescale(noteData[lastNote].aftertouch, 0, 127, 0.f, 10.f);
		}		
		
		
		// fileLoaded light
		lights[LOADMIDI_LIGHT + 0].value = fileLoaded ? 1.0f : 0.0f;
		lights[LOADMIDI_LIGHT + 1].value = !fileLoaded ? 1.0f : 0.0f;
		
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	
		resetLight -= (resetLight / lightLambda) * sampleTime;// * displayRefreshStepSkips;

		// Run light
		lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;

	}// step()	
	
	
	void loadMidiFile() {
		
		osdialog_filters *filters = osdialog_filters_parse("Midi File (.mid):mid;Text File (.txt):txt");
		string dir = lastPath.empty() ? assetLocal("") : stringDirectory(lastPath);
		char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
		if (path) {
			lastPath = path;
			//lastFilename = stringFilename(path);
			if (midifile.read(path)) {
				fileLoaded = true;
				midifile.doTimeAnalysis();
				midifile.linkNotePairs();
				midifile.joinTracks();
				
				int tracks = midifile.getTrackCount();
				cout << "TPQ: " << midifile.getTicksPerQuarterNote() << endl;
				if (tracks > 1) cout << "TRACKS: " << tracks << endl;
				for (int track=0; track<tracks; track++) {
					if (tracks > 1) cout << "\nTrack " << track << endl;
					cout << "Tick\tSeconds\tDur\tMessage" << endl;
					for (int eventIndex=0; eventIndex < midifile[track].size(); eventIndex++) {
						cout << dec << midifile[track][eventIndex].tick;
						cout << '\t' << dec << midifile[track][eventIndex].seconds;
						cout << '\t';
						if (midifile[track][eventIndex].isNoteOn())
							cout << midifile[track][eventIndex].getDurationInSeconds();
						cout << '\t' << hex;
						for (unsigned int i=0; i<midifile[track][eventIndex].size(); i++)
							cout << (int)midifile[track][eventIndex][i] << ' ';
						cout << endl;
					}
				}
				cout << "event count: " << dec << midifile[0].size() << endl;
			}
			else
				fileLoaded = false;
			free(path);
			running = false;
			time = 0.0;
			event = 0;
		}	
		osdialog_filters_free(filters);
	}
	
};// MidiFileModule : module

struct MidiFileWidget : ModuleWidget {
	
	struct LoadMidiPushButton : IMBigPushButton {
		MidiFileModule *moduleL = nullptr;
		void onChange(EventChange &e) override {
			if (value > 0.0 && moduleL != nullptr) {
				moduleL->loadMidiFile();
			}
			IMBigPushButton::onChange(e);
		}
	};	


	MidiFileWidget(MidiFileModule *module) : ModuleWidget(module) {		
		// Main panel from Inkscape
        DynamicSVGPanel* panel = new DynamicSVGPanel();
        panel->mode = &module->panelTheme;
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/MidiFile.svg")));
        //panel->addPanel(SVG::load(assetPlugin(plugin, "res/dark/MidiFile_dark.svg")));
        box.size = panel->box.size;
        addChild(panel);		
		
		// Screws
		addChild(createDynamicScrew<IMScrew>(Vec(15, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(15, 365), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 365), &module->panelTheme));
		
		
		static const int colRulerM0 = 30;
		static const int colRulerMSpacing = 60;
		static const int colRulerM1 = colRulerM0 + colRulerMSpacing;
		static const int colRulerM2 = colRulerM1 + colRulerMSpacing;
		static const int rowRulerM0 = 180;
		
		
		
		// main load button
		LoadMidiPushButton* midiButton = createDynamicParamCentered<LoadMidiPushButton>(Vec(colRulerM0, rowRulerM0), module, MidiFileModule::LOADMIDI_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme);
		midiButton->moduleL = module;
		addParam(midiButton);
		// load light
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(colRulerM0 + 20, rowRulerM0), module, MidiFileModule::LOADMIDI_LIGHT + 0));
		
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerM1, rowRulerM0), module, MidiFileModule::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerM1, rowRulerM0), module, MidiFileModule::RESET_LIGHT));
		
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerM2, rowRulerM0), module, MidiFileModule::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerM2, rowRulerM0), module, MidiFileModule::RUN_LIGHT));
		
		
		
		// channel outputs (CV, GATE, VELOCITY)
		static const int colRulerOuts0 = 55;
		static const int colRulerOutsSpacing = 30;
		static const int rowRulerOuts0 = 250;
		static const int rowRulerOutsSpacing = 30;
		for (int i = 0; i < 4; i++) {
			addOutput(createDynamicPort<IMPort>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0), Port::OUTPUT, module, MidiFileModule::CV_OUTPUTS + i, &module->panelTheme));
			addOutput(createDynamicPort<IMPort>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0 + rowRulerOutsSpacing), Port::OUTPUT, module, MidiFileModule::GATE_OUTPUTS + i, &module->panelTheme));
			addOutput(createDynamicPort<IMPort>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0 + rowRulerOutsSpacing * 2), Port::OUTPUT, module, MidiFileModule::VELOCITY_OUTPUTS + i, &module->panelTheme));
		}
		
	}
};


Model *modelMidiFile = Model::create<MidiFileModule, MidiFileWidget>("Impromptu Modular", "Midi-File", "UTIL - Midi-File", MIDI_TAG);

/*CHANGE LOG

0.6.10:
created 

*/