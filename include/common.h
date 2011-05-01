/*
 * IRC - Internet Relay Chat, include/common.h
 * Copyright (C) 1998 Andrea Cocito
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

 /*
    All the code in common.h/common.c is taken from the NTL
    (Nemesi's Tools Library), adapted for ircu's character set 
    and thereafter released under GNU GPL, from there comes the
    NTL_ prefix of all macro and object names.
    Removed isXdigit() to leave space to other char sets in the
    bitmap, should give the same results as isxdigit() on any
    implementation and isn't used in IRC anyway.
  */

#if !defined(COMMON_H)
#define COMMON_H

/*=============================================================================
 * Macros de eventos
 */
  
#define DelEvent(x,y)          do { \
                                 assert(MyConnect(x)); \
                                 if((x)->y) \
                                   event_del((x)->y); \
                               } while (0)
#define DelReadEvent(x)        DelEvent(x, evread)
#define DelWriteEvent(x)       DelEvent(x, evwrite)
#define DelTimerEvent(x)       DelEvent(x, evtimer)
#define DelCheckPingEvent(x)   DelEvent(x, evcheckping)
#define DelRWEvent(x)          do { \
                                 DelReadEvent(x); \
                                 DelWriteEvent(x); \
                               } while (0)
#define DelClientEvent(x)      do { \
                                 DelReadEvent(x); \
                                 DelWriteEvent(x); \
                                 DelTimerEvent(x); \
                                 DelCheckPingEvent(x); \
                               } while (0)
#define DelRAuthEvent(x)       DelEvent(x, evauthread)
#define DelWAuthEvent(x)       DelEvent(x, evauthwrite)
#define DelRWAuthEvent(x)      do { \
                                 DelRAuthEvent(x); \
                                 DelWAuthEvent(x); \
                               } while (0)
#define UpdateRead(x)          do { \
                                 assert(MyConnect(x)); \
                                 assert(event_add((x)->evread, NULL)!=-1); \
                               } while (0)
#define UpdateWrite(x)         do { \
                                 assert(MyConnect(x)); \
                                 if((x)->evwrite) \
                                 { \
                                   if(DBufLength(&(x)->sendQ) || (x)->listing) \
                                     assert(event_add((x)->evwrite, NULL)!=-1); \
                                   else \
                                     event_del((x)->evwrite); \
                                 } \
                               } while (0)
#define UpdateGTimer(x,y,z,w)  do { \
                                  assert(MyConnect(x)); \
                                  assert(!IsListening(x)); \
                                  assert((x)->z); \
                                  assert((x)->w); \
                                  evutil_timerclear((x)->w); \
                                  (x)->w->tv_usec=0; \
                                  (x)->w->tv_sec=(y); \
                                  Debug((DEBUG_DEBUG, "timer on %s time %d", (x)->name, (x)->w->tv_sec)); \
                                  assert(evtimer_add((x)->z, (x)->w)!=-1); \
                               } while (0)
#define UpdateTimer(x,y)       UpdateGTimer(x,y,evtimer,tm_timer)
#define UpdateCheckPing(x,y)   UpdateGTimer(x,y,evcheckping,tm_checkping)
#define CreateGTimerEvent(x,y,z,w) \
                               do { \
                                  assert(MyConnect(x)); \
                                  if((x)->z) \
                                    event_del((x)->z); \
                                  else \
                                    (x)->z=(struct event*)MyMalloc(sizeof(struct event)); \
                                  if(!(x)->w) \
                                    (x)->w=(struct timeval*)MyMalloc(sizeof(struct timeval)); \
                                  evtimer_set((x)->z, (void *)(y), (void *)(x)); \
                                } while (0)
#define CreateTimerEvent(x,y)   CreateGTimerEvent(x,y,evtimer,tm_timer)
#define CreateCheckPingEvent(x) CreateGTimerEvent(x,event_checkping_callback,evcheckping,tm_checkping)
#define CreateEvent(x,y,z,w,v)  do { \
                                  assert(MyConnect(x) || (x) == &me); \
                                  if((x)->z) \
                                    event_del((x)->z); \
                                  else \
                                    (x)->z=(struct event*)MyMalloc(sizeof(struct event)); \
                                  event_set((x)->z, (x)->v, (w), (void *)y, (void *)x); \
                                  assert(event_add((x)->z, NULL)!=-1); \
                                } while (0)
#define CreateREvent(x,y)       CreateEvent(x,y,evread,(EV_READ|EV_PERSIST),fd)
#define CreateWEvent(x,y)       CreateEvent(x,y,evwrite,(EV_WRITE|EV_PERSIST),fd)
#define CreateRWEvent(x,y)      do { \
                                  CreateREvent(x,y); \
                                  CreateWEvent(x,y); \
                                  CreateTimerEvent(x,y); \
                                } while (0)
#define CreateClientEvent(x)    do { \
                                  CreateREvent(x,event_client_read_callback); \
                                  CreateWEvent(x,event_client_write_callback); \
                                  CreateTimerEvent(x,event_client_read_callback); \
                                  CreateCheckPingEvent(x); \
                                  UpdateCheckPing(x, CONNECTTIMEOUT); \
                                } while (0) 
#define CreateRAuthEvent(x)     CreateEvent(x,event_auth_callback,evauthread,(EV_READ|EV_PERSIST),authfd)
#define CreateWAuthEvent(x)     CreateEvent(x,event_auth_callback,evauthwrite,(EV_WRITE|EV_PERSIST),authfd)
#define CreateRWAuthEvent(x)    do { \
                                  CreateRAuthEvent(x); \
                                  CreateWAuthEvent(x); \
                                } while (0)
#endif /* COMMON_H */
