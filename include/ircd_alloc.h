/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_alloc.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1999 Thomas Helvey <tomh@inxpress.net>
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
 * @brief IRC daemon memory allocation functions.
 * @version $Id: ircd_alloc.h,v 1.6 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef INCLUDED_ircd_alloc_h
#define INCLUDED_ircd_alloc_h

/*
 * memory resource allocation and test functions
 */
/** Type of handler for out-of-memory conditions. */
typedef void (*OutOfMemoryHandler)(void);
extern void set_nomem_handler(OutOfMemoryHandler handler);

/* The mappings for the My* functions... */
/** Helper macro for standard allocations. */
#define MyMalloc(size) \
  DoMalloc(size, "malloc", __FILE__, __LINE__)

/** Helper macro for zero-initialized allocations. */
#define MyCalloc(nelem, size) \
  DoMallocZero((size) * (nelem), "calloc", __FILE__, __LINE__)

/** Helper macro for freeing memory. */
#define MyFree(p) \
  do { if (p) DoFree(p, __FILE__, __LINE__); } while(0)

/** Helper macro for reallocating memory. */
#define MyRealloc(p, size) \
  DoRealloc(p, size, __FILE__, __LINE__)

/* First version: fast non-debugging macros... */
#ifndef MDEBUG
#ifndef INCLUDED_stdlib_h
#include <stdlib.h> /* free */
#define INCLUDED_stdlib_h
#endif

/** Implementation macro for freeing memory. */
#define DoFree(x, file, line) do { free((x)); (x) = 0; } while(0)
extern void* DoMalloc(size_t len, const char*, const char*, int);
extern void* DoMallocZero(size_t len, const char*, const char*, int);
extern void *DoRealloc(void *, size_t, const char*, int);

/* Second version: slower debugging versions... */
#else /* defined(MDEBUG) */
#include <sys/types.h>
#include "memdebug.h"

#define DoMalloc(size, type, file, line) \
  dbg_malloc(size, type, file, line)
#define DoMallocZero(size, type, file, line) \
  dbg_malloc_zero(size, type, file, line)
#define DoFree(p, file, line) \
  do { dbg_free(p, file, line); (p) = 0; } while (0)
#define DoRealloc(p, size, file, line) \
  dbg_realloc(p, size, file, line)
#endif /* defined(MDEBUG) */

#endif /* INCLUDED_ircd_alloc_h */
