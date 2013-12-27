/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ircd_reply.c
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
 * @brief Implementation of functions to send common replies to users.
 * @version $Id: ircd_reply.c,v 1.7 2007-04-21 21:17:22 zolty Exp $
 */
#include "config.h"

#include "ircd_reply.h"
#include "client.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_snprintf.h"
#include "msg.h"
#include "msgq.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_debug.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <string.h>

/** Report a protocol violation warning to anyone listening.  This can
 * be easily used to clean up the last couple of parts of the code.
 * @param[in] cptr Client that violated the protocol.
 * @param[in] pattern Description of how the protocol was violated.
 * @return Zero.
 */
int protocol_violation(struct Client* cptr, const char* pattern, ...)
{
  struct VarData vd;
  char message[BUFSIZE];

  assert(pattern);
  assert(cptr);

  vd.vd_format = pattern;
  va_start(vd.vd_args, pattern);
  ircd_snprintf(NULL, message, sizeof(message),
                   "Protocol Violation from %s: %v", cli_name(cptr), &vd);
  va_end(vd.vd_args);

  sendwallto_group(&me, WALL_DESYNCH, NULL, "%s", message);
  return 0;
}

/** Inform a client that they need to provide more parameters.
 * @param[in] cptr Taciturn client.
 * @param[in] cmd Command name.
 * @return Zero.
 */
int need_more_params(struct Client* cptr, const char* cmd)
{
  send_reply(cptr, ERR_NEEDMOREPARAMS, cmd);
  return 0;
}

/** Send a generic reply to a user.
 * @param[in] to Client that wants a reply.
 * @param[in] reply Numeric of message to send.
 * @return Zero.
 */
int send_reply(struct Client *to, int reply, ...)
{
  struct VarData vd;
  struct MsgBuf *mb;
  const struct Numeric *num;

  assert(0 != to);
  assert(0 != reply);

  num = get_error_numeric(reply & ~SND_EXPLICIT); /* get reply... */

  va_start(vd.vd_args, reply);

  if (reply & SND_EXPLICIT) /* get right pattern */
    vd.vd_format = (const char *) va_arg(vd.vd_args, char *);
  else
    vd.vd_format = num->format;

  assert(0 != vd.vd_format);

  /* build buffer */
  mb = msgq_make(cli_from(to), "%:#C %s %C %v", &me, num->str, to, &vd);

  va_end(vd.vd_args);

  /* send it to the user */
  send_buffer(to, mb, 0);

  msgq_clean(mb);

  return 0; /* convenience return */
}
