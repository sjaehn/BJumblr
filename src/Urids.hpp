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

#ifndef URIDS_HPP_
#define URIDS_HPP_

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/ext/time/time.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>
#include "definitions.h"

struct BJumblrURIs
{
	LV2_URID atom_Sequence;
	LV2_URID atom_Float;
	LV2_URID atom_Double;
	LV2_URID atom_Bool;
	LV2_URID atom_Int;
	LV2_URID atom_Object;
	LV2_URID atom_Blank;
	LV2_URID atom_eventTransfer;
	LV2_URID atom_Vector;
	LV2_URID atom_Long;
	LV2_URID atom_String;
	LV2_URID atom_Path;
	LV2_URID time_Position;
	LV2_URID time_bar;
	LV2_URID time_barBeat;
	LV2_URID time_beatsPerMinute;
	LV2_URID time_beatsPerBar;
	LV2_URID time_beatUnit;
	LV2_URID time_speed;
	LV2_URID midi_Event;
	LV2_URID ui_on;
	LV2_URID ui_off;
	LV2_URID state_pad;
	LV2_URID notify_padEvent;
	LV2_URID notify_padPage;
	LV2_URID notify_pad;
	LV2_URID notify_padFullPattern;
	LV2_URID notify_editMode;
	LV2_URID notify_sampleFreeEvent;
	LV2_URID notify_pathEvent;
	LV2_URID notify_samplePath;
	LV2_URID notify_statusEvent;
	LV2_URID notify_requestMidiLearn;
	LV2_URID notify_midiLearned;
	LV2_URID notify_maxPage;
	LV2_URID notify_playbackPage;
	LV2_URID notify_cursor;
	LV2_URID notify_progressionDelay;
	LV2_URID notify_messageEvent;
	LV2_URID notify_message;
	LV2_URID notify_waveformEvent;
	LV2_URID notify_waveformStart;
	LV2_URID notify_waveformData;
};

void getURIs (LV2_URID_Map* m, BJumblrURIs* uris)
{
	uris->atom_Sequence = m->map(m->handle, LV2_ATOM__Sequence);
	uris->atom_Float = m->map(m->handle, LV2_ATOM__Float);
	uris->atom_Double = m->map(m->handle, LV2_ATOM__Double);
	uris->atom_Bool = m->map(m->handle, LV2_ATOM__Bool);
	uris->atom_Int = m->map(m->handle, LV2_ATOM__Int);
	uris->atom_Object = m->map(m->handle, LV2_ATOM__Object);
	uris->atom_Blank = m->map(m->handle, LV2_ATOM__Blank);
	uris->atom_eventTransfer = m->map(m->handle, LV2_ATOM__eventTransfer);
	uris->atom_Vector = m->map(m->handle, LV2_ATOM__Vector);
	uris->atom_Long = m->map (m->handle, LV2_ATOM__Long);
	uris->atom_String = m->map (m->handle, LV2_ATOM__String);
	uris->atom_Path = m->map(m->handle, LV2_ATOM__Path);
	uris->time_Position = m->map(m->handle, LV2_TIME__Position);
	uris->time_bar = m->map(m->handle, LV2_TIME__bar);
	uris->time_barBeat = m->map(m->handle, LV2_TIME__barBeat);
	uris->time_beatsPerMinute = m->map(m->handle, LV2_TIME__beatsPerMinute);
	uris->time_beatUnit = m->map(m->handle, LV2_TIME__beatUnit);
	uris->time_beatsPerBar = m->map(m->handle, LV2_TIME__beatsPerBar);
	uris->time_speed = m->map(m->handle, LV2_TIME__speed);
	uris->midi_Event = m->map(m->handle, LV2_MIDI__MidiEvent);
	uris->ui_on = m->map(m->handle, BJUMBLR_URI "#UIon");
	uris->ui_off = m->map(m->handle, BJUMBLR_URI "#UIoff");
	uris->state_pad = m->map(m->handle, BJUMBLR_URI "#STATEpad");
	uris->notify_padEvent = m->map(m->handle, BJUMBLR_URI "#NOTIFYpadEvent");
	uris->notify_padPage = m->map(m->handle, BJUMBLR_URI "#NOTIFYpadPage");
	uris->notify_pad = m->map(m->handle, BJUMBLR_URI "#NOTIFYpad");
	uris->notify_padFullPattern = m->map(m->handle, BJUMBLR_URI "#NOTIFYpadFullPattern");
	uris->notify_editMode = m->map(m->handle, BJUMBLR_URI "#NOTIFYeditMode");
	uris->notify_sampleFreeEvent = m->map(m->handle, BJUMBLR_URI "#NOTIFYsampleFreeEvent");
	uris->notify_pathEvent = m->map(m->handle, BJUMBLR_URI "#NOTIFYpathEvent");
	uris->notify_samplePath = m->map(m->handle, BJUMBLR_URI "#NOTIFYsamplePath");
	uris->notify_statusEvent = m->map(m->handle, BJUMBLR_URI "#NOTIFYstatusEvent");
	uris->notify_requestMidiLearn = m->map(m->handle, BJUMBLR_URI "#NOTIFYrequestMidiLearn");
	uris->notify_midiLearned = m->map(m->handle, BJUMBLR_URI "#NOTIFYmidiLearned");
	uris->notify_maxPage = m->map(m->handle, BJUMBLR_URI "#NOTIFYpadMaxPage");
	uris->notify_playbackPage = m->map(m->handle, BJUMBLR_URI "#NOTIFYplaybackPage");
	uris->notify_cursor = m->map(m->handle, BJUMBLR_URI "#NOTIFYcursor");
	uris->notify_progressionDelay = m->map(m->handle, BJUMBLR_URI "#NOTIFYplaybackDelay");
	uris->notify_messageEvent = m->map(m->handle, BJUMBLR_URI "#NOTIFYmessageEvent");
	uris->notify_message = m->map(m->handle, BJUMBLR_URI "#NOTIFYmessage");
	uris->notify_waveformEvent = m->map(m->handle, BJUMBLR_URI "#NOTIFYwaveformEvent");
	uris->notify_waveformStart = m->map(m->handle, BJUMBLR_URI "#NOTIFYwaveformStart");
	uris->notify_waveformData = m->map(m->handle, BJUMBLR_URI "#NOTIFYwaveformData");
}

#endif /* URIDS_HPP_ */
