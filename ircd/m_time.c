/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_time.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Handlers for the TIME command.
 * @version $Id: m_time.c,v 1.6 2007-04-19 22:53:49 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_misc.h"
#include "s_user.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Handle a TIME message from some connection.
 *
 * \a parv has the following elements:
 * \li \a parv[1] (optional) is the server to query
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_time(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  if (hunt_server_cmd(sptr, CMD_TIME, cptr, feature_int(FEAT_HIS_REMOTE), ":%C",
                      1, parc, parv)
      != HUNTED_ISME)
    return 0;

  send_reply(sptr, RPL_TIME, cli_name(&me), TStime(), TSoffset, date((long)0));
  return 0;
}
