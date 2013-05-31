/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_handler.h
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
 * @brief Interfaces for sending common replies to users.
 * @version $Id: ircd_reply.h,v 1.5 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef INCLUDED_ircd_reply_h
#define INCLUDED_ircd_reply_h

struct Client;

extern int protocol_violation(struct Client* cptr, const char* pattern, ...);
extern int need_more_params(struct Client* cptr, const char* cmd);
extern int send_reply(struct Client* to, int reply, ...);

#define SND_EXPLICIT	0x40000000	/**< first arg is a pattern to use */

#endif /* INCLUDED_ircd_reply_h */
