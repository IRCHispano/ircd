/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_wallusers.c
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Handlers for WALLUSERS command.
 * @version $Id: m_wallusers.c,v 1.7 2008-01-19 19:26:03 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "send.h"
#include "ircd_features.h" /* TRANSICION IRC-HISPANO */
#include "ircd.h"  /* TRANSICION IRC-HISPANO */

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Handle a WALLUSERS message from a server.
 *
 * \a parv has the following elements:
 * \li \a parv[\a parc - 1] is the message to send
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_wallusers(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  char *message;

  message = parc > 1 ? parv[parc - 1] : 0;

  /*
   * XXX - PROTOCOL ERROR (shouldn't happen)
   */
  if (EmptyString(message))
    return need_more_params(sptr, "WALLUSERS");

  sendwallto_group(sptr, WALL_WALLUSERS, cptr, "%s", message);
  return 0;
}

/** Handle a WALLUSERS message from an operator.
 *
 * \a parv has the following elements:
 * \li \a parv[\a parc - 1] is the message to send
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int mo_wallusers(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  char *message;

  message = parc > 1 ? parv[parc - 1] : 0;

  if (EmptyString(message))
    return need_more_params(sptr, "WALLUSERS");

  sendwallto_group(sptr, WALL_WALLUSERS, 0, "%s", message);
  return 0;
}
