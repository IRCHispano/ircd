/*
 * IRC - Internet Relay Chat, include/numeric.h
 * Copyright (C) 1990 Jarkko Oikarinen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
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
 * $Id: $
 */

#if !defined(NUMERIC_H)
#define NUMERIC_H

/*=============================================================================
 * Macro's
 */

/*
 * Reserve numerics 000-099 for server-client connections where the client
 * is local to the server. If any server is passed a numeric in this range
 * from another server then it is remapped to 100-199. -avalon
 */
#define RPL_WELCOME	       1
#define RPL_YOURHOST	       2
#define RPL_CREATED	       3
#define RPL_MYINFO	       4
#define RPL_ISUPPORT           5  /* Undernet/Dalnet extension */
/*      RPL_BOUNCE             5          IRCnet extension */
/*      RPL_MAP                6          Unreal */
/*      RPL_MAPEND             7          Unreal */
#define RPL_SNOMASK	       8    /* Undernet extension */
#define RPL_STATMEMTOT	       9  /* Undernet extension */
#define RPL_STATMEM	      10    /* Undernet extension */
/*      RPL_REDIR             10          EFNet extension */
/*      RPL_YOURCOOKIE        14          IRCnet extension */
#define RPL_MAP               15  /* Undernet extension */
#define RPL_MAPMORE           16  /* Undernet extension */
#define RPL_MAPEND            17  /* Undernet extension */
/*     RPL_YOURID            42           IRCnet extension */
/*      RPL_ATTEMPTINGJUNC    50          IRCnet extension */
/*      RPL_ATTEMPTINGREROUTE 51          IRCnet extension */

/*
 * Errors are in the range from 400-599 currently and are grouped by what
 * commands they come from.
 */
/*      ERR_FIRSTERROR       400          unused */
#define ERR_NOSUCHNICK	     401
#define ERR_NOSUCHSERVER     402
#define ERR_NOSUCHCHANNEL    403
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_TOOMANYCHANNELS  405
#define ERR_WASNOSUCHNICK    406
#define ERR_TOOMANYTARGETS   407
/*      ERR_NOSUCHSERVICE    408          IRCnet extension */
/*      ERR_NOCOLORSONCHAN   408          Dalnet extension */
#define ERR_NOORIGIN	     409

#define ERR_NORECIPIENT	     411
#define ERR_NOTEXTTOSEND     412
#define ERR_NOTOPLEVEL	     413
#define ERR_WILDTOPLEVEL     414
/*      ERR_BADMASK          415          IRCnet extension */
#define ERR_QUERYTOOLONG     416  /* Undernet extension */
/*      ERR_TOOMANYMATCHES   416          IRCnet extension */

/*      ERR_LENGTHTRUNCATED  419          Aircd */

#define ERR_INVALIDBANMASK   420  /* Hispano extension */
#define ERR_UNKNOWNCOMMAND   421
#define ERR_NOMOTD	     422
#define ERR_NOADMININFO	     423
/*      ERR_FILEERROR        424           removed from RFC1459 */

/*      ERR_TOOMANYAWAY      429          Dalnet extension */

#define ERR_NONICKNAMEGIVEN  431
#define ERR_INVALIDNICKNAME  432
#define ERR_NICKNAMEINUSE    433
#define ERR_NICKREGISTERED   ERR_NICKNAMEINUSE
/*      ERR_SERVICENAMEINUSE 434          ? */
/*      ERR_NORULES         434           Unreal */
/*      ERR_SERVICECONFUSED  435          ? */
/*      ERR_BANONCHAN        435          Dalnet */
#define ERR_NICKCOLLISION    436
#define ERR_BANNICKCHANGE    437  /* Undernet extension */
/*      ERR_UNAVAILRESOURCE  437          IRCnet extension */
#define ERR_NICKTOOFAST	     438  /* Undernet extension */
/*      ERR_DEAD             438          IRCnet reserved for later use */
#define ERR_TARGETTOOFAST    439  /* Undernet extension */
/*      ERR_SERVICESDOWN     440          Dalnet extension */
#define ERR_USERNOTINCHANNEL 441
#define ERR_NOTONCHANNEL     442
#define ERR_USERONCHANNEL    443
/*      ERR_NOLOGIN          444           removed from RFC1459 */
/*      ERR_SUMMONDISABLED   445           removed from RFC1459 */
/*      ERR_USERSDISABLED    446           removed from RFC1459 */
/*     ERR_NONICKCHANGE     447           Unreal */

#define ERR_NOTREGISTERED    451
/*      ERR_IDCOLLISION      452           IRCnet extension */
/*      ERR_NICKLOST         453           IRCnet extension */

/*      ERR_HOSTILENAME             455           Unreal */

/*      ERR_NOHIDING        459           Unreal */
/*      ERR_NOTFORHALFOPS    460          Unreal */
#define ERR_NEEDMOREPARAMS   461
#define ERR_ALREADYREGISTRED 462
#define ERR_NOPERMFORHOST    463
#define ERR_PASSWDMISMATCH   464
#define ERR_YOUREBANNEDCREEP 465
#define ERR_YOUWILLBEBANNED  466
#define ERR_KEYSET	     467    /* Undernet extension */
#define ERR_INVALIDUSERNAME  468  /* Undernet extension */
/*      ERR_ONLYSERVERSCANCHANGE 468      Dalnet extension */
/*      ERR_LINKSET         469           Dalnet extension */
/*      ERR_LINKCHANNEL             470           Unreal */
/*      ERR_KICKEDFROMCHAN   470          Aircd */
#define ERR_CHANNELISFULL    471
#define ERR_UNKNOWNMODE	     472
#define ERR_INVITEONLYCHAN   473
#define ERR_BANNEDFROMCHAN   474
#define ERR_BADCHANNELKEY    475
#define ERR_BADCHANMASK	     476  /* Undernet extension */
#define ERR_NEEDREGGEDNICK   477  /* DalNet & Undernet Extention */
#define ERR_BANLISTFULL	     478  /* Undernet extension */
#define ERR_BADCHANNAME      479  /* EFNet extension */
               /* 479 Undernet extension badchan */
/*      ERR_LINKFAIL        479           Unreal */
/*      ERR_CANNOTKNOCK             480           Unreal */
/*      ERR_NOULINE         480           Austnet */
#define ERR_NOPRIVILEGES     481
#define ERR_CHANOPRIVSNEEDED 482
#define ERR_CANTKILLSERVER   483
#define ERR_ISCHANSERVICE    484  /* Undernet extension */
/*      ERR_DESYNC          484           Dalnet extension */
/*      ERR_ATTACKDENY      484           Unreal */
/*      ERR_RESTRICTED      484           IRCnet extension */
/*      ERR_UNIQOPRIVSNEEDED 485          IRCnet extension */
/*      ERR_KILLDENY         485          Unreal */
/*      ERR_CANTKICKADMIN    485          PTlink */
#define ERR_NONONREG         486  /* Dalnet extension */
/*      ERR_HTMDISABLED      486          Unreal */
/*      ERR_CHANTOORECENT    487           IRCnet extension */
/*      ERR_TSLESSCHAN       488           IRCnet extension */
#define ERR_VOICENEEDED	     489  /* Undernet extension */

#define ERR_NOOPERHOST	     491
#define ERR_OPERCLASSFULL    492  /* Hispano extension */
/*      ERR_NOSERVICEHOST    492          IRCnet extension */
/*      ERR_NOFEATURE        493          Undernet extension */
#define ERR_NOOPERCLASS      493  /* Hispano extension */
/*      ERR_BADFEATVALUE     494          Undernet extension */
/*      ERR_BADLOGTYPE      495           Undernet extension */
/*      ERR_BADLOGSYS       496           Undernet extension */
/*      ERR_BADLOGVALUE             497           Undernet extension */
#define ERR_ISOPERLCHAN      498  /* Undernet extension */

#define ERR_UMODEUNKNOWNFLAG 501
#define ERR_USERSDONTMATCH   502
/*      ERR_GHOSTEDCLIENT    503          EFNet extension */
/*      ERR_VWORLDWARN      503           Austnet */

#define ERR_SILECANTBESHOWN  509  /* Hispano extension */
#define ERR_ISSILENCING      510  /* Hispano extension */
#define ERR_SILELISTFULL     511  /* Undernet extension */
/*      ERR_NOSUCHGLINE      512          Undernet extension */
#if defined(WATCH)
#define ERR_TOOMANYWATCH     512  /* Dalnet extension */
#endif /* WATCH */
/*      ERR_NOTIFYFULL       512          Aircd */
#define ERR_BADPING	     513    /* Undernet extension */
/*      ERR_NEEDPONG        513           Dalnet extension */
/*      ERR_NOSUCHJUPE       514          Undernet extension */
#define ERR_NOSUCHGLINE      514  /* Hispano extension */
/*      ERR_TOOMANYDCC      514           Dalnet extension */
/*      ERR_BADEXPIRE        515          Undernet extension */
/*      ERR_DONTCHEAT       516           Undernet extension */
/*      ERR_DISABLED        517           Undernet extension */
/*      ERR_LONGMASK        518           Undernet extension */
/*      ERR_NOINVITE        518           Unreal */
/*      ERR_TOOMANYUSERS     519          Undernet extension */
/*      ERR_ADMONLY         519           Unreal */
/*      ERR_MASKTOOWIDE             520           Undernet extension */
/*      ERR_OPERONLY        520           Unreal */
/*      ERR_WHOTRUNC        520           Austnet */
/*      ERR_LISTSYNTAX       521          Dalnet extension */
/*     ERR_WHOSYNTAX        522           Dalnet extension */
/*     ERR_WHOLIMEXCEED     523           Dalnet extension */

/*      ERR_NOTLOWEROPLEVEL  550          Undernet extension */
/*      ERR_NOTMANAGER       551          Undernet extension */
/*      ERR_CHANSECURED      552          Undernet extension */
/*      ERR_UPASSSET         553          Undernet extension */
/*      ERR_UPASSNOTSET      554          Undernet extension */
/*     ERR_LASTERROR        555           Undernet extension */


/*
 * Numberic replies from server commands.
 * These are currently in the range 200-399.
 */
#define RPL_NONE            300 /* Unused */
#define RPL_AWAY	     301
#define RPL_USERHOST	     302
#define RPL_ISON	     303
#define RPL_TEXT            304 /* Unused */
#define RPL_UNAWAY	     305
#define RPL_NOWAWAY	     306
/*      RPL_NOTAWAY          306          Aircd */

#define RPL_WHOISREGNICK     307  /* Hispano extension */
/*      RPL_WHOISREGNICK     307          Dalnet extension */
/*      RPL_SUSERHOST       307           Austnet */
/*      RPL_WHOISADMIN      308           Dalnet extension */
/*      RPL_NOTIFYACTION     308          Aircd */
/*      RPL_RULESSTART      308           Unreal */
/*      RPL_WHOISSADMIN             309           Dalnet extension */
/*      RPL_NICKTRACE        309          Aircd */
/*      RPL_ENDOFRULES      309           Unreal */
/*      RPL_WHOISHELPER             309           Austnet */
#define RPL_WHOISHELPOP      310  /* Hispano extension */
/*      RPL_WHOISSVCMSG      310          Dalnet extension */
/*      RPL_WHOISHELPOP             310           Unreal */
/*      RPL_WHOISSERVICE     310          Austnet */
#define RPL_WHOISUSER	     311  /* See also RPL_ENDOFWHOIS */
#define RPL_WHOISSERVER	     312
#define RPL_WHOISOPERATOR    313
#define RPL_WHOWASUSER	     314  /* See also RPL_ENDOFWHOWAS */
#define RPL_ENDOFWHO	     315  /* See RPL_WHOREPLY/RPL_WHOSPCRPL */
#define RPL_WHOISBOT         316  /* Hispano extension */
#define RPL_WHOISIDLE	     317
#define RPL_ENDOFWHOIS	     318  /* See RPL_WHOISUSER/RPL_WHOISSERVER/
                                     RPL_WHOISOPERATOR/RPL_WHOISIDLE */
#define RPL_WHOISCHANNELS    319
/*      RPL_WHOIS_HIDDEN     320          Anothernet +h, ick! */
/*      RPL_WHOISSPECIAL     320          Unreal */
#define RPL_LISTSTART	     321
#define RPL_LIST	     322
#define RPL_LISTEND	     323
#define RPL_CHANNELMODEIS    324
/*      RPL_CHANNELPASSIS    325           IRCnet extension */
/*      RPL_NOCHANPASS       326           IRCnet extension */
/*      RPL_CHPASSUNKNOWN    327           IRCnet extension */
/*      RPL_CHANNEL_URL      328          Dalnet */
#define RPL_CREATIONTIME     329
/*      RPL_WHOWAS_TIME      330          ? */
#define RPL_NOTOPIC	     331
#define RPL_TOPIC	     332
#define RPL_TOPICWHOTIME     333  /* Undernet extension */
#define RPL_LISTUSAGE	     334  /* Undernet extension */

/*      RPL_WHOISACTUALLY    338          Undernet & Dalnet extension */
/*      RPL_CHANPASSOK       338           IRCnet extension */
/*      RPL_BADCHANPASS      339           IRCnet & Dalnet extension */
#define RPL_USERIP           340  /* Undernet extension */
#define RPL_INVITING	     341
#define RPL_MSGONLYREG       342  /* Hispano extension */

#define RPL_INVITELIST       346  /* IRCnet & Undernet extension */
#define RPL_ENDOFINVITELIST  347  /* IRCnet & Undernet extension */
/*      RPL_EXCEPTLIST       348           IRCnet extension */
/*      RPL_ENDOFEXCEPTLIST  349           IRCnet extension */

#define RPL_VERSION	     351
#define RPL_WHOREPLY	     352  /* See also RPL_ENDOFWHO */
#define RPL_NAMREPLY	     353  /* See also RPL_ENDOFNAMES */
#define RPL_WHOSPCRPL        354  /* Undernet extension,
                                     See also RPL_ENDOFWHO */

#define RPL_KILLDONE        361 /* Unused */
#define RPL_CLOSING	     362
#define RPL_CLOSEEND	     363
#define RPL_LINKS	     364
#define RPL_ENDOFLINKS	     365
#define RPL_ENDOFNAMES	     366  /* See RPL_NAMREPLY */
#define RPL_BANLIST	     367
#define RPL_ENDOFBANLIST     368
#define RPL_ENDOFWHOWAS	     369

#define RPL_INFO	     371
#define RPL_MOTD	     372
#define RPL_INFOSTART       373 /* Unused */
#define RPL_ENDOFINFO	     374
#define RPL_MOTDSTART	     375
#define RPL_ENDOFMOTD	     376
/*      RPL_KICKEXPIRED      377          Aircd */
/*      RPL_SPAM            377           Austnet */
#define RPL_WHOISHOST	     378  /* Hispano extension */
/*      RPL_BANEXPIRED       378          Aircd */
/*      RPL_KICKLINKED       379          Aircd */
#define RPL_WHOISMODES       379  /* Hispano extension */
/*      RPL_BANLINKED        380          Aircd */
#define RPL_YOUREOPER	     381
#define RPL_REHASHING	     382
/*      RPL_YOURSERVICE             383           Various */
#define RPL_MYPORTIS        384 /* Unused */
#define RPL_NOTOPERANYMORE   385  /* Extension to RFC1459, not used */
/*      RPL_QLIST           386           Unreal */
/*      RPL_ENDOFQLIST      387           Unreal */
/*      RPL_ALIST           388           Unreal */
/*      RPL_ENDOFALIST      389           Unreal */
#define RPL_WHOISSUSPNICK    390  /* Hispano extension */
#define RPL_TIME	     391
/*      RPL_START_USERS      392          Dalnet,EFNet,IRCnet */
/*      RPL_USERS            393          Dalnet,EFNet,IRCnet */
/*      RPL_END_USERS        394          Dalnet,EFNet,IRCnet */
/*      RPL_NOUSERS          395          Dalnet,EFNet,IRCnet */
#define RPL_HOSTHIDDEN       396  /* Undernet, IRC-Hispano extension */

#define RPL_TRACELINK	     200
#define RPL_TRACECONNECTING  201
#define RPL_TRACEHANDSHAKE   202
#define RPL_TRACEUNKNOWN     203
#define RPL_TRACEOPERATOR    204
#define RPL_TRACEUSER	     205
#define RPL_TRACESERVER	     206
#define RPL_TRACENEWTYPE     208
#define RPL_TRACECLASS	     209
/*      RPL_STATS            210          Aircd, used instead of having
                                          multiple stats numerics */
/*      RPL_TRACERECONNECT   210          IRCnet extension */
#define RPL_STATSLINKINFO    211
#define RPL_STATSCOMMANDS    212
#define RPL_STATSCLINE	     213
#define RPL_STATSNLINE      214 /* Unused */
/*     RPL_STATSOLDNLINE    214           Unreal */
#define RPL_STATSILINE	     215
#define RPL_STATSKLINE	     216
#define RPL_STATSPLINE	     217  /* Undernet extenstion */
/*      RPL_STATSQLINE       217          Various */
#define RPL_STATSYLINE	     218
#define RPL_ENDOFSTATS	     219  /* See also RPL_STATSDLINE */
/*      RPL_STATSPLINE       220          EFNet - Because 217 was for old Q: lines */
/*      RPL_STATSBLINE       220          Dalnet extension */
#define RPL_UMODEIS	     221
/*     RPL_SQLINE_NICK      222           Dalnet extension */
/*     RPL_STATSELINE       223           Dalnet extension */
/*     RPL_STATSGLINE       223           Unreal */
#define RPL_STATSELINE      223 /* EFNet extension. ELINES */
/*      RPL_STATSFLINE       224          Dalnet & EFNet extension */
/*     RPL_STATSTLINE       224           Unreal */
/*      RPL_STATSDLINE       225          EFNet extension */
/*     RPL_STATSZLINE       225           Dalnet extension */
/*      RPL_STATSELINE      225           Unreal */
/*      RPL_STATSCOUNT      226           Dalnet extension */
/*      RPL_STATSNLINE      226           Unreal */
/*      RPL_STATSGLINE      227           Dalnet extension */
/*      RPL_STATSVLINE      227           Unreal */

#define RPL_SERVICEINFO             231 /* Unused */
#define RPL_ENDOFSERVICES    232  /* Unused */
/*     RPL_RULES            232           Unreal */
#define RPL_SERVICE         233 /* Unused */
#define RPL_SERVLIST        234 /* Unused */
#define RPL_SERVLISTEND             235 /* Unused */
/*      RPL_STATSVERBOSE     236          Undernet extension */
/*      RPL_STATSENGINE      237          Undernet extension */
/*      RPL_STATSFLINE       238          Undernet extension */
/*      RPL_STATSIAUTH       239          IRCnet extension */
/*      RPL_STATSVLINE       240          IRCnet extension */
/*     RPL_STATSXLINE       240           Austnet */
#define RPL_STATSLLINE	     241
#define RPL_STATSUPTIME	     242
#define RPL_STATSOLINE	     243
#define RPL_STATSHLINE	     244
/*      RPL_STATSSLINE       245           Reserved / Dalnet, EFNet, IRCnet*/
#define RPL_STATSTLINE	     246  /* Undernet extension */
/*     RPL_STATSULINE       246           Dalnet extension */
/*     RPL_STATSSPING       246           IRCnet extension */
#define RPL_STATSGLINE	     247  /* Undernet extension */
/*     RPL_STATSBLINE       247           IRCnet extension */
/*      RPL_STATSXLINE       247          EFNet extension */
#define RPL_STATSULINE	     248  /* Undernet extension */
/*     RPL_STATSDEFINE      248           IRCnet extension */
#define RPL_STATSDEBUG	     249  /* Extension to RFC1459 */
#define RPL_STATSCONN	     250  /* Undernet extension */
/*     RPL_STATSDLINE       250           IRCnet extension */

#define RPL_LUSERCLIENT	     251
#define RPL_LUSEROP	     252
#define RPL_LUSERUNKNOWN     253
#define RPL_LUSERCHANNELS    254
#define RPL_LUSERME	     255
#define RPL_ADMINME	     256
#define RPL_ADMINLOC1	     257
#define RPL_ADMINLOC2	     258
#define RPL_ADMINEMAIL	     259

#define RPL_TRACELOG        261 /* Unused */
#define RPL_TRACEPING	     262  /* Extension to RFC1459 */
/*     RPL_TRACEEND         262           EFNet & IRCnet extension */
/*      RPL_LOAD_THROTTLED   263          EFNet extension */
/*     RPL_TRYAGAIN         263           IRCnet extension */
/*     RPL_LOAD2HI          263           Dalnet extension */

#define RPL_LOCALUSERS       265  /* Dal.Net: localusers - Hispano extension */
/*      RPL_CURRENT_LOCAL    265          Dalnet & EFNet extension */
#define RPL_GLOBALUSERS      266  /* Dal.Net: globalusers - Hispano extension */
/*      RPL_CURRENT_GLOBAL   266          Dalnet & EFNet extension */
/*      RPL_START_NETSTAT    267          Aircd */
/*      RPL_NETSTAT          268          Aircd */
/*      RPL_END_NETSTAT      269          Aircd */
/*      RPL_PRIVS            270          Undernet extension */
#define RPL_SILELIST	     271  /* Undernet extension */
#define RPL_ENDOFSILELIST    272  /* Undernet extension */
/*      RPL_NOTIFY           273          Aircd */
/*      RPL_STATSDELTA       274           IRCnet extension */
/*      RPL_STATSDELTA       274          Aircd */
#define RPL_STATSDLINE	     275  /* Undernet extension */

#define RPL_GLIST	     280      /* Undernet extension */
#define RPL_ENDOFGLIST	     281  /* Undernet extension */
/*      RPL_JUPELIST         282          Undernet extension */
/*      RPL_ENDOFJUPELIST    283          Undernet extension */
/*      RPL_FEATURE         284           Undernet extension */
/*      RPL_CHANINFO_HANDLE  285          Aircd */
/*      RPL_CHANINFO_USERS   286          Aircd */
/*      RPL_CHANINFO_CHOPS   287          Aircd */
/*      RPL_CHANINFO_VOICES  288          Aircd */
/*      RPL_CHANINFO_AWAY    289          Aircd */
/*      RPL_CHANINFO_OPERS   290          Aircd */
/*     RPL_HELPHDR          290           Dalnet extension */
/*      RPL_CHANINFO_BANNED  291          Aircd */
/*     RPL_HELPOP           291           Dalnet extension */
/*      RPL_CHANINFO_BANS    292          Aircd */
/*     RPL_HELPTLR          292           Dalnet extension */
/*      RPL_CHANINFO_INVITE  293          Aircd */
/*     RPL_HELPHLP          293           Dalnet extension */
/*      RPL_CHANINFO_INVITES 294          Aircd */
/*     RPL_HELPFWD          294           Dalnet extension */
/*      RPL_CHANINFO_KICK    295          Aircd */
/*     RPL_HELPIGN          295           Dalnet extension */
/*      RPL_CHANINFO_KICKS   296          Aircd */

/*      RPL_END_CHANINFO     299          Aircd */


#if defined(WATCH)
/*
 * Numericos para WATCH
 *
 * --zoltan
 */

#define RPL_LOGON            600  /* Dalnet extension */
#define RPL_LOGOFF           601  /* Dalnet extension */
#define RPL_WATCHOFF         602  /* Dalnet extension */
#define RPL_WATCHSTAT        603  /* Dalnet extension */
#define RPL_NOWON            604  /* Dalnet extension */
#define RPL_NOWOFF           605  /* Dalnet extension */
#define RPL_WATCHLIST        606  /* Dalnet extension */
#define RPL_ENDOFWATCHLIST   607  /* Dalnet extension */
#endif /* WATCH */


/*      RPL_MAPMORE          610          Unreal */

/*      RPL_MAPMORE          615          PTlink */

/*      RPL_DCCSTATUS        617          Dalnet extension */
/*      RPL_DCCLIST          618          Dalnet extension */
/*      RPL_ENDOFDCCLIST     619          Dalnet extension */
/*      RPL_DCCINFO          620          Dalnet extension */

/*      RPL_DUMPING         640           Unreal */
/*      RPL_DUMPRPL         641           Unreal */
/*     RPL_EODUMP           642           Unreal */


#endif /* NUMERIC_H */
