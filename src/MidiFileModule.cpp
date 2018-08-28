//***********************************************************************************************
//MidiFile module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
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

Dekstop (callback mechanism and file opening):
https://github.com/dekstop/vcvrackplugins_dekstop/blob/master/src/Recorder.cpp

VCVRack-Simple (file opening):
https://github.com/IohannRabeson/VCVRack-Simple/commit/2d33e97d2e344d2926548a0b9f11f1c15ee4ca3c

*/


#include "ImpromptuModular.hpp"
#include "midifile/MidiFile.h"
#include "osdialog.h"
#include <iostream>

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
		NUM_OUTPUTS
	};
	enum LightIds {
		RESET_LIGHT,
		RUN_LIGHT,
		ENUMS(LOADMIDI_LIGHT, 2),
		NUM_LIGHTS
	};
	
	
	// Need to save, with reset
	// none
	
	// Need to save, no reset
	int panelTheme;
	string lastPath;// TODO: save also the filename so that it can automatically be reloaded when Rack starts?
	
	// No need to save, with reset
	bool running;
	double time;
	long event;
	PulseGenerator gateGens[4];
	
	// No need to save, no reset
	MidiFile midifile;
	bool fileLoaded;
	SchmittTrigger runningTrigger;
	SchmittTrigger resetTrigger;
	float resetLight;
	
	
	MidiFileModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		lastPath = "";
		
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
		for (int i = 0; i < 4; i++)
			gateGens[i].reset();
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
					running = false;
					event = 0;
					break;
				}
				
				readTime = midifile[track][event].seconds;
				if (readTime > time)
					break;
				info("*** POLL NOTE TYPE, event = %i", event);
				if (midifile[track][event].isNoteOn()) {
					int chan = midifile[track][event].getChannel();
					info("*** NOTE ON chan %i", chan);
					if (chan >= 0 && chan < 4) {
						outputs[CV_OUTPUTS + chan].value = (float)midifile[track][event].getKeyNumber() * (10.0f / 127.0f);// TODO convert to correct CV level
						gateGens[chan].trigger(midifile[track][event].getDurationInSeconds());
						outputs[VELOCITY_OUTPUTS + chan].value = (float)midifile[track][event].getVelocity() * (10.0f / 127.0f);
					}
				}
				else if (midifile[track][event].isNoteOff()) {
					int chan = midifile[track][event].getChannel();
					info("*** NOTE OFF chan %i", chan);
					if (chan >= 0 && chan < 4) {
						gateGens[chan].reset();
					}
				}
				event++;
			}
		
			time += sampleTime;
			
			//info("*** time = %f, readTime = %f, event = %i", time, readTime, event);
		}
		
		
		// Reset
		if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
			//clockTrigger.reset();
			//clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
			resetLight = 1.0f;
			time = 0.0;
			event = 0;
			for (int i = 0; i < 4; i++)
				gateGens[i].reset();
		}				
		
		
		
		//********** Outputs and lights **********
		
		for (int i = 0; i < 4; i++)
			outputs[GATE_OUTPUTS + i].value = gateGens[i].process(sampleTime) ? 10.0f : 0.0f;
		
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
			for (int i = 0; i < 4; i++)
				gateGens[i].reset();
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