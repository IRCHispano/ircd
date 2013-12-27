/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_wallvoices.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (c) 2002 hikari
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
 * @brief Handlers for WALLVOICES command.
 * @version $Id: m_wallvoices.c,v 1.13 2008-01-19 19:26:07 zolty Exp $
 */
#include "config.h"

#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_user.h"
#include "send.h"
#include "ircd_features.h" /* TRANSICION IRC-HISPANO */
#include "ircd.h"  /* TRANSICION IRC-HISPANO */


/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Handle a WALLVOICES message from a local client.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the name of the channel to which to send
 * \li \a parv[\a parc - 1] is the message to send
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_wallvoices(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Channel *chptr;
  const char *ch;

  assert(0 != cptr);
  assert(cptr == sptr);

  if (parc < 2 || EmptyString(parv[1]))
    return send_reply(sptr, ERR_NORECIPIENT, "WALLVOICES");

  if (parc < 3 || EmptyString(parv[parc - 1]))
    return send_reply(sptr, ERR_NOTEXTTOSEND);

  if (IsChannelName(parv[1]) && (chptr = FindChannel(parv[1]))) {
    if (client_can_send_to_channel(sptr, chptr, 0) && !(chptr->mode.mode & MODE_NONOTICE)) {
      if ((chptr->mode.mode & MODE_NOPRIVMSGS) &&
          check_target_limit(sptr, chptr, chptr->chname, 0))
        return 0;

#if 0
      /* +cC checks */
      if (chptr->mode.mode & MODE_NOCOLOUR)
        for (ch=parv[parc - 1];*ch;ch++)
          if (*ch==2 || *ch==3 || *ch==22 || *ch==27 || *ch==31) {
            return 0;
          }

      if ((chptr->mode.mode & MODE_NOCTCP) && ircd_strncmp(parv[parc - 1],"\001ACTION ",8))
        for (ch=parv[parc - 1];*ch;)
          if (*ch++==1) {
            return 0;
          }
#endif
      RevealDelayedJoinIfNeeded(sptr, chptr);
      sendcmdto_channel(sptr, CMD_WALLVOICES, chptr, cptr,
                        SKIP_DEAF | SKIP_BURST | SKIP_NONVOICES,
                        "%H :+ %s", chptr, parv[parc - 1]);
    }
    else
      send_reply(sptr, ERR_CANNOTSENDTOCHAN, parv[1]);
  }
  else
    send_reply(sptr, ERR_NOSUCHCHANNEL, parv[1]);

  return 0;
}

/** Handle a WALLVOICES message from a server.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the name of the channel to which to send
 * \li \a parv[\a parc - 1] is the message to send
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_wallvoices(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Channel *chptr;
  assert(0 != cptr);
  assert(0 != sptr);

  if (parc < 3 || !IsUser(sptr))
    return 0;

  if (!IsLocalChannel(parv[1]) && (chptr = FindChannel(parv[1]))) {
    if (client_can_send_to_channel(sptr, chptr, 1)) {
      sendcmdto_channel(sptr, CMD_WALLVOICES, chptr, cptr,
                        SKIP_DEAF | SKIP_BURST | SKIP_NONVOICES,
                        "%H :+ %s", chptr, parv[parc - 1]);
    } else
      send_reply(sptr, ERR_CANNOTSENDTOCHAN, parv[1]);
  }
  return 0;
}
