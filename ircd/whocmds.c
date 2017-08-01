/*
 * IRC - Internet Relay Chat, ircd/s_user.c (formerly ircd/s_msg.c)
 * Copyright (C) 1990 Jarkko Oikarinen and
 *                    University of Oulu, Computing Center
 *
 * See file AUTHORS in IRC package for additional names of
 * the programmers.
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
#include <sys/stat.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <stdlib.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(USE_SYSLOG)
#include <syslog.h>
#endif
#include "h.h"
#include "s_debug.h"
#include "struct.h"
#include "common.h"
#include "s_serv.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "s_misc.h"
#include "match.h"
#include "hash.h"
#include "s_bsd.h"
#include "whowas.h"
#include "list.h"
#include "s_err.h"
#include "parse.h"
#include "ircd.h"
#include "s_user.h"
#include "support.h"
#include "s_user.h"
#include "channel.h"
#include "random.h"
#include "version.h"
#include "msg.h"
#include "userload.h"
#include "numnicks.h"
#include "sprintf_irc.h"
#include "querycmds.h"
#include "IPcheck.h"
#include "s_bdd.h"
#include "network.h"

RCSTAG_CC("$Id$");

/*
 * m_who() 
 * m_who with support routines rewritten by Nemesi, August 1997
 * - Alghoritm have been flattened (no more recursive)
 * - Several bug fixes
 * - Strong performance improvement
 * - Added possibility to have specific fields in the output
 * See readme.who for further details.
 */

/* Macros used only in here by m_who and its support functions */

#define WHOSELECT_OPER 1
#define WHOSELECT_EXTRA 2

#define WHO_FIELD_QTY 1
#define WHO_FIELD_CHA 2
#define WHO_FIELD_UID 4
#define WHO_FIELD_NIP 8
#define WHO_FIELD_HOS 16
#define WHO_FIELD_SER 32
#define WHO_FIELD_NIC 64
#define WHO_FIELD_FLA 128
#define WHO_FIELD_DIS 256
#define WHO_FIELD_REN 512

#define WHO_FIELD_DEF ( WHO_FIELD_NIC | WHO_FIELD_UID | WHO_FIELD_HOS | WHO_FIELD_SER )

#define IS_VISIBLE_USER(s,ac) ((s==ac) || (!IsInvisible(ac)))

#define SEE_LUSER(s, ac, b) (IS_VISIBLE_USER(s, ac) || \
                             ((b & WHOSELECT_EXTRA) && MyConnect(ac) && \
                             (HasPriv((s), PRIV_SHOW_INVIS) || \
                              HasPriv((s), PRIV_SHOW_ALL_INVIS))))

#define SEE_USER(s, ac, b) (SEE_LUSER(s, ac, b) || \
                            ((b & WHOSELECT_EXTRA) && \
                              HasPriv((s), PRIV_SHOW_ALL_INVIS)))

#define SHOW_MORE(sptr, counter) (HasPriv(sptr, PRIV_UNLIMIT_QUERY) || (!(counter-- < 0)) )

#define SEE_CHANNEL(s, chptr, b) (!SecretChannel(chptr) || ((b & WHOSELECT_EXTRA) && HasPriv((s), PRIV_SEE_CHAN)))


/*
 * A little spin-marking utility to tell us wich clients we have already
 * processed and wich not
 */
static int who_marker = 0;
static void move_marker(void)
{
  if (!++who_marker)
  {
    aClient *cptr = client;
    while (cptr)
    {
      cptr->marker = 0;
      cptr = cptr->next;
    }
    who_marker++;
  }
}

#define CheckMark(x, y) ((x == y) ? 0 : (x = y))
#define Process(cptr) CheckMark(cptr->marker, who_marker)

/*
 * The function that actually prints out the WHO reply for a client found
 */
static void do_who(aClient *sptr, aClient *acptr, aChannel *repchan,
    int fields, int extra, char *qrt)
{
  Reg1 char *p1;
  Reg2 aChannel *chptr;

  static char buf1[512];
  /* NOTE: with current fields list and sizes this _cannot_ overrun, 
     and also the message finally sent shouldn't ever be truncated */

  p1 = buf1;
  chptr = repchan;
  buf1[1] = '\0';

  /* If we don't have a channel and we need one... try to find it,
     unless the listing is for a channel service, we already know
     that there are no common channels, thus use PubChannel and not
     SeeChannel */
  if (!chptr && (!fields || (fields & (WHO_FIELD_CHA | WHO_FIELD_FLA)))
      && !IsNoChan(acptr) || acptr == sptr || IsHelpOp(sptr) || IsAnOper(sptr))
  {
    Reg3 Link *lp;
    for (lp = acptr->user->channel; lp && !chptr; lp = lp->next)
      if (PubChannel(lp->value.chptr) &&
          (acptr == sptr || !is_zombie(acptr, chptr)))
        chptr = lp->value.chptr;
  }

  /* Place the fields one by one in the buffer and send it
     note that fields == NULL means "default query" */

  if (fields & WHO_FIELD_QTY)   /* Query type */
  {
    *(p1++) = ' ';
    if (BadPtr(qrt))
      *(p1++) = '0';
    else
      while ((*qrt) && (*(p1++) = *(qrt++)));
  }

  if (!fields || (fields & WHO_FIELD_CHA))
  {
    Reg3 char *p2;
    *(p1++) = ' ';
    if ((p2 = (chptr ? chptr->chname : NULL)))
      while ((*p2) && (*(p1++) = *(p2++)));
    else
      *(p1++) = '*';
  }

  if (!fields || (fields & WHO_FIELD_UID))
  {
    Reg3 char *p2 = PunteroACadena(acptr->user->username);
    *(p1++) = ' ';
    while ((*p2) && (*(p1++) = *(p2++)));
  }

  if (fields & WHO_FIELD_NIP)
  {
    Reg3 char *p2 = NULL;

#if defined(BDD_VIP)
    if (IsHidden(acptr) && (!can_viewhost(sptr, acptr)) && (acptr != sptr))
    {
      static char buf[8];

      strcpy(buf, "0.0.0.0\0");
      p2 = (char *)buf;
    }
#endif

    if (!p2)
      p2 = ircd_ntoa_c(acptr);
    *(p1++) = ' ';
    while ((*p2) && (*(p1++) = *(p2++)));
  }

  if (!fields || (fields & WHO_FIELD_HOS))
  {
#if defined(BDD_VIP)
    Reg3 char *p2 = get_visiblehost(acptr, extra ? sptr : NULL);
#else
    Reg3 char *p2 = acptr->user->host;
#endif
    *(p1++) = ' ';
    while ((*p2) && (*(p1++) = *(p2++)));
  }

  if (!fields || (fields & WHO_FIELD_SER))
  {
    Reg3 char *p2 = (ocultar_servidores && !(IsAnOper(sptr) || IsHelpOp(sptr))) ? 
                     his.name : acptr->user->server->name;
    *(p1++) = ' ';
    while ((*p2) && (*(p1++) = *(p2++)));
  }

  if (!fields || (fields & WHO_FIELD_NIC))
  {
    Reg3 char *p2 = acptr->name;
    *(p1++) = ' ';
    while ((*p2) && (*(p1++) = *(p2++)));
  }

  if (!fields || (fields & WHO_FIELD_FLA))
  {
    *(p1++) = ' ';
    if (acptr->user->away)
      *(p1++) = 'G';
    else
      *(p1++) = 'H';
    if (IsAnOper(acptr) || IsHelpOp(acptr))
    {
      *(p1++) = '*';
    }

    if (chptr && is_zombie(acptr, chptr))
    {
      *(p1++) = '!';
    }
    else
    {
      if (chptr && is_chan_op(acptr, chptr))
        *(p1++) = '@';
      if (chptr && has_voice(acptr, chptr))
        *(p1++) = '+';
    }

    if (IsDeaf(acptr))
      *(p1++) = 'd';
    if (IsAdmin(acptr))
      *(p1++) = 'a';
    if (IsCoder(acptr))
      *(p1++) = 'C';
    if (IsHelpOp(acptr))
      *(p1++) = 'h';
    if (IsHidden(acptr))
      *(p1++) = 'x';
    if (IsHiddenViewer(acptr))
      *(p1++) = 'X';
    if (IsNickRegistered(acptr))
      *(p1++) = 'r';
    if (IsNickSuspended(acptr))
      *(p1++) = 'S';
    if (IsMsgOnlyReg(acptr))
      *(p1++) = 'R';
    if (IsStripColor(acptr))
      *(p1++) = 'c';
    if (IsSSL(acptr))
      *(p1++) = 'z';
    if (IsServicesBot(acptr))
      *(p1++) = 'B';
    if (IsNoChan(acptr))
      *(p1++) = 'n';
    if (IsChannelService(acptr))
      *(p1++) = 'k';
    if (IsDocking(acptr))
      *(p1++) = 'K';

    if (IsAnOper(sptr) || IsHelpOp(sptr))
    {
      if (IsInvisible(acptr))
        *(p1++) = 'i';
      if (SendWallops(acptr))
        *(p1++) = 'w';
      if (SendDebug(acptr))
        *(p1++) = 'g';
      if (IsUserDeaf(acptr))
        *(p1++) = 'D';
      if (IsUserBitch(acptr))
        *(p1++) = 'P';
      if (IsUserNoJoin(acptr))
        *(p1++) = 'J';
      if (IsNoIdle(acptr))
        *(p1++) = 'I';
      if (IsWhois(acptr) && (IsAdmin(sptr) || IsCoder(sptr) || acptr==sptr))
        *(p1++) = 'W';
    }
  }

  if (!fields || (fields & WHO_FIELD_DIS))
  {
    *p1++ = ' ';
    if (!fields)
      *p1++ = ':';              /* Place colon here for default reply */
    if (ocultar_servidores && !(IsAnOper(sptr) || IsHelpOp(sptr)))
      *p1++ = (sptr == acptr) ? '0' : '3';
    else
      p1 = sprintf_irc(p1, "%d", acptr->hopcount);
  }

  if (!fields || (fields & WHO_FIELD_REN))
  {
    Reg3 char *p2 = PunteroACadena(acptr->info);
    *p1++ = ' ';
    if (fields)
      *p1++ = ':';              /* Place colon here for special reply */
    while ((*p2) && (*(p1++) = *(p2++)));
  }

  /* The first char will always be an useless blank and we 
     need to terminate buf1 */
  *p1 = '\0';
  p1 = buf1;
  sendto_one(sptr, rpl_str(fields ? RPL_WHOSPCRPL : RPL_WHOREPLY),
      me.name, sptr->name, ++p1);
}

/*
 *  m_who
 *
 *  parv[0] = sender prefix
 *  parv[1] = nickname mask list
 *  parv[2] = additional selection flag, only 'o' for now.
 *            and %flags to specify what fields to output
 *            plus a ,querytype if the t flag is specified
 *            so the final thing will be like o%tnchu,777
 *  parv[3] = _optional_ parameter that overrides parv[1]
 *            This can be used as "/quote who foo % :The Black Hacker
 *            to find me, parv[3] _can_ contain spaces !.
 */

int m_who(aClient *UNUSED(cptr), aClient *sptr, int parc, char *parv[])
{
  Reg1 char *mask;              /* The mask we are looking for              */
  Reg2 char ch;                 /* Scratch char                    */
  Reg3 Link *lp, *lp2;
  Reg4 aChannel *chptr;         /* Channel to show                          */
  Reg5 aClient *acptr;          /* Client to show                           */

  int bitsel;                   /* Mask of selectors to apply               */
  int matchsel;                 /* Wich fields the match should apply on    */
  int counter;                  /* Query size counter,
                                   initially used to count fields           */
  int commas;                   /* Does our mask contain any comma ?
                                   If so is a list..                        */
  int fields;                   /* Mask of fields to show                   */
  int isthere = 0;              /* When this set the user is member of chptr */
  char *nick;                   /* Single element extracted from
                                   the mask list                            */
  char *p;                      /* Scratch char pointer                     */
  char *qrt;                    /* Pointer to the query type                */
  static char mymask[512];      /* To save the mask before corrupting it    */

  if (IsServer(sptr))
    return 0;

  /* Let's find where is our mask, and if actually contains something */
  mask = ((parc > 1) ? parv[1] : NULL);
  if (parc > 3 && parv[3])
    mask = parv[3];
  if (mask && ((mask[0] == '\0') ||
      (mask[1] == '\0' && ((mask[0] == '0') || (mask[0] == '*')))))
    mask = NULL;

  /* Evaluate the flags now, we consider the second parameter 
     as "matchFlags%fieldsToInclude,querytype"           */
  bitsel = fields = counter = matchsel = 0;
  qrt = NULL;
  if (parc > 2 && parv[2] && *parv[2])
  {
    p = parv[2];
    while (((ch = *(p++))) && (ch != '%') && (ch != ','))
      switch (ch)
      {
        case 'o':
        case 'O':
          bitsel |= WHOSELECT_OPER;
          continue;
        case 'x':
        case 'X':
          if (HasPriv(sptr, PRIV_WHOX) && IsAnOper(sptr)) {
            bitsel |= WHOSELECT_EXTRA;
#if defined(WPATH)
            write_log(WPATH, "# " TIME_T_FMT " %s!%s@%s WHO %s %s\n",
                now, sptr->name, PunteroACadena(sptr->user->username),
                PunteroACadena(sptr->user->host),
                (BadPtr(parv[3]) ? parv[1] : parv[3]), parv[2]);
#endif /* WPATH */
          }
          continue;
        case 'n':
        case 'N':
          matchsel |= WHO_FIELD_NIC;
          continue;
        case 'u':
        case 'U':
          matchsel |= WHO_FIELD_UID;
          continue;
        case 'h':
        case 'H':
          matchsel |= WHO_FIELD_HOS;
          continue;
        case 'i':
        case 'I':
          matchsel |= WHO_FIELD_NIP;
          continue;
        case 's':
        case 'S':
          matchsel |= WHO_FIELD_SER;
          continue;
        case 'r':
        case 'R':
          matchsel |= WHO_FIELD_REN;
          continue;
      }
    if (ch == '%')
      while ((ch = *p++) && (ch != ','))
      {
        counter++;
        switch (ch)
        {
          case 'c':
          case 'C':
            fields |= WHO_FIELD_CHA;
            break;
          case 'd':
          case 'D':
            fields |= WHO_FIELD_DIS;
            break;
          case 'f':
          case 'F':
            fields |= WHO_FIELD_FLA;
            break;
          case 'h':
          case 'H':
            fields |= WHO_FIELD_HOS;
            break;
          case 'i':
          case 'I':
            fields |= WHO_FIELD_NIP;
            break;
          case 'n':
          case 'N':
            fields |= WHO_FIELD_NIC;
            break;
          case 'r':
          case 'R':
            fields |= WHO_FIELD_REN;
            break;
          case 's':
          case 'S':
            fields |= WHO_FIELD_SER;
            break;
          case 't':
          case 'T':
            fields |= WHO_FIELD_QTY;
            break;
          case 'u':
          case 'U':
            fields |= WHO_FIELD_UID;
            break;
          default:
            break;
        }
      };
    if (ch)
      qrt = p;
  }

  if (!matchsel)
    matchsel = WHO_FIELD_DEF;
  if (!fields)
    counter = 7;

  if (ocultar_servidores && !(IsAnOper(sptr) || IsHelpOp(sptr)))
    matchsel &= ~WHO_FIELD_SER;

  if (qrt && (fields & WHO_FIELD_QTY))
  {
    p = qrt;
    if (!((*p > '9') || (*p < '0')))
      p++;
    if (!((*p > '9') || (*p < '0')))
      p++;
    if (!((*p > '9') || (*p < '0')))
      p++;
    *p = '\0';
  }
  else
    qrt = NULL;

  /* I'd love to add also a check on the number of matches fields per time */
  counter = (2048 / (counter + 4));
  if (mask && (strlen(mask) > 510))
    mask[510] = '\0';
  move_marker();
  commas = (mask && strchr(mask, ','));

  /* First treat mask as a list of plain nicks/channels */
  if (mask)
  {
    strcpy(mymask, mask);
    for (p = NULL, nick = strtoken(&p, mymask, ","); nick;
        nick = strtoken(&p, NULL, ","))
    {
      if (IsChannelName(nick) && (chptr = FindChannel(nick)))
      {
        isthere = (IsMember(sptr, chptr) != NULL);
        if (isthere || SEE_CHANNEL(sptr, chptr, bitsel))
        {
          for (lp = chptr->members; lp; lp = lp->next)
          {
            acptr = lp->value.cptr;
            if ((bitsel & WHOSELECT_OPER) && !(IsAnOper(acptr))
                && !(IsHelpOp(acptr)))
            {
              continue;
            }
            if ((acptr != sptr) && (lp->flags & CHFL_ZOMBIE))
              continue;
            if (!(isthere || (SEE_USER(sptr, acptr, bitsel))))
              continue;
            if (!Process(acptr))  /* This can't be moved before other checks */
              continue;
            if (!(isthere || (SHOW_MORE(sptr, counter))))
              break;
            do_who(sptr, acptr, chptr, fields, bitsel & WHOSELECT_EXTRA, qrt);
          }
        }
      }
      else
      {
        if ((acptr = FindUser(nick)) && ((!(bitsel & WHOSELECT_OPER)) ||
            IsHelpOp(acptr) ||
            IsAnOper(acptr)) && Process(acptr) && SHOW_MORE(sptr, counter))
        {
          do_who(sptr, acptr, NULL, fields, bitsel & WHOSELECT_EXTRA, qrt);
        }
      }
    }
  }

  /* If we didn't have any comma in the mask treat it as a
     real mask and try to match all relevant fields */
  if (!(commas || (counter < 1)))
  {
    int minlen, cset;
    struct irc_in_addr imask;
    unsigned char ibits;
    if (mask)
    {
      matchcomp(mymask, &minlen, &cset, mask);
      if (!ipmask_parse(mask, &imask, &ibits))
        matchsel &= ~WHO_FIELD_NIP;
      if ((minlen > NICKLEN) || !(cset & NTL_IRCNK))
        matchsel &= ~WHO_FIELD_NIC;
      if ((matchsel & WHO_FIELD_SER) &&
          ((minlen > HOSTLEN) || (!(cset & NTL_IRCHN))
          || (!markMatchexServer(mymask, minlen))))
        matchsel &= ~WHO_FIELD_SER;
      if ((minlen > USERLEN) || !(cset & NTL_IRCUI))
        matchsel &= ~WHO_FIELD_UID;
      if ((minlen > HOSTLEN) || !(cset & NTL_IRCHN))
        matchsel &= ~WHO_FIELD_HOS;
    }

    /* First of all loop through the clients in common channels */
    if ((!(counter < 1)) && matchsel)
      for (lp = sptr->user->channel; lp; lp = lp->next)
        for (chptr = lp->value.chptr, lp2 = chptr->members; lp2;
            lp2 = lp2->next)
        {
          acptr = lp2->value.cptr;
          if (!(IsUser(acptr) && Process(acptr)))
            continue;           /* Now Process() is at the beginning, if we fail
                                   we'll never have to show this acptr in this query */

          if ((bitsel & WHOSELECT_OPER) && !(IsAnOper(acptr))
              && !(IsHelpOp(acptr)))
          {
            continue;
          }
#if defined(BDD_VIP)
          if ((mask) &&
              ((!(matchsel & WHO_FIELD_NIC))
              || matchexec(acptr->name, mymask, minlen))
              && ((!(matchsel & WHO_FIELD_UID))
              || matchexec(PunteroACadena(acptr->user->username), mymask,
              minlen)) && ((!(matchsel & WHO_FIELD_SER))
              || (!(acptr->user->server->flags & FLAGS_MAP)))
              && ((!(matchsel & WHO_FIELD_HOS))
              || (matchexec(get_visiblehost(acptr, sptr), mymask, minlen)
              && matchexec(get_visiblehost(acptr, NULL), mymask, minlen)))
              && ((!(matchsel & WHO_FIELD_REN))
              || matchexec(PunteroACadena(acptr->info), mymask, minlen))
              && ((!(matchsel & WHO_FIELD_NIP)) || (IsHidden(acptr)
              && !can_viewhost(sptr, acptr))
              || !ipmask_check(&acptr->ip, &imask, ibits)))
            continue;
#else
          if ((mask) &&
              ((!(matchsel & WHO_FIELD_NIC))
              || matchexec(acptr->name, mymask, minlen))
              && ((!(matchsel & WHO_FIELD_UID))
              || matchexec(PunteroACadena(acptr->user->username), mymask,
              minlen)) && ((!(matchsel & WHO_FIELD_SER))
              || (!(acptr->user->server->flags & FLAGS_MAP)))
              && ((!(matchsel & WHO_FIELD_HOS))
              || matchexec(acptr->user->host, mymask, minlen))
              && ((!(matchsel & WHO_FIELD_REN))
              || matchexec(PunteroACadena(acptr->info), mymask, minlen))
              && ((!(matchsel & WHO_FIELD_NIP))
              || !ipmask_check(&acptr->ip, &imask, ibits)))
            continue;
#endif
          if (!SHOW_MORE(sptr, counter))
            break;
          do_who(sptr, acptr, chptr, fields, bitsel & WHOSELECT_EXTRA, qrt);
        }

    /* Loop through all clients :-\, if we still have something to match to 
       and we can show more clients */
    if ((!(counter < 1)) && matchsel)
      for (acptr = me.prev; acptr; acptr = acptr->prev)
      {
        if (!(IsUser(acptr) && Process(acptr)))
          continue;
        if ((bitsel & WHOSELECT_OPER) && !(IsAnOper(acptr))
            && !(IsHelpOp(acptr)))
        {
          continue;
        }
        if (!(SEE_USER(sptr, acptr, bitsel)))
          continue;
#if defined(BDD_VIP)
        /* tengo ke pensar un rato en ello. 1999/09/14 savage@apostols.org */
        if ((mask) &&
            ((!(matchsel & WHO_FIELD_NIC))
            || matchexec(acptr->name, mymask, minlen))
            && ((!(matchsel & WHO_FIELD_UID))
            || matchexec(PunteroACadena(acptr->user->username), mymask, minlen))
            && ((!(matchsel & WHO_FIELD_SER))
            || (!(acptr->user->server->flags & FLAGS_MAP)))
            && ((!(matchsel & WHO_FIELD_HOS))
            || (matchexec(get_visiblehost(acptr, sptr), mymask, minlen)
            && matchexec(get_visiblehost(acptr, NULL), mymask, minlen)))
            && ((!(matchsel & WHO_FIELD_REN))
            || matchexec(PunteroACadena(acptr->info), mymask, minlen))
            && ((!(matchsel & WHO_FIELD_NIP))
            || (IsHidden(acptr) && !can_viewhost(sptr, acptr))
            || !ipmask_check(&acptr->ip, &imask, ibits)))
          continue;
#else
        if ((mask) &&
            ((!(matchsel & WHO_FIELD_NIC))
            || matchexec(acptr->name, mymask, minlen))
            && ((!(matchsel & WHO_FIELD_UID))
            || matchexec(PunteroACadena(acptr->user->username), mymask, minlen))
            && ((!(matchsel & WHO_FIELD_SER))
            || (!(acptr->user->server->flags & FLAGS_MAP)))
            && ((!(matchsel & WHO_FIELD_HOS))
            || matchexec(acptr->user->host, mymask, minlen))
            && ((!(matchsel & WHO_FIELD_REN))
            || matchexec(PunteroACadena(acptr->info), mymask, minlen))
            && ((!(matchsel & WHO_FIELD_NIP))
            || !ipmask_check(&acptr->ip, &imask, ibits))
          continue;
#endif
        if (!SHOW_MORE(sptr, counter))
          break;
        do_who(sptr, acptr, NULL, fields, bitsel & WHOSELECT_EXTRA, qrt);
      }
  }

  /* Make a clean mask suitable to be sent in the "end of" */
  if (mask && (p = strchr(mask, ' ')))
    *p = '\0';
  sendto_one(sptr, rpl_str(RPL_ENDOFWHO),
      me.name, parv[0], BadPtr(mask) ? "*" : mask);

  /* Notify the user if we decided that his query was too long */
  if (counter < 0)
    sendto_one(sptr, err_str(ERR_QUERYTOOLONG), me.name, parv[0], "WHO");

  return 0;
}

#define MAX_WHOIS_LINES 50

/*
 * m_whois
 *
 * parv[0] = sender prefix
 * parv[1] = nickname masklist
 *
 * or
 *
 * parv[1] = target server
 * parv[2] = nickname masklist
 */
int m_whois(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Reg2 Link *lp;
  Reg3 anUser *user;
  aClient *acptr, *a2cptr;
  aChannel *chptr;
  char *nick, *tmp, *name;
  char *p = NULL;
  int found, len, mlen, total;
  static char buf[512];

  if (parc < 2)
  {
    sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN), me.name, parv[0]);
    return 0;
  }

  if (parc > 2)
  {
    int hunt_result;
    aClient *acptr;
    /* For convenience: Accept a nickname as first parameter, by replacing
       it with the correct servername - as is needed by hunt_server() */
    if (MyUser(sptr) && (acptr = FindUser(parv[1])))
      parv[1] = acptr->user->server->name;
    hunt_result = hunt_server(0, cptr, sptr, MSG_WHOIS, TOK_WHOIS, "%s :%s", 1, parc, parv);
    if (hunt_result == HUNTED_NOSUCH)
      sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name, parv[0], parv[2]);
    if (hunt_result != HUNTED_ISME)
      return 0;
    parv[1] = parv[2];
  }

  total = 0;
  for (tmp = parv[1]; (nick = strtoken(&p, tmp, ",")); tmp = NULL)
  {
    int invis, showperson, member, wilds;

    found = 0;
    collapse(nick);
    wilds = (strchr(nick, '?') || strchr(nick, '*'));
    /* Do a hash lookup if the nick does not contain wilds */
    if (wilds)
    {
      /*
       * We're no longer allowing remote users to generate requests with wildcards.
       */
      if (!MyConnect(sptr))
        continue;
      for (acptr = client; (acptr = next_client(acptr, nick));
          acptr = acptr->next)
      {
        int canales_ocultos;
        int ocultacion_canales;
        if (!IsRegistered(acptr) || IsServer(acptr) || IsPing(acptr))
          continue;
        /*
         * I'm always last :-) and acptr->next == NULL!!
         */
        if (IsMe(acptr))
          break;
        /*
         * 'Rules' established for sending a WHOIS reply:
         *
         * - if wildcards are being used dont send a reply if
         *   the querier isnt any common channels and the
         *   client in question is invisible and wildcards are
         *   in use (allow exact matches only);
         *
         * - only send replies about common or public channels
         *   the target user(s) are on;
         */
        user = acptr->user;
        name = (!acptr->name) ? "?" : acptr->name;

        invis = acptr != sptr && IsInvisible(acptr);
        member = (user && user->channel) ? 1 : 0;
        showperson = (wilds && !invis && !member) || !wilds;
        if (user)
          for (lp = user->channel; lp; lp = lp->next)
          {
            chptr = lp->value.chptr;
            member = IsMember(sptr, chptr) ? 1 : 0;
            if (invis && !member)
              continue;
            if (is_zombie(acptr, chptr))
              continue;
            if (member || (!invis && PubChannel(chptr)))
            {
              showperson = 1;
              break;
            }
            if (!invis && HiddenChannel(chptr) && !SecretChannel(chptr))
              showperson = 1;
          }
        if (!showperson)
          continue;

        if (user)
        {
#if defined(BDD_VIP)
          struct db_reg *reg = db_buscar_registro(BDD_IPVIRTUALDB, name);
#endif
          a2cptr = user->server;
#if defined(BDD_VIP)
          if (IsHidden(acptr) && reg) {
              char *vhost;
              char *vhostcolor;

              /* Copio el valor en memoria para evitar que corte el registro en
               * memoria de la BDD cuando hay un ! de separacion de campos.
               */
              DupString(vhost, reg->valor);
              vhostcolor = strchr(vhost, '!');

              if (vhostcolor) {
                *vhostcolor++;
                sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
                    parv[0], name, PunteroACadena(user->username),
                    vhostcolor, PunteroACadena(acptr->info));
              } else {
                sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
                    parv[0], name, PunteroACadena(user->username),
                    vhost, PunteroACadena(acptr->info));
              }
              RunFree(vhost);
          } else
            sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
                parv[0], name, PunteroACadena(user->username),
                get_visiblehost(acptr, NULL), PunteroACadena(acptr->info));
#else
          sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
              parv[0], name, PunteroACadena(user->username), user->host,
              PunteroACadena(acptr->info));
#endif
        }
        else
        {
          a2cptr = &me;
          sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
              parv[0], name, "<unknown>", "<unknown>", "<unknown>");
        }

        found = 1;

      exact_match:
        if (user
            && (!IsNoChan(acptr) || acptr == sptr || IsHelpOp(sptr) || IsAnOper(sptr))
            )
        {
          mlen = strlen(me.name) + strlen(parv[0]) + 12 + strlen(name);
          /* Inicializamos variables para el nuevo usuario */
          ocultacion_canales = (IsServicesBot(acptr) && IsInvisible(acptr));
          canales_ocultos = 0;
          for (len = 0, *buf = '\0', lp = user->channel; lp; lp = lp->next)
          {
            chptr = lp->value.chptr;
            if (ShowChannel(sptr, chptr) &&
                (acptr == sptr || !is_zombie(acptr, chptr)))
            {
              if (ocultacion_canales && !IsMember(sptr, chptr))
              {
                canales_ocultos++;
                continue;
              }
              if (len + strlen(chptr->chname) + mlen > BUFSIZE - 5)
              {
                sendto_one(sptr, ":%s %d %s %s :%s",
                    me.name, RPL_WHOISCHANNELS, parv[0], name, buf);
                *buf = '\0';
                len = 0;
              }
              if (IsDeaf(acptr))
                *(buf + len++) = '-';
              if (is_zombie(acptr, chptr))
              {
                *(buf + len++) = '!';
              }
              else
              {
                if (is_chan_op(acptr, chptr))
                  *(buf + len++) = '@';
                if (has_voice(acptr, chptr))
                  *(buf + len++) = '+';
              }
              if (len)
                *(buf + len) = '\0';
              strcpy(buf + len, chptr->chname);
              len += strlen(chptr->chname);
              strcat(buf + len, " ");
              len++;
            }
          }
          if (canales_ocultos)
          {
            char temp[32];
            int longitud;

            sprintf(temp, "...(%d)...", canales_ocultos);
            longitud = strlen(temp);
            if (len + longitud + mlen > BUFSIZE - 5)
            {
              sendto_one(sptr, ":%s %d %s %s :%s",
                  me.name, RPL_WHOISCHANNELS, parv[0], name, buf);
              *buf = '\0';
            }
            strcat(buf, temp);
          }
          if (buf[0] != '\0')
            sendto_one(sptr, rpl_str(RPL_WHOISCHANNELS),
                me.name, parv[0], name, buf);
        }

        if (ocultar_servidores && !IsService(a2cptr) &&
            !(IsAnOper(sptr) || IsHelpOp(sptr) || (sptr == acptr)))
          sendto_one(sptr, rpl_str(RPL_WHOISSERVER), me.name,
              parv[0], name, his.name, his.info);
        else
          sendto_one(sptr, rpl_str(RPL_WHOISSERVER), me.name,
              parv[0], name, a2cptr->name, PunteroACadena(a2cptr->info));

        if (user)
        {
          if (user->away)
            sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
                parv[0], name, user->away);

          if (IsNickRegistered(acptr))
            sendto_one(sptr, rpl_str(RPL_WHOISREGNICK), me.name, parv[0], name);

          if (IsServicesBot(acptr))
            sendto_one(sptr, rpl_str(RPL_WHOISBOT), me.name, parv[0], name);

          if (IsNickSuspended(acptr))
            sendto_one(sptr, rpl_str(RPL_WHOISSUSPNICK), me.name, parv[0],
                name);

          if (IsHelpOp(acptr)) {
            if (!user->away && !IsMsgOnlyReg(acptr) && !IsAdmin(acptr) && !IsCoder(acptr)) {
              if (canal_operadores) {
                aChannel *chptr2 = FindChannel(canal_operadores);
                if (chptr2 && IsMember(acptr, chptr2))
                  sendto_one(sptr, rpl_str(RPL_WHOISHELPOP), me.name, parv[0], name);
              } else {
                 sendto_one(sptr, rpl_str(RPL_WHOISHELPOP), me.name, parv[0], name);
              }
            }
          }

          if (IsAdmin(acptr))
            sendto_one(sptr, rpl_str(RPL_WHOISOPERATOR), me.name, parv[0], name,
                "Es un ADMINistrador de los servicios de red");
          else if (IsCoder(acptr))
            sendto_one(sptr, rpl_str(RPL_WHOISOPERATOR), me.name, parv[0], name,
                "Es un Desarrollador de la red");
          else if (IsHelpOp(acptr))
            sendto_one(sptr, rpl_str(RPL_WHOISOPERATOR), me.name, parv[0], name,
                "Es un OPERador de los servicios de red");
          else if (IsAnOper(acptr))
            sendto_one(sptr, rpl_str(RPL_WHOISOPERATOR), me.name, parv[0], name,
                "Is an IRC Operator");

          if (IsMsgOnlyReg(acptr))
            sendto_one(sptr, rpl_str(RPL_MSGONLYREG), me.name, parv[0], name);

          if (IsDocking(acptr))
            sendto_one(sptr, rpl_str(RPL_DOCKING), me.name, parv[0], name);

          if (IsHidden(acptr) && (can_viewhost(sptr, acptr) || acptr == sptr))
            sendto_one(sptr, rpl_str(RPL_WHOISACTUALLY),  me.name, parv[0],
                name, user->username, user->host, ircd_ntoa_c(acptr));

          sendto_one(sptr, rpl_str(RPL_WHOISMODES), me.name,
              parv[0], name, umode_str(acptr, sptr));

         if (IsSSL(acptr))
          sendto_one(sptr, rpl_str(RPL_WHOISSSL), me.name, parv[0], name);

         if (MyConnect(acptr) && (!IsNoIdle(acptr) && !IsAnOper(sptr)) && (!ocultar_servidores ||
                  (sptr == acptr || IsAnOper(sptr) || IsHelpOp(sptr) || parc >= 3)))
            sendto_one(sptr, rpl_str(RPL_WHOISIDLE), me.name,
                parv[0], name, now - user->last, acptr->firsttime);

         if (IsWhois(acptr) && (sptr != acptr))
            sendto_one(acptr->from, ":%s NOTICE %s :%s te ha hecho un /WHOIS", me.name, acptr->name, sptr->name);
        }
        if (found == 2 || total++ >= MAX_WHOIS_LINES)
          break;
      }
    }
    else
    {
      /* No wildcards */
      if ((acptr = FindUser(nick)))
      {
#if defined(BDD_VIP)
        struct db_reg *reg = db_buscar_registro(BDD_IPVIRTUALDB, nick);
#endif
        found = 2;              /* Make sure we exit the loop after passing it once */
        user = acptr->user;
        name = (!acptr->name) ? "?" : acptr->name;
        a2cptr = user->server;

#if defined(BDD_VIP)
        if (IsHidden(acptr) && reg) {
            char *vhost;
            char *vhostcolor;

            /* Copio el valor en memoria para evitar que corte el registro en
             * memoria de la BDD cuando hay un ! de separacion de campos.
             */
            DupString(vhost, reg->valor);
            vhostcolor = strchr(vhost, '!');

            if (vhostcolor) {
              *vhostcolor++;
              sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
                  parv[0], name, PunteroACadena(user->username),
                  vhostcolor, PunteroACadena(acptr->info));
            } else {
              sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
                  parv[0], name, PunteroACadena(user->username),
                  vhost, PunteroACadena(acptr->info));
            }
            RunFree(vhost);
        } else
          sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
              parv[0], name, PunteroACadena(user->username),
              get_visiblehost(acptr, NULL), PunteroACadena(acptr->info));
#else
        sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
            parv[0], name, PunteroACadena(user->username), user->host,
            PunteroACadena(acptr->info));
#endif
        goto exact_match;
      }
    }
    if (!found)
      sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], nick);
    if (p)
      p[-1] = ',';
    if (!MyConnect(sptr) || total >= MAX_WHOIS_LINES)
      break;
  }
  sendto_one(sptr, rpl_str(RPL_ENDOFWHOIS), me.name, parv[0], parv[1]);

  return 0;
}
