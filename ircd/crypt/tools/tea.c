/*
 * tea.c
 *
 *  Created on: 25-nov-2008
 *      Author: Daniel
 */

/*
 * TEA (cifrado)
 *
 * Cifra 64 bits de datos, usando clave de 64 bits (los 64 bits superiores son cero)
 * Se cifra v[0]^x[0], v[1]^x[1], para poder hacer CBC facilmente.
 *
 */
void tea(unsigned int *v, unsigned int *k, unsigned int *x)
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

/*
 * TEA (cifrado)
 *
 * Cifra 64 bits de datos, usando clave de 64 bits (los 64 bits superiores son cero)
 * Se cifra v[0]^x[0], v[1]^x[1], para poder hacer CBC facilmente.
 *
 */
void untea(unsigned int *v, unsigned int *k, unsigned int *x)
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
