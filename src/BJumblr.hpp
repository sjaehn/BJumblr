/*  B.Jumblr
 * LV2 Plugin
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
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/ext/time/time.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>
#include "definitions.h"
#include "Ports.hpp"
#include "Urids.hpp"
#include "Pad.hpp"
#include "PadMessage.hpp"
#include "Message.hpp"

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
	void connect_port(uint32_t port, void *data);
	void run(uint32_t n_samples);
	LV2_State_Status state_save(LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags, const LV2_Feature* const* features);
	LV2_State_Status state_restore(LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, const LV2_Feature* const* features);

private:

	double getPositionFromBeats (const double beats) const;
	double getPositionFromFrames (const uint64_t frames) const;
	double getPositionFromSeconds (const double seconds) const;
	uint64_t getFramesFromValue (const double value) const;
	void runSequencer (const int start, const int end);
	float validateValue (float value, const Limit limit);
	Pad validatePad (Pad pad);
	bool padMessageBufferAppendPad (int row, int step, Pad pad);
	void padMessageBufferAllPads ();
	void notifyPadsToGui ();
	void notifyStatusToGui ();
	void notifyMessageToGui();

	// URIs
	BJumblrURIs uris;
	LV2_URID_Map* map;
	LV2_URID_Unmap* unmap;

	// DSP <-> GUI communication
	const LV2_Atom_Sequence* controlPort;
	LV2_Atom_Sequence* notifyPort;
	float* audioInput1;
	float* audioInput2;
	float* audioOutput1;
	float* audioOutput2;

	LV2_Atom_Forge notifyForge;
	LV2_Atom_Forge_Frame notifyFrame;

	PadMessage padMessageBuffer[MAXSTEPS * MAXSTEPS];

	// Controllers
	float* new_controllers [MAXCONTROLLERS];
	float controllers [MAXCONTROLLERS];
	Limit controllerLimits [MAXCONTROLLERS] =
	{
		{0, 1, 1},	// PLAY
		{4, 32, 1}, 	// NR_OF_STEPS
		{0, 2, 1},	// STEP_BASE
		{0.01, 4, 0},	// STEP_SIZE
		{0, 31, 1}	// STEP_OFFSET
	};

	//Pads
	int editMode;
	Pad pads [MAXSTEPS] [MAXSTEPS];

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
	int cursor;
	double offset;
	uint64_t refFrame;

	size_t maxBufferSize;
	std::vector<float> audioBuffer1;
	std::vector<float> audioBuffer2;
	size_t audioBufferCounter;
	size_t audioBufferSize;

	// Internals
	bool ui_on;
	bool scheduleNotifyPadsToGui;
	bool scheduleNotifyStatusToGui;
	Message message;
};

#endif /* BJUMBLR_HPP_ */
