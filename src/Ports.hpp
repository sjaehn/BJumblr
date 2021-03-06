/* B.Jumblr
 * Pattern-controlled audio stream / sample re-sequencer LV2 plugin
 *
 * Copyright (C) 2018 - 2020 by Sven Jähnichen
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

#ifndef PORTS_HPP_
#define PORTS_HPP_

enum PortIndex {
	CONTROL			= 0,
	NOTIFY			= 1,
	AUDIO_IN_1		= 2,
	AUDIO_IN_2		= 3,
	AUDIO_OUT_1		= 4,
	AUDIO_OUT_2		= 5,

	CONTROLLERS		= 6,
	SOURCE			= 0,
	PLAY			= 1,
	NR_OF_STEPS		= 2,
	STEP_BASE		= 3,
	STEP_SIZE		= 4,
	STEP_OFFSET		= 5,
	MANUAL_PROGRSSION_DELAY	= 6,
	SPEED			= 7,
	PAGE			= 8,

	MIDI			= 9,
	STATUS			= 0,
	CHANNEL			= 1,
	NOTE			= 2,
	VALUE			= 3,
	NR_MIDI_CTRLS		= 4,
	MAXCONTROLLERS		= MIDI + 16 * NR_MIDI_CTRLS
};

enum BaseIndex
{
	SECONDS		= 0,
	BEATS		= 1,
	BARS		= 2
};

#endif /* PORTS_HPP_ */
