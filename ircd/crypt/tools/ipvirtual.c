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
 * $Id: ipvirtual.c 256 2008-12-01 14:40:06Z dfmartinez $
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
    int ts = 0;
    int i = 0;
    char clave[12 + 1];
    char virtualhost[22];
    struct irc_in_addr ip;
    unsigned char bits;

    if(argc != 3) {
        printf("Uso: %s password IP\n", argv[0]);
        return 1;
    }

    if (!ipmask_parse(argv[2], &ip, &bits)) {
        printf("Formato de IP incorrecto\n");
        return 1;
    }

    for (i = 0; i < 8; i++)
        printf("Valor de in6_16[%d]: %u\n", i, ip.in6_16[i]);

    strncpy(clave, argv[1], 12);
    clave[12] = '\0';

    /* IPv4 */
    if (irc_in_addr_is_ipv4(&ip))
    {
        do {
            char tmp;

            x[0] = x[1] = 0;

            tmp = clave[6];
            clave[6] = '\0';
            k[0] = base64toint(clave);
            clave[6] = tmp;
            k[1] = base64toint(clave + 6);

            v[0] = (k[0] & 0xffff0000) + ts;
            v[1] = (ntohs(ip.in6_16[6]) << 16) | ntohs(ip.in6_16[7]);

            tea(v, k, x);

            /* Virtualhost format: qWeRty.AsDfGh.v4 */
            inttobase64(virtualhost, x[0], 6);
            virtualhost[6] = '.';
            inttobase64(virtualhost + 7, x[1], 6);
            strncpy(virtualhost + 13, ".virtual", 64);

            /* No deber�a ocurrir nunca... */
            if (++ts == 65535)
            {
                strncpy(virtualhost, ircd_ntoa(&ip), 64);
                strncat(virtualhost, ".virtual", 64);
                break;
            }
        } while (strchr(virtualhost, ']') || strchr(virtualhost, '['));
    }
    else /* IPv6 */
    {
        char tmp;

        x[0] = x[1] = 0;

        tmp = clave[6];
        clave[6] = '\0';
        k[0] = base64toint(clave);
        clave[6] = tmp;
        k[1] = base64toint(clave + 6);

        v[0] = ntohs((unsigned long)ip.in6_16[0]) << 16 | ntohs((unsigned long)ip.in6_16[1]);
        v[1] = ntohs((unsigned long)ip.in6_16[2]) << 16 | ntohs((unsigned long)ip.in6_16[3]);

        tea(v, k, x);

        /* formato direccion virtual: qWeRty.AsDfGh.virtual6 */
        inttobase64(virtualhost, x[0], 6);
        virtualhost[6] = '.';
        inttobase64(virtualhost + 7, x[1], 6);
        strncpy(virtualhost + 13, ".v6", 64);
    }

    printf("Resultado: %s => %s\n", ircd_ntoa(&ip), virtualhost);
    exit(EXIT_SUCCESS);
}
