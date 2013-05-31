/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/querycmds.c
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1990 Jarkko Oikarinen
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
 * @brief Implementation of client counting functions.
 * @version $Id$
 */
#include "config.h"

#include "querycmds.h"

#include <string.h>

/** Counters of clients, servers etc. */
struct UserStatistics UserStats;

/** Initialize global #UserStats variable. */
void init_counters(void)
{
  memset(&UserStats, 0, sizeof(UserStats));
  UserStats.servers = 1;
}
