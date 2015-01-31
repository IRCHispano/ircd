/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/dbuf.c
 *
 * Copyright (C) 2002-2015 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1990 Markku Savela
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
 * @brief Implementation of functions dealing with data buffers.
 * @version $Id$
 */
#include "config.h"

#include "dbuf.h"
#include "client.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_chattr.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_zlib.h"
#include "send.h"
#if defined(USE_ZLIB)
#include "zlib.h"
#endif

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <string.h>

/*
 * dbuf is a collection of functions which can be used to
 * maintain a dynamic buffering of a byte stream.
 * Functions allocate and release memory dynamically as
 * required [Actually, there is nothing that prevents
 * this package maintaining the buffer on disk, either]
 */

/** Number of dbufs allocated.
 * This should only be modified by dbuf.c.
 */
unsigned int DBufAllocCount = 0;
/** Number of dbufs in use.
 * This should only be modified by dbuf.c.
 */
unsigned int DBufUsedCount = 0;

/** List of allocated but unused DBuf structures. */
static struct DBufBuffer *dbufFreeList = 0;

/** Size of data for a single DBufBuffer. */
#define DBUF_SIZE 2048

/** Single data buffer in a DBuf. */
struct DBufBuffer {
  struct DBufBuffer *next;      /**< Next data buffer, NULL if last */
  char *start;                  /**< data starts here */
  char *end;                    /**< data ends here */
  char data[DBUF_SIZE];         /**< Actual data stored here */
};

/** Return memory used by allocated data buffers.
 * @param[out] allocated Receives number of bytes allocated to DBufs.
 * @param[out] used Receives number of bytes for currently used DBufs.
 */
void dbuf_count_memory(size_t *allocated, size_t *used)
{
  assert(0 != allocated);
  assert(0 != used);
  *allocated = DBufAllocCount * sizeof(struct DBufBuffer);
  *used = DBufUsedCount * sizeof(struct DBufBuffer);
}

/** Allocate a new DBufBuffer.
 * If #dbufFreeList != NULL, use the head of that list; otherwise,
 * allocate a new buffer.
 * @return Newly allocated buffer list.
 */
static struct DBufBuffer *dbuf_alloc(void)
{
  struct DBufBuffer* db = dbufFreeList;

  if (db) {
    dbufFreeList = db->next;
    ++DBufUsedCount;
  }
  else if (DBufAllocCount * DBUF_SIZE < feature_uint(FEAT_BUFFERPOOL)) {
    db = (struct DBufBuffer*) MyMalloc(sizeof(struct DBufBuffer));
    assert(0 != db);
    ++DBufAllocCount;
    ++DBufUsedCount;
  }
  return db;
}

/** Release a DBufBuffer back to the free list.
 * @param[in] db Data buffer to release.
 */
static void dbuf_free(struct DBufBuffer *db)
{
  assert(0 != db);
  --DBufUsedCount;
  db->next = dbufFreeList;
  dbufFreeList = db;
}

/** Handle a memory allocation error on a DBuf.
 * This frees all the buffers owned by the DBuf, since we have to
 * close the associated connection.
 * @param[in] dyn DBuf to clean out.
 * @return Zero.
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

/** Append bytes to a data buffer.
 * @param[in] dyn Buffer to append to.
 * @param[in] buf Data to append.
 * @param[in] length Number of bytes to append.
 * @return Non-zero on success, or zero on failure.
 */
static int dbuf_put_native(struct DBuf *dyn, const char *buf, unsigned int length)
{
  struct DBufBuffer** h;
  struct DBufBuffer*  db;
  unsigned int chunk;

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

  for (; length > 0; h = &(db->next)) {
    if (0 == (db = *h)) {
      if (0 == (db = dbuf_alloc())) {
	if (feature_bool(FEAT_HAS_FERGUSON_FLUSHER)) {
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
	  flush_connections(0);
	  db = dbuf_alloc();
	}

        if (0 == db)
          return dbuf_malloc_error(dyn);
      }
      dyn->tail = db;
      *h = db;
      db->next = 0;
      db->start = db->end = db->data;
    }
    chunk = (db->data + DBUF_SIZE) - db->end;
    if (chunk) {
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

#if defined(USE_ZLIB)
/** Append bytes to a ZLIB data buffer.
 * @param[in] dyn Buffer to append to.
 * @param[in] buf Data to append.
 * @param[in] length Number of bytes to append.
 * @return Non-zero on success, or zero on failure.
 */
static int dbuf_put_zlib(struct Client *cptr, struct DBuf *dyn, const char *buf, unsigned int length)
{
  static void *tmp = NULL;
  int flag = Z_NO_FLUSH;
  int compresion = 0;
  int estado, f;

  assert(0 != dyn);
  assert(0 != buf);
  /*
   * Pongo BUFSIZE*3, pero si hago el
   * "include" tengo un monton de problemas,
   * asi que lo hago a mano.
   */
  if (!tmp)
  {
    tmp = MyMalloc(512 * 3);
    if (!tmp)
      return dbuf_malloc_error(dyn);
  }
  if (cptr && MyConnect(cptr) && (cptr->cli_connect->zlib_negociation & ZLIB_OUT))
  {
    struct zlib_mburst *p = p_microburst;
    long length_out = cli_connect(cptr)->comp_out->total_out;

    compresion = !0;
    cli_connect(cptr)->comp_out->next_in = (void *)buf;
    cli_connect(cptr)->comp_out->avail_in = length;
    cli_connect(cptr)->comp_out_total_in += length;
    cli_connect(cptr)->comp_out->next_out = tmp;
    cli_connect(cptr)->comp_out->avail_out = 512 * 3;  /* Ojo con esta cifra */

    if (microburst)
    {
      estado = deflate(cli_connect(cptr)->comp_out, Z_NO_FLUSH);
      length_out -= cli_connect(cptr)->comp_out->total_out;
      if (length_out < 0)
        length_out = -length_out;
      cli_connect(cptr)->comp_out_total_out += length_out;
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
          p = MyMalloc(sizeof(struct zlib_mburst));
          assert(0 != p);
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
      estado = deflate(cli_connect(cptr)->comp_out, Z_PARTIAL_FLUSH);
      flag = Z_PARTIAL_FLUSH;
      length_out -= cli_connect(cptr)->comp_out->total_out;
      if (length_out < 0)
        length_out = -length_out;
      cli_connect(cptr)->comp_out_total_out += length_out;
    }
    assert(Z_OK == estado);
    buf = tmp;
    length = (cli_connect(cptr)->comp_out->next_out) - (Bytef *) tmp;
    if (!length)
      return 1;
  }

  while (!0)
  {
    long length_out;

    f = dbuf_put_native(dyn, buf, length);
    if (!compresion || (f < 0) || cli_connect(cptr)->comp_out->avail_out)
      return f;

    /* Queda mas */
    send_queued(cptr);
    cli_connect(cptr)->comp_out->next_out = tmp;
    cli_connect(cptr)->comp_out->avail_out = 512 * 3;  /* Ojo con esta cifra */
    length_out = cli_connect(cptr)->comp_out->total_out;
    estado = deflate(cli_connect(cptr)->comp_out, flag);
    length_out -= cli_connect(cptr)->comp_out->total_out;
    if (length_out < 0)
      length_out = -length_out;
    cli_connect(cptr)->comp_out_total_out += length_out;
    assert(Z_OK == estado);
    buf = tmp;
    length = (cli_connect(cptr)->comp_out->next_out) - (Bytef *) tmp;
    if (!length)
      return f;
  }
}
#endif

/** Append bytes to a data buffer.
 * @param[in] cptr .
 * @param[in] dyn Buffer to append to.
 * @param[in] buf Data to append.
 * @param[in] length Number of bytes to append.
 * @return Non-zero on success, or zero on failure.
 */
int dbuf_put(struct Client *cptr, struct DBuf *dyn, const char *buf, unsigned int length)
{
#if defined(USE_ZLIB)
  return dbuf_put_zlib(cptr, dyn, buf, length);
#else
  return dbuf_put_native(dyn, buf, length);
#endif
}

/** Get the first contiguous block of data from a DBuf.
 * Generally a call to dbuf_map(dyn, &count) will be followed with a
 * call to dbuf_delete(dyn, count).
 * @param[in] dyn DBuf to retrieve data from.
 * @param[out] length Receives number of bytes in block.
 * @return Pointer to start of block (or NULL if the first block is empty).
 */
static
const char *dbuf_map(const struct DBuf* dyn, unsigned int* length)
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

/** Discard data from a DBuf.
 * @param[in,out] dyn DBuf to drop data from.
 * @param[in] length Number of bytes to discard.
 */
void dbuf_delete(struct DBuf *dyn, unsigned int length)
{
  struct DBufBuffer *db;
  unsigned int chunk;

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

/** Copy data from a buffer and remove what was copied.
 * @param[in,out] dyn Buffer to copy from.
 * @param[out] buf Buffer to write to.
 * @param[in] length Maximum number of bytes to copy.
 * @return Number of bytes actually copied.
 */
unsigned int dbuf_get(struct DBuf *dyn, char *buf, unsigned int length)
{
  unsigned int moved = 0;
  unsigned int chunk;
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

/** Flush empty lines from a buffer.
 * @param[in,out] dyn Data buffer to flush.
 * @return Number of bytes in first available block (or zero if none).
 */
static unsigned int dbuf_flush(struct DBuf *dyn)
{
  struct DBufBuffer *db = dyn->head;

  if (0 == db)
    return 0;

  assert(db->start < db->end);
  /*
   * flush extra line terms
   */
  while (IsEol(*db->start))
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

/** Copy a single line from a data buffer.
 * If the output buffer cannot hold the whole line, or if there is no
 * EOL in the buffer, return 0.
 * @param[in,out] dyn Data buffer to copy from.
 * @param[out] buf Buffer to copy to.
 * @param[in] length Maximum number of bytes to copy.
 * @return Number of bytes copied to \a buf.
 */
unsigned int dbuf_getmsg(struct DBuf *dyn, char *buf, unsigned int length)
{
  struct DBufBuffer *db;
  char *start;
  char *end;
  unsigned int count;
  unsigned int copied = 0;

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
    end = IRCD_MIN(db->end, (start + length));
    while (start < end && !IsEol(*start))
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
