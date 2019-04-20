/*
 * IRC - Internet Relay Chat, include/sys.h
 * Copyright (C) 1990 University of Oulu, Computing Center
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

#if !defined(__sys_include__)
#define __sys_include__

#include "../config/config.h"
#include "../config/setup.h"
#include <signal.h>

#if WORDS_BIGENDIAN
# define BIT_ZERO_ON_LEFT
#else
# define BIT_ZERO_ON_RIGHT
#endif

#if defined(BSD_RELIABLE_SIGNALS)
#if defined(SYSV_UNRELIABLE_SIGNALS) || defined(POSIX_SIGNALS)
#error You stuffed up config.h signals #defines use only one.
#endif
#define HAVE_RELIABLE_SIGNALS
#endif

#if defined(SYSV_UNRELIABLE_SIGNALS)
#if defined(POSIX_SIGNALS)
#error You stuffed up config.h signals #defines use only one.
#endif
#undef	HAVE_RELIABLE_SIGNALS
#endif

#if defined(POSIX_SIGNALS)
#define HAVE_RELIABLE_SIGNALS
#endif

/*
 * safety margin so we can always have one spare fd, for motd/authd or
 * whatever else.  -24 allows "safety" margin of 10 listen ports, 8 servers
 * and space reserved for logfiles, DNS sockets and identd sockets etc.
 */
#define MAXCLIENTS	(MAXCONNECTIONS-24)

#if defined(CLIENT_FLOOD)
#if (CLIENT_FLOOD > 8000) || (CLIENT_FLOOD < 512)
#error CLIENT_FLOOD needs redefining.
#endif
#else
#define CLIENT_FLOOD 2048
#endif

#if !defined(CONFIG_SETUGID)
#undef IRC_UID
#undef IRC_GID
#endif

#define Reg1
#define Reg2
#define Reg3
#define Reg4
#define Reg5
#define Reg6
#define Reg7
#define Reg8
#define Reg9
#define Reg10

#define register

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>

#if HAVE_ERRNO_H
# include <errno.h>
#else
# if HAVE_NET_ERRNO_H
#  include <net/errno.h>
# endif
#endif

/* See AC_HEADER_STDC in 'info autoconf' */
#if STDC_HEADERS
# include <string.h>
#else
# if HAVE_STRING_H
#  include <string.h>
# endif
# if !defined(HAVE_STRCHR)
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(), *strrchr(), *strtok();
# if HAVE_MEMORY_H              /* See AC_MEMORY_H in 'info autoconf' */
#  include <memory.h>
# endif
# if !defined(HAVE_MEMCPY)
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memset(a, b, c) bzero(a, c) /* We ONLY use memset(x, 0, y) */
# else
#  if NEED_BZERO                /* This is not used yet - needs to be added to `configure' */
#   define bzero(a, c) memset((a), 0, (c))  /* Some use it in FD_ZERO */
#  endif
# endif
# if !defined(HAVE_MEMMOVE)
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#if (defined(__STRICT_ANSI__))
#include <sys/select.h>
#endif

/* See AC_HEADER_TIME in 'info autoconf' */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#define OPT_TYPE void

#define LIMIT_FMT "%ld"

#if defined(DEBUGMODE) && !defined(DEBUGMALLOC)
#define DEBUGMALLOC
#endif

#if defined(STDC_HEADERS)
#include <stdlib.h>
#include <stddef.h>
#else /* !STDC_HEADERS */
#if defined(HAVE_MALLOC_H)
#include <malloc.h>
#else
#if defined(HAVE_SYS_MALLOC_H)
#include <sys/malloc.h>
#endif /* HAVE_SYS_MALLOC_H */
#endif /* HAVE_MALLOC_H */
#endif /* !STDC_HEADERS */

#if !defined(MAX)
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif
#if !defined(MIN)
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#if !defined(FALSE)
#define FALSE (0)
#endif
#if !defined(TRUE)
#define TRUE  (!FALSE)
#endif

#if !defined(offsetof)
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#include "runmalloc.h"

#define MyCoreDump raise(SIGABRT)

/* This isn't really POSIX :(, but we really need it -- can this be replaced ? */
#if defined(__STRICT_ANSI__)
extern int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

/*
 * The following part is donated by Carlo Wood from his package 'libr':
 * (C) Copyright 1996 by Carlo Wood. All rights reserved.
 */

#if defined(__cplusplus)
#define HANDLER_ARG(x) x
#define UNUSED(x)
#else
#define HANDLER_ARG(x)
#define UNUSED(x) x __attribute__ ((unused))
#endif

#endif /* __sys_include__ */
