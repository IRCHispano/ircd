/*
 * IRC - Internet Relay Chat, common/dbuf.c
 * Copyright (C) 1990 Markku Savela
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "sys.h"
#include "h.h"
#include "s_debug.h"
#include "common.h"
#include "struct.h"
#include "dbuf.h"
#include "ircd_alloc.h"
#include "s_serv.h"
#include "list.h"

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
#include "send.h"
#endif

#include <assert.h>
#include <string.h>

RCSTAG_CC("$Id$");

/*
 * dbuf is a collection of functions which can be used to
 * maintain a dynamic buffering of a byte stream.
 * Functions allocate and release memory dynamically as
 * required [Actually, there is nothing that prevents
 * this package maintaining the buffer on disk, either]
 */

int DBufAllocCount = 0;
int DBufUsedCount = 0;

static struct DBufBuffer *dbufFreeList = 0;

#define DBUF_SIZE 2048

struct DBufBuffer {
  struct DBufBuffer *next;      /* Next data buffer, NULL if last */
  char *start;                  /* data starts here */
  char *end;                    /* data ends here */
  char data[DBUF_SIZE];         /* Actual data stored here */
};

void dbuf_count_memory(size_t *allocated, size_t *used)
{
  assert(0 != allocated);
  assert(0 != used);
  *allocated = DBufAllocCount * sizeof(struct DBufBuffer);
  *used = DBufUsedCount * sizeof(struct DBufBuffer);
}

/*
 * dbuf_alloc - allocates a DBufBuffer structure from the free list or
 * creates a new one.
 */
static struct DBufBuffer *dbuf_alloc(void)
{
  struct DBufBuffer *db = dbufFreeList;

  if (db)
  {
    ++DBufUsedCount;
    dbufFreeList = db->next;
  }
  else if (DBufAllocCount * DBUF_SIZE < BUFFERPOOL)
  {
    if ((db = (struct DBufBuffer *)MyMalloc(sizeof(struct DBufBuffer))))
    {
      ++DBufAllocCount;
      ++DBufUsedCount;
    }
  }
  return db;
}

/*
 * dbuf_free - return a struct DBufBuffer structure to the freelist
 */
static void dbuf_free(struct DBufBuffer *db)
{
  assert(0 != db);
  --DBufUsedCount;
  db->next = dbufFreeList;
  dbufFreeList = db;
}

/*
 * This is called when malloc fails. Scrap the whole content
 * of dynamic buffer. (malloc errors are FATAL, there is no
 * reason to continue this buffer...).
 * After this the "dbuf" has consistent EMPTY status.
 */
static int dbuf_malloc_error(struct DBuf *dyn)
{
  struct DBufBuffer *db;
  struct DBufBuffer *next;

  for (db = dyn->head; db; db = next)
  {
    next = db->next;
    dbuf_free(db);
  }
  dyn->tail = dyn->head = 0;
  dyn->length = 0;
  return 0;
}

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
static int microburst = 0;

struct p_mburst {
  struct Client *cptr;
  struct DBuf *dyn;
  struct p_mburst *next;
};

static struct p_mburst *p_microburst = NULL;
static struct p_mburst *p_microburst_cache = NULL;

void inicia_microburst(void)
{
  microburst++;
}

void completa_microburst(void)
{
  struct p_mburst *p, *p2;
  static int ciclos_mburst = 0;

/* Deberian estar anidados, pero por si acaso */
  if (!microburst)
    return;

  if (!(--microburst))
  {
    for (p = p_microburst; p; p = p2)
    {
/*
** p->cptr puede ser NULL si la
** conexion se ha cerrado durante
** la "microrafaga".
*/
      if ((p->cptr != NULL) && MyConnect(p->cptr)
          && (p->cptr->negociacion & ZLIB_ESNET_OUT) && (p->dyn != NULL))
      {
        dbuf_put(p->cptr, p->dyn, NULL, 0);
        UpdateWrite(p->cptr);
      }
      p2 = p->next;
      if (++ciclos_mburst >= 937)
      {                         /* Numero primo */
        ciclos_mburst = 1;
        MyFree(p);
      }
      else
      {
        p->next = p_microburst_cache;
        p_microburst_cache = p;
      }
    }
    p_microburst = NULL;
  }
}

void inicializa_microburst(void)
{
  if (!microburst)
    return;                     /* Esto es lo ideal */
/*
** Si quedan rafagas abiertas, hay que investigarlo
*/
  microburst = 1;
  completa_microburst();
}

void elimina_cptr_microburst(struct Client *cptr)
{
  struct p_mburst *p;

  for (p = p_microburst; p; p = p->next)
  {
    if (p->cptr == cptr)
    {                           /* Lo ideal seria eliminar el registro */
      p->cptr = NULL;
    }
  }
}

#endif


/*
 * dbuf_put - Append the number of bytes to the buffer, allocating memory 
 * as needed. Bytes are copied into internal buffers from users buffer.
 *
 * Returns > 0, if operation successful
 *         < 0, if failed (due memory allocation problem)
 *
 * dyn:         Dynamic buffer header
 * buf:         Pointer to data to be stored
 * length:      Number of bytes to store
 */
static int dbuf_put2(struct Client *cptr, struct DBuf *dyn, const char *buf,
    size_t length)
{
  struct DBufBuffer **h;
  struct DBufBuffer *db;
  size_t chunk;

  assert(0 != dyn);
  assert(0 != buf);
  /*
   * Locate the last non-empty buffer. If the last buffer is full,
   * the loop will terminate with 'db==NULL'.
   * This loop assumes that the 'dyn->length' field is correctly
   * maintained, as it should--no other check really needed.
   */
  if (!dyn->length)
    h = &(dyn->head);
  else
    h = &(dyn->tail);
  /*
   * Append users data to buffer, allocating buffers as needed
   */
  dyn->length += length;

  for (; length > 0; h = &(db->next))
  {
    if (0 == (db = *h))
    {
      if (0 == (db = dbuf_alloc())) {
#if defined(FERGUSON_FLUSHER)
        /*
         * from "Married With Children" episode were Al bought a REAL toilet
         * on the black market because he was tired of the wimpy water
         * conserving toilets they make these days --Bleep
         */
        /*
         * Apparently this doesn't work, the server _has_ to
         * dump a few clients to handle the load. A fully loaded
         * server cannot handle a net break without dumping some
         * clients. If we flush the connections here under a full
         * load we may end up starving the kernel for mbufs and
         * crash the machine
         */
        /*
         * attempt to recover from buffer starvation before
         * bailing this may help servers running out of memory
         */
        flush_sendq_except(dyn);
        if (0 == (db = dbuf_alloc()))
#endif
          return dbuf_malloc_error(dyn);
      }
      dyn->tail = db;
      *h = db;
      db->next = 0;
      db->start = db->end = db->data;
    }
    chunk = (db->data + DBUF_SIZE) - db->end;
    if (chunk)
    {
      if (chunk > length)
        chunk = length;

      memcpy(db->end, buf, chunk);

      length -= chunk;
      buf += chunk;
      db->end += chunk;
    }
  }
  return 1;
}

int dbuf_put(struct Client *cptr, struct DBuf *dyn, const char *buf,
    size_t length)
{
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
  static void *tmp = NULL;
  int flag = Z_NO_FLUSH;
  int compresion = 0;
  int estado, f;

/*
** Pongo BUFSIZE*3, pero si hago el
** "include" tengo un monton de problemas,
** asi que lo hago a mano.
*/
  if (!tmp)
  {
    tmp = MyMalloc(512 * 3);
    if (!tmp)
      return dbuf_malloc_error(dyn);
  }
  if ((cptr != NULL) && MyConnect(cptr) && (cptr->negociacion & ZLIB_ESNET_OUT))
  {
    struct p_mburst *p = p_microburst;
    long length_out = cptr->comp_out->total_out;

    compresion = !0;
    cptr->comp_out->next_in = (void *)buf;
    cptr->comp_out->avail_in = length;
    cptr->comp_out_total_in += length;
    cptr->comp_out->next_out = tmp;
    cptr->comp_out->avail_out = 512 * 3;  /* Ojo con esta cifra */
    if (microburst)
    {
      estado = deflate(cptr->comp_out, Z_NO_FLUSH);
      length_out -= cptr->comp_out->total_out;
      if (length_out < 0)
        length_out = -length_out;
      cptr->comp_out_total_out += length_out;
      while (p && (cptr != p->cptr))
        p = p->next;
      if (p == NULL)
      {
        p = p_microburst_cache;
        if (p)
        {
          p_microburst_cache = p->next;
        }
        else
        {
          p = MyMalloc(sizeof(struct p_mburst));
          if (!p)
          {
            outofmemory();
            assert(0);
          }
        }
        p->next = p_microburst;
        p->cptr = cptr;
        p->dyn = dyn;
        p_microburst = p;
      }
      assert(p->dyn == dyn);
    }
    else
    {
      estado = deflate(cptr->comp_out, Z_PARTIAL_FLUSH);
      flag = Z_PARTIAL_FLUSH;
      length_out -= cptr->comp_out->total_out;
      if (length_out < 0)
        length_out = -length_out;
      cptr->comp_out_total_out += length_out;
    }
    assert(Z_OK == estado);
    buf = tmp;
    length = (cptr->comp_out->next_out) - (Bytef *) tmp;
    if (!length)
      return 1;
  }

  while (!0)
  {
    long length_out;

    f = dbuf_put2(cptr, dyn, buf, length);
    if (!compresion || (f < 0) || cptr->comp_out->avail_out)
      return f;

    /* Queda mas */
    send_queued(cptr);
    cptr->comp_out->next_out = tmp;
    cptr->comp_out->avail_out = 512 * 3;  /* Ojo con esta cifra */
    length_out = cptr->comp_out->total_out;
    estado = deflate(cptr->comp_out, flag);
    length_out -= cptr->comp_out->total_out;
    if (length_out < 0)
      length_out = -length_out;
    cptr->comp_out_total_out += length_out;
    assert(Z_OK == estado);
    buf = tmp;
    length = (cptr->comp_out->next_out) - (Bytef *) tmp;
    if (!length)
      return f;
  }
#else
  return dbuf_put2(cptr, dyn, buf, length);
#endif
}

/*
 * dbuf_map, dbuf_delete
 *
 * These functions are meant to be used in pairs and offer a more efficient
 * way of emptying the buffer than the normal 'dbuf_get' would allow--less
 * copying needed.
 *
 *    map     returns a pointer to a largest contiguous section
 *            of bytes in front of the buffer, the length of the
 *            section is placed into the indicated "long int"
 *            variable. Returns NULL *and* zero length, if the
 *            buffer is empty.
 *
 *    delete  removes the specified number of bytes from the
 *            front of the buffer releasing any memory used for them.
 *
 *    Example use (ignoring empty condition here ;)
 *
 *            buf = dbuf_map(&dyn, &count);
 *            <process N bytes (N <= count) of data pointed by 'buf'>
 *            dbuf_delete(&dyn, N);
 *
 *    Note:   delete can be used alone, there is no real binding
 *            between map and delete functions...
 *
 * dyn:         Dynamic buffer header
 * length:      Return number of bytes accessible
 */
const char *dbuf_map(const struct DBuf *dyn, size_t *length)
{
  assert(0 != dyn);
  assert(0 != length);

  if (0 == dyn->length)
  {
    *length = 0;
    return 0;
  }
  assert(0 != dyn->head);

  *length = dyn->head->end - dyn->head->start;
  return dyn->head->start;
}

/*
 * dbuf_delete - delete length bytes from DBuf
 *
 * dyn:         Dynamic buffer header
 * length:      Number of bytes to delete
 */
void dbuf_delete(struct DBuf *dyn, size_t length)
{
  struct DBufBuffer *db;
  size_t chunk;

  if (length > dyn->length)
    length = dyn->length;

  while (length > 0)
  {
    if (0 == (db = dyn->head))
      break;
    chunk = db->end - db->start;
    if (chunk > length)
      chunk = length;

    length -= chunk;
    dyn->length -= chunk;
    db->start += chunk;

    if (db->start == db->end)
    {
      dyn->head = db->next;
      dbuf_free(db);
    }
  }
  if (0 == dyn->head)
  {
    dyn->length = 0;
    dyn->tail = 0;
  }
}

/*
 * dbuf_get
 *
 * Remove number of bytes from the buffer, releasing dynamic memory,
 * if applicaple. Bytes are copied from internal buffers to users buffer.
 *
 * Returns the number of bytes actually copied to users buffer,
 * if >= 0, any value less than the size of the users
 * buffer indicates the dbuf became empty by this operation.
 *
 * Return 0 indicates that buffer was already empty.
 *
 * dyn:         Dynamic buffer header
 * buf:         Pointer to buffer to receive the data
 * length:      Max amount of bytes that can be received
 */
size_t dbuf_get(struct DBuf *dyn, char *buf, size_t length)
{
  size_t moved = 0;
  size_t chunk;
  const char *b;

  assert(0 != dyn);
  assert(0 != buf);

  while (length > 0 && (b = dbuf_map(dyn, &chunk)) != 0)
  {
    if (chunk > length)
      chunk = length;

    memcpy(buf, b, chunk);
    dbuf_delete(dyn, chunk);

    buf += chunk;
    length -= chunk;
    moved += chunk;
  }
  return moved;
}

static size_t dbuf_flush(struct DBuf *dyn)
{
  struct DBufBuffer *db = dyn->head;

  if (0 == db)
    return 0;

  assert(db->start < db->end);
  /*
   * flush extra line terms
   */
  while (isEol(*db->start))
  {
    if (++db->start == db->end)
    {
      dyn->head = db->next;
      dbuf_free(db);
      if (0 == (db = dyn->head))
      {
        dyn->tail = 0;
        dyn->length = 0;
        break;
      }
    }
    --dyn->length;
  }
  return dyn->length;
}


/*
 * dbuf_getmsg - Check the buffers to see if there is a string which is
 * terminated with either a \r or \n present.  If so, copy as much as 
 * possible (determined by length) into buf and return the amount copied 
 * else return 0.
 */
size_t dbuf_getmsg(struct DBuf *dyn, char *buf, size_t length)
{
  struct DBufBuffer *db;
  char *start;
  char *end;
  size_t count;
  size_t copied = 0;

  assert(0 != dyn);
  assert(0 != buf);

  if (0 == dbuf_flush(dyn))
    return 0;

  assert(0 != dyn->head);

  db = dyn->head;
  start = db->start;

  assert(start < db->end);

  if (length > dyn->length)
    length = dyn->length;
  /*
   * might as well copy it while we're here
   */
  while (length > 0)
  {
    end = MIN(db->end, (start + length));
    while (start < end && !isEol(*start))
      *buf++ = *start++;

    count = start - db->start;
    if (start < end)
    {
      *buf = '\0';
      copied += count;
      dbuf_delete(dyn, copied);
      dbuf_flush(dyn);
      return copied;
    }
    if (0 == (db = db->next))
      break;
    copied += count;
    length -= count;
    start = db->start;
  }
  return 0;
}
