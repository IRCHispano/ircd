/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_burst.c
 *
 * Copyright (C) 2002-2015 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1995-1997 Carlo Wood <carlo@runaway.xs4all.nl>
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
 * @brief Handlers for BURST command.
 * @version $Id: m_burst.c,v 1.23 2008-01-19 13:28:51 zolty Exp $
 */
#include "config.h"

#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "ircd_snprintf.h"
#include "list.h"
#include "match.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_conf.h"
#include "s_misc.h"
#include "send.h"
#include "struct.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/** Find modes in \a parv that may require net.riders to be kicked.
 * \a parv[0] must be a mode string starting with '+', and \a parv
 * must contain enough parameters to satisfy any argument-bearing
 * modes in \a parv[0].
 *
 * The current set of triggering modes are +i, +r, and (if \a curr_key
 * is different than the received key) +k.
 *
 * @param[in] parc Number of valid elements in \a parv.
 * @param[in] parv Mode arguments.
 * @param[in] curr_key The channel's current key.
 * @return A bitmask of MODE_* modes.
 */
static int
netride_modes(int parc, char **parv, const char *curr_key)
{
  char *modes = parv[0];
  int result = 0;

  assert(modes && modes[0] == '+');
  while (*modes) {
    switch (*modes++) {
    case '-':
      return -1;
    case 'i':
      result |= MODE_INVITEONLY;
      break;
    case 'k':
      if (strcmp(curr_key, *++parv))
        result |= MODE_KEY;
      break;
    case 'l':
      ++parv;
      break;
#if defined(UNDERNET)
    case 'r':
#else
    case 'R':
#endif
      result |= MODE_REGONLY;
      break;
    case 'z':
      result |= MODE_SSLONLY;
      break;
    }
  }
  return result;
}

/** Handle a BURST message from a server.
 *
 * --  by Run carlo@runaway.xs4all.nl  december 1995 till march 1997
 *
 * parv[0] = sender prefix
 * parv[1] = channel name
 * parv[2] = channel timestamp
 * The meaning of the following parv[]'s depend on their first character:
 * If parv[n] starts with a '+':
 * Net burst, additive modes
 *   parv[n] = \<mode\>
 *   parv[n+1] = \<param\> (optional)
 *   parv[n+2] = \<param\> (optional)
 * If parv[n] starts with a '%', then n will be parc-1:
 *   parv[n] = \%\<ban\> \<ban\> \<ban\> ...
 * If parv[n] starts with another character:
 *   parv[n] = \<nick\>[:\<mode\>],\<nick\>[:\<mode\>],...
 *
 * defined(UNDERNET)
 *   where \<mode\> defines the mode and op-level
 *   for nick and all following nicks until the
 *   next \<mode\> field.
 *   Digits in the \<mode\> field have of two meanings:
 *   1) if it is the first field in this BURST message
 *      that contains digits, and/or when a 'v' is
 *      present in the \<mode\>:
 *      The absolute value of the op-level.
 *   2) if there are only digits in this field and
 *      it is not the first field with digits:
 *      An op-level increment relative to the previous
 *      op-level.
 *   First all modeless nicks must be emmitted,
 *   then all combinations of modes without ops
 *   (currently that is only 'v') followed by the same
 *   series but then with ops (currently 'o','ov').
 *
 * Example:
 * "A8 B #test 87654321 +ntkAl key secret 123 A8AAG,A8AAC:v,A8AAA:0,A8AAF:2,A8AAD,A8AAB:v1,A8AAE:1 :%ban1 ban2"
 *
 * \<mode\> list example:
 *
 * "xxx,sss:v,ttt,aaa:123,bbb,ccc:2,ddd,kkk:v2,lll:2,mmm"
 *
 * means
 *
 *  xxx		// first modeless nicks
 *  sss +v	// then opless nicks
 *  ttt +v	// no ":\<mode\>": everything stays the same
 *  aaa -123	// first field with digit: absolute value
 *  bbb -123
 *  ccc -125	// only digits, not first field: increment
 *  ddd -125
 *  kkk -2 +v	// field with a 'v': absolute value
 *  lll -4 +v	// only digits: increment
 *  mmm -4 +v
 *
 *
 * !defined(UNDERNET)
 *   where \<mode\> is the channel mode (ov) of nick and all following nicks.
 *
 * Example:
 * "S BURST #channel 87654321 +ntkl key 123 AAA,AAB:o,BAA,BAB:ov :%ban1 ban2"
 *
 * end defines
 *
 * Anti net.ride code.
 *
 * When the channel already exist, and its TS is larger than
 * the TS in the BURST message, then we cancel all existing modes.
 * If its is smaller then the received BURST message is ignored.
 * If it's equal, then the received modes are just added.
 *
 * BURST is also accepted outside a netburst now because it
 * is sent upstream as reaction to a DESTRUCT message.  For
 * these BURST messages it is possible that the listed channel
 * members are already joined.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_burst(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct ModeBuf modebuf, *mbuf = 0;
  struct Channel *chptr;
  time_t timestamp;
  struct Membership *member, *nmember;
  struct Ban *lp, **lp_p;
  unsigned int parse_flags = (MODE_PARSE_FORCE | MODE_PARSE_BURST);
  int param, nickpos = 0, banpos = 0;
  char modestr[BUFSIZE], nickstr[BUFSIZE], banstr[BUFSIZE];
#if 0
  if (parc < 3)
    return protocol_violation(sptr,"Too few parameters for BURST");

  if (!IsChannelName(parv[1]))
    return protocol_violation(sptr, "Invalid channel name in BURST");

  if (!(chptr = get_channel(sptr, parv[1], CGT_CREATE)))
    return 0; /* can't create the channel? */

  timestamp = atoi(parv[2]);

#if defined(UNDERNET)
  if (chptr->creationtime)	/* 0 for new (empty) channels,
                                   i.e. when this server just restarted. */
  {
    if (parc == 3)		/* Zannel BURST? */
    {
      /* An empty channel without +A set, will cause a BURST message
	 with exactly 3 parameters (because all modes have been reset).
	 If the timestamp on such channels is only a few seconds older
	 from our own, then we ignore this burst: we do not deop our
	 own side.
	 Likewise, we expect the other (empty) side to copy our timestamp
	 from our own BURST message, even though it is slightly larger.

	 The reason for this is to allow people to join an empty
	 non-A channel (a zannel) during a net.split, and not be
	 deopped when the net reconnects (with another zannel). When
	 someone joins a split zannel, their side increments the TS by one.
	 If they cycle a few times then we still don't have a reason to
	 deop them. Theoretically I see no reason not to accept ANY timestamp,
	 but to be sure, we only accept timestamps that are just a few
	 seconds off (one second for each time they cycled the channel). */

      /* Don't even deop users who cycled four times during the net.break. */
      if (timestamp < chptr->creationtime &&
          chptr->creationtime <= timestamp + 4 &&
	  chptr->users != 0)	/* Only do this when WE have users, so that
	  			   if we do this the BURST that we sent has
				   parc > 3 and the other side will use the
				   test below: */
	timestamp = chptr->creationtime; /* Do not deop our side. */
    }
    else if (chptr->creationtime < timestamp &&
             timestamp <= chptr->creationtime + 4 &&
	     chptr->users == 0)
    {
      /* If one side of the net.junction does the above
         timestamp = chptr->creationtime, then the other
	 side must do this: */
      chptr->creationtime = timestamp;	/* Use the same TS on both sides. */
    }
    /* In more complex cases, we might still end up with a
       creationtime desync of a few seconds, but that should
       be synced automatically rather quickly (every JOIN
       caries a timestamp and will sync it; modes by users do
       not carry timestamps and are accepted regardless).
       Only when nobody joins the channel on the side with
       the oldest timestamp before a new net.break occurs
       precisely inbetween the desync, an unexpected bounce
       might happen on reconnect. */
  }
#endif

  if (!chptr->creationtime || chptr->creationtime > timestamp) {
    /*
     * Kick local members if channel is +i or +k and our TS was larger
     * than the burst TS (anti net.ride). The modes hack is here because
     * we have to do this before mode_parse, as chptr may go away.
     */
    for (param = 3; param < parc; param++)
    {
      int check_modes;
      if (parv[param][0] != '+')
        continue;
      check_modes = netride_modes(parc - param, parv + param, chptr->mode.key);
      if (check_modes < 0)
      {
        if (chptr->users == 0)
          sub1_from_channel(chptr);
        return protocol_violation(sptr, "Invalid mode string in BURST");
      }
      else if (check_modes)
      {
        /* Clear any outstanding rogue invites */
        mode_invite_clear(chptr);
        for (member = chptr->members; member; member = nmember)
        {
          nmember = member->next_member;
          if (!MyUser(member->user) || IsZombie(member))
            continue;
          /* Kick as netrider if key mismatch *or* remote channel is
           * +i (unless user is an oper) *or* remote channel is +r
           * (unless user has an account)  *or* remote channel is +z
           * (unless user is using SSL).
           */
          if (!(check_modes & MODE_KEY)
              && (!(check_modes & MODE_INVITEONLY) || IsAnOper(member->user))
#if defined(UNDERNET)
              && (!(check_modes & MODE_REGONLY) || IsAccount(member->user))
#elif defined (DDB) || defined(SERVICES)
              && (!(check_modes & MODE_REGONLY) || IsNickRegistered(member->user))
#endif
              && (!(check_modes & MODE_SSLONLY) || IsSSL(member->user)))
            continue;
          sendcmdto_serv(&me, CMD_KICK, NULL, "%H %C :Net Rider", chptr, member->user);
          sendcmdto_channel(&his, CMD_KICK, chptr, NULL, SKIP_SERVERS, "%H %C :Net Rider", chptr, member->user);
          make_zombie(member, member->user, &me, &me, chptr);
        }
      }
      break;
    }

    /* If the channel had only locals, it went away by now. */
    if (!(chptr = get_channel(sptr, parv[1], CGT_CREATE)))
      return 0; /* can't create the channel? */
  }

  /* turn off burst joined flag */
  for (member = chptr->members; member; member = member->next_member)
    member->status &= ~(CHFL_BURST_JOINED|CHFL_BURST_ALREADY_OPPED|CHFL_BURST_ALREADY_VOICED);

  if (!chptr->creationtime) /* mark channel as created during BURST */
    chptr->mode.mode |= MODE_BURSTADDED;

  /* new channel or an older one */
  if (!chptr->creationtime || chptr->creationtime > timestamp) {
    chptr->creationtime = timestamp;

    modebuf_init(mbuf = &modebuf, &me, cptr, chptr,
		 MODEBUF_DEST_CHANNEL | MODEBUF_DEST_NOKEY);
    modebuf_mode(mbuf, MODE_DEL | chptr->mode.mode); /* wipeout modes */
    chptr->mode.mode &= MODE_BURSTADDED | MODE_WASDELJOINS;

    /* wipeout any limit and keys that are set */
    parse_flags |= (MODE_PARSE_SET | MODE_PARSE_WIPEOUT);

    /* mark bans for wipeout */
    for (lp = chptr->banlist; lp; lp = lp->next)
      lp->flags |= BAN_BURST_WIPEOUT;

    /* clear topic set by netrider (if set) */
    if (*chptr->topic) {
      *chptr->topic = '\0';
      *chptr->topic_nick = '\0';
      chptr->topic_time = 0;
      sendcmdto_channel(&his, CMD_TOPIC, chptr, NULL, SKIP_SERVERS,
                        "%H :%s", chptr, chptr->topic);
    }
  } else if (chptr->creationtime == timestamp) {
    modebuf_init(mbuf = &modebuf, &me, cptr, chptr,
		 MODEBUF_DEST_CHANNEL | MODEBUF_DEST_NOKEY);

    parse_flags |= MODE_PARSE_SET; /* set new modes */
  }

  param = 3; /* parse parameters */
  while (param < parc) {
    switch (*parv[param]) {
    case '+': /* parameter introduces a mode string */
      param += mode_parse(mbuf, cptr, sptr, chptr, parc - param,
			  parv + param, parse_flags, NULL);
      break;

    case '%': /* parameter contains bans */
      if (parse_flags & MODE_PARSE_SET) {
	char *banlist = parv[param] + 1, *p = 0, *ban, *ptr;
	struct Ban *newban;

	for (ban = ircd_strtok(&p, banlist, " "); ban;
	     ban = ircd_strtok(&p, 0, " ")) {
	  ban = collapse(pretty_mask(ban));

	    /*
	     * Yeah, we should probably do this elsewhere, and make it better
	     * and more general; this will hold until we get there, though.
	     * I dislike the current add_banid API... -Kev
	     *
	     * I wish there were a better algo. for this than the n^2 one
	     * shown below *sigh*
	     */
	  for (lp = chptr->banlist; lp; lp = lp->next) {
	    if (!ircd_strcmp(lp->banstr, ban)) {
	      ban = 0; /* don't add ban */
	      lp->flags &= ~BAN_BURST_WIPEOUT; /* not wiping out */
	      break; /* new ban already existed; don't even repropagate */
	    } else if (!(lp->flags & BAN_BURST_WIPEOUT) &&
		       !mmatch(lp->banstr, ban)) {
	      ban = 0; /* don't add ban unless wiping out bans */
	      break; /* new ban is encompassed by an existing one; drop */
	    } else if (!mmatch(ban, lp->banstr))
	      lp->flags |= BAN_OVERLAPPED; /* remove overlapping ban */

	    if (!lp->next)
	      break;
	  }

	  if (ban) { /* add the new ban to the end of the list */
	    /* Build ban buffer */
	    if (!banpos) {
	      banstr[banpos++] = ' ';
	      banstr[banpos++] = ':';
	      banstr[banpos++] = '%';
	    } else
	      banstr[banpos++] = ' ';
	    for (ptr = ban; *ptr; ptr++) /* add ban to buffer */
	      banstr[banpos++] = *ptr;

	    newban = make_ban(ban); /* create new ban */

            strcpy(newban->who,
                      cli_name(feature_bool(FEAT_HIS_BANWHO) ? &me : sptr));

	    newban->when = TStime();
	    newban->flags |= BAN_BURSTED;
	    newban->next = 0;
	    if (lp)
	      lp->next = newban; /* link it in */
	    else
	      chptr->banlist = newban;
	  }
	}
      }
      param++; /* look at next param */
      break;

    default: /* parameter contains clients */
      {
	struct Client *acptr;
	char *nicklist = parv[param], *p = 0, *nick, *ptr;
	int current_mode, last_mode, base_mode;
#if defined(UNDERNET)
	int oplevel = -1;	/* Mark first field with digits: means the same as 'o' (but with level). */
	int last_oplevel = 0;
	struct Membership* member;
#endif

        base_mode = CHFL_BURST_JOINED;
        if (chptr->mode.mode & MODE_DELJOINS)
            base_mode |= CHFL_DELAYED;
        current_mode = last_mode = base_mode;

	for (nick = ircd_strtok(&p, nicklist, ","); nick;
	     nick = ircd_strtok(&p, 0, ",")) {

	  if ((ptr = strchr(nick, ':'))) { /* new flags; deal */
	    *ptr++ = '\0';

	    if (parse_flags & MODE_PARSE_SET) {
	      int current_mode_needs_reset;
	      for (current_mode_needs_reset = 1; *ptr; ptr++) {
		if (*ptr == 'o') { /* has oper status */
		  /*
		   * An 'o' is pre-oplevel protocol, so this is only for
		   * backwards compatibility.  Give them an op-level of
		   * MAXOPLEVEL so everyone can deop them.
		   */
#if defined(UNDERNET)
		  oplevel = MAXOPLEVEL;
#endif
		  if (current_mode_needs_reset) {
		    current_mode = base_mode;
		    current_mode_needs_reset = 0;
		  }
		  current_mode = (current_mode & ~CHFL_DELAYED) | CHFL_CHANOP;
		}
#if defined(DDB) || defined(SERVICES)
                else if (*ptr == 'q') { /* has owner status */
                  if (current_mode_needs_reset) {
                    current_mode = base_mode;
                    current_mode_needs_reset = 0;
                  }
                  current_mode = (current_mode & ~CHFL_DELAYED) | CHFL_OWNER;
                }
#endif
		else if (*ptr == 'v') { /* has voice status */
		  if (current_mode_needs_reset) {
                    current_mode = base_mode;
		    current_mode_needs_reset = 0;
		  }
		  current_mode = (current_mode & ~CHFL_DELAYED) | CHFL_VOICE;
#if defined(UNDERNET)
		  oplevel = -1;	/* subsequent digits are an absolute op-level value. */
#endif
                }
#if defined(UNDERNET)
		else if (IsDigit(*ptr)) {
		  int level_increment = 0;
		  if (oplevel == -1) { /* op-level is absolute value? */
		    if (current_mode_needs_reset) {
		      current_mode = base_mode;
		      current_mode_needs_reset = 0;
		    }
		    oplevel = 0;
		  }
		  current_mode = (current_mode & ~CHFL_DELAYED) | CHFL_CHANOP;
		  do {
		    level_increment = 10 * level_increment + *ptr++ - '0';
		  } while (IsDigit(*ptr));
		  --ptr;
		  oplevel += level_increment;
                  if (oplevel > MAXOPLEVEL) {
                    protocol_violation(sptr, "Invalid cumulative oplevel %u during burst", oplevel);
                    oplevel = MAXOPLEVEL;
                    break;
                  }
		}
#endif
		else { /* I don't recognize that flag */
		  protocol_violation(sptr, "Invalid flag '%c' in nick part of burst", *ptr);
		  break; /* so stop processing */
                }
	      }
	    }
	  }

	  if (!(acptr = findNUser(nick)) || cli_from(acptr) != cptr)
	    continue; /* ignore this client */

	  /* Build nick buffer */
	  nickstr[nickpos] = nickpos ? ',' : ' '; /* first char */
	  nickpos++;

	  for (ptr = nick; *ptr; ptr++) /* store nick */
	    nickstr[nickpos++] = *ptr;

	  if (current_mode != last_mode) { /* if mode changed... */
	    last_mode = current_mode;
#if defined(UNDERNET)
	    last_oplevel = oplevel;
#endif

	    nickstr[nickpos++] = ':'; /* add a specifier */
#if defined(DDB) || defined(SERVICES)
            if (current_mode & CHFL_OWNER)
              nickstr[nickpos++] = 'q';
#endif
	    if (current_mode & CHFL_VOICE)
	      nickstr[nickpos++] = 'v';
	    if (current_mode & CHFL_CHANOP)
            {
#if defined(UNDERNET)
              if (oplevel != MAXOPLEVEL)
	        nickpos += ircd_snprintf(0, nickstr + nickpos, sizeof(nickstr) - nickpos, "%u", oplevel);
              else
#endif
                nickstr[nickpos++] = 'o';
            }
#if defined(UNDERNET)
	  } else if (current_mode & CHFL_CHANOP && oplevel != last_oplevel) { /* if just op level changed... */
	    nickstr[nickpos++] = ':'; /* add a specifier */
	    nickpos += ircd_snprintf(0, nickstr + nickpos, sizeof(nickstr) - nickpos, "%u", oplevel - last_oplevel);
            last_oplevel = oplevel;
	  }
#endif

	  if (!(member = find_member_link(chptr, acptr)))
	  {
#if defined(UNDERNET)
	    add_user_to_channel(chptr, acptr, current_mode, oplevel);
#else
            add_user_to_channel(chptr, acptr, current_mode, 0);
#endif
            if (!(current_mode & CHFL_DELAYED))
              sendcmdto_channel(acptr, CMD_JOIN, chptr, NULL, SKIP_SERVERS, "%H", chptr);
	  }
	  else
	  {
	    /* The member was already joined (either by CREATE or JOIN).
	       Remember the current mode. */
	    if (member->status & CHFL_CHANOP)
	      member->status |= CHFL_BURST_ALREADY_OPPED;
	    if (member->status & CHFL_VOICE)
	      member->status |= CHFL_BURST_ALREADY_VOICED;
	    /* Synchronize with the burst. */
	    member->status |= CHFL_BURST_JOINED | (current_mode & (CHFL_CHANOP|CHFL_VOICE));
#if defined(UNDERNET)
	    SetOpLevel(member, oplevel);
#endif
	  }
	}
      }
      param++;
      break;
    } /* switch (*parv[param]) */
  } /* while (param < parc) */

  nickstr[nickpos] = '\0';
  banstr[banpos] = '\0';

  if (parse_flags & MODE_PARSE_SET) {
    modebuf_extract(mbuf, modestr + 1); /* for sending BURST onward */
    modestr[0] = modestr[1] ? ' ' : '\0';
  } else
    modestr[0] = '\0';

  sendcmdto_serv(sptr, CMD_BURST, cptr, "%H %Tu%s%s%s", chptr,
                 chptr->creationtime, modestr, nickstr, banstr);

  if (parse_flags & MODE_PARSE_WIPEOUT || banpos)
    mode_ban_invalidate(chptr);

  if (parse_flags & MODE_PARSE_SET) { /* any modes changed? */
    /* first deal with channel members */
    for (member = chptr->members; member; member = member->next_member) {
      if (member->status & CHFL_BURST_JOINED) { /* joined during burst */
#if defined(DDB) || defined(SERVICES)
        if (member->status & CHFL_OWNER)
          modebuf_mode_client(mbuf, MODE_ADD | CHFL_OWNER, member->user, 0);
#endif
#if defined(UNDERNET)
	if ((member->status & CHFL_CHANOP) && !(member->status & CHFL_BURST_ALREADY_OPPED))
	  modebuf_mode_client(mbuf, MODE_ADD | CHFL_CHANOP, member->user, OpLevel(member));
	if ((member->status & CHFL_VOICE) && !(member->status & CHFL_BURST_ALREADY_VOICED))
	  modebuf_mode_client(mbuf, MODE_ADD | CHFL_VOICE, member->user, OpLevel(member));
#else
	if (member->status & CHFL_CHANOP)
	  modebuf_mode_client(mbuf, MODE_ADD | CHFL_CHANOP, member->user, 0);
        if (member->status & CHFL_VOICE)
          modebuf_mode_client(mbuf, MODE_ADD | CHFL_VOICE, member->user, 0);
#endif
      } else if (parse_flags & MODE_PARSE_WIPEOUT) { /* wipeout old ops */
#if defined(UNDERNET)
	if (member->status & CHFL_CHANOP)
	  modebuf_mode_client(mbuf, MODE_DEL | CHFL_CHANOP, member->user, OpLevel(member));
	if (member->status & CHFL_VOICE)
	  modebuf_mode_client(mbuf, MODE_DEL | CHFL_VOICE, member->user, OpLevel(member));
#else
	if (member->status & CHFL_CHANOP)
	  modebuf_mode_client(mbuf, MODE_DEL | CHFL_CHANOP, member->user, 0);
	if (member->status & CHFL_VOICE)
	  modebuf_mode_client(mbuf, MODE_DEL | CHFL_VOICE, member->user, 0);
#endif
#if defined(UNDERNET)
        member->status &= ~(CHFL_CHANNEL_MANAGER | CHFL_CHANOP | CHFL_VOICE);
#elif defined(DDB) || defined(SERVICES)
        member->status &= ~(CHFL_OWNER | CHFL_CHANOP | CHFL_VOICE);
#else
        member->status &= ~(CHFL_CHANOP | CHFL_VOICE);
#endif
      }
    }

    /* Now deal with channel bans */
    lp_p = &chptr->banlist;
    while (*lp_p) {
      lp = *lp_p;

      /* remove ban from channel */
      if (lp->flags & (BAN_OVERLAPPED | BAN_BURST_WIPEOUT)) {
        char *bandup;
        DupString(bandup, lp->banstr);
	modebuf_mode_string(mbuf, MODE_DEL | MODE_BAN,
			    bandup, 1);
	*lp_p = lp->next; /* clip out of list */
        free_ban(lp);
	continue;
      } else if (lp->flags & BAN_BURSTED) /* add ban to channel */
	modebuf_mode_string(mbuf, MODE_ADD | MODE_BAN,
			    lp->banstr, 0); /* don't free banstr */

      lp->flags &= BAN_IPMASK; /* reset the flag */
      lp_p = &(*lp_p)->next;
    }
  }
#endif
  return mbuf ? modebuf_flush(mbuf) : 0;
}
