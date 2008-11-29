/*
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
    uint8_t k[32], x[24], v[16], s[8];
    unsigned char key[45], res[45];
    int key_len, msg_len;
    unsigned int i;
    
    if (argc != 3)
    {
      printf("Uso: cifraentrada clave ping\n");
      return 1;
    }
    
    
    memset(k, 0, sizeof(k));
    memset(v, 0, sizeof(v));
    memset(s, 0, sizeof(s));
    memset(x, 0, sizeof(x));
    memset(res, 0, sizeof(res));
    memset(key, 'A', sizeof(key));
    
    key_len = strlen(argv[1]);
    msg_len = strlen(argv[2]);
    key_len = (key_len>44) ? 44 : key_len;
    msg_len = (msg_len>16) ? 16 : msg_len;
    
    strncpy((char *)key+(44-key_len), argv[1], (key_len));
    strncpy((char *) v, argv[2], msg_len);
    key[44]='\0';
    res[44]='\0';
    
    genera_aleatorio(s, sizeof(s));

    printf("String clave (base64): %s\n", key);
    printf("String mensaje.......: %s\n", v);
    
    base64_to_buf_r((unsigned char *)k, key);
    memcpy(k+24,s,8);

    DUMP("CLAVE....: ", i, k, sizeof(k));
    DUMP("MENSAJE..: ", i, v, sizeof(v));
    aes256_init(&ctx, k);
    aes256_encrypt_ecb(&ctx, v);
    aes256_done(&ctx);
    memcpy(x,s,8);
    memcpy(x+8,v,16);
    DUMP("MEN ENC..: ", i, v, sizeof(v));
    DUMP("RESULTADO: ", i, x, sizeof(x));

    buf_to_base64_r(res, x, sizeof(x));
    
    printf("Resultado (base64): %s\n\n", res);

    exit(EXIT_SUCCESS);
}
