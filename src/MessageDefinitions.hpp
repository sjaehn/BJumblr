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

#ifndef MESSAGEDEFINITIONS_HPP_
#define MESSAGEDEFINITIONS_HPP_

#include <string>

#ifdef LOCALEFILE
#include LOCALEFILE
#else
#include "Locale_EN.hpp"
#endif

#define MAXMESSAGES 3

enum MessageNr
{
	NO_MSG			= 0,
	JACK_STOP_MSG		= 1,
	CANT_OPEN_SAMPLE	= 2
};

const std::string messageStrings[MAXMESSAGES] =
{
	"",
	BJUMBLR_LABEL_JACK_OFF,
	BJUMBLR_LABEL_CANT_OPEN_SAMPLE
};

#endif /* MESSAGEDEFINITIONS_HPP_ */
