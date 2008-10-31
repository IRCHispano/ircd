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
#ifdef _WIN32
# include <winsock.h>
#else
# include <netinet/in.h>
#endif


#define NUMNICKLOG 6
#define NUMNICKBASE 64          /* (2 << NUMNICKLOG) */
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
 * La siguiente tabla es utilizada por la macro toLower, 
 * esta tabla esta extraida del archivo common.c del ircd.
 *
 * --Daijo
 */
const char NTL_tolower_tab[] = {
       /* x00-x07 */ '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
       /* x08-x0f */ '\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
       /* x10-x17 */ '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
       /* x18-x1f */ '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',
       /* ' '-x27 */    ' ',    '!',    '"',    '#',    '$',    '%',    '&', '\x27',
       /* '('-'/' */    '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
       /* '0'-'7' */    '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
       /* '8'-'?' */    '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',
       /* '@'-'G' */    '@',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
       /* 'H'-'O' */    'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
       /* 'P'-'W' */    'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
       /* 'X'-'_' */    'x',    'y',    'z',    '{',    '|',    '}',    '~',    '_',
       /* '`'-'g' */    '`',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
       /* 'h'-'o' */    'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
       /* 'p'-'w' */    'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
       /* 'x'-x7f */    'x',    'y',    'z',    '{',    '|',    '}',    '~', '\x7f'
};
#define toLower(c) (NTL_tolower_tab[(c)])

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
    y += (z << 4) + a ^ z + sum ^ (z >> 5) + b;
    z += (y << 4) + c ^ y + sum ^ (y >> 5) + d;
  }

  x[0] = y;
  x[1] = z;
}


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
       nick[i] = toLower(nick[i]);
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

    return 0;

}
