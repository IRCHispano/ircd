/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/parse.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
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
/** @file parse.h
 * @brief Declarations for parsing input from users and other servers.
 * @version $Id: parse.h,v 1.6 2007-04-19 22:53:47 zolty Exp $
 */
#ifndef INCLUDED_parse_h
#define INCLUDED_parse_h

struct Client;
struct s_map;

/*
 * Prototypes
 */

extern int parse_client(struct Client *cptr, char *buffer, char *bufend);
extern int parse_server(struct Client *cptr, char *buffer, char *bufend);
extern void initmsgtree(void);

extern int register_mapping(struct s_map *map);
extern int unregister_mapping(struct s_map *map);

#endif /* INCLUDED_parse_h */
