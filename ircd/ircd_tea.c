/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ircd_tea.c
 *
 * Copyright (C) 2002-2007 IRC-Dev Development Team <devel@irc-dev.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: ircd_tea.c,v 1.4 2007-04-19 22:53:48 zolty Exp $
 *
 */
#include "config.h"

void ircd_tea(unsigned int v[], unsigned int k[], unsigned int x[])
{
  unsigned int y = v[0] ^ x[0];
  unsigned int z = v[1] ^ x[1];
  unsigned int a = k[0];
  unsigned int b = k[1];
  unsigned int c = 0;
  unsigned int d = 0;
  unsigned int n = 32;
  unsigned int sum = 0;
  unsigned int delta = 0x9E3779B9;

  while (n-- > 0)
  {
    sum += delta;
    y += ((z << 4) + a) ^ (z + sum) ^ ((z >> 5) + b);
    z += ((y << 4) + c) ^ (y + sum) ^ ((y >> 5) + d);
  }

  x[0] = y;
  x[1] = z;
}

void ircd_untea(unsigned int v[], unsigned int k[], unsigned int x[])
{
  unsigned int y = v[0];
  unsigned int z = v[1];
  unsigned int a = k[0];
  unsigned int b = k[1];
  unsigned int c = 0;
  unsigned int d = 0;
  unsigned int n = 32;
  unsigned int sum = 0xC6EF3720;
  unsigned int delta = 0x9E3779B9;

  /* sum = delta << 5, in general sum = delta * n */

  while (n-- > 0)
  {
    z -= ((y << 4) + c) ^ (y + sum) ^ ((y >> 5) + d);
    y -= ((z << 4) + a) ^ (z + sum) ^ ((z >> 5) + b);
    sum -= delta;
  }

  x[0] = y;
  x[1] = z;
}
