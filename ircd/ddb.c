/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ddb.c
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2004-2007 Toni Garcia (zoltan) <zoltan@irc-dev.net>
 * Copyright (C) 1998-2003 Jesus Cea Avion <jcea@argo.es> Esnet IRC Network
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
 * @brief Implementation of Distributed DataBase.
 * @version $Id$
 */
#include "config.h"

#include "ddb.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_chattr.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_snprintf.h"
#include "ircd_string.h"
#include "ircd_tea.h"
#include "list.h"
#include "match.h"
#include "msg.h"
#include "numeric.h"
#include "s_bsd.h"
#include "s_debug.h"
#include "s_misc.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/** @page ddb Distributed DataBase
 *
 *
 * TODO, explicacion del sistema
 */

/** Count of allocated Ddb structures. */
static int ddbCount = 0;
/** DDB registers cache. */
static struct Ddb ddb_buf_cache[DDB_BUF_CACHE];
/** Buffer cache. */
static int ddb_buf_cache_i = 0;

/** Tables of %DDB. */
struct Ddb **ddb_data_table[DDB_TABLE_MAX];
/** Residents tables of %DDB. */
unsigned int ddb_resident_table[DDB_TABLE_MAX];
/** Registers count of %DDB tables. */
unsigned int ddb_count_table[DDB_TABLE_MAX];
/** ID number of %DDB tables. */
unsigned int ddb_id_table[DDB_TABLE_MAX];
/** Hi hash table. */
unsigned int ddb_hashtable_hi[DDB_TABLE_MAX];
/** Lo hash table. */
unsigned int ddb_hashtable_lo[DDB_TABLE_MAX];
/** File or DB stats of %DDB tables.*/
struct ddb_stat ddb_stats_table[DDB_TABLE_MAX];

/** Last key on iterator. */
static struct Ddb *ddb_iterator_key = NULL;
/** Last content on iterator. */
static struct Ddb **ddb_iterator_content = NULL;
/** Position of hash on iterator. */
static int ddb_iterator_hash_pos = 0;
/** Length of hash on iterator. */
static int ddb_iterator_hash_len = 0;

static void ddb_table_init(unsigned char table);


#if 1 /* DECLARACIONES VIEJAS */
/*
** ATENCION: Lo que sigue debe incrementarse cuando se toque alguna estructura de la BDD
*/
#define MMAP_CACHE_VERSION 4
#include "client.h"
#include "hash.h"
#include "numnicks.h"
#include "channel.h"
#include "ircd_features.h"
#include <stdlib.h>
#include "persistent_malloc.h"
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <syslog.h>

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
#include "dbuf.h"
#endif

struct ddb_memory_table {
  unsigned int len;
  char *position;
  char *point_r;
//  char *puntero_w;
};


/*
** Si modificamos esta estructura habra que
** actualizar tambien 'db_die_persistent'.
*/
struct portable_stat {
  dev_t dev;                    /* ID of device containing a directory entry for this file */
  ino_t ino;                    /* Inode number */
  off_t size;                   /* File size in bytes */
  time_t mtime;                 /* Time of last data modification */
};

/*
** Si se meten mas datos aqui, hay que acordarse de
** gestionarlos en el sistema de persistencia.
*/
#if defined(BDD_MMAP)
static struct portable_stat tabla_stats[DDB_TABLE_MAX];
#endif

#if defined(BDD_MMAP)
static void *mmap_cache_pos = NULL;
#endif

static int persistent_hit;

int db_persistent_hit(void)
{
  return persistent_hit;
}

#if defined(BDD_MMAP)
//#define DdbMalloc(a)	persistent_malloc(a)
//#define DdbFree(a)	persistent_free(a)
#else
//#define DdbMalloc(a)	MyMalloc(a)
//#define DdbFree(a)	MyFree(a)
#endif

#endif /* DECLARACIONES VIEJAS */

/** Verify if a table is resident.
 * @param[in] table Table of the %DDB Distributed DataBase.
 * @return Non-zero if a table is resident; zero is not resident.
 */
int ddb_table_is_resident(unsigned char table)
{
  return ddb_resident_table[table] ? 1 : 0;
}

unsigned int ddb_id_in_table(unsigned char table)
{
  return ddb_id_table[table];
}

unsigned int ddb_count_in_table(unsigned char table)
{
  return ddb_count_table[table];
}

/** Copy a malloc in the memory.
 * @param[in] buf Buffer
 * @param[in] len Length
 * @param[in] p Pointer
 */
static void
DdbCopyMalloc(char *buf, int len, char **p)
{
  char *p2;

  p2 = *p;
  if ((p2) && (strlen(p2) < len))
  {
    MyFree(p2);
    p2 = NULL;
  }
  if (!p2)
  {
    p2 = MyMalloc(len + 1);    /* The '\0' */
    *p = p2;
  }
  memcpy(p2, buf, len);
  p2[len] = '\0';
}

/** Calculates the hash.
 * @param[in] line buffer line reading the tables.
 * @param[in] table Table of the %DDB Distributed DataBase.
 */
static void
ddb_hash_calculate(char *line, unsigned char table)
{
  unsigned int buffer[129 * sizeof(unsigned int)];
  unsigned int *p = buffer;
  unsigned int x[2], v[2], k[2];
  char *p2;

  memset(buffer, 0, sizeof(buffer));
  strncpy((char *)buffer, line, sizeof(buffer) - 1);
  while ((p2 = strchr((char *)buffer, '\n')))
    *p2 = '\0';
  while ((p2 = strchr((char *)buffer, '\r')))
    *p2 = '\0';

  k[0] = k[1] = 0;
  x[0] = ddb_hashtable_hi[table];
  x[1] = ddb_hashtable_lo[table];

  while (*p)
  {
    v[0] = ntohl(*p);
    p++;                        /* No se puede hacer a la vez porque la linea anterior puede ser una expansion de macros */
    v[1] = ntohl(*p);
    p++;                        /* No se puede hacer a la vez porque la linea anterior puede ser una expansion de macros */
    ircd_tea(v, k, x);
  }
  ddb_hashtable_hi[table] = x[0];
  ddb_hashtable_lo[table] = x[1];
}

/** Initialize %DDB Distributed DataBases.
 */
void
ddb_init(void)
{
  unsigned char table;

  ddb_db_init();
  ddb_events_init();

  memset(ddb_resident_table, 0, sizeof(ddb_resident_table));

  /*
   * The lengths MUST be powers of 2 and do not have
   * to be superior to HASHSIZE.
   */
  ddb_resident_table[DDB_BOTDB]          =   256;
  ddb_resident_table[DDB_CHANDB]         =  4096;
  ddb_resident_table[DDB_CHANDB2]        = 32768;
  ddb_resident_table[DDB_EXCEPTIONDB]    =   512;
  ddb_resident_table[DDB_FEATUREDB]      =   256;
  ddb_resident_table[DDB_ILINEDB]        =   256;
  ddb_resident_table[DDB_JUPEDB]         =   512;
  ddb_resident_table[DDB_NICKDB]         = 32768;
  ddb_resident_table[DDB_OPERDB]         =   256;
  ddb_resident_table[DDB_PRIVDB]         =   256;
  ddb_resident_table[DDB_CHANREDIRECTDB] =   256;
  ddb_resident_table[DDB_UWORLDDB]       =   256;
  ddb_resident_table[DDB_VHOSTDB]        =  4096;
  ddb_resident_table[DDB_COLOURVHOSTDB]  =  1024;
  ddb_resident_table[DDB_CONFIGDB]       =   256;

  if (!ddb_cache()) {
    for (table = DDB_INIT; table <= DDB_END; table++)
      ddb_table_init(table);
  }

  /*
   * The previous operation it can be a long operation.
   * Updates time.
   */
  CurrentTime = time(NULL);
}

/** Initialize a table of %DDB.
 * @param[in] table
 */
static void ddb_table_init(unsigned char table)
{
  unsigned int hi, lo;

  /* First drop table */
  ddb_drop_memory(table, 0);

  /* Read the table on file or database */
  ddb_db_read(NULL, table, 0, 0);

  /* Read hashes */
  ddb_db_hash_read(table, &hi, &lo);

  /* Compare memory hashes with local hashes */
  sendto_opmask(0, SNO_OLDSNO, "Lo: %d Hashtable_Lo: %d Hi: %d Hashtable_Hi %d",
        lo, ddb_hashtable_lo[table], hi, ddb_hashtable_hi[table]);

  if ((ddb_hashtable_hi[table] != hi) || (ddb_hashtable_lo[table] != lo))
  {
    struct DLink *lp;
    char buf[1024];

    log_write(LS_DDB, L_INFO, 0, "WARNING - Table '%c' is corrupt. Droping table...", table);
    ddb_db_drop(table);

    ircd_snprintf(0, buf, sizeof(buf), "Table '%c' is corrupt. Reloading via remote burst...", table);
    ddb_splithubs(NULL, table, buf);
    log_write(LS_DDB, L_INFO, 0, "Solicit a copy of table '%s' to neighboring nodes", table);

    /*
     * Solucion temporal
     * Corta conexiones con los HUBs
     */
  drop_hubs:
    for (lp = cli_serv(&me)->down; lp; lp = lp->next)
    {
      if (IsHub(lp->value.cptr)) {
        exit_client(lp->value.cptr, lp->value.cptr, &me, buf);
        goto drop_hubs;
      }
    }
    /*
     * Fin solucion temporal
     */
    ddb_splithubs(NULL, table, buf);
    sendto_opmask(0, SNO_OLDSNO, "Solicit DDB update table '%c'", table);

    /*
     * Solo pide a los HUBs, porque no se
     * aceptan datos de leafs.
     */
    for (lp = cli_serv(&me)->down; lp; lp = lp->next)
    {
      if (IsHub(lp->value.cptr))
      {
        sendcmdto_one(&me, CMD_DB, lp->value.cptr, "%C 0 J %u %c",
                      lp->value.cptr, ddb_id_table[table], table);
      }
    }
  }

  ddb_db_hash_write(table);

  /*
   * Si hemos leido algun registro de compactado,
   * sencillamente nos lo cargamos en memoria, para
   * que cuando llegue otro, el antiguo se borre
   * aunque tenga algun contenido (un texto explicativo, por ejemplo).
   */
  if (ddb_resident_table[table])
  {
    ddb_del_key(table, "*");
    log_write(LS_DDB, L_INFO, 0, "Loading Table '%c' finished: S=%u R=%u",
              table, ddb_id_table[table], ddb_count_table[table]);
  }
  else if (ddb_count_table[table])
    log_write(LS_DDB, L_INFO, 0, "Loading Table '%c' finished: S=%u NoResident",
              table, ddb_id_table[table]);
}

/** Add a new register from the network or reading when ircd is starting.
 * @param[in] cptr %Server sending a new register. If is NULL, it is own server during ircd start.
 * @param[in] table Table of the %DDB Distributed DataBases.
 */
void
ddb_new_register(char *pregister, unsigned char table, struct Client *cptr, struct Client *sptr)
{
  char *p0, *p1, *p2, *p3, *p4;

  assert(0 != table);

  ddb_hash_calculate(pregister, table);

  /* In the ircd starting, cptr is NULL and it not writing on file or database */
  if (cptr)
  {
    ddb_db_write(table, pregister);
    ddb_db_hash_write(table);
  }

  p0 = strtok(pregister, " ");  /* ID */
  p1 = strtok(NULL, " ");       /* mask */
  p2 = strtok(NULL, " ");       /* table */
  p3 = strtok(NULL, " \r\n");   /* key */
  p4 = strtok(NULL, "\r\n");    /* content (optional) */

  if (p3 == NULL)
    return;                     /* Incomplet register */

  ddb_id_table[table] = atol(p0);

  /* If the table is not resident, do not save in memory */
  if (!ddb_resident_table[table])
    return;

  /* If a mask is not concerned with me, do not save in memory */
  /* For lastNNServer bug (find_match_server) it use collapse + match */
  collapse(mask);
  if (!match(mask, cli_name(&me)))
  {
    int update = 0, i = 0;

    if ((strlen(key) + 1 > key_len) || (!keytemp))
    {
      key_len = strlen(key) + 1;
      if (keytemp)
        MyFree(keytemp);
      keytemp = MyMalloc(key_len);

      assert(0 != keytemp);
    }
    strcpy(keytemp, key);

    while (keytemp[i])
    {
      keytemp[i] = ToLower(keytemp[i]);
      i++;
    }

    if (content)
      update = ddb_add_key(table, keytemp, content);
    else
      ddb_del_key(table, keytemp);

/*    if (cptr && ddb_events_table[table]) */
    if (ddb_events_table[table])
      ddb_events_table[table](key, content, update);
  }
}

/** Add or update an register on the memory.
 * @param[in] table Table of the %DDB Distributed DataBases.
 * @param[in] key Key of the register.
 * @param[in] content Content of the key.
 * @return 1 is an update, 0 is a new register.
 */
static int
ddb_add_key(unsigned char table, char *key, char *content)
{
  struct Ddb *ddb;
  char *k, *c;
  int hashi;
  int delete = 0;

  ddb_iterator_key = NULL;

  delete = ddb_del_key(table, key);

  ddb = DdbMalloc(sizeof(struct Ddb) + strlen(key) + strlen(content) + 2);
  assert(0 != ddb);

  k = (char *)ddb + sizeof(struct Ddb);
  c = k + strlen(key) + 1;

  strcpy(k, key);
  strcpy(c, content);

  ddb_key(ddb) = k;
  ddb_content(ddb) = c;
  ddb_next(ddb) = NULL;

  hashi = ddb_hash_register(ddb_key(ddb), ddb_resident_table[table]);

  Debug((DEBUG_INFO, "Add DDB: T='%c' K='%s' C='%s' H=%u", table, ddb_key(ddb), ddb_content(ddb), hashi));

  ddb_next(ddb) = ddb_data_table[table][hashi];
  ddb_data_table[table][hashi] = ddb;
  ddb_count_table[table]++;
  ddbCount++;

  return delete;
}

/** Delete an register from memory.
 * @param[in] table Table of the %DDB Distributed DataBases.
 * @param[in] key Key of the register.
 * @return 1 on success; 0 do not delete.
 */
static int
ddb_del_key(unsigned char table, char *key)
{
  struct Ddb *ddb, *ddb2, **ddb3;
  int hashi;
  int delete = 0;

  ddb_iterator_key = NULL;

  hashi = ddb_hash_register(key, ddb_resident_table[table]);
  ddb3 = &ddb_data_table[table][hashi];

  for (ddb = *ddb3; ddb; ddb = ddb2)
  {
    ddb2 = ddb_next(ddb);
    if (!strcmp(ddb_key(ddb), key))
    {
      *ddb3 = ddb2;
      delete = 1;
      DdbFree(ddb);
      ddb_count_table[table]--;
      ddbCount--;
      break;
    }
    ddb3 = &(ddb_next(ddb));
  }
  return delete;
}

/** Deletes a table.
 * @param[in] table Table of the %DDB Distributed DataBases.
 */
void
ddb_drop(unsigned char table)
{

  /* Delete file or database of the table */
  ddb_db_drop(table);

  /* Delete table from memory */
  ddb_drop_memory(table, 0);

  /* Write hash on file or database */
  ddb_db_hash_write(table);
}

/** Deletes a table from memory.
 * @param[in] table Table of the %DDB Distributed DataBases.
 * @param[in] events Non-zero impliques events.
 */
void
ddb_drop_memory(unsigned char table, int events)
{
  struct Ddb *ddb, *ddb2;
  int i, n;

  ddb_id_table[table] = 0;
  ddb_count_table[table] = 0;
  ddb_hashtable_hi[table] = 0;
  ddb_hashtable_lo[table] = 0;

  n = ddb_resident_table[table];
  if (!n)
    return;

  if (ddb_data_table[table])
  {
    for (i = 0; i < n; i++)
    {
      for (ddb = ddb_data_table[table][i]; ddb; ddb = ddb2)
      {
        ddb2 = ddb_next(ddb);

        if (events && ddb_events_table[table])
          ddb_events_table[table](ddb_key(ddb), NULL, 0);

        DdbFree(ddb);
      }
    }
  }
  else
  {                             /* NO tenemos memoria para esa tabla, asi que la pedimos */
    ddb_data_table[table] = DdbMalloc(n * sizeof(struct Ddb *));
    assert(ddb_data_table[table]);
  }

  for (i = 0; i < n; i++)
    ddb_data_table[table][i] = NULL;

}

/** Packing the table.
 * @param[in] table Table of the %DDB Distributed DataBases.
 * @param[in] id Identify number of the register.
 * @param[in] content Content of the key.
 */
void
ddb_compact(unsigned char table, unsigned int id, char *content)
{
  log_write(LS_DDB, L_INFO, 0, "Packing table '%c'", table);
  ddb_id_table[table] = id;
  ddb_db_compact(table);

  ddb_hash_calculate(table, id, "*", "*", content);
  ddb_db_hash_write(table);
}

/** Sending the %DDB burst tables.
 * @param[in] cptr %Server sending the petition.
 */
void
ddb_burst(struct Client *cptr)
{
  int i;

  sendto_opmask(0, SNO_NETWORK, "Bursting DDB tables");

#if defined(USE_ZLIB)
  zlib_microburst_init();
#endif

  /* La tabla 'n' es un poco especial... */
  sendcmdto_one(&me, CMD_DB, cptr, "* 0 J %u 2",
                ddb_id_table[DDB_NICKDB]);

  for (i = DDB_INIT; i <= DDB_END; i++)
  {
    if (i != DDB_NICKDB)
      sendcmdto_one(&me, CMD_DB, cptr, "* 0 J %u %c",
                    ddb_id_table[i], i);
  }

#if defined(USE_ZLIB)
  zlib_microburst_complete();
#endif
}

/** Initializes %DDB iterator.
 * 
 * @return ddb_iterator_key pointer.
 */
static struct Ddb *
ddb_iterator_init(void)
{
  struct Ddb *ddb;

  if (ddb_iterator_key)
  {
    ddb_iterator_key = ddb_next(ddb_iterator_key);
    if (ddb_iterator_key)
    {
      return ddb_iterator_key;
    }
  }

  /*
   * "ddb_iterator_hash_pos" siempre indica el PROXIMO valor a utilizar.
   */
  while (ddb_iterator_hash_pos < ddb_iterator_hash_len)
  {
    ddb = ddb_iterator_content[ddb_iterator_hash_pos++];
    if (ddb) {
      ddb_iterator_key = ddb;
      return ddb;
    }
  }

  ddb_iterator_key = NULL;
  ddb_iterator_content = NULL;
  ddb_iterator_hash_pos = 0;
  ddb_iterator_hash_len = 0;

  return NULL;
}

/** Initializes iterator for a table.
 * @param[in] table Table of the %DDB Distributed DataBase.
 * @return First active register on a table.
 */
struct Ddb *
ddb_iterator_first(unsigned char table)
{
  assert((table >= DDB_INIT) && (table <= DDB_END));

  ddb_iterator_hash_len = ddb_resident_table[table];
  assert(ddb_iterator_hash_len);

  ddb_iterator_hash_pos = 0;
  ddb_iterator_content = ddb_data_table[table];
  ddb_iterator_key = NULL;

  return ddb_iterator_init();
}

/** Next iterator.
 * @return Next active register on a table.
 */
struct Ddb *
ddb_iterator_next(void)
{
  assert(ddb_iterator_key);
  assert(ddb_iterator_content);
  assert(ddb_iterator_hash_len);

  return ddb_iterator_init();
}

/** Find a register by the key (internal).
 * @param[in] table Table of the %DDB Distributed DataBases.
 * @param[in] key Key of the register.
 * @return Pointer of the register.
 */
static struct Ddb *ddb_find_registry_table(unsigned char table, char *key)
{
  struct Ddb *ddb;
  static char *k = 0;
  static int k_len = 0;
  int i = 0, hashi;

  if ((strlen(key) + 1 > k_len) || (!k))
  {
    k_len = strlen(key) + 1;
    if (k)
      MyFree(k);
    k = MyMalloc(k_len);
    if (!k)
      return 0;
  }
  strcpy(k, key);

  /* Paso a minusculas */
  while (k[i])
  {
    k[i] = ToLower(k[i]);
    i++;
  }

  hashi = ddb_hash_register(k, ddb_resident_table[table]);

  for (ddb = ddb_data_table[table][hashi]; ddb; ddb = ddb_next(ddb))
  {
    if (!ircd_strcmp(ddb_key(ddb), k))
    {
      assert(0 != ddb_content(ddb));
      return ddb;
    }
  }
  return NULL;
}

/** Find a register by the key.
 * @param[in] table Table of the %DDB Distributed DataBases.
 * @param[in] key Key of the register.
 * @return Pointer of the register.
 */
struct Ddb *ddb_find_key(unsigned char table, char *key)
{
  struct Ddb *ddb;
  char *key_init;
  char *key_end;
  char *content_init;
  char *content_end;

  if (!ddb_resident_table[table])
    return NULL;

  ddb = ddb_find_registry_table(table, key);
  if (!ddb)
    return NULL;

  key_init = ddb_key(ddb);
  key_end = key_init + strlen(key_init);
  content_init = ddb_content(ddb);
  content_end = content_init + strlen(content_init);

  DdbCopyMalloc(key_init, key_end - key_init,
            &ddb_buf_cache[ddb_buf_cache_i].ddb_key);
  DdbCopyMalloc(content_init, content_end - content_init,
            &ddb_buf_cache[ddb_buf_cache_i].ddb_content);
  if (++ddb_buf_cache_i >= DDB_BUF_CACHE)
    ddb_buf_cache_i = 0;

  return ddb;
}

/** Get nick!user@host of the virtual bot.
 * @param[in] bot Key of the register.
 * @return nick!user@host of the virtual bot if exists and
 * if not exists, return my servername.
 */
char *
ddb_get_botname(char *bot)
{
  struct Ddb *ddb;

  ddb = ddb_find_key(DDB_CONFIGDB, bot);
  if (ddb)
    return ddb_content(ddb);
  else
    return cli_name(&me);
}

/** When IRCD is reloading, it is executing.
 */
void
ddb_reload(void)
{
  log_write(LS_DDB, L_INFO, 0, "Reload Distributed DataBase...");

  /* ddb_init(); */
}

/** Split all Hubs but one.
 * @param[in] cptr Client that is not closed.
 * @param[in] exitmsg SQUIT message.
 */
void
ddb_splithubs(struct Client *cptr, unsigned char table, char *exitmsg)
{
  struct Client *acptr;
  struct DLink *lp;
  char buf[1024];
  int num_hubs = 0;

  for (lp = cli_serv(&me)->down; lp; lp = lp->next)
  {
    if (IsHub(lp->value.cptr))
      num_hubs++;
  }

  if (num_hubs >= 2)
  {
    /*
     * No podemos simplemente hace el bucle, porque
     * el "exit_client()" modifica la estructura
     */
corta:
    if (num_hubs-- > 1)
    {                           /* Deja un HUB conectado */
      for (lp = cli_serv(&me)->down; lp; lp = lp->next)
      {
        acptr = lp->value.cptr;
        /*
         * Si se especifica que se desea mantener un HUB
         * en concreto, respeta esa peticion
         */
        if ((acptr != cptr) && IsHub(acptr))
        {
          ircd_snprintf(0, buf, sizeof(buf), "DDB '%c' %s. Resynchronizing...",
                        table, exitmsg);
          exit_client(acptr, acptr, &me, buf);
          goto corta;
        }
      }
    }
  }
}

/** Die the server with an DDB error.
  * @param[in] pattern Format string of message.
  */
void
ddb_die(const char *pattern, ...)
{
  struct Client *acptr;
  char exitmsg[1024], exitmsg2[1024];
  va_list vl;
  int i;

  va_start(vl, pattern);
  vsprintf(exitmsg2, pattern, vl);
  va_end(vl);

  ircd_snprintf(0, exitmsg, sizeof(exitmsg), "DDB Error: %s", exitmsg2);

  for (i = 0; i <= HighestFd; i++)
  {
    if (!(acptr = LocalClientArray[i]))
      continue;
    if (IsUser(acptr))
      sendcmdto_one(&me, CMD_NOTICE, acptr, "%C :Server Terminating. %s",
                    acptr, exitmsg);
    else if (IsServer(acptr))
      sendcmdto_one(&me, CMD_ERROR, acptr, ":Terminated by %s", exitmsg);
  }
  exit_schedule(0, 0, 0, exitmsg);
}

/** Finalizes the %DDB subsystem.
 */
void
ddb_end(void)
{
  ddb_db_end();
}

/** Report all F-lines to a user.
 * @param[in] to Client requesting statistics.
 * @param[in] sd Stats descriptor for request (ignored).
 * @param[in] param Extra parameter from user (ignored).
 */
void
ddb_report_stats(struct Client* to, const struct StatDesc* sd, char* param)
{
  unsigned char table;

  for (table = DDB_INIT; table <= DDB_END; table++)
  {
    if (ddb_table_is_resident(table))
      send_reply(to, SND_EXPLICIT | RPL_STATSDEBUG,
                 "b :Table '%c' S=%lu R=%lu", table,
                 (unsigned long)ddb_id_table[table],
                 (unsigned long)ddb_count_table[table]);
    else
    {
      if (ddb_id_table[table])
        send_reply(to, SND_EXPLICIT | RPL_STATSDEBUG,
                   "b :Table '%c' S=%lu NoResident", table,
                   (unsigned long)ddb_id_table[table]);
    }
  }
}

/** Find number of DDB structs allocated and memory used by them.
 * @param[out] count_out Receives number of DDB structs allocated.
 * @param[out] bytes_out Receives number of bytes used by DDB structs.
 */
void ddb_count_memory(size_t* count_out, size_t* bytes_out)
{
  assert(0 != count_out);
  assert(0 != bytes_out);
  *count_out = ddbCount;
  *bytes_out = ddbCount * sizeof(struct Ddb);
}






/*
 * db_es_miembro (tabla, clave, subcadena)
 *
 * valor registro es una lista separada por comas, y si subcadena es
 # una de ellas, retorna la posicion, sino 0
 *                                      1999/07/03 savage@apostols.org
 */
int db_es_miembro(unsigned char tabla, char *clave, char *subcadena)
{
  int j, i = 0;
  static char *buf = NULL;
  static int buf_len = 0;
  char *f, *s = NULL;
  struct Ddb *ddb;

  if ((ddb = ddb_find_key(tabla, clave)) == NULL)
    return 0;

  if ((strlen(ddb_content(ddb)) + 1 > buf_len) || (!buf))
  {
    buf_len = strlen(ddb_content(ddb)) + 1;
    if (buf)
      MyFree(buf);
    buf = MyMalloc(buf_len);
    if (!buf)
      return 0;
  }

  strcpy(buf, ddb_content(ddb));
  for (f = ircd_strtok(&s, buf, ","); f != NULL; f = ircd_strtok(&s, NULL, ","))
  {
    j++;
    if (!ircd_strcmp(f, subcadena))
    {
      i++;
      break;
    }
  }

  return i;
}

/*
** Esta rutina mata el servidor.
** Es casi una copia de m_die().
*/
extern char **myargv;           /* ircd.c */

static void db_die(char *msg, unsigned char que_bdd)
{
  struct Client *acptr;
  int i;
  char buf[1024];

  sprintf_irc(buf, "DB '%c' - %s (%s). El daemon muere...", que_bdd, msg,
      feature_str(FEAT_DDBPATH));
  for (i = 0; i <= HighestFd; i++)
  {
    if (!(acptr = LocalClientArray[i]))
      continue;
    if (IsUser(acptr))
      sendto_one(acptr, ":%s NOTICE %s :%s", cli_name(&me), PunteroACadena(cli_name(acptr)), buf);
    else if (IsServer(acptr))
      sendto_one(acptr, ":%s ERROR :%s", cli_name(&me), buf);
  }

#if !defined(USE_SYSLOG)
  openlog(myargv[0], LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif

  syslog(LOG_ERR, buf);

#if !defined(USE_SYSLOG)
  closelog();
#endif

  Debug((DEBUG_ERROR, "%s", buf));

#if defined(__cplusplus)
  s_die(0);
#else
  s_die();
#endif
}

static void db_die_persistent(struct portable_stat *st1,
    struct portable_stat *st2, char *msg, unsigned char que_bdd)
{
  char buf[4096];
  char *separador = "";

  strcpy(buf, msg);
  strcat(buf, " [");

/*
** Puede no detectarse nada si el tipo que comparamos
** es mas grande que un "unsigned long long".
*/
  if (st1->dev != st2->dev)
  {
    sprintf(buf + strlen(buf), "%sDEVICE: %llu<>%llu", separador,
        (unsigned long long)st1->dev, (unsigned long long)st2->dev);
    separador = " ";
  }
  if (st1->ino != st2->ino)
  {
    sprintf(buf + strlen(buf), "%sINODE: %llu<>%llu", separador,
        (unsigned long long)st1->ino, (unsigned long long)st2->ino);
    separador = " ";
  }
  if (st1->size != st2->size)
  {
    sprintf(buf + strlen(buf), "%sSIZE: %llu<>%llu", separador,
        (unsigned long long)st1->size, (unsigned long long)st2->size);
    separador = " ";
  }
  if (st1->mtime != st2->mtime)
  {
    sprintf(buf + strlen(buf), "%sMTIME: %llu<>%llu", separador,
        (unsigned long long)st1->mtime, (unsigned long long)st2->mtime);
    separador = " ";
  }

  strcat(buf, "]");
  db_die(buf, que_bdd);         /* No regresa */
}


/*
** ATENCION: El multiplicador al final de esta rutina
** me permite variar el número de version del MMAP cache.
*/
static unsigned int mmap_cache_version(void)
{
  unsigned int version;
  int i;

  version = sizeof(struct Ddb) * DDB_TABLE_MAX;

  for (i = 0; i < DDB_TABLE_MAX; i++)
    version *= 1 + ddb_resident_table[i] * (i + 1);

  return version * MMAP_CACHE_VERSION;  /* El multiplicador me permite variar el numero de version del MMAP cache */
}


#if defined(BDD_MMAP)
void db_persistent_commit(void)
{
  char path_buf[sizeof(feature_str(FEAT_DDBPATH)) + 1024];
  uintptr_t *p, *p_base, *p_limite;
  unsigned char *p2;
  unsigned int len_used;
  unsigned int v[2], k[2], x[2];
  struct portable_stat st;
  int handle;
  int i;

  if (!mmap_cache_pos)
    return;

  for (i = 0; i < DDB_TABLE_MAX; i++)
  {
    if ((i < DDB_INIT) || (i > DDB_END))
      continue;
    sprintf_irc(path_buf, "%s/tabla.%c", feature_str(FEAT_DDBPATH), i);
    handle = open(path_buf, O_RDONLY, S_IRUSR | S_IWUSR);
    assert(handle != -1);
    get_stat(handle, &st);
    close(handle);
    if (memcmp(&st, &tabla_stats[i], sizeof(st)))
      db_die_persistent(&st, &tabla_stats[i],
          "Se detecta una modificacion no autorizada de la BDD (MMAP_COMMIT)",
          i);
  }

  p_base = (uintptr_t *)mmap_cache_pos;
  p = p_base;
  p2 = (unsigned char *)p;

  p += 2;                       /* Nos saltamos el HASH, de momento */
  p += 1;                       /* Nos saltamos la version, de momento */
  *p++ = BDD_MMAP_SIZE * 1024 * 1024;
  len_used = (unsigned char *)persistent_top() - p2;
  assert(!(len_used & 7));      /* Multiplo de 8 */
  *p++ = len_used;
  *p++ = (unsigned int)p_base;  /* Esto no me gusta mucho para plataformas 64 bits */

  p2 = (unsigned char *)p;

  sprintf_irc(path_buf, "%s/hashes", feature_str(FEAT_DDBPATH));
  handle = open(path_buf, O_RDONLY);
  if (handle < 0)
    return;
  i = read(handle, p2, 65535);
  close(handle);
  if (i <= 0)
    return;
  p2 +=
      sizeof(unsigned int) * (((i + sizeof(unsigned int) -
      1) / sizeof(unsigned int)));

  memcpy(p2, ddb_resident_table, sizeof(ddb_resident_table));
  p2 += sizeof(ddb_resident_table);
  p2 = persistent_align(p2);
  memcpy(p2, tabla_stats, sizeof(tabla_stats));
  p2 += sizeof(tabla_stats);
  memcpy(p2, ddb_count_table, sizeof(ddb_count_table));
  p2 += sizeof(ddb_count_table);
  memcpy(p2, ddb_data_table, sizeof(ddb_data_table));
  p2 += sizeof(ddb_data_table);
  memcpy(p2, ddb_id_table, sizeof(ddb_id_table));
  p2 += sizeof(ddb_id_table);
  memcpy(p2, ddb_hashtable_hi, sizeof(ddb_hashtable_hi));
  p2 += sizeof(ddb_hashtable_hi);
  memcpy(p2, ddb_hashtable_lo, sizeof(ddb_hashtable_lo));
  p2 += sizeof(ddb_hashtable_lo);

  p = p_base + 2;               /* Nos saltamos el HASH inicial */
  p_limite = p_base + len_used / sizeof(unsigned int);
  k[0] = k[1] = 23;
  x[0] = x[1] = 0;
/*
** La primera iteracion es especial, por el numero de version modificado.
*/
  p++;                          /* Nos saltamos la version de momento */
  v[0] = mmap_cache_version();
  v[1] = *p++;
  tea(v, k, x);

  while (p < p_limite)
  {
    v[0] = *p++;
    v[1] = *p++;
    tea(v, k, x);
  }
  p = p_base;
  *p++ = x[0];
  *p++ = x[1];
  *p++ = mmap_cache_version();
}
#endif /* defined(BDD_MMAP) */


  if (ddb_resident_table[que_bdd])
  {
    if (p4 == NULL)             /* Borrado */
      db_eliminar_registro(que_bdd, p3, 0, cptr, sptr);
    else
      db_insertar_registro(que_bdd, p3, p4, cptr, sptr);
  }
}

/*
** ddb_compact
**
** Elimina los registro superfluos
** de una Base de Datos Local.
** 
** Se invoca cuando se recibe un CheckPoint, y el
** formato es "serie destino id * texto"
*/
void ddb_compact(char *registro, unsigned char que_bdd)
{
  int db_file;
  char path[1024];
  char *map;
  char *lectura, *escritura, *p;
  char *clave, *valor;
  char c;
  unsigned int len, len2;
  struct portable_stat estado;
  struct Ddb *ddb;

/*
** El primer valor es el numero de serie actual
*/
  ddb_id_table[que_bdd] = atol(registro);

  sprintf_irc(path, "%s/tabla.%c", feature_str(FEAT_DDBPATH), que_bdd);
  db_file = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  get_stat(db_file, &estado);
  len = estado.size;

#if defined(BDD_MMAP)
  if (memcmp(&estado, &tabla_stats[que_bdd], sizeof(estado)))
    db_die_persistent(&estado, &tabla_stats[que_bdd],
        "Se detecta una modificacion no autorizada de la BDD (COMPACT)",
        que_bdd);
#endif

  map = mmap(NULL, len, PROT_READ | PROT_WRITE,
      MAP_SHARED | MAP_NORESERVE, db_file, 0);
  close(db_file);
  if (db_file == -1)
    db_die("Error al intentar compactar (open)", que_bdd);
  if ((len != 0) && (map == MAP_FAILED))
    db_die("Error al intentar compactar (mmap)", que_bdd);

  if (!ddb_resident_table[que_bdd])
  {                             /* No residente -> No pack */
    escritura = map + len;
    goto fin;
  }

  ddb_hashtable_hi[que_bdd] = 0;
  ddb_hashtable_lo[que_bdd] = 0;

  p = lectura = escritura = map;
  while (lectura < map + len)
  {
    p = strchr(p, ' ') + 1;     /* Destino */
    p = strchr(p, ' ') + 1;     /* BDD */
    valor = clave = p = strchr(p, ' ') + 1; /* Clave */
    while ((p < map + len) && (*p != '\n') && (*p != '\r'))
      p++;
    while ((*valor != ' ') && (valor < p))
      valor++;
    valor++;                    /* Nos saltamos el espacio */

/*
** Los registros "*" son borrados automaticamente
** al compactar, porque no aparecen en la
** base de datos en memoria.
**
** Solo se mantiene el nuevo, porque es
** el que se graba tras la compactacion
*/
    if (valor < p)
    {                           /* Nuevo registro, no un borrado */
      *(valor - 1) = '\0';
      ddb = ddb_find_key(que_bdd, clave);
      *(valor - 1) = ' ';
      if (ddb != NULL)
      {                         /* El registro sigue existiendo */

        len2 = strlen(ddb_content(ddb));
        if ((valor + len2 == p) && (!strncmp(valor, ddb_content(ddb), len2)))
        {                       /* !!El mismo!! */

/*
** Actualizamos HASH
*/
          if ((*p == '\n') || (*p == '\r'))
          {
            c = *p;
            *p = '\0';
            ddb_hash_calculate(lectura, que_bdd);
            *p = c;
          }
          else
          {                     /* Estamos al final y no hay retorno de carro */
            char temp[1024];

            memcpy(temp, lectura, p - lectura);
            temp[p - lectura] = '\0';
            ddb_hash_calculate(temp, que_bdd);
          }

          while ((p < map + len) && ((*p == '\n') || (*p == '\r')))
            p++;
          memcpy(escritura, lectura, p - lectura);
          escritura += p - lectura;
          lectura = p;
          continue;             /* MUY IMPORTANTE */
        }                       /* Hay otro mas moderno que este */
      }                         /* El registro fue borrado */
    }                           /* Es un borrado */
    while ((p < map + len) && ((*p == '\n') || (*p == '\r')))
      p++;
    lectura = p;
  }

fin:

  munmap(map, len);

  if (truncate(path, escritura - map) == -1)
  {
    db_die("Error al intentar compactar (truncate)", que_bdd);
  }
  db_file = open(path, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
  if (db_file == -1)
  {
    db_die("Error al intentar compactar (open-2)", que_bdd);
  }
  write(db_file, registro, strlen(registro)); /* CheckPoint */

#if defined(BDD_MMAP)
  get_stat(db_file, &tabla_stats[que_bdd]);
#endif

  close(db_file);
  ddb_hash_calculate(registro, que_bdd);
  ddb_db_hash_write(que_bdd);
}


/*
 * reload_db
 *
 * Recarga la base de datos de disco, liberando la memoria
 *
 */
void reload_db(void)
{
  char buf[16];
  unsigned char c;
  sendto_ops("Releyendo Bases de Datos...");
  initdb();
  for (c = DDB_INIT; c <= DDB_END; c++)
  {
    if (ddb_id_table[c])
    {
      inttobase64(buf, ddb_hashtable_hi[c], 6);
      inttobase64(buf + 6, ddb_hashtable_lo[c], 6);
      sendto_ops("DB: '%c'. Ultimo registro: %u. HASH: %s",
          c, ddb_id_table[c], buf);
    }
  }
}
