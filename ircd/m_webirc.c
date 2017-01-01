/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_webirc.c
 *
 * Copyright (C) 2002-2015 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2014 Toni Garcia (zoltan) <zoltan@irc-dev.net>
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
 * @brief Handlers for WEBIRC command.
 * @version $Id$
 */
#include "config.h"

#include "client.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "s_auth.h"
#include "s_conf.h"
#include "s_misc.h"

#include <string.h>

/*
 * m_webirc
 *
 * parv[0] = sender prefix
 * parv[1] = password
 * parv[2] = ident
 * parv[3] = hostname
 * parv[4] = ip
 */
int m_webirc(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  const struct wline *wline;
  char mod_ident[USERLEN+1];
  const char *passwd;
  const char *ident;
  const char *hostname;
  const char *ip;

  if (!IsWebircPort(cptr))
    return exit_client(cptr, cptr, &me, "Use a different port");

  if (parc < 5)
    return need_more_params(sptr, "WEBIRC");

  passwd = parv[1];
  ident = parv[2];
  hostname = parv[3];
  ip = parv[4];

  if (EmptyString(ip))
    return exit_client(cptr, cptr, &me, "WEBIRC needs IP address");

  if (!(wline = find_webirc(&cli_ip(sptr), passwd)))
    return exit_client_msg(cptr, cptr, &me, "WEBIRC not authorized");
  cli_wline(sptr) = wline;

  /* Treat client as a normally connecting user from now on. */
  cli_status(sptr) = STAT_UNKNOWN_USER;

  /* If FEAT_HIS_WEBIRC is off, prefix ident with ^. */
  if (!feature_bool(FEAT_HIS_WEBIRC)) {
      mod_ident[0] = '^';
      strncpy(mod_ident + 1, ident, sizeof(mod_ident) - 1);
      mod_ident[sizeof(mod_ident) - 1] = '\0';
      ident = mod_ident;
  }

  int res = auth_spoof_user(cli_auth(cptr), ident, hostname, ip);
  if (res > 0)
    return exit_client(cptr, cptr, &me, "WEBIRC invalid spoof");
  return res;
}
