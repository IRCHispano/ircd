/*
 * IRC - Internet Relay Chat, ircd/map.c
 * Copyright (C) 1994 Carlo Wood ( Run @ undernet.org )
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
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
#include "match.h"
#include "list.h"
#include "s_err.h"
#include "ircd.h"
#include "s_bsd.h"
#include "s_misc.h"
#include "querycmds.h"
#include "map.h"
#include "numnicks.h"
#include "s_bdd.h"
#include "s_serv.h"

RCSTAG_CC("$Id$");

static void dump_map(struct Client *cptr, struct Client *server, char *mask,
    int prompt_length)
{
  static char prompt[64];
  Dlink *lp;
  char *p = &prompt[prompt_length];
  int cnt = 0;
  char buf[16];

  *p = '\0';
  if (prompt_length > 60)
    sendto_one(cptr, rpl_str(RPL_MAPMORE), me.name, cptr->name,
        prompt, server->name);
  else
  {
    char lag[512];
    unsigned int clientes;
    unsigned int clientes_float;

    clientes = nrof.clients;
    if (!clientes)
      clientes = 1;             /* Evitamos division por cero */

    if (server->serv->lag > 10000)
      lag[0] = 0;
    else if (server->serv->lag < 0)
      strcpy(lag, "(0s)");
    else
      sprintf(lag, "(%is)", server->serv->lag);
    sprintf(buf, "%s:%d", NumServ(server), base64toint(NumServ(server)));
    clientes_float =
        (int)((1000.0 * (server ==
        &me ? nrof.local_clients : server->serv->clients)) / clientes + 0.5);
    sendto_one(cptr, rpl_str(RPL_MAP), me.name, cptr->name, prompt,
        ((IsBurstOrBurstAck(server)) ? "*" : ""), server->name, buf, lag,
        (server == &me) ? nrof.local_clients : server->serv->clients,
        clientes_float / 10, clientes_float % 10);
  }
  if (prompt_length > 0)
  {
    p[-1] = ' ';
    if (p[-2] == '`')
      p[-2] = ' ';
  }
  if (prompt_length > 60)
    return;
  strcpy(p, "|-");
  for (lp = server->serv->down; lp; lp = lp->next)
    if (match(mask, lp->value.cptr->name))
      lp->value.cptr->flags &= ~FLAGS_MAP;
    else
    {
      lp->value.cptr->flags |= FLAGS_MAP;
      cnt++;
    }
  for (lp = server->serv->down; lp; lp = lp->next)
  {
    if ((lp->value.cptr->flags & FLAGS_MAP) == 0)
      continue;
    if (--cnt == 0)
      *p = '`';
    dump_map(cptr, lp->value.cptr, mask, prompt_length + 2);
  }
  if (prompt_length > 0)
    p[-1] = '-';
}

/*
 * m_map  -- by Run
 *
 * parv[0] = sender prefix
 * parv[1] = server mask
 */
int m_map(aClient *UNUSED(cptr), aClient *sptr, int parc, char *parv[])
{

  if (ocultar_servidores && !(IsAnOper(cptr) || IsHelpOp(cptr)))
  {
    aClient *acptr;
    unsigned int clientes;
    unsigned int clientes_float;
    int numps = 0;

    /* MAP Especial solo mostrando services */

    clientes = nrof.clients - nrof.services;
    clientes_float = (int)((1000.0 * clientes) / nrof.clients + 0.5);

    sendto_one(sptr, ":%s 015 %s :%s [%i clientes - %i.%i%%]",
        me.name, parv[0], his.name, clientes,
        clientes_float / 10, clientes_float % 10);

    for (acptr = client; acptr; acptr = acptr->next)
    {
      if (!IsServer(acptr) && !IsMe(acptr))
        continue;
      if (!IsService(acptr))
        continue;

      numps++;
      clientes = acptr->serv->clients;
      clientes_float = (int)((1000.0 * clientes) / nrof.clients + 0.5);

      if (numps < nrof.pservers)
        sendto_one(sptr, ":%s 015 %s :|-%s [%i clientes - %i.%i%%]",
            me.name, parv[0], acptr->name, clientes,
            clientes_float / 10, clientes_float % 10);
      else
        sendto_one(sptr, ":%s 015 %s :`-%s [%i clientes - %i.%i%%]",
            me.name, parv[0], acptr->name, clientes,
            clientes_float / 10, clientes_float % 10);
    }
    sendto_one(sptr, rpl_str(RPL_MAPEND), me.name, parv[0]);
    return 0;
  }

  if (parc < 2)
    parv[1] = "*";

  dump_map(sptr, &me, parv[1], 0);
  sendto_one(sptr, rpl_str(RPL_MAPEND), me.name, parv[0]);

  return 0;
}
