/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_reslib.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1992 Darren Reed
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
/** @file
 * @brief Interface from ircd resolver to its support functions.
 * @version $Id: ircd_reslib.h,v 1.4 2007-04-26 19:17:31 zolty Exp $
 */
#ifndef INCLUDED_ircdreslib_h
#define INCLUDED_ircdreslib_h

#include <netdb.h>

/*
 * Inline versions of get/put short/long.  Pointer is advanced.
 */
/** Get a 16-bit network endian value from \a cp and assign to \a s. */
#define IRC_NS_GET16(s, cp) { \
	const unsigned char *t_cp = (const unsigned char *)(cp); \
	(s) = ((uint16_t)t_cp[0] << 8) \
	    | ((uint16_t)t_cp[1]) \
	    ; \
	(cp) += NS_INT16SZ; \
}

/** Get a 32-bit network endian value from \a cp and assign to \a s. */
#define IRC_NS_GET32(l, cp) { \
	const unsigned char *t_cp = (const unsigned char *)(cp); \
	(l) = ((uint32_t)t_cp[0] << 24) \
	    | ((uint32_t)t_cp[1] << 16) \
	    | ((uint32_t)t_cp[2] << 8) \
	    | ((uint32_t)t_cp[3]) \
	    ; \
	(cp) += NS_INT32SZ; \
}

/** Put \a s at \a cp as a 16-bit network endian value. */
#define IRC_NS_PUT16(s, cp) { \
	uint16_t t_s = (uint16_t)(s); \
	unsigned char *t_cp = (unsigned char *)(cp); \
	*t_cp++ = t_s >> 8; \
	*t_cp   = t_s; \
	(cp) += NS_INT16SZ; \
}

/** Put \a s at \a cp as a 32-bit network endian value. */
#define IRC_NS_PUT32(l, cp) { \
	uint32_t t_l = (uint32_t)(l); \
	unsigned char *t_cp = (unsigned char *)(cp); \
	*t_cp++ = t_l >> 24; \
	*t_cp++ = t_l >> 16; \
	*t_cp++ = t_l >> 8; \
	*t_cp   = t_l; \
	(cp) += NS_INT32SZ; \
}

/** Maximum number of nameservers to bother with. */
#define IRCD_MAXNS 8

int irc_res_init(void);
int irc_dn_expand(const unsigned char *msg, const unsigned char *eom, const unsigned char *src, char *dst, int dstsiz);
int irc_dn_skipname(const unsigned char *ptr, const unsigned char *eom);
int irc_res_mkquery(const char *dname, int class, int type, unsigned char *buf, int buflen);
unsigned int irc_ns_get16(const unsigned char *src);
#endif /* INCLUDED_res_h */
