/*
 * IRC - Internet Relay Chat, include/s_bdd.h
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

#if !defined(S_BDD_H)
#define S_BDD_H

#include "struct.h"

#define ESNET_BDD         'a'
#define ESNET_BDD_END     'z'
#define ESNET_NICKDB      'n'
#define ESNET_CLONESDB    'i'

#define BDD_BOTSDB         'b'
#define BDD_CHANDB         'c'
#define BDD_CHAN2DB        'd'
#define BDD_EXCEPTIONDB    'e'
#define BDD_FEATURESDB     'f'
#define BDD_JUPEDB         'j'
#define BDD_LOGGINGDB      'l'
#define BDD_MOTDDB         'm'
#define BDD_OPERDB         'o'
#define BDD_PSEUDODB       'p'
#define BDD_QUARANTINEDB   'q'
#define BDD_CHANREDIRECTDB 'r'
#define BDD_UWORLDDB       'u'
#define BDD_IPVIRTUALDB    'v'
#define BDD_WEBIRCDB       'w'
#define BDD_PROXYDB        'y'
#define BDD_CONFIGDB       'z'
#define BDD_CHANDB_OLD     'z'


/*
 * Registros compatibles con features del nuevo ircd
 */
#define BDD_ACTIVAR_IDENT                       "noident"
#define BDD_AUTOINVISIBLE                       "autousermodes"
#define BDD_MENSAJE_GLINE                       "msg_glined"
#define BDD_NICKLEN                             "nicklen"
#define BDD_NUMERO_MAXIMO_DE_CLONES_POR_DEFECTO "ipcheck_default_iline"
#define BDD_MENSAJE_DE_DEMASIADOS_CLONES        "ipcheck_msg_toomany"
#define BDD_OCULTAR_SERVIDORES                  "his_netsplit"
#define BDD_SERVER_NAME                         "his_servername"
#define BDD_SERVER_INFO                         "his_serverinfo"
#define BDD_NETWORK                             "network"
#define BDD_CANAL_OPERS                         "operschan"
#define BDD_CANAL_DEBUG                         "debugchan"
#define BDD_CANAL_PRIVSDEBUG                    "privsdebugchan"
#define BDD_CONVERSION_UTF                      "utf8_conversion"
#define BDD_MENSAJE_DE_CAPACIDAD_SUPERADA       "msg_fullcapacity"
#define BDD_PERMITE_NICKS_RANDOM		"allow_random_nicks"
#define BDD_PERMITE_NICKS_SUSPEND               "allow_suspend_nicks"
#define BDD_CLAVE_DE_CIFRADO_DE_IPS             "ip_crypt_key"

/* Para las features de los pseudoBOTS tabla 'c' */
#define BDD_CHANSERV      "chanserv"
#define BDD_NICKSERV      "nickserv"
#define BDD_CLONESSERV    "clonesserv"

/* Privilegios IRCD */
#define PRIV_CHAN_LIMIT      0x000000001 /**< no channel limit on oper */
#define PRIV_MODE_LCHAN      0x000000002 /**< oper can mode local chans */
#define PRIV_WALK_LCHAN      0x000000004 /**< oper can walk through local modes */
#define PRIV_DEOP_LCHAN      0x000000008 /**< no deop oper on local chans */
#define PRIV_SHOW_INVIS      0x000000010 /**< show local invisible users */
#define PRIV_SHOW_ALL_INVIS  0x000000020 /**< show all invisible users */
#define PRIV_UNLIMIT_QUERY   0x000000040 /**< unlimit who queries */
#define PRIV_KILL            0x000000080 /**< oper can KILL */
#define PRIV_LOCAL_KILL      0x000000100 /**< oper can local KILL */
#define PRIV_REHASH          0x000000200 /**< oper can REHASH */
#define PRIV_RESTART         0x000000400 /**< oper can RESTART */
#define PRIV_DIE             0x000000800 /**< oper can DIE */
#define PRIV_GLINE           0x000001000 /**< oper can GLINE */
#define PRIV_LOCAL_GLINE     0x000002000 /**< oper can local GLINE */
#define PRIV_JUPE            0x000004000 /**< oper can JUPE */
#define PRIV_LOCAL_JUPE      0x000008000 /**< oper can local JUPE */
#define PRIV_OPMODE          0x000010000 /**< oper can OP/CLEARMODE */
#define PRIV_LOCAL_OPMODE    0x000020000 /**< oper can local OP/CLEARMODE */
#define PRIV_SET             0x000040000 /**< oper can SET */
#define PRIV_WHOX            0x000080000 /**< oper can use /who x */
#define PRIV_BADCHAN         0x000100000 /**< oper can BADCHAN */
#define PRIV_LOCAL_BADCHAN   0x000200000 /**< oper can local BADCHAN */
#define PRIV_SEE_CHAN        0x000400000 /**< oper can see in secret chans */
#define PRIV_PROPAGATE       0x000800000 /**< propagate oper status */
#define PRIV_DISPLAY         0x001000000 /**< "Is an oper" displayed */
#define PRIV_SEE_OPERS       0x002000000 /**< display hidden opers */
#define PRIV_WIDE_GLINE      0x004000000 /**< oper can set wider G-lines */
#define PRIV_LIST_CHAN       0x008000000 /**< oper can list secret channels */
#define PRIV_FORCE_OPMODE    0x010000000/**< can hack modes on quarantined channels */
#define PRIV_FORCE_LOCAL_OPMODE 0x020000000 /**< can hack modes on quarantined local channels */
#define PRIV_APASS_OPMODE    0x040000000 /**< can hack modes +A/-A/+U/-U */
#define PRIV_WALK_CHAN       0x080000000
#define PRIV_NETWORK         0x100000000
#define PRIV_CHANSERV        0x200000000
#define PRIV_HIDDEN_VIEWER   0x400000000
#define PRIV_WHOIS_NOTICE    0x800000000
#define PRIV_HIDE_IDLE       0x1000000000

#define DDB_NICK_FORBID 1
#define DDB_NICK_SUSPEND 2

struct db_reg {
  char *clave;
  char *valor;
  struct db_reg *next;
};

static void bdd_init(void);
void reload_db(void);
void initdb(void);
struct DB_nick *find_db_nick(char *nick);
void tx_num_serie_dbs(aClient *cptr);
int m_db(aClient *cptr, aClient *sptr, int parc, char *parv[]);

void tea(unsigned int v[], unsigned int k[], unsigned int x[]);
struct db_reg *db_buscar_registro(unsigned char tabla, char *clave);
int db_es_residente(unsigned char tabla);
unsigned int db_num_serie(unsigned char tabla);
unsigned int db_cuantos(unsigned char tabla);
struct db_reg *db_iterador_init(unsigned char tabla);
struct db_reg *db_iterador_next(void);

#if defined(BDD_MMAP)
int db_persistent_hit(void);
void db_persistent_commit(void);
#endif

int m_dbq(aClient *cptr, aClient *sptr, int parc, char *parv[]);

extern char *bot_nickserv;
extern char *bot_chanserv;
extern char *bot_clonesserv;
extern int numero_maximo_de_clones_por_defecto;
extern char *clave_de_cifrado_de_ips;
extern unsigned int clave_de_cifrado_binaria[2];
extern int ocultar_servidores;
extern int activar_ident;
extern int auto_invisible;
extern int excepcion_invisible;
extern int desactivar_redireccion_canales;
extern char *mensaje_quit_personalizado;
extern char *mensaje_part_personalizado;
extern char *mensaje_gline;
extern char *network;
extern char *canal_operadores;
extern char *canal_debug;
extern char *canal_privsdebug;
extern int conversion_utf;
extern int permite_nicks_random;
extern int permite_nicks_suspend;

/* -- mman.h no contiene algunas definicieones en plataformas antiguas -- */
#if !defined(MAP_FAILED)
#define MAP_FAILED ((void *) -1)
#endif
#if !defined(MAP_NORESERVE)
#define MAP_NORESERVE 0
#endif
/* -- savage 1999/11/19 -- */

#endif /* S_BDD_H */
