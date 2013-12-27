/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/s_debug.h
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
/* @file s_debug.h
 * @brief Debug APIs for the ircd.
 * @version $Id: s_debug.h,v 1.5 2007-04-19 22:53:47 zolty Exp $
 */
#ifndef INCLUDED_s_debug_h
#define INCLUDED_s_debug_h
#ifndef INCLUDED_config_h
#include "config.h"          /* Needed for DEBUGMODE */
#endif
#ifndef INCLUDED_ircd_defs_h
#include "ircd_defs.h"       /* Needed for HOSTLEN */
#endif
#ifndef INCLUDED_stdarg_h
#include <stdarg.h>
#define INCLUDED_stdarg_h
#endif

struct Client;
struct StatDesc;

#ifdef DEBUGMODE

/*
 * Macro's
 */

/** If DEBUGMODE is defined, output the debug message.
 * @param x A two-or-more element list containing level, format and arguments.
 */
#define Debug(x) debug x
#define LOGFILE LPATH /**< Path to debug log file. */

/*
 * defined debugging levels
 */
#define DEBUG_FATAL   0  /**< fatal error */
#define DEBUG_ERROR   1  /**< report_error() and other errors that are found */
#define DEBUG_NOTICE  3  /**< somewhat useful, but non-critical, messages */
#define DEBUG_DNS     4  /**< used by all DNS related routines - a *lot* */
#define DEBUG_INFO    5  /**< general useful info */
#define DEBUG_SEND    7  /**< everything that is sent out */
#define DEBUG_DEBUG   8  /**< everything that is received */
#define DEBUG_MALLOC  9  /**< malloc/free calls */
#define DEBUG_LIST   10  /**< debug list use */
#define DEBUG_ENGINE 11  /**< debug event engine; can dump gigabyte logs */

/*
 * proto types
 */

extern void vdebug(int level, const char *form, va_list vl);
extern void debug(int level, const char *form, ...);
extern void send_usage(struct Client *cptr, const struct StatDesc *sd,
                       char *param);

#else /* !DEBUGMODE */

#define Debug(x)
#define LOGFILE "/dev/null"

#endif /* !DEBUGMODE */

extern const char* debug_serveropts(void);
extern void debug_init(int use_tty);
extern void count_memory(struct Client *cptr, const struct StatDesc *sd,
                         char *param);

#endif /* INCLUDED_s_debug_h */
