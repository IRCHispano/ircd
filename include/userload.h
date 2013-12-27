/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/userload.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1997 Carlo Wood
 * Copyright (C) 1993 Michael L. VanLoon (mlv) <michaelv@iastate.edu>
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
/** @file
 * @brief Userload tracking and statistics.
 * @version $Id: userload.h,v 1.5 2007-04-19 22:53:47 zolty Exp $
 */
#ifndef INCLUDED_userload_h
#define INCLUDED_userload_h

struct Client;
struct StatDesc;

/*
 * Structures
 */

/** Tracks load of various types of users. */
struct current_load_st {
  unsigned int client_count; /**< Count of locally connected clients. */
  unsigned int local_count; /**< This field is updated but apparently meaningless. */
  unsigned int conn_count; /**< Locally connected clients plus servers. */
};

/*
 * Proto types
 */

extern void update_load(void);
extern void calc_load(struct Client *sptr, const struct StatDesc *sd,
                      char *param);
extern void initload(void);

extern struct current_load_st current_load;

#endif /* INCLUDED_userload_h */
