/*
 * IRC - Internet Relay Chat, ircd/s_bdd.c
 * Copyright (C) 1999 IRC-Hispano.org - ESNET - jcea & savage
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
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


/*
** ATENCION: Lo que sigue debe incrementarse cuando se toque alguna estructura de la BDD
*/
#define MMAP_CACHE_VERSION 4



#include "sys.h"
#include <stdlib.h>
#include "persistent_malloc.h"

#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include "h.h"
#include "s_debug.h"
#include "struct.h"
#include "ircd.h"
#include "s_serv.h"
#include "s_misc.h"
#include "sprintf_irc.h"
#include "send.h"
#include "s_err.h"
#include "numeric.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "hash.h"
#include "common.h"
#include "match.h"
#include "crule.h"
#include "parse.h"
#include "numnicks.h"
#include "userload.h"
#include "s_user.h"
#include "channel.h"
#include "querycmds.h"
#include "IPcheck.h"
#include "network.h"
#include "slab_alloc.h"

#include "s_bdd.h"

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
#include "dbuf.h"
#endif

#include "msg.h"
#include "support.h"

#include <json-c/json.h>
#include <json-c/json_object.h>

char *bot_nickserv = NULL;
char *bot_chanserv = NULL;
char *bot_clonesserv = NULL;
int numero_maximo_de_clones_por_defecto;
char *clave_de_cifrado_de_ips;
unsigned int clave_de_cifrado_binaria[2];
int ocultar_servidores = 0;
int activar_modos = 0;
int activar_ident = 0;
int auto_invisible = 0;
int excepcion_invisible = 0;
int desactivar_redireccion_canales = 0;
char *mensaje_quit_personalizado = NULL;
char *mensaje_part_personalizado = NULL;
char *mensaje_gline = NULL;
char *network = NULL;
char *canal_operadores = NULL;
char *canal_debug = NULL;
char *canal_privsdebug = NULL;
int conversion_utf = 0;
int permite_nicks_random = 0;
int permite_nicks_suspend = 0;
unsigned char bdd_initialized = 0;

/*
 * Las tablas con los registros, serie, version ...
 */

#define DB_MAX_TABLA      256

/*
** Numero de peticiones de registros
** antes de que haya que empezar a reciclar
*/
#define DB_BUF_CACHE  32

static struct db_reg db_buf_cache[DB_BUF_CACHE];
static int db_buf_cache_i = 0;

struct tabla_en_memoria {
  char *posicion;
  unsigned int len;
  char *puntero_r;
  char *puntero_w;
};


/*
** Si modificamos esta estructura habra que
** actualizar tambien 'db_die_persistent'.
*/
struct portable_stat {
  dev_t dev;                    /* ID of device containing a directory entry for this file */
  ino_t ino;                    /* Inode number */
  off_t size;                   /* File size in bytes */
  time_t mtime;                 /* Time of last data modification */
};

/*
** Si se meten mas datos aqui, hay que acordarse de
** gestionarlos en el sistema de persistencia.
*/
static unsigned int tabla_residente_y_len[DB_MAX_TABLA];
static unsigned int tabla_cuantos[DB_MAX_TABLA];
static struct db_reg **tabla_datos[DB_MAX_TABLA];
static unsigned int tabla_serie[DB_MAX_TABLA];
static unsigned int tabla_hash_hi[DB_MAX_TABLA];
static unsigned int tabla_hash_lo[DB_MAX_TABLA];
#if defined(BDD_MMAP)
static struct portable_stat tabla_stats[DB_MAX_TABLA];
#endif

#if defined(BDD_MMAP)
static void *mmap_cache_pos = NULL;
#endif

static struct db_reg *db_iterador_reg = NULL;
static struct db_reg **db_iterador_datos = NULL;
static int db_iterador_hash_pos = 0;
static int db_iterador_hash_len = 0;

static int persistent_hit;

int db_persistent_hit(void)
{
  return persistent_hit;
}

#if defined(BDD_MMAP)
#define p_malloc(a)	persistent_malloc(a)
#define p_free(a)	persistent_free(a)
#else
#define p_malloc(a)	RunMalloc(a)
#define p_free(a)	RunFree(a)
#endif

static struct portable_stat *get_stat(int handle, struct portable_stat *st)
{
  struct stat st2;

  fstat(handle, &st2);

/*
** Necesario en arquitecturas de 64 bits
** porque los bytes de "padding"
** no son inicializados y contienen
** valores arbitrarios
*/
  memset(st, 0, sizeof(*st));

  st->dev = st2.st_dev;
  st->ino = st2.st_ino;
  st->size = st2.st_size;
  st->mtime = st2.st_mtime;

  return st;
}

int db_es_residente(unsigned char tabla)
{
  return tabla_residente_y_len[tabla] ? 1 : 0;
}

unsigned int db_num_serie(unsigned char tabla)
{
  return tabla_serie[tabla];
}

unsigned int db_cuantos(unsigned char tabla)
{
  return tabla_cuantos[tabla];
}


/*
 * TEA (cifrado)
 *
 * Cifra 64 bits de datos, usando clave de 64 bits (los 64 bits superiores son cero)
 * Se cifra v[0]^x[0], v[1]^x[1], para poder hacer CBC facilmente.
 *
 */
void tea(unsigned int v[], unsigned int k[], unsigned int x[])
{
  unsigned int y = v[0] ^ x[0], z = v[1] ^ x[1], sum = 0, delta = 0x9E3779B9;
  unsigned int a = k[0], b = k[1], n = 32;
  unsigned int c = 0, d = 0;

  while (n-- > 0)
  {
    sum += delta;
    y += ((z << 4) + a) ^ ((z + sum) ^ ((z >> 5) + b));
    z += ((y << 4) + c) ^ ((y + sum) ^ ((y >> 5) + d));
  }

  x[0] = y;
  x[1] = z;
}

static void actualiza_hash(char *registro, unsigned char que_bdd)
{
  unsigned int buffer[129 * sizeof(unsigned int)];
  unsigned int *p = buffer;
  unsigned int x[2], v[2], k[2];
  char *p2;

/*
** Calculamos el HASH
*/
  memset(buffer, 0, sizeof(buffer));
  strncpy((char *)buffer, registro, sizeof(buffer) - 1);
  while ((p2 = strchr((char *)buffer, '\n')))
    *p2 = '\0';
  while ((p2 = strchr((char *)buffer, '\r')))
    *p2 = '\0';
  k[0] = k[1] = 0;
  x[0] = tabla_hash_hi[que_bdd];
  x[1] = tabla_hash_lo[que_bdd];
  while (*p)
  {
    v[0] = ntohl(*p);
    p++;                        /* No se puede hacer a la vez porque la linea anterior puede ser una expansion de macros */
    v[1] = ntohl(*p);
    p++;                        /* No se puede hacer a la vez porque la linea anterior puede ser una expansion de macros */
    tea(v, k, x);
  }
  tabla_hash_hi[que_bdd] = x[0];
  tabla_hash_lo[que_bdd] = x[1];
}

/*
** Esta funcion SOLO debe llamarse desde "db_iterador_init" y "db_iterador_next"
*/
static struct db_reg *db_iterador()
{
  struct db_reg *p;

  if (db_iterador_reg)
  {
    db_iterador_reg = db_iterador_reg->next;
    if (db_iterador_reg)
    {
      return db_iterador_reg;
    }
  }
/*
** "db_iterador_hash_pos" siempre indica el PROXIMO valor a utilizar.
*/
  while (db_iterador_hash_pos < db_iterador_hash_len)
  {
    p = db_iterador_datos[db_iterador_hash_pos++];
    if (p)
    {
      db_iterador_reg = p;
      return p;
    }
  }

  db_iterador_reg = NULL;
  db_iterador_datos = NULL;
  db_iterador_hash_pos = 0;
  db_iterador_hash_len = 0;

  return NULL;
}

struct db_reg *db_iterador_init(unsigned char tabla)
{
  assert((tabla >= ESNET_BDD) && (tabla <= ESNET_BDD_END));
  db_iterador_hash_len = tabla_residente_y_len[tabla];
  assert(db_iterador_hash_len);
  db_iterador_hash_pos = 0;
  db_iterador_datos = tabla_datos[tabla];
  db_iterador_reg = NULL;
  return db_iterador();
}

struct db_reg *db_iterador_next(void)
{
  assert(db_iterador_reg);
  assert(db_iterador_datos);
  assert(db_iterador_hash_len);

  return db_iterador();
}

/*
** db_busca_db_reg
*/
static struct db_reg *db_busca_db_reg(unsigned char tabla, char *clave)
{
  static char *c = NULL;
  static int c_len = 0;
  int i, hashi;
  struct db_reg *reg;

  if ((strlen(clave) + 1 > c_len) || (!c))
  {
    c_len = strlen(clave) + 1;
    if (c)
      RunFree(c);
    c = RunMalloc(c_len);
    if (!c)
      return 0;
  }
  strcpy(c, clave);
  /* paso a minusculas */
  i = 0;
  while (c[i] != 0)
  {
    c[i] = toLower(c[i]);
    i++;
  }
  hashi = db_hash_registro(c, tabla_residente_y_len[tabla]);

  for (reg = tabla_datos[tabla][hashi]; reg != NULL; reg = reg->next)
  {
    if (!strcmp(reg->clave, c))
      return reg;
  }
  return NULL;
}

/*
 * elimina_cache_ips_virtuales
 *
 * Elimina la cach� de las ips virtuales cuando se cambia el valor de la clave de cifrado
 *
 */
static inline void elimina_cache_ips_virtuales(void)
{
  aClient *acptr;

  /* A limpiar la cache */
  for (acptr = client; acptr; acptr = acptr->next)
  {
    if (IsUser(acptr))
      BorraIpVirtual(acptr);
  }
}

/*
 * db_eliminar_registro (tabla, clave)
 *
 * libera (free) un registro de memoria.
 *                                      1999/06/23 savage@apostols.org
 */
static void db_eliminar_registro(unsigned char tabla, char *clave,
    int reemplazar, aClient *cptr, aClient *sptr)
{
  char buf[100];
  int mode;
  struct db_reg *reg, *reg2, **reg3;
  aChannel *chptr;
  int hashi, i = 0;
  static char *c = NULL;
  static int c_len = 0;

  db_iterador_reg = NULL;

  if ((strlen(clave) + 1 > c_len) || (!c))
  {
    c_len = strlen(clave) + 1;
    if (c)
      RunFree(c);
    c = RunMalloc(c_len);
    if (!c)
      return;
  }
  strcpy(c, clave);
  /* paso a minusculas */
  while (c[i] != 0)
  {
    c[i] = toLower(c[i]);
    i++;
  }

  hashi = db_hash_registro(c, tabla_residente_y_len[tabla]);
  
  reg3 = &tabla_datos[tabla][hashi];

  for (reg = *reg3; reg != NULL; reg = reg2)
  {
    if(sptr && !IsBurstOrBurstAck(sptr))
      sendto_op_mask(SNO_SERVICE,
          "%s DB DELETE T='%c' C='%s' H=0x%x", sptr->name, tabla, reg->clave, hashi);
    
    reg2 = reg->next;
    if (!strcmp(reg->clave, c))
    {
      *reg3 = reg2;

      switch (tabla)
      {
        /*
        *
        * Baja de nicks instant�nea.
        *
        */
        case ESNET_NICKDB:
       {
        aClient *sptr;

        if (!reemplazar && (sptr = FindUser(clave)) && MyConnect(sptr))
        {
         /* El usuario esta conectado, y en nuestro servidor. */
         int of, oh;

         of = sptr->flags;
         oh = sptr->hmodes;

         sendto_one(sptr,
              ":%s NOTICE %s :*** Tu nick %s se ha eliminado",
              bot_nickserv ? bot_nickserv : me.name, clave, clave);

         ClearNickRegistered(sptr);
         ClearNickSuspended(sptr);
         ClearHelpOp(sptr);
         if (IsOper(sptr))
             nrof.opers--;

         ClearOper(sptr);
         ClearAdmin(sptr);
         ClearCoder(sptr);
         ClearServicesBot(sptr);

         send_umode_out(sptr, sptr, of, oh, IsRegistered(sptr));
        }
        break;
       }
        /* 05-Ene-04: mount@irc-dev.net
        *
        * Baja de operadores instant�nea.
        *
        */
        case BDD_OPERDB:
       {
        aClient *sptr;

        if ((sptr = FindUser(clave)) && MyConnect(sptr))
        {
         /* El usuario est� conectado, y en nuestro servidor. */
         int of, oh;

         of = sptr->flags;
         oh = sptr->hmodes;

         ClearHelpOp(sptr);
         if (IsOper(sptr))
             nrof.opers--;

         ClearOper(sptr);
         ClearAdmin(sptr);
         ClearCoder(sptr);
         ClearServicesBot(sptr);

         send_umode_out(sptr, sptr, of, oh, IsRegistered(sptr));
        }
       }

        case BDD_BOTSDB:
          if (!reemplazar)
          {
            if (!strcmp(c, BDD_NICKSERV))
            {
              if (bot_nickserv)
              {
                RunFree(bot_nickserv);
                bot_nickserv=NULL;
              }
            }
            else if (!strcmp(c, BDD_CHANSERV))
            {
              if (bot_chanserv)
              {
                RunFree(bot_chanserv);
                bot_chanserv=NULL;
              }
            }
            else if (!strcmp(c, BDD_CLONESSERV))
            {
              if (bot_clonesserv)
              {
                RunFree(bot_clonesserv);
                bot_clonesserv=NULL;
              }
            }
          }                     /* Fin de "!reemplazar" */
          break;

        case BDD_CHANDB:
          if (!reemplazar)
          {
            chptr = get_channel(NULL, c, !CREATE);
            if (chptr && ((mode = chptr->mode.mode) & MODE_REGCHAN))
            {
              chptr->mode.mode = mode & (~(MODE_REGCHAN));  /* Modos vinculados a +r */
              chptr->modos_obligatorios = chptr->modos_prohibidos = 0;
#if 0
              if (chptr->owner)
                RunFree(chptr->owner);
#endif

              if (chptr->users)
              {                 /* Quedan usuarios */
//                Link *lp;

                strcpy(buf, "-r");
                sendto_channel_butserv(chptr, &me, ":%s MODE %s %s", bot_chanserv ? bot_chanserv : me.name,
                    chptr->chname, buf);

#if 0
                /* Eliminamos antiguo founder si existe */
                for (lp = chptr->members; lp; lp = lp->next)
                {
                  if ((lp->flags & CHFL_OWNER)
                      && MyUser(lp->value.cptr))
                  {
                    lp->flags &= ~CHFL_OWNER;
                    sendto_channel_butserv(chptr, NULL, ":%s MODE %s -q %s",
                        bot_chanserv ? bot_chanserv : me.name, chptr->chname, lp->value.cptr->name);
                    sendto_serv_butone(&me, "%s " TOK_BMODE " %s %s -q %s%s",
                        NumServ(&me), BDD_CHANSERV, chptr->chname, NumNick(lp->value.cptr));
                    break;
                  }
                }
#endif
              }
              else
              {                 /* Canal Vacio */
                sub1_from_channel(chptr); /* Elimina el canal */
              }
            }
          }
          break;

        case BDD_FEATURESDB:
          if (!reemplazar)
          {
            if (!strcmp(c, BDD_NUMERO_MAXIMO_DE_CLONES_POR_DEFECTO))
            {
              numero_maximo_de_clones_por_defecto = 0;
            }
            else if (!strcmp(c, BDD_OCULTAR_SERVIDORES))
            {
              ocultar_servidores = 0;
            }
            else if (!strcmp(c, BDD_ACTIVAR_IDENT))
            {
              activar_ident = 0;
            }
            else if (!strcmp(c, BDD_SERVER_NAME))
            {
              SlabStringAllocDup(&(his.name), SERVER_NAME, HOSTLEN);
            }
            else if (!strcmp(c, BDD_SERVER_INFO))
            {
              SlabStringAllocDup(&(his.info), SERVER_INFO, REALLEN);
            }
            else if (!strcmp(c, BDD_AUTOINVISIBLE))
            {
              auto_invisible = 0;
            }
            else if (!strcmp(c, BDD_NICKLEN))
            {
              nicklen = 30;
            }
            else if(!strcmp(c, BDD_MENSAJE_GLINE))
            {
              if(mensaje_gline)
              {
                RunFree(mensaje_gline);
                mensaje_gline=NULL;
              }
            }
            else if(!strcmp(c, BDD_NETWORK))
            {
              if(network)
              {
                RunFree(network);
                network=NULL;
              }
            }
            else if(!strcmp(c, BDD_CANAL_OPERS))
            {
              if(canal_operadores)
              {
                RunFree(canal_operadores);
                canal_operadores=NULL;
              }
            }
            else if(!strcmp(c, BDD_CANAL_DEBUG))
            {
              if(canal_debug)
              {
                RunFree(canal_debug);
                canal_debug=NULL;
              }
            }
            else if(!strcmp(c, BDD_CANAL_PRIVSDEBUG))
            {
              if(canal_privsdebug)
              {
                RunFree(canal_privsdebug);
                canal_privsdebug=NULL;
              }
            }

            else if (!strcmp(c, BDD_CONVERSION_UTF))
            {
              conversion_utf = 0;
            }
            else if (!strcmp(c, BDD_PERMITE_NICKS_RANDOM))
            {
              permite_nicks_random = 0;
            }
            else if (!strcmp(c, BDD_PERMITE_NICKS_SUSPEND))
            {
              permite_nicks_suspend = 0;
            }
          }                     /* Fin de "!reemplazar" */
          break;

        case BDD_CONFIGDB:
          if (!reemplazar)
          {
            if (!strcmp(c, BDD_CLAVE_DE_CIFRADO_DE_IPS))
            {
              clave_de_cifrado_de_ips = NULL;
              clave_de_cifrado_binaria[0] = 0;
              clave_de_cifrado_binaria[1] = 0;
              elimina_cache_ips_virtuales();
            }
            else if(!strncmp(c, "noredirect:", 9) && !strcmp(c+9, me.name))
            {
              desactivar_redireccion_canales=0;
            }
            else if(!strncmp(c, "quit:", 5) && !strcmp(c+5, me.name))
            {
              if(mensaje_quit_personalizado)
              {
                RunFree(mensaje_quit_personalizado);
                mensaje_quit_personalizado=NULL;
              }
            }
            else if(!strncmp(c, "part:", 5) && !strcmp(c+5, me.name))
            {
              if(mensaje_part_personalizado)
              {
                RunFree(mensaje_part_personalizado);
                mensaje_part_personalizado=NULL;
              }
            }
            else if(!strncmp(c, "noinvisible:", 12) && !strcmp(c+12, me.name))
            {
              excepcion_invisible=0;
            }
          }                     /* Fin de "!reemplazar" */
          break;

        case BDD_IPVIRTUALDB:
          if (!reemplazar)
          {
            aClient *sptr;
            if ((sptr = FindUser(c)) && IsUser(sptr))
            {
              BorraIpVirtualPerso(sptr);
              if (MyUser(sptr))
                sendto_one(sptr, rpl_str(RPL_HOSTHIDDEN), me.name, sptr->name,
                    get_virtualhost(sptr, 0));
            }
          }
          break;
        case BDD_EXCEPTIONDB:
          if(cptr==NULL)
            break;

          /* Al borrar una eline hay que comprobar si hay usuarios
           * que puedan verse afectados por glines */

          {
            aClient *acptr;
            int found_g;
            for (i = 0; i <= highest_fd; i++)
            {
              if ((acptr = loc_clients[i]) && !IsMe(acptr))
              {
                if ((found_g = find_kill(acptr)))
                {
                  sendto_op_mask(found_g == -2 ? SNO_GLINE : SNO_OPERKILL,
                      found_g == -2 ? "G-line active for %s" : "K-line active for %s",
                      get_client_name(acptr, FALSE));
                  exit_client(cptr, acptr, &me, found_g == -2 ? "G-lined" : "K-lined");
                }
          #if defined(R_LINES) && defined(R_LINES_REHASH) && !defined(R_LINES_OFTEN)
                if (find_restrict(acptr))
                {
                  sendto_ops("Restricting %s, closing lp", get_client_name(acptr, FALSE));
                  exit_client(cptr, acptr, &me, "R-lined") == CPTR_KILLED);
                }
          #endif
              }
            }
          }
      }
      p_free(reg);
      tabla_cuantos[tabla]--;
      break;
    }
    reg3 = &(reg->next);
  }
}

static inline void crea_canal_persistente(char *nombre, char *valor, int virgen)
{
  aChannel *chptr;
//  aClient *acptr = NULL;
//  Link *lp;
  int add, del;
  char *name = NULL;
  char *modes = NULL;
  char *owner = NULL;
  int cambionombre = 0;
#if 0
  int sameowner = 0;
#endif

  if (*valor == '{')
  {
    /* Formato nuevo JSON */
    json_object *json, *json_name, *json_modes, *json_owner;
    enum json_tokener_error jerr = json_tokener_success;

    json = json_tokener_parse_verbose(valor, &jerr);
    if (jerr != json_tokener_success)
      return;

    json_object_object_get_ex(json, "name", &json_name);
    name = (char *)json_object_get_string(json_name);
    json_object_object_get_ex(json, "modes", &json_modes);
    modes = (char *)json_object_get_string(json_modes);
    json_object_object_get_ex(json, "owner", &json_owner);
    owner = (char *)json_object_get_string(json_owner);
  }
  else
  {
    /* Formato antiguo modos:nombre */
    DupString(modes, valor);
    name=strchr(modes,':'); /* Nombre con mays/mins correcto */

    /* Si hay nombre especificado ... */
    if (name != NULL)
    {
      *name='\0';                  /* corto token */
      name++;
    }
  }

  if (name != NULL)
  {
    if (strCasecmp(nombre, name)) /* Si no coincide el nombre vuelvo al original */
      name=nombre;
    else
      cambionombre = 1;
  } else
    name=nombre;   /* Si no hay nombre especificado vuelvo al original */

  chptr = get_channel(NULL, name, CREATE);
  mascara_canal_flags(modes, &add, &del);
  chptr->modos_obligatorios = add | MODE_REGCHAN;
  chptr->modos_prohibidos = del & ~MODE_REGCHAN;

  /* ajustamos el nombre equivalente */
  if (cambionombre && strcmp(nombre, name)) {
      hRemChannel(chptr);
      strcpy(chptr->chname, name);
      hAddChannel(chptr);
  }

#if 0
  if (chptr->owner)
    RunFree(chptr->owner);
  if (owner)
     DupString(chptr->owner, owner);
#endif

  if (chptr->users)
  {                             /* Hay usuarios dentro */
    if (!(chptr->mode.mode & MODE_REGCHAN))
    {
      sendto_channel_butserv(chptr, &me, ":%s MODE %s +r", bot_chanserv ? bot_chanserv : me.name,
          chptr->chname);
    }

#if 0
    if (owner)
      acptr = FindUser(owner);

    /* Eliminamos antiguo founder si existe y es un nick distinto */
    for (lp = chptr->members; lp; lp = lp->next)
    {
      if (lp->flags & CHFL_OWNER)
      {
        if (acptr && acptr == lp->value.cptr)
        {
          sameowner = 1;
          break;
        }

        if (!MyUser(lp->value.cptr))
          break;

        lp->flags &= ~CHFL_OWNER;
        sendto_channel_butserv(chptr, NULL, ":%s MODE %s -q %s",
            bot_chanserv ? bot_chanserv : me.name, chptr->chname, lp->value.cptr->name);
        sendto_serv_butone(&me, "%s " TOK_BMODE " %s %s -q %s%s",
            NumServ(&me), BDD_CHANSERV, chptr->chname, NumNick(lp->value.cptr));
        break;
      }
    }

    /* Ponemos founder nuevo si es usuario local */
    if (owner && !sameowner)
    {
      if (acptr && MyUser(acptr))
      {
        lp = IsMember(acptr, chptr);
        if (lp)
        {
          lp->flags |= CHFL_OWNER;
          sendto_channel_butserv(chptr, NULL, ":%s MODE %s +q %s",
              bot_chanserv ? bot_chanserv : me.name, chptr->chname, lp->value.cptr->name);
          sendto_serv_butone(&me, "%s " TOK_BMODE " %s %s +q %s%s",
              NumServ(&me), BDD_CHANSERV, chptr->chname, NumNick(lp->value.cptr));
        }
      }
    }
#endif
  }
  if (virgen)
  {
    chptr->mode.mode &= ~del;   /* Primero esto, por si hay un '-r' */
    chptr->mode.mode |= add;
    chptr->mode.mode &= ~MODE_WPARAS; /* Estos modos son especiales y no se guardan aqui */
  }
  chptr->mode.mode |= MODE_REGCHAN;

  if (*valor != '{')
    RunFree(modes);
}

/*
 * db_insertar_registro (tabla, clave, valor)
 *
 * mete un registro en memoria ...
 *                                      1999/06/23 savage@apostols.org
 */
static void db_insertar_registro(unsigned char tabla, char *clave, char *valor,
    aClient *cptr, aClient *sptr)
{
  struct db_reg *reg;
  int hashi;
  char *c, *v;
  int i = 0;

  db_iterador_reg = NULL;

  /* lo borro primero, por si es un cambio */
  db_eliminar_registro(tabla, clave, 1, cptr, sptr);

/*
** Guardamos los datos del registro justo al lado del registro, para
** reducir la ocupacion de memoria y, sobre todo, el numero de
** Mallocs.
*/
  reg = p_malloc(sizeof(struct db_reg) + strlen(clave) + 1 + strlen(valor) + 1);
  assert(reg);

  c = (char *)reg + sizeof(struct db_reg);
  v = c + strlen(clave) + 1;

  strcpy(c, clave);
  strcpy(v, valor);

  /* paso a minusculas */
  while (c[i] != 0)
  {
    c[i] = toLower(c[i]);
    i++;
  }

  /* creo el registro */
  reg->clave = c;
  reg->valor = v;
  reg->next = NULL;

  /* busco hash */
  hashi = db_hash_registro(reg->clave, tabla_residente_y_len[tabla]);

  /*
     sendto_ops("Inserto T='%c' C='%s' H=%u",tabla, reg->clave, hashi);
   */
  if(sptr && !IsBurstOrBurstAck(sptr))
    sendto_op_mask(SNO_SERVICE,
        "%s DB INSERT T='%c' C='%s' H=0x%x", sptr->name, tabla, reg->clave, hashi);
  
  reg->next = tabla_datos[tabla][hashi];
  tabla_datos[tabla][hashi] = reg;

  tabla_cuantos[tabla]++;

  switch (tabla)
  {
   case BDD_BOTSDB:
      if(!strcmp(c, BDD_NICKSERV))
      {
        SlabStringAllocDup(&bot_nickserv, v, 0);
      }
      else if(!strcmp(c, BDD_CHANSERV))
      {
        SlabStringAllocDup(&bot_chanserv, v, 0);
      }
      else if(!strcmp(c, BDD_CLONESSERV))
      {
        SlabStringAllocDup(&bot_clonesserv, v, 0);
      }
      break;

     /* 05-Ene-04: mount@irc-dev.net
      *
      * Alta de operadores instant�nea.
      *
      */
   case BDD_OPERDB:
   {
    aClient *sptr;

    /* Solo usuario con +r y en nuestro servidor */
    if ((sptr = FindUser(clave)) && MyConnect(sptr) && IsNickRegistered(sptr))
    {
      int status;

      status = get_status(sptr);
      if (status)
      {
        int of, oh;

        of = sptr->flags;
        oh = sptr->hmodes;

        if (status & HMODE_ADMIN)
          SetAdmin(sptr);

        if (status & HMODE_CODER)
          SetCoder(sptr);

        if (status & HMODE_SERVICESBOT)
          SetServicesBot(sptr);

        if (status & HMODE_HELPOP)
          SetHelpOp(sptr);

        if (status & FLAGS_OPER)
        {
          if (!IsAnOper(sptr))
            nrof.opers++;
          SetOper(sptr);
          sptr->flags |= (FLAGS_WALLOP | FLAGS_SERVNOTICE | FLAGS_DEBUG);
          set_snomask(sptr, SNO_OPERDEFAULT, SNO_ADD);
          set_privs(sptr);
        }

        send_umode_out(sptr, sptr, of, oh, IsRegistered(sptr));
      }
    }
   }

      /* 29-Oct-03: mount@irc-dev.net
       *
       * Efecto de suspends y forbids instant�neos.
       *
       */
    case ESNET_NICKDB:
    {
      aClient *sptr;

      if ((sptr = FindUser(clave)) && MyConnect(sptr))
      {
        int suspendido = 0;
        int prohibido = 0;
        char *automodes;
        char *reason;

        if (*valor == '{')
        {
          /* Formato nuevo JSON */
          json_object *json, *json_flags, *json_reason, *json_automodes;
          enum json_tokener_error jerr = json_tokener_success;
          int flagsddb;

          json = json_tokener_parse_verbose(reg->valor, &jerr);
          if (jerr != json_tokener_success)
            break;

          json_object_object_get_ex(json, "flags", &json_flags);
          flagsddb = json_object_get_int(json_flags);
          json_object_object_get_ex(json, "reason", &json_reason);
          reason = (char *)json_object_get_string(json_reason);
          json_object_object_get_ex(json, "automodes", &json_automodes);
          automodes = (char *)json_object_get_string(json_automodes);

          if (flagsddb == DDB_NICK_SUSPEND)
            suspendido = !0;
          else if (flagsddb == DDB_NICK_FORBID)
            prohibido = !0;

        } else {
          /* Formato antiguo pass_flag */
          char c = valor[strlen(valor) - 1];

          if (c == '+')
            suspendido = !0;
          else if (c == '*')
            prohibido = !0;

          /* No soportado */
          automodes = NULL;
          reason = NULL;

        }

         if (suspendido || prohibido)
        {
          sendto_one(sptr,
              ":%s NOTICE %s :*** Tu nick %s acaba de ser %s. Motivo: %s",
              bot_nickserv ? bot_nickserv : me.name, clave, clave, (suspendido ? "suspendido" : "prohibido"),
              reason ? reason : "N.D.");

          rename_user(sptr, NULL);

        } else if (IsNickRegistered(sptr) || IsNickSuspended(sptr)) {
          /* Igual hay un cambio de modos, mandamos nuevos modos */
          int of, oh;

          of = sptr->flags;
          oh = sptr->hmodes;

          if (IsNickSuspended(sptr))
          {
            /* Deja de tener nick suspendido, recuperamos */
            int status;

            ClearNickSuspended(sptr);
            SetNickRegistered(sptr);

            status = get_status(sptr);
            if (status)
            {
              if (status & HMODE_ADMIN)
                SetAdmin(sptr);

              if (status & HMODE_CODER)
                SetCoder(sptr);

              if (status & HMODE_SERVICESBOT)
                SetServicesBot(sptr);

              if (status & HMODE_HELPOP)
                SetHelpOp(sptr);

              if (status & FLAGS_OPER)
              {
                if (!IsAnOper(sptr))
                  nrof.opers++;
                SetOper(sptr);
                sptr->flags |= (FLAGS_WALLOP | FLAGS_SERVNOTICE | FLAGS_DEBUG);
                set_snomask(sptr, SNO_OPERDEFAULT, SNO_ADD);
                set_privs(sptr);
              }
            }
          }

          if (automodes)
          {
              int addflags, addhmodes;

              mask_user_flags(automodes, &addflags, &addhmodes);
              sptr->flags |= addflags;
              sptr->hmodes |= addhmodes;
          }

          send_umode_out(sptr, sptr, of, oh, IsRegistered(sptr));

        } else {
          /* No registrado */
          sendto_one(sptr,
              ":%s NOTICE %s :*** Tu nick %s acaba de ser registrado y se ha renombrado a otro. Para poner tu nick registrado, utiliza \002/NICK %s:clave\002",
              bot_nickserv ? bot_nickserv : me.name, clave, clave, clave);
          rename_user(sptr, NULL);
        }
      }
      break;
    }

    case BDD_CHANDB:
      crea_canal_persistente(c, v, !cptr);
      break;

    case BDD_FEATURESDB:
      if (!strcmp(c, BDD_NUMERO_MAXIMO_DE_CLONES_POR_DEFECTO))
      {
        numero_maximo_de_clones_por_defecto = atoi(v);
      }
      else if (!strcmp(c, BDD_OCULTAR_SERVIDORES))
      {
        if (!strcasecmp(v, "TRUE"))
          ocultar_servidores = !0;
        else
          ocultar_servidores = 0;
      }
      else if (!strcmp(c, BDD_ACTIVAR_IDENT))
      {
        if (!strcasecmp(v, "TRUE"))
          activar_ident = !0;
        else
          activar_ident = 0;
      }
      else if (!strcmp(c, BDD_SERVER_NAME))
      {
        SlabStringAllocDup(&(his.name), v, HOSTLEN);
      }
      else if (!strcmp(c, BDD_SERVER_INFO))
      {
        SlabStringAllocDup(&(his.info), v, REALLEN);
      }
      else if (!strcmp(c, BDD_AUTOINVISIBLE))
      {
        auto_invisible = !0;
      }
      else if (!strcmp(c, BDD_NICKLEN))
      {
        int x;

        x = atoi(v);
        /* Solo admitimos un minimo de 9 por el RFC1459 y un maximo de
         * NICKLEN que tiene que ser igual en toda la red.
         */
        if (x < 9)
          nicklen = 9;
        else if (x > NICKLEN)
          nicklen = NICKLEN;
        else
          nicklen = x;
      }
      else if(!strcmp(c, BDD_MENSAJE_GLINE))
      {
        SlabStringAllocDup(&mensaje_gline, v, 0);
      }
      else if(!strcmp(c, BDD_NETWORK))
      {
        SlabStringAllocDup(&network, v, 0);
      }
      else if(!strcmp(c, BDD_CANAL_OPERS))
      {
        SlabStringAllocDup(&canal_operadores, v, 0);
      }
      else if(!strcmp(c, BDD_CANAL_DEBUG))
      {
        SlabStringAllocDup(&canal_debug, v, 0);
      }
      else if(!strcmp(c, BDD_CANAL_PRIVSDEBUG))
      {
        SlabStringAllocDup(&canal_privsdebug, v, 0);
      }
      else if (!strcmp(c, BDD_CONVERSION_UTF))
      {
        if (!strcasecmp(v, "TRUE"))
          conversion_utf = !0;
        else
          conversion_utf = 0;
      }
      else if (!strcmp(c, BDD_PERMITE_NICKS_RANDOM))
      {
        if (!strcasecmp(v, "TRUE"))
          permite_nicks_random = !0;
        else
          permite_nicks_random = 0;
      }
      else if (!strcmp(c, BDD_PERMITE_NICKS_SUSPEND))
      {
        if (!strcasecmp(v, "TRUE"))
          permite_nicks_suspend = !0;
        else
          permite_nicks_suspend = 0;
      }
      break;
    case BDD_CONFIGDB:
      if (!strcmp(c, BDD_CLAVE_DE_CIFRADO_DE_IPS))
      {
        char tmp, clave[12 + 1];

        clave_de_cifrado_de_ips = v;  /* ASCII */
        strncpy(clave, clave_de_cifrado_de_ips, 12);
        clave[12] = '\0';
        tmp = clave[6];
        clave[6] = '\0';
        clave_de_cifrado_binaria[0] = base64toint(clave); /* BINARIO */
        clave[6] = tmp;
        clave_de_cifrado_binaria[1] = base64toint(clave + 6); /* BINARIO */
        elimina_cache_ips_virtuales();
      }
      else if(!strncmp(c, "noredirect:", 9) && !strcmp(c+9, me.name))
      {
        desactivar_redireccion_canales=1;
      }
      else if(!strncmp(c, "quit:", 5) && !strcmp(c+5, me.name))
      {
        SlabStringAllocDup(&mensaje_quit_personalizado, v, 0);
      }
      else if(!strncmp(c, "part:", 5) && !strcmp(c+5, me.name))
      {
        SlabStringAllocDup(&mensaje_part_personalizado, v, 0);
      }
      else if(!strncmp(c, "noinvisible:", 12) && !strcmp(c+12, me.name))
      {
        excepcion_invisible = !0;
      }
      break;
    case BDD_IPVIRTUALDB:
      {
        aClient *sptr;

        if ((sptr = FindUser(c)) && IsUser(sptr))
        {
          BorraIpVirtualPerso(sptr);
          SetIpVirtualPersonalizada(sptr);
          if (MyUser(sptr))
            make_vhostperso(sptr, 1);
        }
      }
      break;
  }
}

static void copia_en_malloc(char *buf, int len, char **p)
{
  char *p2;

  p2 = *p;
  if ((p2 != NULL) && (strlen(p2) < len))
  {
    RunFree(p2);
    p2 = NULL;
  }
  if (!p2)
  {
    p2 = RunMalloc(len + 1);    /* El '\0' final */
    *p = p2;
  }
  memcpy(p2, buf, len);
  p2[len] = '\0';
}


struct db_reg *db_buscar_registro(unsigned char tabla, char *clave)
{
  struct db_reg *reg;
  char *clave_init, *clave_end;
  char *valor_init, *valor_end;

  if (!tabla_residente_y_len[tabla])
    return NULL;

  reg = db_busca_db_reg(tabla, clave);
  if (!reg)
    return NULL;

/* Lo que sigue lo sustituye */
  clave_init = reg->clave;
  clave_end = clave_init + strlen(clave_init);
  valor_init = reg->valor;
  valor_end = valor_init + strlen(valor_init);

  copia_en_malloc(clave_init, clave_end - clave_init,
      &db_buf_cache[db_buf_cache_i].clave);
  copia_en_malloc(valor_init, valor_end - valor_init,
      &db_buf_cache[db_buf_cache_i].valor);
  if (++db_buf_cache_i >= DB_BUF_CACHE)
    db_buf_cache_i = 0;

  return reg;
}

/*
** Esta rutina mata el servidor.
** Es casi una copia de m_die().
*/
extern char **myargv;           /* ircd.c */

static void db_die(char *msg, unsigned char que_bdd)
{
  aClient *acptr;
  int i;
  char buf[1024];

  sprintf_irc(buf, "DB '%c' - %s (%s). El daemon muere...", que_bdd, msg,
      DBPATH);
  for (i = 0; i <= highest_fd; i++)
  {
    if (!(acptr = loc_clients[i]))
      continue;
    if (IsUser(acptr))
      sendto_one(acptr, ":%s NOTICE %s :%s", me.name, PunteroACadena(acptr->name), buf);
    else if (IsServer(acptr))
      sendto_one(acptr, ":%s ERROR :%s", me.name, buf);
  }

#if !defined(USE_SYSLOG)
  openlog(myargv[0], LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif

  syslog(LOG_ERR, "%s", buf);

#if !defined(USE_SYSLOG)
  closelog();
#endif

  Debug((DEBUG_ERROR, "%s", buf));

#if defined(__cplusplus)
  s_die(0);
#else
  s_die();
#endif
}

#if defined(BDD_MMAP)
static void db_die_persistent(struct portable_stat *st1,
    struct portable_stat *st2, char *msg, unsigned char que_bdd)
{
  char buf[4096];
  char *separador = "";

  strcpy(buf, msg);
  strcat(buf, " [");

/*
** Puede no detectarse nada si el tipo que comparamos
** es mas grande que un "unsigned long long".
*/
  if (st1->dev != st2->dev)
  {
    sprintf(buf + strlen(buf), "%sDEVICE: %llu<>%llu", separador,
        (unsigned long long)st1->dev, (unsigned long long)st2->dev);
    separador = " ";
  }
  if (st1->ino != st2->ino)
  {
    sprintf(buf + strlen(buf), "%sINODE: %llu<>%llu", separador,
        (unsigned long long)st1->ino, (unsigned long long)st2->ino);
    separador = " ";
  }
  if (st1->size != st2->size)
  {
    sprintf(buf + strlen(buf), "%sSIZE: %llu<>%llu", separador,
        (unsigned long long)st1->size, (unsigned long long)st2->size);
    separador = " ";
  }
  if (st1->mtime != st2->mtime)
  {
    sprintf(buf + strlen(buf), "%sMTIME: %llu<>%llu", separador,
        (unsigned long long)st1->mtime, (unsigned long long)st2->mtime);
    separador = " ";
  }

  strcat(buf, "]");
  db_die(buf, que_bdd);         /* No regresa */
}
#endif

/*
 * leer_db
 *
 * Lee un registro de la base de datos.
 *
 */
static int leer_db(struct tabla_en_memoria *mapeo, char *buf)
{
  int cont = 0;
  char *p = mapeo->posicion + mapeo->len;

  while (mapeo->puntero_r < p)
  {
    *buf = *(mapeo->puntero_r++);
    if (*buf == '\r')
      continue;
    if (*buf++ == '\n')
    {
      *--buf = '\0';
      return cont;
    }
    if (cont++ > 500)
      break;
  }
  *buf = '\0';
  return -1;
}

/*
** Seek por bidivision
*/
static int
seek_db(struct tabla_en_memoria *mapeo, char *buf, unsigned int registro)
{
  char *p, *p2, *plo, *phi;
  unsigned int v;

/*
** Este caso especial es lo bastante 
** comun como para que se tenga en cuenta.
*/
  if (!registro)
  {
    mapeo->puntero_r = mapeo->posicion;
    return leer_db(mapeo, buf);
  }

  plo = mapeo->posicion;
  phi = plo + mapeo->len;
  while (plo != phi)
  {
/*
** p apunta al principio del registro
** p2 apunta al registro siguiente
*/
    p2 = p = plo + (phi - plo) / 2;
    while ((p >= plo) && (*p != '\n'))
      p--;
    if (p < plo)
      p = plo;
    if (*p == '\n')
      p++;
    while ((p2 < phi) && (*p2++ != '\n'));
/*
** Estamos al principio de un registro
*/
    v = atol(p);
    if (v > registro)
      phi = p;
    else if (v < registro)
      plo = p2;
    else
    {
      plo = phi = p;            /* Encontrado */
      break;
    }
  }
  mapeo->puntero_r = plo;
  return leer_db(mapeo, buf);
}

#if defined(BDD_MMAP)
/*
** ATENCION: El multiplicador al final de esta rutina
** me permite variar el n�mero de version del MMAP cache.
*/
static unsigned int mmap_cache_version(void)
{
  unsigned int version;
  int i;

  version = sizeof(struct db_reg) * DB_MAX_TABLA;

  for (i = 0; i < DB_MAX_TABLA; i++)
    version *= 1 + tabla_residente_y_len[i] * (i + 1);

  return version * MMAP_CACHE_VERSION;  /* El multiplicador me permite variar el numero de version del MMAP cache */
}
#endif

/*
** mmap_cache
** Resultado:
**   Si no se puede crear el fichero, termina a saco.
**   0  - El fichero mapeado correctamente, pero su contenido no nos vale.
**   !0 - El fichero mapeado correctamente, y contenido valido.
*/
#if !defined(BDD_MMAP)
static int mmap_cache(void)
{
  static int done;

  assert(!done);
  done = 1;
  persistent_hit = 0;
  return persistent_hit;
}
#else
static int mmap_cache(void)
{
  char path_buf[sizeof(DBPATH) + 1024];
  char hashes_buf[16384];
  int handle, handle2;
  int flag_problemas = 0;
  unsigned int *pos = NULL, *pos2, *p;
  unsigned char *p_char;
  unsigned long len = BDD_MMAP_SIZE * 1024 * 1024;
  unsigned int len2, len_used;
  unsigned int version;
  unsigned int hash[2], v[2], k[2], x[2];
  int i;
  int hlen;
  static int done;
  struct portable_stat st, *st2;

  assert(!done);
  done = 1;

  handle = open(BDD_MMAP_PATH, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

  if (handle == -1)
    db_die("Error al intentar crear el fichero MMAP cache de la BDD (open)",
        '*');

  if (ftruncate(handle, len))
    db_die("Error al intentar crear el fichero MMAP cache de la BDD (truncate)",
        '*');

  pos2 = pos =
      (unsigned int *)mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
      handle, 0);

  if (pos == MAP_FAILED)
    db_die("Error al intentar crear el fichero MMAP cache de la BDD (mmap1)",
        '*');

/*
** Ojo. Esto no es muy heterodoxo en plataformas 64 bits, como ALPHA.
** NO CAMBIAR EL ORDEN.
*/
  hash[0] = *pos2++;
  hash[1] = *pos2++;
  version = *pos2;
/*
** Cambiamos el numero de version y no el HASH, para detectarlo
** rapidamente, sin tener que evaluar un HASH de megabytes.
*/
  *pos2++ ^= 231231231u;
  len2 = *pos2++;
  len_used = *pos2++;

  assert(sizeof(unsigned int) <= 8);
  if (len_used & 7)
    flag_problemas = 1;

  mmap_cache_pos = (void *)(*pos2++);

  hlen = (pos2 - pos) * sizeof(unsigned int);

  if (len2 != len)
    flag_problemas = 1;

  if (version != mmap_cache_version())
    flag_problemas = 1;

  if (flag_problemas)
    mmap_cache_pos = NULL;

#define HASH_FILE_SIZE	390

/*
** El +1 es por el '\0' final.
** El sizeof+1 es para alineamiento de pagina, con la division que hay mas adelante.
*/
  hlen += HASH_FILE_SIZE + 1 + sizeof(unsigned int) - 1;

  if (!flag_problemas)
  {
    sprintf_irc(path_buf, "%s/hashes", DBPATH);
    handle2 = open(path_buf, O_RDONLY);
    if (handle < 0)
    {
      flag_problemas = 1;
    }
    else
    {
      memset(hashes_buf, 0, sizeof(hashes_buf));
      i = read(handle2, hashes_buf, sizeof(hashes_buf) - 16);
      close(handle2);
      if (i != HASH_FILE_SIZE)
      {
        flag_problemas = 1;
      }
      else
      {
        if (strcmp(hashes_buf, (unsigned char *)pos2))
        {
          flag_problemas = 1;
        }
      }
    }
  }
  if (!flag_problemas)
  {
    p = pos + 2;                /* Nos saltamos el HASH inicial */
    pos2 = (unsigned int *)((unsigned char *)pos + len_used);
    k[0] = k[1] = 23;
    x[0] = x[1] = 0;
/*
** La primera iteracion es especial, por el numero de version modificado.
** No hace falta el ntohl, porque si se intenta usar el MMAP
** en otra arquitectura, queremos que falle la verificacion.
*/
    v[0] = (*p++) ^ 231231231u; /* Numero de serie modificado */
    v[1] = *p++;
    tea(v, k, x);

    while (p < pos2)
    {
      v[0] = *p++;
      v[1] = *p++;
      tea(v, k, x);
    }
    if ((x[0] != hash[0]) || (x[1] != hash[1]))
      flag_problemas = 1;
  }

  munmap((void *)pos, len);

  if (flag_problemas)
    mmap_cache_pos = NULL;

  pos = mmap_cache_pos;

/*
** Solo se debe poner en una posicion fija si se indica una posicion...
*/
  mmap_cache_pos =
      mmap(mmap_cache_pos, len, PROT_READ | PROT_WRITE,
      MAP_SHARED | (mmap_cache_pos ? MAP_FIXED : 0), handle, 0);
  if (mmap_cache_pos == MAP_FAILED)
  {
    mmap_cache_pos =
        mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);
    flag_problemas = 1;
    if (mmap_cache_pos == MAP_FAILED)
      db_die("Error al intentar crear el fichero MMAP cache de la BDD (mmap2)",
          '*');
  }

  close(handle);

  if (pos && (pos != mmap_cache_pos))
    flag_problemas = 1;

  pos = mmap_cache_pos;
  pos += hlen / sizeof(unsigned int);

/*
** Debemos leer aunque sea "flag_problemas",
** para dejar el puntero en el sitio.
*/
  for (i = 0; i < DB_MAX_TABLA; i++)
  {
    if (*pos++ != tabla_residente_y_len[i])
    {
      flag_problemas = 1;       /* Hay que seguir leyendo, para dejar el puntero en su sitio */
    }
  }

/*
** Debemos leer aunque sea "flag_problemas",
** para dejar el puntero en el sitio.
*/
  st2 = (struct portable_stat *)persistent_align(pos);
  for (i = 0; i < DB_MAX_TABLA; i++)
  {
    if ((i >= ESNET_BDD) && (i <= ESNET_BDD_END))
    {
      sprintf_irc(path_buf, "%s/tabla.%c", DBPATH, i);
      handle = open(path_buf, O_RDONLY, S_IRUSR | S_IWUSR);
      assert(handle != -1);
      get_stat(handle, &st);
      close(handle);
      memcpy(&tabla_stats[i], &st, sizeof(st));
      if (memcmp(&st, st2, sizeof(st)))
      {
        flag_problemas = 1;     /* Hay que seguir leyendo, para dejar el puntero en su sitio */
      }
    }
    st2++;
  }
  pos = (unsigned int *)st2;

/*
** No hace falta borrarlo todo. De hecho es
** contraproducente, porque si tenemos un fichero cache
** de 100 MB y solo se usan 20, no hay necesidad de
** meter en memoria 100 MB que se van a sobreescribir inmediatamente.
*/
  if (flag_problemas)
    memset(mmap_cache_pos, 0,
        4096 + hlen + sizeof(tabla_residente_y_len) + sizeof(tabla_stats) +
        sizeof(tabla_cuantos) + sizeof(tabla_datos) + sizeof(tabla_serie) +
        sizeof(tabla_hash_hi) + sizeof(tabla_hash_lo));

  p_char = (unsigned char *)pos;
  memcpy(tabla_cuantos, p_char, sizeof(tabla_cuantos));
  p_char += sizeof(tabla_cuantos);
  memcpy(tabla_datos, p_char, sizeof(tabla_datos));
  p_char += sizeof(tabla_datos);
  memcpy(tabla_serie, p_char, sizeof(tabla_serie));
  p_char += sizeof(tabla_serie);
  memcpy(tabla_hash_hi, p_char, sizeof(tabla_hash_hi));
  p_char += sizeof(tabla_hash_hi);
  memcpy(tabla_hash_lo, p_char, sizeof(tabla_hash_lo));
  p_char += sizeof(tabla_hash_lo);

  persistent_init(p_char, len - (p_char - (unsigned char *)mmap_cache_pos) - 64,
      flag_problemas ? NULL : (unsigned char *)mmap_cache_pos + len_used);

  persistent_hit = !flag_problemas;
  return persistent_hit;
}
#endif /* !defined(BDD_MMAP) */

#if defined(BDD_MMAP)
void db_persistent_commit(void)
{
  char path_buf[sizeof(DBPATH) + 1024];
  uintptr_t *p, *p_base, *p_limite;
  unsigned char *p2;
  unsigned int len_used;
  unsigned int v[2], k[2], x[2];
  struct portable_stat st;
  int handle;
  int i;

  if (!mmap_cache_pos)
    return;

  for (i = 0; i < DB_MAX_TABLA; i++)
  {
    if ((i < ESNET_BDD) || (i > ESNET_BDD_END))
      continue;
    sprintf_irc(path_buf, "%s/tabla.%c", DBPATH, i);
    handle = open(path_buf, O_RDONLY, S_IRUSR | S_IWUSR);
    assert(handle != -1);
    get_stat(handle, &st);
    close(handle);
    if (memcmp(&st, &tabla_stats[i], sizeof(st)))
      db_die_persistent(&st, &tabla_stats[i],
          "Se detecta una modificacion no autorizada de la BDD (MMAP_COMMIT)",
          i);
  }

  p_base = (uintptr_t *)mmap_cache_pos;
  p = p_base;
  p2 = (unsigned char *)p;

  p += 2;                       /* Nos saltamos el HASH, de momento */
  p += 1;                       /* Nos saltamos la version, de momento */
  *p++ = BDD_MMAP_SIZE * 1024 * 1024;
  len_used = (unsigned char *)persistent_top() - p2;
  assert(!(len_used & 7));      /* Multiplo de 8 */
  *p++ = len_used;
  *p++ = (unsigned int)p_base;  /* Esto no me gusta mucho para plataformas 64 bits */

  p2 = (unsigned char *)p;

  sprintf_irc(path_buf, "%s/hashes", DBPATH);
  handle = open(path_buf, O_RDONLY);
  if (handle < 0)
    return;
  i = read(handle, p2, 65535);
  close(handle);
  if (i <= 0)
    return;
  p2 +=
      sizeof(unsigned int) * (((i + sizeof(unsigned int) -
      1) / sizeof(unsigned int)));

  memcpy(p2, tabla_residente_y_len, sizeof(tabla_residente_y_len));
  p2 += sizeof(tabla_residente_y_len);
  p2 = persistent_align(p2);
  memcpy(p2, tabla_stats, sizeof(tabla_stats));
  p2 += sizeof(tabla_stats);
  memcpy(p2, tabla_cuantos, sizeof(tabla_cuantos));
  p2 += sizeof(tabla_cuantos);
  memcpy(p2, tabla_datos, sizeof(tabla_datos));
  p2 += sizeof(tabla_datos);
  memcpy(p2, tabla_serie, sizeof(tabla_serie));
  p2 += sizeof(tabla_serie);
  memcpy(p2, tabla_hash_hi, sizeof(tabla_hash_hi));
  p2 += sizeof(tabla_hash_hi);
  memcpy(p2, tabla_hash_lo, sizeof(tabla_hash_lo));
  p2 += sizeof(tabla_hash_lo);

  p = p_base + 2;               /* Nos saltamos el HASH inicial */
  p_limite = p_base + len_used / sizeof(unsigned int);
  k[0] = k[1] = 23;
  x[0] = x[1] = 0;
/*
** La primera iteracion es especial, por el numero de version modificado.
*/
  p++;                          /* Nos saltamos la version de momento */
  v[0] = mmap_cache_version();
  v[1] = *p++;
  tea(v, k, x);

  while (p < p_limite)
  {
    v[0] = *p++;
    v[1] = *p++;
    tea(v, k, x);
  }
  p = p_base;
  *p++ = x[0];
  *p++ = x[1];
  *p++ = mmap_cache_version();
}
#endif /* defined(BDD_MMAP) */

/*
 * abrir_db
 *
 * Se mueve hasta un registro de la base de datos
 *
 */
#if defined(BDD_MMAP)
static int
abrir_db(unsigned int registro, char *buf, unsigned char que_bdd,
    struct tabla_en_memoria *mapeo, struct portable_stat *st)
#else
static int
abrir_db(unsigned int registro, char *buf, unsigned char que_bdd,
    struct tabla_en_memoria *mapeo)
#endif
{
  int handle;
  int cont;
  char path[1024];
  struct portable_stat estado;

  *buf = '\0';
  sprintf_irc(path, "%s/tabla.%c", DBPATH, que_bdd);
  handle = open(path, O_RDONLY, S_IRUSR | S_IWUSR);
  get_stat(handle, &estado);
  mapeo->len = estado.size;

#if defined(BDD_MMAP)
  if (st)
    memcpy(st, &estado, sizeof(estado));
#endif

  mapeo->posicion = mmap(NULL, mapeo->len,
      PROT_READ, MAP_SHARED | MAP_NORESERVE, handle, 0);
  close(handle);
  mapeo->puntero_r = mapeo->puntero_w = mapeo->posicion;

  if (handle == -1)
    db_die("Error al intentar leer (open)", que_bdd);
  if ((mapeo->len != 0) && (mapeo->posicion == MAP_FAILED))
    db_die("Error al intentar leer (mmap)", que_bdd);

  if ((handle == -1) || (mapeo->posicion == MAP_FAILED))
    return -1;

  cont = seek_db(mapeo, buf, registro);
  if (cont == -1)
  {
    *buf = '\0';
    return -1;
  }
  return handle;
}

static void cerrar_db(struct tabla_en_memoria *mapeo)
{
  munmap(mapeo->posicion, mapeo->len);
}

/*
** almacena_hash
**
** El fichero de HASHES contiene una linea
** por BDD. Cada linea tiene el formato
** "BDD HASH\n"
** En total, cada linea mide 15 bytes.
*/
static void almacena_hash(unsigned char que_bdd)
{
  char path[1024];
  char hash[20];
  int db_file;
  
  if(bootopt & BOOT_BDDCHECK)
    return;

  sprintf_irc(path, "%s/hashes", DBPATH);
  inttobase64(hash, tabla_hash_hi[que_bdd], 6);
  inttobase64(hash + 6, tabla_hash_lo[que_bdd], 6);
  db_file = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  if (db_file == -1)
    db_die("Error al intentar guardar hashes (open)", que_bdd);
  sprintf_irc(path, "%c %s\n", que_bdd, hash);
  if (lseek(db_file, 15 * (que_bdd - ESNET_BDD), SEEK_SET) == -1)
    db_die("Error al intentar guardar hashes (lseek)", que_bdd);
  if (write(db_file, path, strlen(path)) == -1)
    db_die("Error al intentar guardas hashes (write)", que_bdd);
  close(db_file);
}

/*
** lee_hash
*/
static void lee_hash(unsigned char que_bdd, unsigned int *hi, unsigned int *lo)
{
  char path[1024];
  char c;
  int db_file;

  sprintf_irc(path, "%s/hashes", DBPATH);
  db_file = open(path, O_RDONLY);
/*
** No metemos verificacion, porque ya verifica
** al contrastar el hash.
*/
  lseek(db_file, 15 * (que_bdd - ESNET_BDD) + 2, SEEK_SET);
  read(db_file, path, 12);
  close(db_file);
  path[12] = '\0';
  c = path[6];
  path[6] = '\0';
  *hi = base64toint(path);
  path[6] = c;
  *lo = base64toint(path + 6);
}

/*
 * db_alta
 *
 * Da de alta una entrada en la base de datos en memoria
 * Formato "serie destino id clave contenido\n"
 *
 * Modificado para usar las hash con funciones db_*
 *                                      1999/06/30 savage@apostols.org
 */
static void db_alta(char *registro, unsigned char que_bdd, aClient *cptr, aClient *sptr)
{
  char *p0, *p1, *p2, *p3, *p4;
  char path[1024];
  int db_file;

  actualiza_hash(registro, que_bdd);

  if (cptr)
  {
    int offset;
#if defined(BDD_MMAP)
    struct portable_stat st;
#endif

    sprintf_irc(path, "%s/tabla.%c", DBPATH, que_bdd);
    db_file = open(path, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    if (db_file == -1)
      db_die("Error al intentar an~adir nuevo registro (open)", que_bdd);

#if defined(BDD_MMAP)
    get_stat(db_file, &st);
    if (memcmp(&st, &tabla_stats[que_bdd], sizeof(st)))
      db_die_persistent(&st, &tabla_stats[que_bdd],
          "Se detecta una modificacion no autorizada de la BDD (ALTA)",
          que_bdd);
#endif

    offset = lseek(db_file, 0, SEEK_CUR);

    if (offset < 0)
      db_die("Error al intentar an~adir nuevo registro (lseek)", que_bdd);

    if (write(db_file, registro, strlen(registro)) == -1)
    {
/*
** Nos interesa que el sistema de Persistencia
** reconozca el cambio en la BDD, para forzar
** una relectura contrastada con su HASH. De esa
** forma detectaremos cualquier corrupcion en la BDD.
**
** 20030721 - jcea@argo.es
*/
      ftruncate(db_file, offset);
      db_die("Error al intentar an~adir nuevo registro (write)", que_bdd);
    }

#if defined(BDD_MMAP)
    get_stat(db_file, &tabla_stats[que_bdd]);
#endif

    close(db_file);

    almacena_hash(que_bdd);
  }

  p0 = strtok(registro, " ");   /* serie */
  p1 = strtok(NULL, " ");       /* destino */
  p2 = strtok(NULL, " ");       /* tabla */
  p3 = strtok(NULL, " \r\n");   /* clave */
  p4 = strtok(NULL, "\r\n");    /* valor (opcional) */

  if (p3 == NULL)
    return;                     /* registro incompleto */

  tabla_serie[que_bdd] = atol(p0);

  if (tabla_residente_y_len[que_bdd])
  {
    if (p4 == NULL)             /* Borrado */
      db_eliminar_registro(que_bdd, p3, 0, cptr, sptr);
    else
      db_insertar_registro(que_bdd, p3, p4, cptr, sptr);
  }
}

/*
** db_pack
**
** Elimina los registro superfluos
** de una Base de Datos Local.
** 
** Se invoca cuando se recibe un CheckPoint, y el
** formato es "serie destino id * texto"
*/
static void db_pack(char *registro, unsigned char que_bdd)
{
  int db_file;
  char path[1024];
  char *map;
  char *lectura, *escritura, *p;
  char *clave, *valor;
  char c;
  unsigned int len, len2;
  struct portable_stat estado;
  struct db_reg *reg;

/*
** El primer valor es el numero de serie actual
*/
  tabla_serie[que_bdd] = atol(registro);

  sprintf_irc(path, "%s/tabla.%c", DBPATH, que_bdd);
  db_file = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  get_stat(db_file, &estado);
  len = estado.size;

#if defined(BDD_MMAP)
  if (memcmp(&estado, &tabla_stats[que_bdd], sizeof(estado)))
    db_die_persistent(&estado, &tabla_stats[que_bdd],
        "Se detecta una modificacion no autorizada de la BDD (COMPACT)",
        que_bdd);
#endif

  map = mmap(NULL, len, PROT_READ | PROT_WRITE,
      MAP_SHARED | MAP_NORESERVE, db_file, 0);
  close(db_file);
  if (db_file == -1)
    db_die("Error al intentar compactar (open)", que_bdd);
  if ((len != 0) && (map == MAP_FAILED))
    db_die("Error al intentar compactar (mmap)", que_bdd);

  if (!tabla_residente_y_len[que_bdd])
  {                             /* No residente -> No pack */
    escritura = map + len;
    goto fin;
  }

  tabla_hash_hi[que_bdd] = 0;
  tabla_hash_lo[que_bdd] = 0;

  p = lectura = escritura = map;
  while (lectura < map + len)
  {
    p = strchr(p, ' ') + 1;     /* Destino */
    p = strchr(p, ' ') + 1;     /* BDD */
    valor = clave = p = strchr(p, ' ') + 1; /* Clave */
    while ((p < map + len) && (*p != '\n') && (*p != '\r'))
      p++;
    while ((*valor != ' ') && (valor < p))
      valor++;
    valor++;                    /* Nos saltamos el espacio */

/*
** Los registros "*" son borrados automaticamente
** al compactar, porque no aparecen en la
** base de datos en memoria.
**
** Solo se mantiene el nuevo, porque es
** el que se graba tras la compactacion
*/
    if (valor < p)
    {                           /* Nuevo registro, no un borrado */
      *(valor - 1) = '\0';
      reg = db_buscar_registro(que_bdd, clave);
      *(valor - 1) = ' ';
      if (reg != NULL)
      {                         /* El registro sigue existiendo */

        len2 = strlen(reg->valor);
        if ((valor + len2 == p) && (!strncmp(valor, reg->valor, len2)))
        {                       /* !!El mismo!! */

/*
** Actualizamos HASH
*/
          if ((*p == '\n') || (*p == '\r'))
          {
            c = *p;
            *p = '\0';
            actualiza_hash(lectura, que_bdd);
            *p = c;
          }
          else
          {                     /* Estamos al final y no hay retorno de carro */
            char temp[1024];

            memcpy(temp, lectura, p - lectura);
            temp[p - lectura] = '\0';
            actualiza_hash(temp, que_bdd);
          }

          while ((p < map + len) && ((*p == '\n') || (*p == '\r')))
            p++;
          memcpy(escritura, lectura, p - lectura);
          escritura += p - lectura;
          lectura = p;
          continue;             /* MUY IMPORTANTE */
        }                       /* Hay otro mas moderno que este */
      }                         /* El registro fue borrado */
    }                           /* Es un borrado */
    while ((p < map + len) && ((*p == '\n') || (*p == '\r')))
      p++;
    lectura = p;
  }

fin:

  munmap(map, len);

  if (truncate(path, escritura - map) == -1)
  {
    db_die("Error al intentar compactar (truncate)", que_bdd);
  }
  db_file = open(path, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
  if (db_file == -1)
  {
    db_die("Error al intentar compactar (open-2)", que_bdd);
  }
  write(db_file, registro, strlen(registro)); /* CheckPoint */

#if defined(BDD_MMAP)
  get_stat(db_file, &tabla_stats[que_bdd]);
#endif

  close(db_file);
  actualiza_hash(registro, que_bdd);
  almacena_hash(que_bdd);
}

/*
 * borrar_db
 *
 * Borra la base de datos en memoria (modificado para uso de hash)
 *
 *                                      1999/06/30 savage@apostols.org
 */
static void borrar_db(unsigned char que_bdd)
{
  int i, n;
  struct db_reg *reg, *reg2;

  tabla_serie[que_bdd] = 0;
  tabla_cuantos[que_bdd] = 0;
  tabla_hash_hi[que_bdd] = 0;
  tabla_hash_lo[que_bdd] = 0;

  n = tabla_residente_y_len[que_bdd];
  if (!n)
    return;

  if (tabla_datos[que_bdd])
  {
    for (i = 0; i < n; i++)
    {
      for (reg = tabla_datos[que_bdd][i]; reg != NULL; reg = reg2)
      {
        reg2 = reg->next;
        p_free(reg);
      }
    }
  }
  else
  {                             /* NO tenemos memoria para esa tabla, asi que la pedimos */
    tabla_datos[que_bdd] = p_malloc(n * sizeof(struct db_reg *));
    assert(tabla_datos[que_bdd]);
  }

  for (i = 0; i < n; i++)
  {
    tabla_datos[que_bdd][i] = NULL;
  }
}

static void corta_si_multiples_hubs(aClient *cptr, unsigned char que_bdd,
    char *mensaje)
{
  char buf[1024];
  Dlink *lp;
  int num_hubs = 0;
  aClient *acptr;

  for (lp = me.serv->down; lp; lp = lp->next)
  {
    if (find_conf_host(lp->value.cptr->confs,
        lp->value.cptr->name, CONF_HUB) != NULL)
      num_hubs++;
  }

  if (num_hubs >= 2)
  {

/*
** No podemos simplemente hace el bucle, porque
** el "exit_client()" modifica la estructura
*/
  corta:
    if (num_hubs-- > 1)
    {                           /* Deja un HUB conectado */
      for (lp = me.serv->down; lp; lp = lp->next)
      {
        acptr = lp->value.cptr;
/*
** Si se especifica que se desea mantener un HUB
** en concreto, respeta esa peticion
*/
        if ((acptr != cptr) &&
            find_conf_host(acptr->confs, acptr->name, CONF_HUB) != NULL)
        {
          sprintf_irc(buf, "BDD '%c' %s. Resincronizando...", que_bdd, mensaje);
          exit_client(acptr, acptr, &me, buf);
          goto corta;
        }
      }
    }
  }
}

/*
 * initdb2
 *
 * Lee la base de datos de disco
 *
 */
static void initdb2(unsigned char que_bdd)
{
  unsigned int hi, lo;
  char buf[1024];
  char path[1024];
  int res;
  int fd;
  Dlink *lp;
  struct tabla_en_memoria mapeo;
  char *destino, *p = NULL, *p2, *p3;
  int p_len = 0;
  char str[13];

  borrar_db(que_bdd);

#if defined(BDD_MMAP)
  res = abrir_db(0, buf, que_bdd, &mapeo, &tabla_stats[que_bdd]);
#else
  res = abrir_db(0, buf, que_bdd, &mapeo);
#endif

  if (res != -1)
    do
    {
      /* db_alta modifica la cadena */
      destino = strchr(buf, ' ') + 1;
      p3 = strchr(destino, ' ') + 1;
      p2 = strchr(p3, ' ') + 1;
/*
** Curemonos en salud
*/
      if ((*(p3 + 1) != ' ') || ((*p3 != que_bdd) && !((*p3 == 'N')
          && (que_bdd == 'n'))))
      {
/*
** HACK para que la verificacion HASH falle
** y asi purgar la base de datos defectuosa.
** Los valores usados son arbitrarios.
*/
        tabla_hash_hi[que_bdd] += 57;
        tabla_hash_lo[que_bdd] += 97;
      }
      if ((*destino == '*') && (*(destino + 1) == ' '))
      {
/*
** Este caso es tan normal
** que lo optimizamos.
*/
        destino = NULL;
      }
      else
      {
        if (strlen(destino) + 1 > p_len)
        {
          p_len = strlen(destino) + 1;
          if (p)
            RunFree(p);
          p = RunMalloc(p_len);
        }
        strcpy(p, destino);
        *strchr(p, ' ') = '\0';
        collapse(p);
        if (!match(p, me.name))
          destino = NULL;       /* Para nosotros */
      }

      if (tabla_residente_y_len[que_bdd] && (destino == NULL) && !((*p2 == '*')
          && (*(p2 + 1) == '\0')))
      {
        db_alta(buf, que_bdd, NULL, NULL);
      }
      else
      {
/*
** Necesitamos coger su numero de serie
** para poder determinar si los
** registros que nos lleguen por la red
** hay que guardarlos en disco, ademas
** de poder negociar convenientemente
** las versiones de las BDD
** en un NetJoin.
*/
        tabla_serie[que_bdd] = atol(buf);
        actualiza_hash(buf, que_bdd);
      }
    }
    while (leer_db(&mapeo, buf) != -1);

  cerrar_db(&mapeo);

  if (p)
    RunFree(p);

/*
** Ahora comprueba que el HASH de la BDD
** cargada se corresponda con el HASH almacenado
*/
  lee_hash(que_bdd, &hi, &lo);

  if(bootopt & BOOT_BDDCHECK)
  {
    inttobase64(str, tabla_hash_hi[que_bdd], 6);
    inttobase64(str + 6, tabla_hash_lo[que_bdd], 6);
    str[12]='\0';
    fprintf(stderr, "%c %s\n",que_bdd,str);
  }
  else if ((tabla_hash_hi[que_bdd] != hi) || (tabla_hash_lo[que_bdd] != lo))
  {
    sendto_ops("ATENCION - Base de Datos "
        "'%c' aparentemente corrupta. Borrando...", que_bdd);
    borrar_db(que_bdd);
    sprintf_irc(path, "%s/tabla.%c", DBPATH, que_bdd);
    fd = open(path, O_TRUNC, S_IRUSR | S_IWUSR);

#if defined(BDD_MMAP)
    get_stat(fd, &tabla_stats[que_bdd]);
    persistent_hit = 0;
#endif

    if (fd == -1)
      db_die("Error al intentar truncar (open)", que_bdd);
    close(fd);
/*
** Solucion temporal
** Corta conexiones con los HUBs
*/
  corta_hubs:
    for (lp = me.serv->down; lp; lp = lp->next)
    {
      if (find_conf_host(lp->value.cptr->confs,
          lp->value.cptr->name, CONF_HUB) != NULL)
      {
        sprintf_irc(buf, "BDD '%c' inconsistente. Resincronizando...", que_bdd);
        exit_client(lp->value.cptr, lp->value.cptr, &me, buf);
#if 0
        /* fix patch.db52 */
        /* lp ya no es valido al desconectar */
        lp = me.serv->down;
        if (!lp)
          break;
#endif
        /* fix patch.db74 */
        goto corta_hubs;
      }
    }
/*
** Fin solucion temporal
*/

    corta_si_multiples_hubs(NULL, que_bdd, "inconsistente");
    sendto_ops("Solicitando actualizacion BDD '%c' "
        "a los nodos vecinos.", que_bdd);
/*
** Solo pide a los HUBs, porque no se
** aceptan datos de leafs.
*/
    for (lp = me.serv->down; lp; lp = lp->next)
    {
      if (find_conf_host(lp->value.cptr->confs,
          lp->value.cptr->name, CONF_HUB) != NULL)
      {
        sendto_one(lp->value.cptr, "%s DB %s 0 J %u %c",
            NumServ(&me), lp->value.cptr->name, tabla_serie[que_bdd], que_bdd);
      }
    }
  }
  almacena_hash(que_bdd);

/*
** Si hemos leido algun registro de compactado,
** sencillamente nos lo cargamos en memoria, para
** que cuando llegue otro, el antiguo se borre
** aunque tenga algun contenido (un texto explicativo, por ejemplo).
** Parche DB70
*/

  if (tabla_residente_y_len[que_bdd])
    db_eliminar_registro(que_bdd, "*", 0, NULL, NULL);
}

void initdb(void)
{
  int cache;
  char c;
#if defined(BDD_MMAP)
  int i;
  struct db_reg *reg;
#endif

  if (bdd_initialized == 0)
    bdd_init();

  memset(tabla_residente_y_len, 0, sizeof(tabla_residente_y_len));

/*
** Las longitudes DEBEN ser potencias de 2,
** y no deben ser superiores a HASHSIZE, ya que ello
** solo desperdiciaria memoria.
*/
  tabla_residente_y_len[ESNET_NICKDB] = 32768;
#if defined(BDD_CLONES)
  tabla_residente_y_len[ESNET_CLONESDB] = 512;
#endif
  tabla_residente_y_len[BDD_OPERDB] = 256;
  tabla_residente_y_len[BDD_CHANDB] = 2048;
  tabla_residente_y_len[BDD_CHAN2DB] = 8192;
  tabla_residente_y_len[BDD_BOTSDB] = 256;
  tabla_residente_y_len[BDD_FEATURESDB] = 256;
  tabla_residente_y_len[BDD_JUPEDB] = 512;
  tabla_residente_y_len[BDD_LOGGINGDB] = 256;
  tabla_residente_y_len[BDD_MOTDDB] = 256;
  tabla_residente_y_len[BDD_PSEUDODB] = 256;
  tabla_residente_y_len[BDD_QUARANTINEDB] = 256;
  tabla_residente_y_len[BDD_UWORLDDB] = 256;
  tabla_residente_y_len[BDD_IPVIRTUALDB] = 4096;
  tabla_residente_y_len[BDD_WEBIRCDB] = 256;
  tabla_residente_y_len[BDD_CONFIGDB] = 256;
  tabla_residente_y_len[BDD_EXCEPTIONDB] = 512;
  tabla_residente_y_len[BDD_CHANREDIRECTDB] = 256;
  tabla_residente_y_len[BDD_PROXYDB] = 256;
  
  cache = mmap_cache();

  if (!cache)
  {
    for (c = ESNET_BDD; c <= ESNET_BDD_END; c++)
      initdb2(c);
  }
  else
  {
/*
** CACHE HIT
**
** Aqui hacemos las operaciones que causan
** cambios de estado internos. Es decir, los
** registros que por existir modifican
** estructuras internas. El caso evidente
** son los canales persistentes.
*/

#if defined(BDD_MMAP)
    i = tabla_residente_y_len[BDD_CHANDB];
    assert(i);
    for (i--; i >= 0; i--)
    {
      for (reg = tabla_datos[BDD_CHANDB][i]; reg != NULL; reg = reg->next)
      {
        crea_canal_persistente(reg->clave, reg->valor, 1);
      }
    }
    if ((reg =
        db_buscar_registro(BDD_CONFIGDB,
        BDD_NUMERO_MAXIMO_DE_CLONES_POR_DEFECTO)))
    {
      numero_maximo_de_clones_por_defecto = atoi(reg->valor);
    }
    if ((reg = db_buscar_registro(BDD_CONFIGDB, BDD_CLAVE_DE_CIFRADO_DE_IPS)))
    {
      char tmp, clave[12 + 1];

      clave_de_cifrado_de_ips = reg->valor;
      strncpy(clave, clave_de_cifrado_de_ips, 12);
      clave[12] = '\0';
      tmp = clave[6];
      clave[6] = '\0';
      clave_de_cifrado_binaria[0] = base64toint(clave); /* BINARIO */
      clave[6] = tmp;
      clave_de_cifrado_binaria[1] = base64toint(clave + 6); /* BINARIO */

    }
#endif /* CACHE HIT */
  }

/*
** La operacion anterior puede ser una operacion larga.
** Resincronizamos tiempo.
*/
  now = time(NULL);
}

/**
 * bdd_init
 *
 * Initializes bdd files if needed
 */
static void bdd_init()
{
  char path[PATH_MAX];
  char msg[1024];

  struct stat check;

  /* Database directory check */
  if (stat(DBPATH, &check) == -1) {
    switch (errno) {
      /* Database directory does not exists, lets create it: */
      case ENOENT: {
        int f = mkdir(DBPATH, 0700);

        if (f == -1) {
          sprintf(msg, "Error initializing datatabase directory %s: %s",
                  DBPATH, strerror(errno));

          syslog(LOG_ERR, "%s", msg);
          Debug((DEBUG_ERROR, "%s", msg));
          fprintf(stderr, "%s\n", msg);
          s_die();
        }

        sprintf(msg, "Initialized database directory: %s", DBPATH);
        syslog(LOG_INFO, "%s", msg);
        Debug((DEBUG_INFO, "%s", msg));
        close(f);
        break;
      }

      /* Cannot read database directory, this ends the ircd execution */
      case EACCES: {
        sprintf(msg, "Cannot read database directory: %s", DBPATH);

        syslog(LOG_ERR, "%s", msg);
        Debug((DEBUG_ERROR, "%s", msg));
        fprintf(stderr, "%s\n", msg);
        s_die();
      }

    }
  }

  for (char current = ESNET_BDD; current <= ESNET_BDD_END; current++) {
    sprintf_irc(path, "%s/tabla.%c", DBPATH, current);
    int stat_status = stat(path, &check);

     /* Lets perform some checks */
    if (stat_status == -1) {
      switch (errno) {
      /* Database file does not exists, lets create it: */
      	case ENOENT: {
          int f = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

          if (f == -1) {
            sprintf(msg, "Error initializing datatabase file %s: %s",
                    path, strerror(errno));

            syslog(LOG_ERR, "%s", msg);
      	    Debug((DEBUG_ERROR, "%s", msg));
            fprintf(stderr, "%s\n", msg);
            s_die();
          }

  	  sprintf(msg, "Initialized database file: %s", path);
          syslog(LOG_INFO, "%s", msg);
          Debug((DEBUG_INFO, "%s", msg));
          close(f);
       	  break;
        }

       	/* Cannot read database file, this ends the ircd execution */
      	case EACCES: {
          sprintf(msg, "Cannot read database file: %s", path);

          syslog(LOG_ERR, "%s", msg);
          Debug((DEBUG_ERROR, "%s", msg));
          fprintf(stderr, "%s\n", msg);
          s_die();
      	}
      }
    }
  }

  bdd_initialized = 1;
}

/*
 * reload_db
 *
 * Recarga la base de datos de disco, liberando la memoria
 *
 */
void reload_db(void)
{
  char buf[16];
  unsigned char c;
  sendto_ops("Releyendo Bases de Datos...");
  initdb();
  for (c = ESNET_BDD; c <= ESNET_BDD_END; c++)
  {
    if (tabla_serie[c])
    {
      inttobase64(buf, tabla_hash_hi[c], 6);
      inttobase64(buf + 6, tabla_hash_lo[c], 6);
      sendto_ops("DB: '%c'. Ultimo registro: %u. HASH: %s",
          c, tabla_serie[c], buf);
    }
  }
}

void tx_num_serie_dbs(aClient *cptr)
{
  int cont;

/*
** Informamos del estado de nuestras BDD
*/
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
  inicia_microburst();
#endif

/* La tabla 'n' es un poco especial... */
  sendto_one(cptr, "%s DB * 0 J %u 2", NumServ(&me),
      db_num_serie(ESNET_NICKDB));

  for (cont = ESNET_BDD; cont <= ESNET_BDD_END; cont++)
  {
    if (cont != ESNET_NICKDB)   /* No mandamos nicks de nuevo */
      sendto_one(cptr, "%s DB * 0 J %u %c",
          NumServ(&me), tabla_serie[cont], cont);
  }

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
  completa_microburst();
#endif
}


/*
 * m_db
 *
 * Gestion de la base de datos - ESNET
 * 29/May/98 jcea@argo.es
 *
 */
int m_db(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  unsigned int db;
  aClient *acptr;
  Dlink *lp;
  char db_buf[1024];
  char path[1024];
  int db_file;
  int res;
  int es_hub = 0;
  char *p, *p2, *p3, *p4;
  unsigned char que_bdd = ESNET_NICKDB;
  unsigned int mascara_bdd = 0;
  int cont;
#if defined(BDD_MMAP)
  struct portable_stat st;
#endif
  struct tabla_en_memoria mapeo;

  if (!IsServer(sptr) || parc < 5)
    return 0;
  db = atoi(parv[2]);
  if (find_conf_host(cptr->confs, cptr->name, CONF_HUB) != NULL)
    es_hub = !0;
  if (!db)
  {
    db = atol(parv[4]);
    if (parc == 6)
    {
      que_bdd = *parv[5];
      if ((que_bdd < 'a') || (que_bdd > 'z'))
      {
        if ((que_bdd == '*') && ((*parv[3] == 'Q') || (*parv[3] == 'R')))
          que_bdd = '*';        /* Estamos preguntando por un HASH global de todas las tablas */
        else if ((que_bdd < '2') || (que_bdd > '9') || (*parv[3] != 'J'))
          return 0;
        else
          que_bdd = ESNET_NICKDB;
      }
    }

/*
** Hay CPUs (en particular, viejos INTEL) que
** si le dices 1372309 rotaciones, por ejemplo,
** pues te las hace ;-), con lo que ello supone de
** consumo de recursos. Lo normal es que
** la CPU haga la rotacion un numero de veces
** modulo 32 o 64, pero hay que curarse en salud.
*/
    if ((que_bdd >= ESNET_BDD) && (que_bdd <= ESNET_BDD_END))
      mascara_bdd = ((unsigned int)1) << (que_bdd - ESNET_BDD);

    switch (*parv[3])
    {
      case 'B':
        if (es_hub)
          sendto_one(sptr, "%s DB %s 0 J %u %c",
              NumServ(&me), parv[0], tabla_serie[que_bdd], que_bdd);
        return 0;
        break;
      case 'J':
/*
** No hay problemas de CPU en el
** extremo receptor. Esto es para
** limitar el LAG y el caudal
** consumido.
*/
        cont = 1000;
        if (db >= tabla_serie[que_bdd])
        {                       /* Se le pueden mandar registros individuales */
          sptr->serv->esnet_db |= mascara_bdd;
          return 0;
          break;
        }
        else if ((sptr->serv->esnet_db) & mascara_bdd)
        {
/*
** Teniamos el grifo abierto, y ahora
** nos esta pidiendo registros antiguos.
** Eso SOLO ocurre si ha detectado que su
** copia local de la BDD esta corrupta.
*/
          sptr->serv->esnet_db &= ~mascara_bdd;
/*
** Borramos su BDD porque es posible
** que le hayamos enviado mas registros.
** Es preferible empezar desde cero.
*/
          sendto_one(sptr, "%s DB %s 0 D BDD_CORRUPTA %c",
              NumServ(&me), sptr->name, que_bdd);
#if 0
          sendto_one(sptr, "%s DB %s 0 B %u %c",
              NumServ(&me), sptr->name, tabla_serie[que_bdd], que_bdd);
#endif
          /*
           ** No enviamos nada porque ya nos lo pedira el
           ** con un "J"
           */
          return 0;
        }

#if defined(BDD_MMAP)
        res = abrir_db(db + 1, db_buf, que_bdd, &mapeo, &st);
#else
        res = abrir_db(db + 1, db_buf, que_bdd, &mapeo);
#endif
        if (res != -1)
        {
#if defined(BDD_MMAP)
          if (memcmp(&st, &tabla_stats[que_bdd], sizeof(st)))
            db_die_persistent(&st, &tabla_stats[que_bdd],
                "Se detecta una modificacion no autorizada de la BDD (BURST)",
                que_bdd);
#endif
        }
        else
        {
          db_die("Problemas intentando enviar BURST de la BDD", que_bdd);
        }
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
        inicia_microburst();
#endif
        do
        {
          p = strchr(db_buf, ' ');
          if (p == NULL)
            continue;
          *p++ = '\0';
          p2 = strchr(p, ' ');
          if (p2 == NULL)
            continue;
          *p2++ = '\0';
          p3 = strchr(p2, ' ');
          if (p3 == NULL)
            continue;
          *p3++ = '\0';
          p4 = strchr(p3, ' ');
          if (p4 == NULL)
          {
            sendto_one(sptr, "%s DB %s %s %s %s", NumServ(&me), p,
                db_buf, p2, p3);
          }
          else
          {
            *p4++ = '\0';
            sendto_one(sptr, "%s DB %s %s %s %s :%s", NumServ(&me), p,
                db_buf, p2, p3, p4);
          }
          if (!(--cont))
            break;
        }
        while (leer_db(&mapeo, db_buf) != -1);
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
        completa_microburst();
#endif
        cerrar_db(&mapeo);
        if (cont)
          sptr->serv->esnet_db |= mascara_bdd;
        else
          sendto_one(sptr, "%s DB %s 0 B %u %c",
              NumServ(&me), parv[0], tabla_serie[que_bdd], que_bdd);
        return 0;
        break;
      case 'D':
        if (!es_hub)
          return 0;

/*
** Debemos enviar el broadcast ANTES de
** cortar los enlaces.
*/
        sprintf(sendbuf, "%s DB %s 0 D %s %c",
            NumServ(sptr), parv[1], parv[4], que_bdd);
        for (lp = me.serv->down; lp; lp = lp->next)
        {
          if (lp->value.cptr == cptr)
            continue;
/*
** No sabemos si la otra punta
** va a cumplir la mascara, asi que
** nos curamos en salud.
*/
          lp->value.cptr->serv->esnet_db &= ~mascara_bdd;
/*
** No podemos usar directamente 'sendto_one'
** por el riesgo de que existan metacaracteres tipo '%'.
*/
          sendbufto_one(lp->value.cptr);
        }

/*
** Por bug en lastNNServer no se puede usar 'find_match_server()'
*/
        collapse(parv[1]);
        if (!match(parv[1], me.name))
        {
          sprintf_irc(path, "%s/tabla.%c", DBPATH, que_bdd);
          db_file = open(path, O_TRUNC, S_IRUSR | S_IWUSR);
          if (db_file == -1)
          {
            db_die("Error al intentar truncar (open)", que_bdd);
          }
          else
          {
#if defined(BDD_MMAP)
            get_stat(db_file, &tabla_stats[que_bdd]);
#endif
          }
          close(db_file);
          borrar_db(que_bdd);
          almacena_hash(que_bdd);
          sprintf_irc(db_buf, "borrada (%s)", sptr->name);
          corta_si_multiples_hubs(cptr, que_bdd, db_buf);
/*
** Podemos enviar a 'sptr' tranquilos, porque el
** corte afectaria a todos los HUBs menos a
** traves del que llega la peticion.
*/
          sendto_one(sptr, "%s DB %s 0 E %s %c",
              NumServ(&me), sptr->name, parv[4], que_bdd);
        }

/*
** Como no sabemos si el otro extremo nos ha
** cerrado el grifo o no, nos curamos en salud
*/
        sendto_one(cptr, "%s DB %s 0 J %u %c",
            NumServ(&me), cptr->name, tabla_serie[que_bdd], que_bdd);

        return 0;
        break;
      case 'E':                /* Podemos ser destinatarios si el otro estaba corrupto */
      case 'R':
        if ((acptr = find_match_server(parv[1])) && (!IsMe(acptr)))
          sendto_one(acptr, "%s DB %s 0 %c %s %c", NumServ(sptr),
              acptr->name, *parv[3], parv[4], que_bdd);
        return 0;
        break;
      case 'Q':
        if (!es_hub)
          return 0;
/*
** Por bug en lastNNServer no se puede usar 'find_match_server()'
*/
        collapse(parv[1]);
        if (!match(parv[1], me.name))
        {
          if (que_bdd == '*')
          {                     /* Estamos preguntando por un HASH global de todas las tablas */
            unsigned int hashes_hi = 0;
            unsigned int hashes_lo = 0;
            unsigned int tablas_series = 1;
            unsigned int tablas_cuantos = 1;
            int i;

            for (i = ESNET_BDD; i <= ESNET_BDD_END; i++)
            {
              hashes_hi ^= tabla_hash_hi[i];
              hashes_lo ^= tabla_hash_lo[i];
/*
** Lo que sigue da valores distintos si la
** maquina no es "complemento a dos", pero eso
** no es un problema en la red actual.
**
** El AND es para que salgan resultados
** consistentes tanto en maquinas 32 bits como 64 bits.
*/
              tablas_series = (tablas_series * (tabla_serie[i] + 1)) & 0xfffffffful;  /* Por si el valor es cero */
              tablas_cuantos = (tablas_cuantos * (tabla_cuantos[i] + 1)) & 0xfffffffful;  /* Por si el valor es cero */
            }
            inttobase64(db_buf, hashes_hi, 6);
            inttobase64(db_buf + 6, hashes_lo, 6);
            sendto_one(sptr, "%s DB %s 0 R %u-%u-%s-%s %c",
                NumServ(&me), sptr->name, tablas_series,
                tablas_cuantos, db_buf, parv[4], que_bdd);
          }
          else
          {                     /* Una tabla en particular */
            inttobase64(db_buf, tabla_hash_hi[que_bdd], 6);
            inttobase64(db_buf + 6, tabla_hash_lo[que_bdd], 6);
            sendto_one(sptr, "%s DB %s 0 R %u-%u-%s-%s %c",
                NumServ(&me), sptr->name, tabla_serie[que_bdd],
                tabla_cuantos[que_bdd], db_buf, parv[4], que_bdd);
          }
        }

        sprintf(sendbuf, "%s DB %s 0 Q %s %c",
            NumServ(sptr), parv[1], parv[4], que_bdd);
        for (lp = me.serv->down; lp; lp = lp->next)
        {
          if (lp->value.cptr == cptr)
            continue;
/*        
** No podemos usar directamente 'sendto_one'
** por el riesgo de que existan metacaracteres tipo '%'.
*/
          sendbufto_one(lp->value.cptr);
        }

        return 0;
        break;
    }
    return 0;
  }

/* Nuevo registro */

  que_bdd = *parv[3];
/*
** Solo se admite un caracter
*/
  if (*(parv[3] + 1) != '\0')
    return 0;

  if ((que_bdd < 'a') || (que_bdd > 'z'))
  {
    if (que_bdd == 'N')
      que_bdd = ESNET_NICKDB;
    else
      return 0;
  }

  mascara_bdd = ((unsigned int)1) << (que_bdd - ESNET_BDD);
  if (db <= tabla_serie[que_bdd])
    return 0;
  if (!es_hub)
    return 0;
/*
** Ojo, hay que usar 'db' y no 'parv[2]'
** porque en la cadena de formateo se
** dice que se le pasa un numero, no
** una cadena.
** Se le pasa un numero para que se formatee
** correctamente, ya que luego sera
** utilizado en un HASH y todos los
** nodos deberian dar el mismo valor.
*/
  if (parc == 5)
    sprintf_irc(sendbuf, "%s DB %s %u %s %s",
        NumServ(sptr), parv[1], db, parv[3], parv[4]);
  else
    sprintf_irc(sendbuf, "%s DB %s %u %s %s :%s",
        NumServ(sptr), parv[1], db, parv[3], parv[4], parv[5]);
  for (lp = me.serv->down; lp; lp = lp->next)
  {
    if (!((lp->value.cptr->serv->esnet_db) & mascara_bdd) ||
        (lp->value.cptr == cptr))
      continue;
/*        
** No podemos usar directamente 'sendto_one'
** por el riesgo de que existan metacaracteres tipo '%'.
*/
    sendbufto_one(lp->value.cptr);
  }

  if (parc == 5)
    sprintf_irc(db_buf, "%u %s %s %s\n", db, parv[1], parv[3], parv[4]);
  else
    sprintf_irc(db_buf, "%u %s %s %s %s\n", db, parv[1], parv[3],
        parv[4], parv[5]);
  if (strcmp(parv[4], "*"))
  {
    db_alta(db_buf, que_bdd, cptr, sptr);
  }
  else
  {                             /* Checkpoint */
    db_pack(db_buf, que_bdd);
  }
  return 0;
}

/*
 * m_dbq
 *
 *   parv[0]   = prefijo del enviador
 *   parv[1..] = argumentos de DBQ [<server>] <tabla> <clave>
 *
 * Maneja los mensajes DBQ recibidos de users y servers.
 *                                      1999/10/13 savage@apostols.org
 */
int m_dbq(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  unsigned char tabla;
  char *clave, *servidor;
  aClient *acptr;
  struct db_reg *reg;
  int nivel_helper = 0;
  static char *cn = NULL;
  static int cl = 0;
  int i;

  if (!IsUser(sptr))
    return 0;                   /* Ignoramos peticiones de nodos */

  if (!IsOper(sptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;                   /* No autorizado */
  }
  /* <Origen> DBQ [<server>] <Tabla> <Clave> */
  if ((parc != 3 && parc != 4) ||
      (parc == 3 && (parv[1][0] == '\0' || parv[1][1] != '\0')) ||
      (parc == 4 && (parv[2][0] == '\0' || parv[2][1] != '\0')))
  {

    if (!IsServer(sptr))
      sendto_one(cptr,
          ":%s NOTICE %s :Parametros incorrectos: Formato: DBQ [<server>] <Tabla> <Clave>",
          me.name, parv[0]);
    return 0;
  }

  if (parc == 3)
  {
    servidor = NULL;            /* no nos indican server */
    tabla = *parv[1];
    clave = parv[2];
  }
  else
  {
    servidor = parv[1];
    tabla = *parv[2];
    clave = parv[3];
    if (*servidor == '*')
    {
      /* WOOW, BROADCAST */
#if !defined(NO_PROTOCOL9)
      sendto_lowprot_butone(cptr, 9, ":%s DBQ * %c %s", parv[0], tabla, clave);
#endif
      sendto_highprot_butone(cptr, 10, "%s%s " TOK_DBQ " * %c %s", NumNick(sptr), tabla, clave);
    }
    else
    {
      /* NOT BROADCAST */
      if (!(acptr = find_match_server(servidor)))
      {
        /* joer, el server de destino no existe */
        sendto_one(cptr, err_str(ERR_NOSUCHSERVER), me.name, parv[0], servidor);
        return 0;
      }

      if (!IsMe(acptr))         /* no es para mi, a rutar */
      {
#if !defined(NO_PROTOCOL9)
        if (Protocol(acptr->from) < 10)
          sendto_one(acptr, ":%s DBQ %s %c %s", parv[0], servidor, tabla, clave);
        else
#endif
          sendto_one(acptr, "%s " TOK_DBQ " %s %c %s", NumServ(acptr), servidor, tabla, clave);
        return 0;               /* ok, rutado, fin del trabajo */
      }
    }
  }

  i = strlen(clave) + 1;
  if (i > cl)
  {
    cl = i;
    if (cn)
      RunFree(cn);
    cn = RunMalloc(cl);
    assert(cn);
  }

  strcpy(cn, clave);
  i = 0;
  while (cn[i] != 0)
  {
    cn[i] = toLower(cn[i]);
    i++;
  }

  if (!tabla_residente_y_len[tabla])
  {
    if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
        || Protocol(cptr) < 10
#endif
    )
      sendto_one(cptr,
          ":%s NOTICE %s :DBQ ERROR Tabla='%c' Clave='%s' TABLA_NO_RESIDENTE",
          me.name, parv[0], tabla, cn);
    else
      sendto_one(cptr,
          "%s " TOK_NOTICE " %s%s :DBQ ERROR Tabla='%c' Clave='%s' TABLA_NO_RESIDENTE",
          NumServ(&me), NumNick(sptr), tabla, cn);
    return 0;
  }

/*
** Limitamos el acceso a ciertas tablas y ciertos registros
*/
  if (!IsServer(sptr))
  {
    int status = get_status(sptr);

    if (status) {
      if (status & (FLAGS_OPER|HMODE_ADMIN|HMODE_CODER))
        nivel_helper = 10;
      else if (status & HMODE_HELPOP)
        nivel_helper = 5;
      else
        nivel_helper = -1;
    } else {
        nivel_helper = -1;
    }
  }

  switch (tabla)
  {
    case ESNET_NICKDB:         /* 'n' */
      if (nivel_helper < 10)
        nivel_helper = -1;
      break;
    case BDD_WEBIRCDB:         /* 'w' */
    case BDD_PROXYDB:
      if (nivel_helper < 10)
        nivel_helper = -1;
      break;
    case BDD_CONFIGDB:         /* 'z' */
      if (!strcmp(clave, BDD_CLAVE_DE_CIFRADO_DE_IPS) && (nivel_helper < 10))
        nivel_helper = -1;
      break;
  }

  if (nivel_helper < 0)
  {
    if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
        || Protocol(cptr) < 10
#endif
    )
      sendto_one(cptr,
          ":%s NOTICE %s :DBQ ERROR No tienes permiso para acceder a Tabla='%c' Clave='%s'",
          me.name, parv[0], tabla, cn);
    else
      sendto_one(cptr,
          "%s " TOK_NOTICE " %s%s :DBQ ERROR No tienes permiso para acceder a Tabla='%c' Clave='%s'",
          NumServ(&me), NumNick(sptr), tabla, cn);
    return 0;
  }

  reg = db_buscar_registro(tabla, clave);
  if (!reg)
  {
    if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
        || Protocol(cptr) < 10
#endif
    )
      sendto_one(cptr,
          ":%s NOTICE %s :DBQ ERROR Tabla='%c' Clave='%s' REGISTRO_NO_ENCONTRADO",
          me.name, parv[0], tabla, cn);
    else
      sendto_one(cptr,
          "%s " TOK_NOTICE " %s%s :DBQ ERROR Tabla='%c' Clave='%s' REGISTRO_NO_ENCONTRADO",
          NumServ(&me), NumNick(sptr), tabla, cn);
    return 0;
  }


  if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
      || Protocol(cptr) < 10
#endif
  )
    sendto_one(cptr,
        ":%s NOTICE %s :DBQ OK Tabla='%c' Clave='%s' Valor='%s'",
        me.name, parv[0], tabla, reg->clave, reg->valor);
  else
    sendto_one(cptr,
        "%s " TOK_NOTICE " %s%s :DBQ OK Tabla='%c' Clave='%s' Valor='%s'",
        NumServ(&me), NumNick(sptr), tabla, reg->clave, reg->valor);
  return 0;
}
