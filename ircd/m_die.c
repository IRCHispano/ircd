/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_die.c
 *
 * Copyright (C) 2002-2015 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Handlers for DIE command.
 * @version $Id: m_die.c,v 1.5 2007-04-19 22:53:48 zolty Exp $
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
#include "s_bsd.h"
#include "s_user.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */


/** Handle a DIE message from a server connection.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the target server, or "*" for all.
 * \li \a parv[2] is either "cancel" or a time interval in seconds
 * \li \a parv[\a parc - 1] is the reason
 *
 * All fields must be present.  Additionally, the time interval should
 * not be 0 for messages sent to "*", as that may not function
 * reliably due to buffering in the server.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
*/
int ms_die(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  const char *target, *when, *reason;

  if (parc < 4)
    return need_more_params(sptr, "DIE");

  target = parv[1];
  when = parv[2];
  reason = parv[parc - 1];

  /* is it a message we should pay attention to? */
  if (target[0] != '*' || target[1] != '\0') {
    if (hunt_server_cmd(sptr, CMD_DIE, cptr, 0, "%C %s :%s", 1, parc, parv)
    != HUNTED_ISME)
      return 0;
  } else /* must forward the message */
    sendcmdto_serv(sptr, CMD_DIE, cptr, "* %s :%s", when, reason);

  /* OK, the message has been forwarded, but before we can act... */
  if (!feature_bool(FEAT_NETWORK_DIE))
    return 0;

  /* is it a cancellation? */
  if (!ircd_strcmp(when, "cancel"))
    exit_cancel(sptr); /* cancel a pending exit */
  else /* schedule an exit */
    exit_schedule(0, atoi(when), sptr, reason);

  return 0;
}
 
/** Handle a DIE message from an operator.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is either "cancel" or a time interval in seconds
 * \li \a parv[\a parc - 1] is the reason
 *
 * Either the time interval or the reason (or both) may be omitted.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int mo_die(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  time_t when = 0;
  const char *reason = 0;

  if (!HasPriv(sptr, PRIV_DIE))
    return send_reply(sptr, ERR_NOPRIVILEGES);

  if (parc > 1 && !ircd_strcmp(parv[1], "cancel")) {
    exit_cancel(sptr); /* cancel a pending exit */
    return 0;
  } else if (parc > 2) { /* have both time and reason */
    when = atoi(parv[1]);
    reason = parv[parc - 1];
  } else if (parc > 1 && !(when = atoi(parv[1])))
    reason = parv[parc - 1];

  /* now, let's schedule the exit */
  exit_schedule(0, when, sptr, reason);

  return 0;
}
