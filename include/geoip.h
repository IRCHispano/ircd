/*
 * IRC - Internet Relay Chat, include/geoip.h
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

#if !defined(GEOIP_H)
#define GEOIP_H

enum GeoActionType {
  GAT_NO_ACTION = 0,
  /* ... */
};

extern void geoip_init();
extern void geoip_reload();
extern char *geoip_get_country(char *ip, char **error);
extern int geoip_get_asnum(char *ip, char *name, char **error);
extern void geoip_end();

#endif /* GEOIP_H */
