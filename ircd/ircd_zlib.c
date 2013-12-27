/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ircd_zlib.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2003 Jesus Cea Avion <jcea@jcea.es>
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
 * @brief Interfaces and declarations for ZLIB.
 * @version $Id: dbuf.h,v 1.7 2007-12-11 23:38:23 zolty Exp $
 */
#include "config.h"

#if defined(USE_ZLIB)
#include "ircd_zlib.h"
#include "client.h"
#include "ircd_alloc.h"
#include "s_bsd.h"

#include <stdlib.h>

int microburst = 0;
struct zlib_mburst *p_microburst = NULL;
struct zlib_mburst *p_microburst_cache = NULL;

void zlib_microburst_init(void)
{
  microburst++;
}

void zlib_microburst_complete(void)
{
  struct zlib_mburst *p, *p2;
  static int ciclos_mburst = 0;

  /* Deberian estar anidados, pero por si acaso */
  if (!microburst)
    return;

  if (!(--microburst))
  {
    for (p = p_microburst; p; p = p2)
    {
      /*
       * p->cptr puede ser NULL si la
       * conexion se ha cerrado durante
       * la "microrafaga".
       */
      if ((p->cptr != NULL) && MyConnect(p->cptr)
          && (cli_connect(p->cptr)->zlib_negociation & ZLIB_OUT) && (p->dyn != NULL))
      {
        dbuf_put(p->cptr, p->dyn, NULL, 0);
        update_write(p->cptr);
      }
      p2 = p->next;
      if (++ciclos_mburst >= 937)
      {                         /* Numero primo */
        ciclos_mburst = 1;
        MyFree(p);
      }
      else
      {
        p->next = p_microburst_cache;
        p_microburst_cache = p;
      }
    }
    p_microburst = NULL;
  }
}


void zlib_microburst_start(void)
{
  if (!microburst)
    return;                     /* Esto es lo ideal */
  /*
   * Si quedan rafagas abiertas, hay que investigarlo
   */
  microburst = 1;
  zlib_microburst_complete();
}

void zlib_microburst_delete(struct Client *cptr)
{
  struct zlib_mburst *p;

  for (p = p_microburst; p; p = p->next)
  {
    if (p->cptr == cptr)
    {                           /* Lo ideal seria eliminar el registro */
      p->cptr = NULL;
    }
  }
}

#endif /* USE_ZLIB */
