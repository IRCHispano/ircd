/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_kill.c
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
 * @brief Handlers for KILL command.
 * @version $Id: m_kill.c,v 1.11 2007-09-20 21:00:32 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_snprintf.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_misc.h"
#include "send.h"
#include "whowas.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <string.h>

/** Perform the generic work involved in killing a client.
 * @param[in] cptr Client that sent us the KILL.
 * @param[in] sptr Original source of the KILL.
 * @param[in] victim Client to be killed.
 * @param[in] inpath If cptr is a server, its name; else cptr's (true)
 *   hostname.
 * @param[in] path If cptr is a server, the kill path it advertised;
 *   else cptr's nickname.
 * @param[in] msg Kill message
 * @return Has a tail call to exit_client_msg().
 */
static int do_kill(struct Client* cptr, struct Client* sptr,
		   struct Client* victim, char* inpath, char* path, char* msg)
{
  assert(0 != cptr);
  assert(0 != sptr);
  assert(!IsServer(victim));

  /*
   * Notify all *local* opers about the KILL (this includes the one
   * originating the kill, if from this server--the special numeric
   * reply message is not generated anymore).
   *
   * Note: "victim->name" is used instead of "user" because we may
   *       have changed the target because of the nickname change.
   */
  sendto_opmask(0, IsServer(sptr) ? SNO_SERVKILL : SNO_OPERKILL,
                "Received KILL message for %s from %s Path: %s!%s %s",
                get_client_name(victim, SHOW_IP), cli_name(sptr),
                inpath, path, msg);
  log_write_kill(victim, sptr, inpath, path, msg);

  /*
   * And pass on the message to other servers. Note, that if KILL
   * was changed, the message has to be sent to all links, also
   * back.
   * Client suicide kills are NOT passed on --SRB
   */
  if (IsServer(cptr) || !MyConnect(victim)) {
    sendcmdto_serv(sptr, CMD_KILL, cptr, "%C :%s!%s %s", victim,
                   inpath, path, msg);

    /*
     * Set FLAG_KILLED. This prevents exit_one_client from sending
     * the unnecessary QUIT for this. (This flag should never be
     * set in any other place)
     */
    SetFlag(victim, FLAG_KILLED);
  }

  /*
   * Tell the victim she/he has been zapped, but *only* if
   * the victim is on current server--no sense in sending the
   * notification chasing the above kill, it won't get far
   * anyway (as this user don't exist there any more either)
   * In accordance with the new hiding rules, the victim
   * always sees the kill as coming from me.
   */
  if (MyConnect(victim))
    sendcmdto_one(feature_bool(FEAT_HIS_KILLWHO) ? &his : sptr, CMD_KILL,
		  victim, "%C :%s %s", victim, feature_bool(FEAT_HIS_KILLWHO)
		  ? feature_str(FEAT_HIS_SERVERNAME) : cli_name(sptr), msg);
  return exit_client_msg(cptr, victim, feature_bool(FEAT_HIS_KILLWHO)
			 ? &me : sptr, "Killed (%s %s)",
			 feature_bool(FEAT_HIS_KILLWHO) ?
			 feature_str(FEAT_HIS_SERVERNAME) : cli_name(sptr),
			 msg);
}

/** Handle a KILL message from a server connection.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the numnick of the user to kill
 * \li \a parv[\a parc - 1] is the kill path, followed by a space,
 *   followed by the kill message (traditionally in parentheses)
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_kill(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Client* victim;
  char*          path;
  char*          msg;

  assert(0 != cptr);
  assert(0 != sptr);
  assert(IsServer(cptr));

  /*
   * XXX - a server sending less than 3 params could really desync
   * things
   */
  if (parc < 3) {
    protocol_violation(sptr,"Too few arguments for KILL");
    return need_more_params(sptr, "KILL");
  }

  path = parv[parc - 1];        /* Either defined or NULL (parc >= 3) */

  if (!(msg = strchr(path, ' '))) /* Extract out the message */
    msg = "(No reason supplied)";
  else
    *(msg++) = '\0'; /* Remove first character (space) and terminate path */

  if (!(victim = findNUser(parv[1]))) {
    if (IsUser(sptr))
      sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :KILL target disconnected "
		    "before I got him :(", sptr);
    return 0;
  }

  /*
   * We *can* have crossed a NICK with this numeric... --Run
   *
   * Note the following situation:
   *  KILL SAA -->       X
   *  <-- S NICK ... SAA | <-- SAA QUIT <-- S NICK ... SAA <-- SQUIT S
   * Where the KILL reaches point X before the QUIT does.
   * This would then *still* cause an orphan because the KILL doesn't reach S
   * (because of the SQUIT), the QUIT is ignored (because of the KILL)
   * and the second NICK ... SAA causes an orphan on the server at the
   * right (which then isn't removed when the SQUIT arrives).
   * Therefore we still need to detect numeric nick collisions too.
   *
   * Bounce the kill back to the originator, if the client can't be found
   * by the next hop (short lag) the bounce won't propagate further.
   */
  if (MyConnect(victim)) {
    sendcmdto_one(&me, CMD_KILL, cptr, "%C :%s (Ghost 5 Numeric Collided)",
                  victim, path);
  }
  return do_kill(cptr, sptr, victim, cli_name(cptr), path, msg);
}

/** Handle a KILL message from an operator.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the nickname of the user to kill
 * \li \a parv[\a parc - 1] is the kill message
 *
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int mo_kill(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Client* victim;
  char*          user;
  char           msg[TOPICLEN + 3]; /* (, ), and \0 */

  assert(0 != cptr);
  assert(0 != sptr);
  /*
   * oper connection to this server, cptr is always sptr
   */
  assert(cptr == sptr);
  assert(IsAnOper(sptr));

  if (parc < 3 || EmptyString(parv[parc - 1]))
    return need_more_params(sptr, "KILL");

  user = parv[1];
  ircd_snprintf(0, msg, sizeof(msg), "(%.*s)", QUITLEN, parv[parc - 1]);

  if (!(victim = FindClient(user))) {
    /*
     * If the user has recently changed nick, we automatically
     * rewrite the KILL for this new nickname--this keeps
     * servers in synch when nick change and kill collide
     */
    if (!(victim = get_history(user, (long)15)))
      return send_reply(sptr, ERR_NOSUCHNICK, user);

    sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :Changed KILL %s into %s", sptr,
		  user, cli_name(victim));
  }
  if (!HasPriv(sptr, MyConnect(victim) ? PRIV_LOCAL_KILL : PRIV_KILL))
    return send_reply(sptr, ERR_NOPRIVILEGES);

  if (IsServer(victim) || IsMe(victim)) {
    return send_reply(sptr, ERR_CANTKILLSERVER);
  }
  /*
   * if the user is +k, prevent a kill from local user
   */
  if (IsChannelService(victim))
    return send_reply(sptr, ERR_ISCHANSERVICE, "KILL", cli_name(victim));


  if (!MyConnect(victim) && !HasPriv(sptr, PRIV_KILL)) {
    sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :Nick %s isn't on your server",
		  sptr, cli_name(victim));
    return 0;
  }
  return do_kill(cptr, sptr, victim, cli_user(sptr)->host, cli_name(sptr),
		 msg);
}
