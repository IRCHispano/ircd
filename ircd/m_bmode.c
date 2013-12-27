/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_bmode.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2004-2005 Toni Garcis (zoltan) zoltan@irc-dev.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/** @file
 * @brief Handlers for BMODE command (Bot Mode).
 * @version $Id: m_bmode.c,v 1.3 2007-04-21 16:20:18 zolty Exp $
 */
#include "config.h"

#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_reply.h"

#include <stdlib.h>

/** Handle a BMODE message from another server.
 *
 * \a parv[1] is a name of virtual bot.
 * \a parv[2] is a channel.
 * \a parv[3] is a modes.
 * \a parv[4..n] is a parameters.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int
ms_bmode(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Channel *chptr;
  struct ModeBuf mbuf;

  if (parc < 4)
    return need_more_params(sptr, "BMODE");

  if (!(chptr = FindChannel(parv[2])))
    return protocol_violation(sptr, "Attemped to set BMODE on inexistant channel (%s)", parv[2]);

  if (IsLocalChannel(chptr->chname))
    return protocol_violation(sptr, "Attemped to set BMODE on local channel");

  modebuf_init(&mbuf, sptr, cptr, chptr,
               (MODEBUF_DEST_CHANNEL | /* Send mode to clients */
                MODEBUF_DEST_SERVER  | /* Send mode to servers */
  	        MODEBUF_DEST_BOTMODE)); /* Botmode */

  mbuf.mb_botname = parv[1];

  mode_parse(&mbuf, cptr, sptr, chptr, parc - 3, parv + 3,
             (MODE_PARSE_SET    | /* Set the mode */
              MODE_PARSE_STRICT | /* Interpret it strictly */
	      MODE_PARSE_FORCE),  /* And force it to be accepted */
              NULL);
  return modebuf_flush(&mbuf);
}
