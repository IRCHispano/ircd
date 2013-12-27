/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/memdebug.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
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
 * $Id: memdebug.h,v 1.4 2007-04-19 22:53:46 zolty Exp $
 */
/* This file should only ever be included from ircd_alloc.h */

void *dbg_malloc(size_t size, const char *type, const char *file, int line);
void *dbg_malloc_zero(size_t size, const char *type, const char *file, int line);
void *dbg_realloc(void *ptr, size_t size, const char *file, int line);
void dbg_free(void *ptr, const char *file, int line);
size_t fda_get_byte_count(void);
size_t fda_get_block_count(void);
