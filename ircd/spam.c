/*
 * IRC - Internet Relay Chat, ircd/spam.c
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

#include "sys.h"

#include "h.h"
#include "spam.h"
#include "channel.h"
#include "common.h"
#include "ircd.h"
#include "match.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "opercmds.h"
#include "s_conf.h"
#include "s_bdd.h"
#include "s_bsd.h"
#include "s_err.h"
#include "s_serv.h"
#include "send.h"
#include "struct.h"
#include "support.h"

struct SpamFilter *listspam = NULL;

typedef struct {
  enum SpamActionType action;
  char *desc;
} SpamAction;

SpamAction spamactions[] = {
  { SAT_NO_ACTION,    "NO_ACTION" },
  /* ... */
  { -1, NULL }
};

void spam_add(u_int32_t id_filter, char *pattern, char *reason, int action, u_int16_t flags, time_t expire)
{
 /* ... */
}

void spam_del(u_int32_t id_filter)
{
  /* ... */
}

char *get_str_spamaction(enum SpamActionType action)
{
  SpamAction *sa;

  for (sa = spamactions; sa->desc; sa++) {
    if (sa->action == action)
      return sa->desc;
  }
  return NULL;
}

struct SpamFilter *find_spam(u_int32_t id_filter)
{
 /* ... */

  return NULL;
}

static int action_spam(struct SpamFilter *spam, aClient *sptr, char *text, int flags, aChannel *chptr, aClient *acptr)
{
  /* ... */

  return 0;
}

int check_spam(aClient *sptr, char *text, int flags, aChannel *chptr, aClient *acptr)
{
  /* ... */

  return 0;
}

/*
 * m_spam                              - Added October 2018 by Toni Garcia.
 *
 * parv[0] = sender prefix
 * parv[1] = spam id
 * parv[2] = spam event
 * parv[3] = spam action
 * parv[4] = numeric spammer
 * parv[5] = destin
 * parv[6] = id gline
 * parv[parc-1] = message
 */
int m_spam(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  if (!IsServer(cptr))
    return 0;

  if (parc < 8)
    return 0;

  sendto_highprot_butone(cptr, 10, "%s " TOK_SPAM " %s %s %s %s %s %s :%s", NumServ(sptr),
      parv[1], parv[2], parv[3], parv[4], parv[5], parv[6], parv[7]);

  return 0;
}

void spam_stats(aClient *sptr)
{
 /* ... */
}

