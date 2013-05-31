/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ircd_events.c
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Implementation of event loop mid-layer.
 * @version $Id: ircd_events.c,v 1.13 2008-01-19 13:28:51 zolty Exp $
 */
#include "config.h"

#include "ircd_events.h"

#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_log.h"
#include "ircd_snprintf.h"
#include "s_debug.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define SIGS_PER_SOCK	10	/**< number of signals to process per socket
				   readable event */

#ifdef USE_KQUEUE
extern struct Engine engine_kqueue;
#define ENGINE_KQUEUE	&engine_kqueue,
#else
/** Address of kqueue engine (if used). */
#define ENGINE_KQUEUE
#endif /* USE_KQUEUE */

#ifdef USE_DEVPOLL
extern struct Engine engine_devpoll;
#define ENGINE_DEVPOLL	&engine_devpoll,
#else
/** Address of /dev/poll engine (if used). */
#define ENGINE_DEVPOLL
#endif /* USE_DEVPOLL */

#ifdef USE_EPOLL
extern struct Engine engine_epoll;
#define ENGINE_EPOLL &engine_epoll,
#else
/** Address of epoll engine (if used). */
#define ENGINE_EPOLL
#endif /* USE_EPOLL */

#ifdef USE_POLL
extern struct Engine engine_poll;
/** Address of fallback (poll) engine. */
#define ENGINE_FALLBACK	&engine_poll,
#else
extern struct Engine engine_select;
/** Address of fallback (select) engine. */
#define ENGINE_FALLBACK	&engine_select,
#endif /* USE_POLL */

/** list of engines to try */
static const struct Engine *evEngines[] = {
  ENGINE_KQUEUE
  ENGINE_EPOLL
  ENGINE_DEVPOLL
  ENGINE_FALLBACK
  0
};

/** Signal routines pipe data.
 * This is used if an engine does not implement signal handling itself
 * (when Engine::eng_signal is NULL).
 */
static struct sigInfo_s {
  int		fd;	/**< signal routine's fd */
  struct Socket	sock;	/**< and its struct Socket */
} sigInfo = { -1 };

/** All the thread info */
static struct evInfo_s {
  struct Generators    gens;		/**< List of all generators */
  struct Event*	       events_free;	/**< struct Event free list */
  unsigned int	       events_alloc;	/**< count of allocated struct Events */
  const struct Engine* engine;		/**< core engine being used */
#ifdef IRCD_THREADED
  struct GenHeader*    genq_head;	/**< head of generator event queue */
  struct GenHeader*    genq_tail;	/**< tail of generator event queue */
  unsigned int	       genq_count;	/**< count of generators on queue */
#endif
} evInfo = {
  { 0, 0, 0 },
  0, 0, 0
#ifdef IRCD_THREADED
  , 0, 0, 0
#endif
};

/** Initialize a struct GenHeader.
 * @param[in,out] gen GenHeader to initialize.
 * @param[in] call Callback for generated events.
 * @param[in] data User data pointer.
 * @param[in] next Pointer to next generator.
 * @param[in,out] prev_p Pointer to previous pointer for this list.
 */
static void
gen_init(struct GenHeader* gen, EventCallBack call, void* data,
	 struct GenHeader* next, struct GenHeader** prev_p)
{
  assert(0 != gen);

  gen->gh_next = next;
  gen->gh_prev_p = prev_p;
#ifdef IRCD_THREADED
  gen->gh_qnext = 0;
  gen->gh_qprev_p = 0;
  gen->gh_head = 0;
  gen->gh_tail = 0;
#endif
  gen->gh_flags = GEN_ACTIVE;
  gen->gh_ref = 0;
  gen->gh_call = call;
  gen->gh_data = data;
  gen->gh_engdata.ed_int = 0;

  if (prev_p) { /* Going to link into list? */
    if (next) /* do so */
      next->gh_prev_p = &gen->gh_next;
    *prev_p = gen;
  }
}

#ifndef IRCD_THREADED

/** Generate and execute an event.
 * @param[in] type Type of event to generate.
 * @param[in] arg Pointer to an event generator (GenHeader).
 * @param[in] data Extra data for event.
 */
void
event_generate(enum EventType type, void* arg, int data)
{
  struct Event ev;
  struct GenHeader *gen = (struct GenHeader*) arg;

  assert(0 != gen);
  assert(gen->gh_flags & GEN_ACTIVE);

  if (type == ET_DESTROY)
  {
    if (gen->gh_flags & GEN_DESTROY)
      return;
    gen->gh_flags &= ~GEN_ACTIVE;
  }
  else if (type == ET_ERROR)
    gen->gh_flags |= GEN_ERROR;

  Debug((DEBUG_LIST, "Generating event type %s for generator %p (%s)",
	 event_to_name(type), gen, gen_flags(gen->gh_flags)));

  ev.ev_next = NULL;
  ev.ev_prev_p = NULL;
  ev.ev_type = type;
  ev.ev_data = data;
  ev.ev_gen.gen_header = gen;
  gen->gh_ref++;

  gen->gh_call(&ev);
  if (type != ET_DESTROY)
    gen_ref_dec(gen);
}

#else

/** Execute an event.
 * Optimizations should inline this.
 * @param[in] event Event to execute.
 */
static void
event_execute(struct Event* event)
{
  assert(0 != event);
  assert(0 == event->ev_prev_p); /* must be off queue first */
  assert(event->ev_gen.gen_header->gh_flags & GEN_ACTIVE);

  if (event->ev_type == ET_DESTROY) /* turn off active flag *before* destroy */
    event->ev_gen.gen_header->gh_flags &= ~GEN_ACTIVE;
  if (event->ev_type == ET_ERROR) /* turn on error flag before callback */
    event->ev_gen.gen_header->gh_flags |= GEN_ERROR;

  (*event->ev_gen.gen_header->gh_call)(event); /* execute the event */

  /* The logic here is very careful; if the event was an ET_DESTROY,
   * then we must assume the generator is now invalid; fortunately, we
   * don't need to do anything to it if so.  Otherwise, we decrement
   * the reference count; if reference count goes to zero, AND we need
   * to destroy the generator, THEN we generate a DESTROY event.
   */
  if (event->ev_type != ET_DESTROY)
    gen_ref_dec(event->ev_gen.gen_header);

  event->ev_gen.gen_header = 0; /* clear event data */
  event->ev_type = ET_DESTROY;

  event->ev_next = evInfo.events_free; /* add to free list */
  evInfo.events_free = event;
}

/** Add an event to the work queue.
 * @param[in] event Event to enqueue.
 */
/* This is just a placeholder; don't expect ircd to be threaded soon */
/* There should be locks all over the place in here */
static void
event_add(struct Event* event)
{
  struct GenHeader* gen;

  assert(0 != event);

  gen = event->ev_gen.gen_header;

  /* First, place event on generator's event queue */
  event->ev_next = 0;
  if (gen->gh_head) {
    assert(0 != gen->gh_tail);

    event->ev_prev_p = &gen->gh_tail->ev_next;
    gen->gh_tail->ev_next = event;
    gen->gh_tail = event;
  } else { /* queue was empty */
    assert(0 == gen->gh_tail);

    event->ev_prev_p = &gen->gh_head;
    gen->gh_head = event;
    gen->gh_tail = event;
  }

  /* Now, if the generator isn't on the queue yet... */
  if (!gen->gh_qprev_p) {
    gen->gh_qnext = 0;
    if (evInfo.genq_head) {
      assert(0 != evInfo.genq_tail);

      gen->gh_qprev_p = &evInfo.genq_tail->gh_qnext;
      evInfo.genq_tail->gh_qnext = gen;
      evInfo.genq_tail = gen;
    } else { /* queue was empty */
      assert(0 == evInfo.genq_tail);

      gen->gh_qprev_p = &evInfo.genq_head;
      evInfo.genq_head = gen;
      evInfo.genq_tail = gen;
    }

    /* We'd also have to signal the work crew here */
  }
}

/** Generate an event and add it to the queue (or execute it).
 * @param[in] type Type of event to generate.
 * @param[in] arg Pointer to an event generator (GenHeader).
 * @param[in] data Extra data for event.
 */
void
event_generate(enum EventType type, void* arg, int data)
{
  struct Event* ptr;
  struct GenHeader* gen = (struct GenHeader*) arg;

  assert(0 != gen);

  /* don't create events (other than ET_DESTROY) for destroyed generators */
  if (type != ET_DESTROY && (gen->gh_flags & GEN_DESTROY))
    return;

  Debug((DEBUG_LIST, "Generating event type %s for generator %p (%s)",
	 event_to_name(type), gen, gen_flags(gen->gh_flags)));

  if ((ptr = evInfo.events_free))
    evInfo.events_free = ptr->ev_next; /* pop one off the freelist */
  else { /* allocate another structure */
    ptr = (struct Event*) MyMalloc(sizeof(struct Event));
    evInfo.events_alloc++; /* count of allocated events */
  }

  ptr->ev_type = type; /* Record event type */
  ptr->ev_data = data;

  ptr->ev_gen.gen_header = (struct GenHeader*) gen;
  ptr->ev_gen.gen_header->gh_ref++;

  event_add(ptr); /* add event to queue */
}

#endif /* IRCD_THREADED */

/** Place a timer in the correct spot on the queue.
 * @param[in] timer Timer to enqueue.
 */
static void
timer_enqueue(struct Timer* timer)
{
  struct GenHeader** ptr_p;

  assert(0 != timer);
  assert(0 == timer->t_header.gh_prev_p); /* not already on queue */
  assert(timer->t_header.gh_flags & GEN_ACTIVE); /* timer is active */

  /* Calculate expire time */
  switch (timer->t_type) {
  case TT_ABSOLUTE: /* no need to consider it relative */
    timer->t_expire = timer->t_value;
    break;

  case TT_RELATIVE: case TT_PERIODIC: /* relative timer */
    timer->t_expire = timer->t_value + CurrentTime;
    break;
  }

  /* Find a slot to insert timer */
  for (ptr_p = &evInfo.gens.g_timer; ;
       ptr_p = &(*ptr_p)->gh_next)
    if (!*ptr_p || timer->t_expire < ((struct Timer*)*ptr_p)->t_expire)
      break;

  /* link it in the right place */
  timer->t_header.gh_next = *ptr_p;
  timer->t_header.gh_prev_p = ptr_p;
  if (*ptr_p)
    (*ptr_p)->gh_prev_p = &timer->t_header.gh_next;
  *ptr_p = &timer->t_header;
}

/** &Signal handler for writing signal notification to pipe.
 * @param[in] sig Signal number that just happened.
 */
static void
signal_handler(int sig)
{
  unsigned char c;

  assert(sigInfo.fd >= 0);

  c = (unsigned char) sig; /* only write 1 byte to identify sig */

  write(sigInfo.fd, &c, 1);
}

/** Callback for signal "socket" (really pipe) events.
 * @param[in] event Event activity descriptor.
 */
static void
signal_callback(struct Event* event)
{
  unsigned char sigstr[SIGS_PER_SOCK];
  int sig, n_sigs, i;
  struct GenHeader* ptr;

  assert(event->ev_type == ET_READ); /* readable events only */

  n_sigs = read(event->ev_gen.gen_socket->s_fd, sigstr, sizeof(sigstr));

  for (i = 0; i < n_sigs; i++) {
    sig = (int) sigstr[i]; /* get signal */

    for (ptr = evInfo.gens.g_signal; ptr;
	 ptr = ptr->gh_next)
      if (((struct Signal*)ptr)->sig_signal == sig) /* find its descriptor... */
	break;

    if (ptr)
      event_generate(ET_SIGNAL, ptr, sig); /* generate signal event */
  }
}

/** Remove a generator from its queue.
 * @param[in] arg Pointer to a GenHeader to dequeue.
 */
void
gen_dequeue(void* arg)
{
  struct GenHeader* gen = (struct GenHeader*) arg;

  if (gen->gh_next) /* clip it out of the list */
    gen->gh_next->gh_prev_p = gen->gh_prev_p;
  if (gen->gh_prev_p)
    *gen->gh_prev_p = gen->gh_next;

  gen->gh_next = 0; /* mark that it's not in the list anymore */
  gen->gh_prev_p = 0;
}

/** Initializes the event system.
 * @param[in] max_sockets Maximum number of sockets to support.
 */
void
event_init(int max_sockets)
{
  int i, p[2];

  for (i = 0; evEngines[i]; i++) { /* look for an engine... */
    assert(0 != evEngines[i]->eng_name);
    assert(0 != evEngines[i]->eng_init);

    if ((*evEngines[i]->eng_init)(max_sockets))
      break; /* Found an engine that'll work */
  }

  assert(0 != evEngines[i]);

  evInfo.engine = evEngines[i]; /* save engine */

  if (!evInfo.engine->eng_signal) { /* engine can't do signals */
    if (pipe(p)) {
      log_write(LS_SYSTEM, L_CRIT, 0, "Failed to open signal pipe");
      exit(8);
    }

    sigInfo.fd = p[1]; /* write end of pipe */
    socket_add(&sigInfo.sock, signal_callback, 0, SS_NOTSOCK,
	       SOCK_EVENT_READABLE, p[0]); /* read end of pipe */
  }
}

/** Do the event loop. */
void
event_loop(void)
{
  assert(0 != evInfo.engine);
  assert(0 != evInfo.engine->eng_loop);

  (*evInfo.engine->eng_loop)(&evInfo.gens);
}

#if 0
/* Try to verify the timer list */
void
timer_verify(void)
{
  struct Timer* ptr;
  struct Timer** ptr_p = &evInfo.gens.g_timer;
  time_t lasttime = 0;

  for (ptr = evInfo.gens.g_timer; ptr;
       ptr = (struct Timer*) ptr->t_header.gh_next) {
    /* verify timer is supposed to be in the list */
    assert(ptr->t_header.gh_prev_p);
    /* verify timer is correctly ordered */
    assert((struct Timer**) ptr->t_header.gh_prev_p == ptr_p);
    /* verify timer is active */
    assert(ptr->t_header.gh_flags & GEN_ACTIVE);
    /* verify timer ordering is correct */
    assert(lasttime <= ptr->t_expire);

    lasttime = ptr->t_expire; /* store time for ordering check */
    ptr_p = (struct Timer**) &ptr->t_header.gh_next; /* store prev pointer */
  }
}
#endif

/** Initialize a timer structure.
 * @param[in,out] timer Timer to initialize.
 * @return The pointer \a timer.
 */
struct Timer*
timer_init(struct Timer* timer)
{
  gen_init(&timer->t_header, 0, 0, 0, 0);

  timer->t_header.gh_flags = 0; /* turn off active flag */

  return timer; /* convenience return */
}

/** Add a timer to be processed.
 * @param[in] timer Timer to add.
 * @param[in] call Callback for when the timer expires or is removed.
 * @param[in] data User data pointer for the timer.
 * @param[in] type Timer type.
 * @param[in] value Timer expiration, duration or interval (per \a type).
 */
void
timer_add(struct Timer* timer, EventCallBack call, void* data,
	  enum TimerType type, time_t value)
{
  assert(0 != timer);
  assert(0 != call);

  Debug((DEBUG_LIST, "Adding timer %p; time out %Tu (type %s)", timer, value,
	 timer_to_name(type)));

  /* initialize a timer... */
  timer->t_header.gh_flags |= GEN_ACTIVE;
  if (timer->t_header.gh_flags & GEN_MARKED)
    timer->t_header.gh_flags |= GEN_READD;

  timer->t_header.gh_ref = 0;
  timer->t_header.gh_call = call;
  timer->t_header.gh_data = data;

  timer->t_type = type;
  timer->t_value = value;
  timer->t_expire = 0;

  if (!(timer->t_header.gh_flags & GEN_MARKED))
    timer_enqueue(timer); /* and enqueue it */
}

/** Remove a timer from the processing queue.
 * @param[in] timer Timer to remove.
 */
void
timer_del(struct Timer* timer)
{
  assert(0 != timer);

  timer->t_header.gh_flags &= ~GEN_READD;

  if (timer->t_header.gh_flags & GEN_MARKED)
    return; /* timer is being used */

  Debug((DEBUG_LIST, "Deleting timer %p (type %s)", timer,
	 timer_to_name(timer->t_type)));

  gen_dequeue(timer);
  event_generate(ET_DESTROY, timer, 0);
}

/** Change the time a timer expires.
 * @param[in] timer Timer to update.
 * @param[in] type New timer type.
 * @param[in] value New timer expiration value.
 */
void
timer_chg(struct Timer* timer, enum TimerType type, time_t value)
{
  assert(0 != timer);
  assert(0 != value);
  assert(TT_PERIODIC != timer->t_type);
  assert(TT_PERIODIC != type);

  Debug((DEBUG_LIST, "Changing timer %p from type %s timeout %Tu to type %s "
	 "timeout %Tu", timer, timer_to_name(timer->t_type), timer->t_value,
	 timer_to_name(type), value));

  timer->t_type = type; /* Set the new type and value */
  timer->t_value = value;
  timer->t_expire = 0;

  /* If the timer expiration callback tries to change the timer
   * expiration, flag the timer but do not dequeue it yet.
   */
  if (timer->t_header.gh_flags & GEN_MARKED)
  {
    timer->t_header.gh_flags |= GEN_READD;
    return;
  }
  gen_dequeue(timer); /* remove the timer from the queue */
  timer_enqueue(timer); /* re-queue the timer */
}

/** Execute all expired timers. */
void
timer_run(void)
{
  struct Timer* ptr;

  /* go through queue... */
  while ((ptr = (struct Timer*)evInfo.gens.g_timer)) {
    if (CurrentTime < ptr->t_expire)
      break; /* processed all pending timers */

    gen_dequeue(ptr); /* must dequeue timer here */
    ptr->t_header.gh_flags |= (GEN_MARKED |
			       (ptr->t_type == TT_PERIODIC ? GEN_READD : 0));

    event_generate(ET_EXPIRE, ptr, 0); /* generate expire event */

    ptr->t_header.gh_flags &= ~GEN_MARKED;

    if (!(ptr->t_header.gh_flags & GEN_READD)) {
      Debug((DEBUG_LIST, "Destroying timer %p", ptr));
      event_generate(ET_DESTROY, ptr, 0);
    } else {
      Debug((DEBUG_LIST, "Re-enqueuing timer %p", ptr));
      timer_enqueue(ptr); /* re-queue timer */
      ptr->t_header.gh_flags &= ~GEN_READD;
    }
  }
}

/** Adds a signal to the event callback system.
 * @param[in] signal Signal event generator to use.
 * @param[in] call Callback function to use.
 * @param[in] data User data pointer for generator.
 * @param[in] sig Signal number to hook.
 */
void
signal_add(struct Signal* signal, EventCallBack call, void* data, int sig)
{
  struct sigaction act;

  assert(0 != signal);
  assert(0 != call);
  assert(0 != evInfo.engine);

  /* set up struct */
  gen_init(&signal->sig_header, call, data,
	   evInfo.gens.g_signal,
	   &evInfo.gens.g_signal);

  signal->sig_signal = sig;

  if (evInfo.engine->eng_signal)
    (*evInfo.engine->eng_signal)(signal); /* tell engine */
  else {
    act.sa_handler = signal_handler; /* set up signal handler */
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(sig, &act, 0);
  }
}

/** Adds a socket to the event system.
 * @param[in] sock Socket event generator to use.
 * @param[in] call Callback function to use.
 * @param[in] data User data pointer for the generator.
 * @param[in] state Current socket state.
 * @param[in] events Event interest mask for connected or connectionless sockets.
 * @param[in] fd &Socket file descriptor.
 * @return Zero on error, non-zero on success.
 */
int
socket_add(struct Socket* sock, EventCallBack call, void* data,
	   enum SocketState state, unsigned int events, int fd)
{
  assert(0 != sock);
  assert(0 != call);
  assert(fd >= 0);
  assert(0 != evInfo.engine);
  assert(0 != evInfo.engine->eng_add);

  /* set up struct */
  gen_init(&sock->s_header, call, data,
	   evInfo.gens.g_socket,
	   &evInfo.gens.g_socket);

  sock->s_state = state;
  sock->s_events = events & SOCK_EVENT_MASK;
  sock->s_fd = fd;

  return (*evInfo.engine->eng_add)(sock); /* tell engine about it */
}

/** Deletes (or marks for deletion) a socket generator.
 * @param[in] sock Event generator to clear.
 */
void
socket_del(struct Socket* sock)
{
  assert(0 != sock);
  assert(!(sock->s_header.gh_flags & GEN_DESTROY));
  assert(0 != evInfo.engine);
  assert(0 != evInfo.engine->eng_closing);

  /* tell engine socket is going away */
  (*evInfo.engine->eng_closing)(sock);

  sock->s_header.gh_flags |= GEN_DESTROY;

  if (!sock->s_header.gh_ref) { /* not in use; destroy right now */
    gen_dequeue(sock);
    event_generate(ET_DESTROY, sock, 0);
  }
}

/** Sets the socket state to something else.
 * @param[in] sock Socket generator to update.
 * @param[in] state New socket state.
 */
void
socket_state(struct Socket* sock, enum SocketState state)
{
  assert(0 != sock);
  assert(0 != evInfo.engine);
  assert(0 != evInfo.engine->eng_state);

  /* assertions for invalid socket state transitions */
  assert(sock->s_state != state); /* not changing states ?! */
  assert(sock->s_state != SS_LISTENING); /* listening socket to...?! */
  assert(sock->s_state != SS_CONNECTED); /* connected socket to...?! */
  /* connecting socket now connected */
  assert(sock->s_state != SS_CONNECTING || state == SS_CONNECTED);
  /* unconnected datagram socket now connected */
  assert(sock->s_state != SS_DATAGRAM || state == SS_CONNECTDG);
  /* connected datagram socket now unconnected */
  assert(sock->s_state != SS_CONNECTDG || state == SS_DATAGRAM);

  /* Don't continue if an error occurred or the socket got destroyed */
  if (sock->s_header.gh_flags & (GEN_DESTROY | GEN_ERROR))
    return;

  /* tell engine we're changing socket state */
  (*evInfo.engine->eng_state)(sock, state);

  sock->s_state = state; /* set new state */
}

/** Sets the events a socket's interested in.
 * @param[in] sock Socket generator to update.
 * @param[in] events New event interest mask.
 */
void
socket_events(struct Socket* sock, unsigned int events)
{
  unsigned int new_events = 0;

  assert(0 != sock);
  assert(0 != evInfo.engine);
  assert(0 != evInfo.engine->eng_events);

  /* Don't continue if an error occurred or the socket got destroyed */
  if (sock->s_header.gh_flags & (GEN_DESTROY | GEN_ERROR))
    return;

  switch (events & SOCK_ACTION_MASK) {
  case SOCK_ACTION_SET: /* set events to given set */
    new_events = events & SOCK_EVENT_MASK;
    break;

  case SOCK_ACTION_ADD: /* add some events */
    new_events = sock->s_events | (events & SOCK_EVENT_MASK);
    break;

  case SOCK_ACTION_DEL: /* remove some events */
    new_events = sock->s_events & ~(events & SOCK_EVENT_MASK);
    break;
  }

  if (sock->s_events == new_events)
    return; /* no changes have been made */

  /* tell engine about event mask change */
  (*evInfo.engine->eng_events)(sock, new_events);

  sock->s_events = new_events; /* set new events */
}

/** Returns the current engine's name for informational purposes.
 * @return Pointer to a static buffer containing the engine name.
 */
const char*
engine_name(void)
{
  assert(0 != evInfo.engine);
  assert(0 != evInfo.engine->eng_name);

  return evInfo.engine->eng_name;
}

#ifdef DEBUGMODE
/* These routines pretty-print names for states and types for debug printing */

/** Declares a struct variable containing name(s) and value(s) of \a TYPE. */
#define NS(TYPE) \
struct {	\
  char *name;	\
  TYPE value;	\
}

/** Declares an element initialize for an NS() struct. */
#define NM(name)	{ #name, name }

/** Declares end of an NS() struct array. */
#define NE		{ 0 }

/** Looks up name for a socket state.
 * @param[in] state &Socket state to look up.
 * @return Pointer to a static buffer containing the name, or "Undefined socket state".
 */
const char*
state_to_name(enum SocketState state)
{
  int i;
  NS(enum SocketState) map[] = {
    NM(SS_CONNECTING),
    NM(SS_LISTENING),
    NM(SS_CONNECTED),
    NM(SS_DATAGRAM),
    NM(SS_CONNECTDG),
    NM(SS_NOTSOCK),
    NE
  };

  for (i = 0; map[i].name; i++)
    if (map[i].value == state)
      return map[i].name;

  return "Undefined socket state";
}

/** Looks up name for a timer type.
 * @param[in] type &Timer type to look up.
 * @return Pointer to a static buffer containing the name, or "Undefined timer type".
 */
const char*
timer_to_name(enum TimerType type)
{
  int i;
  NS(enum TimerType) map[] = {
    NM(TT_ABSOLUTE),
    NM(TT_RELATIVE),
    NM(TT_PERIODIC),
    NE
  };

  for (i = 0; map[i].name; i++)
    if (map[i].value == type)
      return map[i].name;

  return "Undefined timer type";
}

/** Looks up name for an event type.
 * @param[in] type &Event type to look up.
 * @return Pointer to a static buffer containing the name, or "Undefined event type".
 */
const char*
event_to_name(enum EventType type)
{
  int i;
  NS(enum EventType) map[] = {
    NM(ET_READ),
    NM(ET_WRITE),
    NM(ET_ACCEPT),
    NM(ET_CONNECT),
    NM(ET_EOF),
    NM(ET_ERROR),
    NM(ET_SIGNAL),
    NM(ET_EXPIRE),
    NM(ET_DESTROY),
    NE
  };

  for (i = 0; map[i].name; i++)
    if (map[i].value == type)
      return map[i].name;

  return "Undefined event type";
}

/** Constructs a string describing certain generator flags.
 * @param[in] flags Bitwise combination of generator flags.
 * @return Pointer to a static buffer containing the names of flags set in \a flags.
 */
const char*
gen_flags(unsigned int flags)
{
  size_t loc = 0;
  int i;
  static char buf[256];
  NS(unsigned int) map[] = {
    NM(GEN_DESTROY),
    NM(GEN_MARKED),
    NM(GEN_ACTIVE),
    NM(GEN_READD),
    NM(GEN_ERROR),
    NE
  };

  buf[0] = '\0';

  for (i = 0; map[i].name; i++)
    if (map[i].value & flags) {
      if (loc != 0)
	buf[loc++] = ' ';
      loc += ircd_snprintf(0, buf + loc, sizeof(buf) - loc, "%s", map[i].name);
      if (loc >= sizeof(buf))
	return buf; /* overflow case */
    }

  return buf;
}

/** Constructs a string describing certain socket flags.
 * @param[in] flags Bitwise combination of socket flags.
 * @return Pointer to a static buffer containing the names of flags set in \a flags.
 */
const char*
sock_flags(unsigned int flags)
{
  size_t loc = 0;
  int i;
  static char buf[256];
  NS(unsigned int) map[] = {
    NM(SOCK_EVENT_READABLE),
    NM(SOCK_EVENT_WRITABLE),
    NM(SOCK_ACTION_SET),
    NM(SOCK_ACTION_ADD),
    NM(SOCK_ACTION_DEL),
    NE
  };

  buf[0] = '\0';

  for (i = 0; map[i].name; i++)
    if (map[i].value & flags) {
      if (loc != 0)
	buf[loc++] = ' ';
      loc += ircd_snprintf(0, buf + loc, sizeof(buf) - loc, "%s", map[i].name);
      if (loc >= sizeof(buf))
	return buf; /* overflow case */
    }

  return buf;
}

#endif /* DEBUGMODE */
