/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/watch.c
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2002 Toni Garcia (zoltan) <zoltan@irc-dev.net>
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
 * $Id: watch.c,v 1.7 2007-04-19 22:53:53 zolty Exp $
 *
 */
#include "config.h"

#include "watch.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "list.h"
#include "numeric.h"
#include "s_user.h"
#include "send.h"
#include "struct.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <string.h>

/*
 * WATCH FUNCTIONS
 *
 * WATCH_LIST
 * |
 * |-wptr1-|- cptr1
 * |       |- cptr2
 * |       |- cptr3
 * |
 * |-wptr2-|- cptr2
 *         |- cptr1
 *
 * LINKS in the Client lists.
 *
 * cptr1            cptr2           cptr3
 * |- wptr1(nickA)  |-wptr1(nickA)  |-wptr1(nickA)
 * |- wptr2(nickB)  |-wptr2(nickB)
 *
 * The operation is based on the WATCH of Bahamut and UnrealIRCD.
 *
 * 2002/05/20 zoltan <zoltan@irc-dev.net>
 */

/** Count of allocated Watch structures. */
static int watchCount = 0;

/*
 * make_watch()
 *
 * Reserve an entrance in the Watch list.
 */
struct Watch *make_watch(const char *nick)
{
  struct Watch *wptr;

  wptr = (struct Watch *)MyMalloc(sizeof(struct Watch));
  assert(0 != wptr);

  /*
   * NOTE: Do not remove this, a lot of code depends on the entire
   * structure being zeroed out
   */
  memset(wptr, 0, sizeof(struct Watch));	/* All variables are 0 by default */

  DupString(wt_nick(wptr), nick);

  hAddWatch(wptr);
  watchCount++;

  return (wptr);

}

/*
 * free_watch()
 *
 * Release an entrace of the Watch list.
 */
void free_watch(struct Watch *wptr)
{

  hRemWatch(wptr);
  MyFree(wt_nick(wptr));
  MyFree(wptr);

  watchCount--;
}

/** Find number of Watch structs allocated and memory used by them.
 * @param[out] count_out Receives number of Watch structs allocated.
 * @param[out] bytes_out Receives number of bytes used by Watch structs.
 */
void watch_count_memory(size_t* count_out, size_t* bytes_out)
{
  assert(0 != count_out);
  assert(0 != bytes_out);
  *count_out = watchCount;
  *bytes_out = watchCount * sizeof(struct Watch);
}

/*
 * check_status_watch()
 *
 * Notify the users the input/output of nick.
 */
void check_status_watch(struct Client *cptr, int raw)
{
  struct Watch *wptr;
  struct SLink *lp;

  wptr = FindWatch(cli_name(cptr));

  if (!wptr)
    return;			/* Not this in some notify */

  wt_lasttime(wptr) = TStime();

  /*
   * Sent the warning to all the users who
   * have it in notify.
   */
  for (lp = wt_watch(wptr); lp; lp = lp->next)
  {
    send_reply(lp->value.cptr, raw, cli_name(cptr),
	IsUser(cptr) ? cli_user(cptr)->username : "<N/A>",
	IsUser(cptr) ? 
 	  (HasHiddenHost(cptr) && !IsViewHiddenHost(lp->value.cptr) ?
	  cli_user(cptr)->host : cli_user(cptr)->realhost)
	: "<N/A>",
	wt_lasttime(wptr));
  }

}

/*
 * show_status_watch()
 *
 * Show the state of an user.
 */
void show_status_watch(struct Client *cptr, char *nick, int raw1, int raw2)
{
  struct Client *acptr;

  if ((acptr = FindUser(nick)))
  {
    send_reply(cptr, raw1, cli_name(acptr), cli_user(acptr)->username,
        HasHiddenHost(acptr) && !IsViewHiddenHost(cptr) ?
        cli_user(acptr)->host : cli_user(acptr)->realhost,
	cli_lastnick(acptr));
  }
  else
    send_reply(cptr, raw2, nick, "*", "*", 0);
}


/*
 * add_nick_watch()
 *
 * Add nick to the user watch list.
 */
int add_nick_watch(struct Client *cptr, char *nick)
{
  struct Watch *wptr;
  struct SLink *lp;

  /*
   * If not exist, create the registry.
   */
  if (!(wptr = FindWatch(nick)))
  {
    wptr = make_watch(nick);
    if (!wptr)
      return 0;
    wt_lasttime(wptr) = TStime();
  }

  /*
   * Find if it already has it in watch.
   */
  if ((lp = wt_watch(wptr)))
  {
    while (lp && (lp->value.cptr != cptr))
      lp = lp->next;
  }

  /*
   * Not this, then add it.
   */
  if (!lp)
  {
    /*
     * Link watch to cptr
     */
    lp = wt_watch(wptr);
    wt_watch(wptr) = make_link();
    memset(wt_watch(wptr), 0, sizeof(struct SLink));
    wt_watch(wptr)->value.cptr = cptr;
    wt_watch(wptr)->next = lp;

    /*
     * Link client->user to watch
     */
    lp = make_link();
    memset(lp, 0, sizeof(struct SLink));
    lp->next = cli_user(cptr)->watch;
    lp->value.wptr = wptr;
    cli_user(cptr)->watch = lp;
    cli_user(cptr)->watches++;

  }
  return 0;
}


/*
 * del_nick_watch()
 *
 * Delete a nick of the user watch list.
 */
int del_nick_watch(struct Client *cptr, char *nick)
{
  struct Watch *wptr;
  struct SLink *lp, *lptmp = 0;

  wptr = FindWatch(nick);
  if (!wptr)
    return 0;			/* Not this in any list */

  /*
   * Find for in the link cptr->user to watch
   */
  if ((lp = wt_watch(wptr)))
  {
    while (lp && (lp->value.cptr != cptr))
    {
      lptmp = lp;
      lp = lp->next;
    }
  }

  if (!lp)
    return 0;

  if (!lptmp)
    wt_watch(wptr) = lp->next;
  else
    lptmp->next = lp->next;

  free_link(lp);

  /*
   * For in the link watch to cptr->user
   */
  lptmp = lp = 0;
  if ((lp = cli_user(cptr)->watch))
  {
    while (lp && (lp->value.wptr != wptr))
    {
      lptmp = lp;
      lp = lp->next;
    }
  }

  assert(0 != lp);

  if (!lptmp)
    cli_user(cptr)->watch = lp->next;
  else
    lptmp->next = lp->next;

  free_link(lp);

  /*
   * If it were the onlu associate to nick
   * delete registry in the watch table.
   */
  if (!wt_watch(wptr))
    free_watch(wptr);

  /* Update count */
  cli_user(cptr)->watches--;

  return 0;

}


/*
 * del_list_watch()
 *
 * Delete all the watch list.
 * When it execute a /WATCH C or it leaves the IRC.
 */
int del_list_watch(struct Client *cptr)
{
  struct SLink *lp, *lp2, *lptmp;
  struct Watch *wptr;

  if (!(lp = cli_user(cptr)->watch))
    return 0;			/* Id had the empty list */

  /*
   * Loop of links cptr->user to watch.
   */
  while (lp)
  {
    wptr = lp->value.wptr;
    lptmp = 0;
    for (lp2 = wt_watch(wptr); lp2 && (lp2->value.cptr != cptr); lp2 = lp2->next)
      lptmp = lp2;


    assert(0 != lp2);

    if (!lptmp)
      wt_watch(wptr) = lp2->next;
    else
      lptmp->next = lp2->next;
    free_link(lp2);

    /*
     * If it were the onlu associate to nick
     * delete registry in the watch table. 
     */
    if (!wt_watch(wptr))
      free_watch(wptr);

    lp2 = lp;
    lp = lp->next;
    free_link(lp2);
  }

  /* Update count */
  cli_user(cptr)->watch = 0;
  cli_user(cptr)->watches = 0;

  return 0;
}
