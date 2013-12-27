/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_tea.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief TEA implementation for ircu.
 * @version $Id: ircd_tea.h,v 1.5 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef INCLUDED_ircd_tea_h
#define INCLUDED_ircd_tea_h

extern void ircd_tea(unsigned int v[], unsigned int k[], unsigned int x[]);
extern void ircd_untea(unsigned int v[], unsigned int k[], unsigned int x[]);

#endif /* INCLUDED_ircd_tea_h */

