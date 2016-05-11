/*
 * IRC - Internet Relay Chat, ircd/list.c
 * Copyright (C) 1990 Jarkko Oikarinen and
 *                    University of Oulu, Finland
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
 */

#include "sys.h"
#include "h.h"
#include "s_debug.h"
#include "struct.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "class.h"
#include "match.h"
#include "ircd.h"
#include "s_serv.h"
#include "support.h"
#include "s_misc.h"
#include "s_bsd.h"
#include "whowas.h"
#include "res.h"
#include "common.h"
#include "list.h"
#include "s_user.h"
#include "opercmds.h"
#include "m_watch.h"
#include "hash.h"
#include "slab_alloc.h"


#include <assert.h>

RCSTAG_CC("$Id$");

#if defined(DEBUGMODE)
static struct liststats {
  int inuse;
} cloc, crem, users, servs, links, classs, aconfs, watchs
   ;
#endif

void outofmemory();

#if defined(DEBUGMODE)
void initlists(void)
{
  memset(&cloc, 0, sizeof(cloc));
  memset(&crem, 0, sizeof(crem));
  memset(&users, 0, sizeof(users));
  memset(&servs, 0, sizeof(servs));
  memset(&links, 0, sizeof(links));
  memset(&classs, 0, sizeof(classs));
  memset(&aconfs, 0, sizeof(aconfs));
  memset(&watchs, 0, sizeof(watchs));
}

#endif

void outofmemory(void)
{
  Debug((DEBUG_FATAL, "Out of memory: restarting server..."));
  restart("Out of Memory");
}

/*
 * Create a new aClient structure and set it to initial state.
 *
 *   from == NULL,   create local client (a client connected to a socket).
 *
 *   from != NULL,   create remote client (behind a socket associated with
 *                   the client defined by 'from').
 *                   ('from' is a local client!!).
 */
aClient *make_client(aClient *from, int status)
{
  Reg1 aClient *cptr = NULL;
  Reg2 size_t size = CLIENT_REMOTE_SIZE;

  /*
   * Check freelists first to see if we can grab a client without
   * having to call malloc.
   */
  if (!from)
    size = CLIENT_LOCAL_SIZE;

  if (!(cptr = (aClient *)RunMalloc(size)))
    outofmemory();
  memset(cptr, 0, size);        /* All variables are 0 by default */

#if defined(DEBUGMODE)
  if (size == CLIENT_LOCAL_SIZE)
    cloc.inuse++;
  else
    crem.inuse++;
#endif

  /* Note: structure is zero (memset) */
  cptr->from = from ? from : cptr;  /* 'from' of local client is self! */
  cptr->fd = -1;
  cptr->status = status;
  SlabStringAllocDup(&(cptr->username), "unknown", 0);

  if (size == CLIENT_LOCAL_SIZE)
  {
    cptr->since = cptr->lasttime = cptr->firsttime = now;
    cptr->lastnick = TStime();
    cptr->nextnick = now - NICK_DELAY;
    cptr->nexttarget = now - (TARGET_DELAY * (STARTTARGETS - 1));
    cptr->authfd = -1;
  }

  return (cptr);
}

void free_client(aClient *cptr)
{
  if (cptr->name)
    SlabStringFree(cptr->name);

  if (cptr->username)
    SlabStringFree(cptr->username);

  if (cptr->info)
    SlabStringFree(cptr->info);

  if (MyConnect(cptr))
  {
    if (cptr->sockhost)
      SlabStringFree(cptr->sockhost);
    
    DelClientEvent(cptr);
    DelRWAuthEvent(cptr);
    
    if(cptr->evread)
      RunFree(cptr->evread);

    if(cptr->evwrite)
      RunFree(cptr->evwrite);
    
    if(cptr->evauthread)
      RunFree(cptr->evauthread);

    if(cptr->evauthwrite)
      RunFree(cptr->evauthwrite);

    if(cptr->evtimer)
    {
      RunFree(cptr->evtimer);
      assert(cptr->tm_timer);
      RunFree(cptr->tm_timer);
    }

    if(cptr->evcheckping)
    {
      RunFree(cptr->evcheckping);
      assert(cptr->tm_checkping);
      RunFree(cptr->tm_checkping);
    }
  }

  RunFree(cptr);
}

/*
 * 'make_user' add's an User information block to a client
 * if it was not previously allocated.
 */
anUser *make_user(aClient *cptr)
{
  Reg1 anUser *user;

  user = cptr->user;
  if (!user)
  {
    if (!(user = (anUser *)RunMalloc(sizeof(anUser))))
      outofmemory();
    memset(user, 0, sizeof(anUser));  /* All variables are 0 by default */
#if defined(DEBUGMODE)
    users.inuse++;
#endif
    user->refcnt = 1;
    cptr->user = user;
  }
  return user;
}

aServer *make_server(aClient *cptr)
{
  Reg1 aServer *serv = cptr->serv;

  if (!serv)
  {
    if (!(serv = (aServer *)RunMalloc(sizeof(aServer))))
      outofmemory();
    memset(serv, 0, sizeof(aServer)); /* All variables are 0 by default */

#if defined(DEBUGMODE)
    servs.inuse++;
#endif
    cptr->serv = serv;
    DupString(serv->last_error_msg, "<>");  /* String must be non-empty */
  }
  return cptr->serv;
}

/*
 * free_user
 *
 * Decrease user reference count by one and realease block, if count reaches 0.
 */
void free_user(anUser *user)
{
  if (--user->refcnt == 0)
  {
    assert(!(user->joined || user->channel || (MyConnect(user->server)
        && user->server->invited)));

    if (user->away)
      RunFree(user->away);

    if (user->username)
      SlabStringFree(user->username);
    if (user->vhost)
      SlabStringFree(user->vhost);
    if (user->vhostperso)
      SlabStringFree(user->vhostperso);
    if (user->host)
      SlabStringFree(user->host);

    RunFree(user);
#if defined(DEBUGMODE)
    users.inuse--;
#endif
  }
}

/*
 * Taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
void remove_client_from_list(aClient *cptr)
{
  checklist();
  if (cptr->prev)
    cptr->prev->next = cptr->next;
  else
  {
    client = cptr->next;
    client->prev = NULL;
  }
  if (cptr->next)
    cptr->next->prev = cptr->prev;
  if (IsUser(cptr) && cptr->user)
  {
    add_history(cptr, 0);
    off_history(cptr);
  }
  if (cptr->user)
  {
    free_user(cptr->user);
  }

  if (cptr->serv)
  {
    if (cptr->serv->user)
      free_user(cptr->serv->user);
    if (cptr->serv->client_list)
      RunFree(cptr->serv->client_list);
    RunFree(cptr->serv->last_error_msg);
    if (cptr->serv->by)
      SlabStringFree(cptr->serv->by);
    RunFree(cptr->serv);
#if defined(DEBUGMODE)
    servs.inuse--;
#endif
  }
#if defined(DEBUGMODE)
  if (cptr->fd == -2)
    cloc.inuse--;
  else
    crem.inuse--;
#endif
  free_client(cptr);
  return;
}

/*
 * Although only a small routine, it appears in a number of places
 * as a collection of a few lines...functions like this *should* be
 * in this file, shouldnt they ?  after all, this is list.c, isn't it ?
 * -avalon
 */
void add_client_to_list(aClient *cptr)
{
  /*
   * Since we always insert new clients to the top of the list,
   * this should mean the "me" is the bottom most item in the list.
   */
  cptr->next = client;
  client = cptr;
  if (cptr->next)
    cptr->next->prev = cptr;
  return;
}

/*
 * Look for ptr in the linked listed pointed to by link.
 */
Link *find_user_link(Link *lp, aClient *ptr)
{
  if (ptr)
    while (lp)
    {
      if (lp->value.cptr == ptr)
        return (lp);
      lp = lp->next;
    }
  return NULL;
}

Link *make_link(void)
{
  Reg1 Link *lp;

  lp = (Link *)RunMalloc(sizeof(Link));
#if defined(DEBUGMODE)
  links.inuse++;
#endif
  return lp;
}

void free_link(Link *lp)
{
  RunFree(lp);
#if defined(DEBUGMODE)
  links.inuse--;
#endif
}

Dlink *add_dlink(Dlink **lpp, aClient *cp)
{
  Dlink *lp;
  lp = (Dlink *)RunMalloc(sizeof(Dlink));
  lp->value.cptr = cp;
  lp->prev = NULL;
  if ((lp->next = *lpp))
    lp->next->prev = lp;
  *lpp = lp;
  return lp;
}

void remove_dlink(Dlink **lpp, Dlink *lp)
{
  if (lp->prev)
  {
    if ((lp->prev->next = lp->next))
      lp->next->prev = lp->prev;
  }
  else if ((*lpp = lp->next))
    lp->next->prev = NULL;
  RunFree(lp);
}

aConfClass *make_class(void)
{
  Reg1 aConfClass *tmp;

  tmp = (aConfClass *) RunMalloc(sizeof(aConfClass));
#if defined(DEBUGMODE)
  classs.inuse++;
#endif
  return tmp;
}

void free_class(aConfClass * tmp)
{
  RunFree(tmp);
#if defined(DEBUGMODE)
  classs.inuse--;
#endif
}

aConfItem *make_conf(void)
{
  Reg1 aConfItem *aconf;

  aconf = (struct ConfItem *)RunMalloc(sizeof(aConfItem));
#if defined(DEBUGMODE)
  aconfs.inuse++;
#endif
  memset(&aconf->ipnum, 0, sizeof(struct in_addr));
  aconf->next = NULL;
  aconf->host = aconf->passwd = aconf->name = NULL;
  aconf->status = CONF_ILLEGAL;
  aconf->clients = 0;
  aconf->port = 0;
  aconf->hold = 0;
  aconf->confClass = NULL;
  return (aconf);
}

void delist_conf(aConfItem *aconf)
{
  if (aconf == conf)
    conf = conf->next;
  else
  {
    aConfItem *bconf;

    for (bconf = conf; aconf != bconf->next; bconf = bconf->next);
    bconf->next = aconf->next;
  }
  aconf->next = NULL;
}

void free_conf(aConfItem *aconf)
{
  del_queries((char *)aconf);
  RunFree(aconf->host);
  if (aconf->passwd)
    memset(aconf->passwd, 0, strlen(aconf->passwd));
  RunFree(aconf->passwd);
  RunFree(aconf->name);
  RunFree(aconf);
#if defined(DEBUGMODE)
  aconfs.inuse--;
#endif
  return;
}

aGline *make_gline(char *host, char *reason,
    char *name, time_t expire, time_t lastmod, time_t lifetime)
{
  Reg4 aGline *agline;
  const char *error_str;
  int erroffset;
  int gtype = 0;
  if (*host == '#' || *host == '&' || *host == '+')
    gtype = 1;                  /* BAD CHANNEL GLINE */

  agline = (struct Gline *)RunMalloc(sizeof(aGline)); /* alloc memory */
  DupString(agline->host, host);  /* copy vital information */
  DupString(agline->reason, reason);
  DupString(agline->name, name);
  agline->expire = expire;
  agline->lastmod = lastmod;
  agline->lifetime = lifetime;
  agline->re = NULL; /* Inicializo a NULL, para saber si hacer free luego */
  agline->gflags = GLINE_ACTIVE;  /* gline is active */

  /* Si empieza por $R o $r es de tipo RealName */
  if(*host == '$' && (*(host+1) == 'R' || *(host+1) == 'r')) {
    SetGlineRealName(agline); /* REALNAME GLINE */
    if(*(host+1) == 'r')
      SetGlineRealNameCI(agline);
    agline->re=pcre_compile((host+2), 0, &error_str, &erroffset, NULL);
  } else {
    /* Resto, no son de Realname */
    if (ipmask_parse(host, &agline->gl_addr, &agline->gl_bits))
      SetGlineIsIpMask(agline);
  }

  if (gtype)
  {
    agline->next = badchan;     /* link it into the list */
    return (badchan = agline);
  }

  agline->next = gline;         /* link it into the list */
  return (gline = agline);
}

aGline *find_gline(aClient *cptr, aGline **pgline)
{
  Reg3 aGline *agline = gline, *a2gline = NULL;
  char cptr_info_low[REALLEN+1];
  char *tmp;

  /* Paso el realname a minusculas para matcheo en pcre */
  strncpy(cptr_info_low, PunteroACadena(cptr->info), REALLEN);
  cptr_info_low[REALLEN]='\0';

  tmp=cptr_info_low;
  
  while (*tmp) {
    *tmp=toLower(*tmp);
    *tmp++;
  }
  
  while (agline)
  {                             /* look through all glines */
    if (agline->expire <= TStime())
    {                           /* handle expired glines */
      free_gline(agline, a2gline);
      agline = a2gline ? a2gline->next : gline;
      if (!agline)
        break;                  /* agline == NULL means gline == NULL */
      continue;
    }

    if(GlineIsRealNameCI(agline))
      tmp=cptr_info_low;
    else if(GlineIsRealName(agline))
      tmp=PunteroACadena(cptr->info);

    /* Does gline match? */
    /* Added a check against the user's IP address as well -Kev */
        
    if ((GlineIsIpMask(agline) ? ipmask_check(&cptr->ip, &agline->gl_addr, agline->gl_bits) == 0 :
    	(GlineIsRealName(agline) ? match_pcre(agline->re, tmp) :
    	  match(agline->host, PunteroACadena(cptr->sockhost)))) == 0 &&
    	  match(agline->name, PunteroACadena(cptr->user->username)) == 0)
    {
      if (pgline)
        *pgline = a2gline;      /* If they need it, give them the previous gline
                                   entry (probably for free_gline, below) */
      return agline;
    }

    a2gline = agline;
    agline = agline->next;
  }

  return NULL;                  /* found no glines */
}

void free_gline(aGline *agline, aGline *pgline)
{
  if (pgline)
    pgline->next = agline->next;  /* squeeze agline out */
  else
  {
    if (*agline->host == '#' || *agline->host == '&' || *agline->host == '+')
    {
      badchan = agline->next;
    }
    else
      gline = agline->next;
  }

  RunFree(agline->host);        /* and free up the memory */
  RunFree(agline->reason);
  RunFree(agline->name);
  if(agline->re)
    RunFree(agline->re);
  
  RunFree(agline);
}

int bad_channel(char *name)
{
  aGline *agline;

  agline = badchan;
  while (agline)
  {
    if ((agline->gflags & GLINE_ACTIVE) && (agline->expire > TStime()) &&
        !mmatch(agline->host, name))
    {
      return 1;
    }
    agline = agline->next;
  }
  return 0;
}


/*
 * Listas de WATCH
 *
 * make_watch()  Reserva una entrada en la lista de Watch.
 * free_watch()  Libera una entrada de la lista de Watch.
 *
 * 2002/05/20 zoltan <zoltan@irc-dev.net>
 */

aWatch *make_watch(char *nick)
{
  Reg1 aWatch *wptr;

  if (BadPtr(nick))
    return NULL;

  wptr = (aWatch *) RunMalloc(sizeof(aWatch));
  if (!wptr)
    outofmemory();
  memset(wptr, 0, sizeof(aWatch));

  DupString(wptr->nick, nick);

  hAddWatch(wptr);

#if defined(DEBUGMODE)
  watchs.inuse++;
#endif

  return (wptr);

}

void free_watch(aWatch * wptr)
{

  hRemWatch(wptr);
  RunFree(wptr->nick);
  RunFree(wptr);

#if defined(DEBUGMODE)
  watchs.inuse--;
#endif

}


#if defined(DEBUGMODE)
void send_listinfo(aClient *cptr, char *name)
{
  int inuse = 0, mem = 0, tmp = 0;

  sendto_one(cptr, ":%s %d %s :Local: inuse: %d(%d)",
      me.name, RPL_STATSDEBUG, name, inuse += cloc.inuse,
      tmp = cloc.inuse * CLIENT_LOCAL_SIZE);
  mem += tmp;
  sendto_one(cptr, ":%s %d %s :Remote: inuse: %d(%d)",
      me.name, RPL_STATSDEBUG, name,
      crem.inuse, tmp = crem.inuse * CLIENT_REMOTE_SIZE);
  mem += tmp;
  inuse += crem.inuse;
  sendto_one(cptr, ":%s %d %s :Users: inuse: %d(%d)",
      me.name, RPL_STATSDEBUG, name, users.inuse,
      tmp = users.inuse * sizeof(anUser));
  mem += tmp;
  inuse += users.inuse,
      sendto_one(cptr, ":%s %d %s :Servs: inuse: %d(%d)",
      me.name, RPL_STATSDEBUG, name, servs.inuse,
      tmp = servs.inuse * sizeof(aServer));
  mem += tmp;
  inuse += servs.inuse,
      sendto_one(cptr, ":%s %d %s :Links: inuse: %d(%d)",
      me.name, RPL_STATSDEBUG, name, links.inuse,
      tmp = links.inuse * sizeof(Link));
  mem += tmp;
  inuse += links.inuse,
      sendto_one(cptr, ":%s %d %s :Classes: inuse: %d(%d)",
      me.name, RPL_STATSDEBUG, name, classs.inuse,
      tmp = classs.inuse * sizeof(aConfClass));
  mem += tmp;
  inuse += classs.inuse,
      sendto_one(cptr, ":%s %d %s :Confs: inuse: %d(%d)",
      me.name, RPL_STATSDEBUG, name, aconfs.inuse,
      tmp = aconfs.inuse * sizeof(aConfItem));
  mem += tmp;
  inuse += aconfs.inuse,
      sendto_one(cptr, ":%s %d %s :Watchs: inuse: %d(%d)",
      me.name, RPL_STATSDEBUG, name, watchs.inuse,
      tmp = watchs.inuse * sizeof(aWatch));
  mem += tmp;
  inuse += watchs.inuse,
      sendto_one(cptr, ":%s %d %s :Totals: inuse %d %d",
      me.name, RPL_STATSDEBUG, name, inuse, mem);
}

#endif
