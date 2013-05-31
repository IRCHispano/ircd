/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/IPcheck.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1998 Carlo Wood <Run@undernet.org>
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
 * @brief Interface to count users connected from particular IP addresses.
 * @version $Id: IPcheck.h,v 1.6 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef INCLUDED_ipcheck_h
#define INCLUDED_ipcheck_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>          /* time_t, size_t */
#define INCLUDED_sys_types_h
#endif

struct Client;
struct irc_in_addr;

/*
 * Prototypes
 */
extern void IPcheck_init(void);
extern int IPcheck_local_connect(const struct irc_in_addr *ip, time_t *next_target_out);
extern void IPcheck_connect_fail(const struct Client *cptr);
extern void IPcheck_connect_succeeded(struct Client *cptr);
extern int IPcheck_remote_connect(struct Client *cptr, int is_burst);
extern void IPcheck_disconnect(struct Client *cptr);
extern unsigned short IPcheck_nr(struct Client* cptr);

#endif /* INCLUDED_ipcheck_h */
