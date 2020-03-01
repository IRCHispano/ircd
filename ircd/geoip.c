/*
 * IRC - Internet Relay Chat, ircd/geoip.c
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

#if defined(USE_GEOIP2)
#include "h.h"
#include "geoip.h"
#include "s_bdd.h"

#include <maxminddb.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define PATH_MAXMIND         "/var/lib/GeoIP/"
#define MAXMIND_COUNTRYDB    "GeoLite2-Country.mmdb"
#define MAXMIND_ASNDB        "GeoLite2-ASN.mmdb"

static MMDB_s geoDbCountry, geoDbASN;
static int db_open_country, db_open_asn;
static char buffer[256];
static time_t db_date_country, db_date_asn;

static char *geoip_dbtime(MMDB_s db);

static void geoip_load()
{
}

void geoip_init()
{
}

void geoip_reload()
{
}

static char *geoip_dbtime(MMDB_s db)
{
    return NULL;
}

char *geoip_get_country(char *ip, char **error)
{
    *error = "NO_SOPORTADO";
    return NULL;
}

int geoip_get_asnum(char *ip, char *name, char **error)
{
    *error = "NO_SOPORTADO";
    return 0;
}

void geoip_end()
{

}

#endif /* USE_GEOIP2 */
