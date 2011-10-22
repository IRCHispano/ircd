/*
 * IRC - Internet Relay Chat, include/h.h
 * Copyright (C) 1996 - 1997 Carlo Wood
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
 *
 * $Id$
 */

#if !defined(INCLUDED_numnicks_h)
#define INCLUDED_numnicks_h
#if !defined(INCLUDED_sys_types_h)
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif

/*
 * General defines
 */
struct irc_in_addr;

/*
 * used for buffer size calculations in channel.c
 */
#define NUMNICKLEN 5            /* strlen("YYXXX") */

/*
 * Macros
 */

/*
 * Use this macro as follows: sprintf(buf, "%s%s ...", NumNick(cptr), ...);
 */
#define NumNick(c) (c)->user->server->yxx, (c)->yxx

/*
 * Use this macro as follows: sprintf(buf, "%s ...", NumServ(cptr), ...);
 */
#define NumServ(c) (c)->yxx

/*
 * Use this macro as follows: sprintf(buf, "%s%s ...", NumServCap(cptr), ...);
 */
#define NumServCap(c) (c)->yxx, (c)->serv->nn_capacity

/*
 * Structures
 */
struct Client;

/*
 * Proto types
 */
extern void SetRemoteNumNick(struct Client *cptr, const char *yxx);
extern int SetLocalNumNick(struct Client *cptr);
extern void RemoveYXXClient(struct Client *server, const char *yxx);
extern void SetServerYXX(struct Client *cptr,
    struct Client *server, const char *yxx);
extern void ClearServerYXX(const struct Client *server);

const char *CreateNNforProtocol9server(const struct Client *server);
extern void SetYXXCapacity(struct Client *myself, unsigned int max_clients);
extern void SetYXXServerName(struct Client *myself, unsigned int numeric);

extern int markMatchexServer(const char *cmask, int minlen);
extern struct Client *find_match_server(char *mask);
extern struct Client *findNUser(const char *yxx);
extern struct Client *FindNServer(const char *numeric);

extern unsigned int base64toint(const char *str);
extern const char *inttobase64(char *buf, unsigned int v, unsigned int count);
extern const char* iptobase64(char* buf, const struct irc_in_addr* addr, unsigned int count, int v6_ok);
extern void base64toip(const char* s, struct irc_in_addr* addr);
#ifdef ESNET_NEG
extern int SetXXXChannel(struct Channel *chptr);
extern void RemoveXXXChannel(const char *xxx);
#endif

#endif /* INCLUDED_numnicks_h */
