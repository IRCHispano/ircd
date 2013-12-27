/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_dbq.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2004 Toni Garcia (zoltan) <zoltan@irc-dev.net>
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
 * @brief Handlers for DBQ command.
 * @version $Id: m_dbq.c,v 1.7 2007-04-21 21:17:23 zolty Exp $
 */
#include "config.h"

#include "ddb.h"
#include "client.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "send.h"

#include "s_debug.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Handle a DBQ (DataBase Query) command from a server.
 * See @ref m_functions for general discussion of parameters.
 *
 * \a parv[1] is a server to query (optional)
 * \a parv[parc - 2] is a table
 * \a parv[parc - 1] is a key

 *
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_dbq(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  char table, *key, *server;
  struct Client *acptr;
  struct Ddb *ddb;

  assert(0 != cptr);
  assert(0 != sptr);
  assert(IsServer(cptr));

  if (!IsUser(sptr))
    return 0;

  if ((parc != 3 && parc != 4) ||
      (parc == 3 && (parv[1][0] == '\0' || parv[1][1] != '\0')) ||
      (parc == 4 && (parv[2][0] == '\0' || parv[2][1] != '\0')))
  {
    sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :Incorrect parameters. Format: DBQ [<server>] <Table> <Key>",
                  sptr);
    return need_more_params(sptr, "DBQ");
  }

  if (parc == 3)
  {
    server = NULL;
    table = *parv[parc - 2];
    key = parv[parc - 1];
  }
  else
  {
    server = parv[1];
    table = *parv[parc - 2];
    key = parv[parc - 1];
    if (*server == '*')
    {
      /* WOOW, BROADCAST */
      sendcmdto_serv(sptr, CMD_DBQ, cptr, "* %c %s", table, key);
    }
    else
    {
      /* NOT BROADCAST */
      if (!(acptr = find_match_server(server)))
      {
        send_reply(sptr, ERR_NOSUCHSERVER, server);
        return 0;
      }

      if (!IsMe(acptr))
      {
        sendcmdto_one(acptr, CMD_DBQ, sptr, "%s %c %s", server, table, key);
        return 0;
      }
    }
  }

  sendwallto_group(&me, WALL_WALLOPS, 0,
                   "Remote DBQ %c %s From %#C", table, key, sptr);
  log_write(LS_DDB, L_INFO, 0, "Remote DBQ %c %s From %#C", table, key, sptr);

  if (!ddb_table_is_resident(table))
  {
    sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :DBQ ERROR Table='%c' Key='%s' TABLE_NOT_RESIDENT",
                sptr, table, key);
    return 0;
  }

  ddb = ddb_find_key(table, key);
  if (!ddb)
  {
    sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :DBQ ERROR Table='%c' Key='%s' REG_NOT_FOUND",
                  sptr, table, key);
    return 0;
  }


  sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :DBQ OK Table='%c' Key='%s' Content='%s'",
                sptr, table, ddb_key(ddb), ddb_content(ddb));

  return 0;
}

/** Handle a DBQ (DataBase Query) command from a operator.
 * See @ref m_functions for general discussion of parameters.
 *
 * \a parv[1] is a server to query (optional)
 * \a parv[parc - 2] is a table
 * \a parv[parc - 1] is a key
 *
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int mo_dbq(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  char table, *key, *server;
  struct Client *acptr;
  struct Ddb *ddb;

  if (!HasPriv(cptr, PRIV_DBQ))
    return send_reply(cptr, ERR_NOPRIVILEGES);

  if ((parc != 3 && parc != 4) ||
      (parc == 3 && (parv[1][0] == '\0' || parv[1][1] != '\0')) ||
      (parc == 4 && (parv[2][0] == '\0' || parv[2][1] != '\0')))
  {
    sendcmdto_one(&me, CMD_NOTICE, cptr, "%C :Incorrect parameters. Format: DBQ [<server>] <Table> <Key>",
                  cptr);
    return need_more_params(cptr, "DBQ");;
  }

  if (parc == 3)
  {
    server = NULL;
    table = *parv[parc - 2];
    key = parv[parc - 1];
  }
  else
  {
    server = parv[1];
    table = *parv[parc - 2];
    key = parv[parc - 1];
    if (*server == '*')
    {
      /* WOOW, BROADCAST */
      sendcmdto_serv(cptr, CMD_DBQ, cptr, "* %c %s", table, key);
    }
    else
    {
      /* NOT BROADCAST */
      if (!(acptr = find_match_server(server)))
      {
        send_reply(cptr, ERR_NOSUCHSERVER, server);
        return 0;
      }

      if (!IsMe(acptr))
      {
        sendcmdto_one(acptr, CMD_DBQ, cptr, "%s %c %s", server, table, key);
        return 0;
      }
    }
  }

  sendwallto_group(&me, WALL_WALLOPS, 0,
                   "DBQ %c %s From %#C", table, key, cptr);
  log_write(LS_DDB, L_INFO, 0, "DBQ %c %s From %#C", table, key, cptr);

  if (!ddb_table_is_resident(table))
  {
    sendcmdto_one(&me, CMD_NOTICE, cptr, "%C :DBQ ERROR Table='%c' Key='%s' TABLE_NOT_RESIDENT",
                cptr, table, key);
    return 0;
  }

  ddb = ddb_find_key(table, key);
  if (!ddb)
  {
    sendcmdto_one(&me, CMD_NOTICE, cptr, "%C :DBQ ERROR Table='%c' Key='%s' REG_NOT_FOUND",
                  cptr, table, key);
    return 0;
  }


  sendcmdto_one(&me, CMD_NOTICE, cptr, "%C :DBQ OK Table='%c' Key='%s' Content='%s'",
                cptr, table, ddb_key(ddb), ddb_content(ddb));

  return 0;
}
