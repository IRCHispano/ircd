/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_version.c
 *
 * Copyright (C) 2002-2017 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1990 Jarkko Oikarinen
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
 * @brief Handlers for the VERSION command.
 * @version $Id: m_version.c,v 1.12 2008-01-19 13:28:53 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_snprintf.h"
#include "ircd_string.h"
#include "match.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_debug.h"
#include "s_user.h"
#include "send.h"
#include "version.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Handle a VERSION message from a normal client.
 *
 * \a parv has the following elements:
 * \li \a parc[1] is the server to query (must be me)
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_version(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  if (parc > 1 && match(parv[1], cli_name(&me)))
    return send_reply(sptr, ERR_NOPRIVILEGES);

  send_reply(sptr, RPL_VERSION, version, debugmode, cli_name(&me),
             debug_serveropts());
  send_supported(sptr);
  return 0;
}

/** Handle a VERSION message from an operator.
 *
 * \a parv has the following elements:
 * \li \a parc[1] is the server to query
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
/* TODO-ZOLTAN: Reducir a uno solo (Undernet) */
int mo_version(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Client *acptr;

  if (MyConnect(sptr) && parc > 1)
  {
    if (!(acptr = find_match_server(parv[1])))
    {
      send_reply(sptr, ERR_NOSUCHSERVER, parv[1]);
      return 0;
    }
    parv[1] = cli_name(acptr);
  }

  if (hunt_server_cmd(sptr, CMD_VERSION, cptr, feature_int(FEAT_HIS_REMOTE),
                                                           ":%C", 1,
                                                           parc, parv)
                      == HUNTED_ISME)
  {
    send_reply(sptr, RPL_VERSION, version, debugmode, cli_name(&me),
	       debug_serveropts());
    if (MyUser(sptr))
      send_supported(sptr);
  }

  return 0;
}

/** Handle a VERSION message from a server.
 *
 * \a parv has the following elements:
 * \li \a parc[1] is the server to query
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_version(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Client *acptr;

  if (MyConnect(sptr) && parc > 1)
  {
    if (!(acptr = find_match_server(parv[1])))
    {
      send_reply(sptr, ERR_NOSUCHSERVER, parv[1]);
      return 0;
    }
    parv[1] = cli_name(acptr);
  }

  if (hunt_server_cmd(sptr, CMD_VERSION, cptr, 0, ":%C", 1, parc, parv) ==
      HUNTED_ISME)
  {
    send_reply(sptr, RPL_VERSION, version, debugmode, cli_name(&me),
	       debug_serveropts());
  }

  return 0;
}
