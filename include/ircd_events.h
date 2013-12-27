/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_events.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2001 Kevin L. Mitchell <klmitch@mit.edu>
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
 * @brief Interface and public definitions for event loop.
 * @version $Id: ircd_events.h,v 1.8 2008-01-19 13:28:51 zolty Exp $
 */
#ifndef INCLUDED_ircd_events_h
#define INCLUDED_ircd_events_h

#ifndef INCLUDED_config_h
#include "config.h"
#endif
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>	/* time_t */
#define INCLUDED_sys_types_h
#endif
#if defined(USE_SSL)
#ifndef INCLUDED_ssl_h
#include "ircd_ssl.h"
#endif
#endif

struct Event;

/** Generic callback for event activity. */
typedef void (*EventCallBack)(struct Event*);

/** State of a Socket structure. */
enum SocketState {
  SS_CONNECTING,	/**< Connection in progress on socket */
  SS_LISTENING,		/**< Socket is a listening socket */
  SS_CONNECTED,		/**< Socket is a connected socket */
  SS_DATAGRAM,		/**< Socket is a datagram socket */
  SS_CONNECTDG,		/**< Socket is a connected datagram socket */
  SS_NOTSOCK		/**< Socket isn't a socket at all */
};

/** Type of a Timer (how its expiration is measured). */
enum TimerType {
  TT_ABSOLUTE,		/**< timer that runs at a specific time */
  TT_RELATIVE,		/**< timer that runs so many seconds in the future */
  TT_PERIODIC		/**< timer that runs periodically */
};

/** Type of event that generated a callback. */
enum EventType {
  ET_READ,		/**< Readable event detected */
  ET_WRITE,		/**< Writable event detected */
  ET_ACCEPT,		/**< Connection can be accepted */
  ET_CONNECT,		/**< Connection completed */
  ET_EOF,		/**< End-of-file on connection */
  ET_ERROR,		/**< Error condition detected */
  ET_SIGNAL,		/**< A signal was received */
  ET_EXPIRE,		/**< A timer expired */
  ET_DESTROY		/**< The generator is being destroyed */
};

/** Common header for event generators. */
struct GenHeader {
  struct GenHeader*  gh_next;	/**< linked list of generators */
  struct GenHeader** gh_prev_p; /**< previous pointer to this generator */
#ifdef IRCD_THREADED
  struct GenHeader*  gh_qnext;	/**< linked list of generators in queue */
  struct GenHeader** gh_qprev_p; /**< previous pointer to this generator */
  struct Event*	     gh_head;	/**< head of event queue */
  struct Event*	     gh_tail;	/**< tail of event queue */
#endif
  unsigned int	     gh_flags;	/**< generator flags */
  unsigned int	     gh_ref;	/**< reference count */
  EventCallBack	     gh_call;	/**< generator callback function */
  void*		     gh_data;	/**< extra data */
  union {
    void*	     ed_ptr;	/**< engine data as pointer */
    int		     ed_int;	/**< engine data as integer */
  }		     gh_engdata;/**< engine data */
};

#define GEN_DESTROY	0x0001	/**< generator is to be destroyed */
#define GEN_MARKED	0x0002	/**< generator is marked for destruction */
#define GEN_ACTIVE	0x0004	/**< generator is active */
#define GEN_READD	0x0008	/**< generator (timer) must be re-added */
#define GEN_ERROR	0x0010	/**< an error occurred on the generator */

/** Socket event generator.
 * Note: The socket state overrides the socket event mask; that is, if
 * it's an SS_CONNECTING socket, the engine selects its own definition
 * of what that looks like and ignores s_events.  s_events is meaningful
 * only for SS_CONNECTED, SS_DATAGRAM, and SS_CONNECTDG, but may be set
 * prior to the state transition, if desired.
 */
struct Socket {
  struct GenHeader s_header;	/**< generator information */
  enum SocketState s_state;	/**< state socket's in */
  unsigned int	   s_events;	/**< events socket is interested in */
  int		   s_fd;	/**< file descriptor for socket */
#if defined(USE_SSL)
  SSL*             s_ssl;       /**< if not NULL, use SSL routines on socket */
#endif
};

#define SOCK_EVENT_READABLE	0x0001	/**< interested in readable */
#define SOCK_EVENT_WRITABLE	0x0002	/**< interested in writable */

/** Bitmask of possible event interests for a socket. */
#define SOCK_EVENT_MASK		(SOCK_EVENT_READABLE | SOCK_EVENT_WRITABLE)

#define SOCK_ACTION_SET		0x0000	/**< set interest set as follows */
#define SOCK_ACTION_ADD		0x1000	/**< add to interest set */
#define SOCK_ACTION_DEL		0x2000	/**< remove from interest set */

#define SOCK_ACTION_MASK	0x3000	/**< mask out the actions */

/** Retrieve state of the Socket \a sock. */
#define s_state(sock)	((sock)->s_state)
/** Retrieve interest mask of the Socket \a sock. */
#define s_events(sock)	((sock)->s_events)
/** Retrieve file descriptor of the Socket \a sock. */
#define s_fd(sock)	((sock)->s_fd)
/** Retrieve user data pointer of the Socket \a sock. */
#define s_data(sock)	((sock)->s_header.gh_data)
/** Retrieve engine data integer of the Socket \a sock. */
#define s_ed_int(sock)	((sock)->s_header.gh_engdata.ed_int)
/** Retrieve engine data pointer of the Socket \a sock. */
#define s_ed_ptr(sock)	((sock)->s_header.gh_engdata.ed_ptr)
/** Retrieve whether the Socket \a sock is active. */
#define s_active(sock)	((sock)->s_header.gh_flags & GEN_ACTIVE)

/** Signal event generator. */
struct Signal {
  struct GenHeader sig_header;	/**< generator information */
  int		   sig_signal;	/**< signal number */
};

/** Retrieve signal number of the Signal \a sig. */
#define sig_signal(sig)	((sig)->sig_signal)
/** Retrieve user data pointer of the Signal \a sig. */
#define sig_data(sig)	((sig)->sig_header.gh_data)
/** Retrieve engine data integer of the Signal \a sig. */
#define sig_ed_int(sig)	((sig)->sig_header.gh_engdata.ed_int)
/** Retrieve engine data pointer of the Signal \a sig. */
#define sig_ed_ptr(sig)	((sig)->sig_header.gh_engdata.ed_ptr)
/** Retrieve whether the Signal \a sig is active. */
#define sig_active(sig)	((sig)->sig_header.gh_flags & GEN_ACTIVE)

/** Timer event generator. */
struct Timer {
  struct GenHeader t_header;	/**< generator information */
  enum TimerType   t_type;	/**< what type of timer this is */
  time_t	   t_value;	/**< value timer was added with */
  time_t	   t_expire;	/**< time at which timer expires */
};

/** Retrieve type of the Timer \a tim. */
#define t_type(tim)	((tim)->t_type)
/** Retrieve interval of the Timer \a tim. */
#define t_value(tim)	((tim)->t_value)
/** Retrieve expiration time of the Timer \a tim. */
#define t_expire(tim)	((tim)->t_expire)
/** Retrieve user data pointer of the Timer \a tim. */
#define t_data(tim)	((tim)->t_header.gh_data)
/** Retrieve engine data integer of the Timer \a tim. */
#define t_ed_int(tim)	((tim)->t_header.gh_engdata.ed_int)
/** Retrieve engine data pointer of the Timer \a tim. */
#define t_ed_ptr(tim)	((tim)->t_header.gh_engdata.ed_ptr)
/** Retrieve whether the Timer \a tim is active. */
#define t_active(tim)	((tim)->t_header.gh_flags & GEN_ACTIVE)
/** Retrieve whether the Timer \a tim is enqueued. */
#define t_onqueue(tim)	((tim)->t_header.gh_prev_p)

/** Event activity descriptor. */
struct Event {
  struct Event*	 ev_next;	/**< linked list of events on queue */
  struct Event** ev_prev_p;     /**< previous pointer to this event */
  enum EventType ev_type;	/**< Event type */
  int		 ev_data;	/**< extra data, like errno value */
  union {
    struct GenHeader* gen_header;	/**< Generator header */
    struct Socket*    gen_socket;	/**< Socket generating event */
    struct Signal*    gen_signal;	/**< Signal generating event */
    struct Timer*     gen_timer;	/**< Timer generating event */
  }		 ev_gen;	/**< object generating event */
};

/** Retrieve the type of the Event \a ev. */
#define ev_type(ev)	((ev)->ev_type)
/** Retrieve the extra data of the Event \a ev. */
#define ev_data(ev)	((ev)->ev_data)
/** Retrieve the Socket that generated the Event \a ev. */
#define ev_socket(ev)	((ev)->ev_gen.gen_socket)
/** Retrieve the Signal that generated the Event \a ev. */
#define ev_signal(ev)	((ev)->ev_gen.gen_signal)
/** Retrieve the Timer that generated the Event \a ev. */
#define ev_timer(ev)	((ev)->ev_gen.gen_timer)

/** List of all event generators. */
struct Generators {
  struct GenHeader* g_socket;	/**< list of socket generators */
  struct GenHeader* g_signal;	/**< list of signal generators */
  struct GenHeader* g_timer;	/**< list of timer generators */
};

/** Returns 1 if successfully initialized, 0 if not.
 * @param[in] max_sockets Number of sockets to support.
 */
typedef int (*EngineInit)(int max_sockets);

/** Tell engine about new signal.
 * @param[in] sig Signal event generator to add.
 */
typedef void (*EngineSignal)(struct Signal* sig);

/** Tell engine about new socket.
 * @param[in] sock Socket event generator to add.
 */
typedef int (*EngineAdd)(struct Socket* sock);

/** Tell engine about socket's new_state.
 * @param[in] sock Socket whose state is changing.
 * @param[in] new_state New state for socket.
 */
typedef void (*EngineState)(struct Socket* sock, enum SocketState new_state);

/** Tell engine about socket's new event interests.
 * @param[in] sock Socket whose interest mask is changing.
 * @param[in] new_events New event mask to set (not SOCK_ACTION_ADD or SOCK_ACTION_DEL).
 */
typedef void (*EngineEvents)(struct Socket* sock, unsigned int new_events);

/** Tell engine a socket is going away.
 * @param[in] sock Socket being destroyed.
 */
typedef void (*EngineDelete)(struct Socket* sock);

/** The actual event loop.
 * @param[in] gens List of event generators.
 */
typedef void (*EngineLoop)(struct Generators* gens);

/** Structure for an event engine to describe itself. */
struct Engine {
  const char*	eng_name;	/**< a name for the engine */
  EngineInit	eng_init;	/**< initialize engine */
  EngineSignal	eng_signal;	/**< express interest in a signal (may be NULL) */
  EngineAdd	eng_add;	/**< express interest in a socket */
  EngineState	eng_state;	/**< mention a change in state to engine */
  EngineEvents	eng_events;	/**< express interest in socket events */
  EngineDelete	eng_closing;	/**< socket is being closed */
  EngineLoop	eng_loop;	/**< actual event loop */
};

/** Increment the reference count of \a gen. */
#define gen_ref_inc(gen)	(((struct GenHeader*) (gen))->gh_ref++)
/** Decrement the reference count of \a gen. */
#define gen_ref_dec(gen)						      \
do {									      \
  struct GenHeader* _gen = (struct GenHeader*) (gen);			      \
  if (!--_gen->gh_ref && (_gen->gh_flags & GEN_DESTROY)) {		      \
    gen_dequeue(_gen);							      \
    event_generate(ET_DESTROY, _gen, 0);				      \
  }									      \
} while (0)
/** Clear the error flag for \a gen. */
#define gen_clear_error(gen)						      \
	(((struct GenHeader*) (gen))->gh_flags &= ~GEN_ERROR)

void gen_dequeue(void* arg);

void event_init(int max_sockets);
void event_loop(void);
void event_generate(enum EventType type, void* arg, int data);

struct Timer* timer_init(struct Timer* timer);
void timer_add(struct Timer* timer, EventCallBack call, void* data,
	       enum TimerType type, time_t value);
void timer_del(struct Timer* timer);
void timer_chg(struct Timer* timer, enum TimerType type, time_t value);
void timer_run(void);
/** Retrieve the next timer's expiration time from Generators \a gen. */
#define timer_next(gen)	((gen)->g_timer ? ((struct Timer*)(gen)->g_timer)->t_expire : 0)

void signal_add(struct Signal* signal, EventCallBack call, void* data,
		int sig);

int socket_add(struct Socket* sock, EventCallBack call, void* data,
	       enum SocketState state, unsigned int events, int fd);
void socket_del(struct Socket* sock);
void socket_state(struct Socket* sock, enum SocketState state);
void socket_events(struct Socket* sock, unsigned int events);

const char* engine_name(void);

#ifdef DEBUGMODE
/* These routines pretty-print names for states and types for debug printing */

const char* state_to_name(enum SocketState state);
const char* timer_to_name(enum TimerType type);
const char* event_to_name(enum EventType type);
const char* gen_flags(unsigned int flags);
const char* sock_flags(unsigned int flags);

#endif /* DEBUGMODE */

#endif /* INCLUDED_ircd_events_h */
