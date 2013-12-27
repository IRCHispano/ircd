/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ddb_db_template.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2004-2014 Toni Garcia (zoltan) <zoltan@irc-dev.net>
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
 * @brief Template DataBase mplementation of Distributed DataBases.
 * @version $Id: ddb_db_template.c,v 1.3 2007-04-19 22:53:47 zolty Exp $
 */
#include "config.h"

#include "ddb.h"
#include "client.h"
#include "ircd.h"
#include "ircd_log.h"
#include "ircd_snprintf.h"
#include "ircd_string.h"
#include "msg.h"
#include "numnicks.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Initialize database gestion module of
 * %DDB Distributed DataBases.
 */
void
ddb_db_init(void)
{
}

/** Read the table.
 * @param[in] cptr %Server if is exists, it sends to server.
 * @param[in] table Table of the %DDB Distributed DataBase.
 * @param[in] id ID number in the table.
 * @param[in] count Number of registers to be read.
 * @return 1 No data pending, 0 have data pending, -1 error.
 */
int
ddb_db_read(struct Client *cptr, unsigned char table, unsigned int id, int count)
{
  int int_return;

  int_return = 1; /* 1 = success */

  /* Open file or database */


  /* Read registers */
  do
  {
    if (!cptr)
      /* IRCD starting */
      ddb_new_register(NULL, table, mask, cid, key, content);
    else
    {
      /* Burst */
      if (content)
        sendcmdto_one(&me, CMD_DB, cptr, "%s %u %c %s :%s",
                      mask, cid, table, key, content);
      else
        sendcmdto_one(&me, CMD_DB, cptr, "%s %u %c %s",
                      mask, cid, table, key);

      if (!(--cont))
      {
        int_return = 0;
        break;
      }
    }

  } while(0) /* While read function */

  /* Close File or Database */

  return int_return;
}

/** Write the table.
 * @param[in] table Table of the %DDB Distributed DataBase.
 * @param[in] id ID number in the table.
 * @param[in] mask Mask of the server.
 * @param[in] key Key of the registry.
 * @param[in] content Content of the registry.
 */
void
ddb_db_write(unsigned char table, char *mask, unsigned int id, char *key, char *content)
{
  char buf[1024];

  ircd_snprintf(0, buf, sizeof(buf), "%d %s %s%s%s\n", id, mask, key,
                content ? " " : "", content ? content : "");
}

/** Delete a table.
 * @param[in] table Table of the %DDB Distributed DataBase.
 */
void
ddb_db_drop(unsigned char table)
{
}

/** Pack the table.
 * @param[in] table Table of the %DDB Distributed DataBase.
 */
void
ddb_db_compact(unsigned char table, char *mask, unsigned int id, char *comment)
{
}

/** Read the hashes.
 * @param[in] table Table of the %DDB Distributed DataBase.
 * @param[out] hi Hi hash.
 * @param[out] lo Lo hash.
 */
void
ddb_db_hash_read(unsigned char table, unsigned int *hi, unsigned int *lo)
{
  char buf[30];

  /* Read buf
   * buf = lala;
   */

  buf[12] = '\0';
  c = buf[6];
  buf[6] = '\0';
  *hi = base64toint(buf);
  buf[6] = c;
  *lo = base64toint(buf + 6);
}

/** Write the hash.
 * @param[in] table Table of the %DDB Distributed DataBase.
 */
void
ddb_db_hash_write(unsigned char table)
{
  char hash[20];

  inttobase64(hash, ddb_hashtable_hi[table], 6);
  inttobase64(hash + 6, ddb_hashtable_lo[table], 6);
  ircd_snprintf(0, path, sizeof(path), "%c %s\n", table, hash);
}

/** Executes whe finalizes the %DDB subsystem.
 */
void
ddb_db_end(void)
{
}

