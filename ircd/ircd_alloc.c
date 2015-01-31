/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ircd_alloc.c
 *
 * Copyright (C) 2002-2015 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @version $Id: ircd_alloc.c,v 1.9 2007-04-19 22:53:47 zolty Exp $
 */
#include "config.h"

#include "ircd_alloc.h"
#include "ircd_log.h"
#include "ircd_string.h"
#include "s_debug.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <string.h>

static void nomem_handler(void);

/** Variable holding out-of-memory callback. */
static OutOfMemoryHandler noMemHandler = nomem_handler;

/** Default handler for out-of-memory conditions. */
static void
nomem_handler(void)
{
  log_write(LS_SYSTEM, L_CRIT, 0, "Out of memory, exiting");
#ifdef MDEBUG
  assert(0);
#else
  Debug((DEBUG_FATAL, "Out of memory, exiting"));
  exit(2);
#endif
}

/** Set callback function for out-of-memory conditions. */
void
set_nomem_handler(OutOfMemoryHandler handler)
{
  noMemHandler = handler;
}

#ifndef MDEBUG
/** Allocate memory.
 * @param[in] size Number of bytes to allocate.
 * @param[in] x Type of allocation (ignored).
 * @param[in] y Name of file doing allocation (ignored).
 * @param[in] z Line number doing allocation (ignored).
 * @return Newly allocated block of memory.
 */
void* DoMalloc(size_t size, const char* x, const char* y, int z)
{
  void* t = malloc(size);
  if (!t)
    (*noMemHandler)();
  return t;
}

/** Allocate zero-initialized memory.
 * @param[in] size Number of bytes to allocate.
 * @param[in] x Type of allocation (ignored).
 * @param[in] y Name of file doing allocation (ignored).
 * @param[in] z Line number doing allocation (ignored).
 * @return Newly allocated block of memory.
 */
void* DoMallocZero(size_t size, const char* x, const char* y, int z)
{
  void* t = malloc(size);
  if (!t)
    (*noMemHandler)();
  memset(t, 0, size);
  return t;
}

/** Resize an allocated block of memory.
 * @param[in] orig Original block to resize.
 * @param[in] size Minimum size for new block.
 * @param[in] file Name of file doing reallocation (ignored).
 * @param[in] line Line number doing reallocation (ignored).
 */
void* DoRealloc(void *orig, size_t size, const char *file, int line)
{
  void* t = realloc(orig, size);
  if (!t)
    (*noMemHandler)();
  return t;
}
#endif
