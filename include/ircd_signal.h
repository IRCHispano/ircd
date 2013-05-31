/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_signal.h
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
/** @file ircd_signal.h
 * @brief Interface to signal handler subsystem.
 * @version $Id: ircd_signal.h,v 1.6 2007-04-26 21:17:11 zolty Exp $
 */
#ifndef INCLUDED_ircd_signal_h
#define INCLUDED_ircd_signal_h

typedef void (*SigChldCallBack)(pid_t child_pid, void *datum, int status);

extern void setup_signals(void);
extern void register_child(pid_t child, SigChldCallBack call, void *datum);
extern void unregister_child(pid_t child);
extern void reap_children(void);

#endif /* INCLUDED_ircd_signal_h */

