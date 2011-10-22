/*
 * IRC - Internet Relay Chat, include/m_config.h
 * Copyright (C) 1999 IRC-Hispano.org - ESNET - jcea & savage
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

#if !defined(M_CONFIG_H)
#define M_CONFIG_H

#include "struct.h"

#if defined(ESNET_NEG)
int m_config(aClient *cptr, aClient *sptr, int parc, char *parv[]);
void envia_config_req(aClient *cptr);
void config_resolve_speculative(aClient *cptr);
#endif

#endif /* M_CONFIG_H */
