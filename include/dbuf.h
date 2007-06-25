/*
 * IRC - Internet Relay Chat, include/dbuf.h
 * Copyright (C) 1990 Markku Savela
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
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
#if !defined(INCLUDED_dbuf_h)
#define INCLUDED_dbuf_h
#if !defined(INCLUDED_sys_types_h)
#include <sys/types.h>          /* size_t */
#define INCLUDED_sys_types_h
#endif

/*
 * These two globals should be considered read only
 */
extern int DBufAllocCount;      /* GLOBAL - count of dbufs allocated */
extern int DBufUsedCount;       /* GLOBAL - count of dbufs in use */

struct DBufBuffer;

struct DBuf {
  size_t length;                /* Current number of bytes stored */
  struct DBufBuffer *head;      /* First data buffer, if length > 0 */
  struct DBufBuffer *tail;      /* last data buffer, if length > 0 */
};

/*
 * DBufLength - Returns the current number of bytes stored into the buffer.
 */
#define DBufLength(dyn) ((dyn)->length)

/*
 * DBufClear - Scratch the current content of the buffer.
 * Release all allocated buffers and make it empty.
 */
#define DBufClear(dyn) dbuf_delete((dyn), DBufLength(dyn))

/*
 * Prototypes
 */
extern void dbuf_delete(struct DBuf *dyn, size_t length);
extern int dbuf_put(struct Client *cptr, struct DBuf *dyn, const char *buf,
    size_t length);
extern const char *dbuf_map(const struct DBuf *dyn, size_t *length);
extern size_t dbuf_get(struct DBuf *dyn, char *buf, size_t length);
extern size_t dbuf_getmsg(struct DBuf *dyn, char *buf, size_t length);
extern void dbuf_count_memory(size_t *allocated, size_t *used);

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
void inicia_microburst(void);
void completa_microburst(void);
void inicializa_microburst(void);
void elimina_cptr_microburst(struct Client *cptr);
#endif

#endif /* INCLUDED_dbuf_h */
