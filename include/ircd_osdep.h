/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_osdep.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
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
/** @file ircd_osdep.h
 * @brief Public definitions and APIs for OS-dependent operations.
 * @version $Id: ircd_osdep.h,v 1.6 2007-04-22 13:56:19 zolty Exp $
 */
#ifndef INCLUDED_ircd_osdep_h
#define INCLUDED_ircd_osdep_h

struct Client;
struct irc_sockaddr;
struct MsgQ;

/** Result of an input/output operation. */
typedef enum IOResult {
  IO_FAILURE = -1, /**< Serious I/O error (not due to blocking). */
  IO_BLOCKED = 0,  /**< I/O could not start because it would block. */
  IO_SUCCESS = 1   /**< I/O succeeded. */
} IOResult;

/*
 * NOTE: osdep.c files should never need to know the actual size of a
 * Client struct. When passed as a parameter, the pointer just needs
 * to be forwarded to the enumeration function.
 */
/** Callback function to show rusage information.
 * @param cptr Client to receive the message.
 * @param msg Text message to send to user.
 */
typedef void (*EnumFn)(struct Client* cptr, const char* msg);

extern int os_disable_options(int fd);
extern int os_get_rusage(struct Client* cptr, int uptime, EnumFn enumerator);
extern int os_get_sockerr(int fd);
extern int os_get_sockname(int fd, struct irc_sockaddr* sin_out);
extern int os_get_peername(int fd, struct irc_sockaddr* sin_out);
extern int os_socket(const struct irc_sockaddr* local, int type, const char* port_name, int family);
extern int os_accept(int fd, struct irc_sockaddr* peer);
extern IOResult os_sendto_nonb(int fd, const char* buf, unsigned int length,
                               unsigned int* length_out, unsigned int flags,
                               const struct irc_sockaddr* peer);
extern IOResult os_recv_nonb(int fd, char* buf, unsigned int length,
                        unsigned int* length_out);
extern IOResult os_send_nonb(int fd, const char* buf, unsigned int length,
                        unsigned int* length_out);
extern IOResult os_sendv_nonb(int fd, struct MsgQ* buf,
			      unsigned int* len_in, unsigned int* len_out);
extern IOResult os_recvfrom_nonb(int fd, char* buf, unsigned int len,
                                 unsigned int* length_out,
                                 struct irc_sockaddr* from_out);
extern IOResult os_connect_nonb(int fd, const struct irc_sockaddr* sin);
extern int os_set_fdlimit(unsigned int max_descriptors);
extern int os_set_listen(int fd, int backlog);
extern int os_set_nonblocking(int fd);
extern int os_set_reuseaddr(int fd);
extern int os_set_sockbufs(int fd, unsigned int ssize, unsigned int rsize);
extern int os_set_tos(int fd,int tos);
extern int os_socketpair(int sv[2]);

#endif /* INCLUDED_ircd_osdep_h */
