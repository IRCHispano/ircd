/*
 * IRC - Internet Relay Chat, ircd/m_watch.c
 * Copyright (C) 2002 IRC-Hispano.org - ESNET - jcea & zoltan
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
#include "hash.h"
#include "ircd.h"
#include "list.h"
#include "msg.h"
#include "numeric.h"
#include "s_bsd.h"
#include "s_err.h"
#include "s_user.h"
#include "send.h"
#include "struct.h"
#include "support.h"
#include "m_watch.h"

#include <assert.h>

#if defined(WATCH)
/*
** El mIRC (al menos hasta el 20020530) envia todos los WATCH
** de golpe al conectarnos, independientemente de que le tengamos
** puesta la opcion de control de FLOOD o no. Si nuestra lista
** de WATCH es muy larga, nos podemos caer por flood con facilidad.
**
** En realidad las cuentas que siguen no son muy reales, porque
** no todos los nicks especificados usan la longitud maxima, ni se
** contabiliza el espacio de separacion y el "+" al principio de
** cada nick, pero sirva como referencia aproximada.
**
** Considero que se trata de un bug del mIRC, que NiKoLaS ya ha
** comunicado a su autor pero, mientras tanto, hay que vivir con ello.
*/
#if (MAXWATCH*NICKLEN) > CLIENT_FLOOD
/* #error El numero de entradas WATCH especificadas puede provocar caida 
de los clientes por flood. Reduzca MAXWATCH o incremente CLIENT_FLOOD en 'make config'.
*/
#endif
#endif

#if defined(WATCH)
/*
 * FUNCIONES DE WATCH
 *
 * Esquema del funcionamiento de la lista de WATCH.
 *
 * LISTA_WATCH
 * |
 * |-wptr1-|- sptr1
 * |       |- sptr2
 * |       |- sptr3
 * |
 * |-wptr2-|- sptr2
 *         |- sptr1
 *
 * LINKS en las listas aClient.
 *
 * sptr1            sptr2           sptr3
 * |- wptr1(nickA)  |-wptr1(nickA)  |-wptr1(nickA)
 * |- wptr2(nickB)  |-wptr2(nickB)
 *
 * El funcionamiento se basa en el WATCH de Bahamut y UnrealIRCD.
 *
 * 2002/05/20 zoltan <zoltan@irc-dev.net>
 */

/*
 * chequea_estado_watch()
 *
 * Avisa a los usuarios la entrada/salida de un nick.
 */
void chequea_estado_watch(aClient *sptr, int raw)
{
  Reg1 aWatch *wptr;
  Reg2 Link *lp;
  char *username;

/*
** Ocurre cuando el usuario no completa
** correctamente su entrada en el IRC
*/
  if (!IsUser(sptr))
    return;

  wptr = FindWatch(sptr->name);

  if (!wptr)
    return;                     /* No esta en ningun notify */

  wptr->lasttime = TStime();

  username = PunteroACadena(sptr->user->username);

  /*
   * Mandamos el aviso a todos los usuarios
   * que lo tengan en el notify.
   */
  for (lp = wptr->watch; lp; lp = lp->next)
    sendto_one(lp->value.cptr, watch_str(raw), me.name, lp->value.cptr->name,
        sptr->name, username,
#if defined(BDD_VIP)
        get_visiblehost(sptr, lp->value.cptr),
#else
        sptr->user->host,
#endif
        wptr->lasttime);
}


/*
 * muestra_estado_watch()
 *
 * Muestra el estado de un usuario.
 */
static void muestra_estado_watch(aClient *sptr, char *nick, int raw1, int raw2)
{
  aClient *acptr;

  if ((acptr = FindClient(nick)))
  {
    sendto_one(sptr, watch_str(raw1), me.name, sptr->name,
        acptr->name, PunteroACadena(acptr->user->username),
#if defined(BDD_VIP)
        get_visiblehost(acptr, sptr),
#else
        acptr->user->host,
#endif
        acptr->lastnick);
  }
  else
    sendto_one(sptr, watch_str(raw2), me.name, sptr->name, nick, "*", "*", 0);
}


/*
 * agrega_nick_watch()
 *
 * Agrega un nick a la lista de watch del usuario.
 */
static int agrega_nick_watch(aClient *sptr, char *nick)
{
  aWatch *wptr;
  Link *lp;

  /*
   * Si no existe, creamos el registro.
   */
  if (!(wptr = FindWatch(nick)))
  {
    wptr = make_watch(nick);
    if (!wptr)
      return 0;
    wptr->lasttime = TStime();
  }

  /*
   * Buscar si ya lo tiene en el watch.
   */
  if ((lp = wptr->watch))
  {
    while (lp && (lp->value.cptr != sptr))
      lp = lp->next;
  }

  /*
   * No esta, entonces lo agregamos.
   */
  if (!lp)
  {
    /*
     * Link en lista watch al sptr
     */
    lp = wptr->watch;
    wptr->watch = make_link();
    memset(wptr->watch, 0, sizeof(Link));
    wptr->watch->value.cptr = sptr;
    wptr->watch->next = lp;

    /*
     * Link en lista user al watch
     */
    lp = make_link();
    memset(lp, 0, sizeof(Link));
    lp->next = sptr->user->watch;
    lp->value.wptr = wptr;
    sptr->user->watch = lp;
    sptr->user->cwatch++;

  }
  return 0;
}


/*
 * borra_nick_watch()
 *
 * Borra un nick de la lista de watch de un usuario.
 */
static int borra_nick_watch(aClient *sptr, char *nick)
{
  aWatch *wptr;
  Link *lp, *lptmp = NULL;

  wptr = FindWatch(nick);
  if (!wptr)
    return 0;                   /* No esta en ninguna lista */

  /*
   * Buscamos en el link del watch al nick
   */
  if ((lp = wptr->watch))
  {
    while (lp && (lp->value.cptr != sptr))
    {
      lptmp = lp;
      lp = lp->next;
    }
  }

  if (!lp)
    return 0;                   /* No lo tenia */

  if (!lptmp)
    wptr->watch = lp->next;
  else
    lptmp->next = lp->next;

  free_link(lp);

  /*
   * Buscamos en el link del usuario al watch
   */
  lptmp = lp = NULL;
  if ((lp = sptr->user->watch))
  {
    while (lp && (lp->value.wptr != wptr))
    {
      lptmp = lp;
      lp = lp->next;
    }
  }

  if (!lp)                      /* No deberia ocurrir */
    sendto_ops("ERROR de WATCH, entrada watch sin link a cliente");
  else
  {
    if (!lptmp)
      sptr->user->watch = lp->next;
    else
      lptmp->next = lp->next;

    free_link(lp);
  }

  /*
   * Si era el unico asociado al nick
   * borramos registro en la tabla de watchs.
   */
  if (!wptr->watch)
    free_watch(wptr);

  /* Actualizamos contador de watch's del usuario */
  sptr->user->cwatch--;

  return 0;

}


/*
 * borra_lista_watch()
 *
 * Borra toda la lista de watch de un usuario.
 * Al hacer un /WATCH C  o porque sale del irc.
 */
int borra_lista_watch(aClient *sptr)
{
  Link *lp, *lp2, *lptmp;
  aWatch *wptr;

  if (!(lp = sptr->user->watch))
    return 0;                   /* Tenia la lista vacia */

  /*
   * Bucle de los links del usuario al watch
   */
  while (lp)
  {
    wptr = lp->value.wptr;
    lptmp = NULL;
    for (lp2 = wptr->watch; lp2 && (lp2->value.cptr != sptr); lp2 = lp2->next)
      lptmp = lp2;

    if (!lp2)                   /* No deberia ocurrir */
      sendto_ops("ERROR de WATCH, entrada watch sin link a cliente");
    else
    {
      if (!lptmp)
        wptr->watch = lp2->next;
      else
        lptmp->next = lp2->next;
      free_link(lp2);

      /*
       * Si era el unico asociado al nick
       * borramos registro en la tabla de watchs.
       */
      if (!wptr->watch)
      {
        free_watch(wptr);
      }
    }
    lp2 = lp;
    lp = lp->next;
    free_link(lp2);
  }
  /*
   * Reseteamos valores watch del usuario
   */
  sptr->user->watch = NULL;
  sptr->user->cwatch = 0;

  return 0;
}


/*
 * Comando WATCH
 *
 *   parv[0] = sender prefix
 *   parv[1] = parametros
 *
 * En el parv[1] se puede separar parametros con "," o " " o ambos.
 * Si un parametro empieza por '+', se agrega un nick.
 * Y si empieza por '-' se borra un nick.
 * Si se manda un 'C' o 'c', resetea la lista de watch.
 * Un 'S' o 's', da el estado del notify.
 * El parametro 'l' lista los nicks on-line y 'L' ademas de los
 * on-line, los off-line.
 * 
 * Si no hay parv[1], es un "WATCH l".
 *
 * 2002/05/20 zoltan <zoltan@irc-dev.net>
 */

int m_watch(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  char *s, *p = NULL;

  if (!IsUser(sptr))            /* Comando solo de usuarios */
    return 0;

  if (parc < 2)
  {
    /* Parametro de por defecto 'l' */
    parc = 2;
    parv[1] = "l";
  }

  /*
   * Los parametros pueden estar separados por " " o por "," o ambos.
   */
  for (s = strtoken(&p, parv[1], ", "); s; s = strtoken(&p, NULL, ", "))
  {
    /*
     * Prefijos "+" (agregar) "-" (quitar)
     */
    if (*s == '+' || *s == '-')
    {
      char *nick, *p2;
      char c = *s;

      *s++ = '\0';

      /* Si nos llega nick!user@host lo cortamos */
      if (!(nick = strtoken(&p2, s, "!")))
        nick = s;               /* No tiene '!' */

      /* No admitimos servidores, ni arrobas, ni comodines */
      if (strchr(nick, '*') || strchr(nick, '.') || strchr(nick, '@'))
        continue;

      /* Solo aceptamos longitud de "NICKLEN" caracteres */
      if (strlen(nick) > nicklen)
        nick[nicklen] = '\0';

      if (!*nick)
        continue;

      if (c == '+')             /* Agregar nick */
      {
        /*
         * Comprobamos si la lista esta llena o no.
         */
        if (cptr->user->cwatch >= MAXWATCH)
        {
          sendto_one(cptr, err_str(ERR_TOOMANYWATCH), me.name,
              cptr->name, nick, MAXWATCH);
          continue;
        }
        agrega_nick_watch(cptr, nick);

        /*
         * Decirle si esta on o off line
         */
        muestra_estado_watch(sptr, nick, RPL_NOWON, RPL_NOWOFF);

      }
      else if (c == '-')        /* Borrar nick */
      {
        borra_nick_watch(cptr, nick);
        /*
         * Decirle que se ha borrado.
         */
        muestra_estado_watch(sptr, nick, RPL_WATCHOFF, RPL_WATCHOFF);
      }
      continue;
    }

    /*
     * Parametro C/c
     *
     * Borra toda la lista de WATCH.
     */
    if (*s == 'C' || *s == 'c')
    {
      borra_lista_watch(cptr);
      continue;
    }

    /*
     * Parametro S/s
     *
     * Status de la lista de watch. Nos dice cuantas
     * entradas tenemos, y el numero de usuarios
     * que tienen nuestro nick en el watch local.
     */
    if (*s == 'S' || *s == 's')
    {
#if 0
      aWatch *wptr;
#endif
      Link *lp;
      char linea[MAXLEN * 2];
      int count = 0;

#if 0
      /*
       * Desactivado el mostrar el numero
       * de gente que nos tienen en sus
       * notifys.
       */
      wptr = FindWatch(sptr->name);

      if (wptr)
        for (lp = wptr->watch, count = 1; (lp = lp->next); count++);

      sendto_one(sptr, watch_str(RPL_WATCHSTAT), me.name, parv[0],
          sptr->user->cwatch, count);
#else
      sendto_one(sptr, watch_str(RPL_WATCHSTAT), me.name, parv[0],
          sptr->user->cwatch);
#endif

      /*
       * Enviamos todas las entradas de la lista de WATCH
       */
      lp = sptr->user->watch;
      if (lp)
      {
        /*
         * Controlamos la linea, si es muy larga, cortarlo y 
         * mandar en mas lineas.
         */
        *linea = '\0';
        strcpy(linea, lp->value.wptr->nick);
        count = strlen(parv[0]) + strlen(me.name) + 10 + strlen(linea);
        while ((lp = lp->next))
        {
          if ((count + strlen(lp->value.wptr->nick) + 1) > MAXLEN)
          {
            sendto_one(sptr, watch_str(RPL_WATCHLIST), me.name, parv[0], linea);
            *linea = '\0';
            count = strlen(parv[0]) + strlen(me.name) + 10;
          }
          strcat(linea, " ");
          strcat(linea, lp->value.wptr->nick);
          count += (strlen(lp->value.wptr->nick) + 1);
        }
        sendto_one(sptr, watch_str(RPL_WATCHLIST), me.name, parv[0], linea);
      }
      sendto_one(sptr, watch_str(RPL_ENDOFWATCHLIST), me.name, parv[0], *s);

      continue;
    }

    /*
     * Parametro L/l
     *
     * Lista de WATCH, se lista los usuarios on-line y
     * si especificamos "L" tambien los off-line.
     */
    if (*s == 'L' || *s == 'l')
    {
      aClient *acptr;
      Link *lp = sptr->user->watch;

      while (lp)
      {
        if ((acptr = FindClient(lp->value.wptr->nick)))
        {
          sendto_one(sptr, watch_str(RPL_NOWON), me.name, parv[0],
              acptr->name, PunteroACadena(acptr->user->username),
#if defined(BDD_VIP)
              get_visiblehost(acptr, cptr),
#else
              acptr->user->host,
#endif
              acptr->lastnick);
        }
        /*
         * Si especifica "L" mandar tambien los off-line.
         */
        else if (*s == 'L')
        {
          sendto_one(cptr, watch_str(RPL_NOWOFF), me.name, cptr->name,
              lp->value.wptr->nick, "*", "*", lp->value.wptr->lasttime);
        }

        lp = lp->next;
      }

      sendto_one(sptr, watch_str(RPL_ENDOFWATCHLIST), me.name, parv[0], *s);
      continue;
    }

    /* Parametro desconocido o no soportado,
     * lo ignoramos :) 
     */

  }                             /* Final del for del strtoken */
  return 0;
}

#endif /* WATCH */
