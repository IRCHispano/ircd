/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/s_misc.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1990 Jarkko Oikarinen
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
/** @file s_misc.h
 * @brief Miscellaneous support functions and declarations.
 * @version $Id: s_misc.h,v 1.6 2007-04-19 22:53:47 zolty Exp $
 */
#ifndef INCLUDED_s_misc_h
#define INCLUDED_s_misc_h

#ifndef INCLUDED_stdarg_h
#include <stdarg.h>           /* va_list */
#define INCLUDED_stdarg_h
#endif
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>        /* time_t */
#define INCLUDED_sys_types_h
#endif


struct Client;
struct StatDesc;
struct ConfItem;

/*-----------------------------------------------------------------------------
 * Macros
 */

/** Return value from various functions to indicate the source has
 * been disconnected. */
#define CPTR_KILLED     -2

/*
 * Structures
 */

#ifdef HAVE_INTTYPES_H
# ifndef INCLUDED_inttypes_h
#  include <inttypes.h>
#  define INCLUDED_inttypes_h
# endif
#else
# ifdef HAVE_STDINT_H
#  ifndef INCLUDED_stdint_h
#   include <stdint.h>
#   define INCLUDED_stdint_h
#  endif
# endif
#endif

/** Structure used to count many server-wide statistics. */
struct ServerStatistics {
  unsigned int is_cl;           /**< number of client connections */
  unsigned int is_sv;           /**< number of server connections */
  unsigned int is_ni;           /**< connection but no idea who it was */
  uint64_t is_cbs;              /**< bytes sent to clients */
  uint64_t is_cbr;              /**< bytes received to clients */
  uint64_t is_sbs;              /**< bytes sent to servers */
  uint64_t is_sbr;              /**< bytes received to servers */
  uint64_t is_cti;              /**< time spent connected by clients */
  uint64_t is_sti;              /**< time spent connected by servers */
  unsigned int is_ac;           /**< connections accepted */
  unsigned int is_ref;          /**< accepts refused */
  unsigned int is_unco;         /**< unknown commands */
  unsigned int is_wrdi;         /**< command going in wrong direction */
  unsigned int is_unpf;         /**< unknown prefix */
  unsigned int is_empt;         /**< empty message */
  unsigned int is_num;          /**< numeric message */
  unsigned int is_kill;         /**< number of kills generated on collisions */
  unsigned int is_fake;         /**< MODE 'fakes' */
  unsigned int is_asuc;         /**< successful auth requests */
  unsigned int is_abad;         /**< bad auth requests */
  unsigned int is_loc;          /**< local connections made */
  unsigned int uping_recv;      /**< UDP Pings received */
};

/*
 * Prototypes
 */

extern int check_registered(struct Client *sptr);
extern int check_registered_user(struct Client *sptr);
extern int exit_client(struct Client *cptr, struct Client *bcptr,
    struct Client *sptr, const char *comment);
extern char *myctime(time_t value);
extern int exit_client_msg(struct Client *cptr, struct Client *bcptr,
                           struct Client *sptr, const char *pattern, ...);
extern void initstats(void);
extern char *date(time_t clock);
extern int vexit_client_msg(struct Client *cptr, struct Client *bcptr,
    struct Client *sptr, const char *pattern, va_list vl);
extern void tstats(struct Client *cptr, const struct StatDesc *sd,
                   char *param);

extern struct ServerStatistics* ServerStats;

#endif /* INCLUDED_s_misc_h */
