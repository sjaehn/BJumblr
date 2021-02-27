/* B.Jumblr
 * Pattern-controlled audio stream / sample re-sequencer LV2 plugin
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

#ifndef SF_FORMAT_MP3
#ifndef MINIMP3_IMPLEMENTATION
#define MINIMP3_IMPLEMENTATION
#endif
#endif
#include "Sample.hpp"

inline double floorfrac (const double value) {return value - floor (value);}
inline double floormod (const double numer, const double denom) {return numer - floor(numer / denom) * denom;}

BJumblr::BJumblr (double samplerate, const LV2_Feature* const* features) :
	map (NULL), unmap (NULL), workerSchedule (NULL),
	controlPort (nullptr), notifyPort (nullptr),
	audioInput1 (nullptr), audioInput2 (nullptr),
	audioOutput1 (nullptr), audioOutput2 (nullptr),
	notifyForge (), notifyFrame (),
	waveform {0.0f}, waveformCounter (0), lastWaveformCounter (0),
	new_controllers {nullptr}, controllers {0},
	editMode (0), midiLearn (false), nrPages (1),
	schedulePage (0), playPage (0), lastPage (0),
	pads {Pad()}, patternFlipped (false),
	sample (nullptr), sampleAmp (1.0f),
	rate (samplerate), bpm (120.0f), beatsPerBar (4.0f), beatUnit (0),
	speed (0.0f), bar (0), barBeat (0.0f),
	outCapacity (0), position (0.0), cursor (0.0f), offset (0.0), refFrame (0),
	progressionDelay (0), progressionDelayFrac (0),
	maxBufferSize (samplerate * 24 * 32),
	audioBuffer1 (maxBufferSize, 0.0f),
	audioBuffer2 (maxBufferSize, 0.0f),
	audioBufferCounter (0), audioBufferSize (samplerate * 8),
	activated (false),
	ui_on (false), scheduleNotifyPadsToGui (false),
	scheduleNotifyFullPatternToGui {false},
	scheduleNotifySchedulePageToGui (false),
	scheduleNotifyPlaybackPageToGui (false),
	scheduleNotifyStatusToGui (false),
	scheduleNotifyWaveformToGui (false), scheduleNotifySamplePathToGui (false),
	scheduleNotifyMidiLearnedToGui (false),
	scheduleNotifyStateChanged (false),
	message ()

{
	// Inits
	for (std::array <PadMessage, MAXSTEPS * MAXSTEPS>& p : padMessageBuffer)
	{
		p.fill ({PadMessage()});
		p[0] = PadMessage (ENDPADMESSAGE);
	};

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

	// Initialize pads
	for (int p = 0; p < MAXPAGES; ++p)
	{
		for (int i = 0; i < MAXSTEPS; ++i) pads[p][i][i].level = 1.0;
	}

	// Initialize controllers
	// Controllers are zero initialized and will get data from host, only
	// NR_OF_STEPS need to be set to prevent div by zero.
	controllers[NR_OF_STEPS] = 32;

	ui_on = false;

}

BJumblr::~BJumblr()
{
	if (sample) delete sample;
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
	int iNrOfSteps = controllers[NR_OF_STEPS];
	double delay = progressionDelay + controllers[MANUAL_PROGRSSION_DELAY];

	// Calculate start position data
	double relpos = getPositionFromFrames (start - refFrame);	// Position relative to reference frame
	double pos = floorfrac (position + relpos);			// 0..1 position
	double step = floormod (pos * controllers[NR_OF_STEPS] + controllers[STEP_OFFSET] + delay, controllers[NR_OF_STEPS]);	// 0..NR_OF_STEPS position


	for (int i = start; i < end; ++i)
	{
		int iStep = step;

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
			input1 = 0;
			input2 = 0;

			if (sample)
			{
				if (sample->end > sample->start)
				{
					const uint64_t f0 = getFramesFromValue (pos * controllers[NR_OF_STEPS] * controllers[STEP_SIZE]);
					const int64_t frame = (sample->loop ? (f0 % (sample->end - sample->start)) + sample->start : f0 + sample->start);

					if (frame < sample->end)
					{
						input1 = sampleAmp * sample->get (frame, 0, rate);
						input2 = sampleAmp * sample->get (frame, 1, rate);
					}
				}
			}
		}

		audioBuffer1[audioBufferCounter] = input1;
		audioBuffer2[audioBufferCounter] = input2;

		if (controllers[PLAY] == 1.0f)	// Play
		{
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
			if (fade < 1.0)
			{
				// Begin of step: Page change scheduled ?
				if ((fade < 0.1) && (schedulePage != playPage))
				{
					playPage = schedulePage;
					scheduleNotifyPlaybackPageToGui = true;
					scheduleNotifyStateChanged = true;
				}

				int iPrevStep = (fade < 1.0 ? (iStep + iNrOfSteps - 1) % iNrOfSteps : iStep);	// Previous step
				for (int r = 0; r < iNrOfSteps; ++r)
				{
					float factor = pads[lastPage][r][iPrevStep].level;
					if (factor != 0.0)
					{
						int stepDiff = floormod (iPrevStep - r - delay, iNrOfSteps);
						size_t frame = size_t (maxBufferSize + audioBufferCounter - audioBufferSize * (double (stepDiff) / double (iNrOfSteps))) % maxBufferSize;
						prevAudio1 += factor * audioBuffer1[frame];
						prevAudio2 += factor * audioBuffer2[frame];

						if (editMode == 1) break;	// Only one active pad allowed in REPLACE mode
					}
				}

			}

			else lastPage = playPage;

			// Calculate audio for this step
			for (int r = 0; r < iNrOfSteps; ++r)
			{
				float factor = pads[playPage][r][iStep].level;
				if (factor != 0.0)
				{
					int stepDiff = floormod (iStep - r - delay, iNrOfSteps);
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

		// Calculate next position
		relpos = getPositionFromFrames (i + 1 - refFrame);
		pos = floorfrac (position + relpos);
		step = floormod (pos * controllers[NR_OF_STEPS] + controllers[STEP_OFFSET] + delay, controllers[NR_OF_STEPS]);

		// Change step ? Update delaySteps
		int nextiStep = step;
		if (nextiStep != iStep)
		{
			progressionDelayFrac += controllers[SPEED] - 1;
			double floorDelayFrac = floor (progressionDelayFrac);
			progressionDelay += floorDelayFrac;
			progressionDelayFrac -= floorDelayFrac;
			scheduleNotifyStatusToGui = true;
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

void BJumblr::activate() {activated = true;}

void BJumblr::deactivate() {activated = false;}

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
				if (i == SOURCE)
				{
					if (val == 0.0) message.deleteMessage (CANT_OPEN_SAMPLE);
				}

				else if (i == STEP_BASE)
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

				else if (i == PAGE)
				{
					schedulePage = val;
					scheduleNotifySchedulePageToGui = true;
				}

				controllers[i] = val;
				uint64_t size = getFramesFromValue (controllers[STEP_SIZE] * controllers[NR_OF_STEPS]);
				audioBufferSize = LIMIT (size, 0, maxBufferSize);

				// Also re-calculate waveform buffer for GUI
				if ((i == SOURCE) || (i == NR_OF_STEPS) || (i == STEP_BASE) || (i == STEP_SIZE) || (i == STEP_OFFSET))
				{
					for (size_t i = 0; i < WAVEFORMSIZE; ++i)
					{
						double di = double (i) / WAVEFORMSIZE;
						int wcount = size_t ((position + di + controllers[STEP_OFFSET] / controllers[NR_OF_STEPS]) * WAVEFORMSIZE) % WAVEFORMSIZE;
						size_t acount = (maxBufferSize + audioBufferCounter - audioBufferSize + (i * audioBufferSize) / WAVEFORMSIZE) % maxBufferSize;
						waveform[wcount] = (audioBuffer1[acount] + (audioBuffer2[acount])) / 2;
					}
					if (ui_on) notifyWaveformToGui ((waveformCounter + 1) % WAVEFORMSIZE, waveformCounter);
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
				for (int i = 0; i < nrPages; ++i) scheduleNotifyFullPatternToGui[i] = true;
				scheduleNotifyPadsToGui = true;
				scheduleNotifyStatusToGui = true;
				scheduleNotifySamplePathToGui = true;
			}

			// GUI off
			else if (obj->body.otype == uris.ui_off)
			{
				ui_on = false;
			}

			// GUI pad changed notifications
			else if (obj->body.otype == uris.notify_padEvent)
			{
				LV2_Atom *oEd = NULL, *oPg = NULL, *oPd = NULL, *oPat = NULL;
				int page = -1;
				lv2_atom_object_get (obj,
					 	     uris.notify_editMode, &oEd,
						     uris.notify_padPage, &oPg,
						     uris.notify_pad, &oPd,
						     uris.notify_padFullPattern, &oPat,
						     NULL);

				// EditMode notification
				if (oEd && (oEd->type == uris.atom_Int)) editMode = ((LV2_Atom_Int*)oEd)->body;

				// padPage notification
				if (oPg && (oPg->type == uris.atom_Int))
				{
					page = ((LV2_Atom_Int*)oPg)->body;
					if (page >= nrPages)
					{
						nrPages = LIMIT (page + 1, 1, MAXPAGES);
						if (playPage >= nrPages)
						{
							schedulePage = nrPages - 1;
							scheduleNotifySchedulePageToGui = true;
						}
					}
				}

				// Pad notification
				if (oPd && (oPd->type == uris.atom_Vector) && (page >= 0) && (page < MAXPAGES))
				{
					const LV2_Atom_Vector* vec = (const LV2_Atom_Vector*) oPd;
					if (vec->body.child_type == uris.atom_Float)
					{
						const uint32_t size = (uint32_t) ((oPd->size - sizeof(LV2_Atom_Vector_Body)) / sizeof (PadMessage));
						PadMessage* pMes = (PadMessage*) (&vec->body + 1);

						for (unsigned int i = 0; i < size; ++i)
						{
							int row = pMes[i].row;
							int step = pMes[i].step;

							// Copy PadMessages to pads
							if ((row >= 0) && (row < MAXSTEPS) && (step >= 0) && (step < MAXSTEPS))
							{
								Pad pd (pMes[i].level);
								Pad valPad = validatePad (pd);
								pads[page][row][step] = valPad;
								if (valPad != pd)
								{
									fprintf (stderr, "BJumblr.lv2: Pad out of range in run (): pads[%i][%i][%i].\n", page, row, step);
									padMessageBufferAppendPad (page, row, step, valPad);
									scheduleNotifyPadsToGui = true;
								}
								scheduleNotifyStateChanged = true;
							}
						}
					}
				}

				// Full pattern notification
				if (oPat && (oPat->type == uris.atom_Vector) && (page >= 0) && (page < MAXPAGES))
				{
					const LV2_Atom_Vector* vec = (const LV2_Atom_Vector*) oPat;
					if (vec->body.child_type == uris.atom_Float)
					{
						const uint32_t size = (uint32_t) ((oPat->size - sizeof(LV2_Atom_Vector_Body)) / sizeof (Pad));
						Pad* data = (Pad*) (&vec->body + 1);

						if (size == MAXSTEPS * MAXSTEPS)
						{
							// Copy pattern data
							for (int r = 0; r < MAXSTEPS; ++r)
							{
								for (int s = 0; s < MAXSTEPS; ++s)
								{
									pads[page][r][s] = data[r * MAXSTEPS + s];
								}
							}

							scheduleNotifyStateChanged = true;
						}

						else fprintf (stderr, "BJumblr.lv2: Corrupt pattern size of %i for page %i.\n", size, page);
					}
				}
			}

			// Status notifications
			else if (obj->body.otype == uris.notify_statusEvent)
			{
				LV2_Atom *oMx = NULL, *oPp = NULL, *oMl = NULL, *oFlip = NULL;
				lv2_atom_object_get (obj,
					 	     uris.notify_maxPage, &oMx,
						     uris.notify_playbackPage, &oPp,
						     uris.notify_requestMidiLearn, &oMl,
						     uris.notify_padFlipped, &oFlip,
						     NULL);

				// padMaxPage notification
				if (oMx && (oMx->type == uris.atom_Int))
				{
					int newPages = ((LV2_Atom_Int*)oMx)->body;
					if (newPages != nrPages)
					{
						nrPages = LIMIT (newPages, 1, MAXPAGES);
						if (playPage >= nrPages)
						{
							schedulePage = nrPages - 1;
							scheduleNotifyPlaybackPageToGui = true;
						}
					}
				}

				// playbackPage notification
				if (oPp && (oPp->type == uris.atom_Int))
				{
					int newPp = ((LV2_Atom_Int*)oPp)->body;
					if (newPp != playPage)
					{
						playPage = LIMIT (newPp, 0, MAXPAGES - 1);
						scheduleNotifyStateChanged = true;
					}
				}

				// Midi learn request notification
				if (oMl && (oMl->type == uris.atom_Bool)) midiLearn = ((LV2_Atom_Bool*)oMl)->body;

				// Pattern orientation
				if (oFlip && (oFlip->type == uris.atom_Bool))
				{
					patternFlipped = ((LV2_Atom_Bool*)oFlip)->body;
					scheduleNotifyStateChanged = true;
				}
			}

			// Sample path notification -> forward to worker
			else if (obj->body.otype ==uris.notify_pathEvent)
			{
				LV2_Atom* oPath = NULL, *oStart = NULL, *oEnd = NULL, *oAmp = NULL, *oLoop = NULL;
				lv2_atom_object_get
				(
					obj,
					uris.notify_samplePath, &oPath,
					uris.notify_sampleStart, &oStart,
					uris.notify_sampleEnd, &oEnd,
					uris.notify_sampleAmp, &oAmp,
					uris.notify_sampleLoop, &oLoop,
					NULL
				);

				// New sample
				if (oPath && (oPath->type == uris.atom_Path))
				{
					workerSchedule->schedule_work (workerSchedule->handle, lv2_atom_total_size(&ev->body), &ev->body);
				}

				// Only start / end /amp / loop changed
				else if (sample)
				{
					if (oStart && (oStart->type == uris.atom_Long)) sample->start = LIMIT (((LV2_Atom_Long*)oStart)->body, 0, sample->info.frames - 1);
					if (oEnd && (oEnd->type == uris.atom_Long)) sample->end = LIMIT (((LV2_Atom_Long*)oEnd)->body, 0, sample->info.frames);
					if (oAmp && (oAmp->type == uris.atom_Float)) sampleAmp = LIMIT (((LV2_Atom_Float*)oAmp)->body, 0.0f, 1.0f);
					if (oLoop && (oLoop->type == uris.atom_Bool)) sample->loop = bool(((LV2_Atom_Bool*)oLoop)->body);
					scheduleNotifyStateChanged = true;
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

		// Read incoming MIDI events
		else if (ev->body.type == uris.midi_Event)
		{
			const uint8_t* const msg = (const uint8_t*)(ev + 1);
			const uint8_t status = (msg[0] >> 4);
			const uint8_t channel = msg[0] & 0x0F;
			const uint8_t note = ((status == 8) || (status == 9) || (status == 11) ? msg[1] : 0);
			const uint8_t value = ((status == 8) || (status == 9) || (status == 11) ? msg[2] : 0);

			if (midiLearn)
			{
				midiLearned[0] = status;
				midiLearned[1] = channel;
				midiLearned[2] = note;
				midiLearned[3] = value;
				midiLearn = false;
				scheduleNotifyMidiLearnedToGui = true;
			}

			else
			{

				for (int p = 0; p < nrPages; ++p)
				{
					if
					(
						controllers[MIDI + p * NR_MIDI_CTRLS + STATUS] &&
						(controllers[MIDI + p * NR_MIDI_CTRLS + STATUS] == status) &&
						(
							(controllers[MIDI + p * NR_MIDI_CTRLS + CHANNEL] == 0) ||
							(controllers[MIDI + p * NR_MIDI_CTRLS + CHANNEL] - 1 == channel)
						) &&
						(
							(controllers[MIDI + p * NR_MIDI_CTRLS + NOTE] == 128) ||
							(controllers[MIDI + p * NR_MIDI_CTRLS + NOTE] == note)
						) &&
						(
							(controllers[MIDI + p * NR_MIDI_CTRLS + VALUE] == 128) ||
							(controllers[MIDI + p * NR_MIDI_CTRLS + VALUE] == value)
						)
					)
					{
						schedulePage = p;
						scheduleNotifySchedulePageToGui = true;
						break;
					}
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

		cursor = floormod
		(
			position * controllers[NR_OF_STEPS] + controllers[STEP_OFFSET] + progressionDelay + controllers[MANUAL_PROGRSSION_DELAY],
			controllers[NR_OF_STEPS]
		);
		scheduleNotifyStatusToGui = true;
	}

	if (waveformCounter != lastWaveformCounter) scheduleNotifyWaveformToGui = true;

	if (ui_on)
	{
		if (scheduleNotifyStatusToGui) notifyStatusToGui();
		if (scheduleNotifyPadsToGui) notifyPadsToGui();
		if (scheduleNotifyWaveformToGui) notifyWaveformToGui (lastWaveformCounter, waveformCounter);
		if (scheduleNotifySamplePathToGui) notifySamplePathToGui ();
		if (scheduleNotifySchedulePageToGui) notifySchedulePageToGui ();
		if (scheduleNotifyPlaybackPageToGui) notifyPlaybackPageToGui ();
		if (scheduleNotifyMidiLearnedToGui) notifyMidiLearnedToGui ();
		if (message.isScheduled ()) notifyMessageToGui();
	}

	if (scheduleNotifyStateChanged) notifyStateChanged();
	lv2_atom_forge_pop(&notifyForge, &notifyFrame);
}

LV2_State_Status BJumblr::state_save (LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags,
			const LV2_Feature* const* features)
{
	// Store sample path
	if (sample && sample->path && (sample->path[0] != 0) && (controllers[SOURCE] == 1.0))
	{
		LV2_State_Map_Path* mapPath = NULL;
#ifdef LV2_STATE__freePath
		LV2_State_Free_Path* freePath = NULL;
#endif

		for (int i = 0; features[i]; ++i)
		{
			if (strcmp(features[i]->URI, LV2_URID__map) == 0)
			{
				mapPath = (LV2_State_Map_Path*) features[i]->data;
				break;
			}
		}

#ifdef LV2_STATE__freePath
		for (int i = 0; features[i]; ++i)
		{
			if (strcmp(features[i]->URI, LV2_STATE__freePath) == 0)
			{
				freePath = (LV2_State_Free_Path*) features[i]->data;
				break;
			}
		}
#endif

		if (mapPath)
		{
			char* abstrPath = mapPath->abstract_path(mapPath->handle, sample->path);

			if (abstrPath)
			{
				fprintf(stderr, "BJumblr.lv2: Save abstr_path:%s\n", abstrPath);
				store(handle, uris.notify_samplePath, abstrPath, strlen (abstrPath) + 1, uris.atom_Path, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
				store(handle, uris.notify_sampleStart, &sample->start, sizeof (sample->start), uris.atom_Long, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
				store(handle, uris.notify_sampleEnd, &sample->end, sizeof (sample->end), uris.atom_Long, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
				store(handle, uris.notify_sampleAmp, &sampleAmp, sizeof (sampleAmp), uris.atom_Float, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
				const int32_t sloop = int32_t (sample->loop);
				store(handle, uris.notify_sampleLoop, &sloop, sizeof (sloop), uris.atom_Bool, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);

#ifdef LV2_STATE__freePath
				if (freePath) freePath->free_path (freePath->handle, abstrPath);
				else
#endif
				{
					free (abstrPath);
				}
			}

			else fprintf(stderr, "BJumblr.lv2: Can't generate abstr_path from %s\n", sample->path);
		}
		else
		{
			fprintf (stderr, "BJumblr.lv2: Feature map_path not available! Can't save sample!\n" );
			return LV2_STATE_ERR_NO_FEATURE;
		}
	}

	// Store pattern orientation
	store(handle, uris.notify_padFlipped, &patternFlipped, sizeof (patternFlipped), uris.atom_Bool, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);

	// Store playbackPage
	uint32_t pp = playPage;
	store (handle, uris.notify_playbackPage, &pp, sizeof(uint32_t), uris.atom_Int, LV2_STATE_IS_POD);

	// Store edit mode
	uint32_t em = editMode;
	store (handle, uris.notify_editMode, &em, sizeof(uint32_t), uris.atom_Int, LV2_STATE_IS_POD);

	// Store pads
	char padDataString[0x80100] = "\nMatrix data:\n";

	for (int page = 0; page < nrPages; ++page)
	{
		for (int step = 0; step < MAXSTEPS; ++step)
		{
			for (int row = 0; row < MAXSTEPS; ++row)
			{
				Pad* pd = &pads[page][row][step];
				if (*pd != Pad())
				{
					char valueString[64];
					int id = step * MAXSTEPS + row;
					snprintf (valueString, 62, "pg:%d; id:%d; lv:%f", page, id, pd->level);
					if ((step < MAXSTEPS - 1) || (row < MAXSTEPS)) strcat (valueString, ";\n");
					else strcat(valueString, "\n");
					strcat (padDataString, valueString);
				}
			}
		}
	}
	store (handle, uris.state_pad, padDataString, strlen (padDataString) + 1, uris.atom_String, LV2_STATE_IS_POD);

	return LV2_STATE_SUCCESS;
}

LV2_State_Status BJumblr::state_restore (LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags,
			const LV2_Feature* const* features)
{
	// Get host features
	LV2_Worker_Schedule* schedule = nullptr;
	LV2_State_Map_Path* mapPath = nullptr;
#ifdef LV2_STATE__freePath
	LV2_State_Free_Path* freePath = NULL;
#endif

	for (int i = 0; features[i]; ++i)
	{
		if (strcmp(features[i]->URI, LV2_URID__map) == 0)
		{
			mapPath = (LV2_State_Map_Path*) features[i]->data;
			break;
		}
	}

	for (int i = 0; features[i]; ++i)
	{
		if (strcmp(features[i]->URI, LV2_WORKER__schedule) == 0)
		{
			schedule = (LV2_Worker_Schedule*) features[i]->data;
			break;
		}
	}

#ifdef LV2_STATE__freePath
	for (int i = 0; features[i]; ++i)
	{
		if (strcmp(features[i]->URI, LV2_STATE__freePath) == 0)
		{
			freePath = (LV2_State_Free_Path*) features[i]->data;
			break;
		}
	}
#endif

	if (!mapPath)
	{
		fprintf (stderr, "BJumblr.lv2: Host doesn't support required features.\n");
		return LV2_STATE_ERR_NO_FEATURE;
	}

	size_t   size;
	uint32_t type;
	uint32_t valflags;

	// Retireve sample data
	{
		char samplePath[PATH_MAX] = {0};
		int64_t sampleStart = 0;
		int64_t sampleEnd = 0;
		float sampleAmp = 1.0;
		int32_t sampleLoop = false;

		const void* pathData = retrieve (handle, uris.notify_samplePath, &size, &type, &valflags);
		if (pathData)
		{
			char* absPath  = mapPath->absolute_path (mapPath->handle, (char*)pathData);

		        if (absPath)
			{
				if (strlen (absPath) < PATH_MAX) strcpy (samplePath, absPath);
				else
				{
					fprintf (stderr, "BJumblr.lv2: Sample path too long.\n");
					message.setMessage (CANT_OPEN_SAMPLE);
				}

				fprintf(stderr, "BJumblr.lv2: Restore abs_path:%s\n", absPath);

#ifdef LV2_STATE__freePath
				if (freePath) freePath->free_path (freePath->handle, absPath);
				else
#endif
				{
					free (absPath);
				}
		        }
		}

		const void* startData = retrieve (handle, uris.notify_sampleStart, &size, &type, &valflags);
	        if (startData && (type == uris.atom_Long)) sampleStart = *(int64_t*)startData;
		const void* endData = retrieve (handle, uris.notify_sampleEnd, &size, &type, &valflags);
	        if (endData && (type == uris.atom_Long)) sampleEnd = *(int64_t*)endData;
		const void* ampData = retrieve (handle, uris.notify_sampleAmp, &size, &type, &valflags);
	        if (ampData && (type == uris.atom_Float)) sampleAmp = *(float*)ampData;
		const void* loopData = retrieve (handle, uris.notify_sampleLoop, &size, &type, &valflags);
	        if (loopData && (type == uris.atom_Bool)) sampleLoop = *(int32_t*)loopData;

		if (activated && schedule)
		{
			LV2_Atom_Forge forge;
			lv2_atom_forge_init(&forge, map);
			uint8_t buf[1200];
			lv2_atom_forge_set_buffer(&forge, buf, sizeof(buf));
			LV2_Atom_Forge_Frame frame;
			LV2_Atom* msg = (LV2_Atom*)forgeSamplePath (&forge, &frame, samplePath, sampleStart, sampleEnd, sampleAmp, sampleLoop);
			lv2_atom_forge_pop(&forge, &frame);
			if (msg) schedule->schedule_work(schedule->handle, lv2_atom_total_size(msg), msg);
		}

		else
		{
			// Free old sample
			if (sample)
			{
				delete sample;
				sample = nullptr;
				sampleAmp = 1.0;
			}

			// Load new sample
			message.deleteMessage (CANT_OPEN_SAMPLE);
			try {sample = new Sample (samplePath);}
			catch (std::bad_alloc &ba)
			{
				fprintf (stderr, "Jumblr.lv2: Can't allocate enoug memory to open sample file.\n");
				message.setMessage (CANT_OPEN_SAMPLE);
			}
			catch (std::invalid_argument &ia)
			{
				fprintf (stderr, "%s\n", ia.what());
				message.setMessage (CANT_OPEN_SAMPLE);
			}

			// Set new sample properties
			if  (sample)
			{
				sample->start = sampleStart;
				sample->end = sampleEnd;
				sample->loop = bool (sampleLoop);
				this->sampleAmp = sampleAmp;
			}

			scheduleNotifySamplePathToGui = true;
		}
	}

	// Retrieve pattern orientation
	const void* flipData = retrieve (handle, uris.notify_padFlipped, &size, &type, &valflags);
        if (flipData && (type == uris.atom_Bool))
	{
		patternFlipped = *(bool*) flipData;
		scheduleNotifyStatusToGui = true;
        }

	// Retrieve playbackPage
	const void* ppData = retrieve (handle, uris.notify_playbackPage, &size, &type, &valflags);
        if (ppData && (size == sizeof (uint32_t)) && (type == uris.atom_Int))
	{
		const uint32_t pp = *(const uint32_t*) ppData;
		if ((pp < 0) || (pp >= MAXPAGES)) fprintf (stderr, "BJumblr.lv2: Invalid playbackPage data\n");
		else playPage = pp;
		scheduleNotifyPlaybackPageToGui = true;
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
		nrPages = 1;
		for (int i = 0; i < MAXPAGES; ++i)
		{
			for (int j = 0; j < MAXSTEPS; ++j)
			{
				for (int k = 0; k < MAXSTEPS; ++k)
				{
					pads[i][j][k] = Pad();
				}
			}
		}

		std::string padDataString = (char*) padData;
		const std::string keywords[3] = {"pg:", "id:", "lv:"};

		// Restore pads
		// Parse retrieved data
		while (!padDataString.empty())
		{
			// Look for optional "pg:"
			int page = 0;
			size_t strPos = padDataString.find (keywords[0]);
			size_t nextPos = 0;
			if ((strPos != std::string::npos) && (strPos + 3 <= padDataString.length()))
			{
				padDataString.erase (0, strPos + 3);
				int p;
				try {p = std::stof (padDataString, &nextPos);}
				catch  (const std::exception& e)
				{
					fprintf (stderr, "BJumblr.lv2: Restore pad state incomplete. Can't parse page from \"%s...\"", padDataString.substr (0, 63).c_str());
					break;
				}

				if (nextPos > 0) padDataString.erase (0, nextPos);
				if ((p < 0) || (p >= MAXPAGES))
				{
					fprintf (stderr, "BJumblr.lv2: Restore pad state incomplete. Invalid matrix data block loaded with page %i. Try to use the data before this page.\n", p);
					break;
				}
				if (p >= nrPages) nrPages = p + 1;
				page = p;
			}

			// Look for "id:"
			strPos = padDataString.find (keywords[1]);
			nextPos = 0;
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
			for (int i = 2; i < 3; ++i)
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
				case 2:	pads[page][row][step].level = val;
					break;
				default:break;
				}
			}
		}


		// Validate all pads
		for (int p = 0; p < nrPages; ++p)
		{
			for (int i = 0; i < MAXSTEPS; ++i)
			{
				for (int j = 0; j < MAXSTEPS; ++j)
				{
					Pad valPad = validatePad (pads[p][i][j]);
					if (valPad != pads[p][i][j])
					{
						fprintf (stderr, "BJumblr.lv2: Pad out of range in state_restore (): pads[%i][%i][%i].\n", p, i, j);
						pads[p][i][j] = valPad;
					}
				}
			}

			scheduleNotifyFullPatternToGui[p] = true;
		}

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
	if (!atom) return LV2_WORKER_ERR_UNKNOWN;

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
			const LV2_Atom* path = NULL, *oStart = NULL, *oEnd = NULL, *oAmp = NULL, *oLoop = NULL;
			lv2_atom_object_get
			(
				obj,
				uris.notify_samplePath, &path,
				uris.notify_sampleStart, &oStart,
				uris.notify_sampleEnd, &oEnd,
				uris.notify_sampleAmp, &oAmp,
				uris.notify_sampleLoop, &oLoop,
				0
			);

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

				if (s)
				{
					WorkerMessage sAtom;
					sAtom.atom = {sizeof (s), uris.notify_installSample};
					sAtom.sample = s;
					sAtom.start = (oStart && (oStart->type == uris.atom_Long) ? ((LV2_Atom_Long*)oStart)->body : 0);
					sAtom.end = (oEnd && (oEnd->type == uris.atom_Long) ? ((LV2_Atom_Long*)oEnd)->body : s->info.frames);
					sAtom.amp = (oAmp && (oAmp->type == uris.atom_Float) ? ((LV2_Atom_Float*)oAmp)->body : 1.0f);
					sAtom.loop = (oLoop && (oLoop->type == uris.atom_Bool) ? ((LV2_Atom_Bool*)oLoop)->body : 0);
					respond (handle, sizeof(sAtom), &sAtom);
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
	const LV2_Atom* atom = (const LV2_Atom*)data;
	if (!atom) return LV2_WORKER_ERR_UNKNOWN;

	if (atom->type == uris.notify_installSample)
	{
		const WorkerMessage* nAtom = (const WorkerMessage*)data;
		// Schedule worker to free old sample
		WorkerMessage sAtom = {{sizeof (Sample*), uris.notify_sampleFreeEvent}, sample};
		workerSchedule->schedule_work (workerSchedule->handle, sizeof (sAtom), &sAtom);

		// Install new sample from data
		sample = nAtom->sample;
		if (sample)
		{
			sample->start = LIMIT (nAtom->start, 0, sample->info.frames - 1);
			sample->end = LIMIT (nAtom->end, sample->start, sample->info.frames);
			sampleAmp = LIMIT (nAtom->amp, 0.0f, 1.0f);
			sample->loop = bool (nAtom->loop);
			scheduleNotifyStateChanged = true;
			return LV2_WORKER_SUCCESS;
		}

		else
		{
			scheduleNotifyStateChanged = true;
			return LV2_WORKER_ERR_UNKNOWN;
		}
	}

	else return LV2_WORKER_ERR_UNKNOWN;
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
bool BJumblr::padMessageBufferAppendPad (int page, int row, int step, Pad pad)
{
	PadMessage end = PadMessage (ENDPADMESSAGE);
	PadMessage msg = PadMessage (step, row, pad.level);

	for (int i = 0; i < MAXSTEPS * MAXSTEPS; ++i)
	{
		if (padMessageBuffer[page][i] != end)
		{
			padMessageBuffer[page][i] = msg;
			if (i < MAXSTEPS * MAXSTEPS - 1) padMessageBuffer[page][i + 1] = end;
			return true;
		}
	}
	return false;
}

LV2_Atom_Forge_Ref BJumblr::forgeSamplePath (LV2_Atom_Forge* forge, LV2_Atom_Forge_Frame* frame, const char* path, const int64_t start, const int64_t end, const float amp, const int32_t loop)
{
	const LV2_Atom_Forge_Ref msg = lv2_atom_forge_object (forge, frame, 0, uris.notify_pathEvent);
	if (msg)
	{
		lv2_atom_forge_key (forge, uris.notify_samplePath);
		lv2_atom_forge_path (forge, path, strlen (path) + 1);
		lv2_atom_forge_key (forge, uris.notify_sampleStart);
		lv2_atom_forge_long (forge, start);
		lv2_atom_forge_key (forge, uris.notify_sampleEnd);
		lv2_atom_forge_long (forge, end);
		lv2_atom_forge_key (forge, uris.notify_sampleAmp);
		lv2_atom_forge_float (forge, amp);
		lv2_atom_forge_key (forge, uris.notify_sampleLoop);
		lv2_atom_forge_bool (forge, loop);
	}
	return msg;
}

void BJumblr::notifyPadsToGui ()
{
	PadMessage endmsg (ENDPADMESSAGE);

	for (int p = 0; p < nrPages; ++p)
	{

		LV2_Atom_Forge_Frame frame;
		lv2_atom_forge_frame_time(&notifyForge, 0);
		lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_padEvent);
		lv2_atom_forge_key(&notifyForge, uris.notify_editMode);
		lv2_atom_forge_int(&notifyForge, editMode);
		lv2_atom_forge_key(&notifyForge, uris.notify_padPage);
		lv2_atom_forge_int(&notifyForge, p);

		if (scheduleNotifyFullPatternToGui[p])
		{
			lv2_atom_forge_key(&notifyForge, uris.notify_padFullPattern);
			lv2_atom_forge_vector(&notifyForge, sizeof(float), uris.atom_Float, MAXSTEPS * MAXSTEPS * sizeof(Pad) / sizeof(float), (void*) &pads[p]);
		}

		else if (!(endmsg == padMessageBuffer[p][0]))
		{

			// Get padMessageBuffer size
			int end = 0;
			for (int i = 0; (i < MAXSTEPS * MAXSTEPS) && (!(padMessageBuffer[p][i] == endmsg)); ++i) end = i;

			lv2_atom_forge_key(&notifyForge, uris.notify_pad);
			lv2_atom_forge_vector(&notifyForge, sizeof(float), uris.atom_Float, sizeof(PadMessage) / sizeof(float) * (end + 1), (void*) &padMessageBuffer[p]);
		}

		lv2_atom_forge_pop(&notifyForge, &frame);

		// Empty padMessageBuffer
		padMessageBuffer[p][0] = endmsg;

		scheduleNotifyFullPatternToGui[p] = false;
	}

	scheduleNotifyPadsToGui = false;
}

void BJumblr::notifyStatusToGui ()
{
	// Prepare forge buffer and initialize atom sequence
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(&notifyForge, 0);
	lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_statusEvent);
	lv2_atom_forge_key(&notifyForge, uris.notify_cursor);
	lv2_atom_forge_float(&notifyForge, cursor);
	float delay = progressionDelay + controllers[MANUAL_PROGRSSION_DELAY];
	lv2_atom_forge_key(&notifyForge, uris.notify_progressionDelay);
	lv2_atom_forge_float(&notifyForge, delay);
	lv2_atom_forge_key(&notifyForge, uris.notify_padFlipped);
	lv2_atom_forge_bool(&notifyForge, patternFlipped);
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

void BJumblr::notifySchedulePageToGui ()
{
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(&notifyForge, 0);
	lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_statusEvent);
	lv2_atom_forge_key(&notifyForge, uris.notify_schedulePage);
	lv2_atom_forge_int(&notifyForge, schedulePage);
	lv2_atom_forge_pop(&notifyForge, &frame);

	scheduleNotifySchedulePageToGui = false;
}

void BJumblr::notifyPlaybackPageToGui ()
{
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(&notifyForge, 0);
	lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_statusEvent);
	lv2_atom_forge_key(&notifyForge, uris.notify_playbackPage);
	lv2_atom_forge_int(&notifyForge, playPage);
	lv2_atom_forge_pop(&notifyForge, &frame);

	scheduleNotifyPlaybackPageToGui = false;
}

void BJumblr::notifyMidiLearnedToGui ()
{
	uint32_t ml = midiLearned[0] * 0x1000000 + midiLearned[1] * 0x10000 + midiLearned[2] * 0x100 + midiLearned[3];
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(&notifyForge, 0);
	lv2_atom_forge_object(&notifyForge, &frame, 0, uris.notify_statusEvent);
	lv2_atom_forge_key(&notifyForge, uris.notify_midiLearned);
	lv2_atom_forge_int(&notifyForge, ml);
	lv2_atom_forge_pop(&notifyForge, &frame);

	scheduleNotifyMidiLearnedToGui = false;
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

		if (sample && sample->path && (sample->path[0] != 0))
		{
			forgeSamplePath (&notifyForge, &frame, sample->path, sample->start, sample->end, sampleAmp, int32_t (sample->loop));
		}
		else
		{
			const char* path = ".";
			forgeSamplePath (&notifyForge, &frame, path, 0, 0, sampleAmp, false);
		}

		lv2_atom_forge_pop(&notifyForge, &frame);
	}

	scheduleNotifySamplePathToGui = false;
}

void BJumblr::notifyStateChanged()
{
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(&notifyForge, 0);
	lv2_atom_forge_object(&notifyForge, &frame, 0, uris.state_StateChanged);
	lv2_atom_forge_pop(&notifyForge, &frame);
	scheduleNotifyStateChanged = false;
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

static void activate (LV2_Handle instance)
{
	BJumblr* inst = (BJumblr*) instance;
	if (inst) inst->activate();
}

static void run (LV2_Handle instance, uint32_t n_samples)
{
	BJumblr* inst = (BJumblr*) instance;
	if (inst) inst->run (n_samples);
}

static void deactivate (LV2_Handle instance)
{
	BJumblr* inst = (BJumblr*) instance;
	if (inst) inst->deactivate();
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
		activate,
		run,
		deactivate,
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
