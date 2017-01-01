/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_opmode.c
 *
 * Copyright (C) 2002-2017 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Handlers for OPERMODE command.
 * @version $Id: m_opmode.c,v 1.9 2008-01-19 19:26:03 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "channel.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "querycmds.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

static void make_oper(struct Client *sptr, struct Client *dptr)
{
  struct Flags old_mode = cli_flags(dptr);

  ++UserStats.opers;
  SetOper(dptr);

  if (MyConnect(dptr))
  {
    cli_handler(dptr) = OPER_HANDLER;
    SetWallops(dptr);
    SetDebug(dptr);
    SetServNotice(dptr);
    det_confs_butmask(dptr, CONF_CLIENT & ~CONF_OPERATOR);
    set_snomask(dptr, SNO_OPERDEFAULT, SNO_ADD);
    cli_max_sendq(dptr) = 0; /* Get the sendq from the oper's class */
    client_set_privs(dptr, NULL, 1);

    send_umode_out(dptr, dptr, &old_mode, HasPriv(dptr, PRIV_PROPAGATE), IsRegistered(dptr));
    send_reply(dptr, RPL_YOUREOPER);

    sendto_opmask(0, SNO_OLDSNO, "%s (%s@%s) is now operator (%c)",
             cli_name(dptr), cli_user(dptr)->username,
             cli_sockhost(dptr), IsOper(dptr) ? 'O' : 'o');

    log_write(LS_OPER, L_INFO, 0, "REMOTE OPER (%#C) by (%s)", dptr,
          cli_name(sptr));
  }
}

static void de_oper(struct Client *dptr)
{
  --UserStats.opers;
  ClearOper(dptr);
  if (MyConnect(dptr))
  {
    cli_handler(dptr) = CLIENT_HANDLER;
    if (feature_bool(FEAT_WALLOPS_OPER_ONLY))
      ClearWallops(dptr);
    if (feature_bool(FEAT_HIS_DEBUG_OPER_ONLY))
      ClearDebug(dptr);
    if (feature_bool(FEAT_HIS_SNOTICES_OPER_ONLY))
    {
      ClearServNotice(dptr);
      set_snomask(dptr, 0, SNO_SET);
    }
    det_confs_butmask(dptr, CONF_CLIENT & ~CONF_OPERATOR);
    client_set_privs(dptr, NULL, 0);
  }
}

/** Handle an OPMODE message from a server connection.
 *
 * \a parv has the same elements as for ms_mode().
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_opmode(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Channel *chptr = 0;
  struct ModeBuf mbuf;

  if (parc < 3)
    return need_more_params(sptr, "OPMODE");

  if (IsLocalChannel(parv[1]))
    return 0;

  if (('#' != *parv[1]) && IsServer(sptr))
  {
    struct ConfItem *conf;
    struct Client *dptr;

    conf = find_conf_byhost(cli_confs(cptr), cli_name(sptr), CONF_UWORLD);
    if (!conf || !(conf->flags & CONF_UWORLD_OPER))
      return send_reply(sptr, ERR_NOPRIVILEGES, parv[1]);

    dptr = findNUser(parv[1]);
    if (!dptr)
      return send_reply(sptr, ERR_NOSUCHNICK, parv[1]);

    sendcmdto_serv(sptr, CMD_OPMODE, cptr, "%s %s",
      parv[1], parv[2]);

    /* At the moment, we only support +o and -o.  set_user_mode() does
     * not support remote mode setting or setting +o.
     */
    if (!strcmp(parv[2], "+o") && !IsOper(dptr))
      make_oper(sptr, dptr);
    else if (!strcmp(parv[2], "-o") && IsOper(dptr))
      de_oper(dptr);

    return 0;
  }

  if (!(chptr = FindChannel(parv[1])))
    return send_reply(sptr, ERR_NOSUCHCHANNEL, parv[1]);

  modebuf_init(&mbuf, sptr, cptr, chptr,
	       (MODEBUF_DEST_CHANNEL | /* Send MODE to channel */
		MODEBUF_DEST_SERVER  | /* And to server */
		MODEBUF_DEST_OPMODE  | /* Use OPMODE */
		MODEBUF_DEST_HACK4   | /* Generate a HACK(4) notice */
		MODEBUF_DEST_LOG));    /* Log the mode changes to OPATH */

  mode_parse(&mbuf, cptr, sptr, chptr, parc - 2, parv + 2,
	     (MODE_PARSE_SET    | /* Set the modes on the channel */
	      MODE_PARSE_STRICT | /* Be strict about it */
	      MODE_PARSE_FORCE),  /* And force them to be accepted */
	      NULL);

  modebuf_flush(&mbuf); /* flush the modes */

  return 0;
}

/** Handle an OPMODE message from an operator.
 *
 * \a parv has the same elements as for m_mode().
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int mo_opmode(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Channel *chptr = 0;
  struct ModeBuf mbuf;
  char *chname;
  const char *qreason;
  int force = 0;

  if (!feature_bool(FEAT_CONFIG_OPERCMDS))
    return send_reply(sptr, ERR_DISABLED, "OPMODE");

  if (parc < 3)
    return need_more_params(sptr, "OPMODE");

  chname = parv[1];
  if (*chname == '!')
  {
    chname++;
    if (!HasPriv(sptr, IsLocalChannel(chname) ? PRIV_FORCE_LOCAL_OPMODE
                                              : PRIV_FORCE_OPMODE))
      return send_reply(sptr, ERR_NOPRIVILEGES);
    force = 1;
  }

  if (!HasPriv(sptr,
	       IsLocalChannel(chname) ? PRIV_LOCAL_OPMODE : PRIV_OPMODE))
    return send_reply(sptr, ERR_NOPRIVILEGES);

  if (!IsChannelName(chname) || !(chptr = FindChannel(chname)))
    return send_reply(sptr, ERR_NOSUCHCHANNEL, chname);

  if (!force && (qreason = find_quarantine(chptr->chname)))
    return send_reply(sptr, ERR_QUARANTINED, chptr->chname, qreason);

  modebuf_init(&mbuf, sptr, cptr, chptr,
	       (MODEBUF_DEST_CHANNEL | /* Send MODE to channel */
		MODEBUF_DEST_SERVER  | /* And to server */
		MODEBUF_DEST_OPMODE  | /* Use OPMODE */
		MODEBUF_DEST_HACK4   | /* Generate a HACK(4) notice */
		MODEBUF_DEST_LOG));    /* Log the mode changes to OPATH */

  mode_parse(&mbuf, cptr, sptr, chptr, parc - 2, parv + 2,
	     (MODE_PARSE_SET |    /* set the modes on the channel */
	      MODE_PARSE_FORCE),  /* And force them to be accepted */
	      NULL);

  modebuf_flush(&mbuf); /* flush the modes */

  return 0;
}
