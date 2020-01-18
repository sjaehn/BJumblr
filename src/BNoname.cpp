/*  B.Noname
 * LV2 Plugin
 *
 * Copyright (C) 2018 by Sven Jähnichen
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

#include "BNoname.hpp"

inline double floorfrac (const double value) {return value - floor (value);}

BNoname::BNoname (double samplerate, const LV2_Feature* const* features) :
	map (NULL), unmap (NULL), controlPort (nullptr), notifyPort (nullptr),
	audioInput1 (nullptr), audioInput2 (nullptr),
	audioOutput1 (nullptr), audioOutput2 (nullptr),
	notifyForge (), notifyFrame (),
	padMessageBuffer {PadMessage()},
	new_controllers {nullptr}, controllers {0},
	pads {Pad()},
	rate (samplerate), bpm (120.0f), beatsPerBar (4.0f), beatUnit (0),
	speed (0.0f), bar (0), barBeat (0.0f),
	outCapacity (0), position (0.0), cursor (0), offset (0.0), refFrame (0),
	maxBufferSize (samplerate * 24 *32),
	audioBuffer1 (maxBufferSize, 0.0f),
	audioBuffer2 (maxBufferSize, 0.0f),
	audioBufferCounter (0), audioBufferSize (samplerate * 8),
	ui_on (false), scheduleNotifyPadsToGui (false), scheduleNotifyStatusToGui (false)

{
	//Scan host features for URID map
	LV2_URID_Map* m = NULL;
	LV2_URID_Unmap* u = NULL;
	for (int i = 0; features[i]; ++i)
	{
		if (strcmp (features[i]->URI, LV2_URID__map) == 0)
		{
			m = (LV2_URID_Map*) features[i]->data;
		}
		else if (strcmp (features[i]->URI, LV2_URID__unmap) == 0)
		{
			u = (LV2_URID_Unmap*) features[i]->data;
		}
	}

	if (!m)
	{
		fprintf (stderr, "BNoname.lv2: Host does not support urid:map.\n");
		return;
	}

	//Map URIS
	map = m;
	unmap = u;
	getURIs (m, &uris);
	if (!map) fprintf(stderr, "BNoname.lv2: Host does not support urid:map.\n");

	// Initialize notify
	lv2_atom_forge_init (&notifyForge, map);

	// Initialize padMessageBuffer
	padMessageBuffer[0] = PadMessage (ENDPADMESSAGE);

	// Initialize pads
	for (int i = 0; i < MAXSTEPS; ++i) pads[i][i].level = 1.0;

	// Initialize controllers
	// Controllers are zero initialized and will get data from host, only
	// NR_OF_STEPS need to be set to prevent div by zero.
	controllers[NR_OF_STEPS] = 32;

	ui_on = false;

}

void BNoname::connect_port (uint32_t port, void *data)
{
	switch (port)
	{
		case CONTROL:
			controlPort = (LV2_Atom_Sequence*) data;
			break;
		case NOTIFY:
			notifyPort = (LV2_Atom_Sequence*) data;
			break;
		case AUDIO_IN_1:
			audioInput1 = (float*) data;
			break;
		case AUDIO_IN_2:
			audioInput2 = (float*) data;
			break;
		case AUDIO_OUT_1:
			audioOutput1 = (float*) data;
			break;
		case AUDIO_OUT_2:
			audioOutput2 = (float*) data;
			break;
		default:
			// Connect controllers
			if ((port >= CONTROLLERS) && (port < CONTROLLERS + MAXCONTROLLERS)) new_controllers[port - CONTROLLERS] = (float*) data;
	}
}

void BNoname::runSequencer (const int start, const int end)
{
	for (int i = start; i < end; ++i)
	{
		// Init audio output
		audioOutput1[i] = 0;
		audioOutput2[i] = 0;

		// Interpolate position within the loop
		double relpos = getPositionFromFrames (i - refFrame);	// Position relative to reference frame
		double pos = floorfrac (position + relpos);		// 0..1 position
		double step = pos * controllers[NR_OF_STEPS];		// 0..NR_OF_STEPS position
		int iStep = step;

		// Store to buffers
		audioBuffer1[audioBufferCounter] = audioInput1[i];
		audioBuffer2[audioBufferCounter] = audioInput2[i];

		if (controllers[PLAY] != 0.0f)
		{
			for (int r = 0; r < MAXSTEPS; ++r)
			{
				float factor = pads[r][iStep].level;
				if (factor != 0.0)
				{
					int stepDiff = (r <= iStep ? iStep - r : iStep + controllers[NR_OF_STEPS] - r);
					size_t frame = size_t (maxBufferSize + audioBufferCounter - audioBufferSize * (double (stepDiff) / double (controllers[NR_OF_STEPS]))) % maxBufferSize;
					audioOutput1[i] += factor * audioBuffer1[frame];
					audioOutput2[i] += factor * audioBuffer2[frame];
				}
			}
		}

		// Increment counter
		audioBufferCounter = (audioBufferCounter + 1) % maxBufferSize;
	}
}

double BNoname::getPositionFromBeats (const double beats) const
{
	if (controllers[STEP_SIZE] == 0.0) return 0.0;

	switch (int (controllers[STEP_BASE]))
	{
		case SECONDS: 	return (bpm ? beats / (controllers[STEP_SIZE] * controllers[NR_OF_STEPS] * (bpm / 60.0)) : 0.0);
		case BEATS:	return beats / (controllers[STEP_SIZE] * controllers[NR_OF_STEPS]);
		case BARS:	return (beatsPerBar ? beats / (controllers[STEP_SIZE] * controllers[NR_OF_STEPS] * beatsPerBar) : 0.0);
		default:	return 0.0;
	}
}

double BNoname::getPositionFromFrames (const uint64_t frames) const
{
	if ((controllers[STEP_SIZE] == 0.0) || (rate == 0)) return 0.0;

	switch (int (controllers[STEP_BASE]))
	{
		case SECONDS: 	return frames * (1.0 / rate) / (controllers[STEP_SIZE] * controllers[NR_OF_STEPS]);
		case BEATS:	return (bpm ? frames * (speed / (rate / (bpm / 60))) / (controllers[STEP_SIZE] * controllers[NR_OF_STEPS]) : 0.0);
		case BARS:	return (bpm && beatsPerBar ? frames * (speed / (rate / (bpm / 60))) / (controllers[STEP_SIZE] * controllers[NR_OF_STEPS] * beatsPerBar) : 0.0);
		default:	return 0.0;
	}
}

double BNoname::getPositionFromSeconds (const double seconds) const
{
	if (controllers[STEP_SIZE] == 0.0) return 0.0;

	switch (int (controllers[STEP_BASE]))
	{
		case SECONDS :	return seconds / (controllers[STEP_SIZE] * controllers[NR_OF_STEPS]);
		case BEATS:	return seconds * (bpm / 60.0) / (controllers[STEP_SIZE] * controllers[NR_OF_STEPS]);
		case BARS:	return (beatsPerBar ? seconds * (bpm / 60.0 / beatsPerBar) / (controllers[STEP_SIZE] * controllers[NR_OF_STEPS]) : 0.0);
		default:	return 0;
	}
}

uint64_t BNoname::getFramesFromValue (const double value) const
{
	if (bpm < 1.0) return 0;

	switch (int (controllers[STEP_BASE]))
	{
		case SECONDS :	return value * rate;
		case BEATS:	return value * (60.0 / bpm) * rate;
		case BARS:	return value * beatsPerBar * (60.0 / bpm) * rate;
		default:	return 0;
	}
}

void BNoname::run (uint32_t n_samples)
{
	int64_t last_t = 0;

	if ((!controlPort) || (!notifyPort) || (!audioInput1) || (!audioInput2) || (!audioOutput1) || (!audioOutput2)) return;

	// Init notify port
	uint32_t space = notifyPort->atom.size;
	lv2_atom_forge_set_buffer(&notifyForge, (uint8_t*) notifyPort, space);
	lv2_atom_forge_sequence_head(&notifyForge, &notifyFrame, 0);

	// Validate controllers
	for (int i = 0; i < MAXCONTROLLERS; ++i)
	{
		if (new_controllers[i])
		{
			float val = validateValue (*(new_controllers[i]), controllerLimits[i]);
			if (val != *(new_controllers[i]))
			{
				fprintf (stderr, "BNoname.lv2: Value out of range in run (): Controller#%i\n", i);
				*(new_controllers[i]) = val;
				// TODO update GUI controller
			}
			controllers[i] = val;
		}
	}

	// Read CONTROL port (notifications from GUI and host)
	LV2_ATOM_SEQUENCE_FOREACH (controlPort, ev)
	{
		if ((ev->body.type == uris.atom_Object) || (ev->body.type == uris.atom_Blank))
		{
			const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

			// GUI on
			if (obj->body.otype == uris.ui_on)
			{
				ui_on = true;
				//fprintf (stderr, "BNoname.lv2: UI on received.\n");
				padMessageBufferAllPads ();
				scheduleNotifyPadsToGui = true;
				scheduleNotifyStatusToGui = true;
			}

			// GUI off
			else if (obj->body.otype == uris.ui_off)
			{
				//fprintf (stderr, "BNoname.lv2: UI off received.\n");
				ui_on = false;
			}

			// GUI pad changed notifications
			else if (obj->body.otype == uris.notify_padEvent)
			{
				LV2_Atom *oPd = NULL;
				lv2_atom_object_get (obj, uris.notify_pad,  &oPd, NULL);

				// Pad notification
				if (oPd && (oPd->type == uris.atom_Vector))
				{
					const LV2_Atom_Vector* vec = (const LV2_Atom_Vector*) oPd;
					if (vec->body.child_type == uris.atom_Float)
					{
						const uint32_t size = (uint32_t) ((oPd->size - sizeof(LV2_Atom_Vector_Body)) / sizeof (PadMessage));
						PadMessage* pMes = (PadMessage*) (&vec->body + 1);

						// Copy PadMessages to pads
						for (unsigned int i = 0; i < size; ++i)
						{
							int row = (int) pMes[i].row;
							int step = (int) pMes[i].step;
							if ((row >= 0) && (row < MAXSTEPS) && (step >= 0) && (step < MAXSTEPS))
							{
								Pad pd (pMes[i].level);
								Pad valPad = validatePad (pd);
								pads[row][step] = valPad;
								if (valPad != pd)
								{
									fprintf (stderr, "BNoname.lv2: Pad out of range in run (): pads[%i][%i].\n", row, step);
									padMessageBufferAppendPad (row, step, valPad);
									scheduleNotifyPadsToGui = true;
								}
							}
						}
					}
				}
			}

			// Process time / position data
			else if (obj->body.otype == uris.time_Position)
			{
				bool scheduleUpdatePosition = false;

				// Update bpm, speed, position
				LV2_Atom *oBbeat = NULL, *oBpm = NULL, *oSpeed = NULL, *oBpb = NULL, *oBu = NULL, *oBar = NULL;
				const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
				lv2_atom_object_get
				(
					obj, uris.time_bar, &oBar,
					uris.time_barBeat, &oBbeat,
					uris.time_beatsPerMinute,  &oBpm,
					uris.time_beatsPerBar,  &oBpb,
					uris.time_beatUnit,  &oBu,
					uris.time_speed, &oSpeed,
					NULL
				);

				// BPM changed?
				if (oBpm && (oBpm->type == uris.atom_Float) && (bpm != ((LV2_Atom_Float*)oBpm)->body))
				{
					bpm = ((LV2_Atom_Float*)oBpm)->body;
					scheduleUpdatePosition = true;
				}

				// Beats per bar changed?
				if (oBpb && (oBpb->type == uris.atom_Float) && (beatsPerBar != ((LV2_Atom_Float*)oBpb)->body))
				{
					beatsPerBar = ((LV2_Atom_Float*)oBpb)->body;
					scheduleUpdatePosition = true;
				}

				// BeatUnit changed?
				if (oBu && (oBu->type == uris.atom_Int) && (beatUnit != ((LV2_Atom_Int*)oBu)->body))
				{
					beatUnit = ((LV2_Atom_Int*)oBu)->body;
					scheduleUpdatePosition = true;
				}

				// Speed changed?
				if (oSpeed && (oSpeed->type == uris.atom_Float) && (speed != ((LV2_Atom_Float*)oSpeed)->body))
				{
					speed = ((LV2_Atom_Float*)oSpeed)->body;
					scheduleUpdatePosition = true;
				}

				// Bar position changed
				if (oBar && (oBar->type == uris.atom_Long) && (bar != ((long)((LV2_Atom_Long*)oBar)->body)))
				{
					bar = ((LV2_Atom_Long*)oBar)->body;
					scheduleUpdatePosition = true;
				}

				// Beat position changed (during playing) ?
				if (oBbeat && (oBbeat->type == uris.atom_Float) && (barBeat != ((LV2_Atom_Float*)oBbeat)->body))
				{
					barBeat = ((LV2_Atom_Float*)oBbeat)->body;
					scheduleUpdatePosition = true;
				}

				if (scheduleUpdatePosition)
				{
					// Hard set new position if new data received
					double pos = getPositionFromBeats (barBeat + beatsPerBar * bar);
					position = floorfrac (pos - offset);
					refFrame = ev->time.frames;
					uint64_t size = getFramesFromValue (controllers[STEP_SIZE] * controllers[NR_OF_STEPS]);
					audioBufferSize = LIMIT (size, 0, maxBufferSize);
				}
			}
		}

		// Update for this iteration
		uint32_t next_t = (ev->time.frames < n_samples ? ev->time.frames : n_samples);
		runSequencer (last_t, next_t);
		last_t = next_t;
	}

	// Update for the remainder of the cycle
	if (last_t < n_samples) runSequencer (last_t, n_samples);

	// Update position in case of no new barBeat submitted on next call
	double relpos = getPositionFromFrames (n_samples - refFrame);	// Position relative to reference frame
	position = floorfrac (position + relpos);
	if (controllers[PLAY]) cursor = position * controllers[NR_OF_STEPS];
	refFrame = 0;

	scheduleNotifyStatusToGui = true;

	// Send notifications to GUI
	if (ui_on && scheduleNotifyStatusToGui) notifyStatusToGui ();
	if (ui_on && scheduleNotifyPadsToGui) notifyPadsToGui ();
	lv2_atom_forge_pop(&notifyForge, &notifyFrame);
}

LV2_State_Status BNoname::state_save (LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags,
			const LV2_Feature* const* features)
{
	// Store pads
	char padDataString[0x8010] = "Matrix data:\n";

	for (int step = 0; step < MAXSTEPS; ++step)
	{
		for (int row = 0; row < MAXSTEPS; ++row)
		{
			char valueString[64];
			int id = step * MAXSTEPS + row;
			Pad* pd = &pads[row][step];
			snprintf (valueString, 62, "id:%d; lv:%f", id, pd->level);
			if ((step < MAXSTEPS - 1) || (row < MAXSTEPS)) strcat (valueString, ";\n");
			else strcat(valueString, "\n");
			strcat (padDataString, valueString);
		}
	}
	store (handle, uris.state_pad, padDataString, strlen (padDataString) + 1, uris.atom_String, LV2_STATE_IS_POD);

	//fprintf (stderr, "BNoname.lv2: State saved.\n");
	return LV2_STATE_SUCCESS;
}

LV2_State_Status BNoname::state_restore (LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags,
			const LV2_Feature* const* features)
{
	//fprintf (stderr, "BNoname.lv2: state_restore ()\n");

	// Retrieve pad data
	size_t   size;
	uint32_t type;
	uint32_t valflags;
	const void* padData = retrieve(handle, uris.state_pad, &size, &type, &valflags);

	if (padData && (type == uris.atom_String))
	{
		// Restore pads
		// Parse retrieved data
		std::string padDataString = (char*) padData;
		const std::string keywords[2] = {"id:", "lv:"};
		while (!padDataString.empty())
		{
			// Look for next "id:"
			size_t strPos = padDataString.find ("id:");
			size_t nextPos = 0;
			if (strPos == std::string::npos) break;	// No "id:" found => end
			if (strPos + 3 > padDataString.length()) break;	// Nothing more after id => end
			padDataString.erase (0, strPos + 3);
			int id;
			try {id = std::stof (padDataString, &nextPos);}
			catch  (const std::exception& e)
			{
				fprintf (stderr, "BNoname.lv2: Restore pad state incomplete. Can't parse ID from \"%s...\"", padDataString.substr (0, 63).c_str());
				break;
			}

			if (nextPos > 0) padDataString.erase (0, nextPos);
			if ((id < 0) || (id >= MAXSTEPS * MAXSTEPS))
			{
				fprintf (stderr, "BNoname.lv2: Restore pad state incomplete. Invalid matrix data block loaded with ID %i. Try to use the data before this id.\n", id);
				break;
			}
			int row = id % MAXSTEPS;
			int step = id / MAXSTEPS;

			// Look for pad data
			for (int i = 1; i < 2; ++i)
			{
				strPos = padDataString.find (keywords[i]);
				if (strPos == std::string::npos) continue;	// Keyword not found => next keyword
				if (strPos + 3 >= padDataString.length())	// Nothing more after keyword => end
				{
					padDataString ="";
					break;
				}
				if (strPos > 0) padDataString.erase (0, strPos + 3);
				float val;
				try {val = std::stof (padDataString, &nextPos);}
				catch  (const std::exception& e)
				{
					fprintf (stderr, "BNoname.lv2: Restore padstate incomplete. Can't parse %s from \"%s...\"",
							 keywords[i].substr(0,2).c_str(), padDataString.substr (0, 63).c_str());
					break;
				}

				if (nextPos > 0) padDataString.erase (0, nextPos);
				switch (i) {
				case 1: pads[row][step].level = val;
						break;
				default:break;
				}
			}
		}


		// Validate all pads
		for (int i = 0; i < MAXSTEPS; ++i)
		{
			for (int j = 0; j < MAXSTEPS; ++j)
			{
				Pad valPad = validatePad (pads[i][j]);
				if (valPad != pads[i][j])
				{
					fprintf (stderr, "BNoname.lv2: Pad out of range in state_restore (): pads[%i][%i].\n", i, j);
					pads[i][j] = valPad;
				}
			}
		}

		// Copy all to padMessageBuffer for submission to GUI
		padMessageBufferAllPads ();

		// Force GUI notification
		scheduleNotifyPadsToGui = true;
	}

	// Force GUI notification
	scheduleNotifyStatusToGui = true;

	return LV2_STATE_SUCCESS;
}

/*
 * Checks if a value is within a limit, and if not, puts the value within
 * this limit.
 * @param value
 * @param limit
 * @return		Value is within the limit
 */
float BNoname::validateValue (float value, const Limit limit)
{
	float ltdValue = ((limit.step != 0) ? (limit.min + round ((value - limit.min) / limit.step) * limit.step) : value);
	return LIMIT (ltdValue, limit.min, limit.max);
}

/*
 * Validates a single pad
 */
Pad BNoname::validatePad (Pad pad)
{
	return Pad(validateValue (pad.level, {0, 1, 0}));
}

/*
 * Appends a single pad to padMessageBuffer
 */
bool BNoname::padMessageBufferAppendPad (int row, int step, Pad pad)
{
	PadMessage end = PadMessage (ENDPADMESSAGE);
	PadMessage msg = PadMessage (step, row, pad.level);

	for (int i = 0; i < MAXSTEPS * MAXSTEPS; ++i)
	{
		if (padMessageBuffer[i] != end)
		{
			padMessageBuffer[i] = msg;
			if (i < MAXSTEPS * MAXSTEPS - 1) padMessageBuffer[i + 1] = end;
			return true;
		}
	}
	return false;
}


/*
 * Copies all pads to padMessageBuffer (thus overwrites it!)
 */
void BNoname::padMessageBufferAllPads ()
{
	for (int i = 0; i < MAXSTEPS; ++i)
	{
		for (int j = 0; j < MAXSTEPS; ++j)
		{
			Pad* pd = &(pads[j][i]);
			padMessageBuffer[i * MAXSTEPS + j] = PadMessage (i, j, pd->level);
		}
	}
}

void BNoname::notifyPadsToGui ()
{
	PadMessage endmsg (ENDPADMESSAGE);
	if (!(endmsg == padMessageBuffer[0]))
	{
		// Get padMessageBuffer size
		int end = 0;
		for (int i = 0; (i < MAXSTEPS * MAXSTEPS) && (!(padMessageBuffer[i] == endmsg)); ++i) end = i;

		// Prepare forge buffer and initialize atom sequence

		LV2_Atom_Forge_Frame frame;
		lv2_atom_forge_frame_time(&notifyForge, 0);
		lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_padEvent);
		lv2_atom_forge_key(&notifyForge, uris.notify_pad);
		lv2_atom_forge_vector(&notifyForge, sizeof(float), uris.atom_Float, sizeof(PadMessage) / sizeof(float) * (end + 1), (void*) padMessageBuffer);
		lv2_atom_forge_pop(&notifyForge, &frame);

		// Empty padMessageBuffer
		padMessageBuffer[0] = endmsg;

		scheduleNotifyPadsToGui = false;
	}
}

void BNoname::notifyStatusToGui ()
{
	// Prepare forge buffer and initialize atom sequence
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(&notifyForge, 0);
	lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_statusEvent);
	lv2_atom_forge_key(&notifyForge, uris.notify_cursor);
	lv2_atom_forge_int(&notifyForge, cursor);
	lv2_atom_forge_pop(&notifyForge, &frame);

	scheduleNotifyStatusToGui = false;
}

/*
 *
 *
 ******************************************************************************
 *  LV2 specific declarations
 */

static LV2_Handle instantiate (const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features)
{
	// New instance
	BNoname* instance;
	try {instance = new BNoname(samplerate, features);}
	catch (std::exception& exc)
	{
		fprintf (stderr, "BNoname.lv2: Plugin instantiation failed. %s\n", exc.what ());
		return NULL;
	}

	return (LV2_Handle)instance;
}

static void connect_port (LV2_Handle instance, uint32_t port, void *data)
{
	BNoname* inst = (BNoname*) instance;
	inst->connect_port (port, data);
}

static void run (LV2_Handle instance, uint32_t n_samples)
{
	BNoname* inst = (BNoname*) instance;
	inst->run (n_samples);
}

static LV2_State_Status state_save(LV2_Handle instance, LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags,
           const LV2_Feature* const* features)
{
	BNoname* inst = (BNoname*)instance;
	if (!inst) return LV2_STATE_SUCCESS;

	inst->state_save (store, handle, flags, features);
	return LV2_STATE_SUCCESS;
}

static LV2_State_Status state_restore(LV2_Handle instance, LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags,
           const LV2_Feature* const* features)
{
	BNoname* inst = (BNoname*)instance;
	inst->state_restore (retrieve, handle, flags, features);
	return LV2_STATE_SUCCESS;
}

static void cleanup (LV2_Handle instance)
{
	BNoname* inst = (BNoname*) instance;
	delete inst;
}


static const void* extension_data(const char* uri)
{
  static const LV2_State_Interface  state  = {state_save, state_restore};
  if (!strcmp(uri, LV2_STATE__interface)) {
    return &state;
  }
  return NULL;
}


static const LV2_Descriptor descriptor =
{
		BNONAME_URI,
		instantiate,
		connect_port,
		NULL,	// activate
		run,
		NULL,	// deactivate
		cleanup,
		extension_data
};

// LV2 Symbol Export
LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor (uint32_t index)
{
	switch (index)
	{
	case 0: return &descriptor;
	default: return NULL;
	}
}

/* End of LV2 specific declarations
 *
 * *****************************************************************************
 *
 *
 */
