/*
 * IRC - Internet Relay Chat, include/class.h
 * Copyright (C) 1990 Darren Reed
 * Copyright (C) 1996 - 1997 Carlo Wood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
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

#if !defined(CLASS_H)
#define CLASS_H

/*=========================================================================
 * Structures
 */

struct ConfClass {
  unsigned int conClass;
  unsigned int conFreq;
  unsigned int pingFreq;
  unsigned int maxLinks;
  unsigned int maxSendq;
  unsigned int links;
  struct ConfClass *next;
};

/*=============================================================================
 * Macro's
 */

#define ConClass(x)	((x)->conClass)
#define ConFreq(x)	((x)->conFreq)
#define PingFreq(x)	((x)->pingFreq)
#define MaxLinks(x)	((x)->maxLinks)
#define MaxSendq(x)	((x)->maxSendq)
#define Links(x)	((x)->links)

#define ConfLinks(x)	((x)->confClass->links)
#define ConfMaxLinks(x) ((x)->confClass->maxLinks)
#define ConfClass(x)	((x)->confClass->conClass)
#define ConfConFreq(x)	((x)->confClass->conFreq)
#define ConfPingFreq(x) ((x)->confClass->pingFreq)
#define ConfSendq(x)	((x)->confClass->maxSendq)

#define FirstClass()	classes
#define NextClass(x)	((x)->next)

#define MarkDelete(x)	do { MaxLinks(x) = (unsigned int)-1; } while(0)
#define IsMarkedDelete(x) (MaxLinks(x) == (unsigned int)-1)

/*=============================================================================
 * Proto types
 */

extern aConfClass *find_class(unsigned int cclass);
extern aConfClass *make_class(void);
extern void free_class(aConfClass * tmp);
extern unsigned int get_con_freq(aConfClass * clptr);
extern unsigned int get_client_ping(aClient *acptr);
extern unsigned int get_conf_class(aConfItem *aconf);
extern unsigned int get_client_class(aClient *acptr);
extern void add_class(unsigned int conclass, unsigned int ping,
    unsigned int confreq, unsigned int maxli, size_t sendq);
extern void check_class(void);
extern void initclass(void);
extern void report_classes(aClient *sptr);
extern size_t get_sendq(aClient *cptr);

extern aConfClass *classes;

#endif /* CLASS_H */
