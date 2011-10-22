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

#if defined(__osf__)
#define _OSF_SOURCE
#endif

#if defined(__sun__)
#if defined(__svr4__)
#define SOL2
#else
#define SUNOS4
#endif
#endif

/* Para Solaris 10 de algun nodo */
#if defined(__sun__)
#define u_int16_t unsigned short
#define u_int32_t unsigned int
#endif

#if WORDS_BIGENDIAN
# define BIT_ZERO_ON_LEFT
#else
# define BIT_ZERO_ON_RIGHT
#endif

#if defined(_SEQUENT_)          /* Dynix 1.4 or 2.0 Generic Define.. */
#undef BSD
#define SYSV                    /* Also #define SYSV */
#endif

#if defined(__hpux)
#define HPUX
#endif

#if defined(sgi)
#define SGI
#endif

#if defined(mips)
#undef SYSV
#undef BSD
#define BSD 1                   /* mips only works in bsd43 environment */
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

#if defined(HAVECURSES)
#define DOCURSES
#else
#undef DOCURSES
#endif

#if defined(HAVETERMCAP)
#define DOTERMCAP
#else
#undef DOTERMCAP
#endif

#if !defined(UNIXPORT)
#undef	UNIXPORTPATH
#endif

#if defined(CLIENT_FLOOD)
#if (CLIENT_FLOOD > 8000) || (CLIENT_FLOOD < 512)
#error CLIENT_FLOOD needs redefining.
#endif
#else
#error CLIENT_FLOOD undefined
#endif

#if !defined(CONFIG_SETUGID)
#undef IRC_UID
#undef IRC_GID
#endif
/*
#define Reg1 register
#define Reg2 register
#define Reg3 register
#define Reg4 register
#define Reg5 register
#define Reg6 register
#define Reg7 register
#define Reg8 register
#define Reg9 register
#define Reg10 register
i*/

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

/* Define FD_SETSIZE to what we want before including sys/types.h on BSD */
#if  defined(__FreeBSD__) || defined(__NetBSD__) || defined(__bsdi__)
#if ((!defined(USE_POLL)) && (!defined(FD_SETSIZE)))
#define FD_SETSIZE ((MAXCONNECTIONS)+4)
#endif
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>

#if defined(__osf__)
#undef _OSF_SOURCE
/* Buggy header */
#include <netdb.h>
#define _OSF_SOURCE
#endif

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

#if defined(_AIX) || (defined(__STRICT_ANSI__) && __GLIBC__ >= 2)
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

#if defined(SOL2)
#define OPT_TYPE char           /* opt type for get/setsockopt */
#else
#define OPT_TYPE void
#endif

#if defined(SUNOS4)
#define LIMIT_FMT "%d"
#else
#if (defined(__bsdi__) || defined(__NetBSD__))
#define LIMIT_FMT "%qd"
#else
#define LIMIT_FMT "%ld"
#endif
#endif

/* Different name on NetBSD and FreeBSD --Skip */
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__bsdi__)
#define dn_skipname  __dn_skipname
#endif

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

#define MyCoreDump *((int *)NULL)=0

/* This isn't really POSIX :(, but we really need it -- can this be replaced ? */
#if defined(__STRICT_ANSI__) && !defined(_AIX)
extern int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

/*
 * The following part is donated by Carlo Wood from his package 'libr':
 * (C) Copyright 1996 by Carlo Wood. All rights reserved.
 */

/* GNU CC improvements: We can only use this if we have a gcc/g++ compiler */
#if defined(__GNUC__)

#if (__GNUC__ < 2) || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#define NO_ATTRIBUTE
#endif

#else /* !__GNUC__ */

/* No attributes if we don't have gcc-2.7 or higher */
#define NO_ATTRIBUTE

#endif /* !__GNUC__ */

#if defined(__cplusplus)
#define HANDLER_ARG(x) x
#define UNUSED(x)
#else
#define HANDLER_ARG(x)
#if defined(NO_ATTRIBUTE)
#define __attribute__(x)
#define UNUSED(x) unused_##x
#else
#define UNUSED(x) x __attribute__ ((unused))
#endif
#endif

#if defined(NO_ATTRIBUTE)
#define RCSTAG_CC(string) static char unused_rcs_ident[] = string
#else
#define RCSTAG_CC(string) static char rcs_ident[] __attribute__ ((unused)) = string
#endif

#if defined(HAVE_SYS_CDEFS_H) && !defined(__sun__)
#include <sys/cdefs.h>
#else /* !HAVE_SYS_MALLOC_H */
#undef __BEGIN_DECLS
#undef __END_DECLS
#if defined(__cplusplus)
#define __BEGIN_DECLS   extern "C" {
#define __END_DECLS     }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif /* !HAVE_SYS_CDEFS_H */

#endif /* __sys_include__ */
