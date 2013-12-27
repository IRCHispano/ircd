/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/umkpasswd.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2002 hikari
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
 * $Id: umkpasswd.h,v 1.3 2007-04-19 22:53:47 zolty Exp $
 *
 */
#ifndef INCLUDED_umkpasswd_h
#define INCLUDED_umkpasswd_h

struct umkpasswd_conf_s {
 int debuglevel;	/* you really need me to comment this? */
 char* mech;		/* mechanism we want to use */
 char* conf;		/* conf file, otherwise DPATH/CPATH */
 int flags;		/* to add or not to add (or maybe to update) */
 char* user;		/* username */
 int operclass;		/* connection class to use */
};

typedef struct umkpasswd_conf_s umkpasswd_conf_t;

/* values for flags */
#define ACT_ADDOPER    0x1
#define ACT_UPDOPER    0x2
#define ACT_ADDSERV    0x4 /* not implemented yet */
#define ACT_UPDSRRV    0x8 /* not implemented yet */

const char* default_salts = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

#endif /* INCLUDED_umkpasswd_h */

