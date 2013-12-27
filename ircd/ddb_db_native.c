/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ddb_db_native.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2004-2014 Toni Garcia (zoltan) <zoltan@irc-dev.net>
 * Copyright (C) 1999-2003 Jesus Cea Avion <jcea@argo.es> Esnet IRC Network
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
 * @brief Native DataBase implementation of Distributed DataBases.
 * @version $Id: ddb_db_native.c,v 1.7 2007-11-11 20:37:41 zolty Exp $
 */
#include "config.h"

#include "ddb.h"
#include "client.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_snprintf.h"
#include "ircd_string.h"
#include "msg.h"
#include "numnicks.h"
#include "s_debug.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

/*
 * ddb_db_native
 */
struct ddb_memory_table {
  struct ddb_stat file_stat;
  char *position;   /* Posicion */
  char *point_r;    /* Lectura */
};

static int ddb_read(struct ddb_memory_table *map_table, char *buf);
static int ddb_seek(struct ddb_memory_table *map_table, char *buf, unsigned int id);
static void get_ddb_stat(int fd, struct ddb_stat *ddbstat);
static char *check_corrupt_table(unsigned char table, struct ddb_stat *ddbstat);


/** Initialize database gestion module of
 * %DDB Distributed DataBases.
 */
void
ddb_db_init(void)
{
  char path[1024];
  struct stat sStat;
  unsigned char table;
  int fd;

  ircd_snprintf(0, path, sizeof(path), "%s/", feature_str(FEAT_DDBPATH));
  if ((stat(path, &sStat) == -1))
  {
    if (0 != mkdir(feature_str(FEAT_DDBPATH), 0775))
      ddb_die("Error when creating %s directory", feature_str(FEAT_DDBPATH));
  }
  else
  {
    if (!S_ISDIR(sStat.st_mode))
      ddb_die("Error S_ISDIR(%s)", feature_str(FEAT_DDBPATH));
  }

  /* Verify if hashes file is exist. */
  ircd_snprintf(0, path, sizeof(path), "%s/hashes", feature_str(FEAT_DDBPATH));
  alarm(3);
  fd = open(path, O_WRONLY, S_IRUSR | S_IWUSR);
  if (fd == -1)
  {
    fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
      ddb_die("Error when creating hashes file (OPEN)");

    for (table = DDB_INIT; table <= DDB_END; table++)
    {
      ircd_snprintf(0, path, sizeof(path), "%c AAAAAAAAAAAA\n", table);
      write(fd, path, 15);
    }
  }
  close(fd);
  alarm(0);

  /* Verify if tables file is exist. */
  for (table = DDB_INIT; table <= DDB_END; table++)
  {
    ircd_snprintf(0, path, sizeof(path), "%s/table.%c",
                  feature_str(FEAT_DDBPATH), table);
    alarm(3);
    fd = open(path, O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
      fd = open(path, O_CREAT, S_IRUSR | S_IWUSR);
      if (fd == -1)
        ddb_die("Error when creating table '%c' (OPEN)", table);
    }
    close(fd);
    alarm(0);
  }
}

int
ddb_db_cache(void)
{
  /* No hay cache de DDB */
  return 0;

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
  struct ddb_memory_table map_table;
  char path[1024];
  char buf[1024];
  int fd, cont;
  int int_return;

  int_return = 1; /* 1 = success */

  ircd_snprintf(0, path, sizeof(path), "%s/table.%c",
                feature_str(FEAT_DDBPATH), table);
  alarm(3);
  fd = open(path, O_RDONLY, S_IRUSR | S_IWUSR);
  get_ddb_stat(fd, &ddb_stats_table[table]);

  memcpy(&map_table.file_stat, &ddb_stats_table[table],
         sizeof(ddb_stats_table[table]));

  map_table.position = mmap(NULL, map_table.file_stat.size, PROT_READ,
                             MAP_SHARED | MAP_NORESERVE, fd, 0);

  if (fd == -1)
    ddb_die("Error when reading table '%c' (OPEN)", table);
  if ((map_table.file_stat.size != 0) && (map_table.position == MAP_FAILED))
    ddb_die("Error when reading table '%c' (MMAP)", table);

  close(fd);
  alarm(0);

  map_table.point_r = map_table.position;

  cont = ddb_seek(&map_table, buf, id);
  if (cont == -1)
  {
    *buf = '\0';
    return -1;
  }

  /* Read registers */
  do
  {
    char *mask, *key, *content;
    int cid;

    cid = atoi(buf);
    if (!cid)
      continue;

    mask = strchr(buf, ' ');
    if (!mask)
      continue;
    *mask++ = '\0';

    key = strchr(mask, ' ');
    if (!key)
      continue;
    *key++ = '\0';

    content = strchr(key, ' ');
    if (content)
      *content++ = '\0';

    if (!cptr)
      /* IRCD starting */
      ddb_new_register(NULL, table, cid, mask, key, content);
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

  } while(ddb_read(&map_table, buf) != -1);


  /* Close mmap file */
  munmap(map_table.position, map_table.file_stat.size);

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
ddb_db_write(unsigned char table, unsigned int id, char *mask, char *key, char *content)
{
  struct ddb_stat ddbstat;
  char path[1024];
  char *corrupt;
  int fd, offset;

  ircd_snprintf(0, path, sizeof(path), "%s/table.%c",
                feature_str(FEAT_DDBPATH), table);

  alarm(3);
  fd = open(path, O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
  if (fd == -1)
    ddb_die("Error when saving new key in table '%c' (OPEN)", table);

  get_ddb_stat(fd, &ddbstat);
  corrupt = check_corrupt_table(table, &ddbstat);
  if (corrupt)
    ddb_die("A nonauthorized modification is detect in table '%c' [%s]",
            table, corrupt);

  offset = lseek(fd, 0 , SEEK_CUR);
  if (offset < 0)
    ddb_die("Error when saving new key in table '%c' (LSEEK)", table);

  ircd_snprintf(0, path, sizeof(path), "%d %s %s%s%s\n", id, mask, key,
                content ? " " : "", content ? content : "");
  if (write(fd, path, strlen(path)) == -1)
  {
    /* ftruncate(fd, offset); */
    ddb_die("Error when saving new key in table '%c' (WRITE)", table);
  }

  get_ddb_stat(fd, &ddb_stats_table[table]);
  close(fd);
  alarm(0);
}

/** Delete a table.
 * @param[in] table Table of the %DDB Distributed DataBase.
 */
void
ddb_db_drop(unsigned char table)
{
  char path[1024];
  int fd;

  ircd_snprintf(0, path, sizeof(path), "%s/table.%c",
                feature_str(FEAT_DDBPATH), table);

  alarm(3);
  fd = open(path, O_TRUNC, S_IRUSR | S_IWUSR);

  get_ddb_stat(fd, &ddb_stats_table[table]);

  if (fd == -1)
    ddb_die("Error when droping table '%c' (OPEN)", table);

  close(fd);
  alarm(0);
}

/** Pack the table.
 * @param[in] table Table of the %DDB Distributed DataBase.
 */
void
ddb_db_compact(unsigned char table)
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
  char path[1024];
  char c;
  int fd;

  ircd_snprintf(0, path, sizeof(path), "%s/hashes", feature_str(FEAT_DDBPATH));

  alarm(3);
  fd = open(path, O_RDONLY, S_IRUSR | S_IWUSR);
  if (fd == -1)
  {
    *hi = *lo = 0;
    return;
  }

  if (lseek(fd, (15 * (table - DDB_INIT)) + 2, SEEK_SET) == -1)
    ddb_die("Error when reading table '%c' hashes (LSEEK)", table);

  read(fd, path, 12);
  close(fd);
  alarm(0);

  path[12] = '\0';
  c = path[6];
  path[6] = '\0';
  *hi = base64toint(path);
  path[6] = c;
  *lo = base64toint(path + 6);
}

/** Write the hash.
 * @param[in] table Table of the %DDB Distributed DataBase.
 */
void
ddb_db_hash_write(unsigned char table)
{
  char path[1024];
  char hash[20];
  int fd;

  ircd_snprintf(0, path, sizeof(path), "%s/hashes", feature_str(FEAT_DDBPATH));
  alarm(3);
  fd = open(path, O_WRONLY, S_IRUSR | S_IWUSR);
  if (fd == -1)
    ddb_die("Error when saving table '%c' hashes (OPEN)", table);

  if (lseek(fd, (15 * (table - DDB_INIT)), SEEK_SET) == -1)
    ddb_die("Error when saving table '%c' hashes (LSEEK)", table);

  inttobase64(hash, ddb_hashtable_hi[table], 6);
  inttobase64(hash + 6, ddb_hashtable_lo[table], 6);
  ircd_snprintf(0, path, sizeof(path), "%c %s\n", table, hash);
  if (write(fd, path, strlen(path)) == -1)
    ddb_die("Error when saving table '%c' hashes (WRITE)", table);
  close(fd);
  alarm(0);
}


/** Executes when finalizes the %DDB subsystem.
 */
void
ddb_db_end(void)
{
  /* Backup copy? */
}

/** Read the table.
 * @param[in,out] map_table Structure ddb_memory_table.
 * @param[in] buf Buffer.
 */
static int
ddb_read(struct ddb_memory_table *map_table, char *buf)
{
  char *ptr = map_table->position + map_table->file_stat.size;
  int len = 0;

  while (map_table->point_r < ptr)
  {
    *buf = *(map_table->point_r++);
    if (*buf == '\r')
      continue;
    if ((*buf++ == '\n'))
    {
      *--buf = '\0';
      return len;
    }
    if (len++ > 500)
    {
      break;
    }
  }
  *buf = '\0';
  return -1;
}

/** Seek the table.
 * @param[in,out] memory_table Structure ddb_memory_table.
 * @param[in] buf Buffer.
 * @param[in] id ID number in the table.
 */
static int
ddb_seek(struct ddb_memory_table *map_table, char *buf, unsigned int id)
{
  char *ptr, *ptr2, *ptrhi, *ptrlo;
  unsigned int idtable;

  if (!id)
  {
    map_table->point_r = map_table->position;
    return ddb_read(map_table, buf);
  }

  ptrlo = map_table->position;
  ptrhi = ptrlo + map_table->file_stat.size;

  while (ptrlo != ptrhi)
  {
    ptr2 = ptr = ptrlo + ((ptrhi - ptrlo) / 2);

    while ((ptr >= ptrlo) && (*ptr != '\n'))
      ptr--;
    if (ptr < ptrlo)
      ptr = ptrlo;
    if (*ptr == '\n')
      ptr++;
    while ((ptr2 < ptrhi) && (*ptr2++ != '\n'));

    idtable = atol(ptr);
    if (idtable > id)
      ptrhi = ptr;
    else if (idtable < id)
      ptrlo = ptr2;
    else
    {
      ptrlo = ptrhi = ptr;
      break;
    }
  }
  map_table->point_r = ptrlo;

  return ddb_read(map_table, buf);
}

/** Fstat wrapper.
 * @param[in] fd File Descriptor.
 * @param[out] ddbstat Structure ddb_stat.
 */
static void
get_ddb_stat(int fd, struct ddb_stat *ddbstat)
{
  struct stat st;

  fstat(fd, &st);
  memset(ddbstat, 0, sizeof(*ddbstat));

  ddbstat->dev = st.st_dev;
  ddbstat->ino = st.st_ino;
  ddbstat->size = st.st_size;
  ddbstat->mtime = st.st_mtime;
}

/** Checking if a table is corrupt
 * @param[in] table Table of the %DDB Distributed DataBase.
 * @param[in] ddbstat Structure ddb_stat
 * @return If is NULL, the table is not corrupt else, return the reason message.
 */
static char *
check_corrupt_table(unsigned char table, struct ddb_stat *ddbstat)
{
  char *msg = NULL;

  /* TODO */
  return msg;
  if (memcmp(&ddbstat, &ddb_stats_table[table], sizeof(ddbstat)))
  {
    char buf[1024];
    char *space = "";

    if (ddbstat->dev != ddb_stats_table[table].dev)
    {
      ircd_snprintf(0, buf, sizeof(buf), "DEVICE %llu<>%llu",
                    ddbstat->dev, ddb_stats_table[table].dev);
      space = " ";
      Debug((DEBUG_INFO, "check_corrupt1: %s", buf));
    }

    if (ddbstat->ino != ddb_stats_table[table].ino)
    {
      ircd_snprintf(0, buf + strlen(buf), sizeof(buf), "%sINODE %llu<>%llu",
                    space, ddbstat->ino, ddb_stats_table[table].ino);
      space = " ";
      Debug((DEBUG_INFO, "check_corrupt2: %s", buf));
    }

    if (ddbstat->size != ddb_stats_table[table].size)
    {
      ircd_snprintf(0, buf + strlen(buf), sizeof(buf), "%sSIZE %llu<>%llu",
                    space, ddbstat->size, ddb_stats_table[table].size);
      space = " ";
      Debug((DEBUG_INFO, "check_corrupt3: %s", buf));
    }

    if (ddbstat->mtime != ddb_stats_table[table].mtime)
    {
      ircd_snprintf(0, buf + strlen(buf), sizeof(buf), "%sMTIME %llu<>%llu",
                    space, ddbstat->mtime, ddb_stats_table[table].mtime);
      Debug((DEBUG_INFO, "check_corrupt4: %s", buf));
    }
    Debug((DEBUG_INFO, "check_corrupt: %s", buf));
    ircd_strncpy(msg, buf, strlen(buf));
  }
/* TODO */
  return NULL;
  return msg;
} 
