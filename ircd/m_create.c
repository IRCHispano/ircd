/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_create.c
 *
 * Copyright (C) 2002-2015 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1995-1997 Carlo Wood <carlo@runaway.xs4all.nl>
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
 * @brief Handlers for CREATE command.
 * @version $Id: m_create.c,v 1.12 2007-09-20 21:00:32 zolty Exp $
 */
#include "config.h"

#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_debug.h"
#include "s_misc.h"
#include "s_user.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdlib.h>
#include <string.h>

/** Handle a CREATE message from a server.
 * Atomically creates a new channel and ops the creator.
 *
 * \a parv has the following elements:
 * \li \a parv[1] Comma-separated list of channels to create.
 * \li \a parv[2] Creation timestamp for the channel(s).
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_create(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  time_t chanTS; /* channel creation time */
  char *p; /* strtok state */
  char *name; /* channel name */
  struct Channel *chptr; /* channel */
  struct JoinBuf join; /* join and create buffers */
  struct JoinBuf create;
  struct ModeBuf mbuf; /* a mode buffer */
  int badop; /* a flag */

  if (IsServer(sptr))
    return protocol_violation(sptr,"%s tried to CREATE a channel", cli_name(sptr));

  /* sanity checks: Only accept CREATE messages from servers */
  if (parc < 3 || *parv[2] == '\0')
    return need_more_params(sptr,"CREATE");

  chanTS = atoi(parv[2]);

  /* A create that didn't appear during a burst has that servers idea of
   * the current time.  Use it for lag calculations.
   */
  if (!IsBurstOrBurstAck(sptr) && 0 != chanTS)
    cli_serv(cli_user(sptr)->server)->lag = TStime() - chanTS;

  /* If this server is >1 minute fast, warn */
  if (TStime() - chanTS<-60)
  {
    static time_t rate;
    sendto_opmask_ratelimited(0, SNO_NETWORK, &rate,
                              "Timestamp drift from %C (%is); issuing "
                              "SETTIME to correct this",
                              cli_user(sptr)->server,
                              chanTS - TStime());
    /* Now issue a SETTIME to resync.  If we're in the wrong, our
     * (RELIABLE_CLOCK) hub will bounce a SETTIME back to us.
     */
    sendcmdto_prio_one(&me, CMD_SETTIME, cli_user(sptr)->server,
                       "%Tu %C", TStime(), cli_user(sptr)->server);
  }

  joinbuf_init(&join, sptr, cptr, JOINBUF_TYPE_JOIN, 0, 0);
  joinbuf_init(&create, sptr, cptr, JOINBUF_TYPE_CREATE, 0, chanTS);

  /* For each channel in the comma separated list: */
  for (name = ircd_strtok(&p, parv[1], ","); name;
       name = ircd_strtok(&p, 0, ",")) {
    badop = 0;

    if (IsLocalChannel(name))
      continue;

    if ((chptr = FindChannel(name)))
    {
      /* Is the remote server confused? */
      if (find_member_link(chptr, sptr)) {
        protocol_violation(sptr, "%s tried to CREATE a channel already joined", cli_name(sptr));
        continue;
      }

      /* Check if we need to bounce a mode */
      if (TStime() - chanTS > TS_LAG_TIME ||
	  (chptr->creationtime && chanTS > chptr->creationtime &&
	   /* Accept CREATE for zannels. This is only really necessary on a network
	      with servers prior to 2.10.12.02: we just accept their TS and ignore
	      the fact that it was a zannel. The influence of this on a network
	      that is completely 2.10.12.03 or higher is neglectable: Normally
	      a server only sends a CREATE after first sending a DESTRUCT. Thus,
	      by receiving a CREATE for a zannel one of three things happened:
	      1. The DESTRUCT was sent during a net.break; this could mean that
	         our zannel is at the verge of expiring too, it should have been
		 destructed. It is correct to copy the newer TS now, all modes
		 already have been reset, so it will be as if it was destructed
		 and immediately recreated. In order to avoid desyncs of modes,
		 we don't accept a CREATE for channels that have +A set.
	      2. The DESTRUCT passed, then someone created the channel on our
	         side and left it again. In this situation we have a near
		 simultaneous creation on two servers; the person on our side
		 already left within the time span of a message propagation.
		 The channel will therefore be less than 48 hours old and no
		 'protection' is necessary.
              3. The source server sent the CREATE while linking,
                 before it got the BURST for our zannel.  If this
                 happens, we should reset the channel back to the old
                 timestamp.  This can be distinguished from case #1 by
                 checking IsBurstOrBurstAck(cli_user(sptr)->server).
	    */
#if defined(UNDERNET)
	   !(chptr->users == 0 && !chptr->mode.apass[0]))) {
#else
          !(chptr->users == 0))) {
#endif
        if (!IsBurstOrBurstAck(cli_user(sptr)->server)) {
          modebuf_init(&mbuf, sptr, cptr, chptr,
                       (MODEBUF_DEST_SERVER |  /* Send mode to server */
                        MODEBUF_DEST_HACK2  |  /* Send a HACK(2) message */
                        MODEBUF_DEST_BOUNCE)); /* And bounce the mode */

#if defined(UNDERNET)
          modebuf_mode_client(&mbuf, MODE_ADD | MODE_CHANOP, sptr, MAXOPLEVEL + 1);
#else
          modebuf_mode_client(&mbuf, MODE_ADD | MODE_CHANOP, sptr, 0);
#endif

          modebuf_flush(&mbuf);

          badop = 1;
        } else if (chanTS > chptr->creationtime + 4) {
          /* If their handling of the BURST will lead to deopping the
           * user, have the user join without getting ops (if the
           * server's handling of the BURST keeps their ops, the channel
           * will use our timestamp).
           */
          badop = 1;
        }

        if (badop)
          joinbuf_join(&join, chptr, 0);
      }
    }
    else /* Channel doesn't exist: create it */
      chptr = get_channel(sptr, name, CGT_CREATE);

    if (!badop) {
      /* Set (or correct) our copy of the TS */
      chptr->creationtime = chanTS;
      joinbuf_join(&create, chptr, CHFL_CHANOP);
    }
  }

  joinbuf_flush(&join); /* flush out the joins and creates */
  joinbuf_flush(&create);

  return 0;
}
