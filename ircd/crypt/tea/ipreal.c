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


char *inetoa(unsigned int ip)
{
  static char buf[16];
  unsigned char *s = (unsigned char *)&ip;
  unsigned char a, b, c, d;

  a = *s++;
  b = *s++;
  c = *s++;
  d = *s++;
  sprintf(buf, "%u.%u.%u.%u", d, c, b, a);

  return buf;
}

#define NUMNICKLOG 6

static const unsigned int convert2n[] = {
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  52,53,54,55,56,57,58,59,60,61, 0, 0, 0, 0, 0, 0,
   0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,25,62, 0,63, 0, 0,
   0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51, 0, 0, 0, 0, 0,

   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

unsigned int base64toint(const char *s)
{
  unsigned int i = convert2n[(unsigned char)*s++];
  while (*s)
  {
    i <<= NUMNICKLOG;
    i += convert2n[(unsigned char)*s++];
  }
  return i;
}

/*
 * TEA (cifrado)
 *
 * Cifra 64 bits de datos, usando clave de 64 bits (los 64 bits superiores son cero)
 * Se cifra v[0]^x[0], v[1]^x[1], para poder hacer CBC facilmente.
 *
 */
void untea(unsigned int v[],unsigned int k[], unsigned int x[])
{
   unsigned int y = v[0],z = v[1], sum = 0xC6EF3720, delta = 0x9E3779B9;
   unsigned int a = k[0], b = k[1], n = 32;
   unsigned int c = 0,d = 0;

   /* sum = delta<<5, in general sum = delta * n */

   while (n-- > 0)
   {
	  z -= ((y << 4) + c) ^ ((y + sum) ^ ((y >> 5) + d));
	  y -= ((z << 4) + a) ^ ((z + sum) ^ ((z >> 5) + b));
      sum -= delta;
   }

   x[0]=y; x[1]=z;
}


int main(int argc, char *argv[])
{
    unsigned int v[2], k[2], x[2];
    char tmpvirt[7];
    char clave[12+1];
    char tmp;

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

    printf("Resultado: %s\n", inetoa(x[1]));

    exit(EXIT_SUCCESS);
}
