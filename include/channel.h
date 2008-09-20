/*
 * IRC - Internet Relay Chat, ircd/channel.h
 * Copyright (C) 1990 Jarkko Oikarinen
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
 */

#include "list.h"

#if !defined(CHANNEL_H)
#define CHANNEL_H

/*=============================================================================
 * General defines
 */

#define MAXMODEPARAMS	6
#define MODEBUFLEN	200

#define KEYLEN		23
#define TOPICLEN	240
#define CHANNELLEN	64

#define MAXBANS		75
#define MAXBANLENGTH	2560      /* Este valor debe ajustarse si se cambia el anterior */

#define KICKLEN                160

/*=============================================================================
 * Macro's
 */

#define ChannelExists(n)	(FindChannel(n) != NullChn)
#define NullChn ((aChannel *)0)
#define CREATE 1                /* whether a channel should be
                                 * created or just tested for existance */

#define CHFL_CHANOP		0x0001    /* Channel operator */
#define CHFL_VOICE		0x0002    /* the power to speak */
#define CHFL_DEOPPED		0x0004  /* Is de-opped by a server */
#define CHFL_SERVOPOK		0x0008  /* Server op allowed */
#define CHFL_ZOMBIE		0x0010    /* Kicked from channel */
#define CHFL_BAN		0x0020      /* ban channel flag */
#define CHFL_BAN_IPMASK		0x0040  /* ban mask is an IP-number mask */
#define CHFL_BAN_OVERLAPPED	0x0080  /* ban overlapped, need bounce */
#define CHFL_OVERLAP	(CHFL_CHANOP|CHFL_VOICE)
#define CHFL_BURST_JOINED	0x0100  /* Just joined by net.junction */
#define CHFL_BURST_BAN		0x0200  /* Ban part of last BURST */
#define CHFL_BURST_BAN_WIPEOUT	0x0400  /* Ban will be wiped at end of BURST */
#define CHFL_BANVALID		0x0800  /* CHFL_BANNED bit is valid */
#define CHFL_BANNED		0x1000    /* Channel member is banned */
#define CHFL_SILENCE_IPMASK	0x2000  /* silence mask is an IP-number mask */
#define CHFL_DELAYED		0x4000  /* User's join message is delayed */

/* Channel Visibility macros */

#define MODE_CHANOP	CHFL_CHANOP
#define MODE_VOICE	CHFL_VOICE
#define MODE_PRIVATE	0x0004
#define MODE_SECRET	0x0008
#define MODE_MODERATED	0x0010
#define MODE_TOPICLIMIT 0x0020
#define MODE_INVITEONLY 0x0040
#define MODE_NOPRIVMSGS 0x0080
#define MODE_KEY	0x0100
#define MODE_BAN	0x0200
#define MODE_LIMIT	0x0400
#define MODE_SENDTS	0x0800      /* TS was 0 during a local user /join; send
                                 * temporary TS; can be removed when all 2.10 */
#define MODE_REGCHAN    0x2000
#define MODE_REGNICKS   0x4000
#define MODE_AUTOOP     0x8000
#define MODE_SECUREOP   0x10000
#define MODE_MSGNONREG  0x20000
#define MODE_NOCTCP     0x40000
#define MODE_NONOTICE   0x80000
#define MODE_NOQUITPARTS 0x100000
#define MODE_DELJOINS   0x200000
#define MODE_WASDELJOINS 0x400000
#define MODE_NOCOLOUR    0x800000


#define RegisteredChannel(x)    ((x) && ((x)->mode.mode & MODE_REGCHAN))
#define RestrictedChannel(x)    ((x) && ((x)->mode.mode & MODE_REGNICKS))
#define AutoOpChannel(x)        ((x) && ((x)->mode.mode & MODE_AUTOOP))
#define SecureOpChannel(x)      ((x) && ((x)->mode.mode & MODE_SECUREOP))
#define MsgOnlyRegChannel(x)   ((x) && ((x)->mode.mode & MODE_MSGNONREG))

extern int m_botmode(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/*
 * mode flags which take another parameter (With PARAmeterS)
 */
#define MODE_WPARAS	(MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY|MODE_LIMIT)

#define HoldChannel(x)		(!(x))
/* name invisible */
#define SecretChannel(x)	((x) && ((x)->mode.mode & MODE_SECRET))
/* channel not shown but names are */
#define HiddenChannel(x)	((x) && ((x)->mode.mode & MODE_PRIVATE))
/* channel visible */
#define ShowChannel(v,c)	(PubChannel(c) || IsMember((v),(c)))
#define PubChannel(x)		((!x) || ((x)->mode.mode & \
				    (MODE_PRIVATE | MODE_SECRET)) == 0)
#define is_listed(x)		((x)->mode.mode & MODE_LISTED)

#define IsGlobalChannel(name)		(*(name) == '#')
#define IsLocalChannel(name)		(*(name) == '&')
#define IsModelessChannel(name)                (*(name) == '+' && !IsBadModelessChannel(name))
#define IsModelessChannel2(name)	(IsModelessChannel(name) && ((!*(name+1)) || !IsChannelName2(name+1)))
#define IsBadModelessChannel(name)     (*(name+1) == '@')
#define IsChannelName2(name)		(IsGlobalChannel(name) || IsModelessChannel(name) || IsLocalChannel(name))
#define IsChannelName(name)		(IsGlobalChannel(name) || IsModelessChannel2(name) || IsLocalChannel(name))

/* Check if a sptr is an oper, and chptr is a local channel. */

#define IsOperOnLocalChannel(sptr,chname) ((IsAnOper(sptr)) \
                                          && (IsLocalChannel(chname)))


#if defined(OPER_WALK_THROUGH_LMODES)
  /* used in can_join to determine if an oper forced a join on a channel */
#define MAGIC_OPER_OVERRIDE 1000
#endif



/* used in SetMode() in channel.c and m_umode() in s_msg.c */

#define MODE_NULL      0
#define MODE_ADD       0x40000000
#define MODE_DEL       0x20000000


/* used in ListingArgs.flags */

#define LISTARG_TOPICLIMITS     0x0001
#define LISTARG_SHOWSECRET      0x0002
#define LISTARG_NEGATEWILDCARD  0x0004
#define LISTARG_SHOWMODES       0x0008

/*=============================================================================
 * Structures
 */

struct SMode {
  unsigned int mode;
  unsigned int limit;
  char *key;
};

struct Channel {
  struct Channel *nextch, *prevch, *hnextch;
  Mode mode;
  int modos_obligatorios, modos_prohibidos;
  time_t creationtime;
/*
** 'topic_nick' se almacena JUSTO A CONTINUACION
** del 'topic' propiamente dicho. Asi nos basta
** con un "malloc"/"free", en vez de dos.
*/
  char *topic;
  char *topic_nick;
  time_t topic_time;
  unsigned int users;
  struct SLink *members;
  struct SLink *invites;
  struct SLink *banlist;
  char chname[1];
};

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

/*=============================================================================
 * Proto types
 */

extern int m_names(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern Link *IsMember(aClient *cptr, aChannel *chptr);
extern void remove_user_from_channel(aClient *sptr, aChannel *chptr);
extern int is_chan_op(aClient *cptr, aChannel *chptr);
extern int is_zombie(aClient *cptr, aChannel *chptr);
extern int has_voice(aClient *cptr, aChannel *chptr);
extern int can_send(aClient *cptr, aChannel *chptr);
extern void send_channel_modes(aClient *cptr, aChannel *chptr);
extern int m_mode(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern char *pretty_mask(char *mask);
extern void del_invite(aClient *cptr, aChannel *chptr);
extern void sub1_from_channel(aChannel *chptr);
extern aChannel *get_channel(aClient *sptr, char *chname, int flag);
extern int m_join(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_create(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_destruct(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_burst(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_part(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_kick(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_topic(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_invite(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_list(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_svsjoin(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_svspart(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern void send_user_joins(aClient *cptr, aClient *user);

extern char *adapta_y_visualiza_canal_flags(aChannel *chptr, int add, int del);
extern void mascara_canal_flags(char *modos, int *add, int *del);
void channel_modes(aClient *cptr, char *mbuf, char *pbuf, aChannel *chptr);
extern aChannel *channel;

#endif /* CHANNEL_H */
