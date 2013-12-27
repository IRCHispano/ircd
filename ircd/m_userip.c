/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_userip.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1996 Carlo Wood <carlo@runaway.xs4all.nl>
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
 * @brief Handlers for the USERIP command.
 * @version $Id: m_userip.c,v 1.11 2007-09-20 21:00:32 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "msgq.h"
#include "numeric.h"
#include "s_user.h"
#include "struct.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Prepare one client's IP information for another client.
 * @param[in] cptr Client whose information should be displayed
 * @param[in] sptr Client who should get the information
 * @param[in,out] mb Message buffer to append display to.
 */
static void userip_formatter(struct Client* cptr, struct Client *sptr, struct MsgBuf* mb)
{
  assert(IsUser(cptr));
  msgq_append(0, mb, "%s%s=%c%s@%s", cli_name(cptr),
	      SeeOper(sptr,cptr) ? "*" : "",
	      cli_user(cptr)->away ? '-' : '+', cli_user(cptr)->username,
	      /* Do not *EVER* change this to give opers the real IP.
	       * Too many scripts rely on this data and can inadvertently
	       * publish the user's real IP, thus breaking the security
	       * of +x.  If an oper wants the real IP, he should go to
	       * /whois to get it.
	       */
	      HasHiddenHost(cptr) && !IsViewHiddenHost(sptr) && (sptr != cptr) ?
	      feature_str(FEAT_HIDDEN_IP) :
	      ircd_ntoa(&cli_ip(cptr)));
}

/** Handle a USERIP message from a local client.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is a comma-separated list of nicknames to search for
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_userip(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  assert(0 != cptr);
  assert(cptr == sptr);

  if (parc < 2)
    return need_more_params(sptr, "USERIP");
  send_user_info(sptr, parv[1], RPL_USERIP, userip_formatter);
  return 0;
}
