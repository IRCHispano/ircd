/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_proto.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2000 Kevin L. Mitchell <klmitch@mit.edu>
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
 * @brief Handlers for PROTO command.
 * @version $Id: m_proto.c,v 1.6 2007-04-19 22:53:49 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_alloc.h"
#include "ircd_chattr.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_debug.h"
#include "s_misc.h"
#include "send.h"
#include "version.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const PROTO_REQ = "REQ";
static const char* const PROTO_ACK = "ACK";
static const char* const PROTO_SUP = "SUP";


static
int proto_handle_ack(struct Client* cptr, const char* msg)
{
  /*
   * handle ack here, if option and args supported
   * start option
   */
  return 0;
}

static
int proto_handle_req(struct Client* cptr, const char* msg)
{
  /*
   * handle request here if not supported args send
   * option info. otherwise send ack
   */
  return 0;
}

static
int proto_send_supported(struct Client* cptr)
{
  /*
   * send_reply(cptr, RPL_PROTOLIST, "stuff");
   */
  sendcmdto_one(&me, CMD_PROTO, cptr, "%s unet1 1 1", PROTO_SUP);
  return 0;
}

int m_proto(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  if (0 == parc)
    return proto_send_supported(cptr);

  if (parc < 3)
    return need_more_params(sptr, "PROTO");

  if (0 == ircd_strcmp(PROTO_REQ, parv[1]))
    return proto_handle_req(cptr, parv[2]);

  else if (0 == ircd_strcmp(PROTO_ACK, parv[1]))
    return proto_handle_ack(cptr, parv[2]);

  else if (0 == ircd_strcmp(PROTO_SUP, parv[1]))
    return 0; /* ignore it */

  return 0;
}
