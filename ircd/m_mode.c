/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_mode.c
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
 * @brief Handlers for MODE command.
 * @version $Id: m_mode.c,v 1.18 2008-01-19 19:26:03 zolty Exp $
 */
#include "config.h"

#include "handlers.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_conf.h"
#include "s_debug.h"
#include "s_user.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdlib.h>
#include <string.h>

/** Handle a MODE message from a local client.
 *
 * \a parv has the following elements:
 * \a parv[1] is the name of the channel to modify
 * \a parv[2..\a parc - 1] (optional) are the modes and parameters to
 *   apply to the channel
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int
m_mode(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Channel *chptr = 0;
  struct ModeBuf mbuf;
  struct Membership *member;

  if (parc < 2)
    return need_more_params(sptr, "MODE");

  if (!IsChannelName(parv[1]) || !(chptr = FindChannel(parv[1])))
  {
    struct Client *acptr;

    acptr = FindUser(parv[1]);
    if (!acptr)
    {
      send_reply(sptr, ERR_NOSUCHCHANNEL, parv[1]);
      return 0;
    }
    else if (sptr != acptr)
    {
      send_reply(sptr, ERR_USERSDONTMATCH);
      return 0;
    }
    return set_user_mode(cptr, sptr, parc, parv, ALLOWMODES_ANY);
  }

  member = find_member_link(chptr, sptr);

  if (parc < 3) {
    char modebuf[MODEBUFLEN];
    char parabuf[MODEBUFLEN];

    *modebuf = *parabuf = '\0';
    modebuf[1] = '\0';
    channel_modes(sptr, modebuf, parabuf, sizeof(parabuf), chptr, member);
    send_reply(sptr, RPL_CHANNELMODEIS, chptr->chname, modebuf, parabuf);
    send_reply(sptr, RPL_CREATIONTIME, chptr->chname, chptr->creationtime);
    return 0;
  }

#if defined(DDB) || defined(SERVICES)
  if (!member || !(IsChanOwner(member) || IsChanOp(member))) {
#else
  if (!member || !IsChanOp(member)) {
#endif
    if (IsLocalChannel(chptr->chname) && HasPriv(sptr, PRIV_MODE_LCHAN)) {
      modebuf_init(&mbuf, sptr, cptr, chptr,
		   (MODEBUF_DEST_CHANNEL | /* Send mode to channel */
		    MODEBUF_DEST_HACK4));  /* Send HACK(4) notice */
      mode_parse(&mbuf, cptr, sptr, chptr, parc - 2, parv + 2,
		 (MODE_PARSE_SET |    /* Set the mode */
		  MODE_PARSE_FORCE),  /* Force it to take */
		  member);
      return modebuf_flush(&mbuf);

    } else
      mode_parse(0, cptr, sptr, chptr, parc - 2, parv + 2,
		 (member ? MODE_PARSE_NOTOPER : MODE_PARSE_NOTMEMBER), member);
    return 0;
  }

  modebuf_init(&mbuf, sptr, cptr, chptr,
	       (MODEBUF_DEST_CHANNEL | /* Send mode to channel */
		MODEBUF_DEST_SERVER)); /* Send mode to servers */
  mode_parse(&mbuf, cptr, sptr, chptr, parc - 2, parv + 2, MODE_PARSE_SET, member);
  return modebuf_flush(&mbuf);
}

/** Handle a MODE message from a server.
 *
 * \a parv has the following elements:
 * \a parv[1] is the name of the channel to modify
 * \a parv[2..\a parc - N] are the modes and parameters to apply to
 *   the channel
 * \a parv[\a parc - 1] (optional) is the channel timestamp
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int
ms_mode(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Channel *chptr = 0;
  struct ModeBuf mbuf;
  struct Membership *member;

  if (parc < 3)
    return need_more_params(sptr, "MODE");

  if (IsLocalChannel(parv[1]))
    return 0;

  if (!(chptr = FindChannel(parv[1])))
  {
    struct Client *acptr;

    acptr = FindUser(parv[1]);
    if (!acptr)
    {
      return 0;
    }
    else if (sptr != acptr)
    {
      sendwallto_group(&me, WALL_WALLOPS, 0,
                       "MODE for User %s from %s!%s", parv[1],
                       cli_name(cptr), cli_name(sptr));
      return 0;
    }
    return set_user_mode(cptr, sptr, parc, parv, ALLOWMODES_ANY);
  }

  if (IsServer(sptr)) {
    if (cli_uworld(sptr))
      modebuf_init(&mbuf, sptr, cptr, chptr,
		   (MODEBUF_DEST_CHANNEL | /* Send mode to clients */
		    MODEBUF_DEST_SERVER  | /* Send mode to servers */
		    MODEBUF_DEST_HACK4));  /* Send a HACK(4) message */
#if defined(UNDERNET)
    else if (!feature_bool(FEAT_OPLEVELS))
      modebuf_init(&mbuf, sptr, cptr, chptr,
		   (MODEBUF_DEST_CHANNEL | /* Send mode to clients */
		    MODEBUF_DEST_SERVER  | /* Send mode to servers */
		    MODEBUF_DEST_HACK3));  /* Send a HACK(3) message */
    else
      /* Servers need to be able to op people who join using the Apass
       * or upass, as well as people joining a zannel, therefore we do
       * not generate HACK3 when oplevels are on. */
      modebuf_init(&mbuf, sptr, cptr, chptr,
		   (MODEBUF_DEST_CHANNEL | /* Send mode to clients */
		    MODEBUF_DEST_SERVER));   /* Send mode to servers */
#else
    else
      modebuf_init(&mbuf, sptr, cptr, chptr,
		   (MODEBUF_DEST_CHANNEL | /* Send mode to clients */
		    MODEBUF_DEST_SERVER  | /* Send mode to servers */
		    MODEBUF_DEST_HACK3));  /* Send a HACK(3) message */
#endif

    mode_parse(&mbuf, cptr, sptr, chptr, parc - 2, parv + 2,
	       (MODE_PARSE_SET    | /* Set the mode */
		MODE_PARSE_STRICT | /* Interpret it strictly */
		MODE_PARSE_FORCE),  /* And force it to be accepted */
	        NULL);
  } else {
#if defined(DDB) || defined(SERVICES)
    if (IsService(cli_user(sptr)->server)) {
      modebuf_init(&mbuf, sptr, cptr, chptr,
                   (MODEBUF_DEST_CHANNEL | /* Send mode to clients */
                    MODEBUF_DEST_SERVER)); /* Send mode to servers */
      mode_parse(&mbuf, cptr, sptr, chptr, parc - 2, parv + 2,
                 (MODE_PARSE_SET    | /* Set the mode */
                  MODE_PARSE_STRICT | /* Interpret it strictly */
                  MODE_PARSE_FORCE),  /* And force it to be accepted */
                  NULL);
    } else if (!(member = find_member_link(chptr, sptr)) || !(IsChanOwner(member)
        || IsChanOp(member))) {
#else
    if (!(member = find_member_link(chptr, sptr)) || !IsChanOp(member)) {
#endif
      modebuf_init(&mbuf, sptr, cptr, chptr,
		   (MODEBUF_DEST_SERVER |  /* Send mode to server */
		    MODEBUF_DEST_HACK2  |  /* Send a HACK(2) message */
		    MODEBUF_DEST_DEOP   |  /* Deop the source */
		    MODEBUF_DEST_BOUNCE)); /* And bounce the MODE */
      mode_parse(&mbuf, cptr, sptr, chptr, parc - 2, parv + 2,
		 (MODE_PARSE_STRICT |  /* Interpret it strictly */
		  MODE_PARSE_BOUNCE),  /* And bounce the MODE */
		  member);
    } else {
      modebuf_init(&mbuf, sptr, cptr, chptr,
		   (MODEBUF_DEST_CHANNEL | /* Send mode to clients */
		    MODEBUF_DEST_SERVER)); /* Send mode to servers */
      mode_parse(&mbuf, cptr, sptr, chptr, parc - 2, parv + 2,
		 (MODE_PARSE_SET    | /* Set the mode */
		  MODE_PARSE_STRICT | /* Interpret it strictly */
		  MODE_PARSE_FORCE),  /* And force it to be accepted */
		  member);
    }
  }

  return modebuf_flush(&mbuf);
}
