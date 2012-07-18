/*
 * IRC - Internet Relay Chat, ircd/querycmds.c (formerly ircd/s_serv.c)
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
#include <assert.h>

#include "h.h"
#include "s_debug.h"
#include "struct.h"
#include "parse.h"
#include "send.h"
#include "s_err.h"
#include "numeric.h"
#include "ircd.h"
#include "s_user.h"
#include "version.h"
#include "s_bsd.h"
#include "s_misc.h"
#include "match.h"
#include "s_serv.h"
#include "msg.h"
#include "channel.h"
#include "numnicks.h"
#include "userload.h"
#include "s_conf.h"
#include "support.h"
#include "sprintf_irc.h"
#include "querycmds.h"
#include "s_bdd.h"

RCSTAG_CC("$Id$");

/*
 * m_functions execute protocol messages on this server:
 *
 *   cptr    is always NON-NULL, pointing to a *LOCAL* client
 *           structure (with an open socket connected!). This
 *           identifies the physical socket where the message
 *           originated (or which caused the m_function to be
 *           executed--some m_functions may call others...).
 *
 *   sptr    is the source of the message, defined by the
 *           prefix part of the message if present. If not
 *           or prefix not found, then sptr==cptr.
 *
 *           (!IsServer(cptr)) => (cptr == sptr), because
 *           prefixes are taken *only* from servers...
 *
 *           (IsServer(cptr))
 *                   (sptr == cptr) => the message didn't
 *                   have the prefix.
 *
 *                   (sptr != cptr && IsServer(sptr) means
 *                   the prefix specified servername. (?)
 *
 *                   (sptr != cptr && !IsServer(sptr) means
 *                   that message originated from a remote
 *                   user (not local).
 *
 *           combining
 *
 *           (!IsServer(sptr)) means that, sptr can safely
 *           taken as defining the target structure of the
 *           message in this server.
 *
 *   *Always* true (if 'parse' and others are working correct):
 *
 *   1)      sptr->from == cptr  (note: cptr->from == cptr)
 *
 *   2)      MyConnect(sptr) <=> sptr == cptr (e.g. sptr
 *           *cannot* be a local connection, unless it's
 *           actually cptr!). [MyConnect(x) should probably
 *           be defined as (x == x->from) --msa ]
 *
 *   parc    number of variable parameter strings (if zero,
 *           parv is allowed to be NULL)
 *
 *   parv    a NULL terminated list of parameter pointers,
 *
 *                   parv[0], sender (prefix string), if not present
 *                           this points to an empty string.
 *                   parv[1]...parv[parc-1]
 *                           pointers to additional parameters
 *                   parv[parc] == NULL, *always*
 *
 *           note:   it is guaranteed that parv[0]..parv[parc-1] are all
 *                   non-NULL pointers.
 */

/*
 * m_version
 *
 *   parv[0] = sender prefix
 *   parv[1] = remote server
 */
int m_version(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Reg1 aClient *acptr;

  if (MyConnect(sptr) && parc > 1)
  {
    if (IsUser(sptr) && !IsAnOper(sptr) && !IsHelpOp(sptr))
    {
      sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
      return 0;
    }
    
    if (!(acptr = find_match_server(parv[1])))
    {
      sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name, parv[0], parv[1]);
      return 0;
    }
    parv[1] = acptr->name;
  }

  if (hunt_server(0, cptr, sptr, MSG_VERSION, TOK_VERSION, ":%s", 1, parc, parv) ==
      HUNTED_ISME)
    sendto_one(sptr, rpl_str(RPL_VERSION),
        me.name, parv[0], version, debugmode, me.name, serveropts);

  return 0;
}

/*
 * m_info
 *
 * parv[0] = sender prefix
 * parv[1] = servername
 */
int m_info(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  const char **text = infotext;

  if (hunt_server(1, cptr, sptr, MSG_INFO, TOK_INFO, ":%s", 1, parc, parv) == HUNTED_ISME)
  {
    while (text[2])
    {
      if (!IsOper(sptr))
        sendto_one(sptr, rpl_str(RPL_INFO), me.name, parv[0], *text);
      text++;
    }
    if (IsOper(sptr))
    {
      while (*text)
        sendto_one(sptr, rpl_str(RPL_INFO), me.name, parv[0], *text++);
      sendto_one(sptr, rpl_str(RPL_INFO), me.name, parv[0], "");
    }
    sendto_one(sptr, ":%s %d %s :Birth Date: %s, compile # %s",
        me.name, RPL_INFO, parv[0], creation, generation);
    sendto_one(sptr, ":%s %d %s :On-line since %s",
        me.name, RPL_INFO, parv[0], myctime(me.firsttime));
    sendto_one(sptr, rpl_str(RPL_ENDOFINFO), me.name, parv[0]);
  }
  return 0;
}

/*
 * m_links
 *
 * parv[0] = sender prefix
 * parv[1] = servername mask
 *
 * or
 *
 * parv[0] = sender prefix
 * parv[1] = server to query
 * parv[2] = servername mask
 */
int m_links(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  char *mask;
  aClient *acptr;

  if (ocultar_servidores && !(IsAnOper(cptr) || IsHelpOp(cptr)))
  {
    /* Links especial solo mostrando services*/
    sendto_one(sptr, rpl_str(RPL_LINKS),
        me.name, parv[0], his.name, his.name, 0, 10, his.info);
    for (acptr = client; acptr; acptr = acptr->next)
    {
      if (!IsServer(acptr) && !IsMe(acptr))
        continue;
      if (!IsService(acptr))
        continue;
      sendto_one(sptr, rpl_str(RPL_LINKS),
          me.name, parv[0], acptr->name, his.name,
        1, acptr->serv->prot,
        (acptr->info[0] ? acptr->info : "(Unknown Location)"));
    }

    sendto_one(sptr, rpl_str(RPL_ENDOFLINKS), me.name, parv[0], "*");
    return 0;
  }

  if (parc > 2)
  {
    if (hunt_server(1, cptr, sptr, MSG_LINKS, TOK_LINKS, "%s :%s", 1, parc, parv) !=
        HUNTED_ISME)
      return 0;
    mask = parv[2];
  }
  else
    mask = parc < 2 ? NULL : parv[1];

  for (acptr = client, collapse(mask); acptr; acptr = acptr->next)
  {
    if (!IsServer(acptr) && !IsMe(acptr))
      continue;
    if (!BadPtr(mask) && match(mask, acptr->name))
      continue;
    sendto_one(sptr, rpl_str(RPL_LINKS),
        me.name, parv[0], acptr->name, acptr->serv->up->name,
#if !defined(GODMODE)
        acptr->hopcount, acptr->serv->prot,
#else /* GODMODE */
        acptr->hopcount, acptr->serv->prot, acptr->serv->timestamp,
        NumServ(acptr),
#endif /* GODMODE */
        (acptr->info[0] ? acptr->info : "(Unknown Location)"));
  }

  sendto_one(sptr, rpl_str(RPL_ENDOFLINKS), me.name, parv[0],
      BadPtr(mask) ? "*" : mask);
  return 0;
}

/*
 * m_help
 *
 * parv[0] = sender prefix
 */
int m_help(aClient *UNUSED(cptr), aClient *sptr, int UNUSED(parc), char *parv[])
{
  int i;

#if defined(WEBCHAT_HTML)
  return 0;
#endif

  for (i = 0; msgtab[i].cmd; i++)
    sendto_one(sptr, ":%s NOTICE %s :%s", me.name, parv[0], msgtab[i].cmd);
  return 0;
}

/* Counters of client/servers etc. */
struct lusers_st nrof;

void init_counters(void)
{
  memset(&nrof, 0, sizeof(nrof));
  nrof.servers = 1;
}

/*
 * m_users
 *
 * parv[0] = sender
 * parv[1] = ignored
 * parv[2] = server to query
 */
int m_users(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Reg1 struct tm *since_t = localtime(&me.since);
  char since[15];

  /* Solo ircops y opers tienen acceso a users remotos */
  if (parc > 2 && MyUser(sptr) && !IsAnOper(sptr) && !IsHelpOp(sptr))
    if (hunt_server(1, cptr, sptr, MSG_USERS, TOK_USERS, "%s :%s", 2, parc, parv) !=
        HUNTED_ISME)
      return 0;

  if (ocultar_servidores && MyUser(sptr) && !(IsAnOper(sptr) || IsHelpOp(sptr))) {
    sendto_one(sptr, ":%s 265 %s :Current local users: %d Max: %d",
        me.name, parv[0], nrof.clients - nrof.services, max_global_count - nrof.services);
    sendto_one(sptr, ":%s 266 %s :Current global users: %d Max: %d",
        me.name, parv[0], nrof.clients, max_global_count);
  }
  else
  {
    sprintf_irc(since, "%d%02d%02d-%02d:%02d", 1900 + since_t->tm_year,
        since_t->tm_mon + 1, since_t->tm_mday, since_t->tm_hour, since_t->tm_min);
    sendto_one(sptr, rpl_str(RPL_LOCALUSERS), me.name, parv[0],
        nrof.local_clients, max_client_count, date(max_client_count_TS), since);
    sendto_one(sptr, rpl_str(RPL_GLOBALUSERS), me.name, parv[0], nrof.clients,
        max_global_count, date(max_global_count_TS), since);
  }
  return 0;
}

/*
 * m_lusers
 *
 * parv[0] = sender
 * parv[1] = ignored
 * parv[2] = server to query
 */
int m_lusers(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  /* Solo ircops y opers tienen acceso a users remotos */
  if (parc > 2 && MyUser(sptr) && !IsAnOper(sptr) && !IsHelpOp(sptr))
    if (hunt_server(1, cptr, sptr, MSG_LUSERS, TOK_USERS, "%s :%s", 2, parc, parv) !=
        HUNTED_ISME)
      return 0;

  if (ocultar_servidores && MyUser(sptr) && !(IsAnOper(sptr) || IsHelpOp(sptr)))
    sendto_one(sptr, rpl_str(RPL_LUSERCLIENT), me.name, parv[0],
        nrof.clients - nrof.inv_clients, nrof.inv_clients, nrof.pservers + 1);
  else
    sendto_one(sptr, rpl_str(RPL_LUSERCLIENT), me.name, parv[0],
        nrof.clients - nrof.inv_clients, nrof.inv_clients, nrof.servers);
  if ((nrof.opers) || (nrof.helpers) || (nrof.bots_oficiales))
    sendto_one(sptr, rpl_str(RPL_LUSEROP), me.name, parv[0], nrof.helpers,
        nrof.opers, nrof.bots_oficiales);
  if (nrof.unknowns > 0)
    sendto_one(sptr, rpl_str(RPL_LUSERUNKNOWN), me.name, parv[0],
        nrof.unknowns);
  if (nrof.channels > 0)
    sendto_one(sptr, rpl_str(RPL_LUSERCHANNELS), me.name, parv[0],
        nrof.channels);
  if (ocultar_servidores && MyUser(sptr) && !(IsAnOper(sptr) || IsHelpOp(sptr)))
    sendto_one(sptr, rpl_str(RPL_LUSERME), me.name, parv[0], nrof.clients - nrof.services,
        nrof.pservers);
  else
    sendto_one(sptr, rpl_str(RPL_LUSERME), me.name, parv[0], nrof.local_clients,
        nrof.local_servers);
  if (ocultar_servidores && MyUser(sptr) && !(IsAnOper(sptr) || IsHelpOp(sptr)))
    sendto_one(sptr, rpl_str(RPL_STATSCONN), me.name, parv[0],
        max_global_count - nrof.services, max_global_count - nrof.services - nrof.pservers);
  else
    sendto_one(sptr, rpl_str(RPL_STATSCONN), me.name, parv[0],
        max_connection_count, max_client_count);

  return 0;
}

/*
 * m_admin
 *
 * parv[0] = sender prefix
 * parv[1] = servername
 */
int m_admin(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aConfItem *aconf;

  if (MyConnect(sptr) && parc > 1)
  {
    aClient *acptr;
    if (!(acptr = find_match_server(parv[1])))
    {
      sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name, parv[0], parv[1]);
      return 0;
    }
    parv[1] = acptr->name;
  }
  if (hunt_server(0, cptr, sptr, MSG_ADMIN, TOK_ADMIN, ":%s", 1, parc, parv) != HUNTED_ISME)
    return 0;
  if ((aconf = find_admin()))
  {
    sendto_one(sptr, rpl_str(RPL_ADMINME), me.name, parv[0], me.name);
    sendto_one(sptr, rpl_str(RPL_ADMINLOC1), me.name, parv[0], aconf->host);
    sendto_one(sptr, rpl_str(RPL_ADMINLOC2), me.name, parv[0], aconf->passwd);
    sendto_one(sptr, rpl_str(RPL_ADMINEMAIL), me.name, parv[0], aconf->name);
  }
  else
    sendto_one(sptr, err_str(ERR_NOADMININFO), me.name, parv[0], me.name);
  return 0;
}

/*
 * m_motd
 *
 * parv[0] - sender prefix
 * parv[1] - servername
 *
 * modified 30 mar 1995 by flux (cmlambertus@ucdavis.edu)
 * T line patch - display motd based on hostmask
 * modified again 7 sep 97 by Ghostwolf with code and ideas 
 * stolen from comstud & Xorath.  All motd files are read into
 * memory in read_motd() in s_conf.c
 *
 * When NODEFAULTMOTD is defined, then it is possible that
 * sptr == NULL, which means that this function is called from
 * register_user.
 */
int m_motd(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  time_t *TS = &motd_TS;     /* Default: Most general case */
  atrecord *ptr;
  int count;
  aMotdItem *temp;
  int i = 0;
  struct db_reg *reg;
  char tmp_str[16];

#if defined(NODEFAULTMOTD)
  int no_motd;

  if (sptr)
  {
    no_motd = 0;
#endif
    if (hunt_server(0, cptr, sptr, MSG_MOTD, TOK_MOTD, ":%s", 1, parc,
        parv) != HUNTED_ISME)
      return 0;
#if defined(NODEFAULTMOTD)
  }
  else
  {
    sptr = cptr;
    no_motd = 1;
  }
#endif

  /*
   * Find out if this is a remote query or if we have a T line for our hostname
   */
  if (IsServer(cptr))
  {
    TS = NULL;                  /* Remote MOTD */
    temp = rmotd;
  }
  else
  {
    for (ptr = tdata; ptr; ptr = ptr->next)
    {
      if (!match(ptr->hostmask, PunteroACadena(cptr->sockhost)))
        break;
    }
    if (ptr)
    {
      temp = ptr->tmotd;
      TS = &ptr->tmotd_TS;
    }
    else
      temp = motd;
  }

  reg = db_buscar_registro(BDD_CONFIGDB, "motd.0");

  if (temp == NULL && (!(reg) || !strcmp(reg->valor, "%LOCALMOTD")))
  {
   /** Sabemos seguro que no tenemos ircd.motd:
    ** Sólo pasamos por aquí si tenemos seteado
    ** %LOCALMOTD, o si no existe el registro.
    **
    ** 27/01/04: mount@irc-dev.net -- u2.10.H.08.109
    **/
    sendto_one(sptr, err_str(ERR_NOMOTD), me.name, parv[0]);
    return 0;
  }
#if defined(NODEFAULTMOTD)
  if (!no_motd)
  {
#endif
    if (TS)                     /* Not remote? */
    {
      sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, parv[0], me.name);
      sendto_one(sptr, ":%s %d %s :- %s", me.name, RPL_MOTD,
          parv[0], date(*TS));
      count = 100;
    }
    else
      count = 3;

    while (reg)
    {
      if (!strcmp(reg->valor, "%LOCALMOTD"))
      {
        for (; temp; temp = temp->next)
        {
          sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0], temp->line);
          if (--count == 0)
            break;
        }
      }
      else
        sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0], reg->valor);
      sprintf(tmp_str, "motd.%d", ++i);
      reg = db_buscar_registro(BDD_CONFIGDB, tmp_str);
    }

    if (!i)
      for (; temp; temp = temp->next)
      {
        sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0], temp->line);
        if (--count == 0)
          break;
      }

#if defined(NODEFAULTMOTD)
  }
  else
  {
    assert(TS);
    sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, parv[0], me.name);
    sendto_one(sptr, ":%s %d %s :%s", me.name, RPL_MOTD, parv[0],
        "Type /MOTD to read the AUP before continuing using this service.");
    sendto_one(sptr,
        ":%s %d %s :The message of the day was last changed: %s", me.name,
        RPL_MOTD, parv[0], date(*TS));
  }
#endif
  sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, parv[0]);
  return 0;
}

