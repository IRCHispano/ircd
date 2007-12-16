/*
 * Run's malloc/realloc/calloc/free DEBUG tools v2.0
 *
 * (c) Copyright 1996, 1997
 *
 * Author:
 *
 * 1024/624ACAD5 1997/01/26 Carlo Wood, Run on IRC <carlo@runaway.xs4all.nl>
 * Key fingerprint = 32 EC A7 B6 AC DB 65 A6  F6 F6 55 DD 1C DC FF 61
 * Get key from pgp-public-keys server or
 * finger carlo@runaway.xs4all.nl for public key (dialin, try at 21-22h GMT).
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

#include "sys.h"

#if defined(DEBUGMALLOC)
#include <stdlib.h>
#include "h.h"
#include "s_debug.h"

RCSTAG_CC("$Id: runmalloc.c,v 1.1.1.1 1999/11/16 05:13:14 codercom Exp $");

#define MALLOC_HASHTABLE_SIZE 16384
#define MallocHash(x) \
    ((unsigned int)(((((long int)(x) >> 4) * 0xDEECE66D) >> 16) & (long int)0x3fff))
#define MAGIC_PREFIX 0xe4c483a1
#define MAGIC_POSTFIX 0x435bd0fa

#if defined(MEMLEAKSTATS)
typedef struct {
  const char *filename;
  int line;
  int number_of_allocations;
#if defined(MEMSIZESTATS)
  size_t size;
#endif
} location_st;

#define LOCSIZE 1024            /* Maximum of 256 different locations */
static location_st location[LOCSIZE];
static unsigned int locations;  /* Counter */

static unsigned int find_location(const char *filename, int line)
{
  unsigned int hash;
  hash = line & 0xff;
  while (location[hash].filename && (location[hash].line != line ||
      location[hash].filename != filename))
    if (++hash == LOCSIZE)
      hash = 0;
  if (!location[hash].filename)
  {
    /* New location */
    ++locations;
    location[hash].filename = filename;
    location[hash].line = line;
  }
  return hash;
}

#endif

#if defined(MEMMAGICNUMS)
/* The size of this struct should be a multiple of 4 bytes, just in case... */
typedef struct {
#if defined(MEMMAGICNUMS)
  unsigned int prefix_magicnumber;
#endif
} prefix_blk_st;

#define SIZEOF_PREFIX sizeof(prefix_blk_st)
#else
typedef void prefix_blk_st;
#define SIZEOF_PREFIX 0
#endif

#if defined(MEMMAGICNUMS)
typedef struct {
  unsigned int postfix_magicnumber;
} postfix_blk_st;

#define SIZEOF_POSTFIX sizeof(postfix_blk_st)
#define HAS_POSTFIX
#else
#define SIZEOF_POSTFIX 0
#endif

typedef struct hash_entry_st {
  struct hash_entry_st *next;
  prefix_blk_st *ptr;
#if defined(MEMSIZESTATS)
  size_t size;
#endif
#if defined(MEMLEAKSTATS)
  unsigned int location;
#if defined(MEMTIMESTATS)
  time_t when;
#endif
#endif
} hash_entry_st;

#define memblkp(prefix_ptr) \
    ((void *)((size_t)prefix_ptr + SIZEOF_PREFIX))
#define prefixp(memblk_ptr) \
    ((prefix_blk_st *)((size_t)memblk_ptr - SIZEOF_PREFIX))
#define postfixp(memblk_ptr, size) \
    ((postfix_blk_st *)((size_t)memblk_ptr + size))

static hash_entry_st *hashtable[MALLOC_HASHTABLE_SIZE];
#if defined(MEMSIZESTATS)
static size_t mem_size = 0;     /* Number of allocated bytes  */
static unsigned int alloc_cnt = 0;  /* Number of allocated blocks */
#endif

#if defined(MEMLEAKSTATS)
#include "struct.h"
#include "send.h"
#include "numeric.h"
#include "s_err.h"
#include "ircd.h"
#include "s_serv.h"
#include "numnicks.h"

void report_memleak_stats(aClient *sptr, int parc, char *parv[])
{
  unsigned int hash;
  location_st *loc = location;

#if defined(MEMTIMESTATS)
  time_t till = now;
  time_t from = me.serv->boot_timestamp;
  if (parc > 3)
  {
    location_st tmp_loc[LOCSIZE];
    hash_entry_st **start;
    memset(tmp_loc, 0, sizeof(tmp_loc));
    if (parc > 3)
      till -= atoi(parv[3]);
    if (parc > 4)
      from += atoi(parv[4]);
    for (start = &hashtable[0];
        start < &hashtable[MALLOC_HASHTABLE_SIZE]; ++start)
    {
      hash_entry_st *hash_entry;
      for (hash_entry = *start; hash_entry; hash_entry = hash_entry->next)
        if (hash_entry->when >= from && hash_entry->when <= till)
        {
#if defined(MEMSIZESTATS)
          tmp_loc[hash_entry->location].size += hash_entry->size;
#endif
          tmp_loc[hash_entry->location].number_of_allocations++;
        }
    }
    loc = tmp_loc;
    if (MyUser(sptr) || Protocol(sptr->from) < 10)
      sendto_one(sptr, ":%s NOTICE %s :Memory allocated between " TIME_T_FMT
          " (server start + %s s) and " TIME_T_FMT " (now - %s s):",
          me.name, parv[0], from, parc > 4 ? parv[4] : "0", till,
          parc > 3 ? parv[3] : "0");
    else
      sendto_one(sptr, "%s " TOK_NOTICE " %s%s :Memory allocated between " TIME_T_FMT
          " (server start + %s s) and " TIME_T_FMT " (now - %s s):",
          NumServ(&me), NumNick(sptr), from, parc > 4 ? parv[4] : "0", till,
          parc > 3 ? parv[3] : "0");
  }
#endif
  for (hash = 0; hash < LOCSIZE; ++hash)
    if (loc[hash].number_of_allocations > 0)
      sendto_one(sptr, rpl_str(RPL_STATMEM), me.name, parv[0],
          loc[hash].number_of_allocations,
          location[hash].line, location[hash].filename
#if defined(MEMSIZESTATS)
          , loc[hash].size
#endif
          );
}

void *RunMalloc_memleak(size_t size, int line, const char *filename)
#else
void *RunMalloc(size_t size)
#endif
{
  prefix_blk_st *ptr;
  hash_entry_st *hash_entry;
  hash_entry_st **hashtablep;

#if defined(HAS_POSTFIX)
  size += 3;
  size &= ~3;
#endif

  if (!((ptr = (prefix_blk_st *)
      malloc(SIZEOF_PREFIX + size + SIZEOF_POSTFIX)) &&
      (hash_entry = (hash_entry_st *) malloc(sizeof(hash_entry_st)))))
  {
    if (ptr)
      free(ptr);
    Debug((DEBUG_FATAL, "Out of memory !"));
    return NULL;
  }

  hashtablep = &hashtable[MallocHash(ptr)];
  hash_entry->next = *hashtablep;
  *hashtablep = hash_entry;
  hash_entry->ptr = ptr;
#if defined(MEMLEAKSTATS)
#if defined(MEMTIMESTATS)
  hash_entry->when = now;
#endif
  location[(hash_entry->location =
      find_location(filename, line))].number_of_allocations++;
#endif
#if defined(MEMSIZESTATS)
  hash_entry->size = size;
#if defined(MEMLEAKSTATS)
  location[hash_entry->location].size += size;
#endif
  mem_size += size;
  ++alloc_cnt;
#endif
#if defined(MEMMAGICNUMS)
  ptr->prefix_magicnumber = MAGIC_PREFIX;
  postfixp(memblkp(ptr), size)->postfix_magicnumber = MAGIC_POSTFIX;
#endif

  Debug((DEBUG_DEBUG, "RunMalloc(%u) = %p", size, memblkp(ptr)));

  return memblkp(ptr);
}

#if defined(MEMLEAKSTATS)
void *RunCalloc_memleak(size_t nmemb, size_t size,
    int line, const char *filename)
#else
void *RunCalloc(size_t nmemb, size_t size)
#endif
{
  void *ptr;
  size *= nmemb;
#if defined(MEMLEAKSTATS)
  if ((ptr = RunMalloc_memleak(size, line, filename)))
#else
  if ((ptr = RunMalloc(size)))
#endif
    memset(ptr, 0, size);
  return ptr;
}

int RunFree_test(void *memblk_ptr)
{
  prefix_blk_st *prefix_ptr = prefixp(memblk_ptr);
  hash_entry_st *hash_entry;
  for (hash_entry = hashtable[MallocHash(prefix_ptr)];
      hash_entry && hash_entry->ptr != prefix_ptr;
      hash_entry = hash_entry->next);
  return hash_entry ? 1 : 0;
}

void RunFree(void *memblk_ptr)
{
  prefix_blk_st *prefix_ptr = prefixp(memblk_ptr);
  hash_entry_st *hash_entry, *prev_hash_entry = NULL;
  unsigned int hash = MallocHash(prefix_ptr);

  Debug((DEBUG_DEBUG, "RunFree(%p)", memblk_ptr));

  if (!memblk_ptr)
    return;

  for (hash_entry = hashtable[hash];
      hash_entry && hash_entry->ptr != prefix_ptr;
      prev_hash_entry = hash_entry, hash_entry = hash_entry->next);
  if (!hash_entry)
  {
    Debug((DEBUG_FATAL, "FREEING NON MALLOC PTR !!!"));
    MyCoreDump;
  }
#if defined(MEMMAGICNUMS)
  if (prefix_ptr->prefix_magicnumber != MAGIC_PREFIX)
  {
    Debug((DEBUG_FATAL, "MAGIC_PREFIX CORRUPT !"));
    MyCoreDump;
  }
  prefix_ptr->prefix_magicnumber = 12345678;
  if (postfixp(memblk_ptr, hash_entry->size)->postfix_magicnumber
      != MAGIC_POSTFIX)
  {
    Debug((DEBUG_FATAL, "MAGIC_POSTFIX CORRUPT !"));
    MyCoreDump;
  }
  postfixp(memblk_ptr, hash_entry->size)->postfix_magicnumber = 87654321;
#endif

  if (prev_hash_entry)
    prev_hash_entry->next = hash_entry->next;
  else
    hashtable[hash] = hash_entry->next;

#if defined(MEMLEAKSTATS)
  location[hash_entry->location].number_of_allocations--;
#endif

#if defined(MEMSIZESTATS)
  mem_size -= hash_entry->size;
  --alloc_cnt;
#if defined(MEMLEAKSTATS)
  location[hash_entry->location].size -= hash_entry->size;
#endif
#if defined(DEBUGMODE)
  /* Put 0xfefefefe.. in freed memory */
#if !defined(memset)
  memset(prefix_ptr, 0xfe, hash_entry->size + SIZEOF_PREFIX);
#else
  {
    char *p = prefix_ptr;
    size_t len = hash_entry->size + SIZEOF_PREFIX;
    for (; len; --len)
      *p++ = 0xfe;
  }
#endif
#endif
#endif

  free(hash_entry);
  free(prefix_ptr);
}

#if defined(MEMLEAKSTATS)
void *RunRealloc_memleak(void *memblk_ptr, size_t size,
    int line, const char *filename)
#else
void *RunRealloc(void *memblk_ptr, size_t size)
#endif
{
  prefix_blk_st *ptr;
  prefix_blk_st *prefix_ptr = prefixp(memblk_ptr);
  hash_entry_st *hash_entry, *prev_hash_entry = NULL;
  hash_entry_st **hashtablep;
  unsigned int hash;

  if (!memblk_ptr)
#if defined(MEMLEAKSTATS)
    return RunMalloc_memleak(size, line, filename);
#else
    return RunMalloc(size);
#endif
  if (!size)
  {
    RunFree(memblk_ptr);
    return NULL;
  }

  for (hash_entry = hashtable[(hash = MallocHash(prefix_ptr))];
      hash_entry && hash_entry->ptr != prefix_ptr;
      prev_hash_entry = hash_entry, hash_entry = hash_entry->next);
  if (!hash_entry)
  {
    Debug((DEBUG_FATAL, "REALLOCATING NON MALLOC PTR !!!"));
    MyCoreDump;
  }

#if defined(MEMMAGICNUMS)
  if (prefix_ptr->prefix_magicnumber != MAGIC_PREFIX)
  {
    Debug((DEBUG_FATAL, "MAGIC_PREFIX CORRUPT !"));
    MyCoreDump;
  }
  if (postfixp(memblk_ptr, hash_entry->size)->postfix_magicnumber
      != MAGIC_POSTFIX)
  {
    Debug((DEBUG_FATAL, "MAGIC_POSTFIX CORRUPT !"));
    MyCoreDump;
  }
#endif

#if defined(HAS_POSTFIX)
  size += 3;
  size &= ~3;
#endif

#if defined(MEMMAGICNUMS)
  postfixp(memblkp(prefix_ptr), hash_entry->size)->postfix_magicnumber = 123456;
#endif
#if defined(MEMLEAKSTATS)
  location[hash_entry->location].number_of_allocations--;
#if defined(MEMSIZESTATS)
  location[hash_entry->location].size -= hash_entry->size;
#endif
#endif

  if (!(ptr =
      (prefix_blk_st *) realloc(prefix_ptr,
      SIZEOF_PREFIX + size + SIZEOF_POSTFIX)))
  {
    Debug((DEBUG_FATAL, "RunRealloc: Out of memory :"));
    return NULL;
  }

  if (prev_hash_entry)
    prev_hash_entry->next = hash_entry->next;
  else
    hashtable[hash] = hash_entry->next;

  hashtablep = &hashtable[MallocHash(ptr)];
  hash_entry->next = *hashtablep;
  *hashtablep = hash_entry;
  hash_entry->ptr = ptr;
#if defined(MEMLEAKSTATS)
#if defined(MEMTIMESTATS)
  hash_entry->when = now;
#endif
  location[(hash_entry->location =
      find_location(filename, line))].number_of_allocations++;
#endif
#if defined(MEMSIZESTATS)
  mem_size += size - hash_entry->size;
  hash_entry->size = size;
#if defined(MEMLEAKSTATS)
  location[hash_entry->location].size += size;
#endif
#endif
#if defined(MEMMAGICNUMS)
  postfixp(memblkp(ptr), size)->postfix_magicnumber = MAGIC_POSTFIX;
#endif

  Debug((DEBUG_DEBUG, ": RunRealloc(%p, %u) = %p",
      memblk_ptr, size, memblkp(ptr)));

  return memblkp(ptr);
}

#if defined(MEMSIZESTATS)
unsigned int get_alloc_cnt(void)
{
  return alloc_cnt;
}

size_t get_mem_size(void)
{
  return mem_size;
}

#endif

#endif /* DEBUGMALLOC */
