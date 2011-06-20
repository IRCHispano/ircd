/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/ddb.h
 *
 * Copyright (C) 2002-2007 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2004 Toni Garcia (zoltan) <zoltan@irc-dev.net>
 * Copyright (C) 1999-2004 Jesus Cea Avion <jcea@jcea.es>
 * Copyright (C) 1999-2000 Jordi Murgo <savage@apostols.org>
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
 * @brief Distributed DataBases structures, macros and functions.
 * @version $Id: ddb.h,v 1.14 2007-04-21 21:17:22 zolty Exp $
 */
#ifndef INCLUDED_ddb_h
#define INCLUDED_ddb_h

#include "config.h"
#include "client.h"

#include <sys/stat.h>

#if defined(DDB)

/*
 * General defines
 */
/** Maxium number of tables */
#define DDB_TABLE_MAX      256
/** Number of keys is caching */
#define DDB_BUF_CACHE  32

/*
 * Distributed DataBases Tables
 */
/** First table of %DDB Distributed Databases. */
#define DDB_INIT        'a'
/** Bots table of %DDB Distributed Databases. */
#define DDB_BOTSDB      'b'
/** Channels table of %DDB Distributed Databases. */
#define DDB_CHANDB      'c'
/** Channels table (nicks) of %DDB Distributed Databases. */
#define DDB_CHANDB2     'd'
/** Exceptions table of %DDB Distributed Databases. */
#define DDB_EXCEPTIONDB 'e'
/** Features table of %DDB Distributed Databases. */
#define DDB_FEATUREDB   'f'
/** Ilines table of %DDB Distributed Databases. */
#define DDB_ILINEDB     'i'
/** Jupes table of %DDB Distributed Databases. */
#define DDB_JUPEDB      'j'
/** Nicks table of %DDB Distributed Databases. */
#define DDB_NICKDB      'n'
/** Operators table of %DDB Distributed Databases. */
#define DDB_OPERDB      'o'
/** Privileges table of %DDB Distributed Databases. */
#define DDB_PRIVSDB     'p'
/** Channel redirections table of %DDB Distributed Databases. */
#define DDB_CHANREDIRECTDB 'r'
/** Uworld table of %DDB Distributed Databases. */
#define DDB_UWORLDDB    'u'
/** Vhost table of %DDB Distributed Databases. */
#define DDB_VHOSTDB     'v'
/** Colour vhost table of %DDB Distributed Databases. */
#define DDB_COLOURVHOSTDB 'w'
/** Config table of %DDB Distributed Databases. */
#define DDB_CONFIGDB    'z'
/** Last table of %DDB Distributed Databases. */
#define DDB_END         'z'

/*
 * Config keys of config table 'z'
 */
/** Number of max clones per ip */
#define DDB_CONFIGDB_MAX_CLONES_PER_IP      "maxclones"
/** Message to clients with too many clones from your ip */
#define DDB_CONFIGDB_MSG_TOO_MANY_FROM_IP   "msgmanyperip"
/** Key to crypt ips */
#define DDB_CONFIGDB_IP_CRYPT_KEY       "ipcryptkey"

/*
 * PseudoBots
 */
/** Nickname of virtual bot for nicks registers */
#define DDB_NICKSERV    "NickServ"
/** Nickname of virtual bot for channel bot */
#define DDB_CHANSERV   "ChanServ"

/** Describes a key on one table.
 */
struct Ddb {
  char*     ddb_key;    /**< Key of the register */
  char*     ddb_content;    /**< Content of the key */
  struct Ddb*   ddb_next;   /**< Next key on the table */
};

/** Get key of the register. */
#define ddb_key(ddb)        ((ddb)->ddb_key)
/** Get content of the key. */
#define ddb_content(ddb)    ((ddb)->ddb_content)
/** Get next key on the table. */
#define ddb_next(ddb)       ((ddb)->ddb_next)


extern struct Ddb *ddb_iterator_first(unsigned char table);
extern struct Ddb *ddb_iterator_next(void);
extern struct Ddb *ddb_find_key(unsigned char table, char *key);
extern char *ddb_get_botname(char *botname);

#define DDBPWDLEN	12

/*
** Registros de configuracion en la tabla 'z'
*/
#define BDD_NUMERO_MAXIMO_DE_CLONES_POR_DEFECTO	"numero.maximo.de.clones.por.defecto"
#define BDD_MENSAJE_DE_DEMASIADOS_CLONES	"mensaje.de.demasiados.clones"
#define BDD_MENSAJE_DE_CAPACIDAD_SUPERADA	"mensaje.de.capacidad.superada"
#define BDD_CLAVE_DE_CIFRADO_DE_IPS		"clave.de.cifrado.de.ips"
#define BDD_AUTOINVISIBLE    "auto.invisible"
#define BDD_CLAVE_DE_CIFRADO_DE_COOKIES "clave.de.cifrado.de.cookies"
#define BDD_COMPRESION_ZLIB_CLIENTE "compresion.zlib.cliente"
#define BDD_CANAL_DEBUG "debugchan"
/* Para las features de los pseudoBOTS */
#define BDD_CHANSERV      "ChanServ"
#define BDD_NICKSERV      "NickServ"


void reload_db(void);
void initdb(void);
struct DB_nick *find_db_nick(char *nick);
void tx_num_serie_dbs(struct Client *cptr);
int m_db(struct Client *cptr, struct Client *sptr, int parc, char *parv[]);

int db_es_miembro(unsigned char tabla, char *clave, char *subcadena);
int db_es_residente(unsigned char tabla);
unsigned int db_num_serie(unsigned char tabla);
unsigned int db_cuantos(unsigned char tabla);

#if defined(BDD_MMAP)
int db_persistent_hit(void);
void db_persistent_commit(void);
#endif

int m_dbq(struct Client *cptr, struct Client *sptr, int parc, char *parv[]);

extern int numero_maximo_de_clones_por_defecto;
extern char *clave_de_cifrado_de_ips;
extern unsigned int clave_de_cifrado_binaria[2];
extern unsigned char clave_de_cifrado_de_cookies[32];
extern int cifrado_cookies;
extern int ocultar_servidores;
extern int activar_ident;
extern int auto_invisible;
extern int excepcion_invisible;
extern int activar_redireccion_canales;
extern char *mensaje_quit_personalizado;
extern int compresion_zlib_cliente;


#endif
#endif /* S_BDD_H */
