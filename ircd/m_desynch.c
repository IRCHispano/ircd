/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_desynch.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1998 Carlo Wood <carlo@runaway.xs4all.nl>
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
 * @brief Handlers for DESYNCH command.
 * @version $Id: m_desynch.c,v 1.6 2007-04-21 21:17:23 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_bsd.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Handle a DESYNCH message from a server.
 * Broadcasts the message to opers with +g and to other servers.
 * Added 23 Apr 1998  --Run
 *
 * \a parv has the following elements:
 * \li \a parv[\a parc - 1] is the description of the desynchronization.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_desynch(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  if (parc >= 2)
    sendwallto_group(sptr, WALL_DESYNCH, cptr, "%s", parv[parc - 1]);
  else
    need_more_params(sptr,"DESYNCH");

  return 0;
}
