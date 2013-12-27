/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_xquery.c
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
 * @brief Handlers for XQUERY command.
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
#include "send.h"

#include <string.h>

/*
 * m_xquery - extension message handler
 *
 * parv[0] = sender prefix
 * parv[1] = target server
 * parv[2] = routing information
 * parv[3] = extension message
 */
int mo_xquery(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Client* acptr;

  if (parc < 4) /* have enough parameters? */
    return need_more_params(sptr, "XQUERY");

  /* Look up the target server */
  if (!(acptr = find_match_server(parv[1])))
    return send_reply(sptr, ERR_NOSUCHSERVER, parv[1]);

  /* If it's to us, do nothing; otherwise, forward the query */
  if (!IsMe(acptr))
    sendcmdto_one(sptr, CMD_XQUERY, acptr, "%C %s :%s", acptr, parv[2],
		  parv[3]);

  return 0;
}

/*
 * ms_xquery - extension message handler
 *
 * parv[0] = sender prefix
 * parv[1] = target server numeric
 * parv[2] = routing information
 * parv[3] = extension message
 */
int ms_xquery(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Client* acptr;

  if (parc < 4) /* have enough parameters? */
    return need_more_params(sptr, "XQUERY");

  /* Look up the target server */
  if (!(acptr = FindNServer(parv[1])))
    return send_reply(sptr, SND_EXPLICIT | ERR_NOSUCHSERVER,
		      "* :Server has disconnected");

  /* Forward the query to its destination */
  if (!IsMe(acptr))
    sendcmdto_one(sptr, CMD_XQUERY, acptr, "%C %s :%s", acptr, parv[2],
		  parv[3]);
  else /* if it's to us, log it */
    log_write(LS_SYSTEM, L_NOTICE, 0, "Received extension query from "
	      "%#C to %#C routing %s; message: %s", sptr, acptr,
	      parv[2], parv[3]);

  return 0;
}

