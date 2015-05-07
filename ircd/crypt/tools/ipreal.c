/*
 * IRC - Internet Relay Chat, ircd/crypt/tea/ìpreal.c
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
 * Programa para sacar la IP Real a partir de una IP Virtual.
 *
 * -- zoltan
 *
 * $Id: ipreal.c 256 2008-12-01 14:40:06Z dfmartinez $
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
    struct irc_in_addr ip;
    int lenvirtual;
    char tmpvirt[7];
    char clave[12+1];
    char tmp;

    if (argc != 3) {
        printf("Uso: %s password ip_virtual\n", argv[0]);
        return 1;
    }

    memset(&ip, 0, sizeof(struct irc_in_addr));

    lenvirtual = strlen(argv[2]);

    /* IPv4 AjyuN7.C9aa7L.virtual 21 caracteres */
    if (lenvirtual == 21) {
        if (argv[2][6] != '.' || argv[2][13] != '.' || !strstr(argv[2], ".virtual")) {
            printf("Formato de Vhost incorrecto\n");
            return 1;
        }

        strncpy(clave, argv[1], 12);
        clave[12] = '\0';

        strncpy(tmpvirt, argv[2], 6);
        tmpvirt[6] = '\0';

        v[0] = base64toint(tmpvirt);
        strncpy(tmpvirt, argv[2] + 7, 6);
        tmpvirt[6] = '\0';
        v[1] = base64toint(tmpvirt);

        /* resultado */
        x[0] = x[1] = 0;

        /* valor */
        tmp = clave[6];
        clave[6] = '\0';
        k[0] = base64toint(clave);
        clave[6] = tmp;
        k[1] = base64toint(clave + 6);

        untea(v, k, x);

        ip.in6_16[5] = htons(65535);
        ip.in6_16[6] = htons(x[1] >> 16);
        ip.in6_16[7] = htons(x[1] & 65535);

    /* IPv6 AjyuNL.C9aaN7.v6 16 caracteres */
    } else if (lenvirtual == 16) {
        if (argv[2][6] != '.' || argv[2][13] != '.' || !strstr(argv[2], ".v6")) {
            printf("Formato de Vhost incorrecto\n");
            return 1;
        }

        strncpy(clave, argv[1], 12);
        clave[12] = '\0';

        strncpy(tmpvirt, argv[2], 6);
        tmpvirt[6] = '\0';

        v[0] = base64toint(tmpvirt);
        strncpy(tmpvirt, argv[2] + 7, 6);
        tmpvirt[6] = '\0';
        v[1] = base64toint(tmpvirt);

        /* resultado */
        x[0] = x[1] = 0;

        /* valor */
        tmp = clave[6];
        clave[6] = '\0';
        k[0] = base64toint(clave);
        clave[6] = tmp;
        k[1] = base64toint(clave + 6);

        untea(v, k, x);

        ip.in6_16[0] = htons(x[0] >> 16);
        ip.in6_16[1] = htons(x[0] & 65535);
        ip.in6_16[2] = htons(x[1] >> 16);
        ip.in6_16[3] = htons(x[1] & 65535);

    } else {
        printf("Formato de Vhost incorrecto\n");
        return 1;
    }

    printf("Resultado: %s => %s\n", argv[2], ircd_ntoa(&ip));
    exit(EXIT_SUCCESS);
}
