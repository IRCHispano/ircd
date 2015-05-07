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
 * Programa para pasar de IP a Numerico.
 *
 * -- zoltan
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


int main(int argc, char *argv[])
{
    struct irc_in_addr ip;
    unsigned char bits;
    char ip_base64[25];
#ifdef DEBUG
    int i;
#endif

    if (argc != 2) {
        printf("Uso: %s IP\n", argv[0]);
        return 1;
    }

    if (!ipmask_parse(argv[1], &ip, &bits)) {
        printf("Formato de IP incorrecto\n");
        return 1;
    }

#ifdef DEBUG
    for (i = 0; i < 8; i++)
        printf("Valor de in6_16[%d]: %u\n", i, ip.in6_16[i]);
#endif

    iptobase64(ip_base64, &ip, sizeof(ip_base64), 1);

    printf("Conversion: %s => %s\n", ircd_ntoa(&ip), ip_base64);
    exit(EXIT_SUCCESS);
}
