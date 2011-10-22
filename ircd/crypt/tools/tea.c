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
 * TEA (cifrado)
 *
 * Cifra 64 bits de datos, usando clave de 64 bits (los 64 bits superiores son cero)
 * Se cifra v[0]^x[0], v[1]^x[1], para poder hacer CBC facilmente.
 *
 * $Id$
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
