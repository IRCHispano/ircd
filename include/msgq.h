/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/msgq.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2000 Kevin L. Mitchell <klmitch@mit.edu>
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
 * @brief Outbound message queue interface and declarations.
 * @version $Id: msgq.h,v 1.8 2007-09-20 21:00:31 zolty Exp $
 */
#ifndef INCLUDED_msgq_h
#define INCLUDED_msgq_h

#ifndef INCLUDED_ircd_defs_h
#include "ircd_defs.h"	/* BUFSIZE */
#endif
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif
#ifndef INCLUDED_stdarg_h
#include <stdarg.h>
#define INCLUDED_stdarg_h
#endif

struct iovec;

struct Client;
struct StatDesc;

struct Msg;
struct MsgBuf;

/** Queue of individual messages. */
struct MsgQList {
  struct Msg *head;		/**< First Msg in queue list */
  struct Msg *tail;		/**< Last Msg in queue list */
  unsigned int sent;		/**< Bytes in *head that have already been sent */
};

/** Entire two-priority message queue for a destination. */
struct MsgQ {
  unsigned int length;		/**< Current number of bytes stored */
  unsigned int count;		/**< Current number of messages stored */
  struct MsgQList queue;	/**< Normal Msg queue */
  struct MsgQList prio;		/**< Priority Msg queue */
};

/** Returns the current number of bytes stored in \a mq. */
#define MsgQLength(mq) ((mq)->length)

/** Returns the current number of messages stored in \a mq. */
#define MsgQCount(mq) ((mq)->count)

/** Scratch the current content of the buffer.
 * Release all allocated buffers and make it empty.
 */
#define MsgQClear(mq) msgq_delete((mq), MsgQLength(mq))

/*
 * Prototypes
 */
extern void msgq_init(struct MsgQ *mq);
extern void msgq_delete(struct MsgQ *mq, unsigned int length);
extern int msgq_mapiov(const struct MsgQ *mq, struct iovec *iov, int count,
		       unsigned int *len);
extern struct MsgBuf *msgq_make(struct Client *dest, const char *format, ...);
extern struct MsgBuf *msgq_vmake(struct Client *dest, const char *format,
				 va_list args);
extern void msgq_append(struct Client *dest, struct MsgBuf *mb,
			const char *format, ...);
extern void msgq_clean(struct MsgBuf *mb);
extern void msgq_add(struct MsgQ *mq, struct MsgBuf *mb, int prio);
extern void msgq_count_memory(struct Client *cptr,
                              size_t *msg_alloc, size_t *msg_used);
extern void msgq_histogram(struct Client *cptr, const struct StatDesc *sd,
                           char *param);
extern unsigned int msgq_bufleft(struct MsgBuf *mb);

#endif /* INCLUDED_msgq_h */
