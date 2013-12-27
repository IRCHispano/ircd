/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_clearmode.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Handlers for CLEARMODE command.
 * @version $Id: m_clearmode.c,v 1.16 2008-01-19 19:26:03 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "channel.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "list.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_conf.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Clear a channel's modes.
 * @param[in] cptr The local client sending the CLEARMODE message.
 * @param[in] sptr The original source of the CLEARMODE message.
 * @param[in] chptr The channel being affected.
 * @param[in] control Null-terminated array of modes to clear.
 * @return Zero.
 */
static int
do_clearmode(struct Client *cptr, struct Client *sptr, struct Channel *chptr,
	     char *control)
{
  static int flags[] = {
    MODE_CHANOP,	'o',
    MODE_VOICE,		'v',
    MODE_PRIVATE,	'p',
    MODE_SECRET,	's',
    MODE_MODERATED,	'm',
    MODE_TOPICLIMIT,	't',
    MODE_INVITEONLY,	'i',
    MODE_NOPRIVMSGS,	'n',
    MODE_KEY,		'k',
    MODE_BAN,		'b',
    MODE_LIMIT,		'l',
#if defined(UNDERNET)
    MODE_REGONLY,	'r',
#elif defined(DDB) || defined(SERVICES)
    MODE_REGONLY,       'R',
#endif
    MODE_DELJOINS,      'D',
    MODE_NOQUITPARTS,   'u',
    MODE_NOCOLOUR,      'c',
    MODE_NOCTCP,        'C',
    MODE_NONOTICE,      'N',
    MODE_MODERATENOREG, 'M',
    MODE_SSLONLY,       'z',
    0x0, 0x0
  };
  int *flag_p;
  unsigned int del_mode = 0;
  char control_buf[20];
  int control_buf_i = 0;
  struct ModeBuf mbuf;
  struct Ban *link, *next;
  struct Membership *member;

  /* Ok, so what are we supposed to get rid of? */
  for (; *control; control++) {
    for (flag_p = flags; flag_p[0]; flag_p += 2)
      if (*control == flag_p[1]) {
	del_mode |= flag_p[0];
	break;
      }
  }

  if (!del_mode)
    return 0; /* nothing to remove; ho hum. */

  modebuf_init(&mbuf, sptr, cptr, chptr,
	       (MODEBUF_DEST_CHANNEL | /* Send MODE to channel */
		MODEBUF_DEST_OPMODE  | /* Treat it like an OPMODE */
		MODEBUF_DEST_HACK4));  /* Generate a HACK(4) notice */

  modebuf_mode(&mbuf, MODE_DEL | (del_mode & chptr->mode.mode));
  chptr->mode.mode &= ~del_mode; /* and of course actually delete them */

  /* If we're removing invite, remove all the invites */
  if (del_mode & MODE_INVITEONLY)
    mode_invite_clear(chptr);

  /*
   * If we're removing the key, note that; note that we can't clear
   * the key until after modebuf_* are done with it
   */
  if (del_mode & MODE_KEY && *chptr->mode.key)
    modebuf_mode_string(&mbuf, MODE_DEL | MODE_KEY, chptr->mode.key, 0);

  /* If we're removing the limit, note that and clear the limit */
  if (del_mode & MODE_LIMIT && chptr->mode.limit) {
    modebuf_mode_uint(&mbuf, MODE_DEL | MODE_LIMIT, chptr->mode.limit);
    chptr->mode.limit = 0; /* not referenced, so safe */
  }

  /*
   * Go through and mark the bans for deletion; note that we can't
   * free them until after modebuf_* are done with them
   */
  if (del_mode & MODE_BAN) {
    for (link = chptr->banlist; link; link = next) {
      char *bandup;
      next = link->next;

      DupString(bandup, link->banstr);
      modebuf_mode_string(&mbuf, MODE_DEL | MODE_BAN, /* delete ban */
			  bandup, 1);
      free_ban(link);
    }

    chptr->banlist = 0;
  }

  /* Deal with users on the channel */
  if (del_mode & (MODE_BAN | MODE_CHANOP | MODE_VOICE))
    for (member = chptr->members; member; member = member->next_member) {
      if (IsZombie(member)) /* we ignore zombies */
	continue;

      if (del_mode & MODE_BAN) /* If we cleared bans, clear the valid flags */
	ClearBanValid(member);

      /* Drop channel operator status */
      if (IsChanOp(member) && del_mode & MODE_CHANOP) {
#if defined(UNDERNET)
	modebuf_mode_client(&mbuf, MODE_DEL | MODE_CHANOP, member->user, MAXOPLEVEL + 1);
#else
	modebuf_mode_client(&mbuf, MODE_DEL | MODE_CHANOP, member->user, 0);
#endif
	member->status &= ~CHFL_CHANOP;
      }

      /* Drop voice */
      if (HasVoice(member) && del_mode & MODE_VOICE) {
#if defined(UNDERNET)
	modebuf_mode_client(&mbuf, MODE_DEL | MODE_VOICE, member->user, MAXOPLEVEL + 1);
#else
	modebuf_mode_client(&mbuf, MODE_DEL | MODE_VOICE, member->user, 0);
#endif
	member->status &= ~CHFL_VOICE;
      }
    }

  /* And flush the modes to the channel */
  modebuf_flush(&mbuf);

  /* Finally, we can clear the key... */
  if (del_mode & MODE_KEY)
    chptr->mode.key[0] = '\0';

  /* Ok, build control string again */
  for (flag_p = flags; flag_p[0]; flag_p += 2)
    if (del_mode & flag_p[0])
      control_buf[control_buf_i++] = flag_p[1];

  control_buf[control_buf_i] = '\0';

  /* Log it... */
  log_write(LS_OPERMODE, L_INFO, LOG_NOSNOTICE, "%#C CLEARMODE %H %s", sptr,
	    chptr, control_buf);

  /* Then send it */
  if (!IsLocalChannel(chptr->chname))
    sendcmdto_serv(sptr, CMD_CLEARMODE, cptr, "%H %s", chptr, control_buf);

  return 0;
}

/** Handle a CLEARMODE message from a server.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the channel to affect
 * \li \a parv[2] is the channel modes to clear
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int
ms_clearmode(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Channel *chptr;

  if (parc < 3)
    return need_more_params(sptr, "CLEARMODE");

  if (!IsPrivileged(sptr)) {
    protocol_violation(sptr,"No privileges on source for CLEARMODE, desync?");
    return send_reply(sptr, ERR_NOPRIVILEGES);
  }

  if (!IsChannelName(parv[1]) || IsLocalChannel(parv[1]) ||
      !(chptr = FindChannel(parv[1])))
    return send_reply(sptr, ERR_NOSUCHCHANNEL, parv[1]);

  return do_clearmode(cptr, sptr, chptr, parv[2]);
}

/** Handle a CLEARMODE message from an operator.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the channel to affect
 * \li \a parv[2] (optional) is the channel modes to clear
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int
mo_clearmode(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Channel *chptr;
  char *control = "ovpsmikbl"; /* default control string */
  const char *chname, *qreason;
  int force = 0;

  if (!feature_bool(FEAT_CONFIG_OPERCMDS))
    return send_reply(sptr, ERR_DISABLED, "CLEARMODE");

  if (parc < 2)
    return need_more_params(sptr, "CLEARMODE");

  if (parc > 2)
    control = parv[2];

  chname = parv[1];
  if (*chname == '!')
  {
    chname++;
    if (!HasPriv(sptr, IsLocalChannel(chname) ?
                         PRIV_FORCE_LOCAL_OPMODE :
                         PRIV_FORCE_OPMODE))
      return send_reply(sptr, ERR_NOPRIVILEGES);
    force = 1;
  }

  if (!HasPriv(sptr,
	       IsLocalChannel(chname) ? PRIV_LOCAL_OPMODE : PRIV_OPMODE))
    return send_reply(sptr, ERR_NOPRIVILEGES);

  if (('#' != *chname && '&' != *chname) || !(chptr = FindChannel(chname)))
    return send_reply(sptr, ERR_NOSUCHCHANNEL, chname);

  if (!force && (qreason = find_quarantine(chptr->chname)))
    return send_reply(sptr, ERR_QUARANTINED, chptr->chname, qreason);

  return do_clearmode(cptr, sptr, chptr, control);
}
