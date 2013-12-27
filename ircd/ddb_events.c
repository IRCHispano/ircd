/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ddb_events.c
 *
 * Copyright (C) 2002-2007 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2004-2007 Toni Garcia (zoltan) <zoltan@irc-dev.net>
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
 * @brief Events of Distributed DataBases.
 * @version $Id: ddb_events.c,v 1.11 2007-04-21 21:17:22 zolty Exp $
 */
#include "config.h"

#include "ddb.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_chattr.h"
#include "ircd_features.h"
#include "ircd_snprintf.h"
#include "ircd_tea.h"
#include "msg.h"
#include "numnicks.h"
#include "s_user.h"
#include "send.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** It indicates events is initialized */
static int events_init = 0;
/** Events table engine */
ddb_events_table_td ddb_events_table[DDB_TABLE_MAX];

static void ddb_events_table_c(char *key, char *content, int update);
static void ddb_events_table_d(char *key, char *content, int update);
static void ddb_events_table_f(char *key, char *content, int update);
static void ddb_events_table_o(char *key, char *content, int update);
static void ddb_events_table_n(char *key, char *content, int update);
static void ddb_events_table_v(char *key, char *content, int update);
static void ddb_events_table_z(char *key, char *content, int update);

/* PROVISIONAL */
int max_clones;
char *msg_many_clones;
char *ip_crypt_key;
unsigned int binary_ip_crypt_key[2];

int numero_maximo_de_clones_por_defecto;
char *clave_de_cifrado_de_ips;
unsigned int clave_de_cifrado_binaria[2];
#if defined(WEBCHAT)
unsigned char clave_de_cifrado_de_cookies[32];
int cifrado_cookies = 0;
unsigned char clave_de_cifrado_de_cookies2[32];
int cifrado_cookies2 = 0;
#endif
int ocultar_servidores = 0;
int activar_modos = 0;
int activar_ident = 0;
int auto_invisible = 0;
int excepcion_invisible = 0;
int activar_redireccion_canales = 0;
char *mensaje_quit_personalizado = NULL;
int compresion_zlib_cliente = 1;
char *mensaje_part_svskick = NULL;
int transicion_ircd = 0;




/** Initialize events module of %DDB Distributed DataBases.
 */
void
ddb_events_init(void)
{
  if (events_init)
    return;

  ddb_events_table[DDB_BOTDB] = ddb_events_table_b;
  ddb_events_table[DDB_CHANDB] = ddb_events_table_c;
  ddb_events_table[DDB_CHANDB2] = ddb_events_table_d;
  ddb_events_table[DDB_EXCEPTIONDB] = 0;
  ddb_events_table[DDB_FEATUREDB] = ddb_events_table_f;
  ddb_events_table[DDB_ILINEDB] = 0;
  ddb_events_table[DDB_JUPEDB] = 0;
  ddb_events_table[DDB_NICKDB] = ddb_events_table_n;
  ddb_events_table[DDB_OPERDB] = ddb_events_table_o;
  ddb_events_table[DDB_UWORLDDB] = 0;
  ddb_events_table[DDB_VHOSTDB] = ddb_events_table_v;
  ddb_events_table[DDB_WEBIRCDB] = 0;
  ddb_events_table[DDB_CONFIGDB] = ddb_events_table_z;

  events_init = 1;
}

/** Handle events on Channel Table.
 * @param[in] key Key of registry.
 * @param[in] content Content of registry.
 * @param[in] update Update of registry or no.
 */
static void
ddb_events_table_c(char *key, char *content, int update)
{
  struct Channel *chptr;
  struct Client *founder;
  char *botname;

  if (ddb_count_table[DDB_CONFIGDB])
    botname = ddb_get_botname(DDB_CHANSERV);
  else
    botname = cli_name(&me);

  if (content)
  {
    /* Nuevos o cambios de registro */
    /* Si no existe el canal lo creamos */
    chptr = get_channel(NULL, key, CGT_CREATE);

    /* Ponemos modo +r */
    if (!(chptr->mode.mode & MODE_REGISTERED))
    {
      chptr->mode.mode |= MODE_REGISTERED;
      if (chptr->users)
        sendcmdbotto_channel(botname, CMD_MODE, chptr, NULL, SKIP_SERVERS,
                             "%H +r", chptr);
    }

    if (chptr->users)
    {
      struct Membership *member;

      /* Solo si el nuevo founder es nuestro usuario */
      founder = FindUser(content);
      if (founder && MyUser(founder))
      {
        if (update)
        {
          /* Si hay una actualizacion, borramos el antiguo */
          for (member = chptr->members; member; member = member->next_member)
          {
            if (IsChanOwner(member))
            {
              ClearChanOwner(member);
              sendcmdbotto_channel(botname, CMD_MODE,
                                   member->channel, NULL, SKIP_SERVERS, "%H -q %C",
                                   member->channel, member->user);
              sendcmdto_serv(&me, CMD_BMODE, NULL, "%s %H -q %C",
                             DDB_CHANSERV, member->channel, member->user);
              break;
            }
          }
        }
        member = find_member_link(chptr, founder);
        if (member)
        {
          SetChanOwner(member);
          sendcmdbotto_channel(botname, CMD_MODE,
                               member->channel, NULL, SKIP_SERVERS, "%H +q %C",
                               member->channel, member->user);
          sendcmdto_serv(&me, CMD_BMODE, NULL, "%s %H +q %C",
                         DDB_CHANSERV, member->channel, member->user);
        }
      }
      else if (!founder)
      {
        for (member = chptr->members; member; member = member->next_member)
        {
          if (IsChanOwner(member))
          {
            ClearChanOwner(member);
            if (MyUser(member->user))
            {
              sendcmdbotto_channel(botname, CMD_MODE,
                                   member->channel, NULL, SKIP_SERVERS, "%H -q %C",
                                   member->channel, member->user);
              sendcmdto_serv(&me, CMD_BMODE, NULL, "%s %H -q %C",
                             DDB_CHANSERV, member->channel, member->user);
            }
            break;
          }
        }
      }
    }
  }
  else
  {
    /* Canal dropado */
    chptr = get_channel(NULL, key, CGT_NO_CREATE);

    /* Borramos el canal */
    if (chptr && ((chptr->mode.mode) & MODE_REGISTERED))
    {
      chptr->mode.mode = mode & (~(MODE_REGISTERED | MODE_AUTOOP | MODE_SECUREOP));  /* Modos vinculados a +r */
      chptr->modos_obligatorios = chptr->modos_prohibidos = 0;

      if (chptr->users)
      {
        char buf[64];

        strcpy(buf, "-r");
        if (mode & MODE_AUTOOP)
            strcat(buf, "A"); /* Modos vinculados a +r */
        if (mode & MODE_SECUREOP)
            strcat(buf, "S"); /* Modos vinculados a +r */
#if defined(WEBCHAT)
        /* No se muestran modos en tok/web */
#endif
        /* quitar modos */
        sendcmdbotto_channel(botname, CMD_MODE, chptr,
                             NULL, SKIP_SERVERS, "%H %s", chptr, buf);
      }
      else
        sub1_from_channel(chptr);
    }
  }
}

/** Handle events on Channel Table 2.
 * @param[in] key Key of registry.
 * @param[in] content Content of registry.
 * @param[in] update Update of registry or no.
 */
static void
ddb_events_table_d(char *key, char *content, int update)
{
  struct Channel *chptr;
  struct Client *founder;
  char *botname;

  if (ddb_count_table[DDB_CONFIGDB])
    botname = ddb_get_botname(DDB_CHANSERV);
  else
    botname = cli_name(&me);

  if (content)
  {
    /* Nuevos o cambios de registro */
    chptr = get_channel(NULL, key, CGT_NO_CREATE);

    if (chptr && (chptr->mode.mode & MODE_REGISTERED) && chptr->users)
    {
      struct Membership *member;

      /* Solo si el nuevo founder es nuestro usuario */
      founder = FindUser(content);
      if (founder && MyUser(founder))
      {
        if (update)
        {
          /* Si hay una actualizacion, borramos el antiguo */
          for (member = chptr->members; member; member = member->next_member)
          {
            if (IsChanOwner(member))
            {
              ClearChanOwner(member);
              sendcmdbotto_channel(botname, CMD_MODE,
                                   member->channel, NULL, SKIP_SERVERS, "%H -q %C",
                                   member->channel, member->user);
              sendcmdto_serv(&me, CMD_BMODE, NULL, "%s %H -q %C",
                             DDB_CHANSERV, member->channel, member->user);
              break;
            }
          }
        }
        member = find_member_link(chptr, founder);
        if (member)
        {
          SetChanOwner(member);
          sendcmdbotto_channel(botname, CMD_MODE,
                               member->channel, NULL, SKIP_SERVERS, "%H +q %C",
                               member->channel, member->user);
          sendcmdto_serv(&me, CMD_BMODE, NULL, "%s %H +q %C",
                         DDB_CHANSERV, member->channel, member->user);
        }
      }
      else if (!founder)
      {
        for (member = chptr->members; member; member = member->next_member)
        {
          if (IsChanOwner(member))
          {
            ClearChanOwner(member);
            if (MyUser(member->user))
            {
              sendcmdbotto_channel(botname, CMD_MODE,
                                   member->channel, NULL, SKIP_SERVERS, "%H -q %C",
                                   member->channel, member->user);
              sendcmdto_serv(&me, CMD_BMODE, NULL, "%s %H -q %C",
                             DDB_CHANSERV, member->channel, member->user);
            }
            break;
          }
        }
      }
    }
  }
  else
  {
    /* Borrado de founder */
    chptr = get_channel(NULL, key, CGT_NO_CREATE);

    if (chptr && chptr->users)
    {
      struct Membership *member;

      /* Quitar -q */
      for (member = chptr->members; member; member = member->next_member)
      {
        if (IsChanOwner(member) && MyUser(member->user))
        {
          ClearChanOwner(member);
          sendcmdbotto_channel(botname, CMD_MODE,
                               member->channel, NULL, SKIP_SERVERS, "%H -q %C",
                               member->channel, member->user);
          sendcmdto_serv(&me, CMD_BMODE, NULL, "%s %H -q %C",
                         DDB_CHANSERV, member->channel, member->user);
          break;
        }
      }
    }
  }
}

#if 0
ELINES
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
#endif

/** Handle events on Features Table.
 * @param[in] key Key of registry.
 * @param[in] content Content of registry.
 * @param[in] update Update of registry or no.
 */
static void
ddb_events_table_f(char *key, char *content, int update)
{
  static char *keytemp = NULL;
  static int key_len = 0;
  int i = 0;

  if ((strlen(key) + 1 > key_len) || (!keytemp))
  {
    key_len = strlen(key) + 1;
    if (keytemp)
      MyFree(keytemp);
    keytemp = MyMalloc(key_len);

    assert(0 != keytemp);
  }
  strcpy(keytemp, key);
  while (keytemp[i])
  {
    keytemp[i] = ToUpper(keytemp[i]);
    i++;
  }

  if (content)
  {
    char *tempa[2];
    tempa[0] = keytemp;
    tempa[1] = content;

    feature_set(&me, (const char * const *)tempa, 2);
  }
  else
  {
    char *tempb[1];
    tempb[0] = keytemp;

    feature_set(&me, (const char * const *)tempb, 1);
  }
}

/** Handle events on Nick Table.
 * @param[in] key Key of registry.
 * @param[in] content Content of registry.
 * @param[in] update Update of registry or no.
 */
static void
ddb_events_table_n(char *key, char *content, int update)
{
  struct Client *cptr;
  char *botname;
  int nick_renames = 0;

  /* Only my clients */
  if ((cptr = FindUser(key)) && MyConnect(cptr))
  {
    botname = ddb_get_botname(DDB_NICKSERV);
    /* Droping Key */
    if (!content && (IsNickRegistered(cptr) || IsNickSuspended(cptr)))
    {
      struct Flags oldflags;

      oldflags = cli_flags(cptr);
      ClearNickRegistered(cptr);
      ClearNickSuspended(cptr);

      sendcmdbotto_one(botname, CMD_NOTICE, cptr,
                       "%C :*** Your nick %C is droping", cptr, cptr);

      send_umode_out(cptr, cptr, &oldflags, IsRegistered(cptr));
    }
    else if (content)
    {
      /* New Key or Update Key */
      int nick_suspend = 0;

      if (content[strlen(content) - 1] == '+')
        nick_suspend = 1;

      if (*content == '*')
      {
        /* Forbid Nick */
        sendcmdbotto_one(botname, CMD_NOTICE, cptr,
                         "%C :*** Your nick %C has been forbided, cannot be used", cptr, cptr);
        nick_renames = 1;
      }
      else if (nick_suspend && update && IsNickRegistered(cptr))
      {
        struct Flags oldflags;

        oldflags = cli_flags(cptr);
        ClearNickRegistered(cptr);
        ClearAdmin(cptr);
        ClearCoder(cptr);
        ClearHelpOper(cptr);
        ClearBot(cptr);
        SetNickSuspended(cptr);

        sendcmdbotto_one(botname, CMD_NOTICE, cptr,
                         "%C :*** Your nick %C has been suspended", cptr, cptr);

        send_umode_out(cptr, cptr, &oldflags, IsRegistered(cptr));
      }
      else if (!nick_suspend && update && IsNickSuspended(cptr))
      {
        struct Flags oldflags;

        oldflags = cli_flags(cptr);
        ClearNickSuspended(cptr);
        SetNickRegistered(cptr);

        sendcmdbotto_one(botname, CMD_NOTICE, cptr,
                         "%C :*** Your nick %C has been unsuspended", cptr, cptr);

        send_umode_out(cptr, cptr, &oldflags, IsRegistered(cptr));
      }
      else if (!update)
      {
        sendcmdbotto_one(botname, CMD_NOTICE, cptr,
                         "%C :*** Your nick %C has been registered", cptr, cptr);
        nick_renames = 1;
      }
    }

    if (nick_renames)
    {
      char *newnick;
      char tmp[100];
      char *parv[3];
      int flags = 0;

      newnick = get_random_nick(cptr);

      SetRenamed(flags);

      parv[0] = cli_name(cptr);
      parv[1] = newnick;
      ircd_snprintf(0, tmp, sizeof(tmp), "%T", TStime());
      parv[2] = tmp;

      set_nick_name(cptr, cptr, newnick, 3, parv, flags);
    }
  }
}

/** Handle events on Operators Table.
 * @param[in] key Key of registry.
 * @param[in] content Content of registry.
 * @param[in] update Update of registry or no.
 */
static void
ddb_events_table_o(char *key, char *content, int update)
{
  struct Client *cptr;
  if ((cptr = FindUser(key)) && MyConnect(cptr))
  {
    /* Droping Key */
    if (!content && (IsAdmin(cptr) || IsCoder(cptr) || IsHelpOper(cptr) || IsBot(cptr)))
    {
      struct Flags oldflags;

      oldflags = cli_flags(cptr);
      ClearAdmin(cptr);
      ClearCoder(cptr);
      ClearHelpOper(cptr);
 /* contador -- */
      ClearBot(cptr);

      send_umode_out(cptr, cptr, &oldflags, IsRegistered(cptr));
    }
    else if (content)
    {
      /* New Key or Update Key */
      if (IsNickRegistered(cptr))
      {
        struct Flags oldflags = cli_flags(cptr);
        int update = 0;

        if ((*content == 'a') && !IsAdmin(cptr))
        {
          SetAdmin(cptr);
          ClearCoder(cptr);
          ClearHelpOper(cptr);
          ClearBot(cptr);
          update = 1;
        }
        else if ((*content == 'c') && !IsCoder(cptr))
        {
          ClearAdmin(cptr);
          SetCoder(cptr);
          ClearHelpOper(cptr);
          ClearBot(cptr);
          update = 1;
        }
        else if ((*content == 'B') && !IsBot(cptr))
        {
          ClearAdmin(cptr);
          ClearCoder(cptr);
          ClearHelpOper(cptr);
          SetBot(cptr);
          update = 1;
        }
        else if (!IsHelpOper(cptr))
        {
          ClearAdmin(cptr);
          ClearCoder(cptr);
          SetHelpOper(cptr);
          ClearBot(cptr);
          update = 1;
        }

        if (update)
          send_umode_out(cptr, cptr, &oldflags, IsRegistered(cptr));
      }
    }
  }
}

/** Handle events on Vhosts Table.
 * @param[in] key Key of registry.
 * @param[in] content Content of registry.
 * @param[in] update Update of registry or no.
 */
static void
ddb_events_table_v(char *key, char *content, int update)
{
  struct Client *cptr;

  if (((cptr = FindUser(key))) && IsNickRegistered(cptr))
  {
    /* Droping Key */
    if (!content)
    {
      if (IsHiddenHost(cptr))
      {
        hide_hostmask(cptr, 0, FLAG_NICKREG);
      }
    }
    else if (content && update)
    {
      if (strcmp(content, cli_user(cptr)->host))
        hide_hostmask(cptr, content, FLAG_NICKREG);
    }
    else
      hide_hostmask(cptr, content, FLAG_NICKREG);

#if 0
ALTA
        if ((sptr = FindUser(c)) && IsUser(sptr))
        {
          BorraIpVirtualPerso(sptr);
          SetIpVirtualPersonalizada(sptr);
          if (MyUser(sptr))
            make_vhostperso(sptr, 1);
        }



BAJA
            if ((sptr = FindUser(c)) && IsUser(sptr))
            {
              BorraIpVirtualPerso(sptr);
              if (MyUser(sptr))
                sendto_one(sptr, rpl_str(RPL_HOSTHIDDEN), me.name, sptr->name,
                    get_virtualhost(sptr, 0));
            }


#endif
  }

}
/** Handle events on Config Table.
 * @param[in] key Key of registry.
 * @param[in] content Content of registry.
 * @param[in] update Update of registry or no.
 */
static void
ddb_events_table_z(char *key, char *content, int update)
{
  if (content)
  {
    if (!strcmp(key, DDB_CONFIGDB_MAX_CLONES_PER_IP))
      max_clones = atoi(content);
    else if (!strcmp(key, DDB_CONFIGDB_MSG_TOO_MANY_FROM_IP))
      msg_many_clones = content;
    else if (!strcmp(key, DDB_CONFIGDB_IP_CRYPT_KEY))
    {
      char keytmp[12 + 1];
      char c;

      ip_crypt_key = content;
      strncpy(keytmp, content, 12);
      keytmp[12] = '\0';
      c = keytmp[6];
      keytmp[6] = '\0';
      binary_ip_crypt_key[0] = base64toint(keytmp);
      keytmp[6] = c;
      binary_ip_crypt_key[1] = base64toint(keytmp + 6);
/* limpiar cache */
    }
#if 0
#if defined(WEBCHAT)
      else if (!strcmp(c, BDD_CLAVE_DE_CIFRADO_DE_COOKIES))
      {
        int key_len;
        char key[45];
        memset(key, 'A', sizeof(key));

        key_len = strlen(v);
        key_len = (key_len>44) ? 44 : key_len;

        strncpy((char *)key+(44-key_len), v, (key_len));
        key[44]='\0';

        base64_to_buf_r(clave_de_cifrado_de_cookies, key);
        cifrado_cookies = 1;
      }
      else if (!strcmp(c, BDD_CLAVE_DE_CIFRADO_DE_COOKIES2))
      {
        int key_len;
        char key[45];
        memset(key, 'A', sizeof(key));

        key_len = strlen(v);
        key_len = (key_len>44) ? 44 : key_len;

        strncpy((char *)key+(44-key_len), v, (key_len));
        key[44]='\0';

        base64_to_buf_r(clave_de_cifrado_de_cookies2, key);
        cifrado_cookies2 = 1;
      }
#endif
      else if (!strcmp(c, BDD_AUTOINVISIBLE))
      {
        auto_invisible = !0;
      }
      else if(!strcmp(c, BDD_COMPRESION_ZLIB_CLIENTE))
      {
        int x = atoi(v);
        if(x<0 || x>9)
            compresion_zlib_cliente=0;
        else
            compresion_zlib_cliente=x;
      }
     else if(!strncmp(c, "redirect:", 9) && !strcmp(c+9, me.name))
      {
        activar_redireccion_canales=1;
      }
      else if(!strncmp(c, "quit:", 5) && !strcmp(c+5, me.name))
      {
        SlabStringAllocDup(&mensaje_quit_personalizado, v, 0);
      }
      else if(!strncmp(c, "noinvisible:", 12) && !strcmp(c+12, me.name))
      {
        excepcion_invisible = !0;
      }
      else if(!strcmp(c, BDD_MENSAJE_PART_SVSKICK))
      {
        SlabStringAllocDup(&mensaje_part_svskick, v, 0);
      }

#endif
  }
  else
  {
    if (!strcmp(key, DDB_CONFIGDB_MAX_CLONES_PER_IP))
      max_clones = 0;
    else if (!strcmp(key, DDB_CONFIGDB_MSG_TOO_MANY_FROM_IP))
      msg_many_clones = NULL;
    else if (!strcmp(key, DDB_CONFIGDB_IP_CRYPT_KEY))
    {
      ip_crypt_key = NULL;
      binary_ip_crypt_key[0] = 0;
      binary_ip_crypt_key[1] = 0;
    }
#if 0
#if defined(WEBCHAT)
            else if (!strcmp(c, BDD_CLAVE_DE_CIFRADO_DE_COOKIES))
            {
              int i;
              cifrado_cookies=0;
              for(i=0;i<24;i++)
                clave_de_cifrado_de_cookies[i] = 0;
            }
            else if (!strcmp(c, BDD_CLAVE_DE_CIFRADO_DE_COOKIES2))
            {
              int i;
              cifrado_cookies2=0;
              for(i=0;i<24;i++)
                clave_de_cifrado_de_cookies2[i] = 0;
            }
#endif
            else if (!strcmp(c, BDD_AUTOINVISIBLE))
            {
              auto_invisible = 0;
            }
            else if(!strcmp(c, BDD_COMPRESION_ZLIB_CLIENTE))
            {
              compresion_zlib_cliente = 1;
            }

            else if(!strncmp(c, "redirect:", 9) && !strcmp(c+9, me.name))
            {
              activar_redireccion_canales=0;
            }
            else if(!strncmp(c, "quit:", 5) && !strcmp(c+5, me.name))
            {
              if(mensaje_quit_personalizado)
              {
                RunFree(mensaje_quit_personalizado);
                mensaje_quit_personalizado=NULL;
              }
            }
            else if(!strncmp(c, "noinvisible:", 12) && !strcmp(c+12, me.name))
            {
              excepcion_invisible=0;
            }
            else if(!strcmp(c, BDD_MENSAJE_PART_SVSKICK))
            {
              if(mensaje_part_svskick)
              {
                RunFree(mensaje_part_svskick);
                mensaje_part_svskick=NULL;
              }
            }


#endif
  }
}
