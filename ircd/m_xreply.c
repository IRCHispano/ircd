/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_xreply.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2010 Kevin L. Mitchell <klmitch@mit.edu>
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
 * @brief Handlers for XREPLY command.
 * @version $Id: m_whowas.c,v 1.11 2007-04-26 21:17:12 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_auth.h"
#include "send.h"

#include <string.h>

/*
 * ms_xreply - extension message reply handler
 *
 * parv[0] = sender prefix
 * parv[1] = target server numeric
 * parv[2] = routing information
 * parv[3] = extension message reply
 */
int ms_xreply(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Client* acptr;
  const char* routing;
  const char* reply;

  if (parc < 4) /* have enough parameters? */
    return need_more_params(sptr, "XREPLY");

  routing = parv[2];
  reply = parv[3];

  /* Look up the target */
  if (!(acptr = findNUser(parv[1])) && !(acptr = FindNServer(parv[1])))
    return send_reply(sptr, SND_EXPLICIT | ERR_NOSUCHSERVER,
		      "* :Server has disconnected");

  /* If it's not to us, forward the reply */
  if (!IsMe(acptr)) {
    sendcmdto_one(sptr, CMD_XREPLY, acptr, "%C %s :%s", acptr, routing,
		  reply);
    return 0;
  }

  /* OK, figure out where to route the message */
  if (!ircd_strncmp("iauth:", routing, 6)) {
    /* Forward the reply to the iauth */
    routing += 6;

    auth_send_xreply(sptr, routing, reply);
  } else
    /* If we don't know where to route it, log it and drop it */
    log_write(LS_SYSTEM, L_NOTICE, 0, "Received unroutable extension reply "
	      "from %#C to %#C routing %s; message: %s", sptr, acptr,
	      routing, reply);

  return 0;
}

