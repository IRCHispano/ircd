/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_trace.c
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
 * @brief Handlers for the TRACE command.
 * @version $Id: m_trace.c,v 1.11 2007-04-21 21:17:23 zolty Exp $
 */
#include "config.h"

#include "class.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "match.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_user.h"
#include "send.h"
#include "version.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <string.h>

static
void do_trace(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  int i;
  struct Client *acptr;
  struct Client *acptr2;
  const struct ConnectionClass* cl;
  char* tname;
  int doall;
  int *link_s;
  int *link_u;
  int cnt = 0;
  int wilds;
  int dow;

  if (parc < 2 || BadPtr(parv[1]))
  {
    /* just "TRACE" without parameters. Must be from local client */
    parc = 1;
    acptr = &me;
    tname = cli_name(&me);
    i = HUNTED_ISME;
  }
  else if (parc < 3 || BadPtr(parv[2]))
  {
    /* No target specified. Make one before propagating. */
    parc = 2;
    tname = parv[1];
    if ((acptr = find_match_server(parv[1])) ||
        ((acptr = FindClient(parv[1])) && !MyUser(acptr)))
    {
      if (IsUser(acptr))
        parv[2] = cli_name(cli_user(acptr)->server);
      else
        parv[2] = cli_name(acptr);
      parc = 3;
      parv[3] = 0;
      if ((i = hunt_server_cmd(sptr, CMD_TRACE, cptr, IsServer(acptr),
			       "%s :%C", 2, parc, parv)) == HUNTED_NOSUCH)
        return;
    }
    else
      i = HUNTED_ISME;
  } else {
    /* Got "TRACE <tname> :<target>" */
    parc = 3;
    if (MyUser(sptr) || Protocol(cptr) < 10)
      acptr = find_match_server(parv[2]);
    else
      acptr = FindNServer(parv[2]);
    if ((i = hunt_server_cmd(sptr, CMD_TRACE, cptr, 0, "%s :%C", 2, parc,
			     parv)) == HUNTED_NOSUCH)
      return;
    tname = parv[1];
  }

  if (i == HUNTED_PASS) {
    if (!acptr)
      acptr = next_client(GlobalClientList, tname);
    else
      acptr = cli_from(acptr);
    send_reply(sptr, RPL_TRACELINK,
	       version, debugmode, tname,
	       acptr ? cli_name(cli_from(acptr)) : "<No_match>");
    return;
  }

  doall = (parv[1] && (parc > 1)) ? !match(tname, cli_name(&me)) : 1;
  wilds = !parv[1] || strchr(tname, '*') || strchr(tname, '?');
  dow = wilds || doall;

  /* Don't give (long) remote listings to lusers */
  if (dow && !MyConnect(sptr) && !IsAnOper(sptr)) {
    send_reply(sptr, RPL_TRACEEND);
    return;
  }

  link_s = MyCalloc(2 * maxconnections, sizeof(link_s[0]));
  link_u = link_s + maxconnections;

  if (doall) {
    for (acptr = GlobalClientList; acptr; acptr = cli_next(acptr)) {
      if (IsUser(acptr))
        link_u[cli_fd(cli_from(acptr))]++;
      else if (IsServer(acptr))
        link_s[cli_fd(cli_from(acptr))]++;
    }
  }

  /* report all direct connections */

  for (i = 0; i <= HighestFd; i++) {
    const char *conClass;

    if (!(acptr = LocalClientArray[i])) /* Local Connection? */
      continue;
    if (IsInvisible(acptr) && dow && !(MyConnect(sptr) && IsAnOper(sptr)) &&
        !IsAnOper(acptr) && (acptr != sptr))
      continue;
    if (!doall && wilds && match(tname, cli_name(acptr)))
      continue;
    if (!dow && 0 != ircd_strcmp(tname, cli_name(acptr)))
      continue;

    conClass = get_client_class(acptr);

    switch (cli_status(acptr)) {
      case STAT_CONNECTING:
	send_reply(sptr, RPL_TRACECONNECTING, conClass, cli_name(acptr));
        cnt++;
        break;
      case STAT_HANDSHAKE:
	send_reply(sptr, RPL_TRACEHANDSHAKE, conClass, cli_name(acptr));
        cnt++;
        break;
      case STAT_ME:
        break;
      case STAT_UNKNOWN:
      case STAT_UNKNOWN_USER:
	send_reply(sptr, RPL_TRACEUNKNOWN, conClass,
		   get_client_name(acptr, HIDE_IP));
        cnt++;
        break;
      case STAT_UNKNOWN_SERVER:
	send_reply(sptr, RPL_TRACEUNKNOWN, conClass, "Unknown Server");
        cnt++;
        break;
      case STAT_USER:
        /* Only opers see users if there is a wildcard
           but anyone can see all the opers. */
        if ((IsAnOper(sptr) && (MyUser(sptr) ||
            !(dow && IsInvisible(acptr)))) || !dow || IsAnOper(acptr)) {
          if (IsAnOper(acptr))
	    send_reply(sptr, RPL_TRACEOPERATOR, conClass,
		       get_client_name(acptr, SHOW_IP),
		       CurrentTime - cli_lasttime(acptr));
          else
	    send_reply(sptr, RPL_TRACEUSER, conClass,
		       get_client_name(acptr, SHOW_IP),
		       CurrentTime - cli_lasttime(acptr));
          cnt++;
        }
        break;
        /*
         * Connection is a server
         *
         * Serv <class> <nS> <nC> <name> <ConnBy> <last> <age>
         *
         * class        Class the server is in
         * nS           Number of servers reached via this link
         * nC           Number of clients reached via this link
         * name         Name of the server linked
         * ConnBy       Who established this link
         * last         Seconds since we got something from this link
         * age          Seconds this link has been alive
         *
         * Additional comments etc......        -Cym-<cym@acrux.net>
         */

      case STAT_SERVER:
        if (cli_serv(acptr)->user) {
          if (!cli_serv(acptr)->by[0]
              || !(acptr2 = findNUser(cli_serv(acptr)->by))
              || (cli_user(acptr2) != cli_serv(acptr)->user))
            acptr2 = NULL;
	  send_reply(sptr, RPL_TRACESERVER, conClass, link_s[i],
                     link_u[i], cli_name(acptr),
                     acptr2 ? cli_name(acptr2) : "*",
                     cli_serv(acptr)->user->username,
                     cli_serv(acptr)->user->host,
                     CurrentTime - cli_lasttime(acptr),
                     CurrentTime - cli_serv(acptr)->timestamp);
        } else
	  send_reply(sptr, RPL_TRACESERVER, conClass, link_s[i],
                     link_u[i], cli_name(acptr),
                     (*(cli_serv(acptr))->by) ?  cli_serv(acptr)->by : "*", "*",
                     cli_name(&me), CurrentTime - cli_lasttime(acptr),
		     CurrentTime - cli_serv(acptr)->timestamp);
        cnt++;
        break;
      default:                  /* We actually shouldn't come here, -msa */
	send_reply(sptr, RPL_TRACENEWTYPE, get_client_name(acptr, HIDE_IP));
        cnt++;
        break;
    }
  }
  /*
   * Add these lines to summarize the above which can get rather long
   * and messy when done remotely - Avalon
   */
  if (IsAnOper(sptr) && doall) {
    for (cl = get_class_list(); cl; cl = cl->next) {
      if (Links(cl) > 1)
       send_reply(sptr, RPL_TRACECLASS, ConClass(cl), Links(cl) - 1);
    }
  }
  send_reply(sptr, RPL_TRACEEND);
  MyFree(link_s);
}

/** Handle a TRACE message from a local luser.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the nick or servername to trace
 * \li \a parv[2] is the optional 'target' server to trace from
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int m_trace(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  if (feature_bool(FEAT_HIS_TRACE))
    return send_reply(cptr, ERR_NOPRIVILEGES);
  do_trace(cptr, sptr, parc, parv);
  return 0;
}

/** Handle a TRACE message from a server.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the nick or servername to trace
 * \li \a parv[2] is the mandatory 'target' server to trace from
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_trace(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  do_trace(cptr, sptr, parc, parv);
  return 0;
}

/** Handle a TRACE message from a local oper.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the nick or servername to trace
 * \li \a parv[2] is the optional 'target' server to trace from
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int mo_trace(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  if (feature_bool(FEAT_HIS_TRACE) && !IsAnOper(sptr))
    return send_reply(cptr, ERR_NOPRIVILEGES);
  do_trace(cptr, sptr, parc, parv);
  return 0;
}
