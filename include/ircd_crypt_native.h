/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_crypt_native.h
 *
 * Copyright (C) 2002-2017 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2002 hikari
 * Copyright (C) 1990-1991 Armin Gruner
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
 * @brief Native crypt() function declarations.
 * @version $Id: ircd_crypt_native.h,v 1.3 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef INCLUDED_ircd_crypt_native_h
#define INCLUDED_ircd_crypt_native_h

extern const char* ircd_crypt_native(const char* key, const char* salt);
extern void ircd_register_crypt_native(void);

#endif /* INCLUDED_ircd_crypt_native_h */

