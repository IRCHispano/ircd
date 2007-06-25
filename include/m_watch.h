/*
 * IRC - Internet Relay Chat, include/m_watch.h
 * Copyright (C) 2002 IRC-Hispano.org - Zoltan
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

#if !defined(M_WATCH_H)
#define M_WATCH_H

#if defined(WATCH)
extern int m_watch(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern void chequea_estado_watch(aClient *cptr, int raw, char *ip_override,
    char *ip_override_SeeHidden);
extern int borra_lista_watch(aClient *cptr);
#endif /* WATCH */

#endif /* M_WATCH_H */
