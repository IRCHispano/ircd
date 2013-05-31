/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/s_serv.c
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
 * @brief Miscellaneous server support functions.
 * @version $Id$
 */
#include "config.h"

#include "s_serv.h"
#include "IPcheck.h"
#include "channel.h"
#include "client.h"
#if defined(DDB)
#include "ddb.h"
#endif
#include "gline.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_crypt.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "ircd_snprintf.h"
#include "jupe.h"
#include "list.h"
#include "match.h"
#include "msg.h"
#include "msgq.h"
#include "numeric.h"
#include "numnicks.h"
#include "parse.h"
#include "querycmds.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_debug.h"
#include "s_misc.h"
#include "s_user.h"
#include "send.h"
#include "struct.h"
#include "userload.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdlib.h>
#include <string.h>

/** Maximum connection count since last restart. */
unsigned int max_connection_count = 0;
/** Maximum (local) client count since last restart. */
unsigned int max_client_count = 0;
/** Maximum (global) client count since last restart. */
unsigned int max_global_count = 0;
/** Timestamp maximum (local) client count since last restart. */
time_t max_client_count_TS = 0;
/** Timestamp maximum (global) client count since last restart. */
time_t max_global_count_TS = 0;

/** Squit a new (pre-burst) server.
 * @param cptr Local client that tried to introduce the server.
 * @param sptr Server to disconnect.
 * @param host Name of server being disconnected.
 * @param timestamp Link time of server being disconnected.
 * @param pattern Format string for squit message.
 * @return CPTR_KILLED if cptr == sptr, else 0.
 */
int exit_new_server(struct Client *cptr, struct Client *sptr, const char *host,
                    time_t timestamp, const char *pattern, ...)
{
  struct VarData vd;
  int retval = 0;

  vd.vd_format = pattern;
  va_start(vd.vd_args, pattern);

  if (!IsServer(sptr))
    retval = vexit_client_msg(cptr, cptr, &me, pattern, vd.vd_args);
  else
    sendcmdto_one(&me, CMD_SQUIT, cptr, "%s %Tu :%v", host, timestamp, &vd);

  va_end(vd.vd_args);

  return retval;
}

/** Indicate whether \a a is between \a b and #me (that is, \a b would
 * be killed if \a a squits).
 * @param a A server that may be between us and \a b.
 * @param b A client that may be on the far side of \a a.
 * @return Non-zero if \a a is between \a b and #me.
 */
int a_kills_b_too(struct Client *a, struct Client *b)
{
  for (; b != a && b != &me; b = cli_serv(b)->up);
  return (a == b ? 1 : 0);
}

/** Handle a connection that has sent a valid PASS and SERVER.
 * @param cptr New peer server.
 * @param aconf Connect block for \a cptr.
 * @return Zero.
 */
int server_estab(struct Client *cptr, struct ConfItem *aconf)
{
  struct Client* acptr = 0;
  const char*    inpath;
  int            i;

  assert(0 != cptr);
  assert(0 != cli_local(cptr));

  inpath = cli_name(cptr);

  if (IsUnknown(cptr)) {

#if defined(ESNET_NEG)
    envia_config_req(cptr);
#endif
    if (aconf->passwd[0])
      sendrawto_one(cptr, MSG_PASS " :%s", aconf->passwd);
    /*
     *  Pass my info to the new server
     */
#if defined(P09_SUPPORT)
    if (Protocol(cptr) < 10)
      sendrawto_one(cptr, MSG_SERVER " %s 1 %Tu %Tu J%s %s%s 0 :%s",
          cli_name(&me), cli_serv(&me)->timestamp,
          cli_serv(cptr)->timestamp, MAJOR_PROTOCOL, NumServCap(&me),
          *(cli_info(&me)) ? cli_info(&me) : "IRCers United");
    else

#else
      sendrawto_one(cptr, MSG_SERVER " %s 1 %Tu %Tu J%s %s%s +%s6 :%s",
		  cli_name(&me), cli_serv(&me)->timestamp,
		  cli_serv(cptr)->timestamp, MAJOR_PROTOCOL, NumServCap(&me),
		  feature_bool(FEAT_HUB) ? "h" : "",
		  *(cli_info(&me)) ? cli_info(&me) : "IRCers United");
#endif

#if defined(DDB)
    ddb_burst(cptr);
#endif
  }

  det_confs_butmask(cptr, CONF_SERVER);

  if (!IsHandshake(cptr))
    hAddClient(cptr);
  SetServer(cptr);

#if defined(ESNET_NEG)
  config_resolve_speculative(cptr);
#endif

  cli_handler(cptr) = SERVER_HANDLER;
  Count_unknownbecomesserver(UserStats);
#if defined(P09_SUPPORT)
  if (Protocol(cptr) > 9)
#endif
  SetBurst(cptr);

/*    nextping = CurrentTime; */

  /*
   * NOTE: check for acptr->user == cptr->serv->user is necessary to insure
   * that we got the same one... bleah
   */
  if (cli_serv(cptr)->user && *(cli_serv(cptr))->by &&
      (acptr = findNUser(cli_serv(cptr)->by))) {
    if (cli_user(acptr) == cli_serv(cptr)->user) {
      sendcmdto_one(&me, CMD_NOTICE, acptr, "%C :Link with %s established.",
                    acptr, inpath);
    }
    else {
      /*
       * if not the same client, set by to empty string
       */
      acptr = 0;
      *(cli_serv(cptr))->by = '\0';
    }
  }

  sendto_opmask(acptr, SNO_OLDSNO, "Link with %s established.", inpath);
  cli_serv(cptr)->up = &me;
  cli_serv(cptr)->updown = add_dlink(&(cli_serv(&me))->down, cptr);
  sendto_opmask(0, SNO_NETWORK, "Net junction: %s %s", cli_name(&me),
                cli_name(cptr));
  SetJunction(cptr);

#if defined(DDB)
  //ddb_burst(cptr);
#endif

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
  inicia_microburst();
#endif

  /*
   * Old sendto_serv_but_one() call removed because we now
   * need to send different names to different servers
   * (domain name matching) Send new server to other servers.
   */
  for (i = 0; i <= HighestFd; i++)
  {
    if (!(acptr = LocalClientArray[i]) || !IsServer(acptr) ||
        acptr == cptr || IsMe(acptr))
      continue;
    if (!match(cli_name(&me), cli_name(cptr)))
      continue;
#if defined(P09_SUPPORT)
    if (Protocol(acptr) < 10)
      sendcmdto_one(&me, CMD_SERVER, acptr,
          "%s 2 0 %Tu J%02u %s%s 0 :%s", cli_name(cptr),
          cli_serv(cptr)->timestamp, Protocol(cptr), NumServCap(cptr),
          cli_info(cptr));
    else
#endif
      sendcmdto_one(&me, CMD_SERVER, acptr,
		  "%s 2 0 %Tu J%02u %s%s +%s%s%s :%s", cli_name(cptr),
		  cli_serv(cptr)->timestamp, Protocol(cptr), NumServCap(cptr),
		  IsHub(cptr) ? "h" : "", IsService(cptr) ? "s" : "",
		  IsIPv6(cptr) ? "6" : "", cli_info(cptr));
  }

  /* Send these as early as possible so that glined users/juped servers can
   * be removed from the network while the remote server is still chewing
   * our burst.
   */
  sendto_opmask(0, SNO_NETWORK, "Bursting glines");
  gline_burst(cptr);
  sendto_opmask(0, SNO_NETWORK, "Bursting jupes");
  jupe_burst(cptr);

  /*
   * Pass on my client information to the new server
   *
   * First, pass only servers (idea is that if the link gets
   * canceled because the server was already there,
   * there are no NICK's to be canceled...). Of course,
   * if cancellation occurs, all this info is sent anyway,
   * and I guess the link dies when a read is attempted...? --msa
   *
   * Note: Link cancellation to occur at this point means
   * that at least two servers from my fragment are building
   * up connection this other fragment at the same time, it's
   * a race condition, not the normal way of operation...
   */
  sendto_opmask(0, SNO_NETWORK, "Bursting servers");
  for (acptr = &me; acptr; acptr = cli_prev(acptr)) {
    /* acptr->from == acptr for acptr == cptr */
    if (cli_from(acptr) == cptr)
      continue;
    if (IsServer(acptr)) {
      const char* protocol_str;

      if (Protocol(acptr) > 9)
        protocol_str = IsBurst(acptr) ? "J" : "P";
      else
        protocol_str = IsBurst(acptr) ? "J0" : "P0";

      if (0 == match(cli_name(&me), cli_name(acptr)))
        continue;
#if defined(P09_SUPPORT)
      if (Protocol(cptr) < 10)
        sendcmdto_one(cli_serv(acptr)->up, CMD_SERVER, cptr,
            "%s %d 0 %Tu %s%u %s%s 0 :%s", cli_name(acptr),
            cli_hopcount(acptr) + 1, cli_serv(acptr)->timestamp,
            protocol_str, Protocol(acptr), NumServCap(acptr),
            cli_info(acptr));
      else
#endif
        sendcmdto_one(cli_serv(acptr)->up, CMD_SERVER, cptr,
		    "%s %d 0 %Tu %s%u %s%s +%s%s%s :%s", cli_name(acptr),
		    cli_hopcount(acptr) + 1, cli_serv(acptr)->timestamp,
		    protocol_str, Protocol(acptr), NumServCap(acptr),
		    IsHub(acptr) ? "h" : "", IsService(acptr) ? "s" : "",
		    IsIPv6(acptr) ? "6" : "", cli_info(acptr));
    }
  }

  sendto_opmask(0, SNO_NETWORK, "Bursting nicks");
  for (acptr = &me; acptr; acptr = cli_prev(acptr))
  {
    /* acptr->from == acptr for acptr == cptr */
    if (cli_from(acptr) == cptr)
      continue;
    if (IsUser(acptr))
    {
#if defined(P09_SUPPORT)
      if (Protocol(cptr) < 10)
      {
#ifdef MIGRACION_DEEPSPACE_P10
        char xxx_buf[25];

        sendcmdto_one(cli_user(acptr)->server, CMD_NICK, cptr,
              "%s %d %Tu %s %s %s %s :%s",
              cli_name(acptr), cli_hopcount(acptr) + 1, cli_lastnick(acptr),
              cli_user(acptr)->username, cli_user(acptr)->realhost,
              cli_server(acptr)->name,
              iptobase64(xxx_buf, &cli_ip(acptr), sizeof(xxx_buf), IsIPv6(cptr)),
              cli_info(acptr));
#else
        sendcmdto_one(cli_user(acptr)->server, CMD_NICK, cptr,
              "%s %d %Tu %s %s %s :%s",
              cli_name(acptr), cli_hopcount(acptr) + 1, cli_lastnick(acptr),
              cli_user(acptr)->username, cli_user(acptr)->realhost,
              cli_user(acptr)->server->cli_name, cli_info(acptr));
#endif
        send_umode(cptr, acptr, 0, SEND_UMODES);
        send_user_joins(cptr, acptr);
      } else {
        char xxx_buf[25];
        char *s = umode_str(acptr);
        sendcmdto_one(cli_user(acptr)->server, CMD_NICK, cptr,
              "%s %d %Tu %s %s %s%s%s%s %s%s :%s",
              cli_name(acptr), cli_hopcount(acptr) + 1, cli_lastnick(acptr),
              cli_user(acptr)->username, cli_user(acptr)->realhost,
              *s ? "+" : "", s, *s ? " " : "",
              iptobase64(xxx_buf, &cli_ip(acptr), sizeof(xxx_buf), IsIPv6(cptr)),
              NumNick(acptr), cli_info(acptr));


      }
#else
      char xxx_buf[25];
      char *s = umode_str(acptr);
      sendcmdto_one(cli_user(acptr)->server, CMD_NICK, cptr,
		    "%s %d %Tu %s %s %s%s%s%s %s%s :%s",
		    cli_name(acptr), cli_hopcount(acptr) + 1, cli_lastnick(acptr),
		    cli_user(acptr)->username, cli_user(acptr)->realhost,
		    *s ? "+" : "", s, *s ? " " : "",
		    iptobase64(xxx_buf, &cli_ip(acptr), sizeof(xxx_buf), IsIPv6(cptr)),
		    NumNick(acptr), cli_info(acptr));
#endif
    }
  }
  /*
   * Last, send the BURST.
   * (Or for 2.9 servers: pass all channels plus statuses)
   */
  sendto_opmask(0, SNO_NETWORK, "Bursting channels");
  {
    struct Channel *chptr;
    for (chptr = GlobalChannelList; chptr; chptr = chptr->next)
      send_channel_modes(cptr, chptr);
  }

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
  completa_microburst();
#endif

#if defined(P09_SUPPORT)
  if (Protocol(cptr) > 9)
#endif
  sendcmdto_one(&me, CMD_END_OF_BURST, cptr, "");
  return 0;
}
