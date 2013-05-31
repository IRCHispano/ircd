/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_set.c
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2000 Kevin L. Mitchell <klmitch@mit.edu>
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
 * @brief Handlers for the SET command.
 * @version $Id: m_set.c,v 1.5 2007-04-19 22:53:49 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "numeric.h"
#include "numnicks.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Handle a SET message from an operator
 *
 * \a parv normally has the following elements:
 * \li \a parv[1] is the Feature setting to change
 * \li \a parv[2] is the new value for the setting
 *
 * If \a parv[1] is "LOG" and \a parc is less than 4, \a parv has the
 * following elements:
 * \li \a parv[2] (optional) is the default syslog facility to use
 *   (one of "NONE", "DEFAULT", "AUTH", possibly "AUTHPRIV", "CRON",
 *   "DAEMON", "LOCAL0" through "LOCAL7", "LPR", "MAIL", "NEWS",
 *   "USER" * or "UUCP")
 *
 * Otherwise, if \a parv[1] is "LOG", \a parv has the following
 * elements:
 * \li \a parv[2] is the log subsystem
 * \li \a parv[3] is the parameter type ("FILE", "FACILITY", "SNOMASK"
 *   or "LEVEL") to set
 * \li \a parv[4] (optional) is the new value for the option
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int mo_set(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  return feature_set(sptr, (const char* const*)parv + 1, parc - 1);
}
