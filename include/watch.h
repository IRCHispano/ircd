/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/watch.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2002 Toni Garcia (zoltan) <zoltan@irc-dev.net>
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
 * $Id: watch.h,v 1.5 2007-04-19 22:53:47 zolty Exp $
 *
 */
#ifndef INCLUDED_watch_h
#define INCLUDED_watch_h

#ifndef INCLUDED_config_h
#include "config.h"
#endif
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif


struct Client;

/*
 * Structures
 */

struct Watch {
  struct Watch *wt_next;
  struct SLink *wt_watch;	/* Pointer to watch list */
  char         *wt_nick;	/* Nick */
  time_t        wt_lasttime;	/* last time status change */
};


/*
 * Macros
 */

#define wt_next(wt)		((wt)->wt_next)
#define wt_watch(wt)		((wt)->wt_watch)
#define wt_nick(wt)		((wt)->wt_nick)
#define wt_lasttime(wt)		((wt)->wt_lasttime)


/*
 * Proto types
 */
extern void check_status_watch(struct Client *sptr, int raw);
extern void show_status_watch(struct Client *sptr, char *nick, int raw1, int raw2);
extern int add_nick_watch(struct Client *sptr, char *nick);
extern int del_nick_watch(struct Client *sptr, char *nick);
extern int del_list_watch(struct Client *sptr);
extern void watch_count_memory(size_t* count_out, size_t* bytes_out);

#endif /* INCLUDED_watch_h */
