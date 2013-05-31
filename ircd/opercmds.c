/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/opercmds.c
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
/** @file
 * @brief Implementation of AsLL ping helper commands.
 * @version $Id$
 */
#include "config.h"

#include "opercmds.h"
#include "class.h"
#include "client.h"
#include "ircd.h"
#include "ircd_chattr.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "listener.h"
#include "match.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_conf.h"
#include "send.h"
#include "struct.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/** Calculate current time or elapsed time.
 *
 * If neither \a sec nor \a usec are NULL, calculate milliseconds
 * elapsed since that time, and return a string containing that
 * number.
 *
 * If either \a sec or \a usec are NULL, format a timestamp containing
 * Unix timestamp and microseconds since that second (separated by
 * spaces), and return a string containing that timestamp.
 *
 * @todo This should be made into two functions.
 * @param[in] sec Either NULL or a Unix timestamp in seconds.
 * @param[in] usec Either NULL or an offset to \a sec in microseconds.
 * @return A static buffer with contents as described above.
 */
char *militime(char* sec, char* usec)
{
  struct timeval tv;
  static char timebuf[18];

  gettimeofday(&tv, NULL);
  if (sec && usec)
    sprintf(timebuf, "%ld",
        (tv.tv_sec - atoi(sec)) * 1000 + (tv.tv_usec - atoi(usec)) / 1000);
  else
    sprintf(timebuf, "%ld %ld", tv.tv_sec, tv.tv_usec);
  return timebuf;
}

/** Calculate current time or elapsed time.
 *
 * If \a start is NULL, create a timestamp containing Unix timestamp
 * and microseconds since that second (separated by a period), and
 * return a string containing that timestamp.
 *
 * Otherwise, if \a start does not contain a period, return a string
 * equal to "0".
 *
 * Otherwise, calculate milliseconds elapsed since the Unix time
 * described in \a start (in the format described above), and return a
 * string containing that number.
 *
 * @todo This should be made into two functions.
 * @param[in] start Either NULL or a Unix timestamp in
 * pseudo-floating-point format.
 * @return A static buffer with contents as described above.
 */
char *militime_float(char* start)
{
  struct timeval tv;
  static char timebuf[18];
  char *p;

  gettimeofday(&tv, NULL);
  if (start)
  {
    if ((p = strchr(start, '.')))
    {
      p++;
      sprintf(timebuf, "%ld",
          (tv.tv_sec - atoi(start)) * 1000 + (tv.tv_usec - atoi(p)) / 1000);
    }
    else
      strcpy(timebuf, "0");
  }
  else
    sprintf(timebuf, "%ld.%ld", tv.tv_sec, tv.tv_usec);
  return timebuf;
}


#if 0
/*
 * m_stats/stats_conf
 *
 * Report N/C-configuration lines from this server. This could
 * report other configuration lines too, but converting the
 * status back to "char" is a bit akward--not worth the code
 * it needs...
 *
 * Note: The info is reported in the order the server uses
 *       it--not reversed as in ircd.conf!
 */

static unsigned int report_array[18][3] = {
  {CONF_CONNECT_SERVER, RPL_STATSCLINE, 'C'},
  {CONF_CLIENT, RPL_STATSILINE, 'I'},
  {CONF_KILL, RPL_STATSKLINE, 'K'},
  {CONF_IPKILL, RPL_STATSKLINE, 'k'},
  {CONF_LEAF, RPL_STATSLLINE, 'L'},
  {CONF_OPERATOR, RPL_STATSOLINE, 'O'},
  {CONF_HUB, RPL_STATSHLINE, 'H'},
  {CONF_LOCOP, RPL_STATSOLINE, 'o'},
  {CONF_CRULEALL, RPL_STATSDLINE, 'D'},
  {CONF_CRULEAUTO, RPL_STATSDLINE, 'd'},
  {CONF_UWORLD, RPL_STATSULINE, 'U'},
  {CONF_TLINES, RPL_STATSTLINE, 'T'},
  {CONF_LISTEN_PORT, RPL_STATSPLINE, 'P'},
  {CONF_LISTEN_PORT|CONF_COOKIE_ENC, RPL_STATSPLINE, 'P'},
  {CONF_EXCEPTION, RPL_STATSELINE, 'E'},
#if defined(ESNET_NEG)
  {CONF_NEGOTIATION, RPL_STATSKLINE, 'F'},
  {CONF_NEGOTIATION, RPL_STATSKLINE, 'f'},
#endif
  {0, 0}
};

static void report_configured_links(struct Client *sptr, int mask)
{
  static char null[] = "<NULL>";
  aConfItem *tmp;
  unsigned int *p;
  unsigned short int port;
  char c, *host, *pass, *name;

  for (tmp = conf; tmp; tmp = tmp->next)
    if ((tmp->status & mask))
    {
      for (p = &report_array[0][0]; *p; p += 3)
        if (*p == tmp->status)
          break;
      if (!*p)
        continue;
      c = (char)*(p + 2);
      host = BadPtr(tmp->host) ? null : tmp->host;
      pass = BadPtr(tmp->passwd) ? null : tmp->passwd;
      name = BadPtr(tmp->name) ? null : tmp->name;
      port = tmp->port;
      /*
       * On K line the passwd contents can be
       * displayed on STATS reply.    -Vesa
       */
      /* Special-case 'k' or 'K' lines as appropriate... -Kev */
      if ((tmp->status & CONF_KLINE))
        sendto_one(sptr, rpl_str(p[1]), me.name,
            sptr->name, c, host, pass, name, port, get_conf_class(tmp));
      /* connect rules are classless */
      else if ((tmp->status & CONF_CRULE))
        sendto_one(sptr, rpl_str(p[1]), me.name, sptr->name, c, host, name);
      else if ((tmp->status & CONF_TLINES))
        sendto_one(sptr, rpl_str(p[1]), me.name, sptr->name, c, host, pass);
      else if ((tmp->status & CONF_LISTEN_PORT))
        sendto_one(sptr, rpl_str(p[1]), me.name, sptr->name, c, port,
            tmp->clients, tmp->status);
      else if ((tmp->status & CONF_UWORLD))
        sendto_one(sptr, rpl_str(p[1]),
            me.name, sptr->name, c, host, "*", name, port, get_conf_class(tmp));
      else if ((tmp->status & CONF_CONNECT_SERVER))
        sendto_one(sptr, rpl_str(p[1]), me.name, sptr->name, c, "*", name,
            port, get_conf_class(tmp));
      else if ((tmp->status & (CONF_EXCEPTION)))
        sendto_one(sptr, rpl_str(p[1]), me.name, sptr->name, c, host, name,
            port, get_conf_class(tmp));
#if defined(ESNET_NEG)
      else if ((tmp->status & CONF_NEGOTIATION))
        sendto_one(sptr, rpl_str(p[1]), me.name, sptr->name, c, host, pass,
            name, port, get_conf_class(tmp));
#endif
      else
        sendto_one(sptr, rpl_str(p[1]), me.name, sptr->name, c, host, name,
            port, get_conf_class(tmp));
    }
  return;
}

/*
 * m_stats
 *
 *    parv[0] = sender prefix
 *    parv[1] = statistics selector (defaults to Message frequency)
 *    parv[2] = target server (current server defaulted, if omitted)
 * And 'stats l' and 'stats' L:
 *    parv[3] = server mask ("*" defaulted, if omitted)
 * Or for stats p,P:
 *    parv[3] = port mask (returns p-lines when its port is matched by this)
 * Or for stats k,K,i and I:
 *    parv[3] = [user@]host.name  (returns which K/I-lines match this)
 *           or [user@]host.mask  (returns which K/I-lines are mmatched by this)
 *              (defaults to old reply if ommitted, when local or Oper)
 *              A remote mask (something containing wildcards) is only
 *              allowed for IRC Operators.
 * Or for stats M:
 *    parv[3] = time param
 *    parv[4] = time param
 *    (see report_memleak_stats() in runmalloc.c for details)
 *
 * This function is getting really ugly. -Ghostwolf
 */
int m_stats(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  static char Sformat[] =
      ":%s %d %s Connection SendQ SendM SendKBytes SendComp "
      "RcveM RcveKBytes RcveComp :Open since";
  static char Lformat[] = ":%s %d %s %s %u %u %u %u%% %u %u %u%% :" TIME_T_FMT;
  aMessage *mptr;
  struct Client *acptr;
  aGline *agline, *a2gline;
  aConfItem *aconf;
  unsigned char stat = parc > 1 ? parv[1][0] : '\0';
  Reg1 int i;

#ifdef HISPANO_WEBCHAT
  if (!IsAnOper(sptr))
    return 0;
#endif

  /* Solo ircops y opers tienen acceso a hacer stats remotos */
  if (parc > 2 && MyUser(sptr) && !IsAnOper(sptr) && !IsHelpOp(sptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }

/* m_stats is so obnoxiously full of special cases that the different
 * hunt_server() possiblites were becoming very messy. It now uses a
 * switch() so as to be easier to read and update as params change.
 * -Ghostwolf
 */
  switch (stat)
  {
      /* open to all, standard # of params */
    case 'U':
    case 'u':
    {
      if (hunt_server(0, cptr, sptr, MSG_STATS, TOK_STATS, "%s :%s", 2, parc, parv)
          != HUNTED_ISME)
        return 0;
      break;
    }

      /* open to all, varying # of params */
    case 'k':
    case 'K':
    case 'E':
    case 'e':
    case 'i':
    case 'I':
    case 'p':
    case 'P':
    {
      if (parc > 3)
      {
        if (hunt_server(0, cptr, sptr, MSG_STATS, TOK_STATS, "%s %s :%s", 2, parc, parv)
            != HUNTED_ISME)
          return 0;
      }
      else
      {
        if (hunt_server(0, cptr, sptr, MSG_STATS, TOK_STATS, "%s :%s", 2, parc, parv)
            != HUNTED_ISME)
          return 0;
      }
      break;
    }

      /* oper only, varying # of params */
    case 'l':
    case 'L':
    case 'M':
    {
      if (parc == 4)
      {
        if (hunt_server(1, cptr, sptr, MSG_STATS, TOK_STATS, "%s %s :%s", 2, parc, parv)
            != HUNTED_ISME)
          return 0;
      }
      else if (parc > 4)
      {
        if (hunt_server(1, cptr, sptr, MSG_STATS, TOK_STATS, "%s %s %s :%s", 2, parc,
            parv) != HUNTED_ISME)
          return 0;
      }
      else if (hunt_server(1, cptr, sptr, MSG_STATS, TOK_STATS, "%s :%s", 2, parc, parv)
          != HUNTED_ISME)
        return 0;
      break;
    }

      /* oper only, standard # of params */
    default:
    {
      if (hunt_server(1, cptr, sptr, MSG_STATS, TOK_STATS, "%s :%s", 2, parc, parv)
          != HUNTED_ISME)
        return 0;
      break;
    }
  }

  switch (stat)
  {
    case 'L':
    case 'l':
    {
      int doall = 0, wilds = 0;
      char *name = "*";

      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }

      if (parc > 3 && *parv[3])
      {
        char *p;
        name = parv[3];
        wilds = (*name == '*' || *name == '?');
        for (p = name + 1; *p; ++p)
          if ((*p == '*' || *p == '?') && p[-1] != '\\')
          {
            wilds = 1;
            break;
          }
      }
      else
        doall = 1;
      /*
       * Send info about connections which match, or all if the
       * mask matches me.name.  Only restrictions are on those who
       * are invisible not being visible to 'foreigners' who use
       * a wild card based search to list it.
       */
      sendto_one(sptr, Sformat, me.name, RPL_STATSLINKINFO, parv[0]);
      for (i = 0; i <= highest_fd; i++)
      {
        if (!(acptr = loc_clients[i]))
          continue;
        /* Don't return clients when this is a request for `all' */
        if (doall && IsUser(acptr))
          continue;
        /* Don't show invisible people to unauthorized people when using
         * wildcards  -- Is this still needed now /stats is oper only ? */
        if (IsInvisible(acptr) && (doall || wilds) &&
            !(MyConnect(sptr) && IsOper(sptr)) &&
            !IsAnOper(acptr) && (acptr != sptr))
          continue;
        /* Only show the ones that match the given mask - if any */
        if (!doall && wilds && match(name, PunteroACadena(acptr->name)))
          continue;
        /* Skip all that do not match the specific query */
        if (!(doall || wilds) && strCasediff(name, PunteroACadena(acptr->name)))
          continue;
        sendto_one(sptr, Lformat, me.name, RPL_STATSLINKINFO, parv[0],
            acptr->name ? acptr->name : "*",
            (int)DBufLength(&acptr->cli_connect->sendQ), (int)acptr->cli_connect->sendM,
            (int)acptr->cli_connect->sendK,
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
            (IsServer(acptr) && (acptr->cli_connect->negociacion & ZLIB_ESNET_OUT)) ?
            (int)((acptr->cli_connect->comp_out_total_out * 100.0 /
            acptr->cli_connect->comp_out_total_in) + 0.5) : 100,
#else
            100,
#endif
            (int)acptr->cli_connect->receiveM, (int)acptr->cli_connect->receiveK,
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
            (IsServer(acptr) && (acptr->cli_connect->negociacion & ZLIB_ESNET_IN)) ?
            (int)((acptr->cli_connect->comp_in_total_in * 100.0 /
            acptr->cli_connect->comp_in_total_out) + 0.5) : 100,
#else
            100,
#endif
            time(NULL) - cli_firsttime(acptr));
      }
      break;
    }
    case 'C':
    case 'c':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      report_configured_links(sptr, CONF_CONNECT_SERVER);
      break;
    case 'G':
    case 'g':
    {
      int longitud;
      char buf[MAXLEN * 2];
      char comtemp[MAXLEN * 2];

      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }

      /* send glines */
      for (agline = gline, a2gline = NULL; agline; agline = agline->next)
      {
        if (agline->expire <= TStime())
        {                       /* handle expired glines */
          free_gline(agline, a2gline);
          agline = a2gline ? a2gline : gline; /* make sure to splice
                                                 list together */
          if (!agline)
            break;              /* last gline; break out of loop */
          continue;             /* continue! */
        }

        /*
         * Comprobacion longitud de gline
         */
        strcpy(comtemp, agline->reason);
        buf[0] = '\0';
        sprintf(buf, ":%s %d %s G %s@%s " TIME_T_FMT " :%s (expires at %s)",
            me.name, RPL_STATSGLINE, sptr->name, agline->name, agline->host,
            agline->expire, comtemp, date(agline->expire));

        longitud = strlen(buf);
        if (longitud > MAXLEN)
        {
          /* Truncamos el comentario */
          comtemp[strlen(agline->reason) - (longitud - MAXLEN)] = '\0';
        }

        sendto_one(sptr, rpl_str(RPL_STATSGLINE), me.name,
            sptr->name, 'G', agline->name, agline->host,
            agline->expire, comtemp, date(agline->expire));
        a2gline = agline;
      }
      break;
    }
    case 'E':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      report_configured_links(sptr, CONF_EXCEPTION);
      {
        struct db_reg *reg;

        for (reg = db_iterador_init(BDD_EXCEPTIONDB); reg;
            reg = db_iterador_next())
        { /* Mando con una e minuscula los que estan en BDD */
          sendto_one(sptr, rpl_str(RPL_STATSELINE), me.name, sptr->name, 'E',
              reg->clave, reg->valor, 0, -1);
        }
      }
      break;
    case 'e':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      sendto_one(sptr, rpl_str(RPL_STATSENGINE), me.name, sptr->name, event_get_method());
      break;
#if defined(ESNET_NEG)
    case 'f':
    case 'F':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      report_configured_links(sptr, CONF_NEGOTIATION);
      break;
#endif

    case 'H':
    case 'h':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      report_configured_links(sptr, CONF_HUB | CONF_LEAF);
      break;
    case 'I':
    case 'i':
    case 'K':
    case 'k':                  /* display CONF_IPKILL as well
                                   as CONF_KILL -Kev */
    {
      int wilds, count;
      char *user, *host, *p;
      int conf_status = (stat == 'k' || stat == 'K') ? CONF_KLINE : CONF_CLIENT;

      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }

      if ((MyUser(sptr) || IsOper(sptr)) && parc < 4)
      {
        report_configured_links(sptr, conf_status);
        break;
      }
      if (parc < 4 || *parv[3] == '\0')
      {
        sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
            (conf_status & CONF_KLINE) ? "STATS K" : "STATS I");
        return 0;
      }
      wilds = 0;
      for (p = parv[3]; *p; p++)
      {
        if (*p == '\\')
        {
          if (!*++p)
            break;
          continue;
        }
        if (*p == '?' || *p == '*')
        {
          wilds = 1;
          break;
        }
      }
      if (!(MyConnect(sptr) || IsOper(sptr)))
      {
        wilds = 0;
        count = 3;
      }
      else
        count = 1000;

      if (conf_status == CONF_CLIENT)
      {
        user = NULL;            /* Not used, but to avoid compiler warning. */

        host = parv[3];
      }
      else
      {
        if ((host = strchr(parv[3], '@')))
        {
          user = parv[3];
          *host++ = 0;;
        }
        else
        {
          user = NULL;
          host = parv[3];
        }
      }
      for (aconf = conf; aconf; aconf = aconf->next)
      {
        if ((aconf->status & conf_status))
        {
          if (conf_status == CONF_KLINE)
          {
            if ((!wilds && ((user || aconf->host[1]) &&
                !match(aconf->host, host) &&
                (!user || !match(aconf->name, user)))) ||
                (wilds && !mmatch(host, aconf->host) &&
                (!user || !mmatch(user, aconf->name))))
            {
              sendto_one(sptr, rpl_str(RPL_STATSKLINE), me.name,
                  sptr->name, 'K', aconf->host, aconf->passwd, aconf->name,
                  aconf->port, get_conf_class(aconf));
              if (--count == 0)
                break;
            }
          }
          else if (conf_status == CONF_CLIENT)
          {
            if ((!wilds && (!match(aconf->host, host) ||
                !match(aconf->name, host))) ||
                (wilds && (!mmatch(host, aconf->host) ||
                !mmatch(host, aconf->name))))
            {
              sendto_one(sptr, rpl_str(RPL_STATSILINE), me.name,
                  sptr->name, 'I', aconf->host, aconf->name,
                  aconf->port, get_conf_class(aconf));
              if (--count == 0)
                break;
            }
          }
        }
      }
      break;
    }
    case 'M':
#if defined(MEMSIZESTATS)
//      sendto_one(sptr, rpl_str(RPL_STATMEMTOT),
  //        me.name, parv[0], get_mem_size(), get_alloc_cnt());
#endif
#if defined(MEMLEAKSTATS)
    //  report_memleak_stats(sptr, parc, parv);
#endif
#if !defined(MEMSIZESTATS) && !defined(MEMLEAKSTATS)
      sendto_one(sptr, ":%s NOTICE %s :stats M : Memory allocation monitoring "
          "is not enabled on this server", me.name, parv[0]);
#endif
      break;
    case 'm':
      for (mptr = msgtab; mptr->cmd; mptr++)
        if (mptr->count)
          sendto_one(sptr, rpl_str(RPL_STATSCOMMANDS),
              me.name, parv[0], mptr->cmd, mptr->count, mptr->bytes);
      break;
    case 'o':
    case 'O':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      report_configured_links(sptr, CONF_OPS);
      break;
    case 'p':
    case 'P':
    {
      int count = 100;
      char port[6];
      if ((MyUser(sptr) || IsOper(sptr)) && parc < 4)
      {
        report_configured_links(sptr, CONF_LISTEN_PORT);
        break;
      }

      if (!(MyConnect(sptr) || IsOper(sptr)))
        count = 3;
      for (aconf = conf; aconf; aconf = aconf->next) {
        if (IsConfListenPort(aconf))
        {
          if (parc >= 4 && *parv[3] != '\0')
          {
            sprintf_irc(port, "%u", aconf->port);
            if (match(parv[3], port))
              continue;
          }
          sendto_one(sptr, rpl_str(RPL_STATSPLINE), me.name, sptr->name, 'P',
              aconf->port, aconf->clients, aconf->status);
          if (--count == 0)
            break;
        }

      }
      break;
    }
    case 'R':
    case 'r':
#if defined(DEBUGMODE)
      send_usage(sptr, parv[0]);
#endif
      break;
    case 'D':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      report_configured_links(sptr, CONF_CRULEALL);
      break;
    case 'd':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      report_configured_links(sptr, CONF_CRULE);
      break;
    case 't':
      tstats(sptr, parv[0]);
      break;
    case 'T':
      report_configured_links(sptr, CONF_TLINES);
      break;
    case 'U':
      report_configured_links(sptr, CONF_UWORLD);
      {
        struct db_reg *reg;

        for (reg = db_iterador_init(BDD_UWORLDDB); reg;
            reg = db_iterador_next())
        {
          sendto_one(sptr, rpl_str(RPL_STATSULINE), me.name, sptr->name, 'U',
              reg->clave, "<NULL>", "(Base de Datos Distribuida)", 0, -1);
        }
      }
      break;
    case 'u':
    {
      time_t nowr;

      nowr = now - cli_since(&me);
      sendto_one(sptr, rpl_str(RPL_STATSUPTIME), me.name, parv[0],
          nowr / 86400, (nowr / 3600) % 24, (nowr / 60) % 60, nowr % 60);
      sendto_one(sptr, rpl_str(RPL_STATSCONN), me.name, parv[0],
          max_connection_count, max_client_count);
      break;
    }
    case 'W':
    case 'w':
      calc_load(sptr);
      break;
    case 'X':
    case 'x':
#if defined(DEBUGMODE)
      send_listinfo(sptr, parv[0]);
#endif
      break;
    case 'Y':
    case 'y':
      report_classes(sptr);
      break;
    case 'Z':
    case 'z':
      count_memory(sptr, parv[0]);
      break;
    case 'B':
    case 'b':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
#if defined(BDD_MMAP)
      struct persistent_mallinfo m;

      m = persistent_mallinfo();
      sendto_one(sptr,
        ":%s %d %s %c :Cache %s - Arena: %u - Usados: %u - Max: %u  de %u - KeepCost: %u",
        me.name, RPL_STATSDEBUG, parv[0], stat,
        db_persistent_hit()? "HIT" : "MISS", m.arena, m.uordblks, m.usmblks,
        BDD_MMAP_SIZE * 1024u * 1024u, m.keepcost);
#endif
      for (i = ESNET_BDD; i <= ESNET_BDD_END; i++)
      {
        if (db_es_residente(i))
        {
          sendto_one(sptr, ":%s %d %s %c Tabla '%c' :S=%lu R=%lu",
            me.name, RPL_STATSDEBUG, parv[0], stat,
            i, (unsigned long)db_num_serie(i), (unsigned long)db_cuantos(i));
        }
        else
        {
          if (db_num_serie(i))
          {
            sendto_one(sptr, ":%s %d %s %c Tabla '%c' :S=%lu NoResidente",
              me.name, RPL_STATSDEBUG, parv[0], stat, i,
              (unsigned long)db_num_serie(i));
          }
        }
      }
      break;
    case 'J':
    case 'j':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      {
        struct db_reg *reg;

        for (reg = db_iterador_init(BDD_JUPEDB); reg;
             reg = db_iterador_next())
        {
          sendto_one(sptr, rpl_str(RPL_STATSJLINE), me.name, sptr->name, reg->clave, reg->valor);
        }
      }
      break;
    default:
      stat = '*';
      break;
  }
  sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, parv[0], stat);
  return 0;
}



static void add_gline(struct Client *cptr, struct Client *sptr, int ip_mask, char *host, char *comment,
    char *user, time_t expire, time_t lastmod, time_t lifetime, int local)
{

  struct Client *acptr;
  aGline *agline;
  int fd, gtype = 0;
  char cptr_info_low[REALLEN+1];
  char *tmp;

#if defined(BADCHAN)
  if (*host == '#' || *host == '&' || *host == '+')
    gtype = 1;                  /* BAD CHANNEL */
#endif

  if (!lifetime) /* si no me ponen tiempo de vida uso el tiempo de expiracion */
    lifetime = expire;

  if((!IsHub(sptr) || !buscar_uline(cli_confs(cptr), sptr->name)) && !lastmod) /* Si no es hub y no tiene uline y me pasan ultima mod 0 salgo */
    return;

  /* Inform ops */
  if(!IsBurstOrBurstAck(sptr))
    sendto_op_mask(SNO_GLINE,
        "%s adding %s%s for %s@%s, expiring at " TIME_T_FMT " (%s): %s",
        sptr->name, local ? "local " : "", gtype ? "BADCHAN" : "GLINE",
        user, host, expire, date(expire), comment);

#if defined(GPATH)
  write_log(GPATH,
      "# " TIME_T_FMT " %s adding %s %s for %s@%s, expiring at " TIME_T_FMT
      ": %s\n", TStime(), sptr->name, local ? "local" : "global",
      gtype ? "BADCHAN" : "GLINE", user, host, expire, comment);

  /* this can be inserted into the conf */
  if (!gtype)
    write_log(GPATH, "%c:%s:%s:%s\n", ip_mask ? 'k' : 'K', host, comment, user);
#endif /* GPATH */

  if(!lastmod)
    lastmod = TStime();

  if (!lifetime) /* si no me ponen tiempo de vida uso el tiempo de expiracion */
    lifetime = expire;

  agline = make_gline(ip_mask, host, comment, user, expire, lastmod, lifetime);
  if (local)
    SetGlineIsLocal(agline);

#if defined(BADCHAN)
  if (gtype)
    return;
#endif

  for (fd = highest_fd; fd >= 0; --fd)  /* get the users! */
    if ((acptr = loc_clients[fd]) && !IsMe(acptr))
    {

      if (!acptr->cli_user || (acptr->cli_connect->sockhost
          && strlen(acptr->cli_connect->sockhost) > (size_t)HOSTLEN)
          || (acptr->cli_user->username ? strlen(acptr->cli_user->username) : 0) >
          (size_t)HOSTLEN)
        continue;               /* these tests right out of
                                   find_kill for safety's sake */

      if(find_exception(acptr)) /* Si hay una excepcion me lo salto */
        continue;
      
      if(GlineIsRealNameCI(agline)) {
        /* Paso el realname a minusculas para matcheo en pcre */
        strncpy(cptr_info_low, PunteroACadena(acptr->info), REALLEN);
        cptr_info_low[REALLEN]='\0';
        
        tmp=cptr_info_low;
        
        while (*tmp) {
          *tmp=ToLower(*tmp);
          *tmp++;
        }
        
        tmp=cptr_info_low;
      } else if(GlineIsRealName(agline))
        tmp=PunteroACadena(acptr->info);

      if ((GlineIsIpMask(agline) ? match(agline->host, ircd_ntoa(client_addr(acptr))) :
          (GlineIsRealName(agline) ? match_pcre(agline->re, tmp) :
            match(agline->host, PunteroACadena(acptr->cli_connect->sockhost)))) == 0 &&
            match(agline->name, PunteroACadena(acptr->cli_user->username)) == 0)
      {
        int longitud;
        char buf[MAXLEN * 2];
        char comtemp[MAXLEN * 2];

        /*
         * Comprobacion longitud de gline
         */
        strcpy(comtemp, comment);
        buf[0] = '\0';
        sprintf(buf, ":%s %d %s " TIME_T_FMT " :%s (expires at %s).",
            me.name, ERR_YOUREBANNEDCREEP, PunteroACadena(acptr->name), agline->expire,
            comtemp, date(agline->expire));

        longitud = strlen(buf);
        if (longitud > MAXLEN)
        {
          /* Truncamos el comentario */
          comtemp[strlen(comment) - (longitud - MAXLEN)] = '\0';
        }

        /* ok, he was the one that got G-lined */
        sendto_one(acptr, ":%s %d %s " TIME_T_FMT " :%s (expires at %s).",
            me.name, ERR_YOUREBANNEDCREEP, PunteroACadena(acptr->name), agline->expire,
            comtemp, date(agline->expire));

        /* let the ops know about my first kill */
        sendto_op_mask(SNO_GLINE, "G-line active for %s",
            get_client_name(acptr, FALSE));

        /* and get rid of him */
        if (sptr != acptr)
          exit_client_msg(cli_form(sptr), acptr, &me, "G-lined (%s)",
              agline->reason);
      }
    }
}

/*
 * m_gline
 *
 * parv[0] = Send prefix
 *
 * From server:
 *
 * parv[1] = Target: server numeric
 * parv[2] = [+|-]<G-line mask>
 * parv[3] = Expiration offset
 * parv[4] = G-line last modification
 * parv[5] = G-line lifetime
 * parv[parc - 1] = Comment
 *
 * From client:
 *
 * parv[1] = [+|-]<G-line mask>
 * parv[2] = Expiration offset
 * parv[3] = Comment
 *
 */
int m_gline(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Client *acptr = NULL;        /* Init. to avoid compiler warning. */

  aGline *agline, *a2gline;
  char *user, *host;
  int active, ip_mask, gtype = 0;
  time_t expire = 0, lastmod = 0, lifetime = 0;

  /* Remove expired G-lines */
  for (agline = gline, a2gline = NULL; agline; agline = agline->next)
  {
    if (agline->expire <= TStime())
    {
      free_gline(agline, a2gline);
      agline = a2gline ? a2gline : gline;
      if (!agline)
        break;
      continue;
    }
    a2gline = agline;
  }

#if defined(BADCHAN)
  /* Remove expired bad channels */
  for (agline = badchan, a2gline = NULL; agline; agline = agline->next)
  {
    if (agline->expire <= TStime())
    {
      free_gline(agline, a2gline);
      agline = a2gline ? a2gline : badchan;
      if (!agline)
        break;
      continue;
    }
    a2gline = agline;
  }
#endif


  if (IsServer(cptr)) /* Si la gline la manda un servidor */
    ms_gline(cptr, sptr, agline, a2gline, parc, parv);
  else if (IsUser(sptr) && IsAnOper(sptr))
    ms_gline(cptr, sptr, agline, a2gline, parc, parv);
  else
    mo_gline(cptr, sptr, agline, a2gline, parc, parv);

  return 0;
}

/*
 * ms_gline
 *
 * parv[0] = Send prefix
 *
 * parv[1] = Target: server numeric
 * parv[2] = [+|-]<G-line mask>
 * parv[3] = Expiration offset
 * parv[4] = G-line last modification
 * parv[parc - 1] = Comment
 *
 */
int ms_gline(struct Client *cptr, struct Client *sptr, aGline *agline, aGline *a2gline, int parc, char *parv[])
{
  char *user, *host;
  int active, ip_mask, gtype = 0;
  time_t expire = 0, lastmod = 0, lifetime = 0;
  int tiene_uline;

  if(parc>5)
    lastmod = atoi(parv[4]);

  if(parc>6)
    lifetime = atoi(parv[5]);

  if (parc < 3 || (*parv[2] != '-' && (parc < 5 || *parv[parc - 1] == '\0')))
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
        "GLINE");
    return 0;
  }

  if (*parv[2] == '-')      /* add mode or delete mode? */
    active = 0;
  else
    active = 1;

  if (*parv[2] == '+' || *parv[2] == '-')
    parv[2]++;              /* step past mode indicator */

  tiene_uline=buscar_uline(cli_confs(cptr), sptr->name);
  
  if(!tiene_uline) /* Si no tiene uline */
  {
    if(!IsHub(sptr) || !lastmod || !IsBurstOrBurstAck(sptr)) /* y no es hub o me pasan ultima mod 0 o no estoy en burst */ 
      return 0; /* salgo */
  }
  else {
    /* El usuario puede especificar una gline de nick, que no tiene @ ni . (IPv4) ni : (IPv6) */
    if (IsUser(sptr)) 
    {
      if (!((strchr(parv[2], '@')) || (strchr(parv[2], '.')) || (strchr(parv[2], ':'))))
      {
        struct Client *acptr;

        acptr = FindClient(parv[2]);
        /* Encontrado, ahora a cambiar por IP */
        if (acptr)
          parv[2] = ircd_ntoa_c(acptr);
      }
    }

    if(!propaga_gline(cptr, sptr, active, expire, lastmod, lifetime, parc, parv))
      return 0;

  }
  

  if (!(host = strchr(parv[2], '@')))
  {                         /* convert user@host */
    user = "*";             /* no @'s; assume username is '*' */
    host = parv[2];
  }
  else
  {
    user = parv[2];
    *(host++) = '\0';       /* break up string at the '@' */
  }
  ip_mask = check_if_ipmask(host);  /* Store this boolean */
#if defined(BADCHAN)
  if (*host == '#' || *host == '&' || *host == '+')
    gtype = 1;              /* BAD CHANNEL GLINE */
#endif

  for (agline = (gtype) ? badchan : gline, a2gline = NULL; agline;
      agline = agline->next)
  {
    if (!strCasediff(agline->name, user)
        && ((GlineIsRealName(agline) && !strcmp(agline->host, host)) ||
            (!GlineIsRealName(agline) && !strCasediff(agline->host, host)))
       ) /* No chequeo casediff por si es pcre */
      break;
    a2gline = agline;
  }

  if (!active && agline)
  {                         /* removing the gline */
    if(!tiene_uline)
      return 0;

    /* notify opers */
    sendto_op_mask(SNO_GLINE, "%s removing %s for %s@%s", parv[0],
        gtype ? "BADCHAN" : "GLINE", agline->name, agline->host);

#if defined(GPATH)
    write_log(GPATH, "# " TIME_T_FMT " %s removing %s for %s@%s\n",
        TStime(), parv[0], gtype ? "BADCHAN" : "GLINE", agline->name,
        agline->host);
#endif /* GPATH */

    free_gline(agline, a2gline);  /* remove the gline */
  }
  else if (active)
  {                         /* must be adding a gline */
    expire = atoi(parv[3]) + TStime();  /* expire time? */
    if (agline)
    {                       /* modifico gline; new expire time? */
      modifica_gline(cptr, sptr, agline, gtype, expire, lastmod, lifetime, parv[0]);
    }
    else if (!agline)
    {                       /* create gline */
/* Elimino el chequeo de gline overlappeada, consume mucha CPU */
#if 0
      for (agline = gtype ? badchan : gline; agline; agline = agline->next)
          if (!mmatch(agline->name, user) &&
              (ip_mask ? GlineIsIpMask(agline) : !GlineIsIpMask(agline)) &&
              !mmatch(agline->host, host))
            return 0;         /* found an existing G-line that matches */
#endif
        /* add the line: */
        add_gline(cptr, sptr, ip_mask, host, parv[parc - 1], user, expire, lastmod, lifetime, 0);
    }
  }

  return 0;
}

/*
 * mo_gline
 *
 * parv[0] = Send prefix
 *
 * parv[1] = [+|-]<G-line mask>
 * parv[2] = Expiration offset
 * parv[3] = Comment
 *
 */
int mo_gline(struct Client *cptr, struct Client *sptr, aGline *agline, aGline *a2gline, int parc, char *parv[])
{
  char *user, *host;
  int active, ip_mask, gtype = 0;
  time_t expire = 0, lastmod = 0, lifetime = 0;

  if (parc < 2 || *parv[1] == '\0')
  {
    int longitud;
    char buf[MAXLEN * 2];
    char comtemp[MAXLEN * 2];

    /* Solo ircops y opers tienen acceso */
    if (!IsAnOper(sptr) && !IsHelpOp(sptr))
    {
      sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
      return 0;
    }
    /* Not enough args and a user; list glines */
    for (agline = gline; agline; agline = agline->next)
    {
      /*
       * Comprobacion longitud de gline
       */

      strcpy(comtemp, agline->reason);
      buf[0] = '\0';
      sprintf(buf, ":%s %d %s %s@%s " TIME_T_FMT " :%s%s (expires at %s)",
          me.name, RPL_GLIST, parv[0], agline->name, agline->host,
          agline->expire, comtemp,
          GlineIsActive(agline) ? (GlineIsLocal(agline) ? " (local)" : "") :
          " (Inactive)", date(agline->expire));

      longitud = strlen(buf);
      if (longitud > MAXLEN)
      {
        /* Truncamos el comentario */
        comtemp[strlen(agline->reason) - (longitud - MAXLEN)] = '\0';
      }

      sendto_one(cptr, rpl_str(RPL_GLIST), me.name, parv[0],
          agline->name, agline->host, agline->expire, comtemp,
          GlineIsActive(agline) ? (GlineIsLocal(agline) ? " (local)" : "") :
          " (Inactive)", date(agline->expire));
    }
    sendto_one(cptr, rpl_str(RPL_ENDOFGLIST), me.name, parv[0]);
  }
  else
  {
    int priv;

#if defined(LOCOP_LGLINE)
    priv = IsAnOper(cptr);
#else
    priv = IsOper(cptr);
#endif

    if (priv)
    {                           /* non-oper not permitted to change things */
      if (*parv[1] == '-')
      {                         /* oper wants to deactivate the gline */
        active = 0;
        parv[1]++;
      }
      else if (*parv[1] == '+')
      {                         /* oper wants to activate inactive gline */
        active = 1;
        parv[1]++;
      }
      else
        active = -1;

      if (parc > 2)
        expire = atoi(parv[2]) + TStime();  /* oper wants to reset
                                               expire TS */
    }
    else
      active = -1;

    /* Solo ircops y opers tienen acceso */
    if (!IsAnOper(sptr) && !IsHelpOp(sptr))
    {
      sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
      return 0;
    }

    if (!(host = strchr(parv[1], '@')))
    {
      user = "*";               /* no @'s; assume username is '*' */
      host = parv[1];
    }
    else
    {
      user = parv[1];
      *(host++) = '\0';         /* break up string at the '@' */
    }
    ip_mask = check_if_ipmask(host);  /* Store this boolean */
#if defined(BADCHAN)
    if (*host == '#' || *host == '&' || *host == '+')
#if !defined(LOCAL_BADCHAN)
      return 0;
#else
      gtype = 1;                /* BAD CHANNEL */
#endif
#endif

    for (agline = gtype ? badchan : gline, a2gline = NULL; agline;
        agline = agline->next)
    {
      if (!mmatch(agline->name, user) &&
          (ip_mask ? GlineIsIpMask(agline) : !GlineIsIpMask(agline)) &&
          !mmatch(agline->host, host))
        break;
      a2gline = agline;
    }

    if (!agline)
    {
#if defined(OPER_LGLINE)
      if (priv && active && expire > now)
      {
        /* Add local G-line */
        if (parc < 4 || !strchr(parv[3], ' '))
        {
          sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
              me.name, parv[0], "GLINE");
          return 0;
        }
        add_gline(cptr, sptr, ip_mask, host, parv[3], user, expire, lastmod, lifetime, 1);
      }
      else
#endif
        sendto_one(cptr, err_str(ERR_NOSUCHGLINE), me.name, parv[0], user,
            host);

      return 0;
    }

    if (expire <= agline->expire)
      expire = 0;

    if ((active == -1 ||
        (active ? GlineIsActive(agline) : !GlineIsActive(agline))) &&
        expire == 0)
    {
      int longitud;
      char buf[MAXLEN * 2];
      char comtemp[MAXLEN * 2];

      /*
       * Comprobacion longitud de gline
       */
      strcpy(comtemp, agline->reason);
      buf[0] = '\0';
      sprintf(buf, ":%s %d %s %s@%s " TIME_T_FMT " :%s%s (expires at %s).",
          me.name, RPL_GLIST, parv[0], agline->name, agline->host,
          agline->expire, comtemp,
          GlineIsActive(agline) ? "" : " (Inactive)", date(agline->expire));

      longitud = strlen(buf);
      if (longitud > MAXLEN)
      {
        /* Truncamos el comentario */
        comtemp[strlen(agline->reason) - (longitud - MAXLEN)] = '\0';
      }

      /* oper wants a list of one gline only */
      sendto_one(cptr, rpl_str(RPL_GLIST), me.name, parv[0], agline->name,
          agline->host, agline->expire, comtemp,
          GlineIsActive(agline) ? "" : " (Inactive)", date(agline->expire));

      sendto_one(cptr, rpl_str(RPL_ENDOFGLIST), me.name, parv[0]);
      return 0;
    }

    if (active != -1 &&
        (active ? !GlineIsActive(agline) : GlineIsActive(agline)))
    {
      if (active)               /* reset activation on gline */
        SetActive(agline);
#if defined(OPER_LGLINE)
      else if (GlineIsLocal(agline))
      {
        /* Remove local G-line */
        sendto_op_mask(SNO_GLINE, "%s removed local %s for %s@%s",
            parv[0], gtype ? "BADCHAN" : "GLINE", agline->name, agline->host);
#if defined(GPATH)
        write_log(GPATH, "# " TIME_T_FMT
            " %s!%s@%s removed local %s for %s@%s\n",
            TStime(), parv[0], PunteroACadena(cptr->cli_user->username),
            cptr->cli_user->host, gtype ? "BADCHAN" : "GLINE", agline->name,
            agline->host);
#endif /* GPATH */
        free_gline(agline, a2gline);  /* remove the gline */
        return 0;
      }
#endif
      else
        ClearActive(agline);
    }
    else
      active = -1;              /* for later sendto_ops and logging functions */

    if (expire)
      agline->expire = expire;  /* reset expiration time */

    /* inform the operators what's up */
    if (active != -1)
    {                           /* changing the activation */
      sendto_op_mask(SNO_GLINE, !expire ? "%s %sactivating %s for %s@%s" :
          "%s %sactivating %s for %s@%s and "
          "resetting expiration time to " TIME_T_FMT " (%s)",
          parv[0], active ? "re" : "de", gtype ? "BADCHAN" : "GLINE",
          agline->name, agline->host, agline->expire, date(agline->expire));
#if defined(GPATH)
      write_log(GPATH, !expire ? "# " TIME_T_FMT " %s!%s@%s %sactivating "
          "%s for %s@%s\n" : "# " TIME_T_FMT " %s!%s@%s %sactivating %s "
          "for %s@%s and resetting expiration time to " TIME_T_FMT "\n",
          TStime(), parv[0], PunteroACadena(cptr->cli_user->username),
          cptr->cli_user->host, active ? "re" : "de", gtype ? "BADCHAN" : "GLINE",
          agline->name, agline->host, agline->expire);
#endif /* GPATH */

    }
    else if (expire)
    {                           /* changing only the expiration */
      sendto_op_mask(SNO_GLINE,
          "%s resetting expiration time on %s for %s@%s to " TIME_T_FMT " (%s)",
          parv[0], gtype ? "BADCHAN" : "GLINE", agline->name, agline->host,
          agline->expire, date(agline->expire));
#if defined(GPATH)
      write_log(GPATH, "# " TIME_T_FMT " %s!%s@%s resetting expiration "
          "time on %s for %s@%s to " TIME_T_FMT "\n", TStime(), parv[0],
          PunteroACadena(cptr->cli_user->username), cptr->cli_user->host,
          gtype ? "BADCHAN" : "GLINE", agline->name, agline->host,
          agline->expire);
#endif /* GPATH */
    }
  }

  return 0;
}

 /*
  * Funcion para propagar una gline a otros servidores
  */
int propaga_gline(struct Client *cptr, struct Client *sptr, int active, time_t expire, time_t lastmod, time_t lifetime, int parc, char **parv) {
  struct Client *acptr = NULL;

  /* forward the message appropriately */
  if (!strCasediff(parv[1], "*")) /* global! */
  {
#if !defined(NO_PROTOCOL9)
    if(parc>6)
      sendto_lowprot_butone(cptr, 9, active ? ":%s GLINE %s +%s %s %s %s :%s" :
          ":%s GLINE %s -%s", parv[0], parv[1], parv[2], parv[3], parv[4], parv[5], parv[parc - 1]);
    else if(parc>5)
      sendto_lowprot_butone(cptr, 9, active ? ":%s GLINE %s +%s %s %s :%s" :
          ":%s GLINE %s -%s", parv[0], parv[1], parv[2], parv[3], parv[4], parv[parc - 1]);
    else
      sendto_lowprot_butone(cptr, 9, active ? ":%s GLINE %s +%s %s :%s" :
          ":%s GLINE %s -%s", parv[0], parv[1], parv[2], parv[3], parv[parc - 1]);
#endif
    if (IsUser(sptr)) {
      if(parc>6)
        sendto_highprot_butone(cptr, 10, active ? "%s%s " TOK_GLINE " %s +%s %s %s %s :%s" :
            "%s%s " TOK_GLINE " %s -%s", NumNick(sptr), parv[1], parv[2], parv[3], parv[4], parv[5], parv[parc - 1]);
      else if(parc>5)
        sendto_highprot_butone(cptr, 10, active ? "%s%s " TOK_GLINE " %s +%s %s %s :%s" :
            "%s%s " TOK_GLINE " %s -%s", NumNick(sptr), parv[1], parv[2], parv[3], parv[4], parv[parc - 1]);
      else
        sendto_highprot_butone(cptr, 10, active ? "%s%s " TOK_GLINE " %s +%s %s :%s" :
            "%s%s " TOK_GLINE " %s -%s", NumNick(sptr), parv[1], parv[2], parv[3], parv[parc - 1]);
    }
    else
    {
      if(parc>6)
        sendto_highprot_butone(cptr, 10, active ? "%s " TOK_GLINE " %s +%s %s %s %s :%s" :
            "%s " TOK_GLINE " %s -%s", NumServ(sptr), parv[1], parv[2], parv[3], parv[4], parv[5], parv[parc - 1]);
      else if(parc>5)
        sendto_highprot_butone(cptr, 10, active ? "%s " TOK_GLINE " %s +%s %s %s :%s" :
            "%s " TOK_GLINE " %s -%s", NumServ(sptr), parv[1], parv[2], parv[3], parv[4], parv[parc - 1]);
      else
        sendto_highprot_butone(cptr, 10, active ? "%s " TOK_GLINE " %s +%s %s :%s" :
            "%s " TOK_GLINE " %s -%s", NumServ(sptr), parv[1], parv[2], parv[3], parv[parc - 1]);
    }
  } else if ((
#if 1
      /*
       * REMOVE THIS after all servers upgraded to 2.10.01 and
       * Uworld uses a numeric too
       */
      (strlen(parv[1]) != 1 && !(acptr = FindClient(parv[1])))) ||
      (strlen(parv[1]) == 1 &&
#endif
      !(acptr = FindNServer(parv[1])))) {
    return 0;               /* no such server/user exists; forget it */
  }
  else
#if 1
/*
 * REMOVE THIS after all servers upgraded to 2.10.01 and
 * Uworld uses a numeric too
 */
  if (IsServer(acptr) || !MyConnect(acptr))
#endif
  {
#if !defined(NO_PROTOCOL9)
    if (Protocol(cli_from(acptr)) < 10)
    {
      if(parc>6)
        sendto_one(acptr, active ? ":%s GLINE %s +%s %s %s %s :%s" : ":%s GLINE %s -%s", parv[0], parv[1], parv[2], parv[3], parv[4], parv[5], parv[parc - 1]);  /* single destination */
      else if(parc>5)
        sendto_one(acptr, active ? ":%s GLINE %s +%s %s %s :%s" : ":%s GLINE %s -%s", parv[0], parv[1], parv[2], parv[3], parv[4], parv[parc - 1]);  /* single destination */
      else
        sendto_one(acptr, active ? ":%s GLINE %s +%s %s :%s" : ":%s GLINE %s -%s", parv[0], parv[1], parv[2], parv[3], parv[parc - 1]);  /* single destination */
    }
    else
#endif
    {
      if (IsUser(sptr)) {
        if(parc>6)
          sendto_one(acptr, active ? "%s%s " TOK_GLINE " %s +%s %s %s %s :%s" : "%s%s " TOK_GLINE " %s -%s",
            NumNick(sptr), parv[1], parv[2], parv[3], parv[4], parv[5], parv[parc - 1]);
        else if(parc>5)
          sendto_one(acptr, active ? "%s%s " TOK_GLINE " %s +%s %s %s :%s" : "%s%s " TOK_GLINE " %s -%s",
            NumNick(sptr), parv[1], parv[2], parv[3], parv[4], parv[parc - 1]);
        else
          sendto_one(acptr, active ? "%s%s " TOK_GLINE " %s +%s %s :%s" : "%s%s " TOK_GLINE " %s -%s",
            NumNick(sptr), parv[1], parv[2], parv[3], parv[parc - 1]);
      }
      else
      {
        if(parc>6)
          sendto_one(acptr, active ? "%s " TOK_GLINE " %s +%s %s %s %s :%s" : ":%s " TOK_GLINE " %s -%s",
              NumServ(sptr), parv[1], parv[2], parv[3], parv[4], parv[5], parv[parc - 1]);
        else if(parc>5)
          sendto_one(acptr, active ? "%s " TOK_GLINE " %s +%s %s %s :%s" : ":%s " TOK_GLINE " %s -%s",
              NumServ(sptr), parv[1], parv[2], parv[3], parv[4], parv[parc - 1]);
        else
          sendto_one(acptr, active ? "%s " TOK_GLINE " %s +%s %s :%s" : ":%s " TOK_GLINE " %s -%s",
              NumServ(sptr), parv[1], parv[2], parv[3], parv[parc - 1]);
      }
    }
    return 0;               /* only the intended  destination
                               should add this gline */
  }

  return 1;
}

int modifica_gline(struct Client *cptr, struct Client *sptr, aGline *agline, int gtype, time_t expire, time_t lastmod, time_t lifetime, char *who) {

  if(!buscar_uline(cli_confs(cptr), sptr->name) && /* Si el que la envia no tiene uline Y*/
      (agline->lastmod >= lastmod))            /* tenemos una version igual o mayor salimos */
    return 0;

  /* yes, notify the opers */
  sendto_op_mask(SNO_GLINE,
    "%s resetting expiration time on %s for %s@%s to "
    TIME_T_FMT " (%s)",
    who, gtype ? "BADCHAN" : "GLINE", agline->name, agline->host,
    expire, date(expire));

#if defined(GPATH)
  write_log(GPATH, "# " TIME_T_FMT " %s resetting expiration time "
      "on %s for %s@%s to " TIME_T_FMT "\n",
      TStime(), who, gtype ? "BADCHAN" : "GLINE",
      agline->name, agline->host, expire);
#endif /* GPATH */

  if (!lifetime) /* si no me ponen tiempo de vida uso el tiempo de expiracion */
    lifetime = expire;

  if (!lastmod) /* si no me ponen tiempo de vida uso el tiempo de expiracion */
    lastmod = TStime();

  agline->expire = expire;  /* reset the expire time */
  agline->lastmod = lastmod;
  agline->lifetime = lifetime;
}

#endif