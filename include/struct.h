/*
 * IRC - Internet Relay Chat, include/struct.h
 * Copyright (C) 1990 Jarkko Oikarinen and
 *                    University of Oulu, Computing Center
 * Copyright (C) 1996 -1997 Carlo Wood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
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

#if !defined(STRUCT_H)
#define STRUCT_H

#include <netinet/in.h>         /* Needed for struct in_addr */
#include "whowas.h"             /* Needed for whowas struct */

#if !defined(INCLUDED_dbuf_h)
#include "dbuf.h"
#endif

#include "ircd_defs.h"
#include "event.h"
#include "res.h"

/*=============================================================================
 * General defines
 */

#define MAXLEN         490
#define QUITLEN        300      /* Hispano extension */
#define COOKIELEN      16       /* Hispano extension */
#define COOKIECRYPTLEN 44       /* Hispano extension */

/*-----------------------------------------------------------------------------
 * Macro's
 */

#define COOKIE_PLAIN     0x02 /* Cookie enviada sin encriptar */
#define COOKIE_ENCRYPTED 0x04 /* Cookie enviada encriptada */
#define COOKIE_VERIFIED  0x08 /* Cookie verificada correctamente */

#define IsCookiePlain(x)     ((x)->cli_connect->cookie_status & COOKIE_PLAIN)
#define IsCookieEncrypted(x) ((x)->cli_connect->cookie_status & COOKIE_ENCRYPTED)
#define IsCookieVerified(x)  ((x)->cli_connect->cookie_status & COOKIE_VERIFIED)

#define SetCookiePlain(x)     ((x)->cli_connect->cookie_status |= COOKIE_PLAIN)
#define SetCookieEncrypted(x) ((x)->cli_connect->cookie_status |= COOKIE_ENCRYPTED)
#define SetCookieVerified(x)  ((x)->cli_connect->cookie_status |= COOKIE_VERIFIED)

/*=============================================================================
 * Structures
 *
 * Only put structures here that are being used in a very large number of
 * source files. Other structures go in the header file of there corresponding
 * source file, or in the source file itself (when only used in that file).
 */

#if defined(ZLIB_ESNET)
#include "zlib.h"

#define ZLIB_ESNET_IN   0x1
#define ZLIB_ESNET_OUT  0x2
#define ZLIB_ESNET_OUT_SPECULATIVE     0x4
#endif

#if defined(ESNET_NEG)
#define USER_TOK  0x8
#endif

struct Server {
  struct Server *nexts;
  struct Client *up;            /* Server one closer to me */
  struct DLink *down;          /* List with downlink servers */
  struct DLink *updown;        /* own Dlink in up->serv->down struct */
  aClient **client_list;        /* List with client pointers on this server */
  struct User *user;            /* who activated this connection */
  struct ConfItem *cline;       /* C-line pointer for this server */
  time_t timestamp;             /* Remotely determined connect try time */
  time_t ghost;                 /* Local time at which a new server
                                   caused a Ghost */

  int lag;                      /* Approximation of the amount of lag to this server */
  unsigned int clients;         /* Number of clients on the server */

  unsigned short prot;          /* Major protocol */
  unsigned short nn_last;       /* Last numeric nick for p9 servers only */
  unsigned int nn_mask;         /* [Remote] FD_SETSIZE - 1 */
  char nn_capacity[4];          /* numeric representation of server capacity */
  unsigned long esnet_db;       /* Mascara de grifo abierto para cada BDD */
#if defined(LIST_DEBUG)
  struct Client *bcptr;
#endif
  char *last_error_msg;         /* Allocated memory with last message receive with an ERROR */
  char *by;
};

struct User {
  struct User *nextu;
  struct Client *server;        /* client structure of server */
  struct SLink *channel;        /* chain of channel pointer blocks */
  struct SLink *silence;        /* chain of silence pointer blocks */
#if defined(WATCH)
  struct SLink *watch;          /* Cadena de punteros a lista aWatch */
  int cwatch;                   /* Contador de entradas de lista WATCH */
#endif                          /* WATCH */
  char *away;                   /* pointer to away message */
  time_t last;
  unsigned int refcnt;          /* Number of times this block is referenced */
  unsigned int joined;          /* number of channels joined */
  char *username;
  char *host;
#if defined(BDD_VIP)
  char *virtualhost;
#endif
#if defined(LIST_DEBUG)
  struct Client *bcptr;
#endif
};

#define PunteroACadena(x)      ((x) ? (x) : "")

#endif /* STRUCT_H */
