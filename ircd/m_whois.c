/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_whois.c
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
 * @brief Handlers for WHOIS command.
 * @version $Id: m_whois.c,v 1.20 2007-11-11 21:53:08 zolty Exp $
 */
#include "config.h"

#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "match.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_user.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <string.h>

/** Maximum number of lines to send in response to a /WHOIS. */
#define MAX_WHOIS_LINES 50

/*
 * 2000-07-01: Isomer
 *  * Rewritten to make this understandable
 *  * You can no longer /whois unregistered clients.
 *
 *
 * General rules:
 *  /whois nick always shows the nick.
 *  /whois wild* shows the nick if:
 *   * they aren't +i and aren't on any channels.
 *   * they are on a common channel.
 *   * they aren't +i and are on a public channel. (not +p and not +s)
 *   * they aren't +i and are on a private channel. (+p but not +s)
 *  Or to look at it another way (I think):
 *   * +i users are only shown if your on a common channel.
 *   * users on +s channels only aren't shown.
 *
 *  whois doesn't show what channels a +k client is on, for the reason that
 *  /whois X or /whois W floods a user off the server. :)
 *
 * nb: if the code and this comment disagree, the codes right and I screwed
 *     up.
 */

/** Send whois information for \a acptr to \a sptr.
 * @param[in] sptr Client requesting information
 * @param[in] acptr Client whose information is requested
 * @param[in] parc 2 for a "normal" whois, >=3 for a remote whois
 */
static void do_whois(struct Client* sptr, struct Client *acptr, int parc)
{
  struct Client *a2cptr=0;
  struct Channel *chptr=0;
  int mlen;
  int len;
  static char buf[512];

  const struct User* user = cli_user(acptr);
  const char* name = (!*(cli_name(acptr))) ? "?" : cli_name(acptr);
  a2cptr = feature_bool(FEAT_HIS_WHOIS_SERVERNAME) && !IsAnOper(sptr)
      && sptr != acptr ? &his : user->server;
  assert(user);
  send_reply(sptr, RPL_WHOISUSER, name, user->username, user->host,
		   cli_info(acptr));

  /* Display the channels this user is on. */
  if (!IsChannelService(acptr))
  {
    struct Membership* chan;
    mlen = strlen(cli_name(&me)) + strlen(cli_name(sptr)) + 12 + strlen(name);
    len = 0;
    *buf = '\0';
    for (chan = user->channel; chan; chan = chan->next_channel)
    {
       chptr = chan->channel;

       if (!ShowChannel(sptr, chptr)
           && !(IsAnOper(sptr) && IsLocalChannel(chptr->chname)))
          continue;

       if (acptr != sptr && IsZombie(chan))
          continue;

       /* Don't show local channels when HIS is defined, unless it's a
	* remote WHOIS --ULtimaTe_
	*/
       if (IsLocalChannel(chptr->chname) && (acptr != sptr) && (parc == 2)
           && feature_bool(FEAT_HIS_WHOIS_LOCALCHAN) && !IsAnOper(sptr))
	  continue;

       if (len+strlen(chptr->chname) + mlen > BUFSIZE - 5)
       {
          send_reply(sptr, SND_EXPLICIT | RPL_WHOISCHANNELS, "%s :%s", name, buf);
          *buf = '\0';
          len = 0;
       }
       if (IsDeaf(acptr))
         *(buf + len++) = '-';
       if (!ShowChannel(sptr, chptr))
         *(buf + len++) = '*';
       if (IsDelayedJoin(chan) && (sptr != acptr))
         *(buf + len++) = '<';
#if defined(DDB) || defined(SERVICES)
       else if (IsChanOwner(chan))
         *(buf + len++) = '.';
#endif
       else if (IsChanOp(chan))
         *(buf + len++) = '@';
       else if (HasVoice(chan))
         *(buf + len++) = '+';
       else if (IsZombie(chan))
         *(buf + len++) = '!';
       if (len)
          *(buf + len) = '\0';
       strcpy(buf + len, chptr->chname);
       len += strlen(chptr->chname);
       strcat(buf + len, " ");
       len++;
     }
     if (buf[0] != '\0')
        send_reply(sptr, RPL_WHOISCHANNELS, name, buf);
  }

  send_reply(sptr, RPL_WHOISSERVER, name, cli_name(a2cptr),
             cli_info(a2cptr));

  if (user)
  {
    char *modes = umode_str(acptr);

    if (user->away)
       send_reply(sptr, RPL_AWAY, name, user->away);

#if defined(DDB) || defined(SERVICES)
    if (IsNickRegistered(acptr))
      send_reply(sptr, RPL_WHOISREGNICK, name);
    else if (IsNickSuspended(acptr))
      send_reply(sptr, RPL_WHOISSUSPEND, name);

    if (SeeOper(sptr,acptr))
    {
      if (IsAdmin(acptr))
        send_reply(sptr, SND_EXPLICIT | RPL_WHOISOPERATOR, "%s :is a Services Administrator", name);
      else if (IsCoder(acptr))
        send_reply(sptr, SND_EXPLICIT | RPL_WHOISOPERATOR, "%s :is a Network Coder", name);
      else if (IsHelpOper(acptr))
        send_reply(sptr, SND_EXPLICIT | RPL_WHOISOPERATOR, "%s :is a Services Operator", name);
      else if (IsOper(acptr))
        send_reply(sptr, RPL_WHOISOPERATOR, name);
      else
        send_reply(sptr, SND_EXPLICIT | RPL_WHOISOPERATOR, "%s :is a Local IRC Operator", name);
    }

    if (IsBot(acptr))
      send_reply(sptr, RPL_WHOISBOT, name, feature_str(FEAT_NETWORK));

#else /* UNDERNET */
    if (SeeOper(sptr,acptr))
       send_reply(sptr, RPL_WHOISOPERATOR, name);

    if (IsAccount(acptr))
      send_reply(sptr, RPL_WHOISACCOUNT, name, user->account);
#endif

    if (HasHiddenHost(acptr) && (IsViewHiddenHost(sptr) || (acptr == sptr)))
        send_reply(sptr, RPL_WHOISACTUALLY, name, user->username,
          user->realhost, ircd_ntoa(&cli_ip(acptr)));

    send_reply(sptr, RPL_WHOISMODES, name, *modes ? modes : "");

#ifdef USE_SSL
    if (MyConnect(acptr) && IsSSL(acptr) && ((parc >= 3) ||
        (acptr == sptr) || IsAnOper(sptr)))
      send_reply(sptr, RPL_WHOISSSL, name);
#endif

    /* Hint: if your looking to add more flags to a user, eg +h, here's
     *       probably a good place to add them :)
     */

    if (MyConnect(acptr) && (!feature_bool(FEAT_HIS_WHOIS_IDLETIME) ||
                             (sptr == acptr || IsAnOper(sptr) || parc >= 3)))
       send_reply(sptr, RPL_WHOISIDLE, name, CurrentTime - user->last,
                  cli_firsttime(acptr));
  }
}

/** Search and return as many people as matched by the wild 'nick'.
 * @param[in] sptr Client searching for other clients
 * @param[in] nick Nickname mask to search for
 * @param[in] count Number of clients previously found
 * @param[in] parc Value passed to do_whois
 * @return The number of people found (or, obviously, 0, if none were
 * found).
 */
static int do_wilds(struct Client* sptr, char *nick, int count, int parc)
{
  struct Client *acptr; /* Current client we're considering */
  struct User *user; 	/* the user portion of the client */
  struct Membership* chan;
  int invis; 		/* does +i apply? */
  int member;		/* Is this user on any channels? */
  int showperson;       /* Should we show this person? */
  int found = 0 ;	/* How many were found? */

  /* Ech! This is hideous! */
  for (acptr = GlobalClientList; (acptr = next_client(acptr, nick));
      acptr = cli_next(acptr))
  {
    if (!IsRegistered(acptr))
      continue;

    if (IsServer(acptr))
      continue;
    /*
     * I'm always last :-) and acptr->next == 0!!
     *
     * Isomer: Does this strike anyone else as being a horrible hideous
     *         hack?
     */
    if (IsMe(acptr)) {
      assert(!cli_next(acptr));
      break;
    }

    /*
     * 'Rules' established for sending a WHOIS reply:
     *
     * - if wildcards are being used don't send a reply if
     *   the querier isn't any common channels and the
     *   client in question is invisible.
     *
     * - only send replies about common or public channels
     *   the target user(s) are on;
     */
    user = cli_user(acptr);
    assert(user);

    invis = (acptr != sptr) && IsInvisible(acptr);
    member = (user && user->channel) ? 1 : 0;
    showperson = !invis && !member;

    /* Should we show this person now? */
    if (showperson) {
    	found++;
    	do_whois(sptr, acptr, parc);
    	if (count+found>MAX_WHOIS_LINES)
    	  return found;
    	continue;
    }

    /* Step through the channels this user is on */
    for (chan = user->channel; chan; chan = chan->next_channel)
    {
      struct Channel *chptr = chan->channel;

      /* If this is a public channel, show the person */
      if (!invis && PubChannel(chptr)) {
        showperson = 1;
        break;
      }

      /* if this channel is +p and not +s, show them */
      if (!invis && HiddenChannel(chptr) && !SecretChannel(chptr)) {
          showperson = 1;
          break;
      }

      member = find_channel_member(sptr, chptr) ? 1 : 0;
      if (invis && !member)
        continue;

      /* If sptr isn't really on this channel, skip it */
      if (IsZombie(chan))
        continue;

      /* Is this a common channel? */
      if (member) {
        showperson = 1;
        break;
      }
    } /* of for (chan in channels) */

    /* Don't show this person */
    if (!showperson)
      continue;

    do_whois(sptr, acptr, parc);
    found++;
    if (count+found>MAX_WHOIS_LINES)
       return found;
  } /* of global client list */

  return found;
}

/** Handle a WHOIS message from a local client
 *
 * \a parv has the following elements:
 * \li \a parv[1] (optional) is the target's nickname if a remote WHOIS is requested
 * \li \a parv[N+1] is the target's nickname, or a comma-separated mask list
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_whois(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  char*           nick;
  char*           tmp;
  char*           p = 0;
  int             found = 0;
  int		  total = 0;
  int             wildscount = 0;

  if (parc < 2)
  {
    send_reply(sptr, ERR_NONICKNAMEGIVEN);
    return 0;
  }

  if (parc > 2)
  {
    /* For convenience: Accept a nickname as first parameter, by replacing
     * it with the correct servername - as is needed by hunt_server().
     * This is the secret behind the /whois nick nick trick.
     */
    if (feature_int(FEAT_HIS_REMOTE))
    {
      /* If remote queries are disabled, then use the *second* parameter of
       * of whois, so /whois nick nick still works.
       */
      if (!IsAnOper(sptr))
      {
        if (!FindUser(parv[2]))
        {
          send_reply(sptr, ERR_NOSUCHNICK, parv[2]);
          send_reply(sptr, RPL_ENDOFWHOIS, parv[2]);
          return 0;
        }
        parv[1] = parv[2];
      }
    }

    if (hunt_server_cmd(sptr, CMD_WHOIS, cptr, 0, "%C :%s", 1, parc, parv) !=
       HUNTED_ISME)
    return 0;

    parv[1] = parv[2];
  }

  for (tmp = parv[1]; (nick = ircd_strtok(&p, tmp, ",")); tmp = 0)
  {
    int wilds;

    found = 0;

    collapse(nick);

    wilds = (strchr(nick, '?') || strchr(nick, '*'));
    if (!wilds) {
      struct Client *acptr = 0;
      /* No wildcards */
      acptr = FindUser(nick);
      if (acptr && !IsServer(acptr)) {
        do_whois(sptr, acptr, parc);
        found = 1;
      }
    }
    else /* wilds */
    {
      if (++wildscount > 3) {
        send_reply(sptr, ERR_QUERYTOOLONG, parv[1]);
        break;
      }
      found=do_wilds(sptr, nick, total, parc);
    }

    if (!found)
      send_reply(sptr, ERR_NOSUCHNICK, nick);
    total+=found;
    if (total >= MAX_WHOIS_LINES) {
      send_reply(sptr, ERR_QUERYTOOLONG, parv[1]);
      break;
    }
    if (p)
      p[-1] = ',';
  } /* of tokenised parm[1] */
  send_reply(sptr, RPL_ENDOFWHOIS, parv[1]);

  return 0;
}

/** Handle a WHOIS message from a server.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the numnick of the server to query
 * \li \a parv[N+1] is the target's nickname, or a comma-separated mask list
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_whois(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  char*           nick;
  char*           tmp;
  char*           p = 0;
  int             found = 0;
  int		  total = 0;

  if (parc < 2)
  {
    send_reply(sptr, ERR_NONICKNAMEGIVEN);
    return 0;
  }

  if (parc > 2)
  {
    if (hunt_server_cmd(sptr, CMD_WHOIS, cptr, 0, "%C :%s", 1, parc, parv) !=
        HUNTED_ISME)
      return 0;
    parv[1] = parv[2];
  }

  total = 0;

  for (tmp = parv[1]; (nick = ircd_strtok(&p, tmp, ",")); tmp = 0)
  {
    struct Client *acptr = 0;

    found = 0;

    collapse(nick);


    acptr = FindUser(nick);
    if (acptr && !IsServer(acptr)) {
      found++;
      do_whois(sptr, acptr, parc);
    }

    if (!found)
      send_reply(sptr, ERR_NOSUCHNICK, nick);

    total+=found;

    if (total >= MAX_WHOIS_LINES) {
      send_reply(sptr, ERR_QUERYTOOLONG, parv[1]);
      break;
    }

    if (p)
      p[-1] = ',';
  } /* of tokenised parm[1] */
  send_reply(sptr, RPL_ENDOFWHOIS, parv[1]);

  return 0;
}
