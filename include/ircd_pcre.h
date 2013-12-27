/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/pcre_match.h
 *
 * Copyright (C) 2009-2014 IRC-Dev Development Team <devel@irc-dev.net>
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
/** @file pcre_match.h
 * @brief Interface for PCRE matching strings to IRC masks.
 * @version $Id: match.h,v 1.6 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef INCLUDED_pcre_h
#define INCLUDED_pcre_h

#ifndef INCLUDED_pcre_h
#include "pcre.h"
#endif

#define OVECCOUNT 3

extern int match_pcre(pcre *re, char *subject);
extern int match_pcre_str(char *regexp, char *subject);
extern int match_pcre_ci(pcre *re, char *subject);

#endif /* INCLUDED_pcre_match_h */

