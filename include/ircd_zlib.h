/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_zlib.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2003 Jesus Cea Avion <jcea@jcea.es>
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
 * @brief Interfaces and declarations for ZLIB.
 * @version $Id: dbuf.h,v 1.7 2007-12-11 23:38:23 zolty Exp $
 */
#ifndef INCLUDED_ircd_zlib_h
#define INCLUDED_ircd_zlib_h

struct Client;

/*
 * Prototypes
 */
#if defined(USE_ZLIB)
void inicia_microburst(void);
void completa_microburst(void);
void inicializa_microburst(void);
void elimina_cptr_microburst(struct Client *cptr);
#endif

#endif /* INCLUDED_ircd_zlib_h */
