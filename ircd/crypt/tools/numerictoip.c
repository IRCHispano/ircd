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
 * Programa para pasar de Numerico a IP.
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
#ifdef DEBUG
    unsigned int debug;
    int i;
#endif

    if (argc != 2) {
        printf("Uso: %s numerico\n", argv[0]);
        return 1;
    }

    base64toip(argv[1], &ip);

#ifdef DEBUG
    for (i = 0; i < 8; i++)
        printf("Valor de in6_16[%d]: %u\n", i, ip.in6_16[i]);

    if (strlen(argv[1]) == 6) {
        debug = base64toint(argv[1]);
        printf("Valor de int: %u\n", debug);
    }
#endif

    printf("Conversion: %s => %s\n", argv[1], ircd_ntoa(&ip));
    exit(EXIT_SUCCESS);
}
