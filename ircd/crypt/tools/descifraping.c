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
 * $Id$
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

int main(int argc, char *argv[])
{
    aes256_context ctx;
    uint8_t k[32], v[16], x[24];
    char key[45], msg[33];
    int key_len, msg_len;
    unsigned int i;
    
    if (argc != 3)
    {
      printf("Uso: descifraping clave mensaje\n");
      return 1;
    }
    
    memset(k, 0, sizeof(k));
    memset(v, 0, sizeof(v));
    memset(x, 0, sizeof(x));
    memset(key, 'A', sizeof(key));
    memset(msg, 'A', sizeof(msg));
    key_len = strlen(argv[1]);
    msg_len = strlen(argv[2]);
    key_len = (key_len>44) ? 44 : key_len;
    msg_len = (msg_len>32) ? 32 : msg_len;
    
    strncpy(key+(44-key_len), argv[1], (key_len));
    strncpy(msg+(32-msg_len), argv[2], (msg_len));
    key[44]='\0';
    msg[32]='\0';
    
    printf("String clave (base64): %s\n", key);
    printf("String mensaje.......: %s\n", msg);
    
    base64_to_buf_r(k, (unsigned char *) key);
    base64_to_buf_r(x, (unsigned char *) msg);
    memcpy(k+24,x,8);
    memcpy(v,x+8,16);

    DUMP("CLAVE....: ", i, k, sizeof(k));
    DUMP("ENTRADA..: ", i, x, sizeof(x));
    DUMP("ENCRIPT..: ", i, v, sizeof(v));
    aes256_init(&ctx, k);
    aes256_decrypt_ecb(&ctx, v);
    aes256_done(&ctx);
    DUMP("DESCRIPT.: ", i, v, sizeof(v));
    printf("Resultado: %s\n\n", v);

    exit(EXIT_SUCCESS);
}
