/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_db.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2004-2005 Toni Garcia (zoltan) <zoltan@irc-dev.net>
 * Copyright (C) 1999-2003 Jesus Cea Avion <jcea@argo.es>
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
 * @brief Handlers for DB command.
 * @version $Id: m_db.c,v 1.8 2007-04-21 21:17:23 zolty Exp $
 */
#include "config.h"

#include "ddb.h"
#include "client.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_snprintf.h"
#include "list.h"
#include "match.h"
#include "msg.h"
#include "numnicks.h"
#include "send.h"
#include "s_debug.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Handle a DB command from a server.
 *
 * \a parv has the following elements:
 * \a parv[1] is the destin server o servermask
 * \li If a command parv has the following elements
 * \li \a parv[2] is not used
 * \li \a parv[3] is the command (B, D, E, H, J, Q, R)
 * \li \a parv[4] is the id or token
 * \li \a parv[5] is the table of DDB
 * \li If not a command, it is a new register
 * \li \a parv[2] is the id
 * \li \a parv[3] is the table of DDB
 * \li \a parv[4] is a key of register
 * \li \a parv[5] is a content of register (optional)
 *
 * See @ref m_functions for general discussion of parameters.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int ms_db(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Client *acptr;
  struct DLink *lp;
  char ddb_buf[1024];
  unsigned char table = 0;
  unsigned int id, ddb_mask = 0;

  assert(0 != cptr);
  assert(0 != sptr);
  assert(IsServer(cptr));

  if ((parc < 5) || (*(parv[3] + 1) != '\0'))
  {
    protocol_violation(sptr, "Too few parameters for DB");
    return 0;
  }

  id = atoi(parv[2]);
  if (!id)
  {
    /* It is not a register */
    id = atoi(parv[4]);
    if (parc == 6)
    {
      table = *parv[5];
      if ((table < DDB_INIT) || (table > DDB_END))
      {
        /* A global HASH of all the tables is requested */
        if ((table == '*') && ((*parv[3] == 'Q') || (*parv[3] == 'R')))
          table = '*';
#if 1
        /* ESNET Compatibility */
        else if (table == '2')
          table = DDB_NICKDB;
#endif
        else
          return protocol_violation(sptr, "Incorrect table parameter for DB");
      }
    }
  }

  /*
   * Hay CPUs (en particular, viejos INTEL) que
   * si le dices 1372309 rotaciones, por ejemplo,
   * pues te las hace ;-), con lo que ello supone de
   * consumo de recursos. Lo normal es que
   * la CPU haga la rotacion un numero de veces
   * modulo 32 o 64, pero hay que curarse en salud.
   */
  if ((table >= DDB_INIT) && (table <= DDB_END))
    ddb_mask = ((unsigned int)1) << (table - DDB_INIT);

  switch (*parv[3])
  {
    /* Burst command (Pending registers for send) */
    case 'B':
      if (IsHub(cptr))
        sendcmdto_one(&me, CMD_DB, cptr, "%s 0 J %u %c", parv[0], ddb_id_table[table], table);
      break;

    /* Drop */
    case 'D':
    {
      if (!IsHub(cptr))
        break;

      /* Send the message to the rest of servers */
      for (lp = cli_serv(&me)->down; lp; lp = lp->next)
      {
        if (lp->value.cptr != cptr)
        {
          /* TODO-ZOLTAN: Mirar si es necesario (ESNET) */
          cli_serv(lp->value.cptr)->ddb_open &= ~ddb_mask;

          sendcmdto_one(&me, CMD_DB, lp->value.cptr, "%s 0 D %s %c",
                        parv[1], parv[4], table);
        }
      }

      /* For lastNNServer bug (find_match_server) it use collapse + match */
      collapse(parv[1]);
      /* If it is not for us, it ignored */
      if (match(parv[1], cli_name(&me)))
        break;

      /* Drop the table on memory and disk */
      ddb_drop(table);

      /* Disconnect with all hubs except which it sends to us */
      log_write(LS_DDB, L_INFO, 0, "DB Drop Table %c from %C", table, cptr);
      ircd_snprintf(0, ddb_buf, sizeof(ddb_buf), "DB DROP Table %c from %C",
                   table, cptr);
      ddb_splithubs(cptr, ddb_buf);

      /* Send the conformity response */
      sendcmdto_one(&me, CMD_DB, cptr, "%s 0 E %s %c", cli_name(cptr),
                    parv[4], table);

      /* TODO-ZOLTAN: hay que mandar a todos ? */
      /* cli_serv(cptr)->ddb_open &= ~ddb_mask; */
   
      sendcmdto_one(&me, CMD_DB, cptr, "%s 0 J %u %c", cli_name(cptr),
                    ddb_id_table[table], table);
      break;
    }

    /* Drop response */
    case 'E':
      /* If it is not for us, passed it to others */
      if ((acptr = find_match_server(parv[1])) && (!IsMe(acptr)))
        sendcmdto_one(sptr, CMD_DB, acptr, "%s 0 E %s %c", cli_name(acptr),
                      parv[4], table);     
      break;

    /* Automatic verification of HASH leaf <=> Hub */
    case 'H':
    {
      char *hash;
      unsigned int hash_lo, hash_hi;

      if (feature_bool(FEAT_HUB))
        return protocol_violation(cptr, "Don't HASH check to HUBs, table %c from %#C", table, cptr);
      
      hash = parv[4];
      if (strlen(hash) != 12)
        return protocol_violation(cptr, "HASH check, erroneus parameters");

      hash[6] = '\0';
      hash_lo = base64toint(hash);
      hash[6] = parv[4][6];
      hash_hi = base64toint(hash + 6);

      sendto_opmask(0, SNO_OLDSNO, "Lo: %d Hashtable_Lo: %d Hi: %d Hashtable_Hi %d",
        hash_lo, ddb_hashtable_lo[table], hash_hi, ddb_hashtable_hi[table]);
#if 0
      if (!((hash_lo == ddb_hashtable_lo[table]) && (hash_hi == ddb_hashtable_hi[table])))
      {
        /* The HASH is incorrect, to erase */
        log_write(LS_DDB, L_INFO, 0, "DB HASH Check failed, droping table %c", table);

        /* Drop the table on memory and disk */
        ddb_drop(table);

        sendcmdto_one(&me, CMD_DB, cptr, "%s 0 J %u %c", cli_name(cptr),
                      ddb_id_table[table], table);
      }
#endif
      break;
    }

    /* Join */
    case 'J':
    {
      int end_burst;

      if (id >= ddb_id_table[table])
      {
        /* Individual registers*/
        cli_serv(cptr)->ddb_open |= ddb_mask;
        break;
      }
      else if ((cli_serv(cptr)->ddb_open) & ddb_mask)
      {
        /* Open faucet and old registers.
         * It happens if the copy of cptr of the DDB
         * is corrupt.
         */
        (cli_serv(cptr)->ddb_open) &= ~ddb_mask;

        /* Drop your DDB */
        sendcmdto_one(&me, CMD_DB, cptr, "%s 0 D DDB_CORRUPT %c",
                      cli_name(cptr), table);

        break;
      }

      /* In the burst of table, bursts become of 1000 registers, is send
       * one in each JOIN request. This function gives back 1 if all the
       * data is sent and 0 if still there are pending data.
       * The automatic HASH verification is due to do to leafs, NEVER to
       * HUBS, since if a HUB has the corrupt DB, all the network is down
       * by wind with a massive erasure.
       */
      end_burst = ddb_db_read(cptr, table, (id + 1), 1000);
      if (end_burst == 1)
      {
        cli_serv(cptr)->ddb_open |= ((unsigned int)1) << (table - DDB_INIT);
        if (IsHub(cptr))
          break;

        inttobase64(ddb_buf, ddb_hashtable_hi[table], 6);
        inttobase64(ddb_buf + 6, ddb_hashtable_lo[table], 6);

        sendcmdto_one(&me, CMD_DB, cptr, "%s 0 H %s %c", cli_name(cptr), ddb_buf, table);
      }
      else if (end_burst == 0)
        sendcmdto_one(&me, CMD_DB, cptr, "%s 0 B %u %c",
                       cli_name(cptr), ddb_id_table[table], table);
      break;
    }

    /* Hash Query command for verify from a service */
    case 'Q':
    {
      if (!IsHub(cptr))
        break;
  
      /* For lastNNServer bug (find_match_server) it use collapse + match */
      collapse(parv[1]);
      /* If it is not for us, passed it to others */
      if (!match(parv[1], cli_name(&me)))
      {
        if (table == '*')
        {
          /* HASH of all the tables */
          unsigned int hashes_hi = 0;
          unsigned int hashes_lo = 0;
          unsigned int id_tables = 1;
          unsigned int count_tables = 1;
          int i;

          for (i = DDB_INIT; i <= DDB_END; i++)
          {
            hashes_hi ^= ddb_hashtable_hi[i];
            hashes_lo ^= ddb_hashtable_lo[i];

            id_tables = (id_tables * (ddb_id_table[i] + 1)) & 0xfffffffful;
            count_tables = (count_tables * (ddb_count_table[i] + 1)) & 0xfffffffful;
          }
          inttobase64(ddb_buf, hashes_hi, 6);
          inttobase64(ddb_buf + 6, hashes_lo, 6);
       
          sendcmdto_one(&me, CMD_DB, cptr, "%s 0 R %u-%u-%s-%s %c",
                        cli_name(cptr), id_tables, count_tables,
                        ddb_buf, parv[4], table);
        }
        else
        {
          /* One table */
          inttobase64(ddb_buf, ddb_hashtable_hi[table], 6);
          inttobase64(ddb_buf + 6, ddb_hashtable_lo[table], 6);

          sendcmdto_one(&me, CMD_DB, cptr, "%s 0 R %u-%u-%s-%s %c",
                        cli_name(cptr), ddb_id_table[table], ddb_count_table[table],
                        ddb_buf, parv[4], table);
        }
      }

      /* Send the petition to the others */
      for (lp = cli_serv(&me)->down; lp; lp = lp->next)
      {
        if (lp->value.cptr != cptr)
          sendcmdto_one(&me, CMD_DB, lp->value.cptr, "%s 0 Q %s %c",
                        parv[1], parv[4], table);
      }      
      break;
    }

    /* Hash response */
    case 'R':
      /* If it is not for us, passed it to others */
      if ((acptr = find_match_server(parv[1])) && (!IsMe(acptr)))
        sendcmdto_one(sptr, CMD_DB, acptr, "%s 0 R %s %c", cli_name(acptr),
                      parv[4], table);
      break;

    /* New register, parv[3] is the table */
    default:
    {
      int delete;

      /* Only accepted registers coming from a HUB */
       if (!IsHub(cptr))
        return 0;

      table = *parv[3];
      if (table < DDB_INIT || table > DDB_END)
        return 0;

      id = atoi(parv[2]);
      /* Rejected registers with ID lower to our */
      if (id <= ddb_id_table[table])
        return 0;

      if (parc == 5)
        /* Delete register */
        delete = 1;
      else if (parc > 5)
        /* Add register */
        delete = 0;
      else
        return protocol_violation(sptr, "DB Incorrect parameters");

      ddb_mask = ((unsigned int) 1) << (table - DDB_INIT);

      /* Propagated to our child servers */
      for (lp = cli_serv(&me)->down; lp; lp = lp->next)
      {
        if ((lp->value.cptr != cptr) && 
            (cli_serv(lp->value.cptr)->ddb_open & ddb_mask))
        {
          if (!delete)
            sendcmdto_one(&me, CMD_DB, lp->value.cptr, "%s %u %c %s :%s",
                          parv[1], id, table, parv[4], parv[5]);
          else 
            sendcmdto_one(&me, CMD_DB, lp->value.cptr, "%s %u %c %s",
                          parv[1], id, table, parv[4]);
        }
      }

      if (strcmp(parv[4], "*"))
        ddb_new_register(cptr, table, id, parv[1], parv[4], (!delete ? parv[5] : NULL));
      else
        ddb_compact(table, id, parv[5]);

      break;
    }
  }

  return 0;
}
