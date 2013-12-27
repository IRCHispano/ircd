/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/hash.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1998 Andrea Cocito
 * Copyright (C) 1991 Darren Reed
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
 * @brief Hash table management APIs.
 * @version $Id: hash.h,v 1.9 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef INCLUDED_hash_h
#define INCLUDED_hash_h

struct Client;
struct Channel;
struct StatDesc;
struct Watch;

/*
 * general defines
 */

/** Size of client and channel hash tables.
 * Both must be of the same size.
 */
#define HASHSIZE                32768

/*
 * Structures
 */

/*
 * Macros for internal use
 */

/*
 * Externally visible pseudofunctions (macro interface to internal functions)
 */

/* Raw calls, expect a core if you pass a NULL or zero-length name */
/** Search for a channel by name. */
#define SeekChannel(name)       hSeekChannel((name))
/** Search for any client by name. */
#define SeekClient(name)        hSeekClient((name), ~0)
/** Search for a registered user by name. */
#define SeekUser(name)          hSeekClient((name), (STAT_USER))
/** Search for a server by name. */
#define SeekServer(name)        hSeekClient((name), (STAT_ME | STAT_SERVER))
/** Search for a watch by name. */
#define SeekWatch(name)         hSeekWatch((name))

/* Safer macros with sanity check on name, WARNING: these are _macros_,
   no side effects allowed on <name> ! */
/** Search for a channel by name. */
#define FindChannel(name)       (BadPtr((name)) ? 0 : SeekChannel(name))
/** Search for any client by name. */
#define FindClient(name)        (BadPtr((name)) ? 0 : SeekClient(name))
/** Search for a registered user by name. */
#define FindUser(name)          (BadPtr((name)) ? 0 : SeekUser(name))
/** Search for a server by name. */
#define FindServer(name)        (BadPtr((name)) ? 0 : SeekServer(name))
/** Search for a wach by name. */
#define FindWatch(name)         (BadPtr((name)) ? 0 : SeekWatch(name))

/*
 * Proto types
 */

extern void init_hash(void);    /* Call me on startup */
extern int hAddClient(struct Client *cptr);
extern int hAddChannel(struct Channel *chptr);
extern int hAddWatch(struct Watch *wptr);
extern int hRemClient(struct Client *cptr);
extern int hChangeClient(struct Client *cptr, const char *newname);
extern int hRemChannel(struct Channel *chptr);
extern int hRemWatch(struct Watch *wptr);
extern struct Client *hSeekClient(const char *name, int TMask);
extern struct Channel *hSeekChannel(const char *name);
extern struct Watch *hSeekWatch(const char *name);

extern int m_hash(struct Client *cptr, struct Client *sptr, int parc, char *parv[]);

#if defined(DDB)
extern int isNickJuped(const char *nick, char *reason);
#else
extern int isNickJuped(const char *nick);
extern int addNickJupes(const char *nicks);
extern void clearNickJupes(void);
#endif
extern void stats_nickjupes(struct Client* to, const struct StatDesc* sd,
			    char* param);
extern void list_next_channels(struct Client *cptr);

#if defined(DDB)
extern int ddb_hash_register(char *key, int hash_size);
#endif

#endif /* INCLUDED_hash_h */
