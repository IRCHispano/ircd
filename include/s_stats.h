/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/s_stats.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2000 Joseph Bongaarts
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
 * @brief Report configuration lines and other statistics from this server.
 * @version $Id: s_stats.h,v 1.6 2007-04-19 22:53:47 zolty Exp $
 */
#ifndef INCLUDED_s_stats_h
#define INCLUDED_s_stats_h

#ifndef INCLUDED_features_h
#include "ircd_features.h"
#endif

struct Client;

struct StatDesc;

/** Statistics callback function.
 * @param[in] cptr Client requesting statistics.
 * @param[in] sd Stats descriptor for request.
 * @param[in] param Extra parameter from user (may be NULL).
 */
typedef void (*StatFunc)(struct Client *cptr, const struct StatDesc *sd, char *param);

/** Statistics entry. */
struct StatDesc
{
  char         sd_c;           /**< stats character (or '\\0') */
  char        *sd_name;        /**< full name for stats */
  unsigned int sd_flags;       /**< flags to control the stats */
  enum Feature sd_control;     /**< feature controlling stats */
  StatFunc     sd_func;        /**< function to dispatch to */
  int          sd_funcdata;    /**< extra data for the function */
  char        *sd_desc;        /**< descriptive text */
};

#define STAT_FLAG_OPERONLY 0x01    /**< Oper-only stat */
#define STAT_FLAG_OPERFEAT 0x02    /**< Oper-only if the feature is true */
#define STAT_FLAG_LOCONLY  0x04    /**< Local user only */
#define STAT_FLAG_CASESENS 0x08    /**< Flag is case-sensitive */
#define STAT_FLAG_VARPARAM 0x10    /**< May have an extra parameter */

extern void stats_init(void);
const struct StatDesc *stats_find(const char *name_or_char);

#endif /* INCLUDED_s_stats_h */
