/*
 * IRC - Internet Relay Chat, ircd/crypt/tea/cifranick.c
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
 * Programa para generar las claves cifradas de nicks
 * para introducirlos en la tabla 'n' del ircd.
 *
 * -- zoltan
 */


#include <stdio.h>
#include <string.h>
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

    if (argc != 3)
    {
      printf("Uso: cifranick nick password\n");
      return 1;
    }

    char *nick = argv[1];
    int longitud_nick = strlen(nick);
    /* Para nicks <16 uso cont 2 para el resto lo calculo */
    int cont=(longitud_nick < 16) ? 2 : ((longitud_nick + 8) / 8);

    char tmpnick[8 * cont + 1];
    char tmppass[12 + 1];
    unsigned int *p = (unsigned int *)tmpnick; /* int == 32 bits */

    char clave[12 + 1];                /* Clave encriptada */
    int i = 0;

    /* Normalizar nick */
    while (nick[i] != 0)
    {
       nick[i] = toLower((int) nick[i]);
       i++;
    }

    memset(tmpnick, 0, sizeof(tmpnick));
    strncpy(tmpnick, nick ,sizeof(tmpnick) - 1);

    memset(tmppass, 0, sizeof(tmppass));
    strncpy(tmppass, argv[2], sizeof(tmppass) - 1);

    /* relleno   ->   123456789012 */
    strncat(tmppass, "AAAAAAAAAAAA", sizeof(tmppass) - strlen(tmppass) -1);

    x[0] = x[1] = 0;

    k[1] = base64toint(tmppass + 6);
    tmppass[6] = '\0';
    k[0] = base64toint(tmppass);

    while(cont--)
    {
      v[0] = ntohl(*p++);      /* 32 bits */
      v[1] = ntohl(*p++);      /* 32 bits */
      tea(v, k, x);
    }

    inttobase64(clave, x[0], 6);
    inttobase64(clave + 6, x[1], 6);

    printf("Clave cifrada para %s es %s\n", nick, clave);

    exit(EXIT_SUCCESS);
}
