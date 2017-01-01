/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/s_numeric.c
 *
 * Copyright (C) 2002-2017 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1999 Andrea Cocito
 * Copyright (C) 1993 Markku Savela
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
 * @brief Send a numeric message to a client.
 * @version $Id$
 */
#include "config.h"

#include "s_numeric.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_snprintf.h"
#include "numnicks.h"
#include "send.h"
#include "struct.h"


/*
 * do_numeric()
 * Rewritten by Nemesi, Jan 1999, to support numeric nicks in parv[1]
 *
 * Called when we get a numeric message from a remote _server_ and we are
 * supposed to forward it somewhere. Note that we always ignore numerics sent
 * to 'me' and simply drop the message if we can't handle with this properly:
 * the savvy approach is NEVER generate an error in response to an... error :)
 */

/** Forwards a numeric message from a remote server.
 *
 * For this function, parv[2] may be special in the sense that it may
 * actually be multiple parameters.  parse_server() has special case
 * code to do this when it parses numeric messages.
 *
 * @param numeric Value of numeric message.
 * @param nnn If non-zero, treat parv[1] as a numnick; else as a client name.
 * @param cptr Client that originated the numeric.
 * @param sptr Peer that sent us the numeric.
 * @param parc Count of valid arguments in \a parv.
 * @param parv Argument list.
 * @return Zero (always).
 */
int do_numeric(int numeric, int nnn, struct Client *cptr, struct Client *sptr,
    int parc, char *parv[])
{
  struct Client *acptr = 0;
  struct Channel *achptr = 0;
  int send_flags = SKIP_DEAF | SKIP_BURST;
  char num[4];

  /* Avoid trash, we need it to come from a server and have a target  */
  if ((parc < 2) || !IsServer(sptr))
    return 0;

  /* Who should receive this message ? Will we do something with it ?
     Note that we use findUser functions, so the target can't be neither
     a server, nor a channel (?) nor a list of targets (?) .. u2.10
     should never generate numeric replies to non-users anyway
     Ahem... it can be a channel actually, csc bots use it :\ --Nem */

  if (parv[1][0] == '@' && IsChannelName(parv[1] + 1))
  {
    send_flags |= SKIP_NONOPS;
    achptr = FindChannel(parv[1] + 1);
  }
  else if (IsChannelName(parv[1]))
    achptr = FindChannel(parv[1]);
  else
    acptr = (nnn) ? (findNUser(parv[1])) : (FindUser(parv[1]));

  if (((!acptr) || (cli_from(acptr) == cptr)) && !achptr)
    return 0;

  /* Remap low number numerics, not that I understand WHY.. --Nemesi  */
  /* numerics below 100 talk about the current 'connection', you're not
   * connected to a remote server so it doesn't make sense to send them
   * remotely - but the information they contain may be useful, so we
   * remap them up.  Weird, but true.  -- Isomer */
  if (numeric < 100)
    numeric += 100;

  ircd_snprintf(0, num, sizeof(num), "%03d", numeric);

  /* Since 2.10.10.pl14 we rewrite numerics from remote servers to appear to
   * come from the local server
   */
  if (acptr)
    sendcmdto_one((feature_bool(FEAT_HIS_REWRITE) && !IsAnOper(acptr)) ?
                    &me : sptr,
                  num, num, acptr, "%C %s", acptr, parv[2]);
  else
    sendcmdto_channel(feature_bool(FEAT_HIS_REWRITE) ? &me : sptr,
                      num, num, achptr, cptr, send_flags,
                      "%H %s", achptr, parv[2]);
  return 0;
}
