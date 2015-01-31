/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_defs.h
 *
 * Copyright (C) 2002-2015 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1999 Thomas Helvey <tomh@inxpress.net>
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
 * @brief Definitions used everywhere in the server.
 *
 * NOTE: Changing any of these definitions (except for the
 * target-related ones at the bottom) is equivalent to a protocol
 * revision. Every server on a given network must use the same values.
 * @version $Id: ircd_defs.h,v 1.6 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef INCLUDED_ircd_defs_h
#define INCLUDED_ircd_defs_h

/** Maximum length allowed for a nickname.
 * Because certain networks are very helpful in finding bugs, the below
 * is a default that can easily be overridden in CFLAGS.  Just add
 * -DNICKLEN=15 to CFLAGS during configure, and you can forget about
 * it.  Thanks for helping debug guys.
 * See also F:NICKLEN in ircd.conf.
 */
#ifndef NICKLEN
#define NICKLEN         30
#endif
/** Maximum length allowed of a user name, including an optional
 * leading '~' if the user name has not been authenticated by an auth (RFC 931)
 * server query.
 */
#define USERLEN         10
/** Exactly long enough to hold one (1) segment of FQDN or hostname.
 * This is due to an historical misinterpretation of RFC 1034.
 * 3.1. Name space specifications and terminology
 *
 * The domain name space is a tree structure.  Each node and leaf on the
 * tree corresponds to a resource set (which may be empty).  The domain
 * system makes no distinctions between the uses of the interior nodes and
 * leaves, and this memo uses the term "node" to refer to both.
 *
 * Each node has a label, which is zero to 63 octets in length.  Brother
 * nodes may not have the same label, although the same label can be used
 * for nodes which are not brothers.  One label is reserved, and that is
 * the null (i.e., zero length) label used for the root.
 *
 * This has proven not to be a problem in the past as connections with FQDN's
 * of greater than 63 characters are rejected by the server, and most FQDN's
 * are shorter. It is possible to have a valid FQDN longer than 63 characters.
 */
#define HOSTLEN         63
/** Maximum length for the account name, which can be set
 * with the ACCOUNT (AC) command.  This is used for keeping track of who's
 * logged into which account, for the benefit of irc services.
 */
#define ACCOUNTLEN      12
/** Maximum length for user supplied information about a client
 * connection (gcos). This information is set at client/server registration
 * time.
 */
#define REALLEN         70
/** Maximum length for a password used for connecting servers and clients.
 */
#define PASSWDLEN       20
/** Maximum length of a numeric IP (v4 or v6) address.
 * "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"
 */
#define SOCKIPLEN 45
/** Maximum length for channel topics and kill comments.
 */
#define TOPICLEN        240
/** Maximum length for away messages.
 */
#define AWAYLEN		160
/** Maximum length for quit messages.
 */
#define QUITLEN        300
/** Maximum length for gline messages.
 */
#define MAXLEN         490

/** Exactly long enough to hold one protocol message (RFC 1459)
 * including the line termination (\\r\\n).  DO NOT CHANGE THIS!!!!
 */
#define BUFSIZE         512

/** Maximum available targets for a user. */
#define MAXTARGETS      20
/** Starting free targets for a user. */
#define STARTTARGETS    10
/** Target number to start assigning new targets. */
#define RESERVEDTARGETS 12

#endif /* INCLUDED_ircd_defs_h */
