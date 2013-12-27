/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/listener.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1999 Thomas Helvey <tomh@inxpress.net>
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
 * @brief Interface and declarations for handling listening sockets.
 * @version $Id: listener.h,v 1.7 2007-11-11 21:53:05 zolty Exp $
 */
#ifndef INCLUDED_listener_h
#define INCLUDED_listener_h

#ifndef INCLUDED_ircd_defs_h
#include "ircd_defs.h"       /* HOSTLEN */
#endif
#ifndef INCLUDED_ircd_events_h
#include "ircd_events.h"
#endif
#ifndef INCLUDED_res_h
#include "res.h"
#endif
#ifndef INCLUDED_flagset_h
#include "flagset.h"
#endif
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>       /* size_t, broken BSD system headers */
#define INCLUDED_sys_types_h
#endif

struct Client;
struct StatDesc;

enum ListenerFlag {
  /** Port is currently accepting connections. */
  LISTEN_ACTIVE,
  /** Port is hidden from /STATS P output. */
  LISTEN_HIDDEN,
  /** Port accepts only server connections. */
  LISTEN_SERVER,
  /** Port is exempt from connection prohibitions. */
  LISTEN_EXEMPT,
  /** Port listens for IPv4 connections. */
  LISTEN_IPV4,
  /** Port listens for IPv6 connections. */
  LISTEN_IPV6,
#if defined(USE_SSL)
  /** Port listens for SSL connections. */
  LISTEN_SSL,
#endif
  /** Sentinel for counting listener flags. */
  LISTEN_LAST_FLAG
};

DECLARE_FLAGSET(ListenerFlags, LISTEN_LAST_FLAG);

/** Describes a single listening port. */
struct Listener {
  struct Listener* next;               /**< list node pointer */
  struct ListenerFlags flags;          /**< on-off flags for listener */
  int              fd_v4;              /**< file descriptor for IPv4 */
  int              fd_v6;              /**< file descriptor for IPv6 */
  int              ref_count;          /**< number of connection references */
  unsigned char    mask_bits;          /**< number of bits in mask address */
  int              index;              /**< index into poll array */
  time_t           last_accept;        /**< last time listener accepted */
  struct irc_sockaddr addr;            /**< virtual address and port */
  struct irc_in_addr mask;             /**< listener hostmask */
  struct Socket    socket_v4;          /**< describe IPv4 socket to event system */
  struct Socket    socket_v6;          /**< describe IPv6 socket to event system */
};

#define listener_server(LISTENER) FlagHas(&(LISTENER)->flags, LISTEN_SERVER)
#define listener_active(LISTENER) FlagHas(&(LISTENER)->flags, LISTEN_ACTIVE)
#define listener_exempt(LISTENER) FlagHas(&(LISTENER)->flags, LISTEN_EXEMPT)
#if defined(USE_SSL)
#define listener_ssl(LISTENER) FlagHas(&(LISTENER)->flags, LISTEN_SSL)
#endif

extern void        add_listener(int port, const char* vaddr_ip,
                                const char* mask,
                                const struct ListenerFlags *flags);
extern void        close_listeners(void);
extern void        count_listener_memory(int* count_out, size_t* size_out);
extern void        mark_listeners_closing(void);
extern void show_ports(struct Client* client, const struct StatDesc* sd,
                       char* param);
extern void        release_listener(struct Listener* listener);

#endif /* INCLUDED_listener_h */
