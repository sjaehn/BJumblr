/* B.Jumblr
 * Pattern-controlled audio stream / sample re-sequencer LV2 plugin
 *
 * Copyright (C) 2018, 2019 by Sven Jähnichen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef BJUMBLR_HPP_
#define BJUMBLR_HPP_

#define FADETIME 0.01
#define CONTROLLER_CHANGED(con) ((new_controllers[con]) ? (controllers[con] != *(new_controllers[con])) : false)

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <array>
#include <stdexcept>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/ext/time/time.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>
#include "lv2/lv2plug.in/ns/ext/worker/worker.h"
#include "definitions.h"
#include "Ports.hpp"
#include "Urids.hpp"
#include "Pad.hpp"
#include "PadMessage.hpp"
#include "Message.hpp"
#include "sndfile.h"

class Sample;	// Forward declaration

struct Limit
{
	float min;
	float max;
	float step;
};

class BJumblr
{
public:
	BJumblr (double samplerate, const LV2_Feature* const* features);
	~BJumblr();
	void connect_port(uint32_t port, void *data);
	void run(uint32_t n_samples);
	void activate();
	void deactivate();
	LV2_State_Status state_save(LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags, const LV2_Feature* const* features);
	LV2_State_Status state_restore(LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, const LV2_Feature* const* features);
	LV2_Worker_Status work (LV2_Worker_Respond_Function respond, LV2_Worker_Respond_Handle handle, uint32_t size, const void* data);
	LV2_Worker_Status work_response (uint32_t size, const void* data);

private:

	double getPositionFromBeats (const double beats) const;
	double getPositionFromFrames (const uint64_t frames) const;
	double getPositionFromSeconds (const double seconds) const;
	uint64_t getFramesFromValue (const double value) const;
	void runSequencer (const int start, const int end);
	float validateValue (float value, const Limit limit);
	Pad validatePad (Pad pad);
	bool padMessageBufferAppendPad (int page, int row, int step, Pad pad);
	LV2_Atom_Forge_Ref forgeSamplePath (LV2_Atom_Forge* forge, LV2_Atom_Forge_Frame* frame, const char* path, const int64_t start, const int64_t end, const float amp, const bool loop);
	void notifyPadsToGui ();
	void notifyStatusToGui ();
	void notifyWaveformToGui (const int start, const int end);
	void notifySchedulePageToGui ();
	void notifyPlaybackPageToGui ();
	void notifyMidiLearnedToGui ();
	void notifyMessageToGui();
	void notifySamplePathToGui ();

	// URIs
	BJumblrURIs uris;
	LV2_URID_Map* map;
	LV2_URID_Unmap* unmap;

	LV2_Worker_Schedule* workerSchedule;

	// DSP <-> GUI communication
	const LV2_Atom_Sequence* controlPort;
	LV2_Atom_Sequence* notifyPort;
	float* audioInput1;
	float* audioInput2;
	float* audioOutput1;
	float* audioOutput2;

	LV2_Atom_Forge notifyForge;
	LV2_Atom_Forge_Frame notifyFrame;

	std::array<std::array <PadMessage, MAXSTEPS * MAXSTEPS>, MAXPAGES> padMessageBuffer;

	float waveform[WAVEFORMSIZE];
	int waveformCounter;
	int lastWaveformCounter;

	// Controllers
	float* new_controllers [MAXCONTROLLERS];
	float controllers [MAXCONTROLLERS];
	Limit controllerLimits [MAXCONTROLLERS] =
	{
		{0, 1, 1},		// SOURCE
		{0, 2, 1},		// PLAY
		{2, 32, 1}, 		// NR_OF_STEPS
		{0, 2, 1},		// STEP_BASE
		{0.01, 4, 0},		// STEP_SIZE
		{0, 31, 1},		// STEP_OFFSET
		{-32, 32, 0},		// MANUAL_PROGRSSION_DELAY
		{0, 4, 0},		// SPEED
		{0, 15, 1},		// PAGE
		{0, 15, 1},		// MIDI
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1},
		{0, 15, 1},
		{0, 16, 1},
		{0, 128, 1},
		{0, 128, 1}
	};

	// Pads
	int editMode;
	bool midiLearn;
	uint8_t midiLearned[4];
	int nrPages;
	int schedulePage;
	int playPage;
	int lastPage;
	Pad pads [MAXPAGES] [MAXSTEPS] [MAXSTEPS];

	Sample* sample;
	float sampleAmp;

	struct WorkerMessage
	{
		LV2_Atom atom;
        	Sample*  sample;
		int64_t start;
		int64_t end;
		float amp;
		bool loop;
	};

	// Host communicated data
	double rate;
	float bpm;
	float beatsPerBar;
	int beatUnit;
	float speed;
	long bar;
	float barBeat;
	uint32_t outCapacity;

	// Position data
	double position;
	float cursor;
	double offset;
	uint64_t refFrame;
	float progressionDelay;
	float progressionDelayFrac;

	size_t maxBufferSize;
	std::vector<float> audioBuffer1;
	std::vector<float> audioBuffer2;
	size_t audioBufferCounter;
	size_t audioBufferSize;

	// Internals
	bool activated;
	bool ui_on;
	bool scheduleNotifyPadsToGui;
	bool scheduleNotifyFullPatternToGui[MAXPAGES];
	bool scheduleNotifySchedulePageToGui;
	bool scheduleNotifyPlaybackPageToGui;
	bool scheduleNotifyStatusToGui;
	bool scheduleNotifyWaveformToGui;
	bool scheduleNotifySamplePathToGui;
	bool scheduleNotifyMidiLearnedToGui;
	Message message;
};

#endif /* BJUMBLR_HPP_ */
