/*
 * IRC - Internet Relay Chat, include/spam.h
 * Copyright (C) 2018-2020 Toni Garcia - zoltan <toni@tonigarcia.es>
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

#if !defined(SPAM_H)
#define SPAM_H

#include "pcre.h"

enum SpamActionType {
  SAT_NO_ACTION = 0
  /* ... */
};

#define SPAM_PCRE    0x01
 /* ... */

struct SpamFilter {
  u_int32_t id_filter;        /* Id de filtro para su identificacion */
  /* ... */
};

extern void spam_add(u_int32_t id_filter, char *pattern, char *reason, int action, u_int16_t pcre, time_t expire);
extern void spam_del(u_int32_t id_filter);
extern char *get_str_spamaction(enum SpamActionType action);
extern struct SpamFilter *find_spam(u_int32_t);
extern int check_spam(aClient *sptr, char *mesage, int flags, aChannel *chptr, aClient *acptr);
extern int m_spam(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern void spam_stats(aClient *cptr);

#endif /* SPAM_H */
