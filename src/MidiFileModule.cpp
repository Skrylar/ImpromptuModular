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

//http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html

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
		LOOP_PARAM,
		CHANNEL_PARAM,// 0 means all channels, 1 to 16 for filter channel
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
	
	// Need to save
	int panelTheme = 0;
	PolyMode polyMode = RESET_MODE;// From QuadMIDIToCVInterface.cpp
	string lastPath = "";
	string lastFilename = "--";
	
	
	// No need to save
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
	
	
	unsigned int lightRefreshCounter = 0;	
	float resetLight = 0.0f;
	bool fileLoaded = false;
	MidiFile midifile;
	SchmittTrigger runningTrigger;
	SchmittTrigger resetTrigger;
	
	
	inline int getChannelKnob() {return (int)(params[CHANNEL_PARAM].value + 0.5f);}
	
	
	MidiFileModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS), cachedNotes(128) {
		onReset();
	}

	
	void onReset() override {
		running = false;
		resetPlayer();
	}

	
	void onRandomize() override {

	}

	
	void resetPlayer() {
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
	
	
	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
		
		// polyMode
		json_object_set_new(rootJ, "polyMode", json_integer(polyMode));

		// lastPath
		json_object_set_new(rootJ, "lastPath", json_string(lastPath.c_str()));
		
		return rootJ;
	}

	
	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// polyMode
		json_t *polyModeJ = json_object_get(rootJ, "polyMode");
		if (polyModeJ)
			polyMode = (PolyMode) json_integer_value(polyModeJ);
	
		// lastPath
		json_t *lastPathJ = json_object_get(rootJ, "lastPath");
		if (lastPathJ)
			lastPath = json_string_value(lastPathJ);
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
			// channel aftertouch (0xa is key-aftertouch, 0xd is channel-aftertouch)
			case 0xd: {
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
		
		// "Clock"
		const int track = 0;// midifile was flattened when loaded
		double readTime = 0.0;
		if (running) {
			for (int ii = 0; ii < 200; ii++) {// assumes max N events at the same time
				if (event >= midifile[track].size()) {
					if (params[LOOP_PARAM].value < 0.5f)
						running = false;
					resetPlayer();
					break;
				}
				
				readTime = midifile[track][event].seconds;
				if (readTime > time) {
					time += sampleTime;
					break;
				}

				int channel = getChannelKnob() - 1;// getChannelKnob is 1 indexed
				if (channel < 0 || channel == midifile[track][event].getChannelNibble())
					processMessage(&midifile[track][event]);
				event++;
			}	
			
		}
		
		
		// Reset
		if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
			resetLight = 1.0f;
			resetPlayer();
		}				
		
		
		
		//********** Outputs and lights **********
		
		for (int i = 0; i < 4; i++) {
			uint8_t lastNote = notes[i];
			uint8_t lastGate = (gates[i] || pedalgates[i]);
			outputs[CV_OUTPUTS + i].value = (lastNote - 60) / 12.f;
			outputs[GATE_OUTPUTS + i].value = (lastGate && running) ? 10.f : 0.f;
			outputs[VELOCITY_OUTPUTS + i].value = rescale(noteData[lastNote].velocity, 0, 127, 0.f, 10.f);
			//outputs[AFTERTOUCH_OUTPUTS + i].value = rescale(noteData[lastNote].aftertouch, 0, 127, 0.f, 10.f);
		}		
		
		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;
			
			// fileLoaded light
			lights[LOADMIDI_LIGHT + 0].value = fileLoaded ? 1.0f : 0.0f;
			lights[LOADMIDI_LIGHT + 1].value = !fileLoaded ? 1.0f : 0.0f;
			
			// Reset light
			lights[RESET_LIGHT].value =	resetLight;	
			resetLight -= (resetLight / lightLambda) * sampleTime * displayRefreshStepSkips;

			// Run light
			lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;
		}// lightRefreshCounter

	}// step()	
	
	
	void loadMidiFile() {
		
		osdialog_filters *filters = osdialog_filters_parse("Midi File (.mid):mid;Text File (.txt):txt");
		string dir = lastPath.empty() ? assetLocal("") : stringDirectory(lastPath);
		char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
		if (path) {
			lastPath = path;
			lastFilename = stringFilename(path);
			if (midifile.read(path)) {
				fileLoaded = true;
				midifile.doTimeAnalysis();
				midifile.linkNotePairs();
				midifile.joinTracks();
				
				// int tracks = midifile.getTrackCount();
				// cout << "TPQ: " << midifile.getTicksPerQuarterNote() << endl;
				// if (tracks > 1) cout << "TRACKS: " << tracks << endl;
				// for (int track=0; track<tracks; track++) {
					// if (tracks > 1) cout << "\nTrack " << track << endl;
					// cout << "Tick\tSeconds\tDur\tMessage" << endl;
					// for (int eventIndex=0; eventIndex < midifile[track].size(); eventIndex++) {
						// cout << dec << midifile[track][eventIndex].tick;
						// cout << '\t' << dec << midifile[track][eventIndex].seconds;
						// cout << '\t';
						// if (midifile[track][eventIndex].isNoteOn())
							// cout << midifile[track][eventIndex].getDurationInSeconds();
						// cout << '\t' << hex;
						// for (unsigned int i=0; i<midifile[track][eventIndex].size(); i++)
							// cout << (int)midifile[track][eventIndex][i] << ' ';
						// cout << endl;
					// }
				// }
				// cout << "event count: " << dec << midifile[0].size() << endl;
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
	

	struct MainDisplayWidget : TransparentWidget {
		MidiFileModule *module;
		std::shared_ptr<Font> font;
		static const int displaySize = 12;
		char text[displaySize + 1];

		MainDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		string removeExtension(const string& filename) {
			size_t lastdot = filename.find_last_of(".");
			if (lastdot == string::npos) return filename;
			return filename.substr(0, lastdot); 
		}

		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box, 12);
			nvgFontFaceId(vg, font->handle);
			nvgTextLetterSpacing(vg, -0.5);

			Vec textPos = Vec(5, 18);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			string empty = std::string(displaySize, '~');
			nvgText(vg, textPos.x, textPos.y, empty.c_str(), NULL);
			nvgFillColor(vg, textColor);
			for (int i = 0; i <= displaySize; i++)
				text[i] = ' ';
			snprintf(text, displaySize + 1, "%s", (removeExtension(module->lastFilename)).c_str());
			nvgText(vg, textPos.x, textPos.y, text, NULL);
		}
	};

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
		static const int colRulerM3 = colRulerM2 + colRulerMSpacing;
		static const int rowRulerT5 = 100;
		static const int rowRulerM0 = 180;
		
		
		// Main display
		MainDisplayWidget *displayMain = new MainDisplayWidget();
		displayMain->box.pos = Vec(12, 46);
		displayMain->box.size = Vec(137, 23.5f);// x characters
		displayMain->module = module;
		addChild(displayMain);
		
		// Channel knobs
		addParam(createDynamicParamCentered<IMBigSnapKnob>(Vec(colRulerM2+offsetIMBigKnob, rowRulerT5+offsetIMBigKnob), module, MidiFileModule::CHANNEL_PARAM, 0.0f, 16.0f, 0.0f, &module->panelTheme));		
		
		
		// Main load button
		LoadMidiPushButton* midiButton = createDynamicParamCentered<LoadMidiPushButton>(Vec(colRulerM0, rowRulerM0), module, MidiFileModule::LOADMIDI_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme);
		midiButton->moduleL = module;
		addParam(midiButton);
		// Load light
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(colRulerM0 + 20, rowRulerM0), module, MidiFileModule::LOADMIDI_LIGHT + 0));
		
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerM1, rowRulerM0), module, MidiFileModule::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerM1, rowRulerM0), module, MidiFileModule::RESET_LIGHT));
		
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerM2, rowRulerM0), module, MidiFileModule::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerM2, rowRulerM0), module, MidiFileModule::RUN_LIGHT));
		
		// Loop
		addParam(createParamCentered<CKSS>(Vec(colRulerM3, rowRulerM0), module, MidiFileModule::LOOP_PARAM, 0.0f, 1.0f, 1.0f));		
		
		
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