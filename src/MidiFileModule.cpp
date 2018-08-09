//***********************************************************************************************
//MidiFile module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//Also based on Midifile, a C++ MIDI file parsing library by Craig Stuart Sapp
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept by Marc Boulé
//***********************************************************************************************

// TODO: use osdialog's OSDIALOG_OPEN_DIR to open just a dir, and have next/prev load buttons to
//   scan all midi files in the dir (NOT SURE this is a GOOD IDEA)

/* temporary notes

https://github.com/craigsapp/midifile

Dekstop (callback mechanism and file opening):
https://github.com/dekstop/vcvrackplugins_dekstop/blob/master/src/Recorder.cpp

VCVRack-Simple (file opening):
https://github.com/IohannRabeson/VCVRack-Simple/commit/2d33e97d2e344d2926548a0b9f11f1c15ee4ca3c


*/


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"
#include "midifile/MidiFile.h"
#include "osdialog.h"
#include <iostream>

using namespace std;
using namespace smf;


//*****************************************************************************


struct MidiFileModule : Module {
	enum ParamIds {
		LOADMIDI_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		LOADMIDI_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Need to save, with reset
	// none
	
	// Need to save, no reset
	int panelTheme;
	
	// No need to save, with reset
	// none
	
	// No need to save, no reset
	MidiFile midifile;
	string lastPath;// TODO make this a Need to save, no reset
	
	
	
	MidiFileModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		
		// No need to save, no reset
		lastPath = "";

		onReset();
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {

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
		lights[LOADMIDI_LIGHT].value = params[LOADMIDI_PARAM].value > 0.5f ? 1.0f : 0.0f;
	}// step()	
	
	
	void loadMidiFile() {
		
		osdialog_filters *filters = osdialog_filters_parse("MidiFile:mid;TextFile:txt");
		string dir = lastPath.empty() ? assetLocal("") : stringDirectory(lastPath);
		char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
		if (path) {
			midifile.read(path);//"MIDI_sample.mid"
			midifile.doTimeAnalysis();
			midifile.linkNotePairs();

			int tracks = midifile.getTrackCount();
			cout << "TPQ: " << midifile.getTicksPerQuarterNote() << endl;
			if (tracks > 1) cout << "TRACKS: " << tracks << endl;
			for (int track=0; track<tracks; track++) {
				if (tracks > 1) cout << "\nTrack " << track << endl;
				cout << "Tick\tSeconds\tDur\tMessage" << endl;
				for (int event=0; event<midifile[track].size(); event++) {
					cout << dec << midifile[track][event].tick;
					cout << '\t' << dec << midifile[track][event].seconds;
					cout << '\t';
					if (midifile[track][event].isNoteOn())
						cout << midifile[track][event].getDurationInSeconds();
					cout << '\t' << hex;
					for (unsigned int i=0; i<midifile[track][event].size(); i++)
						cout << (int)midifile[track][event][i] << ' ';
					cout << endl;
				}
			}			
			
			free(path);
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
		
		// main load button
		LoadMidiPushButton* midiButton = createDynamicParam<LoadMidiPushButton>(Vec(100, 100), module, MidiFileModule::LOADMIDI_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme);
		midiButton->moduleL = module;
		addParam(midiButton);
		
		// load light
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(100, 200), module, MidiFileModule::LOADMIDI_LIGHT + 0));		
	}
};


Model *modelMidiFile = Model::create<MidiFileModule, MidiFileWidget>("Impromptu Modular", "Midi-File", "UTIL - Midi-File", UTILITY_TAG);

/*CHANGE LOG

0.6.10:
created 

*/