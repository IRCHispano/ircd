/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_ison.c
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1991 Darren Reed
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
 * @brief Handlers for ISON command.
 * @version $Id: m_ison.c,v 1.6 2007-04-19 22:53:48 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msgq.h"
#include "numeric.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <string.h>

/** Handle an ISON request from a local connection.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the comma-separated list of nicknames to check
 *
 * Added by Darren Reed 13/8/91 to act as an efficient user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
 * format:
 * ISON :nicklist
 *
 * XXX - this is virtually the same as send_user_info, but doesn't send
 * no nick found, might be refactored so that m_userhost, m_userip, and
 * m_ison all use the same function with different formatters. --Bleep
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_ison(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Client* acptr;
  char*          name;
  char*          p = 0;
  struct MsgBuf* mb;
  int i;

  if (parc < 2)
    return need_more_params(sptr, "ISON");

  mb = msgq_make(sptr, rpl_str(RPL_ISON), cli_name(&me), cli_name(sptr));

  for (i = 1; i < parc; i++) {
    for (name = ircd_strtok(&p, parv[i], " "); name;
	 name = ircd_strtok(&p, 0, " ")) {
      if ((acptr = FindUser(name))) {
	if (msgq_bufleft(mb) < strlen(cli_name(acptr)) + 1) {
	  send_buffer(sptr, mb, 0); /* send partial response */
	  msgq_clean(mb); /* then do another round */
	  mb = msgq_make(sptr, rpl_str(RPL_ISON), cli_name(&me),
			 cli_name(sptr));
	}
	msgq_append(0, mb, "%s ", cli_name(acptr));
      }
    }
  }

  send_buffer(sptr, mb, 0); /* send response */
  msgq_clean(mb);

  return 0;
}
