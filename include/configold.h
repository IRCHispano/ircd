/*
 * Automatically generated C config: don't edit
 */
#define AUTOCONF_INCLUDED
#define CHANGE_CONFIG

/*
 * Compile stuff
 */
#define CC "gcc"
#define CFLAGS "-g -pipe"
#define cflags_O3_remark done
#define EXTRA_INCLUDEDIRS "none"
#define LDFLAGS "none"
#define IRCDLIBS "-lresolv -lrt -lcrypt"
#define BINDIR "/home/ircd/bin/ircd.hub"
#define SYMLINK "ircd.hub"
#define IRCDMODE "711"
#define IRCDOWN "ircd"
#define IRCDGRP "ircd"
#define MANDIR "/home/ircd/bin/ircd.hub"

/*
 * General defines
 */
#undef  CHROOTDIR
#undef  CONFIG_SETUGID
#define IRC_UID (1004)
#define IRC_GID (1004)
#undef  CMDLINE_CONFIG
#undef  UNIXPORT
#undef  VIRTUAL_HOST
#define HUB

/*
 * Debugging (do not define this on production servers)
 */
#define DEBUGMODE
#define DEBUGMALLOC
#define MEMMAGICNUMS
#define MEMLEAKSTATS
#define MEMSIZESTATS
#define MEMTIMESTATS
#undef  NODNS

/*
 * Paths and files
 */
#define CPATH "ircd.conf"
#define MPATH "ircd.motd"
#define RPATH "remote.motd"
#define PPATH "ircd.pid"

/*
 * Logging (filenames are either full paths or files within DPATH)
 */
#define CONFIG_LOG_WHOX
#define WPATH "whox.log"

/*
 * Bad Channel G-Lines allow operators to add channel masks to a list which prohibits local clients from being able joining channels which match the mask.  Remote BadChan Glines allow Uworld to add or remove channels from the servers internal list of badchans
 */
#define BADCHAN
#undef  LOCAL_BADCHAN
#define CONFIG_LOG_GLINES
#define GPATH "gline.log"
#define CONFIG_LOG_USERS
#define FNAME_USERLOG "users.log"
#define CONFIG_LOG_OPERS
#define FNAME_OPERLOG "opers.log"
#undef  USE_SYSLOG

/*
 * Configuration
 */
#define CRYPT_OPER_PASSWORD
#define BUFFERPOOL (27000000)
#define FERGUSON_FLUSHER
#define CLIENT_FLOOD (4096)
#define PORTNUM (6667)
#define NICKNAMEHISTORYLENGTH (800)
#define ALLOW_SNO_CONNEXIT
#define SNO_CONNEXIT_IP
#undef  R_LINES
#undef  USEONE
#define NODEFAULTMOTD

/*
 * Oper commands
 */
#define SHOW_INVISIBLE_USERS
#define SHOW_ALL_INVISIBLE_USERS
#define WHOX_HELPERS
#define OPERS_SEE_IN_SECRET_CHANNELS
#undef  LOCOP_SEE_IN_SECRET_CHANNELS
#undef  UNLIMIT_OPER_QUERY
#define OPER_KILL
#define OPER_REHASH
#define OPER_RESTART
#define OPER_DIE
#define OPER_LGLINE
#define OPER_REMOTE
#define OPER_JOIN_GOD_ESNET
#define OPER_CHANNEL_SERVICE_ESNET
#define CS_NO_FLOOD_ESNET
#define OPER_XMODE_ESNET
#define LOCOP_REHASH
#undef  LOCOP_RESTART
#undef  LOCOP_DIE
#define LOCOP_LGLINE
#define OPER_NO_CHAN_LIMIT
#define OPER_MODE_LCHAN
#undef  OPER_WALK_THROUGH_LMODES
#undef  NO_OPER_DEOP_LCHAN

/*
 * Server characteristics
 */
#define CONFIG_LIST
#define DEFAULT_LIST "T<10"
#define DEFAULT_LIST_PARAM "T<10"
#define COMMENT_IS_FILE
#define IDLE_FROM_MSG
#undef  CHECK_TS_LINKS

/*
 * Mandatory defines (you should leave these untouched)
 */
#define DBPATH "database"
#undef  BDD_MMAP
#define BDD_CLONES
#define BDD_CHAN_HACK
#define BDD_OPER_HACK
#define BDD_OPER_HACK_KMODE
#define BDD_VIP
#undef  BDD_VIP3
#define BDD_VIP2
#define XMODE_ESNET
#define ESNET_NEG
#define ZLIB_ESNET
#define MAXIMUM_LINKS (1)
#undef  MSGLOG_ENABLED
#undef  LOCAL_KILL_ONLY
#define KILLCHASETIMELIMIT (30)
#define MAXCHANNELSPERUSER (20)
#define MAXSILES (25)
#define WATCH
#define MAXWATCH (256)
#define AVBANLEN (40)
#define MAXSILELENGTH (40 * MAXSILES)
#define TIMESEC (60)
#define PINGFREQUENCY (120)
#define CONNECTFREQUENCY (600)
#define HANGONGOODLINK (300)
#define HANGONRETRYDELAY (10)
#define CONNECTTIMEOUT (90)
#define DEFAULTMAXSENDQLENGTH (40000)
