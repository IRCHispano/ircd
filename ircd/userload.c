/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/userload.c
 *
 * Copyright (C) 2002-2015 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1997 Carlo Wood <carlo@runaway.xs4all.nl>
 * Copyright (C) 1993 Michael L. VanLoon (mlv) <michaelv@iastate.edu>
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
 * @brief Userload tracking and statistics.
 * @version $Id$
 */
#include "config.h"

#include "userload.h"
#include "client.h"
#include "ircd.h"
#include "msg.h"
#include "numnicks.h"
#include "querycmds.h"
#include "s_misc.h"
#include "s_stats.h"
#include "send.h"
#include "struct.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

struct current_load_st current_load;    /**< The current load */

static struct current_load_st cspm_sum; /**< Number of connections times number
                                           of seconds per minute. */
static struct current_load_st csph_sum; /**< Number of connections times number
                                           of seconds per hour. */
static struct current_load_st cspm[60]; /**< Last 60 minutes */
static struct current_load_st csph[72]; /**< Last 72 hours */

static int m_index; /**< Next entry to use in #cspm. */
static int h_index; /**< Next entry to use in #csph. */

/** Update load average to reflect a change in the local client count.
 */
void update_load(void)
{
  static struct tm tm_now;      /* Current time. */
  static time_t last_sec;       /* Seconds of last time that
                                   update_load() called. */
  static time_t last_min;
  static time_t last;           /* Last time that update_load() was called. */
  static struct current_load_st last_load;      /* The load last time that
                                                   update_load() was called. */
  static int initialized;       /* Boolean, set when initialized. */
  int diff_time;        /* Temp. variable used to hold time intervals
                                   in seconds, minutes or hours. */

  /* Update `current_load' */
  current_load.client_count = UserStats.local_clients;
  current_load.conn_count = UserStats.local_clients + UserStats.local_servers;

  /* Nothing needed when still in the same second */
  if (!(diff_time = CurrentTime - last))
  {
    last_load = current_load;   /* Update last_load to be the load last
                                   time that update_load() was called. */
    return;
  }

  /* If we get here we entered a new second */

  /*
   * Make sure we keep the accurate time in 'tm_now'
   */
  if ((tm_now.tm_sec += diff_time) > 59)
  {
    /* This is done once every minute */
    diff_time = tm_now.tm_sec / 60;
    tm_now.tm_sec -= 60 * diff_time;
    if ((tm_now.tm_min += diff_time) > 59)
    {
      /* This is done once every hour */
      diff_time = tm_now.tm_min / 60;
      tm_now.tm_min -= 60 * diff_time;
      if ((tm_now.tm_hour += diff_time) > 23)
      {
        tm_now = *localtime(&CurrentTime);      /* Only called once a day */
        if (!initialized)
        {
          initialized = 1;
          last_sec = 60;
          last_min = tm_now.tm_min;
        }
      }
    }

    /* If we get here we entered a new minute */

    /* Finish the calculation of cspm of the last minute first: */
    diff_time = 60 - last_sec;
    cspm_sum.conn_count += last_load.conn_count * diff_time;
    cspm_sum.client_count += last_load.client_count * diff_time;
    cspm_sum.local_count += last_load.local_count * diff_time;

    /* Add the completed minute to the Connections*Seconds/Hour sum */
    csph_sum.conn_count += cspm_sum.conn_count - cspm[m_index].conn_count;
    csph_sum.client_count += cspm_sum.client_count - cspm[m_index].client_count;
    csph_sum.local_count += cspm_sum.local_count - cspm[m_index].local_count;

    /* Store the completed minute in an array */
    cspm[m_index] = cspm_sum;

    /* How long did last_cspm last ? */
    diff_time = tm_now.tm_min - last_min;
    last_min = tm_now.tm_min;

    if (diff_time < 0)
      diff_time += 60;          /* update_load() must be called at
                                   _least_ once an hour */

    if (diff_time > 1)          /* Did more then one minute pass ? */
    {
      /* Calculate the constant load during those extra minutes */
      cspm_sum.conn_count = last_load.conn_count * 60;
      cspm_sum.client_count = last_load.client_count * 60;
      cspm_sum.local_count = last_load.local_count * 60;
    }

    for (;;)
    {
      /* Increase minute index */
      if (++m_index == 60)
      {
        m_index = 0;
        /* Keep a list of the last 72 hours */
        csph[h_index] = csph_sum;
        if (++h_index == 72)
          h_index = 0;
      }

      if (--diff_time <= 0)     /* '<' to prevent endless loop if update_load()
                                   was not called once an hour :/ */
        break;

      /* Add extra minutes to the Connections*Seconds/Hour sum */
      csph_sum.conn_count += cspm_sum.conn_count - cspm[m_index].conn_count;
      csph_sum.client_count +=
          cspm_sum.client_count - cspm[m_index].client_count;
      csph_sum.local_count += cspm_sum.local_count - cspm[m_index].local_count;

      /* Store extra minutes in the array */
      cspm[m_index] = cspm_sum;
    }

    /* Now start the calculation of the new minute: */
    last_sec = tm_now.tm_sec;
    cspm_sum.conn_count = last_load.conn_count * last_sec;
    cspm_sum.client_count = last_load.client_count * last_sec;
    cspm_sum.local_count = last_load.local_count * last_sec;
  }
  else
  {
    /* A new second, but the same minute as last time */
    /* How long did last_load last ? */
    diff_time = tm_now.tm_sec - last_sec;
    last_sec = tm_now.tm_sec;
    if (diff_time == 1)         /* Just one second ? */
    {
      cspm_sum.conn_count += last_load.conn_count;
      cspm_sum.client_count += last_load.client_count;
      cspm_sum.local_count += last_load.local_count;
    }
    else
    {
      /* More then one second */
      /* At most 3 integer multiplication per second */
      cspm_sum.conn_count += last_load.conn_count * diff_time;
      cspm_sum.client_count += last_load.client_count * diff_time;
      cspm_sum.local_count += last_load.local_count * diff_time;
    }
  }
  last_load = current_load;     /* Update last_load to be the load last
                                   time that update_load() was called. */
  last = CurrentTime;
}

/** Statistics callback to display userload.
 * @param[in] sptr Client requesting statistics.
 * @param[in] sd Stats descriptor for request (ignored).
 * @param[in] param Extra parameter from user (ignored).
 */
void
calc_load(struct Client *sptr, const struct StatDesc *sd, char *param)
{
  /* *INDENT-OFF* */
  static const char *header =
  /*   ----.-  ----.-  ----  ----  ----   ------------ */
      "Minute  Hour    Day   Yest. YYest. Userload for:";
  /* *INDENT-ON* */
  static const char *what[3] = {
    "local clients",
    "total clients",
    "total connections"
  };
  int i, j, times[5][3];        /* [min,hour,day,Yest,YYest]
                                   [local,client,conn] */
  int last_m_index = m_index, last_h_index = h_index;

  update_load();                /* We want stats accurate as of *now* */

  if (--last_m_index < 0)
    last_m_index = 59;
  times[0][0] = (cspm[last_m_index].local_count + 3) / 6;
  times[0][1] = (cspm[last_m_index].client_count + 3) / 6;
  times[0][2] = (cspm[last_m_index].conn_count + 3) / 6;

  times[1][0] = (csph_sum.local_count + 180) / 360;
  times[1][1] = (csph_sum.client_count + 180) / 360;
  times[1][2] = (csph_sum.conn_count + 180) / 360;

  for (i = 2; i < 5; ++i)
  {
    times[i][0] = 43200;
    times[i][1] = 43200;
    times[i][2] = 43200;
    for (j = 0; j < 24; ++j)
    {
      if (--last_h_index < 0)
        last_h_index = 71;
      times[i][0] += csph[last_h_index].local_count;
      times[i][1] += csph[last_h_index].client_count;
      times[i][2] += csph[last_h_index].conn_count;
    }
    times[i][0] /= 86400;
    times[i][1] /= 86400;
    times[i][2] /= 86400;
  }

  sendcmdto_one(&me, CMD_NOTICE, sptr, "%C :%s", sptr, header);
  for (i = 0; i < 3; ++i)
    sendcmdto_one(&me, CMD_NOTICE, sptr,
		  "%C :%4d.%1d  %4d.%1d  %4d  %4d  %4d   %s", sptr,
		  times[0][i] / 10, times[0][i] % 10,
		  times[1][i] / 10, times[1][i] % 10,
		  times[2][i], times[3][i], times[4][i], what[i]);
}

/** Initialize the userload statistics. */
void initload(void)
{
  memset(&current_load, 0, sizeof(current_load));
  update_load();                /* Initialize the load list */
}
