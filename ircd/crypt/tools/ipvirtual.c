/*
 * IRC - Internet Relay Chat, ircd/crypt/tea/ipvirtual.c
 * Copyright (C) 2002 IRC-Hispano.org - ESNET - zoltan
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

/*
 * Programa para cifrar una IP virtual a partir de una IP real.
 *
 * -- zoltan
 *
 * $Id$
 */


#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#ifdef _WIN32
# include <winsock.h>
#else
# include <netinet/in.h>
#endif
#include "misc.h"
#include "tea.h"


int main(int argc, char *argv[])
{
    unsigned int v[2], k[2], x[2];
    int ts=0;
    char clave[12 + 1];
    char virtualhost[22];
    struct hostent *hp;
    struct in_addr addr;

    if(argc!=3) {
        printf("Uso: %s password IP\n", argv[0]);
        return 1;
    }

    hp = gethostbyname((char *)argv[2]);

    if (hp==NULL) {
      return 1;
    }

    memcpy(&addr,hp->h_addr,hp->h_length);

    strncpy(clave, argv[1], 12);
    clave[12] = '\0';

    /* resultado */
    x[0] = x[1] = 0;

    while (1)
    {
      char tmp;

    /* resultado */
    x[0] = x[1] = 0;

    /* valor */
    tmp = clave[6];
    clave[6] = '\0';
    k[0] = base64toint(clave);
    clave[6] = tmp;
    k[1] = base64toint(clave + 6);

    v[0] = (k[0] & 0xffff0000) + ts;
    v[1] = ntohl((unsigned long)addr.s_addr);

    tea(v, k, x);

    /* formato direccion virtual: qWeRty.AsDfGh.virtual */
    inttobase64(virtualhost, x[0], 6);
    virtualhost[6] = '.';
    inttobase64(virtualhost + 7, x[1], 6);
    strcpy(virtualhost + 13, ".virtual");

    /* el nombre de Host es correcto? */
    if (!strchr(virtualhost, '[') &&
        !strchr(virtualhost, ']'))
      break;                    /* nice host name */
    else
    {
      if (++ts == 65536)
      {                         /* No deberia ocurrir nunca */
        strcpy(virtualhost, "Soy pepe");
        break;
      }
    }
  }
  printf("Resultado: %s\n", virtualhost);
  exit(EXIT_SUCCESS);
}
