/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/list.h
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
/** @file list.h
 * @brief Singly and doubly linked list manipulation interface.
 * @version $Id: list.h,v 1.9 2007-04-22 13:56:19 zolty Exp $
 */
#ifndef INCLUDED_list_h
#define INCLUDED_list_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>         /* time_t, size_t */
#define INCLUDED_sys_types_h
#endif

struct Client;
struct Connection;
struct Channel;
struct ConfItem;
struct Watch;

/*
 * Structures
 */

/** Node in a singly linked list. */
struct SLink {
  struct SLink *next; /**< Next element in list. */
  union {
    struct Client *cptr;    /**< List element as a client. */
    struct Channel *chptr;  /**< List element as a channel. */
    struct ConfItem *aconf; /**< List element as a configuration item. */
    struct Watch *wptr;     /**< List element as a watch. */
    char *cp;               /**< List element as a string. */
  } value;                  /**< Value of list element. */
  unsigned int flags;       /**< Modifier flags for list element. */
};

/** Node in a doubly linked list. */
struct DLink {
  struct DLink*  next;      /**< Next element in list. */
  struct DLink*  prev;      /**< Previous element in list. */
  union {
    struct Client*  cptr;   /**< List element as a client. */
    struct Channel* chptr;  /**< List element as a channel. */
    char*           ch;     /**< List element as a string. */
  } value;                  /**< Value of list element. */
};

/*
 * Proto types
 */

extern void free_link(struct SLink *lp);
extern struct SLink *make_link(void);
extern void init_list(int maxconn);
extern struct Client *make_client(struct Client *from, int status);
extern void free_connection(struct Connection *con);
extern void free_client(struct Client *cptr);
extern struct Server *make_server(struct Client *cptr);
extern void remove_client_from_list(struct Client *cptr);
extern void add_client_to_list(struct Client *cptr);
extern struct DLink *add_dlink(struct DLink **lpp, struct Client *cp);
extern void remove_dlink(struct DLink **lpp, struct DLink *lp);
extern struct ConfItem *make_conf(int type);
extern void send_listinfo(struct Client *cptr, char *name);

#endif /* INCLUDED_list_h */
