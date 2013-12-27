/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ddb_db_persistent.c
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
 * @brief Persistent DataBase mplementation of Distributed DataBases.
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
#include "persistent_malloc.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/*
 * ATENCION: Lo que sigue debe incrementarse cuando se toque alguna estructura de la BDD
 */
#define MMAP_CACHE_VERSION 4

static struct db_reg db_buf_cache[DB_BUF_CACHE];
static int db_buf_cache_i = 0;

/*
 * ddb_db_native
 */
struct ddb_memory_table {
  struct ddb_stat file_stat;
  char *position;   /* Posicion */
  char *point_r;    /* Lectura */
};

static void *mmap_cache_pos = NULL;
static int persistent_hit;

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

/*
** ATENCION: El multiplicador al final de esta rutina
** me permite variar el n�mero de version del MMAP cache.
*/
static unsigned int mmap_cache_version(void)
{
  unsigned int version;
  int i;

  version = sizeof(struct db_reg) * DB_MAX_TABLA;

  for (i = 0; i < DB_MAX_TABLA; i++)
    version *= 1 + tabla_residente_y_len[i] * (i + 1);

  return version * MMAP_CACHE_VERSION;  /* El multiplicador me permite variar el numero de version del MMAP cache */
}

/*
** mmap_cache
** Resultado:
**   Si no se puede crear el fichero, termina a saco.
**   0  - El fichero mapeado correctamente, pero su contenido no nos vale.
**   !0 - El fichero mapeado correctamente, y contenido valido.
*/
int
ddb_db_cache(void)
{
  char path_buf[sizeof(DBPATH) + 1024];
  char hashes_buf[16384];
  int handle, handle2;
  int flag_problemas = 0;
  unsigned int *pos = NULL, *pos2, *p;
  unsigned char *p_char;
  unsigned long len = BDD_MMAP_SIZE * 1024 * 1024;
  unsigned int len2, len_used;
  unsigned int version;
  unsigned int hash[2], v[2], k[2], x[2];
  int i;
  int hlen;
  static int done;
  struct portable_stat st, *st2;

  assert(!done);
  done = 1;

  handle = open(BDD_MMAP_PATH, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

  if (handle == -1)
    db_die("Error al intentar crear el fichero MMAP cache de la BDD (open)",
        '*');

  if (ftruncate(handle, len))
    db_die("Error al intentar crear el fichero MMAP cache de la BDD (truncate)",
        '*');

  pos2 = pos =
      (unsigned int *)mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
      handle, 0);

  if (pos == MAP_FAILED)
    db_die("Error al intentar crear el fichero MMAP cache de la BDD (mmap1)",
        '*');

/*
** Ojo. Esto no es muy heterodoxo en plataformas 64 bits, como ALPHA.
** NO CAMBIAR EL ORDEN.
*/
  hash[0] = *pos2++;
  hash[1] = *pos2++;
  version = *pos2;
/*
** Cambiamos el numero de version y no el HASH, para detectarlo
** rapidamente, sin tener que evaluar un HASH de megabytes.
*/
  *pos2++ ^= 231231231u;
  len2 = *pos2++;
  len_used = *pos2++;

  assert(sizeof(unsigned int) <= 8);
  if (len_used & 7)
    flag_problemas = 1;

  mmap_cache_pos = (void *)(*pos2++);

  hlen = (pos2 - pos) * sizeof(unsigned int);

  if (len2 != len)
    flag_problemas = 1;

  if (version != mmap_cache_version())
    flag_problemas = 1;

  if (flag_problemas)
    mmap_cache_pos = NULL;

#define HASH_FILE_SIZE  390

/*
** El +1 es por el '\0' final.
** El sizeof+1 es para alineamiento de pagina, con la division que hay mas adelante.
*/
  hlen += HASH_FILE_SIZE + 1 + sizeof(unsigned int) - 1;

  if (!flag_problemas)
  {
    sprintf_irc(path_buf, "%s/hashes", DBPATH);
    handle2 = open(path_buf, O_RDONLY);
    if (handle < 0)
    {
      flag_problemas = 1;
    }
    else
    {
      memset(hashes_buf, 0, sizeof(hashes_buf));
      i = read(handle2, hashes_buf, sizeof(hashes_buf) - 16);
      close(handle2);
      if (i != HASH_FILE_SIZE)
      {
        flag_problemas = 1;
      }
      else
      {
        if (strcmp(hashes_buf, (unsigned char *)pos2))
        {
          flag_problemas = 1;
        }
      }
    }
  }
  if (!flag_problemas)
  {
    p = pos + 2;                /* Nos saltamos el HASH inicial */
    pos2 = (unsigned int *)((unsigned char *)pos + len_used);
    k[0] = k[1] = 23;
    x[0] = x[1] = 0;
/*
** La primera iteracion es especial, por el numero de version modificado.
** No hace falta el ntohl, porque si se intenta usar el MMAP
** en otra arquitectura, queremos que falle la verificacion.
*/
    v[0] = (*p++) ^ 231231231u; /* Numero de serie modificado */
    v[1] = *p++;
    tea(v, k, x);

    while (p < pos2)
    {
      v[0] = *p++;
      v[1] = *p++;
      tea(v, k, x);
    }
    if ((x[0] != hash[0]) || (x[1] != hash[1]))
      flag_problemas = 1;
  }

  munmap((void *)pos, len);

  if (flag_problemas)
    mmap_cache_pos = NULL;

  pos = mmap_cache_pos;

/*
** Solo se debe poner en una posicion fija si se indica una posicion...
*/
  mmap_cache_pos =
      mmap(mmap_cache_pos, len, PROT_READ | PROT_WRITE,
      MAP_SHARED | (mmap_cache_pos ? MAP_FIXED : 0), handle, 0);
  if (mmap_cache_pos == MAP_FAILED)
  {
    mmap_cache_pos =
        mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);
    flag_problemas = 1;
    if (mmap_cache_pos == MAP_FAILED)
      db_die("Error al intentar crear el fichero MMAP cache de la BDD (mmap2)",
          '*');
  }

  close(handle);

  if (pos && (pos != mmap_cache_pos))
    flag_problemas = 1;

  pos = mmap_cache_pos;
  pos += hlen / sizeof(unsigned int);

/*
** Debemos leer aunque sea "flag_problemas",
** para dejar el puntero en el sitio.
*/
  for (i = 0; i < DB_MAX_TABLA; i++)
  {
    if (*pos++ != tabla_residente_y_len[i])
    {
      flag_problemas = 1;       /* Hay que seguir leyendo, para dejar el puntero en su sitio */
    }
  }

/*
** Debemos leer aunque sea "flag_problemas",
** para dejar el puntero en el sitio.
*/
  st2 = (struct portable_stat *)persistent_align(pos);
  for (i = 0; i < DB_MAX_TABLA; i++)
  {
    if ((i >= ESNET_BDD) && (i <= ESNET_BDD_END))
    {
      sprintf_irc(path_buf, "%s/tabla.%c", DBPATH, i);
      handle = open(path_buf, O_RDONLY, S_IRUSR | S_IWUSR);
      assert(handle != -1);
      get_stat(handle, &st);
      close(handle);
      memcpy(&tabla_stats[i], &st, sizeof(st));
      if (memcmp(&st, st2, sizeof(st)))
      {
        flag_problemas = 1;     /* Hay que seguir leyendo, para dejar el puntero en su sitio */
      }
    }
    st2++;
  }
  pos = (unsigned int *)st2;

/*
** No hace falta borrarlo todo. De hecho es
** contraproducente, porque si tenemos un fichero cache
** de 100 MB y solo se usan 20, no hay necesidad de
** meter en memoria 100 MB que se van a sobreescribir inmediatamente.
*/
  if (flag_problemas)
    memset(mmap_cache_pos, 0,
        4096 + hlen + sizeof(tabla_residente_y_len) + sizeof(tabla_stats) +
        sizeof(tabla_cuantos) + sizeof(tabla_datos) + sizeof(tabla_serie) +
        sizeof(tabla_hash_hi) + sizeof(tabla_hash_lo));

  p_char = (unsigned char *)pos;
  memcpy(tabla_cuantos, p_char, sizeof(tabla_cuantos));
  p_char += sizeof(tabla_cuantos);
  memcpy(tabla_datos, p_char, sizeof(tabla_datos));
  p_char += sizeof(tabla_datos);
  memcpy(tabla_serie, p_char, sizeof(tabla_serie));
  p_char += sizeof(tabla_serie);
  memcpy(tabla_hash_hi, p_char, sizeof(tabla_hash_hi));
  p_char += sizeof(tabla_hash_hi);
  memcpy(tabla_hash_lo, p_char, sizeof(tabla_hash_lo));
  p_char += sizeof(tabla_hash_lo);

  persistent_init(p_char, len - (p_char - (unsigned char *)mmap_cache_pos) - 64,
      flag_problemas ? NULL : (unsigned char *)mmap_cache_pos + len_used);
  persistent_hit = !flag_problemas;
  return persistent_hit;

  if (persistent_hit) {
/*
** CACHE HIT
**
** Aqui hacemos las operaciones que causan
** cambios de estado internos. Es decir, los
** registros que por existir modifican
** estructuras internas. El caso evidente
** son los canales persistentes.
*/
    i = tabla_residente_y_len[BDD_CHANDB];
    assert(i);
    for (i--; i >= 0; i--)
    {
      for (reg = tabla_datos[BDD_CHANDB][i]; reg != NULL; reg = reg->next)
      {
        crea_canal_persistente(reg->clave, reg->valor, 1);
      }
    }
    if ((reg =
        db_buscar_registro(BDD_CONFIGDB,
        BDD_NUMERO_MAXIMO_DE_CLONES_POR_DEFECTO)))
    {
      numero_maximo_de_clones_por_defecto = atoi(reg->valor);
    }
    if ((reg = db_buscar_registro(BDD_CONFIGDB, BDD_CLAVE_DE_CIFRADO_DE_IPS)))
    {
      char tmp, clave[12 + 1];

      clave_de_cifrado_de_ips = reg->valor;
      strncpy(clave, clave_de_cifrado_de_ips, 12);
      clave[12] = '\0';
      tmp = clave[6];
      clave[6] = '\0';
      clave_de_cifrado_binaria[0] = base64toint(clave); /* BINARIO */
      clave[6] = tmp;
      clave_de_cifrado_binaria[1] = base64toint(clave + 6); /* BINARIO */

    }
#if defined(WEBCHAT)
    if ((reg = db_buscar_registro(BDD_CONFIGDB, BDD_CLAVE_DE_CIFRADO_DE_COOKIES)))
    {
      char key[45];
      char *v = reg->valor;
      int key_len;

      memset(key, 'A', sizeof(key));

      key_len = strlen(v);
      key_len = (key_len>44) ? 44 : key_len;

      strncpy((char *)key+(44-key_len), v, (key_len));
      key[44]='\0';

      base64_to_buf_r(clave_de_cifrado_de_cookies, key);
      cifrado_cookies=1;
    }
#endif
   }


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
  char path[1024];
  int fd;

  ircd_snprintf(0, path, sizeof(path), "%s/table.%c",
                feature_str(FEAT_DDBPATH), table);

  alarm(3);
  fd = open(path, O_TRUNC, S_IRUSR | S_IWUSR);

  get_ddb_stat(fd, &ddb_stats_table[table]);
  persistent_hit = 0;

  if (fd == -1)
    ddb_die("Error when droping table '%c' (OPEN)", table);

  close(fd);
  alarm(0);
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

/** Executes whe finalizes the %DDB subsystem.
 */
void
ddb_db_end(void)
{
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

