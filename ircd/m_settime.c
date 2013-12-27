/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_settime.c
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
 * @brief Handlers for the SETTIME command.
 * @version $Id: m_settime.c,v 1.9 2007-04-21 21:17:23 zolty Exp $
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
#include "list.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_user.h"
#include "send.h"
#include "struct.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdlib.h>

/** Handle a SETTIME message from a server.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the new network time
 * \li \a parv[2] (optional) is the target server to apply it to
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_settime(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  time_t t;
  long dt;
  static char tbuf[11];
  struct DLink *lp;

  if (parc < 2) /* verify argument count */
    return need_more_params(sptr, "SETTIME");

  t = atoi(parv[1]); /* convert time and compute delta */
  dt = TStime() - t;

  /* verify value */
  if (t < OLDEST_TS || dt < -9000000)
  {
    if (IsServer(sptr)) /* protocol violation if it's from a server */
      protocol_violation(sptr, "SETTIME: Bad value (%Tu, delta %ld)", t, dt);
    else
      sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :SETTIME: Bad value (%Tu, "
                    "delta %ld)", sptr, t, dt);
      sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :SETTIME: Bad value", sptr);
    return 0;
  }

  /* reset time... */
  if (feature_bool(FEAT_RELIABLE_CLOCK))
  {
    ircd_snprintf(0, tbuf, sizeof(tbuf), "%Tu", TStime());
    parv[1] = tbuf;
  }

  if (BadPtr(parv[2])) /* spam the network */
  {
    for (lp = cli_serv(&me)->down; lp; lp = lp->next)
      if (cptr != lp->value.cptr)
        sendcmdto_prio_one(sptr, CMD_SETTIME, lp->value.cptr, "%s", parv[1]);
  }
  else
  {
    if (hunt_server_prio_cmd(sptr, CMD_SETTIME, cptr, 1, "%s %C", 2, parc,
                             parv) != HUNTED_ISME)
    {
      /* If the destination was *not* me, but I'm RELIABLE_CLOCK and the
       * delta is more than 30 seconds off, bounce back a corrected
       * SETTIME
       */
      if (feature_bool(FEAT_RELIABLE_CLOCK) && (dt > 30 || dt < -30))
        sendcmdto_prio_one(&me, CMD_SETTIME, cptr, "%s %C", parv[1], cptr);
      return 0;
    }
  }

  if (feature_bool(FEAT_RELIABLE_CLOCK))
  {
    /* don't apply settime--reliable */
    if ((dt > 600) || (dt < -600))
      sendcmdto_serv(&me, CMD_DESYNCH, 0, ":Bad SETTIME from %s: %Tu "
                     "(delta %ld)", cli_name(sptr), t, dt);
    /* Let user know we're ignoring him */
    if (IsUser(sptr))
    {
      sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :clock is not set %ld "
		    "seconds %s : RELIABLE_CLOCK is defined", sptr,
		    (dt < 0) ? -dt : dt, (dt < 0) ? "forwards" : "backwards");
    }
  }
  else /* tell opers about time change */
  {
    sendto_opmask(0, SNO_OLDSNO, "SETTIME from %s, clock is set %ld "
                  "seconds %s", cli_name(sptr), (dt < 0) ? -dt : dt,
                  (dt < 0) ? "forwards" : "backwards");
    /* Apply time change... */
    TSoffset -= dt;
    /* Let the issuing user know what we did... */
    if (IsUser(sptr))
    {
      sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :clock is set %ld seconds %s",
                    sptr, (dt < 0) ? -dt : dt,
                    (dt < 0) ? "forwards" : "backwards");
    }
  }

  return 0;
}

/** Handle a SETTIME message from an operator.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the new network time
 * \li \a parv[2] (optional) is the target server to apply it to
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int mo_settime(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  time_t t;
  long dt;
  static char tbuf[11];

  /* Must be a global oper */
  if (!IsAnOper(sptr))
    return send_reply(sptr, ERR_NOPRIVILEGES);

  if (parc < 2) /* verify argument count */
    return need_more_params(sptr, "SETTIME");

  if (parc == 2 && MyUser(sptr)) /* default to me */
    parv[parc++] = cli_name(&me);

  t = atoi(parv[1]); /* convert the time */

  /* If we're reliable_clock or if the oper specified a 0 time, use current */
  if (!t || feature_bool(FEAT_RELIABLE_CLOCK))
  {
    t = TStime();
    ircd_snprintf(0, tbuf, sizeof(tbuf), "%Tu", TStime());
    parv[1] = tbuf;
  }

  dt = TStime() - t; /* calculate the delta */

  if (t < OLDEST_TS || dt < -9000000) /* verify value */
  {
    sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :SETTIME: Bad value", sptr);
    return 0;
  }

  /* OK, send the message off to its destination */
  if (hunt_server_prio_cmd(sptr, CMD_SETTIME, cptr, 1, "%s %C", 2, parc,
                           parv) != HUNTED_ISME)
    return 0;

  if (feature_bool(FEAT_RELIABLE_CLOCK)) /* don't apply settime--reliable */
  {
    if ((dt > 600) || (dt < -600))
      sendcmdto_serv(&me, CMD_DESYNCH, 0, ":Bad SETTIME from %s: %Tu "
                     "(delta %ld)", cli_name(sptr), t, dt);
    if (IsUser(sptr)) /* Let user know we're ignoring him */
      sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :clock is not set %ld seconds "
                    "%s: RELIABLE_CLOCK is defined", sptr, (dt < 0) ? -dt : dt,
                    (dt < 0) ? "forwards" : "backwards");
  }
  else /* tell opers about time change */
  {
    sendto_opmask(0, SNO_OLDSNO, "SETTIME from %s, clock is set %ld "
                  "seconds %s", cli_name(sptr), (dt < 0) ? -dt : dt,
                  (dt < 0) ? "forwards" : "backwards");
    TSoffset -= dt; /* apply time change */
    if (IsUser(sptr)) /* let user know what we did */
      sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :clock is set %ld seconds %s",
		    sptr, (dt < 0) ? -dt : dt,
		    (dt < 0) ? "forwards" : "backwards");
  }

  return 0;
}
