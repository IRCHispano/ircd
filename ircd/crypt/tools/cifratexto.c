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
 * cifraping - Utilidad para el calculo de cifrado de cookie
 * (C) FreeMind 2008/12/1
 * 
 * $Id: cifraping.c 256 2008-12-01 14:40:06Z dfmartinez $
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
    uint8_t k[32], v[16], v2[16], s[8];
    unsigned char key[33];
    unsigned int key_len, msg_len;
    unsigned int i;
    unsigned int j,x_len;
    char *msg,*res;
    uint8_t *x;
    
    if (argc != 3)
    {
      printf("Uso: cifratexto clave texto\n");
      return 1;
    }
    
    
    memset(k, 0, sizeof(k));
    memset(v, 0, sizeof(v));
    memset(v2, 0, sizeof(v2));

    memset(s, 0, sizeof(s));

    memset(key, 'A', sizeof(key)-1);
    
    key_len = strlen(argv[1]);
    msg = argv[2];
    msg_len = strlen(msg);
    key_len = (key_len>32) ? 32 : key_len;
    x_len = 24 + msg_len - msg_len % 16;
    x = (uint8_t *) malloc(x_len);
    res = (char *) malloc(x_len*sizeof(char)*2);
    
    memset(x, 0, x_len);
    memset(res, 0, x_len*sizeof(char)*2);
    
    strncpy((char *)key+(32-key_len), argv[1], (key_len));
    key[32]='\0';
    
    genera_aleatorio(s, sizeof(s));

    printf("String clave (base64): %s\n", key);
    printf("String mensaje.......: %s\n", msg);
    
    base64_to_buf_r((unsigned char *)k, key);
    memcpy(k+24,s,8);

    DUMP("CLAVE....: ", i, k, sizeof(k));
    DUMP("MENSAJE..: ", i, msg, msg_len);

    memcpy(x,s,8);
    
    aes256_init(&ctx, k);
    for(j=0;j<msg_len;j=j+16) {
      memset(v, 0, sizeof(v));
      memcpy(v, msg+(j*sizeof(char)), (msg_len-j) > 16 ? 16 : (msg_len-j));
      xor(v, v2, sizeof(v));
      aes256_encrypt_ecb(&ctx, v);
      memcpy(v2, v, sizeof(v));
      memcpy(x+j+8, v, 16);
    }
    aes256_done(&ctx);
    
    buf_to_base64_r((unsigned char *)res, x, x_len);    

    DUMP("RESULTADO: ", i, x, x_len);
    
    printf("Resultado (base64): %s\n\n", res);

    free(x);
    free(res);

    exit(EXIT_SUCCESS);
}
