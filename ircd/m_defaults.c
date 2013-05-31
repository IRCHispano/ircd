/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_defaults.c
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
 * @brief 
 * @version $Id: m_defaults.c,v 1.7 2007-09-20 21:00:32 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "numeric.h"
#include "numnicks.h"
#include "send.h"
#include "version.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Inform a client he is not opered.
 * \a parv is ignored.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_not_oper(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  return send_reply(cptr, ERR_NOPRIVILEGES);
}

/** Inform a client he must be registered first.
 * \a parv is ignored.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_unregistered(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  send_reply(cptr, SND_EXPLICIT | ERR_NOTREGISTERED, "%s :Register first.",
	     parv[0]);
  return 0;
}

/** Inform a client he is already registered.
 * \a parv is ignored.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_registered(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  return send_reply(sptr, ERR_ALREADYREGISTRED);
}

/** Ignore a command entirely.
 * \a parv is ignored.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_ignore(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  return 0;
}
