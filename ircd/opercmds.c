/*
 * IRC - Internet Relay Chat, ircd/opercmds.c (formerly ircd/s_serv.c)
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
#ifdef _WIN32
# include <winsock.h>
#else
# include <netinet/in.h>
#endif
#include "h.h"
#include "s_debug.h"
#include "opercmds.h"
#include "struct.h"
#include "ircd.h"
#include "s_bsd.h"
#include "send.h"
#include "s_err.h"
#include "numeric.h"
#include "match.h"
#include "s_misc.h"
#include "s_conf.h"
#include "class.h"
#include "s_bdd.h"
#include "s_user.h"
#include "common.h"
#include "msg.h"
#include "sprintf_irc.h"
#include "userload.h"
#include "parse.h"
#include "numnicks.h"
#include "crule.h"
#include "version.h"
#include "support.h"
#include "s_serv.h"
#include "hash.h"
#if defined(BDD_MMAP)
#include "persistent_malloc.h"
#endif


RCSTAG_CC("$Id$");

/*
 *  m_squit
 *
 *    parv[0] = sender prefix
 *    parv[1] = server name
 *    parv[2] = timestamp
 *    parv[parc-1] = comment
 */
int m_squit(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Reg1 aConfItem *aconf;
  char *server;
  Reg2 aClient *acptr;
  char *comment = (parc > ((!IsServer(cptr)) ? 2 : 3) &&
      !BadPtr(parv[parc - 1])) ? parv[parc - 1] : cptr->name;

  if (!IsPrivileged(sptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }

  if (parc > (IsServer(cptr) ? 2 : 1))
  {
    server = parv[1];
    /*
     * To accomodate host masking, a squit for a masked server
     * name is expanded if the incoming mask is the same as
     * the server name for that link to the name of link.
     */
    if ((*server == '*') && IsServer(cptr) && (aconf = cptr->serv->cline) &&
        !strCasediff(server, my_name_for_link(me.name, aconf)))
    {
      server = cptr->name;
      acptr = cptr;
    }
    else
    {
      /*
       * The following allows wild cards in SQUIT. Only usefull
       * when the command is issued by an oper.
       */
      for (acptr = client; (acptr = next_client(acptr, server));
          acptr = acptr->next)
        if (IsServer(acptr) || IsMe(acptr))
          break;

      if (acptr)
      {
        if (IsMe(acptr))
        {
          if (IsServer(cptr))
          {
            acptr = cptr;
            server = PunteroACadena(cptr->sockhost);
          }
          else
            acptr = NULL;
        }
        else
        {
          /*
           * Look for a matching server that is closer,
           * that way we won't accidently squit two close
           * servers like davis.* and davis-r.* when typing
           * /SQUIT davis*
           */
          aClient *acptr2;
          for (acptr2 = acptr->serv->up; acptr2 != &me;
              acptr2 = acptr2->serv->up)
            if (!match(server, acptr2->name))
              acptr = acptr2;
        }
      }
    }
    /* If atoi(parv[2]) == 0 we must indeed squit !
     * It wil be our neighbour.
     */
    if (acptr && IsServer(cptr) &&
        atoi(parv[2]) && atoi(parv[2]) != acptr->serv->timestamp)
    {
      Debug((DEBUG_NOTICE, "Ignoring SQUIT with wrong timestamp"));
      return 0;
    }
  }
  else
  {
    sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SQUIT");
    if (IsServer(cptr))
    {
      /*
       * This is actually protocol error. But, well, closing
       * the link is very proper answer to that...
       */
      server = PunteroACadena(cptr->sockhost);
      acptr = cptr;
    }
    else
      return 0;
  }
  if (!acptr)
  {
    if (IsUser(sptr))
      sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name, parv[0], server);
    return 0;
  }
  if (IsLocOp(sptr) && !MyConnect(acptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }

  return exit_client(cptr, acptr, sptr, comment);
}

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

static unsigned int report_array[19][3] = {
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
  {CONF_WEBIRC, RPL_STATSILINE, 'W'},
#if defined(ESNET_NEG)
  {CONF_NEGOTIATION, RPL_STATSKLINE, 'F'},
  {CONF_NEGOTIATION, RPL_STATSKLINE, 'f'},
#endif
  {0, 0}
};

static void report_configured_links(aClient *sptr, int mask)
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
      else if ((tmp->status & (CONF_WEBIRC)))
        sendto_one(sptr, rpl_str(p[1]), me.name, sptr->name, c, host, "*",
            0, -1);
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
int m_stats(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  static char Sformat[] =
      ":%s %d %s Connection SendQ SendM SendKBytes SendComp "
      "RcveM RcveKBytes RcveComp :Open since";
  static char Lformat[] = ":%s %d %s %s %u %u %u %u%% %u %u %u%% :" TIME_T_FMT;
  aMessage *mptr;
  aClient *acptr;
  aGline *agline, *a2gline;
  aConfItem *aconf;
  unsigned char stat = parc > 1 ? parv[1][0] : '\0';
  Reg1 int i;

#if defined(WEBCHAT_HTML)
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
            (int)DBufLength(&acptr->sendQ), (int)acptr->sendM,
            (int)acptr->sendK,
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
            (IsServer(acptr) && (acptr->negociacion & ZLIB_ESNET_OUT)) ?
            (int)((acptr->comp_out_total_out * 100.0 /
            acptr->comp_out_total_in) + 0.5) : 100,
#else
            100,
#endif
            (int)acptr->receiveM, (int)acptr->receiveK,
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
            (IsServer(acptr) && (acptr->negociacion & ZLIB_ESNET_IN)) ?
            (int)((acptr->comp_in_total_in * 100.0 /
            acptr->comp_in_total_out) + 0.5) : 100,
#else
            100,
#endif
            time(NULL) - acptr->firsttime);
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
      sendto_one(sptr, rpl_str(RPL_STATMEMTOT),
          me.name, parv[0], get_mem_size(), get_alloc_cnt());
#endif
#if defined(MEMLEAKSTATS)
      report_memleak_stats(sptr, parc, parv);
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

      nowr = now - me.since;
      sendto_one(sptr, rpl_str(RPL_STATSUPTIME), me.name, parv[0],
          nowr / 86400, (nowr / 3600) % 24, (nowr / 60) % 60, nowr % 60);
      sendto_one(sptr, rpl_str(RPL_STATSCONN), me.name, parv[0],
          max_connection_count, max_client_count);
      break;
    }
    case 'w':
      calc_load(sptr);
      break;
    case 'W':
      /* Solo ircops y opers tienen acceso */
      if (!IsAnOper(sptr) && !IsHelpOp(sptr))
      {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return 0;
      }
      report_configured_links(sptr, CONF_WEBIRC);
      {
        struct db_reg *reg;

        for (reg = db_iterador_init(BDD_WEBIRCDB); reg;
            reg = db_iterador_next())
        {
          if (!isDigit(*reg->clave))
            continue;  /* Provisional mientras se use BDD w para vhosts */
          sendto_one(sptr, rpl_str(RPL_STATSILINE), me.name, sptr->name, 'W',
              reg->clave, "*", 0, 1);
        }
      }
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

/*
 *  m_connect                           - Added by Jto 11 Feb 1989
 *
 *    parv[0] = sender prefix
 *    parv[1] = servername
 *    parv[2] = port number
 *    parv[3] = remote server
 */
int m_connect(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  int retval;
  unsigned short int port, tmpport;
  aConfItem *aconf, *cconf;
  aClient *acptr;

  if (!IsPrivileged(sptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return -1;
  }

  if (IsLocOp(sptr) && parc > 3)  /* Only allow LocOps to make */
    return 0;                   /* local CONNECTS --SRB      */

  if (parc > 3 && MyUser(sptr))
  {
    aClient *acptr2, *acptr3;
    if (!(acptr3 = find_match_server(parv[3])))
    {
      sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name, parv[0], parv[3]);
      return 0;
    }

    /* Look for closest matching server */
    for (acptr2 = acptr3; acptr2 != &me; acptr2 = acptr2->serv->up)
      if (!match(parv[3], acptr2->name))
        acptr3 = acptr2;

    parv[3] = acptr3->name;
  }

  if (hunt_server(1, cptr, sptr, MSG_CONNECT, TOK_CONNECT, "%s %s :%s", 3, parc, parv) !=
      HUNTED_ISME)
    return 0;

  if (parc < 2 || *parv[1] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "CONNECT");
    return -1;
  }

  if ((acptr = FindServer(parv[1])))
  {
    if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
        || Protocol(cptr) < 10
#endif
    )
      sendto_one(sptr, ":%s NOTICE %s :Connect: Server %s %s %s.",
          me.name, parv[0], parv[1], "already exists from", acptr->from->name);
    else
      sendto_one(sptr, "%s " TOK_NOTICE " %s%s :Connect: Server %s %s %s.",
          NumServ(&me), NumNick(sptr), parv[1], "already exists from",
          acptr->from->name);
    return 0;
  }

  for (aconf = conf; aconf; aconf = aconf->next)
    if (aconf->status == CONF_CONNECT_SERVER &&
        match(parv[1], aconf->name) == 0)
      break;
  /* Checked first servernames, then try hostnames. */
  if (!aconf)
    for (aconf = conf; aconf; aconf = aconf->next)
      if (aconf->status == CONF_CONNECT_SERVER &&
          (match(parv[1], aconf->host) == 0))
        break;

  if (!aconf)
  {
    if (MyUser(sptr)
#if !defined(NO_PROTOCOL9)
        || Protocol(cptr) < 10
#endif
    )
      sendto_one(sptr,
          ":%s NOTICE %s :Connect: Host %s not listed in ircd.conf",
          me.name, parv[0], parv[1]);
    else
      sendto_one(sptr,
          "%s " TOK_NOTICE " %s%s :Connect: Host %s not listed in ircd.conf",
          NumServ(&me), NumNick(sptr), parv[1]);
    return 0;
  }
  /*
   *  Get port number from user, if given. If not specified,
   *  use the default from configuration structure. If missing
   *  from there, then use the precompiled default.
   */
  tmpport = port = aconf->port;
  if (parc > 2 && !BadPtr(parv[2]))
  {
    if ((port = atoi(parv[2])) == 0)
    {
      if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
          || Protocol(cptr) < 10
#endif
      )
        sendto_one(sptr,
            ":%s NOTICE %s :Connect: Invalid port number", me.name, parv[0]);
      else
        sendto_one(sptr, "%s " TOK_NOTICE " %s%s :Connect: Invalid port number",
            NumServ(&me), NumNick(sptr));
      return 0;
    }
  }
  else if (port == 0 && (port = PORTNUM) == 0)
  {
    if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
        || Protocol(cptr) < 10
#endif
    )
      sendto_one(sptr, ":%s NOTICE %s :Connect: missing port number",
          me.name, parv[0]);
    else
      sendto_one(sptr, "%s " TOK_NOTICE " %s%s :Connect: missing port number",
          NumServ(&me), NumNick(sptr));
    return 0;
  }

  /*
   * Evaluate connection rules...  If no rules found, allow the
   * connect.   Otherwise stop with the first true rule (ie: rules
   * are ored together.  Oper connects are effected only by D
   * lines (CRULEALL) not d lines (CRULEAUTO).
   */
  for (cconf = conf; cconf; cconf = cconf->next)
    if ((cconf->status == CONF_CRULEALL) &&
        (match(cconf->host, aconf->name) == 0))
      if (crule_eval(cconf->passwd))
      {
        if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
            || Protocol(cptr) < 10
#endif
        )
          sendto_one(sptr, ":%s NOTICE %s :Connect: Disallowed by rule: %s",
              me.name, parv[0], cconf->name);
        else
          sendto_one(sptr, "%s " TOK_NOTICE " %s%s :Connect: Disallowed by rule: %s",
              NumServ(&me), NumNick(sptr), cconf->name);
        return 0;
      }

  /*
   * Notify all operators about remote connect requests
   */
  if (!IsAnOper(cptr))
  {
    sendto_ops_butone(NULL, &me,
        ":%s WALLOPS :Remote CONNECT %s %s from %s", me.name, parv[1],
        parv[2] ? parv[2] : "", get_client_name(sptr, FALSE));
#if defined(USE_SYSLOG) && defined(SYSLOG_CONNECT)
    syslog(LOG_DEBUG,
        "CONNECT From %s : %s %d", parv[0], parv[1], parv[2] ? parv[2] : "");
#endif
  }
  aconf->port = port;
  switch (retval = connect_server(aconf, sptr, NULL))
  {
    case 0:
      if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
          || Protocol(cptr) < 10
#endif
      )
        sendto_one(sptr,
            ":%s NOTICE %s :*** Connecting to %s.",
            me.name, parv[0], aconf->name);
      else
        sendto_one(sptr,
            "%s " TOK_NOTICE " %s%s :*** Connecting to %s.",
            NumServ(&me), NumNick(sptr), aconf->name);
      break;
    case -1:
      /* Comments already sent */
      break;
    case -2:
      if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
          || Protocol(cptr) < 10
#endif
      )
        sendto_one(sptr, ":%s NOTICE %s :*** Host %s is unknown.",
            me.name, parv[0], aconf->name);
      else
        sendto_one(sptr, "%s " TOK_NOTICE " %s%s :*** Host %s is unknown.",
            NumServ(&me), NumNick(sptr), aconf->name);
      break;
    default:
      if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
          || Protocol(cptr) < 10
#endif
      )

        sendto_one(sptr,
            ":%s NOTICE %s :*** Connection to %s failed: %s",
            me.name, parv[0], aconf->name, strerror(retval));
      else
        sendto_one(sptr,
            "%s " TOK_NOTICE " %s%s :*** Connection to %s failed: %s",
            NumServ(&me), NumNick(sptr), aconf->name, strerror(retval));
  }
  aconf->port = tmpport;
  return 0;
}

/*
 * m_wallops
 *
 * Writes to all +w users currently online
 *
 * parv[0] = sender prefix
 * parv[1] = message text
 */
int m_wallops(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  char *message;

  if (!IsServer(sptr) && MyConnect(sptr) && !IsAnOper(sptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }

  message = parc > 1 ? parv[1] : NULL;

  if (BadPtr(message))
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "WALLOPS");
    return 0;
  }

  sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
      ":%s WALLOPS :%s", parv[0], message);
  return 0;
}

/*
 * m_time
 *
 * parv[0] = sender prefix
 * parv[1] = servername
 */
int m_time(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  if (hunt_server(0, cptr, sptr, MSG_TIME, TOK_TIME, ":%s", 1, parc, parv) == HUNTED_ISME)
    sendto_one(sptr, rpl_str(RPL_TIME), me.name,
        parv[0], me.name, TStime(), TSoffset, date((long)0));
  return 0;
}


#if defined(RELIABLE_CLOCK)
#error "Mala configuracion. ¡¡Sigue los pasos!!"
#endif

/*
 * m_settime
 *
 * parv[0] = sender prefix
 * parv[1] = new time
 * parv[2] = servername (Only used when sptr is an Oper).
 */
int m_settime(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  time_t t;
  long int dt;
  static char tbuf[11];
  Dlink *lp;

  if (!IsPrivileged(sptr))
    return 0;

  if (parc < 2)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SETTIME");
    return 0;
  }

  if (parc == 2 && MyUser(sptr))
    parv[parc++] = me.name;

  t = atoi(parv[1]);
  dt = TStime() - t;

  if (t < 779557906 || dt < -9000000)
  {
    sendto_one(sptr, ":%s NOTICE %s :SETTIME: Bad value", me.name, parv[0]);
    return 0;
  }

  if (IsServer(sptr))           /* send to unlagged servers */
  {
#if defined(RELIABLE_CLOCK)
    sprintf_irc(tbuf, TIME_T_FMT, TStime());
    parv[1] = tbuf;
#endif
    for (lp = me.serv->down; lp; lp = lp->next)
      if (cptr != lp->value.cptr && DBufLength(&lp->value.cptr->sendQ) < 8000)
      {
#if !defined(NO_PROTOCOL9)
        if (Protocol(lp->value.cptr) < 10)
          sendto_one(lp->value.cptr, ":%s SETTIME %s", parv[0], parv[1]);
        else
#endif
          sendto_one(lp->value.cptr, "%s " TOK_SETTIME " %s", NumServ(sptr), parv[1]);
      }
  }
  else
  {
    sprintf_irc(tbuf, TIME_T_FMT, TStime());
    parv[1] = tbuf;
    if (hunt_server(1, cptr, sptr, MSG_SETTIME, TOK_SETTIME, "%s %s", 2, parc, parv) !=
        HUNTED_ISME)
      return 0;
  }

#if defined(RELIABLE_CLOCK)
  if ((dt > 600) || (dt < -600))
  {
#if !defined(NO_PROTOCOL9)
    sendto_lowprot_butone(NULL, 9,
        ":%s WALLOPS :Bad SETTIME from %s: " TIME_T_FMT, me.name, sptr->name,
        t);
#endif
    sendto_highprot_butone(NULL, 10,
        "%s " TOK_WALLOPS " :Bad SETTIME from %s: " TIME_T_FMT, NumServ(&me), sptr->name,
        t);
  }
  if (IsUser(sptr))
  {

    if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
        || Protocol(cptr) < 10
#endif
    )
      sendto_one(sptr, ":%s NOTICE %s :clock is not set %ld seconds %s : "
          "RELIABLE_CLOCK is defined", me.name, parv[0],
          (dt < 0) ? -dt : dt, (dt <= 0) ? "forwards" : "backwards");
    else
      sendto_one(sptr, "%s " TOK_NOTICE " %s%s :clock is not set %ld seconds %s : "
          "RELIABLE_CLOCK is defined", NumServ(&me), NumNick(sptr),
          (dt < 0) ? -dt : dt, (dt <= 0) ? "forwards" : "backwards");
  }
#else
  sendto_ops("SETTIME from %s, clock is set %ld seconds %s",
      sptr->name, (dt < 0) ? -dt : dt, (dt <= 0) ? "forwards" : "backwards");
  TSoffset -= dt;
  if (IsUser(sptr))
  {
    if (MyUser(sptr)
#if !defined(NO_PROTOCOL9)
        || Protocol(cptr) < 10
#endif
    )
      sendto_one(sptr, ":%s NOTICE %s :clock is set %ld seconds %s", me.name,
          parv[0], (dt < 0) ? -dt : dt, (dt <= 0) ? "forwards" : "backwards");
    else
      sendto_one(sptr, "%s " TOK_NOTICE " %s%s :clock is set %ld seconds %s",
          NumServ(&me), NumNick(sptr),
          (dt < 0) ? -dt : dt, (dt <= 0) ? "forwards" : "backwards");
  }
#endif
  return 0;
}

static char *militime(char *sec, char *usec)
{
  struct timeval tv;
  static char timebuf[18];

  gettimeofday(&tv, NULL);
  if (sec && usec)
#if defined(__sun__) || defined(__bsdi__) || (__GLIBC__ >= 2) || defined(__NetBSD__)
    sprintf(timebuf, "%ld",
        (tv.tv_sec - atoi(sec)) * 1000 + (tv.tv_usec - atoi(usec)) / 1000);
#else
    sprintf_irc(timebuf, "%d",
        (tv.tv_sec - atoi(sec)) * 1000 + (tv.tv_usec - atoi(usec)) / 1000);
#endif
  else
#if defined(__sun__) || defined(__bsdi__) || (__GLIBC__ >= 2) || defined(__NetBSD__)
    sprintf(timebuf, "%ld %ld", tv.tv_sec, tv.tv_usec);
#else
    sprintf_irc(timebuf, "%d %d", tv.tv_sec, tv.tv_usec);
#endif
  return timebuf;
}

/*
 * m_rping  -- by Run
 *
 *    parv[0] = sender (sptr->name thus)
 * if sender is a person: (traveling towards start server)
 *    parv[1] = pinged server[mask]
 *    parv[2] = start server (current target)
 *    parv[3] = optional remark
 * if sender is a server: (traveling towards pinged server)
 *    parv[1] = pinged server (current target)
 *    parv[2] = original sender (person)
 *    parv[3] = start time in s
 *    parv[4] = start time in us
 *    parv[5] = the optional remark
 */
int m_rping(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;

  if (!IsPrivileged(sptr))
    return 0;

  if (parc < (IsAnOper(sptr) ? (MyConnect(sptr) ? 2 : 3) : 6))
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "RPING");
    return 0;
  }
  if (MyUser(sptr))
  {
    if (parc == 2)
      parv[parc++] = me.name;
    else if (!(acptr = find_match_server(parv[2])))
    {
      parv[3] = parv[2];
      parv[2] = me.name;
      parc++;
    }
    else
      parv[2] = acptr->name;
    if (parc == 3)
      parv[parc++] = "<No client start time>";
  }

  if (IsAnOper(sptr))
  {
    if (hunt_server(1, cptr, sptr, MSG_RPING, TOK_RPING, "%s %s :%s", 2, parc, parv) !=
        HUNTED_ISME)
      return 0;
    if (!(acptr = find_match_server(parv[1])) || !IsServer(acptr))
    {
      sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name, parv[0], parv[1]);
      return 0;
    }
#if !defined(NO_PROTOCOL9)
    if (Protocol(acptr->from) < 10)
      sendto_one(acptr, ":%s RPING %s %s %s :%s",
          me.name, acptr->name, sptr->name, militime(NULL, NULL), parv[3]);
    else
#endif
      sendto_one(acptr, "%s " TOK_RPING " %s %s %s :%s",
          NumServ(&me), NumServ(acptr), sptr->name, militime(NULL, NULL), parv[3]);
  }
  else
  {
    if (hunt_server(1, cptr, sptr, MSG_RPING, TOK_RPING, "%s %s %s %s :%s", 1, parc, parv)
        != HUNTED_ISME)
      return 0;
    sendto_one(cptr, ":%s RPONG %s %s %s %s :%s", me.name, parv[0],
        parv[2], parv[3], parv[4], parv[5]);
  }
  return 0;
}

/*
 * m_rpong  -- by Run too :)
 *
 * parv[0] = sender prefix
 * parv[1] = from pinged server: start server; from start server: sender
 * parv[2] = from pinged server: sender; from start server: pinged server
 * parv[3] = pingtime in ms
 * parv[4] = client info (for instance start time)
 */
int m_rpong(aClient *UNUSED(cptr), aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;

  if (!IsServer(sptr))
    return 0;

  if (parc < 5)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "RPING");
    return 0;
  }

  if (!(acptr = FindClient(parv[1])))
    return 0;

  if (!IsMe(acptr))
  {
    if (IsServer(acptr) && parc > 5)
    {
      sendto_one(acptr, ":%s RPONG %s %s %s %s :%s",
          parv[0], parv[1], parv[2], parv[3], parv[4], parv[5]);
      return 0;
    }
  }
  else
  {
    parv[1] = parv[2];
    parv[2] = sptr->name;
    parv[0] = me.name;
    parv[3] = militime(parv[3], parv[4]);
    parv[4] = parv[5];
    if (!(acptr = FindUser(parv[1])))
      return 0;                 /* No bouncing between servers ! */
  }

  sendto_one(acptr, ":%s RPONG %s %s %s :%s",
      parv[0], parv[1], parv[2], parv[3], parv[4]);
  return 0;
}

#if defined(OPER_REHASH) || defined(LOCOP_REHASH)
/*
 * m_rehash
 */
static int m_rehash_local(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
#if !defined(LOCOP_REHASH)
  if (!MyUser(sptr) || !IsOper(sptr))
#else
#if defined(OPER_REHASH)
  if (!MyUser(sptr) || !IsAnOper(sptr))
#else
  if (!MyUser(sptr) || !IsLocOp(sptr))
#endif
#endif
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }
  sendto_one(sptr, rpl_str(RPL_REHASHING), me.name, parv[0], configfile);
  sendto_ops("%s is rehashing Server config file", parv[0]);
#if defined(USE_SYSLOG)
  syslog(LOG_INFO, "REHASH From %s\n", get_client_name(sptr, FALSE));
#endif
  return rehash(cptr, (parc > 1) ? ((*parv[1] == 'q') ? 2 : 0) : 0);
}

#endif

static int m_rehash_remoto(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  int flag = 0;
  const char *target;

  if (parc < 2)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
        "REHASH");
    return 0;
  }

  target = parv[1];

  /* is it a message we should pay attention to? */
  if (target[0] != '*' || target[1] != '\0')
  {
    if (hunt_server(1, cptr, sptr, MSG_REHASH, TOK_REHASH, parc > 2 ?
           "%s %s" : "%s", 1, parc, parv) != HUNTED_ISME)
      return 0;
  } else if (parc > 2) /* must forward the message with flags */
     sendto_highprot_butone(cptr, 10, "%s " TOK_REHASH " * %s",
         NumServ(sptr), parv[2]);
  else /* just have to forward the message */
     sendto_highprot_butone(cptr, 10, "%s " TOK_REHASH " *",
         NumServ(sptr));

  /* OK, the message has been forwarded, but before we can act... */
  /*
  if (!feature_bool(FEAT_NETWORK_REHASH))
    return 0;
  */

  if (parc > 2) { /* special processing */
    if (*parv[2] == 'q')
      flag = 2;
  }

  sendto_ops("%s is rehashing Server config file", sptr->name);
#if defined(USE_SYSLOG)
  syslog(LOG_INFO, "REHASH From %s\n", get_client_name(sptr, FALSE));
#endif
  return rehash(cptr, flag);

}

int m_rehash(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  if (MyUser(sptr))
#if defined(OPER_REHASH) || defined(LOCOP_REHASH)
    return m_rehash_local(cptr, sptr, parc, parv);
#else
    return 0;
#endif
  else
    return m_rehash_remoto(cptr, sptr, parc, parv);
}

#if defined(OPER_RESTART) || defined(LOCOP_RESTART)
/*
 * m_restart
 */
static int m_restart_local(aClient *UNUSED(cptr), aClient *sptr, int UNUSED(parc),
    char *parv[])
{
#if !defined(LOCOP_RESTART)
  if (!MyUser(sptr) || !IsOper(sptr))
#else
#if defined(OPER_RESTART)
  if (!MyUser(sptr) || !IsAnOper(sptr))
#else
  if (!MyUser(sptr) || !IsLocOp(sptr))
#endif
#endif
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }
#if defined(USE_SYSLOG)
  syslog(LOG_WARNING, "Server RESTART by %s\n", get_client_name(sptr, FALSE));
#endif
  server_reboot();
  return 0;
}
#endif

static int m_restart_remoto(aClient *cptr, aClient *sptr, int parc,
    char *parv[])
{
  const char *target, *when, *reason;

  if (parc < 4)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "RESTART");
    return 0;
  }

  target = parv[1];
  when = parv[2];
  reason = parv[parc - 1];

  /* is it a message we should pay attention to? */
  if (target[0] != '*' || target[1] != '\0')
  {
    if (hunt_server(1, cptr, sptr, MSG_RESTART, TOK_RESTART, "%s %s :%s", 1, parc, parv) !=
            HUNTED_ISME)
      return 0;
  } else /* must forward the message */
    sendto_highprot_butone(cptr, 10, "%s " TOK_RESTART " * %s :%s",
        NumServ(sptr), when, reason);

  /* OK, the message has been forwarded, but before we can act... */
/*
  if (!feature_bool(FEAT_NETWORK_RESTART))
    return 0;
*/

#if defined(USE_SYSLOG)
  syslog(LOG_WARNING, "Server RESTART by %s: %s\n", get_client_name(sptr, FALSE), reason);
#endif
  server_reboot();
  return 0;

}

int m_restart(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  if (MyUser(sptr))
#if defined(OPER_RESTART) || defined(LOCOP_RESTART)
    return m_restart_local(cptr, sptr, parc, parv);
#else
    return 0;
#endif
  else
    return m_restart_remoto(cptr, sptr, parc, parv);
}

/*
 * m_trace
 *
 * parv[0] = sender prefix
 * parv[1] = nick or servername
 * parv[2] = 'target' servername
 */
int m_trace(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Reg1 int i;
  Reg2 aClient *acptr;
  aConfClass *cltmp;
  char *tname;
  int doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
  int cnt = 0, wilds, dow;

#if defined(BDD_VIP)
  if (!IsAnOper(cptr) && !IsHelpOp(cptr) && !IsServer(cptr))
  {
    sendto_one(cptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;                   /* no puedes */
  }
#endif

  if (parc < 2 || BadPtr(parv[1]))
  {
    /* just "TRACE" without parameters. Must be from local client */
    parc = 1;
    acptr = &me;
    tname = me.name;
    i = HUNTED_ISME;
  }
  else if (parc < 3 || BadPtr(parv[2]))
  {
    /* No target specified. Make one before propagating. */
    parc = 2;
    tname = parv[1];
    if ((acptr = find_match_server(parv[1])) ||
        ((acptr = FindClient(parv[1])) && !MyUser(acptr)))
    {
      if (IsUser(acptr))
        parv[2] = PunteroACadena(acptr->user->server->name);
      else
        parv[2] = PunteroACadena(acptr->name);
      parc = 3;
      parv[3] = NULL;
      if ((i = hunt_server(IsServer(acptr), cptr, sptr,
          MSG_TRACE, TOK_TRACE, "%s :%s", 2, parc, parv)) == HUNTED_NOSUCH)
      {
        sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name, parv[0], parv[2]);
        return 0;
      }
    }
    else
      i = HUNTED_ISME;
  }
  else
  {
    /* Got "TRACE <tname> :<target>" */
    parc = 3;
    if (MyUser(sptr)
#if !defined(NO_PROTOCOL9)
        || Protocol(cptr) < 10
#endif
    )
      acptr = find_match_server(parv[2]);
    else
      acptr = FindNServer(parv[2]);
    if ((i = hunt_server(0, cptr, sptr,
        MSG_TRACE, TOK_TRACE, "%s :%s", 2, parc, parv)) == HUNTED_NOSUCH)
      return 0;
    tname = parv[1];
  }

  if (i == HUNTED_PASS)
  {
    if (!acptr)
      acptr = next_client(client, tname);
    else
      acptr = acptr->from;
    sendto_one(sptr, rpl_str(RPL_TRACELINK), me.name, parv[0],
#if !defined(GODMODE)
        version, debugmode, tname,
        acptr ? PunteroACadena(acptr->from->name) : "<No_match>");
#else /* GODMODE */
        version, debugmode, tname,
        acptr ? PunteroACadena(acptr->from->name) : "<No_match>", (acptr
        && acptr->from->serv) ? acptr->from->serv->timestamp : 0);
#endif /* GODMODE */
    return 0;
  }

  doall = (parv[1] && (parc > 1)) ? !match(tname, me.name) : TRUE;
  wilds = !parv[1] || strchr(tname, '*') || strchr(tname, '?');
  dow = wilds || doall;

  /* Don't give (long) remote listings to lusers */
  if (dow && !MyConnect(sptr) && !IsAnOper(sptr))
    return 0;

  for (i = 0; i < MAXCONNECTIONS; i++)
    link_s[i] = 0, link_u[i] = 0;

  if (doall)
  {
    for (acptr = client; acptr; acptr = acptr->next)
      if (IsUser(acptr))
        link_u[acptr->from->fd]++;
      else if (IsServer(acptr))
        link_s[acptr->from->fd]++;
  }

  /* report all direct connections */

  for (i = 0; i <= highest_fd; i++)
  {
    unsigned int conClass;

    if (!(acptr = loc_clients[i]))  /* Local Connection? */
      continue;
    if (IsInvisible(acptr) && dow && !(MyConnect(sptr) && IsOper(sptr)) &&
        !IsAnOper(acptr) && (acptr != sptr))
      continue;
    if (!doall && wilds && match(tname, PunteroACadena(acptr->name)))
      continue;
    if (!dow && strCasediff(tname, PunteroACadena(acptr->name)))
      continue;
    conClass = get_client_class(acptr);

    switch (acptr->status)
    {
      case STAT_CONNECTING:
        sendto_one(sptr, rpl_str(RPL_TRACECONNECTING),
            me.name, parv[0], conClass, PunteroACadena(acptr->name));
        cnt++;
        break;
      case STAT_HANDSHAKE:
        sendto_one(sptr, rpl_str(RPL_TRACEHANDSHAKE),
            me.name, parv[0], conClass, PunteroACadena(acptr->name));
        cnt++;
        break;
      case STAT_ME:
        break;
      case STAT_UNKNOWN:
      case STAT_UNKNOWN_USER:
        sendto_one(sptr, rpl_str(RPL_TRACEUNKNOWN), me.name, parv[0], conClass,
#if defined (BDD_VIP)
            get_visible_name(sptr, acptr));
#else
            get_client_name(acptr, FALSE));
#endif
        cnt++;
        break;
      case STAT_UNKNOWN_SERVER:
        sendto_one(sptr, rpl_str(RPL_TRACEUNKNOWN),
            me.name, parv[0], conClass, PunteroACadena(acptr->name));
        cnt++;
        break;
      case STAT_USER:
        /* Only opers see users if there is a wildcard
           but anyone can see all the opers. */
        if ((IsAnOper(sptr) && (MyUser(sptr) ||
            !(dow && IsInvisible(acptr)))) || !dow || IsAnOper(acptr))
        {
          if (IsAnOper(acptr))
            sendto_one(sptr, rpl_str(RPL_TRACEOPERATOR),
                me.name, parv[0], conClass,
#if defined (BDD_VIP)
                get_visible_name(sptr, acptr),
#else
                get_client_name(acptr, FALSE),
#endif
                now - acptr->lasttime);
          else
            sendto_one(sptr, rpl_str(RPL_TRACEUSER), me.name, parv[0], conClass,
#if defined (BDD_VIP)
                get_visible_name(sptr, acptr),
#else
                get_client_name(acptr, FALSE),
#endif
                now - acptr->lasttime);
          cnt++;
        }
        break;
        /*
         * Connection is a server
         *
         * Serv <class> <nS> <nC> <name> <ConnBy> <last> <age>
         *
         * class        Class the server is in
         * nS           Number of servers reached via this link
         * nC           Number of clients reached via this link
         * name         Name of the server linked
         * ConnBy       Who established this link
         * last         Seconds since we got something from this link
         * age          Seconds this link has been alive
         *
         * Additional comments etc......        -Cym-<cym@acrux.net>
         */

      case STAT_SERVER:
        if (acptr->serv->user)
          sendto_one(sptr, rpl_str(RPL_TRACESERVER),
              me.name, parv[0], conClass, link_s[i],
              link_u[i], PunteroACadena(acptr->name),
              PunteroACadena(acptr->serv->by),
              PunteroACadena(acptr->serv->user->username),
              PunteroACadena(acptr->serv->user->host), now - acptr->lasttime,
              now - acptr->serv->timestamp);
        else
          sendto_one(sptr, rpl_str(RPL_TRACESERVER),
              me.name, parv[0], conClass, link_s[i],
              link_u[i], PunteroACadena(acptr->name), acptr->serv->by ?
              acptr->serv->by : "*", "*", me.name,
              now - acptr->lasttime, now - acptr->serv->timestamp);
        cnt++;
        break;
      case STAT_LOG:
        sendto_one(sptr, rpl_str(RPL_TRACELOG),
            me.name, parv[0], LOGFILE, acptr->port);
        cnt++;
        break;
      case STAT_PING:
        sendto_one(sptr, rpl_str(RPL_TRACEPING), me.name,
            parv[0], PunteroACadena(acptr->name),
            (acptr->acpt) ? PunteroACadena(acptr->acpt->name) : "<null>");
        break;
      default:                 /* We actually shouldn't come here, -msa */
        sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name, parv[0],
            PunteroACadena(acptr->name));
        cnt++;
        break;
    }
  }
  /*
   * Add these lines to summarize the above which can get rather long
   * and messy when done remotely - Avalon
   */
  if (!IsAnOper(sptr) || !cnt)
  {
    if (!cnt)
      /* let the user have some idea that its at the end of the trace */
      sendto_one(sptr, rpl_str(RPL_TRACESERVER),
          me.name, parv[0], 0, link_s[me.fd],
          link_u[me.fd], "<No_match>", me.serv->by ?
          me.serv->by : "*", "*", me.name, 0, 0);
    return 0;
  }
  for (cltmp = FirstClass(); doall && cltmp; cltmp = NextClass(cltmp))
    if (Links(cltmp) > 0)
      sendto_one(sptr, rpl_str(RPL_TRACECLASS), me.name,
          parv[0], ConClass(cltmp), Links(cltmp));
  return 0;
}

/*
 *  m_close                              - added by Darren Reed Jul 13 1992.
 */
int m_close(aClient *cptr, aClient *sptr, int UNUSED(parc), char *parv[])
{
  Reg1 aClient *acptr;
  Reg2 int i;
  int closed = 0;

  if (!MyOper(sptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }

  for (i = highest_fd; i; i--)
  {
    if (!(acptr = loc_clients[i]))
      continue;
    if (!IsUnknown(acptr) && !IsConnecting(acptr) && !IsHandshake(acptr))
      continue;
    sendto_one(sptr, rpl_str(RPL_CLOSING), me.name, parv[0],
        get_client_name(acptr, FALSE), acptr->status);
    exit_client(cptr, acptr, &me, "Oper Closing");
    closed++;
  }
  sendto_one(sptr, rpl_str(RPL_CLOSEEND), me.name, parv[0], closed);
  return 0;
}

#if defined(OPER_DIE) || defined(LOCOP_DIE)
/*
 * m_die
 */
static int m_die_local(aClient *UNUSED(cptr), aClient *sptr, int UNUSED(parc), char *parv[])
{
  Reg1 aClient *acptr;
  Reg2 int i;

#if !defined(LOCOP_DIE)
  if (!MyUser(sptr) || !IsOper(sptr))
#else
#if defined(OPER_DIE)
  if (!MyUser(sptr) || !IsAnOper(sptr))
#else
  if (!MyUser(sptr) || !IsLocOp(sptr))
#endif
#endif
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }

  for (i = 0; i <= highest_fd; i++)
  {
    if (!(acptr = loc_clients[i]))
      continue;
    if (IsUser(acptr))
      sendto_one(acptr, ":%s NOTICE %s :Server Terminating. %s",
          me.name, PunteroACadena(acptr->name), get_client_name(sptr, FALSE));
    else if (IsServer(acptr))
      sendto_one(acptr, ":%s ERROR :Terminated by %s",
          me.name, get_client_name(sptr, FALSE));
  }

  flush_connections(me.fd);

#if defined(BDD_MMAP)
  db_persistent_commit();
#endif

#if defined(__cplusplus)
  s_die(0);
#else
  s_die();
#endif
  return 0;
}
#endif

static int m_die_remoto(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Reg1 aClient *acptr;
  Reg2 int i;
  const char *target, *when, *reason;

  if (parc < 4)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "DIE");
    return 0;
  }

  target = parv[1];
  when = parv[2];
  reason = parv[parc - 1];

  /* is it a message we should pay attention to? */
  if (target[0] != '*' || target[1] != '\0')
  {
    if (hunt_server(1, cptr, sptr, MSG_DIE, TOK_DIE, "%s %s :%s", 1, parc, parv) !=
                HUNTED_ISME)
      return 0;
  } else /* must forward the message */
      sendto_highprot_butone(cptr, 10, "%s " TOK_DIE " * %s :%s",
          NumServ(sptr), when, reason);

  /* OK, the message has been forwarded, but before we can act... */
  /*
  if (!feature_bool(FEAT_NETWORK_DIE))
    return 0;
    */

  for (i = 0; i <= highest_fd; i++)
  {
    if (!(acptr = loc_clients[i]))
      continue;
    if (IsUser(acptr))
      sendto_one(acptr, ":%s NOTICE %s :Server Terminating. %s: %s",
          me.name, PunteroACadena(acptr->name), get_client_name(sptr, FALSE), reason);
    else if (IsServer(acptr))
      sendto_one(acptr, ":%s ERROR :Terminated by %s: %s",
          me.name, get_client_name(sptr, FALSE), reason);
  }

  flush_connections(me.fd);

#if defined(BDD_MMAP)
  db_persistent_commit();
#endif

#if defined(__cplusplus)
  s_die(0);
#else
  s_die();
#endif
  return 0;
}

int m_die(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  if (MyUser(sptr))
#if defined(OPER_DIE) || defined(LOCOP_DIE)
    return m_die_local(cptr, sptr, parc, parv);
#else
    return 0;
#endif
  else
    return m_die_remoto(cptr, sptr, parc, parv);
}

static void add_gline(aClient *cptr, aClient *sptr, char *host, char *comment,
    char *user, time_t expire, time_t lastmod, time_t lifetime, int local)
{

  aClient *acptr;
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

  if((!IsHub(sptr) || !buscar_uline(cptr->confs, sptr->name)) && !lastmod) /* Si no es hub y no tiene uline y me pasan ultima mod 0 salgo */
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
    write_log(GPATH, "%c:%s:%s:%s\n", 'K', host, comment, user);
#endif /* GPATH */

  if(!lastmod)
    lastmod = TStime();

  if (!lifetime) /* si no me ponen tiempo de vida uso el tiempo de expiracion */
    lifetime = expire;

  agline = make_gline(host, comment, user, expire, lastmod, lifetime);
  if (local)
    SetGlineIsLocal(agline);

#if defined(BADCHAN)
  if (gtype)
    return;
#endif

  for (fd = highest_fd; fd >= 0; --fd)  /* get the users! */
    if ((acptr = loc_clients[fd]) && !IsMe(acptr))
    {

      if (!acptr->user || (acptr->sockhost
          && strlen(acptr->sockhost) > (size_t)HOSTLEN)
          || (acptr->user->username ? strlen(acptr->user->username) : 0) >
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
          *tmp=toLower(*tmp);
          *tmp++;
        }
        
        tmp=cptr_info_low;
      } else if(GlineIsRealName(agline))
        tmp=PunteroACadena(acptr->info);

      if ((GlineIsIpMask(agline) ? ipmask_check(&acptr->ip, &agline->gl_addr, agline->gl_bits) == 0 :
          (GlineIsRealName(agline) ? match_pcre(agline->re, tmp) :
            match(agline->host, PunteroACadena(acptr->sockhost)))) == 0 &&
            match(agline->name, PunteroACadena(acptr->user->username)) == 0)
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
          exit_client_msg(sptr->from, acptr, &me, "G-lined (%s)",
              agline->reason);
      }
    }
}

#if !defined(BADCHAN)
#error "Mala configuracion, activa por favor el BADCHAN en el make config"
#endif

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
int m_gline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr = NULL;        /* Init. to avoid compiler warning. */

  aGline *agline, *a2gline;
  char *user, *host;
  int active, gtype = 0;
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
int ms_gline(aClient *cptr, aClient *sptr, aGline *agline, aGline *a2gline, int parc, char *parv[])
{
  char *user, *host;
  int active, gtype = 0;
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

  tiene_uline=buscar_uline(cptr->confs, sptr->name);
  
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
        aClient *acptr;

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
              !mmatch(agline->host, host))
            return 0;         /* found an existing G-line that matches */
#endif
        /* add the line: */
        add_gline(cptr, sptr, host, parv[parc - 1], user, expire, lastmod, lifetime, 0);
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
int mo_gline(aClient *cptr, aClient *sptr, aGline *agline, aGline *a2gline, int parc, char *parv[])
{
  char *user, *host;
  int active, gtype = 0;
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
        add_gline(cptr, sptr, host, parv[3], user, expire, lastmod, lifetime, 1);
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
            TStime(), parv[0], PunteroACadena(cptr->user->username),
            cptr->user->host, gtype ? "BADCHAN" : "GLINE", agline->name,
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
          TStime(), parv[0], PunteroACadena(cptr->user->username),
          cptr->user->host, active ? "re" : "de", gtype ? "BADCHAN" : "GLINE",
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
          PunteroACadena(cptr->user->username), cptr->user->host,
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
int propaga_gline(aClient *cptr, aClient *sptr, int active, time_t expire, time_t lastmod, time_t lifetime, int parc, char **parv) {
  aClient *acptr = NULL;

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
    if (Protocol(acptr->from) < 10)
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

int modifica_gline(aClient *cptr, aClient *sptr, aGline *agline, int gtype, time_t expire, time_t lastmod, time_t lifetime, char *who) {

  if(!buscar_uline(cptr->confs, sptr->name) && /* Si el que la envia no tiene uline Y*/
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

