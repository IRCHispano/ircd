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
#include "client.h"
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
#include "ircd_alloc.h"
#include "ircd_chattr.h"
#include "ircd_string.h"
#include "pcre_match.h"
#include <assert.h>

RCSTAG_CC("$Id$");

/** Stores linked list statistics for various types of lists. */
static struct liststats {
  size_t alloc; /**< Number of structures ever allocated. */
  size_t inuse; /**< Number of structures currently in use. */
  size_t mem;   /**< Memory used by in-use structures. */
//} clients, connections, servs, links;
} clients, connections, servs, links, classs, aconfs, watchs;

/** Linked list of currently unused Client structures. */
static struct Client* clientFreeList;

/** Linked list of currently unused Connection structures. */
static struct Connection* connectionFreeList;

/** Linked list of currently unused SLink structures. */
static struct SLink* slinkFreeList;

/** Initialize the list manipulation support system.
 * @arg[in] maxconn Number of Client and Connection structures to preallocate.
 */
void init_list(int maxconn)
{
  struct Client* cptr;
  struct Connection* con;
  int i;
  /*
   * pre-allocate \a maxconn clients and connections
   */
  for (i = 0; i < maxconn; ++i) {
    cptr = (struct Client*) MyMalloc(sizeof(struct Client));
    cli_next(cptr) = clientFreeList;
    clientFreeList = cptr;
    clients.alloc++;

    con = (struct Connection*) MyMalloc(sizeof(struct Connection));
    con_next(con) = connectionFreeList;
    connectionFreeList = con;
    connections.alloc++;
  }
}

#if defined(DEBUGMODE)
void initlists(void)
{
  memset(&servs, 0, sizeof(servs));
  memset(&links, 0, sizeof(links));
  memset(&classs, 0, sizeof(classs));
  memset(&aconfs, 0, sizeof(aconfs));
#if defined(WATCH)
  memset(&watchs, 0, sizeof(watchs));
#endif
}

#endif

/** Allocate a new Client structure.
 * If #clientFreeList != NULL, use the head of that list.
 * Otherwise, allocate a new structure.
 * @return Newly allocated Client.
 */
static struct Client* alloc_client(void)
{
  struct Client* cptr = clientFreeList;

  if (!cptr) {
    cptr = (struct Client*) MyMalloc(sizeof(struct Client));
    clients.alloc++;
  } else
    clientFreeList = cli_next(cptr);

  clients.inuse++;

  memset(cptr, 0, sizeof(struct Client));

  return cptr;
}

/** Release a Client structure by prepending it to #clientFreeList.
 * @param[in] cptr Client that is no longer being used.
 */
static void dealloc_client(struct Client* cptr)
{
  assert(cli_verify(cptr));
  assert(0 == cli_connect(cptr));

  --clients.inuse;

  cli_next(cptr) = clientFreeList;
  clientFreeList = cptr;

  cli_magic(cptr) = 0;
}

/** Allocate a new Connection structure.
 * If #connectionFreeList != NULL, use the head of that list.
 * Otherwise, allocate a new structure.
 * @return Newly allocated Connection.
 */
static struct Connection* alloc_connection(void)
{
  struct Connection* con = connectionFreeList;

  if (!con) {
    con = (struct Connection*) MyMalloc(sizeof(struct Connection));
    connections.alloc++;
  } else
    connectionFreeList = con_next(con);

  connections.inuse++;

  memset(con, 0, sizeof(struct Connection));
//  timer_init(&(con_proc(con)));

  return con;
}

/** Release a Connection and all memory associated with it.
 * The connection's DNS reply field is freed, its file descriptor is
 * closed, its msgq and sendq are cleared, and its associated Listener
 * is dereferenced.  Then it is prepended to #connectionFreeList.
 * @param[in] con Connection to free.
 */
static void dealloc_connection(struct Connection* con)
{
  assert(con_verify(con));
//  assert(!t_active(&(con_proc(con))));
//  assert(!t_onqueue(&(con_proc(con))));

  Debug((DEBUG_LIST, "Deallocating connection %p", con));

//  if (-1 < con_fd(con))
//    close(con_fd(con));
//  MsgQClear(&(con_sendQ(con)));
//  client_drop_sendq(con);
//  DBufClear(&(con_recvQ(con)));
//  if (con_listener(con))
//    release_listener(con_listener(con));

  --connections.inuse;

  con_next(con) = connectionFreeList;
  connectionFreeList = con;

  con_magic(con) = 0;
}

/** Allocate a new client and initialize it.
 * If \a from == NULL, initialize the fields for a local client,
 * including allocating a Connection for him; otherwise initialize the
 * fields for a remote client..
 * @param[in] from Server connection that introduced the client (or
 * NULL).
 * @param[in] status Initial Client::cli_status value.
 * @return Newly allocated and initialized Client.
 */
struct Client* make_client(struct Client *from, int status)
{
  struct Client* cptr = 0;

  assert(!from || cli_verify(from));

  cptr = alloc_client();

  assert(0 != cptr);
  assert(!cli_magic(cptr));
  assert(0 == from || 0 != cli_connect(from));

  if (!from) { /* local client, allocate a struct Connection */
    struct Connection *con = alloc_connection();

    assert(0 != con);
    assert(!con_magic(con));

    con_magic(con) = CONNECTION_MAGIC;
//    con_fd(con) = -1; /* initialize struct Connection */
    con_freeflag(con) = 0;
    con_nextnick(con) = now - NICK_DELAY;
    con_nexttarget(con) = now - (TARGET_DELAY * (STARTTARGETS - 1));
//    con_handler(con) = UNREGISTERED_HANDLER;
    con_client(con) = cptr;

    cli_connect(cptr) = con; /* set the connection and other fields */
    cli_since(cptr) = cli_lasttime(cptr) = cli_firsttime(cptr) = now;
    cli_lastnick(cptr) = TStime();
  } else
    cli_connect(cptr) = cli_connect(from); /* use 'from's connection */

  assert(con_verify(cli_connect(cptr)));

  cli_magic(cptr) = CLIENT_MAGIC;
  cli_status(cptr) = status;
  cli_hnext(cptr) = cptr;
  //strcpy(cli_username(cptr), "unknown");
  SlabStringAllocDup(&(cptr->username), "unknown", 0);

  return cptr;
}

/** Release a Connection.
 * @param[in] con Connection to free.
 */
void free_connection(struct Connection* con)
{
  if (!con)
    return;

  assert(con_verify(con));
  assert(0 == con_client(con));

  dealloc_connection(con); /* deallocate the connection */
}

/** Release a Client.
 * In addition to the cleanup done by dealloc_client(), this will free
 * any pending auth request, free the connection for local clients,
 * and delete the processing timer for the client.
 * @param[in] cptr Client to free.
 */
void free_client(struct Client* cptr)
{
  if (!cptr)
    return;
  /*
   * forget to remove the client from the hash table?
   */
  assert(cli_verify(cptr));
  assert(cli_hnext(cptr) == cptr);
  /* or from linked list? */
  assert(cli_next(cptr) == 0);
  assert(cli_prev(cptr) == 0);

  Debug((DEBUG_LIST, "Freeing client %s [%p], connection %p", cli_name(cptr),
     cptr, cli_connect(cptr)));

//  if (cli_auth(cptr))
//    destroy_auth_request(cli_auth(cptr));

  /* Make sure we didn't magically get re-added to the list */
  assert(cli_next(cptr) == 0);
  assert(cli_prev(cptr) == 0);

  if (cli_from(cptr) == cptr) { /* in other words, we're local */
    cli_from(cptr) = 0;
    /* timer must be marked as not active */
//    if (!cli_freeflag(cptr) && !t_active(&(cli_proc(cptr))))
//      dealloc_connection(cli_connect(cptr)); /* connection not open anymore */
//    else {
//      if (-1 < cli_fd(cptr) && cli_freeflag(cptr) & FREEFLAG_SOCKET)
//    socket_del(&(cli_socket(cptr))); /* queue a socket delete */
//      if (cli_freeflag(cptr) & FREEFLAG_TIMER)
//    timer_del(&(cli_proc(cptr))); /* queue a timer delete */
//    }
  }

  if (cptr->name)
    SlabStringFree(cptr->name);

  if (cptr->username)
    SlabStringFree(cptr->username);

  if (cptr->info)
    SlabStringFree(cptr->info);

  if (MyConnect(cptr))
  {
    if (cptr->cli_connect->sockhost)
      SlabStringFree(cptr->cli_connect->sockhost);

    if (cptr->cli_connect->cookie)
      SlabStringFree(cptr->cli_connect->cookie);

    DelClientEvent(cptr);
    DelRWAuthEvent(cptr);

    if(cptr->cli_connect->evread)
      MyFree(cptr->cli_connect->evread);

    if(cptr->cli_connect->evwrite)
      MyFree(cptr->cli_connect->evwrite);

    if(cptr->cli_connect->evauthread)
      MyFree(cptr->cli_connect->evauthread);

    if(cptr->cli_connect->evauthwrite)
      MyFree(cptr->cli_connect->evauthwrite);

    if(cptr->cli_connect->evtimer)
    {
      MyFree(cptr->cli_connect->evtimer);
      assert(cptr->cli_connect->tm_timer);
      MyFree(cptr->cli_connect->tm_timer);
    }
    if(cptr->cli_connect->evcheckping)
    {
      MyFree(cptr->cli_connect->evcheckping);
      assert(cptr->cli_connect->tm_checkping);
      MyFree(cptr->cli_connect->tm_checkping);
    }
  }

  cli_connect(cptr) = 0;

  dealloc_client(cptr); /* actually destroy the client */
}


/*
 * 'make_user' add's an User information block to a client
 * if it was not previously allocated.
 */
anUser *make_user(struct Client *cptr)
{
  Reg1 anUser *user;

  user = cptr->cli_user;
  if (!user)
  {
    if (!(user = (anUser *)MyMalloc(sizeof(anUser))))
      outofmemory();
    memset(user, 0, sizeof(anUser));  /* All variables are 0 by default */
#if defined(DEBUGMODE)
//    users.inuse++;
#endif
    user->refcnt = 1;
    cptr->cli_user = user;
  }
  return user;
}

aServer *make_server(struct Client *cptr)
{
  Reg1 aServer *serv = cptr->cli_serv;

  if (!serv)
  {
    if (!(serv = (aServer *)MyMalloc(sizeof(aServer))))
      outofmemory();
    memset(serv, 0, sizeof(aServer)); /* All variables are 0 by default */

#if defined(DEBUGMODE)
    servs.inuse++;
#endif
    cptr->cli_serv = serv;
    DupString(serv->last_error_msg, "<>");  /* String must be non-empty */
  }
  return cptr->cli_serv;
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
        && user->server->cli_connect->invited)));

    if (user->away)
      MyFree(user->away);

    if (user->username)
      SlabStringFree(user->username);
    if (user->virtualhost)
      SlabStringFree(user->virtualhost);
    if (user->host)
      SlabStringFree(user->host);

    MyFree(user);
#if defined(DEBUGMODE)
//    users.inuse--;
#endif
  }
}

/*
 * Taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
void remove_client_from_list(struct Client *cptr)
{
  checklist();
  if (cptr->cli_prev)
    cptr->cli_prev->cli_next = cptr->cli_next;
  else
  {
    client = cptr->cli_next;
    client->cli_prev = NULL;
  }
  if (cptr->cli_next)
    cptr->cli_next->cli_prev = cptr->cli_prev;
  if (IsUser(cptr) && cptr->cli_user)
  {
    add_history(cptr, 0);
    off_history(cptr);
  }
  if (cptr->cli_user)
  {
    free_user(cptr->cli_user);
  }

  if (cptr->cli_serv)
  {
    if (cptr->cli_serv->user)
      free_user(cptr->cli_serv->user);
    if (cptr->cli_serv->client_list)
      MyFree(cptr->cli_serv->client_list);
    MyFree(cptr->cli_serv->last_error_msg);
    if (cptr->cli_serv->by)
      SlabStringFree(cptr->cli_serv->by);
    MyFree(cptr->cli_serv);
#if defined(DEBUGMODE)
    servs.inuse--;
#endif
  }
#if defined(DEBUGMODE1)
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
void add_client_to_list(struct Client *cptr)
{
  /*
   * Since we always insert new clients to the top of the list,
   * this should mean the "me" is the bottom most item in the list.
   */
  cptr->cli_next = client;
  client = cptr;
  if (cptr->cli_next)
    cptr->cli_next->cli_prev = cptr;
  return;
}

/*
 * Look for ptr in the linked listed pointed to by link.
 */
Link *find_user_link(Link *lp, struct Client *ptr)
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

/** Allocate a new SLink element.
 * Pulls from #slinkFreeList if it contains anything, else it
 * allocates a new one from the heap.
 * @return Newly allocated list element.
 */
struct SLink* make_link(void)
{
  struct SLink* lp = slinkFreeList;
  if (lp)
    slinkFreeList = lp->next;
  else {
    lp = (struct SLink*) MyMalloc(sizeof(struct SLink));
    links.alloc++;
  }
  assert(0 != lp);
  links.inuse++;
  memset(lp, 0, sizeof(*lp));
  return lp;
}

/** Release a singly linked list element.
 * @param[in] lp List element to mark as unused.
 */
void free_link(struct SLink* lp)
{
  if (lp) {
    lp->next = slinkFreeList;
    slinkFreeList = lp;
    links.inuse--;
  }
}

/** Add an element to a doubly linked list.
 * If \a lpp points to a non-NULL pointer, its DLink::prev field is
 * updated to point to the newly allocated element.  Regardless,
 * \a lpp is overwritten with the pointer to the new link.
 * @param[in,out] lpp Pointer to insertion location.
 * @param[in] cp %Client to put in newly allocated element.
 * @return Allocated link structure (same as \a lpp on output).
 */
struct DLink *add_dlink(struct DLink **lpp, struct Client *cp)
{
  struct DLink* lp = (struct DLink*) MyMalloc(sizeof(struct DLink));
  assert(0 != lp);
  lp->value.cptr = cp;
  lp->prev = 0;
  if ((lp->next = *lpp))
    lp->next->prev = lp;
  *lpp = lp;
  return lp;
}

/** Remove a node from a doubly linked list.
 * @param[out] lpp Pointer to next list element.
 * @param[in] lp List node to unlink.
 */
void remove_dlink(struct DLink **lpp, struct DLink *lp)
{
  assert(0 != lpp);
  assert(0 != lp);

  if (lp->prev) {
    if ((lp->prev->next = lp->next))
      lp->next->prev = lp->prev;
  }
  else if ((*lpp = lp->next))
    lp->next->prev = NULL;
  MyFree(lp);
}

aConfClass *make_class(void)
{
  Reg1 aConfClass *tmp;

  tmp = (aConfClass *) MyMalloc(sizeof(aConfClass));
#if defined(DEBUGMODE)
  classs.inuse++;
#endif
  return tmp;
}

void free_class(aConfClass * tmp)
{
  MyFree(tmp);
#if defined(DEBUGMODE)
  classs.inuse--;
#endif
}

aConfItem *make_conf(void)
{
  Reg1 aConfItem *aconf;

  aconf = (struct ConfItem *)MyMalloc(sizeof(aConfItem));
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
  MyFree(aconf->host);
  if (aconf->passwd)
    memset(aconf->passwd, 0, strlen(aconf->passwd));
  MyFree(aconf->passwd);
  MyFree(aconf->name);
  MyFree(aconf);
#if defined(DEBUGMODE)
  aconfs.inuse--;
#endif
  return;
}

aGline *make_gline(int is_ipmask, char *host, char *reason,
    char *name, time_t expire, time_t lastmod, time_t lifetime)
{
  Reg4 aGline *agline;
  const char *error_str;
  int erroffset;
#if defined(BADCHAN)
  int gtype = 0;
  if (*host == '#' || *host == '&' || *host == '+')
    gtype = 1;                  /* BAD CHANNEL GLINE */
#endif

  agline = (struct Gline *)MyMalloc(sizeof(aGline)); /* alloc memory */
  DupString(agline->host, host);  /* copy vital information */
  DupString(agline->reason, reason);
  DupString(agline->name, name);
  agline->expire = expire;
  agline->lastmod = lastmod;
  agline->lifetime = lifetime;
  agline->re = NULL; /* Inicializo a NULL, para saber si hacer free luego */
  agline->gflags = GLINE_ACTIVE;  /* gline is active */
  if (is_ipmask)
    SetGlineIsIpMask(agline);
  
  /* Si empieza por $R o $r es de tipo RealName */
  if(*host == '$' && (*(host+1) == 'R' || *(host+1) == 'r')) {
    SetGlineRealName(agline); /* REALNAME GLINE */
    if(*(host+1) == 'r')
      SetGlineRealNameCI(agline);
    agline->re=pcre_compile((host+2), 0, &error_str, &erroffset, NULL);
  }

#if defined(BADCHAN)
  if (gtype)
  {
    agline->next = badchan;     /* link it into the list */
    return (badchan = agline);
  }
#endif
  agline->next = gline;         /* link it into the list */
  return (gline = agline);
}

aGline *find_gline(struct Client *cptr, aGline **pgline)
{
  Reg3 aGline *agline = gline, *a2gline = NULL;
  char cptr_info_low[REALLEN+1];
  char *tmp;

  /* Paso el realname a minusculas para matcheo en pcre */
  strncpy(cptr_info_low, PunteroACadena(cptr->info), REALLEN);
  cptr_info_low[REALLEN]='\0';

  tmp=cptr_info_low;
  
  while (*tmp) {
    *tmp=ToLower(*tmp);
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
        
    if ((GlineIsIpMask(agline) ? match(agline->host, ircd_ntoa(client_addr(cptr))) :
    	(GlineIsRealName(agline) ? match_pcre(agline->re, tmp) :
#ifdef HISPANO_WEBCHAT
    	  match(agline->host, PunteroACadena(cptr->cli_user->host)))) == 0 &&
#else
    	  match(agline->host, PunteroACadena(cptr->cli_connect->sockhost)))) == 0 &&
#endif
    	  match(agline->name, PunteroACadena(cptr->cli_user->username)) == 0)
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
#if defined(BADCHAN)
    if (*agline->host == '#' || *agline->host == '&' || *agline->host == '+')
    {
      badchan = agline->next;
    }
    else
#endif
      gline = agline->next;
  }

  MyFree(agline->host);        /* and free up the memory */
  MyFree(agline->reason);
  MyFree(agline->name);
  if(agline->re)
    MyFree(agline->re);
  
  MyFree(agline);
}

#if defined(BADCHAN)
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

#endif /* BADCHAN */

#if defined(WATCH)
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

  wptr = (aWatch *) MyMalloc(sizeof(aWatch));
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
  MyFree(wptr->nick);
  MyFree(wptr);

#if defined(DEBUGMODE)
  watchs.inuse--;
#endif

}

#endif /* WATCH */

#if defined(DEBUGMODE)
void send_listinfo(struct Client *cptr, char *name)
{
  int inuse = 0, mem = 0, tmp = 0;
#if 0
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
#endif
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
#if defined(WATCH)
      sendto_one(cptr, ":%s %d %s :Watchs: inuse: %d(%d)",
      me.name, RPL_STATSDEBUG, name, watchs.inuse,
      tmp = watchs.inuse * sizeof(aWatch));
  mem += tmp;
  inuse += watchs.inuse,
#endif /* WATCH */
      sendto_one(cptr, ":%s %d %s :Totals: inuse %d %d",
      me.name, RPL_STATSDEBUG, name, inuse, mem);
}

#endif
