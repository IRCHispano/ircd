/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_join.c
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
 * @brief Handlers for JOIN command.
 * @version $Id: m_join.c,v 1.22 2007-12-11 23:38:25 zolty Exp $
 */
#include "config.h"

#include "channel.h"
#include "client.h"
#include "ddb.h"
#include "gline.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_chattr.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_debug.h"
#include "s_user.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdlib.h>
#include <string.h>

/** Searches for and handles a 0 in a join list.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] chanlist List of channels to join.
 * @return First token in \a chanlist after the final 0 entry, which
 * may be its nul terminator (if the final entry is a 0 entry).
 */
static char *
last0(struct Client *cptr, struct Client *sptr, char *chanlist)
{
  char *p;
  int join0 = 0;

  for (p = chanlist; p[0]; p++) /* find last "JOIN 0" */
    if (p[0] == '0' && (p[1] == ',' || p[1] == '\0')) {
      if (p[1] == ',')
        p++;
      chanlist = p + 1;
      join0 = 1;
    } else {
      while (p[0] != ',' && p[0] != '\0') /* skip past channel name */
	p++;

      if (!p[0]) /* hit the end */
	break;
    }

  if (join0) {
    struct JoinBuf part;
    struct Membership *member;

    joinbuf_init(&part, sptr, cptr, JOINBUF_TYPE_PARTALL,
                 "Left all channels", 0);

    joinbuf_join(&part, 0, 0);

    while ((member = cli_user(sptr)->channel))
      joinbuf_join(&part, member->channel,
                   IsZombie(member) ? CHFL_ZOMBIE :
                   IsDelayedJoin(member) ? CHFL_DELAYED :
                   0);

    joinbuf_flush(&part);
  }

  return chanlist;
}

/** Handle a JOIN message from a client connection.
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_join(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Channel *chptr;
  struct JoinBuf join;
  struct JoinBuf create;
  struct Gline *gline;
  char *p = 0;
  char *chanlist;
  char *name;
  char *keys;
#if defined(DDB)
  struct Ddb *ddb_redir;
  char chanddb[BUFSIZE];
#endif

  if (parc < 2 || *parv[1] == '\0')
    return need_more_params(sptr, "JOIN");

  joinbuf_init(&join, sptr, cptr, JOINBUF_TYPE_JOIN, 0, 0);
  joinbuf_init(&create, sptr, cptr, JOINBUF_TYPE_CREATE, 0, TStime());

#if defined(DDB)
  if (IsUserNoJoin(sptr)) {
    struct Ddb *ddb;
    if ((ddb = ddb_find_key(DDB_CONFIGDB, DDB_CONFIGDB_DEBUGCHAN_J))) {
      strcpy(chanddb, ddb_content(ddb));
      parv[1] = chanddb;
    }
  }
#endif

  chanlist = last0(cptr, sptr, parv[1]); /* find last "JOIN 0" */

  keys = parv[2]; /* remember where keys are */

  for (name = ircd_strtok(&p, chanlist, ","); name;
       name = ircd_strtok(&p, 0, ",")) {
    char *key = 0;

    /* If we have any more keys, take the first for this channel. */
    if (!BadPtr(keys)
        && (keys = strchr(key = keys, ',')))
      *keys++ = '\0';

    /* Empty keys are the same as no keys. */
    if (key && !key[0])
      key = 0;

#if defined(DDB)
    /* Redireccion de canales */
/*    if (activar_redireccion_canales && (ddb_redir = ddb_find_key(DDB_CHANREDIRECTDB, name)))*/
    if ((ddb_redir = ddb_find_key(DDB_CHANREDIRECTDB, name)))
      name = ddb_content(ddb_redir);
#endif

    if (!IsChannelName(name) || !strIsIrcCh(name))
    {
      /* bad channel name */
      send_reply(sptr, ERR_NOSUCHCHANNEL, name);
      continue;
    }

    if (cli_user(sptr)->joined >= feature_uint(FEAT_MAXCHANNELSPERUSER)
	&& !HasPriv(sptr, PRIV_CHAN_LIMIT)) {
      send_reply(sptr, ERR_TOOMANYCHANNELS, name);
      break; /* no point processing the other channels */
    }

    /* BADCHANed channel */
    if ((gline = gline_find(name, GLINE_BADCHAN | GLINE_EXACT)) &&
	GlineIsActive(gline) && !IsAnOper(sptr)) {
      send_reply(sptr, ERR_BANNEDFROMCHAN, name);
      continue;
    }

    if (!(chptr = FindChannel(name))) {
      if (((name[0] == '&') && !feature_bool(FEAT_LOCAL_CHANNELS))
	  || strlen(name) > IRCD_MIN(CHANNELLEN, feature_uint(FEAT_CHANNELLEN))) {
        send_reply(sptr, ERR_NOSUCHCHANNEL, name);
        continue;
      }

      if (!(chptr = get_channel(sptr, name, CGT_CREATE)))
        continue;

      /* Try to add the new channel as a recent target for the user. */
      if (check_target_limit(sptr, chptr, chptr->chname, 0)) {
#if defined(UNDERNET)
        chptr->members = 0;
        destruct_channel(chptr);
        continue;
      }

      joinbuf_join(&create, chptr, CHFL_CHANOP | CHFL_CHANNEL_MANAGER);
#else
/*TODO-ZOLTAN: Ojo con esto, con el target limit*/
        sub1_from_channel(chptr);
        continue;
      }

      joinbuf_join(&create, chptr, CHFL_CHANOP);
#endif
    } else if (find_member_link(chptr, sptr)) {
      continue; /* already on channel */
    } else if (check_target_limit(sptr, chptr, chptr->chname, 0)) {
      continue;
    } else {
      struct Invite *invite;
      int flags = 0;
      int err = 0;
#if defined(DDB)
      struct Ddb *ddb = ddb_find_key(DDB_CHANDB, chptr->chname);
#endif

#if defined(UNDERNET)
      /* Check Apass/Upass -- since we only ever look at a single
       * "key" per channel now, this hampers brute force attacks. */
      if (key && !strcmp(key, chptr->mode.apass))
        flags = CHFL_CHANOP | CHFL_CHANNEL_MANAGER;
      else if (key && !strcmp(key, chptr->mode.upass))
        flags = CHFL_CHANOP;
      else if (chptr->users == 0 && !chptr->mode.apass[0]) {
        /* Joining a zombie channel (zannel): give ops and increment TS. */
        flags = CHFL_CHANOP;
        chptr->creationtime++;
      } else if (chptr->mode.mode & MODE_INVITEONLY)
#elif defined(DDB)
      if (ddb && !ircd_strcmp(ddb_content(ddb), cli_name(sptr)))
        flags = CHFL_OWNER;
      else if (chptr->mode.mode & MODE_INVITEONLY)
#else
      if (chptr->mode.mode & MODE_INVITEONLY)
#endif
        err = ERR_INVITEONLYCHAN;
      else if (chptr->mode.limit && (chptr->users >= chptr->mode.limit))
        err = ERR_CHANNELISFULL;
#if defined(UNDERNET)
      else if ((chptr->mode.mode & MODE_REGONLY) && !IsAccount(sptr))
#elif defined(DDB) || defined(SERVICES)
      else if ((chptr->mode.mode & MODE_REGONLY) && !IsNickRegistered(sptr))
#endif
        err = ERR_NEEDREGGEDNICK;
      else if ((chptr->mode.mode & MODE_SSLONLY) && !IsSSL(sptr))
        err = ERR_NOSSL;
      else if (find_ban(sptr, chptr->banlist))
        err = ERR_BANNEDFROMCHAN;
      else if (*chptr->mode.key && (!key || strcmp(key, chptr->mode.key)))
        err = ERR_BADCHANNELKEY;

      /* If the user is invited, s/he can bypass the normal errors. */
      if (err && feature_bool(FEAT_ANNOUNCE_INVITES)
          && (invite = is_invited(sptr, chptr))) {
        const struct Numeric *num;
        num = get_error_numeric(RPL_ISSUEDINVITE);
        sendcmdto_channel(&me, num->str, num->str, chptr, cptr,
                          SKIP_NONOPS | SKIP_BURST,
                          "%H %s %s :%s has been invited by %s",
                          chptr, cli_name(sptr), invite->inviter,
                          cli_name(sptr), invite->inviter);
        err = 0;
      }

      /* An oper with WALK_LCHAN privilege can join a local channel
       * he otherwise could not join by using "OVERRIDE" as the key.
       * This will generate a HACK(4) notice, but fails if the oper
       * could normally join the channel. */
      if (IsLocalChannel(chptr->chname)
          && HasPriv(sptr, PRIV_WALK_LCHAN)
          && !(flags & CHFL_CHANOP)
          && key && !strcmp(key, "OVERRIDE"))
      {
        switch (err) {
        case 0:
          if (strcmp(chptr->mode.key, "OVERRIDE")
#if defined(UNDERNET)
              && strcmp(chptr->mode.apass, "OVERRIDE")
              && strcmp(chptr->mode.upass, "OVERRIDE")) {
#else
              ) {
#endif
            send_reply(sptr, ERR_DONTCHEAT, chptr->chname);
            continue;
          }
          break;
        case ERR_INVITEONLYCHAN: err = 'i'; break;
        case ERR_CHANNELISFULL:  err = 'l'; break;
        case ERR_BANNEDFROMCHAN: err = 'b'; break;
        case ERR_BADCHANNELKEY:  err = 'k'; break;
#if defined(UNDERNET)
        case ERR_NEEDREGGEDNICK: err = 'r'; break;
#else
        case ERR_NEEDREGGEDNICK: err = 'R'; break;
#endif
        case ERR_NOSSL:          err = 'z'; break;
        default: err = '?'; break;
        }
        /* send accountability notice */
        if (err)
          sendto_opmask(0, SNO_HACK4, "OPER JOIN: %C JOIN %H "
                        "(overriding +%c)", sptr, chptr, err);
        err = 0;
      }

      /* Is there some reason the user may not join? */
      if (err) {
        switch(err) {
          case ERR_NEEDREGGEDNICK:
            send_reply(sptr,
                       ERR_NEEDREGGEDNICK,
                       chptr->chname,
                       feature_str(FEAT_URLREG));
            break;
          default:
            send_reply(sptr, err, chptr->chname);
            break;
        }
        continue;
      }

      joinbuf_join(&join, chptr, flags);
      cli_last_join(sptr) = CurrentTime;
      if (flags & CHFL_CHANOP) {
        struct ModeBuf mbuf;
	/* Always let the server op him: this is needed on a net with older servers
	   because they 'destruct' channels immediately when they become empty without
	   sending out a DESTRUCT message. As a result, they would always bounce a mode
	   (as HACK(2)) when the user ops himself.
           (There is also no particularly good reason to have the user op himself.)
        */
	modebuf_init(&mbuf, &me, cptr, chptr, MODEBUF_DEST_SERVER);
#if defined(UNDERNET)
	modebuf_mode_client(&mbuf, MODE_ADD | MODE_CHANOP, sptr,
                            chptr->mode.apass[0] ? ((flags & CHFL_CHANNEL_MANAGER) ? 0 : 1) : MAXOPLEVEL);
#elif defined(DDB) || defined(SERVICES)
	modebuf_mode_client(&mbuf, MODE_ADD | MODE_CHANOP, sptr, 0);
#endif
	modebuf_flush(&mbuf);
      }
    }

    del_invite(sptr, chptr);

    if (chptr->topic[0]) {
      send_reply(sptr, RPL_TOPIC, chptr->chname, chptr->topic);
      send_reply(sptr, RPL_TOPICWHOTIME, chptr->chname, chptr->topic_nick,
		 chptr->topic_time);
    }

    do_names(sptr, chptr, NAMES_ALL|NAMES_EON); /* send /names list */
  }

  joinbuf_flush(&join); /* must be first, if there's a JOIN 0 */
  joinbuf_flush(&create);

  return 0;
}

/** Handle a JOIN message from a server connection.
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_join(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Membership *member;
  struct Channel *chptr;
  struct JoinBuf join;
  time_t creation = 0;
  char *p = 0;
  char *chanlist;
  char *name;

  if (IsServer(sptr))
  {
    return protocol_violation(cptr,
                              "%s tried to JOIN %s, duh!",
                              cli_name(sptr),
                              (parc < 2 || *parv[1] == '\0') ? "a channel" :
                                                               parv[1]
                              );
  }

  if (parc < 2 || *parv[1] == '\0')
    return need_more_params(sptr, "JOIN");

  if (parc > 2 && parv[2])
    creation = atoi(parv[2]);

  joinbuf_init(&join, sptr, cptr, JOINBUF_TYPE_JOIN, 0, 0);

  chanlist = last0(cptr, sptr, parv[1]); /* find last "JOIN 0" */

  for (name = ircd_strtok(&p, chanlist, ","); name;
       name = ircd_strtok(&p, 0, ",")) {

    if (IsLocalChannel(name) || !IsChannelName(name))
    {
      protocol_violation(cptr, "%s tried to join %s", cli_name(sptr), name);
      continue;
    }

    if (!(chptr = FindChannel(name)))
    {
      /* No channel exists, so create one */
      if (!(chptr = get_channel(sptr, name, CGT_CREATE)))
      {
        protocol_violation(sptr,"couldn't get channel %s for %s",
        		   name,cli_name(sptr));
      	continue;
      }
      chptr->creationtime = creation;
    }
    else { /* We have a valid channel? */
      if ((member = find_member_link(chptr, sptr)))
      {
	/* It is impossible to get here --Run */
	if (!IsZombie(member)) /* already on channel */
	  continue;

	remove_user_from_channel(sptr, chptr);
	chptr = FindChannel(name);
      }
      /* Always copy the timestamp when it is older, that is the only way to
         ensure network-wide synchronization of creation times.
         We now also copy a creation time that only 1 second younger...
         this is needed because the timestamp must be incremented
         by one when someone joins an existing, but empty, channel.
         However, this is only necessary when the channel is still
         empty (also here) and when this channel doesn't have +A set.

         To prevent this from allowing net-rides on the channel, we
         clear all modes from the channel.

         (Scenario for a net ride: c1 - s1 - s2 - c2, with c1 the only
         user in the channel; c1 parts and rejoins, gaining ops.
         Before s2 sees c1's part, c2 joins the channel and parts
         immediately.  s1 sees c1 part, c1 create, c2 join, c2 part;
         c2's join resets the timestamp.  s2 sees c2 join, c2 part, c1
         part, c1 create; but since s2 sees the channel as a zannel or
         non-existent, it does not bounce the create with the newer
         timestamp.)
      */
#if defined(UNDERNET)
      if (creation && (creation < chptr->creationtime ||
		       (!chptr->mode.apass[0] && chptr->users == 0))) {
#else
      if (creation && (creation < chptr->creationtime ||
                       (chptr->users == 0))) {
#endif
        struct Membership *member;
        struct ModeBuf mbuf;

	chptr->creationtime = creation;
        /* Wipe out the current modes on the channel. */
        modebuf_init(&mbuf, sptr, cptr, chptr, MODEBUF_DEST_CHANNEL | MODEBUF_DEST_HACK3);

        modebuf_mode(&mbuf, MODE_DEL | chptr->mode.mode);
        chptr->mode.mode &= MODE_BURSTADDED | MODE_WASDELJOINS;

        if (chptr->mode.limit) {
          modebuf_mode_uint(&mbuf, MODE_DEL | MODE_LIMIT, chptr->mode.limit);
          chptr->mode.limit = 0;
        }

        if (chptr->mode.key[0]) {
          modebuf_mode_string(&mbuf, MODE_DEL | MODE_KEY, chptr->mode.key, 0);
          chptr->mode.key[0] = '\0';
        }

#if defined(UNDERNET)
        if (chptr->mode.upass[0]) {
          modebuf_mode_string(&mbuf, MODE_DEL | MODE_UPASS, chptr->mode.upass, 0);
          chptr->mode.upass[0] = '\0';
        }

        if (chptr->mode.apass[0]) {
          modebuf_mode_string(&mbuf, MODE_DEL | MODE_APASS, chptr->mode.apass, 0);
          chptr->mode.apass[0] = '\0';
        }
#endif

        for (member = chptr->members; member; member = member->next_member)
        {
          if (IsChanOp(member)) {
            modebuf_mode_client(&mbuf, MODE_DEL | MODE_CHANOP, member->user, OpLevel(member));
	    member->status &= ~CHFL_CHANOP;
	  }
          if (HasVoice(member)) {
            modebuf_mode_client(&mbuf, MODE_DEL | MODE_VOICE, member->user, OpLevel(member));
	    member->status &= ~CHFL_VOICE;
          }
        }
        modebuf_flush(&mbuf);
      }
    }

    joinbuf_join(&join, chptr, 0);
  }

  joinbuf_flush(&join); /* flush joins... */

  return 0;
}
