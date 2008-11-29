/*
 * misc.c
 *
 */

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
# include <winsock.h>
#else
# include <netinet/in.h>
#endif

#define DUMP(s, i, buf, sz)  {printf(s);                   \
                              for (i = 0; i < (sz);i++)    \
                                  printf("%02x ", buf[i]); \
                              printf("\n");}


const unsigned char base64[] = {
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]"
};

const unsigned char inv_base64[128] =
{
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 
         52,  53,  54,  55,  56,  57,  58,  59,
         60,  61, 255, 255, 255, 255, 255, 255, 
        255,   0,   1,   2,   3,   4,   5,   6,
          7,   8,   9,  10,  11,  12,  13,  14, 
         15,  16,  17,  18,  19,  20,  21,  22,
         23,  24,  25,  62, 255,  63, 255, 255, 
        255,  26,  27,  28,  29,  30,  31,  32,
         33,  34,  35,  36,  37,  38,  39,  40, 
         41,  42,  43,  44,  45,  46,  47,  48,
         49,  50,  51, 255, 255, 255, 255, 255,
};

void buf_to_base64_r(unsigned char *out, const unsigned char *buf, size_t buf_len)
{
        size_t i, j;
        uint32_t limb;

/*        out = (unsigned char*) malloc(((buf_len * 8 + 5) / 6) + 5); */

        for (i = 0, j = 0, limb = 0; i + 2 < buf_len; i += 3, j += 4) {
                limb =
                        ((uint32_t) buf[i] << 16) |
                        ((uint32_t) buf[i + 1] << 8) |
                        ((uint32_t) buf[i + 2]);

                out[j] = base64[(limb >> 18) & 63];
                out[j + 1] = base64[(limb >> 12) & 63];
                out[j + 2] = base64[(limb >> 6) & 63];
                out[j + 3] = base64[(limb) & 63];
        }
  
        switch (buf_len - i) {
          case 0:
                break;
          case 1:
                limb = ((uint32_t) buf[i]);
                out[j++] = base64[(limb >> 2) & 63];
                out[j++] = base64[(limb << 4) & 63];
                out[j++] = '=';
                out[j++] = '=';
                break;
          case 2:
                limb = ((uint32_t) buf[i] << 8) | ((uint32_t) buf[i + 1]);
                out[j++] = base64[(limb >> 10) & 63];
                out[j++] = base64[(limb >> 4) & 63];
                out[j++] = base64[(limb << 2) & 63];
                out[j++] = '=';
                break;
          default:
                // something wonkey happened...
                break;
        }

        out[j] = '\0';
}

size_t base64_to_buf_r(unsigned char *buf, unsigned char *str)
{
        int i, j, len;
        uint32_t limb;
        size_t buf_len;

        len = strlen((char *) str);
        buf_len = (len * 6 + 7) / 8;
        /*buf = (unsigned char*) malloc(buf_len);*/
  
        for (i = 0, j = 0, limb = 0; i + 3 < len; i += 4) {
                if (str[i] == '=' || str[i + 1] == '=' || str[i + 2] == '=' || str[i + 3] == '=') {
                        if (str[i] == '=' || str[i + 1] == '=') {
                                break;
                        }
          
                        if (str[i + 2] == '=') {
                                limb =
                                        ((uint32_t) inv_base64[str[i]] << 6) |
                                        ((uint32_t) inv_base64[str[i + 1]]);
                                buf[j] = (unsigned char) (limb >> 4) & 0xff;
                                j++;
                        }
                        else {
                                limb =
                                        ((uint32_t) inv_base64[str[i]] << 12) |
                                        ((uint32_t) inv_base64[str[i + 1]] << 6) |
                                        ((uint32_t) inv_base64[str[i + 2]]);
                                buf[j] = (unsigned char) (limb >> 10) & 0xff;
                                buf[j + 1] = (unsigned char) (limb >> 2) & 0xff;
                                j += 2;
                        }
                }
                else {
                        limb =
                                ((uint32_t) inv_base64[str[i]] << 18) |
                                ((uint32_t) inv_base64[str[i + 1]] << 12) |
                                ((uint32_t) inv_base64[str[i + 2]] << 6) |
                                ((uint32_t) inv_base64[str[i + 3]]);
          
                        buf[j] = (unsigned char) (limb >> 16) & 0xff;
                        buf[j + 1] = (unsigned char) (limb >> 8) & 0xff;
                        buf[j + 2] = (unsigned char) (limb) & 0xff;
                        j += 3;
                }
        }

        buf_len = j;
  
        return buf_len;
}

unsigned int base64toint(const char *s)
{
  unsigned int i = inv_base64[(unsigned char)*s++];
  while (*s)
  {
    i <<= 6;
    i += inv_base64[(unsigned char)*s++];
  }
  return i;
}

const char *inttobase64(char *buf, unsigned int v, unsigned int count)
{
  buf[count] = '\0';
  while (count > 0)
  {
    buf[--count] = base64[(v & 63)];
    v >>= 6;
  }
  return buf;
}

/*
 * Funcion para obtener numeros aleatorios
 * 
 * -- FreeMind
 */
unsigned int ircrandom() {
  unsigned int number;
  FILE* urandom = fopen("/dev/urandom", "r");
  if (urandom) {
    fread(&number, 1, sizeof(number), urandom);
    fclose(urandom);
  }
  return number;
}

void genera_aleatorio(unsigned char *out, int count) {
    int i;
    for(i=0;i<count;i++)
      *out++=(unsigned char) ircrandom();
}

/*
 * Funcion para transformar de ip a ascii reentrante
 */
void inetoa_r(char *buf, unsigned int ip)
{
  unsigned char *s = (unsigned char *)&ip;
  unsigned char a, b, c, d;

  a = *s++;
  b = *s++;
  c = *s++;
  d = *s++;
  sprintf(buf, "%u.%u.%u.%u", d, c, b, a);
}
