/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ircd_parser.y
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2001 by Andrew Miller, Diana Bruce and ircd-hybrid team
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
 * $Id: ircd_parser.y,v 1.30 2007-12-11 23:38:25 zolty Exp $
 *
 */
%{
#include "config.h"

#include "s_conf.h"
#include "class.h"
#include "client.h"
#include "crule.h"
#include "ircd_features.h"
#include "fileio.h"
#include "gline.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_chattr.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_snprintf.h"
#include "ircd_string.h"
#include "list.h"
#include "listener.h"
#include "match.h"
#include "motd.h"
#include "numeric.h"
#include "numnicks.h"
#include "opercmds.h"
#include "parse.h"
#include "res.h"
#include "s_auth.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_debug.h"
#include "s_misc.h"
#include "send.h"
#include "struct.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#define MAX_STRINGS 80 /* Maximum number of feature params. */
#define USE_IPV4 (1 << 16)
#define USE_IPV6 (1 << 17)

  extern struct LocalConf   localConf;
  extern struct DenyConf*   denyConfList;
  extern struct ExceptConf* exceptConfList;
  extern struct CRuleConf*  cruleConfList;
  extern struct ServerConf* serverConfList;
  extern struct s_map*      GlobalServiceMapList;
  extern struct qline*      GlobalQuarantineList;

  int yylex(void);
  void lexer_include(const char *filename);

  /* Now all the globals we need :/... */
  static int tping, tconn, maxlinks, sendq, port, invert, stringno, flags;
  static char *name, *pass, *host, *ip, *username, *origin, *hub_limit;
  struct SLink *hosts;
  static char *stringlist[MAX_STRINGS];
  struct ListenerFlags listen_flags;
  static struct ConnectionClass *c_class;
  static struct DenyConf *dconf;
  static struct ExceptConf *econf;
  static struct s_map *smap;
  static struct Privs privs;
  static struct Privs privs_dirty;

#define parse_error yyserror

enum ConfigBlock
{
  BLOCK_ADMIN,
  BLOCK_CLASS,
  BLOCK_CLIENT,
  BLOCK_CONNECT,
  BLOCK_CRULE,
  BLOCK_EXCEPT,
  BLOCK_FEATURES,
  BLOCK_GENERAL,
  BLOCK_IAUTH,
  BLOCK_INCLUDE,
  BLOCK_JUPE,
  BLOCK_KILL,
  BLOCK_MOTD,
  BLOCK_OPER,
  BLOCK_PORT,
  BLOCK_PSEUDO,
  BLOCK_QUARANTINE,
  BLOCK_UWORLD,
  BLOCK_LAST_BLOCK
};

struct ConfigBlocks
{
  struct ConfigBlocks *cb_parent;
  unsigned long cb_allowed;
  char cb_fname[1];
};

static struct ConfigBlocks *includes;

static int
permitted(enum ConfigBlock type, int warn)
{
  static const char *block_names[BLOCK_LAST_BLOCK] = {
    "Admin", "Class", "Client", "Connect", "CRule", "Exception", "Features",
    "General", "IAuth", "Include", "Jupe", "Kill", "Motd",
    "Oper", "Port", "Pseudo", "Quarantine", "UWorld",
  };

  if (!includes)
    return 1;
  if (includes->cb_allowed & (1 << type))
    return 1;
  if (warn)
  {
    /* Unfortunately, flex's yylineno is hosed for included files, so
     * do not try to use it.
     */
    yywarning("Forbidden '%s' block at %s.", block_names[type],
              includes->cb_fname);
  }
  return 0;
}

static void free_slist(struct SLink **link) {
  struct SLink *next;
  while (*link != NULL) {
    next = (*link)->next;
    MyFree((*link)->value.cp);
    free_link(*link);
    *link = next;
  }
}

%}

%token <text> QSTRING
%token <num> NUMBER

%token GENERAL
%token ADMIN
%token LOCATION
%token CONTACT
%token CONNECT
%token CLASS
%token PINGFREQ
%token CONNECTFREQ
%token MAXLINKS
%token MAXHOPS
%token SENDQ
%token NAME
%token HOST
%token IP
%token USERNAME
%token PASS
%token LOCAL
%token SECONDS
%token MINUTES
%token HOURS
%token DAYS
%token WEEKS
%token MONTHS
%token YEARS
%token DECADES
%token BYTES
%token KBYTES
%token MBYTES
%token GBYTES
%token TBYTES
%token SERVER
%token PORT
%token MASK
%token HUB
%token LEAF
%token UWORLD
%token YES
%token NO
%token OPER
%token VHOST
%token HIDDEN
%token EXEMPT
%token SSLPORT
%token MOTD
%token JUPE
%token NICK
%token NUMERIC
%token DESCRIPTION
%token CLIENT
%token KILL
%token EXCEPT
%token CRULE
%token REAL
%token REASON
%token TFILE
%token RULE
%token ALL
%token FEATURES
%token QUARANTINE
%token PSEUDO
%token PREPEND
%token USERMODE
%token IAUTH
%token FAST
%token AUTOCONNECT
%token ZLIB
%token ZLIB_IN
%token ZLIB_OUT
%token PROGRAM
%token TOK_IPV4 TOK_IPV6
%token DNS
%token INCLUDE
%token LINESYNC
%token FROM
%token TEOF
%token LOGICAL_AND LOGICAL_OR
%token CONNECTED DIRECTCON VIA DIRECTOP
/* and now a lot of privileges... */
%token TPRIV_CHAN_LIMIT TPRIV_MODE_LCHAN TPRIV_DEOP_LCHAN TPRIV_WALK_LCHAN
%token TPRIV_LOCAL_KILL TPRIV_REHASH TPRIV_RESTART TPRIV_DIE
%token TPRIV_GLINE TPRIV_LOCAL_GLINE TPRIV_LOCAL_JUPE TPRIV_LOCAL_BADCHAN
%token TPRIV_LOCAL_OPMODE TPRIV_OPMODE TPRIV_SET TPRIV_WHOX TPRIV_BADCHAN
%token TPRIV_SEE_CHAN TPRIV_SHOW_INVIS TPRIV_SHOW_ALL_INVIS TPRIV_PROPAGATE
%token TPRIV_UNLIMIT_QUERY TPRIV_DISPLAY TPRIV_SEE_OPERS TPRIV_WIDE_GLINE
%token TPRIV_FORCE_OPMODE TPRIV_FORCE_LOCAL_OPMODE TPRIV_UMODE_K 
%token TPRIV_UMODE_X TPRIV_DBQ TPRIV_APASS_OPMODE 
%token TPRIV_LIST_CHAN 
/* and some types... */
%type <num> sizespec
%type <num> timespec timefactor factoredtimes factoredtime
%type <num> expr yesorno privtype address_family
%type <num> blocklimit blocktypes blocktype
%type <num> optall
%type <crule> crule_expr
%left LOGICAL_OR
%left LOGICAL_AND
%left '+' '-'
%left '*' '/'
%nonassoc '!'
%nonassoc '(' ')'

%union{
 struct CRuleNode *crule;
 char *text;
 int num;
}

%%
/* Blocks in the config file... */
blocks: blocks block | block;
block: adminblock | generalblock | classblock | connectblock |
       uworldblock | operblock | portblock | jupeblock | clientblock |
       killblock | exceptblock | cruleblock | motdblock | featuresblock | quarantineblock |
       pseudoblock | iauthblock | includeblock | error ';';

/* The timespec, sizespec and expr was ripped straight from
 * ircd-hybrid-7. */
timespec: expr | factoredtimes;

factoredtimes: factoredtimes factoredtime
{
  $$ = $1 + $2;
} | factoredtime;

factoredtime: expr timefactor
{
  $$ = $1 * $2;
};

timefactor: SECONDS { $$ = 1; }
| MINUTES { $$ = 60; }
| HOURS { $$ = 60 * 60; }
| DAYS { $$ = 60 * 60 * 24; }
| WEEKS { $$ = 60 * 60 * 24 * 7; }
| MONTHS { $$ = 60 * 60 * 24 * 7 * 4; }
| YEARS { $$ = 60 * 60 * 24 * 365; }
| DECADES { $$ = 60 * 60 * 24 * 365 * 10; };


sizespec:	expr	{
			$$ = $1;
		}
		| expr BYTES  { 
			$$ = $1;
		}
		| expr KBYTES {
			$$ = $1 * 1024;
		}
		| expr MBYTES {
			$$ = $1 * 1024 * 1024;
		}
		| expr GBYTES {
			$$ = $1 * 1024 * 1024 * 1024;
		}
		| expr TBYTES {
			$$ = $1 * 1024 * 1024 * 1024;
		}
		;

/* this is an arithmetic expression */
expr: NUMBER
		{ 
			$$ = $1;
		}
		| expr '+' expr { 
			$$ = $1 + $3;
		}
		| expr '-' expr { 
			$$ = $1 - $3;
		}
		| expr '*' expr { 
			$$ = $1 * $3;
		}
		| expr '/' expr { 
			$$ = $1 / $3;
		}
/* leave this out until we find why it makes BSD yacc dump core -larne
		| '-' expr  %prec NEG {
			$$ = -$2;
		} */
		| '(' expr ')' {
			$$ = $2;
		}
		;

jupeblock: JUPE '{' {
  (void)permitted(BLOCK_JUPE, 1);
} jupeitems '}' ';' ;
jupeitems: jupeitem jupeitems | jupeitem;
jupeitem: jupenick;
jupenick: NICK '=' QSTRING ';'
{
  if (permitted(BLOCK_JUPE, 0))
  {
    addNickJupes($3);
    MyFree($3);
  }
};

generalblock: GENERAL
{
  if (permitted(BLOCK_GENERAL, 1))
  {
    /* Zero out the vhost addresses, in case they were removed. */
    memset(&VirtualHost_v4.addr, 0, sizeof(VirtualHost_v4.addr));
    memset(&VirtualHost_v6.addr, 0, sizeof(VirtualHost_v6.addr));
  }
} '{' generalitems '}' ';' {
  if (localConf.name == NULL)
    parse_error("Your General block must contain a name.");
  if (localConf.numeric == 0)
    parse_error("Your General block must contain a numeric (between 1 and 4095).");
};
generalitems: generalitem generalitems | generalitem;
generalitem: generalnumeric | generalname | generalvhost | generaldesc
  | generaldnsvhost | generaldnsserver;

generalnumeric: NUMERIC '=' NUMBER ';'
{
  if (!permitted(BLOCK_GENERAL, 0))
    ;
  else if (localConf.numeric == 0)
    localConf.numeric = $3;
  else if (localConf.numeric != (unsigned int)$3)
    parse_error("Redefinition of server numeric %i (%i)", $3,
    		localConf.numeric);
};

generalname: NAME '=' QSTRING ';'
{
  if (!permitted(BLOCK_GENERAL, 0))
    MyFree($3);
  else if (localConf.name == NULL)
    localConf.name = $3;
  else
  {
    if (strcmp(localConf.name, $3))
      parse_error("Redefinition of server name %s (%s)", $3,
                  localConf.name);
    MyFree($3);
  }
};

generaldesc: DESCRIPTION '=' QSTRING ';'
{
  if (!permitted(BLOCK_GENERAL, 0))
    MyFree($3);
  else
  {
    MyFree(localConf.description);
    localConf.description = $3;
    ircd_strncpy(cli_info(&me), $3, REALLEN);
  }
};

generalvhost: VHOST '=' QSTRING ';'
{
  struct irc_in_addr addr;
  char *vhost = $3;

  if (!permitted(BLOCK_GENERAL, 0))
    ;
  else if (!strcmp(vhost, "*")) {
    /* This traditionally meant bind to all interfaces and connect
     * from the default. */
  } else if (!ircd_aton(&addr, vhost))
    parse_error("Invalid virtual host '%s'.", vhost);
  else if (irc_in_addr_is_ipv4(&addr))
    memcpy(&VirtualHost_v4.addr, &addr, sizeof(addr));
  else
    memcpy(&VirtualHost_v6.addr, &addr, sizeof(addr));
  MyFree(vhost);
};

generaldnsvhost: DNS VHOST '=' address_family QSTRING ';'
{
  struct irc_in_addr addr;
  int families = $4;
  char *vhost = $5;

  if (!strcmp(vhost, "*")) {
    /* Let the operating system assign the default. */
  } else if (!ircd_aton(&addr, vhost))
    parse_error("Invalid DNS virtual host '%s'.", vhost);
  else
  {
    if ((families & USE_IPV4)
        || (!families && irc_in_addr_is_ipv4(&addr)))
      memcpy(&VirtualHost_dns_v4.addr, &addr, sizeof(addr));
    if ((families & USE_IPV6)
        || (!families && !irc_in_addr_is_ipv4(&addr)))
      memcpy(&VirtualHost_dns_v6.addr, &addr, sizeof(addr));
  }
  MyFree(vhost);
};

generaldnsserver: DNS SERVER '=' QSTRING ';'
{
  char *server = $4;

  add_nameserver(server);
  MyFree(server);
};

adminblock: ADMIN
{
  if (permitted(BLOCK_ADMIN, 1))
  {
    MyFree(localConf.location1);
    MyFree(localConf.location2);
    MyFree(localConf.contact);
    localConf.location1 = localConf.location2 = localConf.contact = NULL;
  }
}
'{' adminitems '}' ';'
{
  if (localConf.location1 == NULL)
    DupString(localConf.location1, "");
  if (localConf.location2 == NULL)
    DupString(localConf.location2, "");
  if (localConf.contact == NULL)
    DupString(localConf.contact, "");
};
adminitems: adminitems adminitem | adminitem;
adminitem: adminlocation | admincontact;
adminlocation: LOCATION '=' QSTRING ';'
{
  if (!permitted(BLOCK_ADMIN, 0))
    MyFree($3);
  else if (localConf.location1 == NULL)
    localConf.location1 = $3;
  else if (localConf.location2 == NULL)
    localConf.location2 = $3;
  else /* Otherwise just drop it. -A1kmm */
    MyFree($3);
};
admincontact: CONTACT '=' QSTRING ';'
{
  if (!permitted(BLOCK_ADMIN, 0))
    MyFree($3);
  else
  {
    MyFree(localConf.contact);
    localConf.contact = $3;
  }
};

classblock: CLASS {
  tping = 90;
} '{' classitems '}' ';'
{
  if (!permitted(BLOCK_CLASS, 1))
    ;
  else if (name != NULL)
  {
    struct ConnectionClass *c_class;
    add_class(name, tping, tconn, maxlinks, sendq);
    c_class = find_class(name);
    MyFree(c_class->default_umode);
    c_class->default_umode = pass;
    memcpy(&c_class->privs, &privs, sizeof(c_class->privs));
    memcpy(&c_class->privs_dirty, &privs_dirty, sizeof(c_class->privs_dirty));
  }
  else {
   parse_error("Missing name in class block");
  }
  name = NULL;
  pass = NULL;
  tconn = 0;
  maxlinks = 0;
  sendq = 0;
  memset(&privs, 0, sizeof(privs));
  memset(&privs_dirty, 0, sizeof(privs_dirty));
};
classitems: classitem classitems | classitem;
classitem: classname | classpingfreq | classconnfreq | classmaxlinks |
           classsendq | classusermode | priv;
classname: NAME '=' QSTRING ';'
{
  MyFree(name);
  name = $3;
};
classpingfreq: PINGFREQ '=' timespec ';'
{
  tping = $3;
};
classconnfreq: CONNECTFREQ '=' timespec ';'
{
  tconn = $3;
};
classmaxlinks: MAXLINKS '=' expr ';'
{
  maxlinks = $3;
};
classsendq: SENDQ '=' sizespec ';'
{
  sendq = $3;
};
classusermode: USERMODE '=' QSTRING ';'
{
  MyFree(pass);
  pass = $3;
};

connectblock: CONNECT
{
 flags = CONF_AUTOCONNECT;
} '{' connectitems '}' ';'
{
 struct ConfItem *aconf = NULL;
 if (!permitted(BLOCK_CONNECT, 1))
   ;
 else if (name == NULL)
  parse_error("Missing name in connect block");
 else if (pass == NULL)
  parse_error("Missing password in connect block");
 else if (strlen(pass) > PASSWDLEN)
  parse_error("Password too long in connect block");
 else if (host == NULL)
  parse_error("Missing host in connect block");
 else if (strchr(host, '*') || strchr(host, '?'))
  parse_error("Invalid host '%s' in connect block", host);
 else if (c_class == NULL)
  parse_error("Missing or non-existent class in connect block");
 else {
   aconf = make_conf(CONF_SERVER);
   aconf->name = name;
   aconf->origin_name = origin;
   aconf->passwd = pass;
   aconf->conn_class = c_class;
   aconf->address.port = port;
   aconf->host = host;
   /* If the user specified a hub allowance, but not maximum links,
    * allow an effectively unlimited number of hops.
    */
   aconf->maximum = (hub_limit != NULL && maxlinks == 0) ? 65535 : maxlinks;
   aconf->hub_limit = hub_limit;
   aconf->flags = flags;
   lookup_confhost(aconf);
 }
 if (!aconf) {
   MyFree(name);
   MyFree(pass);
   MyFree(host);
   MyFree(origin);
   MyFree(hub_limit);
 }
 name = pass = host = origin = hub_limit = NULL;
 c_class = NULL;
 port = flags = maxlinks = 0;
};
connectitems: connectitem connectitems | connectitem;
connectitem: connectname | connectpass | connectclass | connecthost
              | connectport | connectvhost | connectleaf | connecthub
              | connecthublimit | connectmaxhops | connectauto
              | connectzlibin | connectzlibout | connectzlib;
connectname: NAME '=' QSTRING ';'
{
 MyFree(name);
 name = $3;
};
connectpass: PASS '=' QSTRING ';'
{
 MyFree(pass);
 pass = $3;
};
connectclass: CLASS '=' QSTRING ';'
{
 c_class = find_class($3);
 if (!c_class)
  parse_error("No such connection class '%s' for Connect block", $3);
 MyFree($3);
};
connecthost: HOST '=' QSTRING ';'
{
 MyFree(host);
 host = $3;
};
connectport: PORT '=' NUMBER ';'
{
 port = $3;
};
connectvhost: VHOST '=' QSTRING ';'
{
 MyFree(origin);
 origin = $3;
};
connectleaf: LEAF ';'
{
 maxlinks = 0;
};
connecthub: HUB ';'
{
 MyFree(hub_limit);
 DupString(hub_limit, "*");
};
connecthublimit: HUB '=' QSTRING ';'
{
 MyFree(hub_limit);
 hub_limit = $3;
};
connectmaxhops: MAXHOPS '=' expr ';'
{
  maxlinks = $3;
};
connectauto: AUTOCONNECT '=' YES ';' { flags |= CONF_AUTOCONNECT; }
 | AUTOCONNECT '=' NO ';' { flags &= ~CONF_AUTOCONNECT; };
connectzlibin: ZLIB_IN '=' YES ';' 
{
#if defined(USE_ZLIB)
 flags |= CONF_ZLIB_IN;
} | ZLIB_IN '=' NO ';' 
{ flags &= ~CONF_ZLIB_IN; 
#endif
};
connectzlibout: ZLIB_OUT '=' YES ';'
{
#if defined(USE_ZLIB)
 flags |= CONF_ZLIB_OUT;
} | ZLIB_OUT '=' NO ';'
{ flags &= ~CONF_ZLIB_OUT;
#endif
};
connectzlib: ZLIB '=' YES ';'
{
#if defined(USE_ZLIB)
 flags |= CONF_ZLIB;
} | ZLIB '=' NO ';'
{ flags &= ~CONF_ZLIB;
#endif
};

uworldblock: UWORLD '{' {
  (void)permitted(BLOCK_UWORLD, 1);
}  uworlditems '}' ';';
uworlditems: uworlditem uworlditems | uworlditem;
uworlditem: uworldname;
uworldname: NAME '=' QSTRING ';'
{
  if (permitted(BLOCK_UWORLD, 0))
    conf_make_uworld($3);
};

uworldblock: UWORLD QSTRING ';'
{
  if (permitted(BLOCK_UWORLD, 1))
    conf_make_uworld($2);
}

operblock: OPER '{' operitems '}' ';'
{
  struct ConfItem *aconf = NULL;
  struct SLink *link;

  if (!permitted(BLOCK_OPER, 1))
    ;
  else if (name == NULL)
    parse_error("Missing name in operator block");
  else if (pass == NULL)
    parse_error("Missing password in operator block");
  /* Do not check password length because it may be crypted. */
  else if (hosts == NULL)
    parse_error("Missing host(s) in operator block");
  else if (c_class == NULL)
    parse_error("Invalid or missing class in operator block");
  else if (!FlagHas(&privs_dirty, PRIV_PROPAGATE)
           && !FlagHas(&c_class->privs_dirty, PRIV_PROPAGATE))
    parse_error("Operator block for %s and class %s have no LOCAL setting", name, c_class->cc_name);
  else for (link = hosts; link != NULL; link = link->next) {
    aconf = make_conf(CONF_OPERATOR);
    DupString(aconf->name, name);
    DupString(aconf->passwd, pass);
    conf_parse_userhost(aconf, link->value.cp);
    aconf->conn_class = c_class;
    memcpy(&aconf->privs, &privs, sizeof(aconf->privs));
    memcpy(&aconf->privs_dirty, &privs_dirty, sizeof(aconf->privs_dirty));
  }
  MyFree(name);
  MyFree(pass);
  free_slist(&hosts);
  name = pass = NULL;
  c_class = NULL;
  memset(&privs, 0, sizeof(privs));
  memset(&privs_dirty, 0, sizeof(privs_dirty));
};
operitems: operitem | operitems operitem;
operitem: opername | operpass | operhost | operclass | priv;
opername: NAME '=' QSTRING ';'
{
  MyFree(name);
  name = $3;
};
operpass: PASS '=' QSTRING ';'
{
  MyFree(pass);
  pass = $3;
};
operhost: HOST '=' QSTRING ';'
{
 struct SLink *link;
 link = make_link();
 if (!strchr($3, '@'))
 {
   int uh_len;
   link->value.cp = (char*) MyMalloc((uh_len = strlen($3)+3));
   ircd_snprintf(0, link->value.cp, uh_len, "*@%s", $3);
 }
 else
   DupString(link->value.cp, $3);
 MyFree($3);
 link->next = hosts;
 hosts = link;
};
operclass: CLASS '=' QSTRING ';'
{
 c_class = find_class($3);
 if (!c_class)
  parse_error("No such connection class '%s' for Operator block", $3);
 MyFree($3);
};

priv: privtype '=' yesorno ';'
{
  FlagSet(&privs_dirty, $1);
  if (($3 == 1) ^ invert)
    FlagSet(&privs, $1);
  else
    FlagClr(&privs, $1);
  invert = 0;
};

privtype: TPRIV_CHAN_LIMIT { $$ = PRIV_CHAN_LIMIT; } |
          TPRIV_MODE_LCHAN { $$ = PRIV_MODE_LCHAN; } |
          TPRIV_DEOP_LCHAN { $$ = PRIV_DEOP_LCHAN; } |
          TPRIV_WALK_LCHAN { $$ = PRIV_WALK_LCHAN; } |
          KILL { $$ = PRIV_KILL; } |
          TPRIV_LOCAL_KILL { $$ = PRIV_LOCAL_KILL; } |
          TPRIV_REHASH { $$ = PRIV_REHASH; } |
          TPRIV_RESTART { $$ = PRIV_RESTART; } |
          TPRIV_DIE { $$ = PRIV_DIE; } |
          TPRIV_GLINE { $$ = PRIV_GLINE; } |
          TPRIV_LOCAL_GLINE { $$ = PRIV_LOCAL_GLINE; } |
          JUPE { $$ = PRIV_JUPE; } |
          TPRIV_LOCAL_JUPE { $$ = PRIV_LOCAL_JUPE; } |
          TPRIV_LOCAL_OPMODE { $$ = PRIV_LOCAL_OPMODE; } |
          TPRIV_OPMODE { $$ = PRIV_OPMODE; }|
          TPRIV_SET { $$ = PRIV_SET; } |
          TPRIV_WHOX { $$ = PRIV_WHOX; } |
          TPRIV_BADCHAN { $$ = PRIV_BADCHAN; } |
          TPRIV_LOCAL_BADCHAN { $$ = PRIV_LOCAL_BADCHAN; } |
          TPRIV_SEE_CHAN { $$ = PRIV_SEE_CHAN; } |
          TPRIV_SHOW_INVIS { $$ = PRIV_SHOW_INVIS; } |
          TPRIV_SHOW_ALL_INVIS { $$ = PRIV_SHOW_ALL_INVIS; } |
          TPRIV_PROPAGATE { $$ = PRIV_PROPAGATE; } |
          TPRIV_UNLIMIT_QUERY { $$ = PRIV_UNLIMIT_QUERY; } |
          TPRIV_DISPLAY { $$ = PRIV_DISPLAY; } |
          TPRIV_SEE_OPERS { $$ = PRIV_SEE_OPERS; } |
          TPRIV_WIDE_GLINE { $$ = PRIV_WIDE_GLINE; } |
          TPRIV_LIST_CHAN { $$ = PRIV_LIST_CHAN; } |
          LOCAL { $$ = PRIV_PROPAGATE; invert = 1; } |
          TPRIV_FORCE_OPMODE { $$ = PRIV_FORCE_OPMODE; } |
          TPRIV_FORCE_LOCAL_OPMODE { $$ = PRIV_FORCE_LOCAL_OPMODE; } |
          TPRIV_APASS_OPMODE { $$ = PRIV_APASS_OPMODE; }
          ;

yesorno: YES { $$ = 1; } | NO { $$ = 0; };

/* not a recursive definition because some pedant will just come along
 * and whine that the parser accepts "ipv4 ipv4 ipv4 ipv4"
 */
address_family:
               { $$ = 0; }
    | TOK_IPV4 { $$ = USE_IPV4; }
    | TOK_IPV6 { $$ = USE_IPV6; }
    | TOK_IPV4 TOK_IPV6 { $$ = USE_IPV4 | USE_IPV6; }
    | TOK_IPV6 TOK_IPV4 { $$ = USE_IPV6 | USE_IPV4; }
    ;

/* The port block... */
portblock: PORT '{' portitems '}' ';'
{
  struct ListenerFlags flags_here;
  struct SLink *link;
  if (hosts == NULL) {
    struct SLink *link;
    link = make_link();
    DupString(link->value.cp, "*");
    link->flags = 0;
    link->next = hosts;
    hosts = link;
  }
  if (!permitted(BLOCK_PORT, 1))
    ;
  else for (link = hosts; link != NULL; link = link->next) {
    memcpy(&flags_here, &listen_flags, sizeof(&flags_here));
    switch (link->flags & (USE_IPV4 | USE_IPV6)) {
    case USE_IPV4:
      FlagSet(&flags_here, LISTEN_IPV4);
      break;
    case USE_IPV6:
      FlagSet(&flags_here, LISTEN_IPV6);
      break;
    default: /* 0 or USE_IPV4|USE_IPV6 */
      FlagSet(&flags_here, LISTEN_IPV4);
      FlagSet(&flags_here, LISTEN_IPV6);
      break;
    }
    if (link->flags & 65535)
      port = link->flags & 65535;
    add_listener(port, link->value.cp, pass, &flags_here);
  }
  free_slist(&hosts);
  MyFree(pass);
  memset(&listen_flags, 0, sizeof(listen_flags));
  pass = NULL;
  port = 0;
};
portitems: portitem portitems | portitem;
portitem: portnumber | portvhost | portvhostnumber | portmask | portserver | porthidden | portexempt | portssl;
portnumber: PORT '=' address_family NUMBER ';'
{
  if ($4 < 1 || $4 > 65535) {
    parse_error("Port %d is out of range", port);
  } else {
    port = $3 | $4;
    if (hosts && (0 == (hosts->flags & 65535)))
      hosts->flags = (hosts->flags & ~65535) | port;
  }
};

portvhost: VHOST '=' address_family QSTRING ';'
{
  struct SLink *link;
  link = make_link();
  link->value.cp = $4;
  link->flags = $3 | port;
  link->next = hosts;
  hosts = link;
};

portvhostnumber: VHOST '=' address_family QSTRING NUMBER ';'
{
  if ($5 < 1 || $5 > 65535) {
    parse_error("Port %d is out of range", port);
  } else {
    struct SLink *link;
    link = make_link();
    link->value.cp = $4;
    link->flags = $3 | $5;
    link->next = hosts;
    hosts = link;
  }
};

portmask: MASK '=' QSTRING ';'
{
  MyFree(pass);
  pass = $3;
};

portserver: SERVER '=' YES ';'
{
  FlagSet(&listen_flags, LISTEN_SERVER);
} | SERVER '=' NO ';'
{
  FlagClr(&listen_flags, LISTEN_SERVER);
};

porthidden: HIDDEN '=' YES ';'
{
  FlagSet(&listen_flags, LISTEN_HIDDEN);
} | HIDDEN '=' NO ';'
{
  FlagClr(&listen_flags, LISTEN_HIDDEN);
};

portexempt: EXEMPT '=' YES ';'
{
  FlagSet(&listen_flags, LISTEN_EXEMPT);
} | EXEMPT '=' NO ';'
{
  FlagClr(&listen_flags, LISTEN_EXEMPT);
};

portssl: SSLPORT '=' YES ';'
{
#if defined(USE_SSL)
  FlagSet(&listen_flags, LISTEN_SSL);
} | SSLPORT '=' NO ';'
{
  FlagClr(&listen_flags, LISTEN_SSL);
#endif
};


clientblock: CLIENT
{
  maxlinks = 65535;
  port = 0;
}
'{' clientitems '}' ';'
{
  struct ConfItem *aconf = 0;
  struct irc_in_addr addr;
  unsigned char addrbits = 0;

  if (!permitted(BLOCK_CLIENT, 1))
    ;
  else if (!c_class)
    parse_error("Invalid or missing class in Client block");
  else if (pass && strlen(pass) > PASSWDLEN)
    parse_error("Password too long in connect block");
  else if (ip && !ipmask_parse(ip, &addr, &addrbits))
    parse_error("Invalid IP address %s in Client block", ip);
  else {
    aconf = make_conf(CONF_CLIENT);
    aconf->username = username;
    aconf->host = host;
    if (ip)
      memcpy(&aconf->address.addr, &addr, sizeof(aconf->address.addr));
    else
      memset(&aconf->address.addr, 0, sizeof(aconf->address.addr));
    aconf->address.port = port;
    aconf->addrbits = addrbits;
    aconf->name = ip;
    aconf->conn_class = c_class;
    aconf->maximum = maxlinks;
    aconf->passwd = pass;
  }
  if (!aconf) {
    MyFree(username);
    MyFree(host);
    MyFree(ip);
    MyFree(pass);
  }
  host = NULL;
  username = NULL;
  c_class = NULL;
  maxlinks = 0;
  ip = NULL;
  pass = NULL;
  port = 0;
};
clientitems: clientitem clientitems | clientitem;
clientitem: clienthost | clientip | clientusername | clientclass | clientpass | clientmaxlinks | clientport;
clienthost: HOST '=' QSTRING ';'
{
  char *sep = strchr($3, '@');
  MyFree(host);
  if (sep) {
    *sep++ = '\0';
    MyFree(username);
    DupString(host, sep);
    username = $3;
  } else {
    host = $3;
  }
};
clientip: IP '=' QSTRING ';'
{
  char *sep;
  sep = strchr($3, '@');
  MyFree(ip);
  if (sep) {
    *sep++ = '\0';
    MyFree(username);
    DupString(ip, sep);
    username = $3;
  } else {
    ip = $3;
  }
};
clientusername: USERNAME '=' QSTRING ';'
{
  MyFree(username);
  username = $3;
};
clientclass: CLASS '=' QSTRING ';'
{
  c_class = find_class($3);
  if (!c_class)
    parse_error("No such connection class '%s' for Client block", $3);
  MyFree($3);
};
clientpass: PASS '=' QSTRING ';'
{
  MyFree(pass);
  pass = $3;
};
clientmaxlinks: MAXLINKS '=' expr ';'
{
  maxlinks = $3;
};
clientport: PORT '=' expr ';'
{
  port = $3;
};

killblock: KILL
{
  dconf = (struct DenyConf*) MyCalloc(1, sizeof(*dconf));
} '{' killitems '}' ';'
{
  if (!permitted(BLOCK_KILL, 1))
  {
    MyFree(dconf->usermask);
    MyFree(dconf->hostmask);
    MyFree(dconf->realmask);
    MyFree(dconf->message);
    MyFree(dconf);
  }
  else if (dconf->usermask || dconf->hostmask ||dconf->realmask)
  {
    dconf->next = denyConfList;
    denyConfList = dconf;
  }
  else
  {
    MyFree(dconf->usermask);
    MyFree(dconf->hostmask);
    MyFree(dconf->realmask);
    MyFree(dconf->message);
    MyFree(dconf);
    parse_error("Kill block must match on at least one of username, host or realname");
  }
  dconf = NULL;
};
killitems: killitem killitems | killitem;
killitem: killuhost | killreal | killusername | killreasonfile | killreason;
killuhost: HOST '=' QSTRING ';'
{
  char *h;
  MyFree(dconf->hostmask);
  MyFree(dconf->usermask);
  if ((h = strchr($3, '@')) == NULL)
  {
    DupString(dconf->usermask, "*");
    dconf->hostmask = $3;
  }
  else
  {
    *h++ = '\0';
    DupString(dconf->hostmask, h);
    dconf->usermask = $3;
  }
  ipmask_parse(dconf->hostmask, &dconf->address, &dconf->bits);
};

killusername: USERNAME '=' QSTRING ';'
{
  MyFree(dconf->usermask);
  dconf->usermask = $3;
};

killreal: REAL '=' QSTRING ';'
{
 MyFree(dconf->realmask);
 dconf->realmask = $3;
};

killreason: REASON '=' QSTRING ';'
{
 dconf->flags &= ~DENY_FLAGS_FILE;
 MyFree(dconf->message);
 dconf->message = $3;
};

killreasonfile: TFILE '=' QSTRING ';'
{
 dconf->flags |= DENY_FLAGS_FILE;
 MyFree(dconf->message);
 dconf->message = $3;
};

exceptblock: EXCEPT
{
  econf = (struct ExceptConf*) MyCalloc(1, sizeof(*econf));
} '{' exceptitems '}' ';'
{
  if (!permitted(BLOCK_EXCEPT, 1))
  {
    MyFree(econf->usermask);
    MyFree(econf->hostmask);
    MyFree(econf->password);
    MyFree(econf);
  }
  else if (econf->usermask || econf->hostmask || econf->password)
  {
    econf->next = exceptConfList;
    exceptConfList = econf;
  }
  else
  {
    MyFree(econf->usermask);
    MyFree(econf->hostmask);
    MyFree(econf->password);
    MyFree(econf);
    parse_error("Eline block must match on at least one of username, host or password");
  }
  econf = NULL;
};
exceptitems: exceptitem exceptitems | exceptitem;
exceptitem: exceptuhost | exceptpassword | exceptusername | exceptport;
exceptuhost: HOST '=' QSTRING ';'
{
  char *h;
  MyFree(econf->hostmask);
  MyFree(econf->usermask);
  if ((h = strchr($3, '@')) == NULL)
  {
    DupString(econf->usermask, "*");
    econf->hostmask = $3;
  }
  else
  {
    *h++ = '\0';
    DupString(econf->hostmask, h);
    econf->usermask = $3;
  }
  ipmask_parse(econf->hostmask, &econf->address, &econf->bits);
};

exceptusername: USERNAME '=' QSTRING ';'
{
  MyFree(econf->usermask);
  econf->usermask = $3;
};

exceptpassword: PASS '=' QSTRING ';'
{
 MyFree(econf->password);
 econf->password = $3;
};
exceptport: PORT '=' NUMBER ';'
{
 econf->port = $3;
};

cruleblock: CRULE optall QSTRING optall crule_expr ';'
{
  if (permitted(BLOCK_CRULE, 1) && $5)
  {
    struct CRuleConf *p = (struct CRuleConf*) MyMalloc(sizeof(*p));
    p->hostmask = collapse($3);
    p->rule = crule_text($5);
    p->type = ($2 || $4) ? CRULE_ALL : CRULE_AUTO;
    p->node = $5;
    p->next = cruleConfList;
    cruleConfList = p;
  }
};

optall: { $$ = 0; };
  | ALL { $$ = 1; };

crule_expr:
    '(' crule_expr ')' { $$ = $2; }
  | crule_expr LOGICAL_AND crule_expr { $$ = crule_make_and($1, $3); }
  | crule_expr LOGICAL_OR crule_expr { $$ = crule_make_or($1, $3); }
  | '!' crule_expr { $$ = crule_make_not($2); }
  | CONNECTED '(' QSTRING ')' { $$ = crule_make_connected($3); }
  | DIRECTCON '(' QSTRING ')' { $$ = crule_make_directcon($3); }
  | VIA '(' QSTRING ',' QSTRING ')' { $$ = crule_make_via($3, $5); }
  | DIRECTOP '(' ')' { $$ = crule_make_directop(); }
  ;

motdblock: MOTD '{' motditems '}' ';'
{
  struct SLink *link;
  if (permitted(BLOCK_MOTD, 1) && pass != NULL) {
    for (link = hosts; link != NULL; link = link->next)
      motd_add(link->value.cp, pass);
  }
  free_slist(&hosts);
  MyFree(pass);
  pass = NULL;
};

motditems: motditem motditems | motditem;
motditem: motdhost | motdfile;
motdhost: HOST '=' QSTRING ';'
{
  struct SLink *link;
  link = make_link();
  link->value.cp = $3;
  link->next = hosts;
  hosts = link;
};

motdfile: TFILE '=' QSTRING ';'
{
  MyFree(pass);
  pass = $3;
};

featuresblock: FEATURES '{' {
  (void)permitted(BLOCK_FEATURES, 1);
} featureitems '}' ';';
featureitems: featureitems featureitem | featureitem;

featureitem: QSTRING
{
  stringlist[0] = $1;
  stringno = 1;
} '=' stringlist ';' {
  int ii;
  if (permitted(BLOCK_FEATURES, 0))
    feature_set(NULL, (const char * const *)stringlist, stringno);
  for (ii = 0; ii < stringno; ++ii)
    MyFree(stringlist[ii]);
};

stringlist: stringlist extrastring | extrastring;
extrastring: QSTRING
{
  if (stringno < MAX_STRINGS)
    stringlist[stringno++] = $1;
  else
    MyFree($1);
};

quarantineblock: QUARANTINE '{' {
  (void)permitted(BLOCK_QUARANTINE, 1);
} quarantineitems '}' ';';
quarantineitems: quarantineitems quarantineitem | quarantineitem;
quarantineitem: QSTRING '=' QSTRING ';'
{
  if (!permitted(BLOCK_QUARANTINE, 0))
  {
    MyFree($1);
    MyFree($3);
  }
  else
  {
    struct qline *qconf = MyCalloc(1, sizeof(*qconf));
    qconf->chname = $1;
    qconf->reason = $3;
    qconf->next = GlobalQuarantineList;
    GlobalQuarantineList = qconf;
  }
};

pseudoblock: PSEUDO QSTRING '{'
{
  smap = MyCalloc(1, sizeof(struct s_map));
  smap->command = $2;
}
pseudoitems '}' ';'
{
  int valid = 0;

  if (!permitted(BLOCK_PSEUDO, 1))
    ;
  else if (!smap->name)
    parse_error("Missing name in pseudo %s block", smap->command);
  else if (!smap->services)
    parse_error("Missing nick in pseudo %s block", smap->command);
  else if (!strIsAlpha(smap->command))
    parse_error("Pseudo command %s invalid: must all be letters", smap->command);
  else
    valid = 1;
  if (valid && register_mapping(smap))
  {
    smap->next = GlobalServiceMapList;
    GlobalServiceMapList = smap;
  }
  else
  {
    free_mapping(smap);
  }
  smap = NULL;
};

pseudoitems: pseudoitem pseudoitems | pseudoitem;
pseudoitem: pseudoname | pseudoprepend | pseudonick | pseudoflags;
pseudoname: NAME '=' QSTRING ';'
{
  MyFree(smap->name);
  smap->name = $3;
};
pseudoprepend: PREPEND '=' QSTRING ';'
{
  MyFree(smap->prepend);
  smap->prepend = $3;
};
pseudonick: NICK '=' QSTRING ';'
{
  char *sep = strchr($3, '@');

  if (sep != NULL) {
    size_t slen = strlen($3);
    struct nick_host *nh = MyMalloc(sizeof(*nh) + slen);
    memcpy(nh->nick, $3, slen + 1);
    nh->nicklen = sep - $3;
    nh->next = smap->services;
    smap->services = nh;
  }
  MyFree($3);
};
pseudoflags: FAST ';'
{
  smap->flags |= SMAP_FAST;
};

iauthblock: IAUTH '{' iauthitems '}' ';'
{
  if (permitted(BLOCK_IAUTH, 1))
    auth_spawn(stringno, stringlist);
  while (stringno > 0)
  {
    --stringno;
    MyFree(stringlist[stringno]);
  }
};

iauthitems: iauthitem iauthitems | iauthitem;
iauthitem: iauthprogram;
iauthprogram: PROGRAM '='
{
  while (stringno > 0)
  {
    --stringno;
    MyFree(stringlist[stringno]);
  }
} stringlist ';';

includeblock: INCLUDE blocklimit QSTRING ';' {
  struct ConfigBlocks *child;

  child = MyCalloc(1, sizeof(*child) + strlen($3));
  strcpy(child->cb_fname, $3);
  child->cb_allowed = $2 & (includes ? includes->cb_allowed : ~0ul);
  child->cb_parent = includes;
  MyFree($3);

  if (permitted(BLOCK_INCLUDE, 1))
    lexer_include(child->cb_fname);
  else
    lexer_include(NULL);

  includes = child;
} blocks TEOF {
  struct ConfigBlocks *parent;

  parent = includes->cb_parent;
  MyFree(includes);
  includes = parent;
};

blocklimit: { $$ = ~0; } ;
blocklimit: blocktypes FROM;
blocktypes: blocktypes ',' blocktype { $$ = $1 | $3; };
blocktypes: blocktype;
blocktype: ALL { $$ = ~0; }
  | ADMIN { $$ = 1 << BLOCK_ADMIN; }
  | CLASS { $$ = 1 << BLOCK_CLASS; }
  | CLIENT { $$ = 1 << BLOCK_CLIENT; }
  | CONNECT { $$ = 1 << BLOCK_CONNECT; }
  | CRULE { $$ = 1 << BLOCK_CRULE; }
  | EXCEPT { $$ = 1 << BLOCK_EXCEPT; }
  | FEATURES { $$ = 1 << BLOCK_FEATURES; }
  | GENERAL { $$ = 1 << BLOCK_GENERAL; }
  | IAUTH { $$ = 1 << BLOCK_IAUTH; }
  | INCLUDE { $$ = 1 << BLOCK_INCLUDE; }
  | JUPE { $$ = 1 << BLOCK_JUPE; }
  | KILL { $$ = 1 << BLOCK_KILL; }
  | MOTD { $$ = 1 << BLOCK_MOTD; }
  | OPER { $$ = 1 << BLOCK_OPER; }
  | PORT { $$ = 1 << BLOCK_PORT; }
  | PSEUDO { $$ = 1 << BLOCK_PSEUDO; }
  | QUARANTINE { $$ = 1 << BLOCK_QUARANTINE; }
  | UWORLD { $$ = 1 << BLOCK_UWORLD; }
  ;
