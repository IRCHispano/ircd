/* 
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
 * descifraping - Utilidad para el calculo de descifrado de cookie
 * (C) FreeMind 2008/12/1
 * 
 * $Id: descifraping.c 256 2008-12-01 14:40:06Z dfmartinez $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
# include <winsock.h>
#else
# include <netinet/in.h>
#endif
#include <fcntl.h>
#include "misc.h"
#include "aes256.h"

void xor(uint8_t *dst, uint8_t *org, size_t tam);

int main(int argc, char *argv[])
{
    aes256_context ctx;
    uint8_t k[32], v[16], v2[16], v3[16], *x, *z;
    char key[45], *msg;
    unsigned int key_len, msg_len, x_len, j;
    unsigned int i;
    
    if (argc != 3)
    {
      printf("Uso: descifratexto clave mensaje\n");
      return 1;
    }
    
    memset(k, 0, sizeof(k));
    memset(v, 0, sizeof(v));
    memset(v2, 0, sizeof(v2));
    memset(v3, 0, sizeof(v3));

    memset(key, 'A', sizeof(key));

    key_len = strlen(argv[1]);
    msg = argv[2];
    msg_len = strlen(msg);
    key_len = (key_len>44) ? 44 : key_len;
    x_len = 6 * (msg_len/4);
    
    strncpy(key+(44-key_len), argv[1], (key_len));
    key[44]='\0';
    
    printf("String clave (base64): %s\n", key);
    printf("String mensaje.......: %s\n", msg);

    x=malloc(x_len);
    z=malloc(x_len);
    memset(x, 0, x_len);
    memset(z, 0, x_len);
    base64_to_buf_r(k, (unsigned char *) key);
    base64_to_buf_r(x, (unsigned char *) msg);
    memcpy(k+24,x,8);

    DUMP("CLAVE....: ", i, k, sizeof(k));
    DUMP("ENCRIPT..: ", i, x, x_len);
    aes256_init(&ctx, k);
    for(j=0;j<x_len;j=j+16) {
      memset(v, 0, sizeof(v));
      memcpy(v, x+j+8, (x_len-j) > 16 ? 16 : (x_len-j));

      memcpy(v3, v, sizeof(v));
      aes256_decrypt_ecb(&ctx, v);
      xor(v, v2, sizeof(v));
      memcpy(v2, v3, sizeof(v));

      memcpy(z+j, v, (x_len-j) > 16 ? 16 : (x_len-j));
    }

    aes256_done(&ctx);
    DUMP("DESCRIPT.: ", i, v, sizeof(v));
    printf("Resultado: %s\n\n", (char *)z);

    exit(EXIT_SUCCESS);
}
