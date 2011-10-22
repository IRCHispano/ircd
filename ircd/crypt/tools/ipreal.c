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
    char tmpvirt[7];
    char clave[12+1];
    char tmp;
    char resultado[16];
    

    if(argc!=3) {
        printf("Uso: %s password ip_virtual\n", argv[0]);
        printf("NOTA: En el campo de ip virtual lo que haya despues de caracter 13 sera ignorado\n\n");
        return 1;
    }

    if(argv[2][6]!='.') {
      printf("Formato de ip virtual incorrecto\n");
      return 1;
    }

    strncpy(clave, argv[1], 12);
    clave[12] = '\0';
    strncpy(tmpvirt,argv[2],6);
    tmpvirt[6] = '\0';

    v[0]=base64toint(tmpvirt);
    strncpy(tmpvirt,argv[2]+7,6);
    tmpvirt[6] = '\0';
    v[1]=base64toint(tmpvirt);

    /* resultado */
    x[0] = x[1] = 0;

    /* valor */
    tmp = clave[6];
    clave[6] = '\0';
    k[0] = base64toint(clave);
    clave[6] = tmp;
    k[1] = base64toint(clave + 6);

    untea(v, k, x);
    inetoa_r(resultado, x[1]);
    printf("Resultado: %s\n", resultado);

    exit(EXIT_SUCCESS);
}
