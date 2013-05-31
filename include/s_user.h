/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/s_user.h
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
/** @file s_user.h
 * @brief Miscellaneous user-related helper functions.
 * @version $Id: s_user.h,v 1.13 2007-12-11 23:38:23 zolty Exp $
 */
#ifndef INCLUDED_s_user_h
#define INCLUDED_s_user_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif

struct Client;
struct User;
struct Channel;
struct MsgBuf;
struct Flags;

/*
 * Macros
 */

/**
 * Nick flood limit.
 * Minimum time between nick changes.
 * (The first two changes are allowed quickly after another however).
 */
#define NICK_DELAY 30

/**
 * Target flood time.
 * Minimum time between target changes.
 * (MAXTARGETS are allowed simultaneously however).
 * Its set to a power of 2 because we devide through it quite a lot.
 */
#define TARGET_DELAY 128

/* return values for hunt_server() */

#define HUNTED_NOSUCH   (-1)    /**< if the hunted server is not found */
#define HUNTED_ISME     0       /**< if this server should execute the command */
#define HUNTED_PASS     1       /**< if message passed onwards successfully */

/* send sets for send_umode() */
#define ALL_UMODES 0  /**< both local and global user modes */
#define SEND_UMODES 1  /**< global user modes only */
#define SEND_UMODES_BUT_OPER 2  /**< global user modes except for FLAG_OPER */

/* used when sending to #mask or $mask */

#define MATCH_SERVER  1 /**< flag for relay_masked_message (etc) to indicate the mask matches a server name */
#define MATCH_HOST    2 /**< flag for relay_masked_message (etc) to indicate the mask matches host name */

/* used for parsing user modes */
#define ALLOWMODES_ANY 0 /**< Allow any user mode */
#define ALLOWMODES_DEFAULT  1 /**< Only allow the subset of modes that are legit defaults */

/* used in set_nick_name */
#define NICK_EQUIVALENT 0x01    /** < Equivalent */
#define NICK_RENAMED    0x02    /** < Rename */
#if defined(DDB)
#define NICK_GHOST      0x04    /** < Ghost */
#define NICK_IDENTIFY   0x08    /** < Identify */
#endif /* defined(DDB) */

#define SetNickEquivalent(x)    ((x) |= NICK_EQUIVALENT)
#define IsNickEquivalent(x)     ((x) & NICK_EQUIVALENT)
#define SetRenamed(x)           ((x) |= NICK_RENAMED)
#define IsRenamed(x)            ((x) & NICK_RENAMED)
#if defined(DDB)
#define SetGhost(x)             ((x) |= NICK_GHOST)
#define IsGhost(x)              ((x) & NICK_GHOST)
#define SetIdentify(x)          ((x) |= NICK_IDENTIFY)
#define IsIdentify(x)           ((x) & NICK_IDENTIFY)
#endif /* defined(DDB) */

/** Formatter function for send_user_info().
 * @param who Client being displayed.
 * @param sptr Client requesting information.
 * @param buf Message buffer that should receive the response text.
 */
typedef void (*InfoFormatter)(struct Client* who, struct Client *sptr, struct MsgBuf* buf);

/*
 * Prototypes
 */
extern struct User* make_user(struct Client *cptr);
extern void         free_user(struct User *user);
extern int          register_user(struct Client* cptr, struct Client *sptr);

extern void         user_count_memory(size_t* count_out, size_t* bytes_out);

#if defined(DDB)
extern int verify_pass_nick(char *nick, char *cryptpass, char *pass);
#endif
extern int do_nick_name(char* nick);
extern char *get_random_nick(struct Client* cptr);
extern int set_nick_name(struct Client* cptr, struct Client* sptr,
                         const char* nick, int parc, char* parv[], int flags);
extern void send_umode_out(struct Client* cptr, struct Client* sptr,
                          struct Flags* old, int prop, int register);
extern int whisper(struct Client* source, const char* nick,
                   const char* channel, const char* text, int is_notice);
extern void send_user_info(struct Client* to, char* names, int rpl,
                           InfoFormatter fmt);

extern int hide_hostmask(struct Client *cptr, const char *vhost, unsigned int flags);
extern int set_user_mode(struct Client *cptr, struct Client *sptr,
                         int parc, char *parv[], int allow_modes);
extern int is_silenced(struct Client *sptr, struct Client *acptr);
extern int hunt_server_cmd(struct Client *from, const char *cmd,
			   const char *tok, struct Client *one,
			   int MustBeOper, const char *pattern, int server,
			   int parc, char *parv[]);
extern int hunt_server_prio_cmd(struct Client *from, const char *cmd,
				const char *tok, struct Client *one,
				int MustBeOper, const char *pattern,
				int server, int parc, char *parv[]);
extern struct Client* next_client(struct Client* next, const char* ch);
extern char *umode_str(struct Client *cptr);
extern void set_snomask(struct Client *, unsigned int, int);
extern int check_target_limit(struct Client *sptr, void *target, const char *name,
    int created);
extern void add_target(struct Client *sptr, void *target);

extern void init_isupport(void);
extern void add_isupport_i(const char *name, int value);
extern void add_isupport_s(const char *name, const char *value);
extern void del_isupport(const char *name);
extern int send_supported(struct Client *cptr);

#define NAMES_ALL 1 /**< List all users in channel */
#define NAMES_VIS 2 /**< List only visible users in non-secret channels */
#define NAMES_EON 4 /**< Add an 'End Of Names' reply to the end */
#define NAMES_DEL 8 /**< Show delayed joined users only */

void do_names(struct Client* sptr, struct Channel* chptr, int filter);

#endif /* INCLUDED_s_user_h */
