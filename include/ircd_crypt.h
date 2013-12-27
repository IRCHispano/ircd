/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ircd_crypt.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2002 hikari
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
 */
/** @file
 * @brief Core password encryption and hashing APIs.
 * @version $Id: ircd_crypt.h,v 1.4 2007-12-11 23:38:23 zolty Exp $
 */
#ifndef INCLUDED_ircd_crypt_h
#define INCLUDED_ircd_crypt_h

/* forward planning for modularisation */
struct crypt_mech_s {
 char* mechname;     /* name of the mechanism */
 char* shortname;    /* short name of the module */
 char* description;  /* description of the mechanism */

 const char* (*crypt_function)(const char *, const char *);
                     /* pointer to the crypt function */

 char* crypt_token;  /* what identifies a password string
                        as belonging to this mechanism */

 unsigned int crypt_token_size; /* how long is the token */
};

typedef struct crypt_mech_s crypt_mech_t;

struct crypt_mechs_s;
typedef struct crypt_mechs_s crypt_mechs_t;

struct crypt_mechs_s {
 crypt_mech_t* mech;
 crypt_mechs_t* next;
 crypt_mechs_t* prev;
};

/* access macros */
#define MechName(x) x->mechname
#define ShortName(x) x->shortname
#define Description(x) x->description
#define CryptFunc(x) x->crypt_function
#define CryptTok(x) x->crypt_token
#define CryptTokSize(x) x->crypt_token_size

/* exported functions */
extern void ircd_crypt_init(void);
extern char* ircd_crypt(const char* key, const char* salt);
extern int ircd_crypt_register_mech(crypt_mech_t* mechanism);
extern int ircd_crypt_unregister_mech(crypt_mech_t* mechanism);

/* exported variables */
extern crypt_mechs_t* crypt_mechs_root;

#endif /* INCLUDED_ircd_crypt_h */

