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

#ifndef MESSAGE_HPP_
#define MESSAGE_HPP_

#include <cstdint>
#include "MessageDefinitions.hpp"

class Message
{
public:
	Message () : messageBits (0), scheduled (true) {}

	void clearMessages ()
	{
		messageBits = 0;
		scheduled = true;
	}

	void setMessage (MessageNr messageNr)
	{
		if ((messageNr > NO_MSG) && (messageNr < MAXMESSAGES) && (!isMessage (messageNr)))
		{
			messageBits = messageBits | (1 << (messageNr - 1));
			scheduled = true;
		}
	}

	void deleteMessage (MessageNr messageNr)
	{
		if ((messageNr > NO_MSG) && (messageNr < MAXMESSAGES) && (isMessage (messageNr)))
		{
			messageBits = messageBits & (~(1 << (messageNr - 1)));
			scheduled = true;
		}
	}

	bool isMessage (MessageNr messageNr)
	{
		if ((messageNr > NO_MSG) && (messageNr < MAXMESSAGES)) return ((messageBits & (1 << (messageNr - 1))) != 0);
		else if (messageNr == NO_MSG) return (messageBits == 0);
		else return false;
	}

	MessageNr loadMessage ()
	{
		scheduled = false;
		for (int i = NO_MSG + 1; i < MAXMESSAGES; ++i)
		{
			MessageNr messageNr = MessageNr (i);
			if (isMessage (messageNr)) return messageNr;
		}
		return NO_MSG;
	}

	bool isScheduled () {return scheduled;}
private:
	uint32_t messageBits;
	bool scheduled;
};

#endif /* MESSAGE_HPP_ */
