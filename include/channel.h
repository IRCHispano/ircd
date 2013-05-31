/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/channel.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1996-1997 Carlo Wood
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
 * @brief Channel management and maintenance.
 * @version $Id: channel.h,v 1.21 2008-01-19 19:25:57 zolty Exp $
 */
#ifndef INCLUDED_channel_h
#define INCLUDED_channel_h

#ifndef INCLUDED_ircd_defs_h
#include "ircd_defs.h"        /* NICKLEN */
#endif
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif
#ifndef INCLUDED_res_h
#include "res.h"
#endif

struct SLink;
struct Client;

/*
 * General defines
 */

#define MAXMODEPARAMS   6   /**< Maximum number of mode parameters */
#define MODEBUFLEN      200 /**< Maximum length of a mode */

#define KEYLEN          23  /**< Maximum length of a key */
#define CHANNELLEN      64  /**< Maximum length of a channel */

#define MAXJOINARGS 15  /**< number of slots for join buffer */
#define STARTJOINLEN    10  /**< fuzzy numbers */
#define STARTCREATELEN  20

#define KICKLEN                160

/*
 * Macro's
 */

#define ChannelExists(n)        (0 != FindChannel(n))

#define CHFL_CHANOP             0x0001  /**< Channel operator */
#define CHFL_VOICE              0x0002  /**< the power to speak */
#define CHFL_ZOMBIE             0x0010  /**< Kicked from channel */
#define CHFL_BURST_JOINED       0x0100  /**< Just joined by net.junction */
#define CHFL_BANVALID           0x0800  /**< CHFL_BANNED bit is valid */
#define CHFL_BANNED             0x1000  /**< Channel member is banned */
#define CHFL_SILENCE_IPMASK     0x2000  /**< silence mask is a CIDR */
#if defined(UNDERNET)
#define CHFL_BURST_ALREADY_OPPED    0x04000
                    /**< In oob BURST, but was already
                     * joined and opped
                     */
#define CHFL_BURST_ALREADY_VOICED   0x08000
                    /**, In oob BURST, but was already
                     * joined and voiced
                     */
#define CHFL_CHANNEL_MANAGER    0x10000 /**< Set when creating channel or using
                     * Apass
                     */
#endif
#define CHFL_USER_PARTING       0x20000 /**< User is already parting that
                     * channel
                     */
#define CHFL_DELAYED            0x40000 /**< User's join message is delayed */

#if defined(DDB) || defined(SERVICES)
#define CHFL_OWNER      0x10000 /**< Channel owner */

#define CHFL_OVERLAP         (CHFL_OWNER | CHFL_CHANOP | CHFL_VOICE)
#define CHFL_BANVALIDMASK    (CHFL_BANVALID | CHFL_BANNED)
#define CHFL_VOICED_OR_OPPED (CHFL_OWNER | CHFL_CHANOP | CHFL_VOICE)
#else

#define CHFL_OVERLAP         (CHFL_CHANOP | CHFL_VOICE)
#define CHFL_BANVALIDMASK    (CHFL_BANVALID | CHFL_BANNED)
#define CHFL_VOICED_OR_OPPED (CHFL_CHANOP | CHFL_VOICE)
#endif

/* Channel Visibility macros */

#if defined(DDB) || defined(SERVICES)
#define MODE_OWNER  CHFL_OWNER  /**< +q Channel owner */
#endif
#define MODE_CHANOP     CHFL_CHANOP /**< +o Chanop */
#define MODE_VOICE      CHFL_VOICE  /**< +v Voice */
#define MODE_PRIVATE    0x0004      /**< +p Private */
#define MODE_SECRET     0x0008      /**< +s Secret */
#define MODE_MODERATED  0x0010      /**< +m Moderated */
#define MODE_TOPICLIMIT 0x0020      /**< +t Topic Limited */
#define MODE_INVITEONLY 0x0040      /**< +i Invite only */
#define MODE_NOPRIVMSGS 0x0080      /**< +n No Private Messages */
#define MODE_KEY        0x0100      /**< +k Keyed */
#define MODE_BAN        0x0200      /**< +b Ban */
#define MODE_LIMIT      0x0400      /**< +l Limit */
#define MODE_REGONLY    0x0800      /**< +R Only +r users may join */
#define MODE_DELJOINS   0x1000      /**< New join messages are delayed */
#define MODE_REGISTERED 0x2000    /**< Channel marked as registered */
#define MODE_NOCOLOUR   0x4000          /**< No mIRC/ANSI colors/bold */
#define MODE_NOCTCP     0x8000          /**< No channel CTCPs */
#define MODE_NONOTICE   0x10000         /**< No channel notices */
#define MODE_SAVE   0x20000     /**< save this mode-with-arg 'til
                     * later */
#define MODE_FREE   0x40000     /**< string needs to be passed to
                     * MyFree() */
#define MODE_BURSTADDED 0x80000     /**< channel was created by a BURST */
#if defined(UNDERNET)
#define MODE_UPASS  0x100000
#define MODE_APASS  0x200000
#endif
#define MODE_WASDELJOINS 0x400000   /**< Not DELJOINS, but some joins
                     * pending */
#define MODE_NOQUITPARTS 0x800000

#define MODE_MODERATENOREG 0x1000000    /**< +M Moderate unauthed users */

#if defined(UNDERNET)
/** mode flags which take another parameter (With PARAmeterS)
 */
#define MODE_WPARAS     (MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY|MODE_LIMIT|MODE_APASS|MODE_UPASS)
/** Available Channel modes */
#define infochanmodes feature_bool(FEAT_OPLEVELS) ? "AbiklmnopstUvrDRcCNuM" : "biklmnopstvrDRcCNuM"
/** Available Channel modes that take parameters */
#define infochanmodeswithparams feature_bool(FEAT_OPLEVELS) ? "AbkloUv" : "bklov"
#elif defined(DDB) || defined(SERVICES)
/** mode flags which take another parameter (With PARAmeterS)
 */
#define MODE_WPARAS     (MODE_OWNER|MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY|MODE_LIMIT)

/** Available Channel modes */
#define infochanmodes "biklmnopstvrRDqcCNuM"
/** Available Channel modes that take parameters */
#define infochanmodeswithparams "bklovq"
#else
/** mode flags which take another parameter (With PARAmeterS)
 */
#define MODE_WPARAS     (MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY|MODE_LIMIT)

/** Available Channel modes */
#define infochanmodes "biklmnopstvrDRcCNuM"
/** Available Channel modes that take parameters */
#define infochanmodeswithparams "bklov"
#endif


/**** VIEJOOOOS ****/
#define CHFL_DEOPPED		0x40  /* Is de-opped by a server */
#define CHFL_SERVOPOK		0x80  /* Server op allowed */

/* Channel Visibility macros */
#define MODE_SENDTS	0x0800      /* TS was 0 during a local user /join; send
                                 * temporary TS; can be removed when all 2.10 */
#define MODE_AUTOOP     0x8000
#define MODE_SECUREOP   0x10000
/**** FIN VIEJOS */


#define HoldChannel(x)          (!(x))
/** name invisible */
#define SecretChannel(x)        ((x) && ((x)->mode.mode & MODE_SECRET))
/** channel not shown but names are */
#define HiddenChannel(x)        ((x) && ((x)->mode.mode & MODE_PRIVATE))
/** channel visible */
#define ShowChannel(v,c)        (PubChannel(c) || find_channel_member((v),(c)))
#define PubChannel(x)           ((!x) || ((x)->mode.mode & \
                                    (MODE_PRIVATE | MODE_SECRET)) == 0)

#define IsGlobalChannel(name)   (*(name) == '#')
#define IsLocalChannel(name)    (*(name) == '&')
#define IsChannelName(name)     (IsGlobalChannel(name) || IsLocalChannel(name))

typedef enum ChannelGetType {
  CGT_NO_CREATE,
  CGT_CREATE
} ChannelGetType;

/* used in SetMode() in channel.c and m_umode() in s_msg.c */

#define MODE_NULL      0
#define MODE_ADD       0x40000000
#define MODE_DEL       0x20000000

/* used in ListingArgs.flags */

#define LISTARG_TOPICLIMITS     0x0001
#define LISTARG_SHOWSECRET      0x0002
#define LISTARG_NEGATEWILDCARD  0x0004
#define LISTARG_SHOWMODES       0x0008

/**
 * Maximum acceptable lag time in seconds: A channel younger than
 * this is not protected against hacking admins.
 * Mainly here to check if the TS clocks really sync (otherwise this
 * will start causing HACK notices.
 * This value must be the same on all servers.
 *
 * This value has been increased to 1 day in order to distinguish this
 * "normal" type of HACK wallops / desyncs, from possiblity still
 * existing bugs.
 */
#define TS_LAG_TIME 86400

extern const char* const PartFmt1;
extern const char* const PartFmt2;
extern const char* const PartFmt1serv;
extern const char* const PartFmt2serv;


/*
 * Structures
 */

/** Information about a client on one channel
 *
 * This structure forms a sparse matrix with users down the side, and
 * channels across the top.  This matrix holds all the information about
 * which users are on what channels, and what modes that user has on that
 * channel (if they are op'd, voice'd and cached information if they are
 * banned or not)
 */
struct Membership {
  struct Client*     user;      /**< The user */
  struct Channel*    channel;       /**< The channel */
  struct Membership* next_member;   /**< The next user on this channel */
  struct Membership* prev_member;   /**< The previous user on this channel*/
  struct Membership* next_channel;  /**< Next channel this user is on */
  struct Membership* prev_channel;  /**< Previous channel this user is on*/
  unsigned int       status;        /**< Flags for op'd, voice'd, etc */
  unsigned short     oplevel;       /**< Op level */
};

#define MAXOPLEVELDIGITS    3
#define MAXOPLEVEL          999

#define IsZombie(x)         ((x)->status & CHFL_ZOMBIE) /**< see \ref zombie */
#define IsBanned(x)         ((x)->status & CHFL_BANNED)
#define IsBanValid(x)       ((x)->status & CHFL_BANVALID)
#if defined(DDB) || defined(SERVICES)
#define IsChanOwner(x)      ((x)->status & CHFL_OWNER)
#endif
#define IsChanOp(x)         ((x)->status & CHFL_CHANOP)
#define OpLevel(x)          ((x)->oplevel)
#define HasVoice(x)         ((x)->status & CHFL_VOICE)
#define IsBurstJoined(x)    ((x)->status & CHFL_BURST_JOINED)
#define IsVoicedOrOpped(x)  ((x)->status & CHFL_VOICED_OR_OPPED)
#if defined(UNDERNET)
#define IsChannelManager(x) ((x)->status & CHFL_CHANNEL_MANAGER)
#endif
#define IsUserParting(x)    ((x)->status & CHFL_USER_PARTING)
#define IsDelayedJoin(x)    ((x)->status & CHFL_DELAYED)

#define SetBanned(x)        ((x)->status |= CHFL_BANNED)
#define SetBanValid(x)      ((x)->status |= CHFL_BANVALID)
#define SetBurstJoined(x)   ((x)->status |= CHFL_BURST_JOINED)
#define SetZombie(x)        ((x)->status |= CHFL_ZOMBIE)
#if defined(UNDERNET)
#define SetChannelManager(x) ((x)->status |= CHFL_CHANNEL_MANAGER)
#endif
#if defined(DDB) || defined(SERVICES)
#define SetChanOwner(x)     ((x)->status |= CHFL_OWNER)
#endif
#define SetOpLevel(x, v)    (void)((x)->oplevel = (v))
#define SetUserParting(x)   ((x)->status |= CHFL_USER_PARTING)
#define SetDelayedJoin(x)   ((x)->status |= CHFL_DELAYED)

#define ClearBanned(x)      ((x)->status &= ~CHFL_BANNED)
#define ClearBanValid(x)    ((x)->status &= ~CHFL_BANVALID)
#define ClearBurstJoined(x) ((x)->status &= ~CHFL_BURST_JOINED)
#define ClearDelayedJoin(x) ((x)->status &= ~CHFL_DELAYED)
#if defined(DDB) || defined(SERVICES)
#define ClearChanOwner(x)   ((x)->status &= ~CHFL_OWNER)
#endif

/** Mode information for a channel */
struct Mode {
  unsigned int mode;
  unsigned int limit;
  char key[KEYLEN + 1];
#if defined(UNDERNET)
  char upass[KEYLEN + 1];
  char apass[KEYLEN + 1];
#endif
};

#define BAN_IPMASK         0x0001  /**< ban mask is an IP-number mask */
#define BAN_OVERLAPPED     0x0002  /**< ban overlapped, need bounce */
#define BAN_BURSTED        0x0004  /**< Ban part of last BURST */
#define BAN_BURST_WIPEOUT  0x0008  /**< Ban will be wiped at EOB */
#define BAN_EXCEPTION      0x0010  /**< Ban is an exception */
#define BAN_DEL            0x4000  /**< Ban is being removed */
#define BAN_ADD            0x8000  /**< Ban is being added */

/** A single ban for a channel. */
struct Ban {
  struct Ban* next;           /**< next ban in the channel */
  struct irc_in_addr address; /**< address for BAN_IPMASK bans */
  time_t when;                /**< timestamp when ban was added */
  unsigned short flags;       /**< modifier flags for the ban */
  unsigned char nu_len;       /**< length of nick!user part of banstr */
  unsigned char addrbits;     /**< netmask length for BAN_IPMASK bans */
  char who[NICKLEN+1];        /**< name of client that set the ban */
  char banstr[NICKLEN+USERLEN+HOSTLEN+3];  /**< hostmask that the ban matches */
};

/** An invitation to a channel. */
struct Invite {
  struct Invite*     next_user;    /**< next invite to the user */
  struct Invite*     next_channel; /**< next invite to the channel */
  struct Client*     user;         /**< user being invited */
  struct Channel*    channel;      /**< channel to which invited */
  char inviter[NICKLEN+USERLEN+HOSTLEN+3]; /**< hostmask of inviter */
};

/** Information about a channel */
struct Channel {
  struct Channel*    next;  /**< next channel in the global channel list */
  struct Channel*    prev;  /**< previous channel */
  struct Channel*    hnext; /**< Next channel in the hash table */
  struct DestructEvent* destruct_event;
  time_t             creationtime; /**< Creation time of this channel */
  time_t             topic_time;   /**< Modification time of the topic */
  unsigned int       users;    /**< Number of clients on this channel */
  struct Membership* members;      /**< Pointer to the clients on this channel*/
  struct Invite*     invites;      /**< List of invites on this channel */
  struct Ban*        banlist;      /**< List of bans on this channel */
  struct Mode        mode;     /**< This channels mode */
#if defined(DDB)
  int                modos_obligatorios;
  int                modos_prohibidos;
#endif
#if defined(WEBCHAT)
  char numeric[4];
#endif
  char               topic[TOPICLEN + 1]; /**< Channels topic */
  char               topic_nick[NICKLEN + 1]; /**< Nick of the person who set
                        *  The topic
                        */
  char               chname[1];    /**< Dynamically allocated string of the
                     * channel name
                     */
};

/** Information about a /list in progress */
struct ListingArgs {
  time_t max_time;
  time_t min_time;
  unsigned int max_users;
  unsigned int min_users;
  unsigned int flags;
  time_t max_topic_time;
  time_t min_topic_time;
  unsigned int bucket;
  char wildcard[CHANNELLEN];
};

struct ModeBuf {
  unsigned int      mb_add;     /**< Modes to add */
  unsigned int      mb_rem;     /**< Modes to remove */
  struct Client        *mb_source;  /**< Source of MODE changes */
  struct Client        *mb_connect; /**< Connection of MODE changes */
  struct Channel       *mb_channel; /**< Channel they affect */
#if defined(DDB)
  char             *mb_botname; /**< Nick Bot of MODE changes */
#endif
  unsigned int      mb_dest;    /**< Destination of MODE changes */
  unsigned int      mb_count;   /**< Number of modes w/args */
  struct {
    unsigned int    mbm_type;   /**< Type of argument */
    union {
      unsigned int  mbma_uint;  /**< A limit */
      char         *mbma_string;    /**< A string */
      struct Client    *mbma_client;    /**< A client */
    }           mbm_arg;    /**< The mode argument */
#if defined(UNDERNET)
    unsigned short      mbm_oplevel;    /**< Oplevel for a bounce */
#endif
  }         mb_modeargs[MAXMODEPARAMS];
                    /**< A mode w/args */
};

#define MODEBUF_DEST_CHANNEL    0x00001 /**< Mode is flushed to channel */
#define MODEBUF_DEST_SERVER 0x00002 /**< Mode is flushed to server */

#define MODEBUF_DEST_OPMODE 0x00100 /**< Send server mode as OPMODE */
#define MODEBUF_DEST_DEOP   0x00200 /**< Deop the offender */
#define MODEBUF_DEST_BOUNCE 0x00400 /**< Bounce the modes */
#define MODEBUF_DEST_LOG    0x00800 /**< Log the mode changes to OPATH */

#define MODEBUF_DEST_HACK2  0x02000 /**< Send a HACK(2) notice, reverse */
#define MODEBUF_DEST_HACK3  0x04000 /**< Send a HACK(3) notice, TS == 0 */
#define MODEBUF_DEST_HACK4  0x08000 /**< Send a HACK(4) notice, TS == 0 */

#define MODEBUF_DEST_NOKEY  0x10000 /**< Don't send the real key */
#if defined(DDB)
#define MODEBUF_DEST_BOTMODE    0x20000 /**< Mode send by Bot */
#endif
#if 1 /* TRANSICION IRC-HISPANO */
#define MODEBUF_DEST_XMODE_JCEA 0x40000
#endif

#define MB_TYPE(mb, i)      ((mb)->mb_modeargs[(i)].mbm_type)
#define MB_UINT(mb, i)      ((mb)->mb_modeargs[(i)].mbm_arg.mbma_uint)
#define MB_STRING(mb, i)    ((mb)->mb_modeargs[(i)].mbm_arg.mbma_string)
#define MB_CLIENT(mb, i)    ((mb)->mb_modeargs[(i)].mbm_arg.mbma_client)
#if defined(UNDERNET)
#define MB_OPLEVEL(mb, i)       ((mb)->mb_modeargs[(i)].mbm_oplevel)
#endif

/** A buffer represeting a list of joins to send */
struct JoinBuf {
  struct Client        *jb_source;  /**< Source of joins (ie, joiner) */
  struct Client        *jb_connect; /**< Connection of joiner */
  unsigned int      jb_type;    /**< Type of join (JOIN or CREATE) */
  char             *jb_comment; /**< part comment */
  time_t        jb_create;  /**< Creation timestamp */
  unsigned int      jb_count;   /**< Number of channels */
  unsigned int      jb_strlen;  /**< length so far */
  struct Channel       *jb_channels[MAXJOINARGS];
                    /**< channels joined or whatever */
};

#define JOINBUF_TYPE_JOIN   0   /**< send JOINs */
#define JOINBUF_TYPE_CREATE 1   /**< send CREATEs */
#define JOINBUF_TYPE_PART   2   /**< send PARTs */
#define JOINBUF_TYPE_PARTALL    3   /**< send local PARTs, but not remote */

extern struct Channel* GlobalChannelList;
extern int             LocalChanOperMode;

/*
 * Proto types
 */
extern void channel_modes(struct Client *cptr, char *mbuf, char *pbuf,
                          int buflen, struct Channel *chptr,
              struct Membership *member);
extern int set_mode(struct Client* cptr, struct Client* sptr,
                    struct Channel* chptr, int parc, char* parv[],
                    char* mbuf, char* pbuf, char* npbuf, int* badop);
extern void send_hack_notice(struct Client *cptr, struct Client *sptr,
                             int parc, char *parv[], int badop, int mtype);
extern struct Channel *get_channel(struct Client *cptr,
                                   char *chname, ChannelGetType flag);
extern struct Membership* find_member_link(struct Channel * chptr,
                                           const struct Client* cptr);
extern int sub1_from_channel(struct Channel* chptr);
extern int destruct_channel(struct Channel* chptr);
extern void add_user_to_channel(struct Channel* chptr, struct Client* who,
                                unsigned int flags, int oplevel);
extern void make_zombie(struct Membership* member, struct Client* who,
                        struct Client* cptr, struct Client* sptr,
                        struct Channel* chptr);
extern struct Client* find_chasing(struct Client* sptr, const char* user, int* chasing);
int number_of_zombies(struct Channel *chptr);

extern const char* find_no_nickchange_channel(struct Client* cptr);
extern struct Membership* find_channel_member(struct Client* cptr, struct Channel* chptr);
extern int member_can_send_to_channel(struct Membership* member, int reveal);
extern int client_can_send_to_channel(struct Client *cptr, struct Channel *chptr, int reveal);
extern void remove_user_from_channel(struct Client *sptr, struct Channel *chptr);
extern void remove_user_from_all_channels(struct Client* cptr);

#if defined(DDB) || defined(SERVICES)
extern int is_chan_owner(struct Client *cptr, struct Channel *chptr);
#endif
extern int is_chan_op(struct Client *cptr, struct Channel *chptr);
extern void send_channel_modes(struct Client *cptr, struct Channel *chptr);
extern char *pretty_mask(char *mask);
extern struct Invite* is_invited(struct Client* cptr, struct Channel* chptr);
extern void add_invite(struct Client *cptr, struct Channel *chptr, struct Client *inviter);
extern void del_invite(struct Client *cptr, struct Channel *chptr);
extern void list_set_default(void); /* this belongs elsewhere! */
extern void check_spambot_warning(struct Client *cptr);

extern void modebuf_init(struct ModeBuf *mbuf, struct Client *source,
             struct Client *connect, struct Channel *chan,
             unsigned int dest);
extern void modebuf_mode(struct ModeBuf *mbuf, unsigned int mode);
extern void modebuf_mode_uint(struct ModeBuf *mbuf, unsigned int mode,
                  unsigned int uint);
extern void modebuf_mode_string(struct ModeBuf *mbuf, unsigned int mode,
                char *string, int free);
extern void modebuf_mode_client(struct ModeBuf *mbuf, unsigned int mode,
                struct Client *client, int oplevel);
extern int modebuf_flush(struct ModeBuf *mbuf);
extern void modebuf_extract(struct ModeBuf *mbuf, char *buf);

extern void mode_ban_invalidate(struct Channel *chan);
extern void mode_invite_clear(struct Channel *chan);

extern int mode_parse(struct ModeBuf *mbuf, struct Client *cptr,
              struct Client *sptr, struct Channel *chptr,
              int parc, char *parv[], unsigned int flags,
              struct Membership* member);

#define MODE_PARSE_SET      0x01    /**< actually set channel modes */
#define MODE_PARSE_STRICT   0x02    /**< +m +n +t style not supported */
#define MODE_PARSE_FORCE    0x04    /**< force the mode to be applied */
#define MODE_PARSE_BOUNCE   0x08    /**< we will be bouncing the modes */
#define MODE_PARSE_NOTOPER  0x10    /**< send "not chanop" to user */
#define MODE_PARSE_NOTMEMBER    0x20    /**< send "not member" to user */
#define MODE_PARSE_WIPEOUT  0x40    /**< wipe out +k and +l during burst */
#define MODE_PARSE_BURST    0x80    /**< be even more strict w/extra args */

extern void joinbuf_init(struct JoinBuf *jbuf, struct Client *source,
             struct Client *connect, unsigned int type,
             char *comment, time_t create);
extern void joinbuf_join(struct JoinBuf *jbuf, struct Channel *chan,
             unsigned int flags);
extern int joinbuf_flush(struct JoinBuf *jbuf);
extern struct Ban *make_ban(const char *banstr);
extern struct Ban *find_ban(struct Client *cptr, struct Ban *banlist);
extern int apply_ban(struct Ban **banlist, struct Ban *newban, int free);
extern void free_ban(struct Ban *ban);

/** VIEJOOOOS */
extern struct SLink *IsMember(struct Client *cptr, struct Channel *chptr);
extern void remove_user_from_channel(struct Client *sptr, struct Channel *chptr);
extern int is_chan_op(struct Client *cptr, struct Channel *chptr);
extern int is_zombie(struct Client *cptr, struct Channel *chptr);
extern int has_voice(struct Client *cptr, struct Channel *chptr);
extern void send_channel_modes(struct Client *cptr, struct Channel *chptr);
extern char *pretty_mask(char *mask);
extern void del_invite(struct Client *cptr, struct Channel *chptr);
extern void send_user_joins(struct Client *cptr, struct Client *user);

extern char *adapta_y_visualiza_canal_flags(struct Channel *chptr, int add, int del);
extern void mascara_canal_flags(char *modos, int *add, int *del);

#endif /* INCLUDED_channel_h */