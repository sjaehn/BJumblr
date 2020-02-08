/*  B.Jumblr
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

#include "BJumblr.hpp"

inline double floorfrac (const double value) {return value - floor (value);}

BJumblr::BJumblr (double samplerate, const LV2_Feature* const* features) :
	map (NULL), unmap (NULL), workerSchedule (NULL),
	controlPort (nullptr), notifyPort (nullptr),
	audioInput1 (nullptr), audioInput2 (nullptr),
	audioOutput1 (nullptr), audioOutput2 (nullptr),
	notifyForge (), notifyFrame (),
	padMessageBuffer {PadMessage()},
	waveform {0.0f}, waveformCounter (0), lastWaveformCounter (0),
	new_controllers {nullptr}, controllers {0},
	editMode (0), pads {Pad()}, sample (nullptr),
	rate (samplerate), bpm (120.0f), beatsPerBar (4.0f), beatUnit (0),
	speed (0.0f), bar (0), barBeat (0.0f),
	outCapacity (0), position (0.0), cursor (0), offset (0.0), refFrame (0),
	maxBufferSize (samplerate * 24 * 32),
	audioBuffer1 (maxBufferSize, 0.0f),
	audioBuffer2 (maxBufferSize, 0.0f),
	audioBufferCounter (0), audioBufferSize (samplerate * 8),
	ui_on (false), scheduleNotifyPadsToGui (false), scheduleNotifyStatusToGui (false),
	scheduleNotifyWaveformToGui (false), scheduleNotifySamplePathToGui (false),
	message ()

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

		else if (!strcmp(features[i]->URI, LV2_WORKER__schedule))
		{
                        workerSchedule = (LV2_Worker_Schedule*)features[i]->data;
		}
	}

	if (!m) throw std::invalid_argument ("BJumblr.lv2: Host does not support urid:map.");
	if (!workerSchedule) throw std::invalid_argument ("BJumblr.lv2: Host does not support work:schedule.");

	//Map URIS
	map = m;
	unmap = u;
	getURIs (m, &uris);

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

BJumblr::Sample::Sample () : info {0, 0, 0, 0, 0, 0}, data (nullptr), path (nullptr) {}

BJumblr::Sample::Sample (const char* samplepath) : info {0, 0, 0, 0, 0, 0}, data (nullptr), path (nullptr)
{
	if (!samplepath) return;

	SNDFILE* const sndfile = sf_open (samplepath, SFM_READ, &info);

        if (!sndfile || !info.frames) throw std::invalid_argument("BJumblr.lv2: Can't open " + std::string(samplepath) + ".");

        // Read & render data
        data = (float*) malloc (sizeof(float) * info.frames * info.channels);
        if (!data)
	{
		sf_close (sndfile);
		throw std::bad_alloc();
	}

        sf_seek (sndfile, 0, SEEK_SET);
        sf_read_float (sndfile, data, info.frames);
        sf_close (sndfile);

	int len = strlen (samplepath);
        path = (char*) malloc (len + 1);
        if (path) memcpy(path, samplepath, len + 1);
}

BJumblr::Sample::~Sample ()
{
	if (data) free (data);
	if (path) free (path);
}

float BJumblr::Sample::get (const sf_count_t frame, const int channel, const int rate)
{
	if (!data) return 0.0f;

	// Direct access if same frame rate
	if (info.samplerate == rate)
	{
		if (frame >= info.frames) return 0.0f;
		else return data[frame * info.channels + channel];
	}

	// Linear rendering (TODO) if frame rates differ
	double f = (frame * info.samplerate) / rate;
	sf_count_t f1 = f;

	if (f1 + 1 >= info.frames) return 0.0f;
	if (f1 == f) return data[f1 * info.channels + channel];

	float data1 = data[f1 * info.channels + channel];
	float data2 = data[(f1 + 1) * info.channels + channel];
	return data1 + (f - double (f1)) * (data2 - data1);
}

void BJumblr::connect_port (uint32_t port, void *data)
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

void BJumblr::runSequencer (const int start, const int end)
{
	for (int i = start; i < end; ++i)
	{
		// Interpolate position within the loop
		double relpos = getPositionFromFrames (i - refFrame);	// Position relative to reference frame
		double pos = floorfrac (position + relpos);		// 0..1 position

		float input1 = 0;
		float input2 = 0;

		// Store audio input signal to buffer
		if (controllers[SOURCE] == 0)	// Audio stream
		{
			input1 = audioInput1[i];
			input2 = audioInput2[i];
		}
		else	// Sample
		{
			if (sample)
			{
				uint64_t sampleframe = getFramesFromValue (pos * controllers[NR_OF_STEPS] * controllers[STEP_SIZE]);
				input1 = sample->get (sampleframe, 0, rate);
				input2 = sample->get (sampleframe, 1, rate);
			}
		}

		audioBuffer1[audioBufferCounter] = input1;
		audioBuffer2[audioBufferCounter] = input2;

		if (controllers[PLAY] == 1.0f)	// Play
		{
			// Calculate step
			double step = fmod (pos * controllers[NR_OF_STEPS] + controllers[STEP_OFFSET], controllers[NR_OF_STEPS]);	// 0..NR_OF_STEPS position
			int iStep = step;
			int iNrOfSteps = controllers[NR_OF_STEPS];
			double fracTime = 0;					// Time from start of step
			switch (int (controllers[STEP_BASE]))
			{
				case SECONDS:	fracTime = (step - iStep) * controllers[STEP_SIZE];
						break;

				case BEATS:	fracTime = (step - iStep) * controllers[STEP_SIZE] / (bpm / 60);
						break;

				case BARS:	fracTime = (step - iStep) * controllers[STEP_SIZE] / (bpm / (60 * beatsPerBar));
						break;

				default:	break;

			}
			double fade = (fracTime < FADETIME ? fracTime / FADETIME : 1.0);

			double prevAudio1 = 0;
			double prevAudio2 = 0;
			double audio1 = 0;
			double audio2 = 0;

			// Fade out: Extrapolate audio using previous step data
			if (fade != 1.0)
			{
				int iPrevStep = (iStep + iNrOfSteps - 1) % iNrOfSteps;	// Previous step
				for (int r = 0; r < iNrOfSteps; ++r)
				{
					float factor = pads[r][iPrevStep].level;
					if (factor != 0.0)
					{
						int stepDiff = (r <= iPrevStep ? iPrevStep - r : iPrevStep + iNrOfSteps - r);
						size_t frame = size_t (maxBufferSize + audioBufferCounter - audioBufferSize * (double (stepDiff) / double (iNrOfSteps))) % maxBufferSize;
						prevAudio1 += factor * audioBuffer1[frame];
						prevAudio2 += factor * audioBuffer2[frame];

						if (editMode == 1) break;	// Only one active pad allowed in REPLACE mode
					}
				}

			}

			// Calculate audio for this step
			for (int r = 0; r < iNrOfSteps; ++r)
			{
				float factor = pads[r][iStep].level;
				if (factor != 0.0)
				{
					int stepDiff = (r <= iStep ? iStep - r : iStep + iNrOfSteps - r);
					size_t frame = size_t (maxBufferSize + audioBufferCounter - audioBufferSize * (double (stepDiff) / double (iNrOfSteps))) % maxBufferSize;
					audio1 += factor * audioBuffer1[frame];
					audio2 += factor * audioBuffer2[frame];

					if (editMode == 1) break;	// Only one active pad allowed in REPLACE mode
				}
			}

			// Mix audio and store into output
			audioOutput1[i] = fade * audio1 + (1 - fade) * prevAudio1;
			audioOutput2[i] = fade * audio2 + (1 - fade) * prevAudio2;

			waveformCounter = int ((pos + controllers[STEP_OFFSET] / controllers[NR_OF_STEPS]) * WAVEFORMSIZE) % WAVEFORMSIZE;
			waveform[waveformCounter] = (input1 + input2) / 2;
		}

		else if (controllers[PLAY] == 2.0f)	// Bypass
		{
			audioOutput1[i] = input1;
			audioOutput2[i] = input2;

			waveformCounter = int ((pos + controllers[STEP_OFFSET] / controllers[NR_OF_STEPS]) * WAVEFORMSIZE) % WAVEFORMSIZE;
			waveform[waveformCounter] = (input1 + input2) / 2;
		}

		else	// Stop
		{
			audioOutput1[i] = 0;
			audioOutput2[i] = 0;
		}

		// Increment counter
		audioBufferCounter = (audioBufferCounter + 1) % maxBufferSize;
	}
}

double BJumblr::getPositionFromBeats (const double beats) const
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

double BJumblr::getPositionFromFrames (const uint64_t frames) const
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

double BJumblr::getPositionFromSeconds (const double seconds) const
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

uint64_t BJumblr::getFramesFromValue (const double value) const
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

void BJumblr::run (uint32_t n_samples)
{
	uint32_t last_t = 0;

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
				fprintf (stderr, "BJumblr.lv2: Value out of range in run (): Controller#%i\n", i);
				*(new_controllers[i]) = val;
				// TODO update GUI controller
			}

			if (controllers[i] != val)
			{
				if ((i == SOURCE) && (val == 0.0)) message.deleteMessage (CANT_OPEN_SAMPLE);

				if (i == STEP_BASE)
				{
					if (val == SECONDS)
					{
						if (bpm < 1.0) message.setMessage (JACK_STOP_MSG);
						else message.deleteMessage (JACK_STOP_MSG);
					}
					else
					{
						if ((speed == 0) || (bpm < 1.0)) message.setMessage (JACK_STOP_MSG);
						else message.deleteMessage (JACK_STOP_MSG);
					}
				}

				controllers[i] = val;
				uint64_t size = getFramesFromValue (controllers[STEP_SIZE] * controllers[NR_OF_STEPS]);
				audioBufferSize = LIMIT (size, 0, maxBufferSize);

				// Also re-calculate waveform buffer for GUI
				if (i != PLAY)
				{
					for (size_t i = 0; i < WAVEFORMSIZE; ++i)
					{
						double di = double (i) / WAVEFORMSIZE;
						int wcount = size_t ((position + di + controllers[STEP_OFFSET] / controllers[NR_OF_STEPS]) * WAVEFORMSIZE) % WAVEFORMSIZE;
						size_t acount = (maxBufferSize + audioBufferCounter - audioBufferSize + (i * audioBufferSize) / WAVEFORMSIZE) % maxBufferSize;
						waveform[wcount] = (audioBuffer1[acount] + (audioBuffer2[acount])) / 2;
					}
					notifyWaveformToGui ((waveformCounter + 1) % WAVEFORMSIZE, waveformCounter);
				}
			}
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
				padMessageBufferAllPads ();
				scheduleNotifyPadsToGui = true;
				scheduleNotifyStatusToGui = true;
			}

			// GUI off
			else if (obj->body.otype == uris.ui_off)
			{
				ui_on = false;
			}

			// GUI pad changed notifications
			else if (obj->body.otype == uris.notify_padEvent)
			{
				LV2_Atom *oEd = NULL, *oPd = NULL;
				lv2_atom_object_get (obj,
					 	     uris.notify_editMode, &oEd,
						     uris.notify_pad, &oPd,
						     NULL);

				// EditMode notification
				if (oEd && (oEd->type == uris.atom_Int)) editMode = ((LV2_Atom_Int*)oEd)->body;

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
									fprintf (stderr, "BJumblr.lv2: Pad out of range in run (): pads[%i][%i].\n", row, step);
									padMessageBufferAppendPad (row, step, valPad);
									scheduleNotifyPadsToGui = true;
								}
							}
						}
					}
				}
			}

			// Sample path notification -> forward to worker
			else if (obj->body.otype ==uris.notify_pathEvent)
			{
				LV2_Atom* oPath = NULL;
				lv2_atom_object_get (obj, uris.notify_samplePath, &oPath, NULL);

				if (oPath && (oPath->type == uris.atom_Path))
				{
					workerSchedule->schedule_work (workerSchedule->handle, lv2_atom_total_size(&ev->body), &ev->body);
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

					// Store message
					if (((bpm < 1.0) || (speed == 0.0)) && (controllers[STEP_BASE] != SECONDS)) message.setMessage (JACK_STOP_MSG);
					else message.deleteMessage (JACK_STOP_MSG);
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
	refFrame = 0;

	if (controllers[PLAY])
	{
		cursor = int (position * controllers[NR_OF_STEPS] + controllers[STEP_OFFSET]) % int (controllers[NR_OF_STEPS]);
	}

	scheduleNotifyStatusToGui = true;
	if (waveformCounter != lastWaveformCounter) scheduleNotifyWaveformToGui = true;

	// Send notifications to GUI
	if (ui_on)
	{
		if (scheduleNotifyStatusToGui) notifyStatusToGui();
		if (scheduleNotifyPadsToGui) notifyPadsToGui();
		if (scheduleNotifyWaveformToGui) notifyWaveformToGui (lastWaveformCounter, waveformCounter);
		if (scheduleNotifySamplePathToGui) notifySamplePathToGui ();
		if (message.isScheduled ()) notifyMessageToGui();
	}
	lv2_atom_forge_pop(&notifyForge, &notifyFrame);
}

LV2_State_Status BJumblr::state_save (LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags,
			const LV2_Feature* const* features)
{
	// Store sample path
	if (sample && sample->path && (controllers[SOURCE] == 1.0))
	{
		LV2_State_Map_Path* map_path = NULL;
	        for (int i = 0; features[i]; ++i)
		{
	                if (!strcmp(features[i]->URI, LV2_STATE__mapPath))
			{
	                        map_path = (LV2_State_Map_Path*)features[i]->data;
				break;
	                }
	        }

		if (map_path)
		{
			char* abstrPath = map_path->abstract_path(map_path->handle, sample->path);
			store(handle, uris.notify_samplePath, abstrPath, strlen (sample->path) + 1, uris.atom_Path, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
			free (abstrPath);
		}
		else fprintf (stderr, "BJumblr.lv2: Feature map_path not available! Can't save sample!\n" );
	}

	// Store edit mode
	uint32_t em = editMode;
	store (handle, uris.notify_editMode, &em, sizeof(uint32_t), uris.atom_Int, LV2_STATE_IS_POD);

	// Store pads
	char padDataString[0x8010] = "\nMatrix data:\n";

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

	return LV2_STATE_SUCCESS;
}

LV2_State_Status BJumblr::state_restore (LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags,
			const LV2_Feature* const* features)
{
	size_t   size;
	uint32_t type;
	uint32_t valflags;

	// Retireve sample path
	const void* pathData = retrieve (handle, uris.notify_samplePath, &size, &type, &valflags);
        if (pathData)
	{
		message.deleteMessage (CANT_OPEN_SAMPLE);
		const char* path = (const char*)pathData;
		if (sample) delete sample;
		try {sample = new Sample (path);}
		catch (std::bad_alloc &ba)
		{
			fprintf (stderr, "BJumblr.lv2: Can't allocate enoug memory to open sample file.\n");
			message.setMessage (CANT_OPEN_SAMPLE);
		}
		catch (std::invalid_argument &ia)
		{
			fprintf (stderr, "%s\n", ia.what());
			message.setMessage (CANT_OPEN_SAMPLE);
		}
		scheduleNotifySamplePathToGui = true;
        }

	// Retrieve edit mode
	const void* modeData = retrieve (handle, uris.notify_editMode, &size, &type, &valflags);
        if (modeData && (size == sizeof (uint32_t)) && (type == uris.atom_Int))
	{
		const uint32_t mode = *(const uint32_t*) modeData;
		if ((mode < 0) || (mode > 1)) fprintf (stderr, "BJumblr.lv2: Invalid editMode data\n");
		else editMode = mode;
        }

	// Retrieve pad data
	const void* padData = retrieve(handle, uris.state_pad, &size, &type, &valflags);

	if (padData && (type == uris.atom_String))
	{
		std::string padDataString = (char*) padData;
		const std::string keywords[2] = {"id:", "lv:"};

		// Restore pads
		// Parse retrieved data
		while (!padDataString.empty())
		{
			// Look for next "id:"
			size_t strPos = padDataString.find (keywords[0]);
			size_t nextPos = 0;
			if (strPos == std::string::npos) break;	// No "id:" found => end
			if (strPos + 3 > padDataString.length()) break;	// Nothing more after id => end
			padDataString.erase (0, strPos + 3);
			int id;
			try {id = std::stof (padDataString, &nextPos);}
			catch  (const std::exception& e)
			{
				fprintf (stderr, "BJumblr.lv2: Restore pad state incomplete. Can't parse ID from \"%s...\"", padDataString.substr (0, 63).c_str());
				break;
			}

			if (nextPos > 0) padDataString.erase (0, nextPos);
			if ((id < 0) || (id >= MAXSTEPS * MAXSTEPS))
			{
				fprintf (stderr, "BJumblr.lv2: Restore pad state incomplete. Invalid matrix data block loaded with ID %i. Try to use the data before this id.\n", id);
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
					fprintf (stderr, "BJumblr.lv2: Restore padstate incomplete. Can't parse %s from \"%s...\"",
							 keywords[i].substr(0,2).c_str(), padDataString.substr (0, 63).c_str());
					break;
				}

				if (nextPos > 0) padDataString.erase (0, nextPos);
				switch (i) {
				case 1:	pads[row][step].level = val;
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
					fprintf (stderr, "BJumblr.lv2: Pad out of range in state_restore (): pads[%i][%i].\n", i, j);
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

LV2_Worker_Status BJumblr::work (LV2_Worker_Respond_Function respond, LV2_Worker_Respond_Handle handle, uint32_t size, const void* data)
{
	const LV2_Atom* atom = (const LV2_Atom*)data;

	// Free old sample
        if (atom->type == uris.notify_sampleFreeEvent)
	{
		const WorkerMessage* workerMessage = (WorkerMessage*) atom;
		if (workerMessage->sample) delete workerMessage->sample;
        }

	// Load sample
	else
	{
                const LV2_Atom_Object* obj = (const LV2_Atom_Object*)data;

		if (obj->body.otype == uris.notify_pathEvent)
		{
			const LV2_Atom* path = NULL;
			lv2_atom_object_get(obj, uris.notify_samplePath, &path, 0);

			if (path && (path->type == uris.atom_Path))
			{
				message.deleteMessage (CANT_OPEN_SAMPLE);
				Sample* s = nullptr;
				try {s = new Sample ((const char*)LV2_ATOM_BODY_CONST(path));}
				catch (std::bad_alloc &ba)
				{
					fprintf (stderr, "BJumblr.lv2: Can't allocate enough memory to open sample file.\n");
					message.setMessage (CANT_OPEN_SAMPLE);
					return LV2_WORKER_ERR_NO_SPACE;
				}
				catch (std::invalid_argument &ia)
				{
					fprintf (stderr, "%s\n", ia.what());
					message.setMessage (CANT_OPEN_SAMPLE);
					return LV2_WORKER_ERR_UNKNOWN;
				}
				if (s) respond (handle, sizeof(s), &s);
			}

			else return LV2_WORKER_ERR_UNKNOWN;
		}
        }

        return LV2_WORKER_SUCCESS;
}

LV2_Worker_Status BJumblr::work_response (uint32_t size, const void* data)
{
	// Schedule worker to free old sample
	WorkerMessage workerMessage = {{sizeof (Sample*), uris.notify_sampleFreeEvent}, sample};
	workerSchedule->schedule_work (workerSchedule->handle, sizeof (workerMessage), &workerMessage);

	// Install new sample from data
	sample = *(Sample* const*) data;
	return LV2_WORKER_SUCCESS;
}

/*
 * Checks if a value is within a limit, and if not, puts the value within
 * this limit.
 * @param value
 * @param limit
 * @return		Value is within the limit
 */
float BJumblr::validateValue (float value, const Limit limit)
{
	float ltdValue = ((limit.step != 0) ? (limit.min + round ((value - limit.min) / limit.step) * limit.step) : value);
	return LIMIT (ltdValue, limit.min, limit.max);
}

/*
 * Validates a single pad
 */
Pad BJumblr::validatePad (Pad pad)
{
	return Pad(validateValue (pad.level, {0, 1, 0}));
}

/*
 * Appends a single pad to padMessageBuffer
 */
bool BJumblr::padMessageBufferAppendPad (int row, int step, Pad pad)
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
void BJumblr::padMessageBufferAllPads ()
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

void BJumblr::notifyPadsToGui ()
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
		lv2_atom_forge_key(&notifyForge, uris.notify_editMode);
		lv2_atom_forge_int(&notifyForge, editMode);
		lv2_atom_forge_key(&notifyForge, uris.notify_pad);
		lv2_atom_forge_vector(&notifyForge, sizeof(float), uris.atom_Float, sizeof(PadMessage) / sizeof(float) * (end + 1), (void*) padMessageBuffer);
		lv2_atom_forge_pop(&notifyForge, &frame);

		// Empty padMessageBuffer
		padMessageBuffer[0] = endmsg;

		scheduleNotifyPadsToGui = false;
	}
}

void BJumblr::notifyStatusToGui ()
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

void BJumblr::notifyWaveformToGui (const int start, const int end)
{
	int p1 = (start <= end ? end : WAVEFORMSIZE - 1);

	// Notify shapeBuffer (position to end)
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(&notifyForge, 0);
	lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_waveformEvent);
	lv2_atom_forge_key(&notifyForge, uris.notify_waveformStart);
	lv2_atom_forge_int(&notifyForge, start);
	lv2_atom_forge_key(&notifyForge, uris.notify_waveformData);
	lv2_atom_forge_vector(&notifyForge, sizeof(float), uris.atom_Float, (uint32_t) (p1 + 1 - start), &waveform[start]);
	lv2_atom_forge_pop(&notifyForge, &frame);

	// Additional notification if position exceeds end
	if (start > waveformCounter)
	{
		LV2_Atom_Forge_Frame frame;
		lv2_atom_forge_frame_time(&notifyForge, 0);
		lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_waveformEvent);
		lv2_atom_forge_key(&notifyForge, uris.notify_waveformStart);
		lv2_atom_forge_int(&notifyForge, 0);
		lv2_atom_forge_key(&notifyForge, uris.notify_waveformData);
		lv2_atom_forge_vector(&notifyForge, sizeof(float), uris.atom_Float, (uint32_t) (end), &waveform[0]);
		lv2_atom_forge_pop(&notifyForge, &frame);
	}

	scheduleNotifyWaveformToGui = false;
	lastWaveformCounter = end;
}

void BJumblr::notifyMessageToGui()
{
	uint32_t messageNr = message.loadMessage ();

	// Send notifications
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(&notifyForge, 0);
	lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_messageEvent);
	lv2_atom_forge_key(&notifyForge, uris.notify_message);
	lv2_atom_forge_int(&notifyForge, messageNr);
	lv2_atom_forge_pop(&notifyForge, &frame);
}

void BJumblr::notifySamplePathToGui ()
{
	if (sample && sample->path)
	{
		LV2_Atom_Forge_Frame frame;
		lv2_atom_forge_frame_time(&notifyForge, 0);
		lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_pathEvent);
		lv2_atom_forge_key(&notifyForge, uris.notify_samplePath);
		lv2_atom_forge_path (&notifyForge, sample->path, strlen (sample->path) + 1);
		lv2_atom_forge_pop(&notifyForge, &frame);
	}

	scheduleNotifySamplePathToGui = false;
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
	BJumblr* instance;
	try {instance = new BJumblr(samplerate, features);}
	catch (std::exception& exc)
	{
		fprintf (stderr, "BJumblr.lv2: Plugin instantiation failed. %s\n", exc.what ());
		return NULL;
	}

	return (LV2_Handle)instance;
}

static void connect_port (LV2_Handle instance, uint32_t port, void *data)
{
	BJumblr* inst = (BJumblr*) instance;
	inst->connect_port (port, data);
}

static void run (LV2_Handle instance, uint32_t n_samples)
{
	BJumblr* inst = (BJumblr*) instance;
	if (inst) inst->run (n_samples);
}

static LV2_State_Status state_save(LV2_Handle instance, LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags,
           const LV2_Feature* const* features)
{
	BJumblr* inst = (BJumblr*)instance;
	if (!inst) return LV2_STATE_SUCCESS;

	return inst->state_save (store, handle, flags, features);
}

static LV2_State_Status state_restore(LV2_Handle instance, LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags,
           const LV2_Feature* const* features)
{
	BJumblr* inst = (BJumblr*)instance;
	if (!inst) return LV2_STATE_SUCCESS;

	return inst->state_restore (retrieve, handle, flags, features);
}

static LV2_Worker_Status work (LV2_Handle instance, LV2_Worker_Respond_Function respond, LV2_Worker_Respond_Handle handle,
	uint32_t size, const void* data)
{
	BJumblr* inst = (BJumblr*)instance;
	if (!inst) return LV2_WORKER_SUCCESS;

	return inst->work (respond, handle, size, data);
}

static LV2_Worker_Status work_response (LV2_Handle instance, uint32_t size,  const void* data)
{
	BJumblr* inst = (BJumblr*)instance;
	if (!inst) return LV2_WORKER_SUCCESS;

	return inst->work_response (size, data);
}

static void cleanup (LV2_Handle instance)
{
	BJumblr* inst = (BJumblr*) instance;
	if (inst) delete inst;
}


static const void* extension_data(const char* uri)
{
  static const LV2_State_Interface  state  = {state_save, state_restore};
  static const LV2_Worker_Interface worker = {work, work_response, NULL};
  if (!strcmp(uri, LV2_STATE__interface)) return &state;
  if (!strcmp(uri, LV2_WORKER__interface)) return &worker;
  return NULL;
}


static const LV2_Descriptor descriptor =
{
		BJUMBLR_URI,
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
