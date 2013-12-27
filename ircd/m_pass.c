/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_pass.c
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
 * @brief Handlers for PASS command.
 * @version $Id: m_pass.c,v 1.7 2007-09-20 21:00:32 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "s_auth.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Handle a PASS message from an unregistered connection.
 *
 * \a parv[1..\a parc-1] is a possibly broken-up password.  Since some
 * clients do not prefix the password with ':', this functions stitches
 * the elements back together before using it.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int mr_pass(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  char password[BUFSIZE];
  int arg, len;

  assert(0 != cptr);
  assert(cptr == sptr);
  assert(!IsRegistered(sptr));

  /* Some clients (brokenly) send "PASS x y" rather than "PASS :x y"
   * when the user enters "x y" as the password.  Unsplit arguments to
   * work around this.
   */
  for (arg = 1, len = 0; arg < parc; ++arg)
  {
    ircd_strncpy(password + len, parv[arg], sizeof(password) - len);
    len += strlen(parv[arg]);
    password[len++] = ' ';
  }
  if (len > 0)
    --len;
  password[len] = '\0';

  if (EmptyString(password))
    return need_more_params(cptr, "PASS");

#if defined(DDB)
  if (IsUserPort(cptr))
  {
    /*
     *   PASS :server_pass[:ddb_pass]
     *   PASS :ddb_pass
     */
    char *ddb_pwd;

    ddb_pwd = strchr(password, ':');
    if (ddb_pwd)
      *ddb_pwd++ = '\0';
    else
      ddb_pwd = password;

    ircd_strncpy(cli_ddb_passwd(cptr), ddb_pwd, DDBPWDLEN);
  }
#endif
  ircd_strncpy(cli_passwd(cptr), password, PASSWDLEN);
  return cli_auth(cptr) ? auth_set_password(cli_auth(cptr), password) : 0;
}
