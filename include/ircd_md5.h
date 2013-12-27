/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_md5.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2002 hikari
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
 */
/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * ircuified 2002 by hikari
 */
/** @file
 * @brief MD5 implementation for ircu.
 * @version $Id: ircd_md5.h,v 1.3 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef ircd_md5_h
#define ircd_md5_h

/** Typedef for an unsigned 32-bit integer. */
typedef unsigned int uint32;

/** MD5 context structure. */
struct MD5Context {
	uint32 buf[4];        /**< Current digest state/value. */
	uint32 bits[2];       /**< Number of bits hashed so far. */
	unsigned char in[64]; /**< Residual input buffer. */
};

void MD5Init(struct MD5Context *);
void MD5Update(struct MD5Context *, unsigned const char *, unsigned);
void MD5Final(unsigned char digest[16], struct MD5Context *);
void MD5Transform(uint32 buf[4], uint32 const in[16]);

char *crypt_md5(const char *pw, const char *salt);

/** Helper typedef for the MD5 context structure. */
typedef struct MD5Context MD5_CTX;

#endif /* ircd_md5_h */
