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
 */


#include <stdio.h>
#include <string.h>
#include <netdb.h>
#ifdef _WIN32
# include <winsock.h>
#else
# include <netinet/in.h>
#endif


#define NUMNICKLOG 6
#define NUMNICKMASK 63          /* (NUMNICKBASE-1) */

static const char convert2y[] = {
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
  'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
  'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
  'w','x','y','z','0','1','2','3','4','5','6','7','8','9','[',']'
};

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

const char *inttobase64(char *buf, unsigned int v, unsigned int count)
{
  buf[count] = '\0';
  while (count > 0)
  {
    buf[--count] = convert2y[(v & NUMNICKMASK)];
    v >>= NUMNICKLOG;
  }
  return buf;
}

/*
 * TEA (cifrado)
 *
 * Cifra 64 bits de datos, usando clave de 64 bits (los 64 bits superiores son cero)
 * Se cifra v[0]^x[0], v[1]^x[1], para poder hacer CBC facilmente.
 *
 */
void tea(unsigned int v[], unsigned int k[], unsigned int x[])
{
  unsigned int y = v[0] ^ x[0], z = v[1] ^ x[1], sum = 0, delta = 0x9E3779B9;
  unsigned int a = k[0], b = k[1], n = 32;
  unsigned int c = 0, d = 0;

  while (n-- > 0)
  {
    sum += delta;
    y += ((z << 4) + a) ^ ((z + sum) ^ ((z >> 5) + b));
    z += ((y << 4) + c) ^ ((y + sum) ^ ((y >> 5) + d));
  }

  x[0] = y;
  x[1] = z;
}


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
