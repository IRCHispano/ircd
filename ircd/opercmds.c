/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/opercmds.c
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Implementation of AsLL ping helper commands.
 * @version $Id$
 */
#include "config.h"

#include "opercmds.h"
#include "class.h"
#include "client.h"
#include "ircd.h"
#include "ircd_chattr.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "listener.h"
#include "match.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_conf.h"
#include "send.h"
#include "struct.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/** Calculate current time or elapsed time.
 *
 * If neither \a sec nor \a usec are NULL, calculate milliseconds
 * elapsed since that time, and return a string containing that
 * number.
 *
 * If either \a sec or \a usec are NULL, format a timestamp containing
 * Unix timestamp and microseconds since that second (separated by
 * spaces), and return a string containing that timestamp.
 *
 * @todo This should be made into two functions.
 * @param[in] sec Either NULL or a Unix timestamp in seconds.
 * @param[in] usec Either NULL or an offset to \a sec in microseconds.
 * @return A static buffer with contents as described above.
 */
char *militime(char* sec, char* usec)
{
  struct timeval tv;
  static char timebuf[18];

  gettimeofday(&tv, NULL);
  if (sec && usec)
    sprintf(timebuf, "%ld",
        (tv.tv_sec - atoi(sec)) * 1000 + (tv.tv_usec - atoi(usec)) / 1000);
  else
    sprintf(timebuf, "%ld %ld", tv.tv_sec, tv.tv_usec);
  return timebuf;
}

/** Calculate current time or elapsed time.
 *
 * If \a start is NULL, create a timestamp containing Unix timestamp
 * and microseconds since that second (separated by a period), and
 * return a string containing that timestamp.
 *
 * Otherwise, if \a start does not contain a period, return a string
 * equal to "0".
 *
 * Otherwise, calculate milliseconds elapsed since the Unix time
 * described in \a start (in the format described above), and return a
 * string containing that number.
 *
 * @todo This should be made into two functions.
 * @param[in] start Either NULL or a Unix timestamp in
 * pseudo-floating-point format.
 * @return A static buffer with contents as described above.
 */
char *militime_float(char* start)
{
  struct timeval tv;
  static char timebuf[18];
  char *p;

  gettimeofday(&tv, NULL);
  if (start)
  {
    if ((p = strchr(start, '.')))
    {
      p++;
      sprintf(timebuf, "%ld",
          (tv.tv_sec - atoi(start)) * 1000 + (tv.tv_usec - atoi(p)) / 1000);
    }
    else
      strcpy(timebuf, "0");
  }
  else
    sprintf(timebuf, "%ld.%ld", tv.tv_sec, tv.tv_usec);
  return timebuf;
}
