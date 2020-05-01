/*
 * IRC - Internet Relay Chat, ircd/s_user.c (formerly ircd/s_msg.c)
 * Copyright (C) 1990 Jarkko Oikarinen and
 *                    University of Oulu, Computing Center
 *
 * See file AUTHORS in IRC package for additional names of
 * the programmers.
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

#include "sys.h"
#include <sys/stat.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <stdlib.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(USE_SYSLOG)
#include <syslog.h>
#endif

/* Definimos esta constante aqui ya que es el fichero donde es declarada */
#define _NOTSENDER_INCLUIDO_

#include <assert.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>

#include "h.h"
#include "s_debug.h"
#include "struct.h"
#include "common.h"
#include "s_serv.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "s_misc.h"
#include "match.h"
#include "hash.h"
#include "s_bsd.h"
#include "whowas.h"
#include "list.h"
#include "s_err.h"
#include "parse.h"
#include "m_watch.h"
#include "s_bdd.h"
#include "ircd.h"
#include "s_user.h"
#include "support.h"
#include "s_user.h"
#include "channel.h"
#include "random.h"
#include "version.h"
#include "msg.h"
#include "userload.h"
#include "numnicks.h"
#include "sprintf_irc.h"
#include "querycmds.h"
#include "IPcheck.h"
#include "class.h"
#include "slab_alloc.h"
#include "network.h"
#include "m_config.h"
#include "spam.h"
#if defined(USE_GEOIP2)
#include "geoip.h"
#endif

#include <json-c/json.h>
#include <json-c/json_object.h>


static char buf[BUFSIZE], buf2[BUFSIZE];
static char *nuevo_nick_aleatorio(aClient *cptr);

/*
 * m_functions execute protocol messages on this server:
 *
 *    cptr    is always NON-NULL, pointing to a *LOCAL* client
 *            structure (with an open socket connected!). This
 *            identifies the physical socket where the message
 *            originated (or which caused the m_function to be
 *            executed--some m_functions may call others...).
 *
 *    sptr    is the source of the message, defined by the
 *            prefix part of the message if present. If not
 *            or prefix not found, then sptr==cptr.
 *
 *            (!IsServer(cptr)) => (cptr == sptr), because
 *            prefixes are taken *only* from servers...
 *
 *            (IsServer(cptr))
 *                    (sptr == cptr) => the message didn't
 *                    have the prefix.
 *
 *                    (sptr != cptr && IsServer(sptr) means
 *                    the prefix specified servername. (?)
 *
 *                    (sptr != cptr && !IsServer(sptr) means
 *                    that message originated from a remote
 *                    user (not local).
 *
 *            combining
 *
 *            (!IsServer(sptr)) means that, sptr can safely
 *            taken as defining the target structure of the
 *            message in this server.
 *
 *    *Always* true (if 'parse' and others are working correct):
 *
 *    1)      sptr->from == cptr  (note: cptr->from == cptr)
 *
 *    2)      MyConnect(sptr) <=> sptr == cptr (e.g. sptr
 *            *cannot* be a local connection, unless it's
 *            actually cptr!). [MyConnect(x) should probably
 *            be defined as (x == x->from) --msa ]
 *
 *    parc    number of variable parameter strings (if zero,
 *            parv is allowed to be NULL)
 *
 *    parv    a NULL terminated list of parameter pointers,
 *
 *                    parv[0], sender (prefix string), if not present
 *                            this points to an empty string.
 *                    parv[1]...parv[parc-1]
 *                            pointers to additional parameters
 *                    parv[parc] == NULL, *always*
 *
 *            note:   it is guaranteed that parv[0]..parv[parc-1] are all
 *                    non-NULL pointers.
 */

/*
 * next_client
 *
 * Local function to find the next matching client. The search
 * can be continued from the specified client entry. Normal
 * usage loop is:
 *
 * for (x = client; x = next_client(x,mask); x = x->next)
 *     HandleMatchingClient;
 *
 */
aClient *next_client(aClient *next, char *ch)
{
  Reg3 aClient *tmp = next;

  if (!tmp)
    return NULL;

  next = FindClient(ch);
  next = next ? next : tmp;
  if (tmp->prev == next)
    return NULL;
  if (next != tmp)
    return next;
  for (; next; next = next->next)
    if ((next->name) && (!match(ch, next->name)))
      break;
  return next;
}

/*
 * hunt_server
 *
 *    Do the basic thing in delivering the message (command)
 *    across the relays to the specific server (server) for
 *    actions.
 *
 *    Note:   The command is a format string and *MUST* be
 *            of prefixed style (e.g. ":%s COMMAND %s ...").
 *            Command can have only max 8 parameters.
 *
 *    server  parv[server] is the parameter identifying the
 *            target server.
 *
 *    *WARNING*
 *            parv[server] is replaced with the pointer to the
 *            real servername from the matched client (I'm lazy
 *            now --msa).
 *
 *    returns: (see #defines)
 */
int hunt_server(int MustBeOper, aClient *cptr, aClient *sptr, char *command,
    char *token, const char *pattern, int server, int parc, char *parv[])
{
  aClient *acptr;
  char y[8];

  /* Assume it's me, if no server or an unregistered client */
  if (parc <= server || BadPtr(parv[server]) || IsUnknown(sptr))
    return (HUNTED_ISME);

  /* Make sure it's a server */
  if (MyUser(sptr) 
#if !defined(NO_PROTOCOL9)
      || Protocol(cptr) < 10
#endif
  )  
  {
    /* Make sure it's a server */
    if (!strchr(parv[server], '*'))
    {
      if (0 == (acptr = FindClient(parv[server])))
        return HUNTED_NOSUCH;
      if (acptr->user)
        acptr = acptr->user->server;
    }
    else if (!(acptr = find_match_server(parv[server])))
    {
      sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
          me.name, parv[0], parv[server]);
      return (HUNTED_NOSUCH);
    }
  }
  else if (!(acptr = FindNServer(parv[server])))
    return (HUNTED_NOSUCH);     /* Server broke off in the meantime */

  if (IsMe(acptr))
    return (HUNTED_ISME);

  if (MustBeOper && !IsPrivileged(sptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
    return HUNTED_NOSUCH;
  }

  if (Protocol(acptr->from) > 9)
  {
    strcpy(y, acptr->yxx);
    parv[server] = y;
  }
#if !defined(NO_PROTOCOL9)
  else
    parv[server] = acptr->name;
#endif
  sendto_one_hunt(acptr, sptr, command, token, pattern, parv[1], parv[2], parv[3], parv[4],
      parv[5], parv[6], parv[7], parv[8]);

  return (HUNTED_PASS);
}

/*
 * 'do_nick_name' ensures that the given parameter (nick) is really a proper
 * string for a nickname (note, the 'nick' may be modified in the process...)
 *
 * RETURNS the length of the final NICKNAME (0, if nickname is invalid)
 *
 * Nickname characters are in range 'A'..'}', '_', '-', '0'..'9'
 *  anything outside the above set will terminate nickname.
 * In addition, the first character cannot be '-' or a Digit.
 *
 * Note:
 *  The '~'-character should be allowed, but a change should be global,
 *  some confusion would result if only few servers allowed it...
 */

static int do_nick_name(char *nick)
{
  Reg1 char *ch;

  if (*nick == '-' || isDigit(*nick)) /* first character in [0..9-] */
    return 0;

  for (ch = nick; *ch && (ch - nick) < NICKLEN; ch++)
    if (!isIrcNk(*ch))
      break;

  *ch = '\0';

  return (ch - nick);
}

/*
 * canonize
 *
 * reduce a string of duplicate list entries to contain only the unique
 * items.  Unavoidably O(n^2).
 */
char *canonize(char *buffer)
{
  static char cbuf[BUFSIZ];
  char *s, *t, *cp = cbuf;
  int l = 0;
  char *p = NULL, *p2;

  *cp = '\0';

  for (s = strtoken(&p, buffer, ","); s; s = strtoken(&p, NULL, ","))
  {
    if (l)
    {
      p2 = NULL;
      for (t = strtoken(&p2, cbuf, ","); t; t = strtoken(&p2, NULL, ","))
        if (!strCasediff(s, t))
          break;
        else if (p2)
          p2[-1] = ',';
    }
    else
      t = NULL;
    if (!t)
    {
      if (l)
        *(cp - 1) = ',';
      else
        l = 1;
      strcpy(cp, s);
      if (p)
        cp += (p - s);
    }
    else if (p2)
      p2[-1] = ',';
  }
  return cbuf;
}

/*
 * clean_user_id
 *
 * Copy `source' to `dest', replacing all occurances of '~' and characters that
 * are not `isIrcUi' by an underscore.
 * Copies at most USERLEN - 1 characters or up till the first control character.
 * If `tilde' is true, then a tilde is prepended to `dest'.
 * Note that `dest' and `source' can point to the same area or to different
 * non-overlapping areas.
 */


/*
 * 27/Nov/03 - RyDeN
 * Modifico dest para hacerlo compatible con el slab allocator
 *
 */
static char *clean_user_id(char **dest, char *source, int tilde)
{
  char ch;
  char *d;
  char *s = source;
  int rlen = USERLEN;
  char temp_userid[USERLEN + 1];

  d = temp_userid;
  ch = *s++;                    /* Store first character to copy: */
  if (tilde)
  {
    *d++ = '~';                 /* If `dest' == `source', then this overwrites `ch' */
    --rlen;
  }
  while (ch && !isCntrl(ch) && rlen--)
  {
    char nch = *s++;   /* Store next character to copy */
    *d++ = isIrcUi(ch) ? ch : '_';  /* This possibly overwrites it */
    if (nch == '~')
      ch = '_';
    else
      ch = nch;
  }
  *d = 0;
  SlabStringAllocDup(dest, temp_userid, 0);
  return *dest;
}

/*
 * register_user
 *
 * This function is called when both NICK and USER messages
 * have been accepted for the client, in whatever order. Only
 * after this the USER message is propagated.
 *
 * NICK's must be propagated at once when received, although
 * it would be better to delay them too until full info is
 * available. Doing it is not so simple though, would have
 * to implement the following:
 *
 * 1) user telnets in and gives only "NICK foobar" and waits
 * 2) another user far away logs in normally with the nick
 *    "foobar" (quite legal, as this server didn't propagate it).
 * 3) now this server gets nick "foobar" from outside, but
 *    has already the same defined locally. Current server
 *    would just issue "KILL foobar" to clean out dups. But,
 *    this is not fair. It should actually request another
 *    nick from local user or kill him/her...
 */

static int register_user(aClient *cptr, aClient *sptr,
    char *nick, char *username)
{
  Reg1 aConfItem *aconf;
  char *parv[3], *tmpstr, *tmpstr2;
  char c = 0 /* not alphanum */ , d = 'a' /* not a digit */ ;
  short upper = 0;
  short lower = 0;
  short pos = 0, leadcaps = 0, other = 0, digits = 0, badid = 0;
  short digitgroups = 0;
  anUser *user = sptr->user;
  Dlink *lp;
  char ip_base64[25];
  int found_g;

  user->last = now;
  parv[0] = sptr->name;
  assert(parv[0] != NULL);

  parv[1] = parv[2] = NULL;

  if (MyConnect(sptr))
  {
    static time_t last_too_many1, last_too_many2;
    switch (check_client(sptr))
    {
      case ACR_OK:
        break;
      case ACR_NO_AUTHORIZATION:
        sendto_op_mask(SNO_UNAUTH, "Unauthorized connection from %s.",
            PunteroACadena(sptr->sockhost));
        ircstp->is_ref++;
        return exit_client(cptr, sptr, &me,
            "No Authorization - use another server");
      case ACR_TOO_MANY_IN_CLASS:
        if (now - last_too_many1 >= (time_t) 60)
        {
          last_too_many1 = now;
          sendto_op_mask(SNO_TOOMANY, "Too many connections in class for %s.",
              PunteroACadena(sptr->sockhost));
        }
        IPcheck_connect_fail(sptr);
        ircstp->is_ref++;
        {
          char *msg =
              "Sorry, your connection class is full - try again later or try another server";
#if defined(BDD_CLONES)
          struct db_reg *msg_db;

          msg_db =
              db_buscar_registro(BDD_CONFIGDB,
              BDD_MENSAJE_DE_CAPACIDAD_SUPERADA);
          if (msg_db)
          {
            msg = msg_db->valor;
          }
#endif
          return exit_client(cptr, sptr, &me, msg);
        }
      case ACR_TOO_MANY_FROM_IP:
        if (now - last_too_many2 >= (time_t) 60)
        {
          last_too_many2 = now;
          sendto_op_mask(SNO_TOOMANY,
              "Too many connections from same IP for %s.",
              PunteroACadena(sptr->sockhost));
        }
        ircstp->is_ref++;
        return exit_client(cptr, sptr, &me,
            "Too many connections from your host");
      case ACR_ALREADY_AUTHORIZED:
        /* Can this ever happen? */
      case ACR_BAD_SOCKET:
        IPcheck_connect_fail(sptr);
        return exit_client(cptr, sptr, &me, "Unknown error -- Try again");
    }
    if (IsUnixSocket(sptr))
      SlabStringAllocDup(&(user->host), me.name, HOSTLEN);
    else
      SlabStringAllocDup(&(user->host), PunteroACadena(sptr->sockhost),
          HOSTLEN);
    aconf = sptr->confs->value.aconf;

    if (!activar_ident)
      sptr->flags &= ~FLAGS_DOID;

    if (sptr->flags & FLAGS_GOTID)
      assert(sptr->username);

    clean_user_id(&(user->username),
        (sptr->flags & FLAGS_GOTID) ? sptr->username : username,
        (sptr->flags & FLAGS_DOID) && !(sptr->flags & FLAGS_GOTID));

    if ((user->username[0] == '\000')
        || ((user->username[0] == '~') && (user->username[1] == '\000')))
      return exit_client(cptr, sptr, &me, "USER: Bogus userid.");

    if (!BadPtr(aconf->passwd)
        && !(isDigit(*aconf->passwd) && !aconf->passwd[1])
        && strcmp(PunteroACadena(sptr->passwd), aconf->passwd))
    {
      ircstp->is_ref++;
      sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name, parv[0]);
      IPcheck_connect_fail(sptr);
      return exit_client(cptr, sptr, &me, "Bad Password");
    }

    /*
     * following block for the benefit of time-dependent K:-lines
     */
    if ((found_g = find_kill(sptr)))
    {
      ircstp->is_ref++;
      return exit_client(cptr, sptr, &me,
          found_g == -2 ? "G-lined" : "K-lined");
    }

#if defined(R_LINES)
    if (find_restrict(sptr))
    {
      ircstp->is_ref++;
      return exit_client(cptr, sptr, &me, "R-lined");
    }
#endif

#if defined(USE_GEOIP2)
    if (geo_enable)
    {
      /* ... */
      strncpy(sptr->country_iso, "--", 2);
      sptr->asnum = 0;
      /* ... */
    } else {
      strncpy(sptr->country_iso, "--", 2);
      sptr->asnum = 0;
    }
#endif

    /*
     * Check for mixed case usernames, meaning probably hacked.  Jon2 3-94
     * Summary of rules now implemented in this patch:         Ensor 11-94
     * In a mixed-case name, if first char is upper, one more upper may
     * appear anywhere.  (A mixed-case name *must* have an upper first
     * char, and may have one other upper.)
     * A third upper may appear if all 3 appear at the beginning of the
     * name, separated only by "others" (-/_/.).
     * A single group of digits is allowed anywhere.
     * Two groups of digits are allowed if at least one of the groups is
     * at the beginning or the end.
     * Only one '-', '_', or '.' is allowed (or two, if not consecutive).
     * But not as the first or last char.
     * No other special characters are allowed.
     * Name must contain at least one letter.
     */
  if (activar_ident)
  {
    tmpstr2 = tmpstr = (username[0] == '~' ? &username[1] : username);
    while (*tmpstr && !badid)
    {
      pos++;
      c = *tmpstr;
      tmpstr++;
      if (isLower(c))
      {
        lower++;
      }
      else if (isUpper(c))
      {
        upper++;
        if ((leadcaps || pos == 1) && !lower && !digits)
          leadcaps++;
      }
      else if (isDigit(c))
      {
        digits++;
        if (pos == 1 || !isDigit(d))
        {
          digitgroups++;
          if (digitgroups > 2)
            badid = 1;
        }
      }
      else if (c == '-' || c == '_' || c == '.')
      {
        other++;
        if (pos == 1)
          badid = 1;
        else if (d == '-' || d == '_' || d == '.' || other > 2)
          badid = 1;
      }
      else
        badid = 1;
      d = c;
    }
    if (!badid)
    {
      if (lower && upper && (!leadcaps || leadcaps > 3 ||
          (upper > 2 && upper > leadcaps)))
        badid = 1;
      else if (digitgroups == 2 && !(isDigit(tmpstr2[0]) || isDigit(c)))
        badid = 1;
      else if ((!lower && !upper) || !isAlnum(c))
        badid = 1;
    }
    if (badid && (!(sptr->flags & FLAGS_GOTID) ||
        strcmp(PunteroACadena(sptr->username), username) != 0))
    {
      ircstp->is_ref++;
      sendto_one(cptr, ":%s %d %s :Your username is invalid.",
          me.name, ERR_INVALIDUSERNAME, cptr->name);
      sendto_one(cptr,
          ":%s %d %s :Connect with your real username, in lowercase.",
          me.name, ERR_INVALIDUSERNAME, cptr->name);
      sendto_one(cptr, ":%s %d %s :If your mail address were foo@bar.com, "
          "your username would be foo.",
          me.name, ERR_INVALIDUSERNAME, cptr->name);
      return exit_client(cptr, sptr, &me, "USER: Bad username");
    }
  } /* activar_ident */

    Count_unknownbecomesclient(sptr, nrof);
  }
  else
  {
    SlabStringAllocDup(&(user->username), username, USERLEN);
    Count_newremoteclient(nrof, sptr);
    if (IsService(sptr->user->server))
      nrof.services++;
  }
  SetUser(sptr);

#if defined(ESNET_NEG)
  config_resolve_speculative(cptr);
#endif

  if (IsInvisible(sptr))
    ++nrof.inv_clients;

  if (MyConnect(sptr))
  {
    sendto_one(sptr, rpl_str(RPL_WELCOME), me.name, nick, network ? network : NETWORK_NAME, nick);
    /* This is a duplicate of the NOTICE but see below... */
    sendto_one(sptr, rpl_str(RPL_YOURHOST), me.name, nick, me.name, version);
    sendto_one(sptr, rpl_str(RPL_CREATED), me.name, nick, creation);
    sendto_one(sptr, rpl_str(RPL_MYINFO), me.name, parv[0], me.name, version);
    send_features(sptr, nick);

    m_lusers(sptr, sptr, 1, parv);
    m_users(sptr, sptr, 1, parv);
    update_load();
#if defined(NODEFAULTMOTD)
    m_motd(sptr, NULL, 1, parv);
#else
    m_motd(sptr, sptr, 1, parv);
#endif

    UpdateCheckPing(sptr, get_client_ping(sptr));
    //nextping = now;
    if (sptr->snomask & SNO_NOISY)
      set_snomask(sptr, sptr->snomask & SNO_NOISY, SNO_ADD);
#if defined(ALLOW_SNO_CONNEXIT)
#if defined(SNO_CONNEXIT_IP)
    sprintf_irc(sendbuf,
        ":%s NOTICE * :*** Notice -- Client connecting: %s (%s@%s) [%s] {%d}",
        me.name, nick, PunteroACadena(user->username),
        PunteroACadena(user->host), ircd_ntoa_c(sptr), get_client_class(sptr));
    sendbufto_op_mask(SNO_CONNEXIT);
#else /* SNO_CONNEXIT_IP */
    sprintf_irc(sendbuf,
        ":%s NOTICE * :*** Notice -- Client connecting: %s (%s@%s)",
        me.name, nick, PunteroACadena(user->username),
        PunteroACadena(user->host));
    sendbufto_op_mask(SNO_CONNEXIT);
#endif /* SNO_CONNEXIT_IP */
#endif /* ALLOW_SNO_CONNEXIT */

#if defined(USE_GEOIP2)
    if (geo_enable)
      sendto_debug_channel(canal_connexitdebug, "[IN] %s (%s@%s) [%s - AS%d %s]",
          nick, PunteroACadena(user->username), ircd_ntoa_c(sptr), sptr->country_iso,
          sptr->asnum, PunteroACadena(sptr->asnum_name));
    else
#endif
    sendto_debug_channel(canal_connexitdebug, "[IN] %s (%s@%s) [%s]",
        nick, PunteroACadena(user->username), PunteroACadena(user->host), ircd_ntoa_c(sptr));

    IPcheck_connect_succeeded(sptr);
  }
  else
    /* if (IsServer(cptr)) */
  {
    aClient *acptr;

    acptr = user->server;
    if (acptr->from != sptr->from)
    {
#if !defined(NO_PROTOCOL9)
      if (Protocol(cptr) < 10)
        sendto_one(cptr, ":%s KILL %s :%s (%s != %s[%s])",
            me.name, sptr->name, me.name, user->server->name, acptr->from->name,
            PunteroACadena(acptr->from->sockhost));
      else
#endif
        sendto_one(cptr, "%s " TOK_KILL " %s%s :%s (%s != %s[%s])",
            NumServ(&me), NumNick(sptr), me.name, user->server->name,
            acptr->from->name, PunteroACadena(acptr->from->sockhost));
      sptr->flags |= FLAGS_KILLED;
      return exit_client(cptr, sptr, &me, "NICK server wrong direction");
    }
    else
      sptr->flags |= (acptr->flags & FLAGS_TS8);

    /*
     * Check to see if this user is being propogated
     * as part of a net.burst, or is using protocol 9.
     * FIXME: This can be speeded up - its stupid to check it for
     * every NICK message in a burst again  --Run.
     */
    for (acptr = user->server; acptr != &me; acptr = acptr->serv->up)
      if (IsBurst(acptr) || Protocol(acptr) < 10)
        break;
    if (IPcheck_remote_connect(sptr, PunteroACadena(user->host),
        (acptr != &me)) == -1)
      /* We ran out of bits to count this */
      return exit_client(cptr, sptr, &me,
          "More then 65535 connections from this IP number");
  }
#if defined(NO_PROTOCOL9)       /* Use this when all servers are 2.10 (but test it first) --Run */

  tmpstr = umode_str(sptr, NULL);
  sendto_serv_butone(cptr, *tmpstr ?
      "%s " TOK_NICK " %s %d %d %s %s +%s %s %s%s :%s" :
      "%s " TOK_NICK " %s %d %d %s %s %s%s %s%s :%s",
      NumServ(user->server), nick, sptr->hopcount + 1, sptr->lastnick,
      PunteroACadena(user->username), PunteroACadena(user->host), tmpstr,
      iptobase64(ip_base64, &sptr->ip, sizeof(ip_base64), 1),
      NumNick(sptr), PunteroACadena(sptr->info));

#else /* Remove the following when all servers are 2.10 */

  /* First send message to all 2.9 servers */
  sprintf_irc(sendbuf, ":%s NICK %s %d " TIME_T_FMT " %s %s %s :%s",
      user->server->name, nick, sptr->hopcount + 1, sptr->lastnick,
      PunteroACadena(user->username), PunteroACadena(user->host),
      user->server->name, PunteroACadena(sptr->info));

  for (lp = me.serv->down; lp; lp = lp->next)
  {
    if (lp->value.cptr == cptr)
      continue;
    if (Protocol(lp->value.cptr) < 10)
      sendbufto_one(lp->value.cptr);
  }

  /* If the user has no umode, no need to generate a user MODE */
  if (*(tmpstr = umode_str(sptr, NULL)) && (MyConnect(sptr)
      || Protocol(cptr) > 9))
    /* Is it necessary to generate an user MODE message ? */
  {
    for (lp = me.serv->down; lp; lp = lp->next)
    {
      if (lp->value.cptr == cptr)
        continue;
      if (Protocol(lp->value.cptr) < 10)
        sendto_one(lp->value.cptr, ":%s MODE %s :%s", sptr->name,
            sptr->name, tmpstr);
    }
  }

  /* Now send message to all 2.10 servers */
  sprintf_irc(sendbuf, *tmpstr ?
      "%s " TOK_NICK " %s %d %d %s %s +%s %s %s%s :%s" :
      "%s " TOK_NICK " %s %d %d %s %s %s%s %s%s :%s",
      NumServ(user->server), nick, sptr->hopcount + 1, (int)(sptr->lastnick),
      PunteroACadena(user->username), PunteroACadena(user->host), tmpstr,
      iptobase64(ip_base64, &sptr->ip, sizeof(ip_base64), 1),
      NumNick(sptr), PunteroACadena(sptr->info));

  for (lp = me.serv->down; lp; lp = lp->next)
  {
    if (lp->value.cptr == cptr || Protocol(lp->value.cptr) < 10)
      continue;
    sendbufto_one(lp->value.cptr);
  }

#endif

  /* Send umode to client */
  if (MyUser(sptr))
  {
    send_umode(cptr, sptr, 0, ALL_UMODES, 0, ALL_HMODES);
    if (sptr->snomask != SNO_DEFAULT && (sptr->flags & FLAGS_SERVNOTICE))
      sendto_one(sptr, rpl_str(RPL_SNOMASK), me.name, sptr->name,
          sptr->snomask, sptr->snomask);
  }

  /*
   * Avisamos a sus contactos que el nick
   * ha entrado en la red.
   * (Nuevo usuario local)
   */
  chequea_estado_watch(sptr, RPL_LOGON);

  return 0;
}

/* Envia RAW 005 (RPL_ISUPPORT) */
void send_features(aClient *sptr, char *nick)
{
  char buf[500];

  sprintf(buf, "CHANMODES=b,k,l,imnpstcrORMCNuWz");
  sprintf(buf, "%s CHANTYPES=#&+ KICKLEN=%d MAXBANS=%d", buf, KICKLEN, MAXBANS);
  sendto_one(sptr, rpl_str(RPL_ISUPPORT), me.name, nick, buf);

  sprintf(buf, "MAXCHANNELS=%d CHANNELLEN=%d MAXTARGETS=%d MODES=%d NICKLEN=%d",
      MAXCHANNELSPERUSER, CHANNELLEN, MAXTARGETS, MAXMODEPARAMS, nicklen);
  sendto_one(sptr, rpl_str(RPL_ISUPPORT), me.name, nick, buf);

  sprintf(buf, "PREFIX=(qov).@+ SILENCE=%d TOPICLEN=%d WALLCHOPS WHOX",
      MAXSILES, TOPICLEN);
  sendto_one(sptr, rpl_str(RPL_ISUPPORT), me.name, nick, buf);

  sprintf(buf,
      "USERIP CPRIVMSG CNOTICE CASEMAPPING=rfc1459 NETWORK=%s", network ? network : NETWORK_NAME);
  sendto_one(sptr, rpl_str(RPL_ISUPPORT), me.name, nick, buf);

  sprintf(buf, "MAP SAFELIST QUITLEN=%d AWAYLEN=%d", QUITLEN, AWAYLEN);
#if defined(XMODE_ESNET)
  sprintf(buf, "%s XMODE", buf);
#endif
  sprintf(buf, "%s FNC GHOST", buf);
  sprintf(buf, "%s WATCH=%d", buf, MAXWATCH);
  sendto_one(sptr, rpl_str(RPL_ISUPPORT), me.name, nick, buf);
}

/* *INDENT-OFF* */

static int user_modes[] = {
  FLAGS_OPER,		'o',
  FLAGS_LOCOP,		'O',
  FLAGS_INVISIBLE,	'i',
  FLAGS_WALLOP,		'w',
  FLAGS_SERVNOTICE,	's',
  FLAGS_DEAF,		'd',
  FLAGS_CHSERV,		'k',
  FLAGS_DEBUG,          'g',
  0,			0
};

static int user_hmodes[] = {
  HMODE_NICKREGISTERED, 'r',
  HMODE_NICKSUSPENDED,  'S',
  HMODE_ADMIN,          'a',
  HMODE_CODER,          'C',
  HMODE_HELPOP,         'h',
  HMODE_HIDDEN,         'x',
  HMODE_HIDDENVIEWER,   'X',
  HMODE_VHOSTPERSO,     'v',
  HMODE_SERVICESBOT,    'B',
  HMODE_MSGONLYREG,     'R',
  HMODE_STRIPCOLOR,     'c',
  HMODE_NOCHAN,         'n',
  HMODE_SSL,            'z',
  HMODE_DOCKING,        'K',
  HMODE_NOIDLE,         'I',
  HMODE_WHOIS,          'W',
/* Control Spam */
  HMODE_USERDEAF,       'D',
  HMODE_USERBITCH,      'P',
  HMODE_USERNOJOIN,     'J',
  HMODE_PENDVALIDATION, 'V',
  0,			0
};

/* *INDENT-ON* */

/*
** Devuelve 0 si las claves no valen, !0 si coincide
*/
static int verifica_clave_nick(char *nick, char *hash, char *clave)
{
  unsigned int v[2], k[2], x[2];

  int longitud_nick = strlen(nick);
  /* Para nicks <16 uso cont 2 para el resto lo calculo */
  int cont=(longitud_nick < 16) ? 2 : ((longitud_nick + 8) / 8);

  char tmpnick[8 * cont + 1];
  char tmppass[12 + 1];
  unsigned int *p = (unsigned int *)tmpnick;  /* int == 32bits */
  unsigned int numpass[2];

  memset(tmpnick, 0, sizeof(tmpnick));
  strncpy(tmpnick, nick, sizeof(tmpnick) - 1);

  memset(tmppass, 0, sizeof(tmppass));
  strncpy(tmppass, hash, sizeof(tmppass) - 1);

  numpass[1] = base64toint(tmppass + 6);
  tmppass[6] = '\0';
  numpass[0] = base64toint(tmppass);

  memset(tmppass, 0, sizeof(tmppass));

  strncpy(tmppass, clave, sizeof(tmppass) - 1);

  /* relleno   ->   123456789012 */
  strncat(tmppass, "AAAAAAAAAAAA", sizeof(tmppass) - strlen(tmppass) - 1);

  x[0] = x[1] = 0;

  k[1] = base64toint(tmppass + 6);
  tmppass[6] = '\0';
  k[0] = base64toint(tmppass);

  while (cont--)
  {
    v[0] = ntohl(*p);           /* 32 bits */
    p++;                        /* No se puede hacer de una vez porque puede tratarse de una expansion de macros */
    v[1] = ntohl(*p);           /* 32 bits */
    p++;                        /* No se puede hacer de una vez porque puede tratarse de una expansion de macros */
    tea(v, k, x);
  }

  if ((x[0] == numpass[0]) && (x[1] == numpass[1]))
    return 1;

  return 0;
}


/*
 * add_target
 *
 * sptr must be a local client!
 *
 * Cannonifies target for client `sptr'.
 */
void add_target(aClient *sptr, void *target)
{
  unsigned char *p;
  unsigned int tmp = ((size_t)target & 0xffff00) >> 8;
  unsigned char hash = (tmp * tmp) >> 12;
  if (sptr->targets[0] == hash) /* Last person that we messaged ourself? */
    return;
  for (p = sptr->targets; p < &sptr->targets[MAXTARGETS - 1];)
    if (*++p == hash)
      return;                   /* Already in table */

  /* New target */
  memmove(&sptr->targets[RESERVEDTARGETS + 1],
      &sptr->targets[RESERVEDTARGETS], MAXTARGETS - RESERVEDTARGETS - 1);
  sptr->targets[RESERVEDTARGETS] = hash;
  return;
}

/*
 * check_target_limit
 *
 * sptr must be a local client !
 *
 * Returns 'true' (1) when too many targets are addressed.
 * Returns 'false' (0) when it's ok to send to this target.
 */
int check_target_limit(aClient *sptr, void *target, const char *name,
    int created)
{
  unsigned char *p;
  unsigned int tmp = ((size_t)target & 0xffff00) >> 8;
  unsigned char hash = (tmp * tmp) >> 12;

  if (IsChannelService(sptr) || IsAnOper(sptr) || IsDocking(sptr))
    return 0;

  if (sptr->targets[0] == hash) /* Same target as last time ? */
    return 0;
  for (p = sptr->targets; p < &sptr->targets[MAXTARGETS - 1];)
    if (*++p == hash)
    {
      memmove(&sptr->targets[1], &sptr->targets[0], p - sptr->targets);
      sptr->targets[0] = hash;
      return 0;
    }

  /* New target */
  if (!created)
  {
    if (now < sptr->nexttarget)
    {
      if (sptr->nexttarget - now < TARGET_DELAY + 8)  /* No server flooding */
      {
        sptr->nexttarget += 2;
        sendto_one(sptr, err_str(ERR_TARGETTOOFAST),
            me.name, sptr->name, name, sptr->nexttarget - now);
      }
      return 1;
    }
    else
    {
#if defined(GODMODE)
      sendto_one(sptr, ":%s NOTICE %s :New target: %s; ft " TIME_T_FMT,
          me.name, sptr->name, name, (now - sptr->nexttarget) / TARGET_DELAY);
#endif
      sptr->nexttarget += TARGET_DELAY;
      if (sptr->nexttarget < now - (TARGET_DELAY * (MAXTARGETS - 1)))
        sptr->nexttarget = now - (TARGET_DELAY * (MAXTARGETS - 1));
    }
  }
  memmove(&sptr->targets[1], &sptr->targets[0], MAXTARGETS - 1);
  sptr->targets[0] = hash;
  return 0;
}


/*
 * Elimino colores mIRC de un mensaje, adaptado de strip_color2 de xchat
 *
 * -- FreeMind 2009/02/16
 */
void strip_color (const char *src, int maxlength, char *dst)
{
  int rcol = 0, bgcol = 0;
  int pos=0;

  while (*src && pos<(maxlength - 1))
    {
      if (rcol > 0 && (isdigit ((unsigned char)*src) ||
          (*src == ',' && isdigit ((unsigned char)src[1]) && !bgcol)))
        {
          if (src[1] != ',') rcol--;
          if (*src == ',')
            {
              rcol = 2;
              bgcol = 1;
            }
        } else
          {
            rcol = bgcol = 0;
            switch (*src)
            {
            case '\003':
              rcol = 2;
            case '\010':
            case '\007':
            case '\017':
            case '\026':
            case '\002':
            case '\037':
            case '\035':
              break;
            default:
              *dst++ = *src;
            }
          }
      src++;
      pos++;
    }
  *dst = 0;
}

void spam_set_modes(aClient *sptr)
{
  int of, oh;

  of = sptr->flags;
  oh = sptr->hmodes;

  SetUserDeaf(sptr);
  SetUserBitch(sptr);

  send_umode_out(sptr, sptr, of, oh, IsRegistered(sptr));
}

/*
 * m_message (used in m_private() and m_notice())
 *
 * The general function to deliver MSG's between users/channels
 *
 * parv[0] = sender prefix
 * parv[1] = receiver list
 * parv[parc-1] = message text
 *
 * massive cleanup
 * rev argv 6/91
 */
static int m_message(aClient *cptr, aClient *sptr,
    int parc, char *parv[], int notice)
{
  Reg1 aClient *acptr;
  Reg2 char *s;
  aChannel *chptr;
  char *nick, *server, *p, *cmd, *host;
  char buffer_nocolor[1024];

  sptr->flags &= ~FLAGS_TS8;

  cmd = notice ? MSG_NOTICE : MSG_PRIVATE;

  if (parc < 2 || *parv[1] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NORECIPIENT), me.name, parv[0], cmd);
    return -1;
  }

  if (parc < 3 || *parv[parc - 1] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
    return -1;
  }

  if (MyUser(sptr))
    parv[1] = canonize(parv[1]);
  for (p = NULL, nick = strtoken(&p, parv[1], ","); nick;
      nick = strtoken(&p, NULL, ","))
  {
    /*
     * channel msg?
     */
    if (IsChannelName(nick))
    {
      if ((chptr = FindChannel(nick)))
      {
        if (can_send(sptr, chptr) == 0  /* This first: Almost never a server/service */
            || IsChannelService(sptr) || IsServer(sptr))
        {
          if (MyUser(sptr) && (chptr->mode.mode & MODE_NOPRIVMSGS) &&
              check_target_limit(sptr, chptr, chptr->chname, 0))
            continue;
          if (MyUser(sptr) && (chptr->mode.mode & MODE_NOCTCP) &&
              (*parv[parc - 1] == 1) && strncmp(parv[parc - 1], "\001ACTION ", 8) &&
              !IsChannelService(sptr))
          {
            sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
                me.name, parv[0], chptr->chname);
            continue;
          }
          if (MyUser(sptr) && notice && (chptr->mode.mode & MODE_NONOTICE) && !IsChannelService(sptr))
          {
            sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
                me.name, parv[0], chptr->chname);
            continue;
          }

          if (IsUserDeaf(sptr))
            continue;
          if(chptr->mode.mode & MODE_NOCOLOUR) {
            /* Calcula el color solo una vez */
            strip_color(parv[parc-1], sizeof(buffer_nocolor), buffer_nocolor);

            sendto_channel_color_butone(cptr, sptr, chptr,
                ":%s %s %s :%s", parv[0], cmd, chptr->chname, parv[parc - 1]);
            sendto_channel_nocolor_butone(cptr, sptr, chptr,
                ":%s %s %s :%s", parv[0], cmd, chptr->chname, buffer_nocolor);
          } else {
            sendto_channel_butone(cptr, sptr, chptr,
                ":%s %s %s :%s", parv[0], cmd, chptr->chname, parv[parc - 1]);

          }
        }
        else                    /* if (!notice) */
          /* Enviamos el mensaje tambien SI es notice */
          sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
              me.name, parv[0], chptr->chname);
        continue;
      }
    }
    else if (*nick != '$' && !strchr(nick, '@'))
    {
      /*
       * nickname addressed?
       */
      if (MyUser(sptr) || Protocol(cptr) < 10)
        acptr = FindUser(nick);
      else if ((acptr = findNUser(nick)) && !IsUser(acptr))
        acptr = NULL;
      if (acptr)
      {
        if (MyUser(sptr) && check_target_limit(sptr, acptr, acptr->name, 0))
          continue;

        /* Los +P solo reciben si viene de un ircop o un clon */
        if (MyUser(sptr) && !IsOper(sptr) && irc_in_addr_cmp(&sptr->ip, &acptr->ip)
            && IsUserBitch(acptr))
          continue;

        /* Los +P solo mandan a un clon o a un Bot */
        if (MyUser(sptr) && IsUserBitch(sptr) && irc_in_addr_cmp(&sptr->ip, &acptr->ip) && !IsServicesBot(acptr))
          continue;

        if (MyUser(sptr) && IsMsgOnlyReg(acptr) && !IsNickRegistered(sptr)
            && !IsAnOper(sptr))
        {
          sendto_one(sptr, err_str(ERR_NONONREG), me.name, parv[0],
              acptr->name);
          continue;
        }
        if (!is_silenced(sptr, acptr))
        {
          if (!notice && MyConnect(sptr) && acptr->user && acptr->user->away)
            sendto_one(sptr, rpl_str(RPL_AWAY),
                me.name, parv[0], acptr->name, acptr->user->away);
          if (MyUser(acptr) || Protocol(acptr->from) < 10)
          {
            if (MyUser(acptr))
              add_target(acptr, sptr);
            sendto_prefix_one(acptr, sptr, ":%s %s %s :%s",
               parv[0], cmd, acptr->name, parv[parc - 1]);
          }
          else {
            if (IsServer(sptr))
              sendto_prefix_one(acptr, sptr, "%s %s %s%s :%s",
                  NumServ(sptr), notice ? TOK_NOTICE : TOK_PRIVATE,
                  NumNick(acptr), parv[parc - 1]);
            else
              sendto_prefix_one(acptr, sptr, "%s%s %s %s%s :%s",
                  NumNick(sptr), notice ? TOK_NOTICE : TOK_PRIVATE,
                  NumNick(acptr), parv[parc - 1]);
          }
        }
        else
        {
          sendto_one(sptr, err_str(ERR_ISSILENCING), me.name, sptr->name,
              acptr->name);
        }
      }
      else if (MyUser(sptr) || Protocol(cptr) < 10)
        sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], nick);
      else
      {
/*
** jcea@argo.es - u2.10.H.05.94
** No podemos devolver el NICK porque
** aqui solo tenemos el "numeric", y si
** el usuario ya se ha desconectado, como
** es el caso, pues no podemos hacer
** la conversion "numeric" -> "nick".
*/
        sendto_one(sptr,
            ":%s %d %s * :Target left IRC. Failed to deliver: [%.50s]",
            me.name, ERR_NOSUCHNICK, sptr->name, parv[parc - 1]);
      }
      continue;
    }
    /*
     * The following two cases allow masks in NOTICEs
     * (for OPERs only)
     *
     * Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
     */
    if ((*nick == '$' || *nick == '#') && IsAnOper(sptr))
    {
      if (MyConnect(sptr))
      {
        if (!(s = strrchr(nick, '.')))
        {
          sendto_one(sptr, err_str(ERR_NOTOPLEVEL), me.name, parv[0], nick);
          continue;
        }
        while (*++s)
          if (*s == '.' || *s == '*' || *s == '?')
            break;
        if (*s == '*' || *s == '?')
        {
          sendto_one(sptr, err_str(ERR_WILDTOPLEVEL), me.name, parv[0], nick);
          continue;
        }
      }
/*
 * jcea@argo.es - 1999/12/16:
 *
 * OJO: PATRON A "PIN~ON FIJO"
 */
      sendto_match_butone(IsServer(cptr) ? cptr : NULL,
          sptr, nick + 1, (*nick == '#') ? MATCH_HOST : MATCH_SERVER,
          parv[0], cmd, nick, sptr->name, nick, parv[parc - 1]);
      continue;
    }
    else if ((server = strchr(nick, '@')) && (acptr = FindServer(server + 1)))
    {
      /*
       * NICK[%host]@server addressed? See if <server> is me first
       */
      if (!IsMe(acptr))
      {
        sendto_one(acptr, ":%s %s %s :%s", parv[0], cmd, nick, parv[parc - 1]);
        continue;
      }

      /* Look for an user whose NICK is equal to <nick> and then
       * check if it's hostname matches <host> and if it's a local
       * user. */
      *server = '\0';
      if ((host = strchr(nick, '%')))
        *host++ = '\0';

      if ((!(acptr = FindUser(nick))) ||
          (!(MyUser(acptr))) ||
          ((!(BadPtr(host))) && match(host, PunteroACadena(acptr->user->host))))
        acptr = NULL;

      *server = '@';
      if (host)
        *--host = '%';

      if (acptr)
      {
        /* Los +P solo reciben si viene de un ircop o un clon */
        if (!IsOper(sptr) && irc_in_addr_cmp(&sptr->ip, &acptr->ip)
            && IsUserBitch(acptr))
          continue;

        /* Los +P solo mandan a un clon */
        if (IsUserBitch(sptr) && irc_in_addr_cmp(&sptr->ip, &acptr->ip))
          continue;

        if (IsMsgOnlyReg(acptr) && !IsNickRegistered(sptr)
            && !IsAnOper(sptr))
        {
          sendto_one(sptr, err_str(ERR_NONONREG), me.name, parv[0],
              acptr->name);
          continue;
        }

        if (!(is_silenced(sptr, acptr))) {
          sendto_prefix_one(acptr, sptr, ":%s %s %s :%s",
              parv[0], cmd, nick, parv[parc - 1]);
        } else {
          sendto_one(sptr, err_str(ERR_ISSILENCING), me.name, sptr->name,
              acptr->name);
        }
        continue;
      }
    }
    if (IsChannelName(nick))
      sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL), me.name, parv[0], nick);
    else
      sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], nick);
  }
  return 0;
}

/*
 * m_private
 *
 * parv[0] = sender prefix
 * parv[1] = receiver list
 * parv[parc-1] = message text
 */
int m_private(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  return m_message(cptr, sptr, parc, parv, 0);
}

/*
 * m_notice
 *
 * parv[0] = sender prefix
 * parv[1] = receiver list
 * parv[parc-1] = notice text
 */
int m_notice(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  if (MyUser(sptr) && parv[1] && parv[1][0] == '@' &&
      IsChannelName(&parv[1][1]))
  {
    parv[1]++;                  /* Get rid of '@' */
    return m_wallchops(cptr, sptr, parc, parv);
  }
  return m_message(cptr, sptr, parc, parv, 1);
}


/*
 * whisper - called from m_cnotice and m_cprivmsg.
 *
 * parv[0] = sender prefix
 * parv[1] = nick
 * parv[2] = #channel
 * parv[3] = Private message text
 *
 * Added 971023 by Run.
 * Reason: Allows channel operators to sent an arbitrary number of private
 *   messages to users on their channel, avoiding the max.targets limit.
 *   Building this into m_private would use too much cpu because we'd have
 *   to a cross channel lookup for every private message!
 * Note that we can't allow non-chan ops to use this command, it would be
 *   abused by mass advertisers.
 */
int whisper(aClient *sptr, int parc, char *parv[], int notice)
{
  int s_is_member = 0, s_is_voiced = 0, t_is_member = 0;
  aClient *tcptr;
  aChannel *chptr;
  Link *lp;

  if (!MyUser(sptr))
    return 0;
  if (parc < 4 || BadPtr(parv[3]))
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
        me.name, parv[0], notice ? "CNOTICE" : "CPRIVMSG");
    return 0;
  }
  if (!(chptr = FindChannel(parv[2])))
  {
    sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL), me.name, parv[0], parv[2]);
    return 0;
  }
  if (!(tcptr = FindUser(parv[1])))
  {
    sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
    return 0;
  }
  for (lp = chptr->members; lp; lp = lp->next)
  {
    aClient *mcptr = lp->value.cptr;
    if (mcptr == sptr)
    {
      s_is_member = 1;
      if ((lp->flags & (CHFL_CHANOP | CHFL_VOICE)))
        s_is_voiced = 1;
      else
        break;
      if (t_is_member)
        break;
    }
    if (mcptr == tcptr)
    {
      t_is_member = 1;
      if (s_is_voiced)
        break;
    }
  }
  if (!s_is_voiced)
  {
    sendto_one(sptr, err_str(s_is_member ? ERR_VOICENEEDED : ERR_NOTONCHANNEL),
        me.name, parv[0], chptr->chname);
    return 0;
  }
  if (!t_is_member)
  {
    sendto_one(sptr, err_str(ERR_USERNOTINCHANNEL),
        me.name, parv[0], tcptr->name, chptr->chname);
    return 0;
  }
  if (MyUser(sptr) && IsMsgOnlyReg(tcptr) && !IsNickRegistered(sptr)
      && !IsAnOper(sptr))
  {
    sendto_one(sptr, err_str(ERR_NONONREG), me.name, parv[0],
        tcptr->name);
    return 0;
  }
  if (is_silenced(sptr, tcptr))
  {
    sendto_one(sptr, err_str(ERR_ISSILENCING), me.name, sptr->name,
        tcptr->name);
    return 0;
  }

  if (tcptr->user && tcptr->user->away)
    sendto_one(sptr, rpl_str(RPL_AWAY),
        me.name, parv[0], tcptr->name, tcptr->user->away);
  if (MyUser(tcptr) || Protocol(tcptr->from) < 10)
    sendto_prefix_one(tcptr, sptr, ":%s %s %s :%s",
        parv[0], notice ? "NOTICE" : "PRIVMSG", tcptr->name, parv[3]);
  else
    sendto_prefix_one(tcptr, sptr, "%s%s %s %s%s :%s",
         NumNick(sptr), notice ? TOK_NOTICE : TOK_PRIVATE, NumNick(tcptr), parv[3]);
  return 0;
}

int m_cnotice(aClient *UNUSED(cptr), aClient *sptr, int parc, char *parv[])
{
  return whisper(sptr, parc, parv, 1);
}

int m_cprivmsg(aClient *UNUSED(cptr), aClient *sptr, int parc, char *parv[])
{
  return whisper(sptr, parc, parv, 0);
}

/*
 * m_wallchops
 *
 * parv[0] = sender prefix
 * parv[1] = target channel
 * parv[parc - 1] = wallchops text
 */
int m_wallchops(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aChannel *chptr;

  sptr->flags &= ~FLAGS_TS8;

  if (parc < 2 || *parv[1] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NORECIPIENT), me.name, parv[0], "WALLCHOPS");
    return -1;
  }

  if (parc < 3 || *parv[parc - 1] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
    return -1;
  }

  if (MyUser(sptr))
    parv[1] = canonize(parv[1]);

  if (IsChannelName(parv[1]) && (chptr = FindChannel(parv[1])))
  {
    if (can_send(sptr, chptr) == 0)
    {
      if (MyUser(sptr) && (chptr->mode.mode & MODE_NOPRIVMSGS) &&
          check_target_limit(sptr, chptr, chptr->chname, 0))
        return 0;
      /* Send to local clients: */
      if (IsUser(sptr))
        sendto_lchanops_butone(cptr, sptr, chptr,
            ":%s NOTICE @%s :%s", parv[0], parv[1], parv[parc - 1]);
      else
        sendto_lchanops_butone(cptr, sptr, chptr,
            ":%s NOTICE @%s :%s", (ocultar_servidores) ? his.name : parv[0], parv[1], parv[parc - 1]);
#if defined(NO_PROTOCOL9)
      /* And to other servers: */
      sendto_chanopsserv_butone(cptr, sptr, chptr,
          ":%s WC %s :%s", parv[0], parv[1], parv[parc - 1]);
#else
      /*
       * WARNING: `sendto_chanopsserv_butone' is heavily hacked when
       * `NO_PROTOCOL9' is not defined ! Therefore this is the ONLY
       * place you may use `sendto_chanopsserv_butone', until all
       * servers are 2.10.
       */
      sendto_chanopsserv_butone(cptr, sptr, chptr,
          ":%s WC %s :%s", parv[0], parv[1], parv[parc - 1]);
#endif
    }
    else
      sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
          me.name, parv[0], parv[1]);
  }
  else
    sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL), me.name, parv[0], parv[1]);

  return 0;
}

/*
 * m_user
 *
 * parv[0] = sender prefix
 * parv[1] = username (login name, account)
 * parv[2] = umode mask
 * parv[3] = server notice mask
 * parv[4] = users real name info
 */
int m_user(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
#define UFLAGS	(FLAGS_INVISIBLE|FLAGS_WALLOP|FLAGS_SERVNOTICE)
  char *username, *host, *server, *realname;
  anUser *user;

  if (IsServer(cptr))
    return 0;

  if (IsServerPort(cptr))
    return exit_client(cptr, cptr, &me, "Use a different port");

  if (parc > 2 && (username = strchr(parv[1], '@')))
    *username = '\0';
  if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
      *parv[3] == '\0' || *parv[4] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "USER");
    return 0;
  }

  /* Copy parameters into better documenting variables */

  username = (parc < 2 || BadPtr(parv[1])) ? "<bad-boy>" : parv[1];
  host = (parc < 3 || BadPtr(parv[2])) ? "<nohost>" : parv[2];
  server = (parc < 4 || BadPtr(parv[3])) ? "<noserver>" : parv[3];
  realname = (parc < 5 || BadPtr(parv[4])) ? "<bad-realname>" : parv[4];

  user = make_user(sptr);

  if (!IsUnknown(sptr))
  {
    sendto_one(sptr, err_str(ERR_ALREADYREGISTRED), me.name, parv[0]);
    return 0;
  }

  if (!strchr(host, '.'))       /* Not an IP# as hostname ? */
    sptr->flags |= (UFLAGS & atoi(host));
  if ((sptr->flags & FLAGS_SERVNOTICE))
    set_snomask(sptr, (isDigit(*server) && !strchr(server, '.')) ?
        (atoi(server) & SNO_USER) : SNO_DEFAULT, SNO_SET);
  user->server = &me;
  SlabStringAllocDup(&(sptr->info), realname, REALLEN);

  if (sptr->name && IsCookieVerified(sptr))
    /* NICK and PONG already received, now we have USER... */
    return register_user(cptr, sptr, sptr->name, username);
  else
  {
    SlabStringAllocDup(&(sptr->user->username), username, USERLEN);
    SlabStringAllocDup(&(user->host), host, HOSTLEN);
  }
  return 0;
}

/*
 * m_webirc
 *
 * parv[0] = sender prefix
 * parv[1] = password   (linea W o tabla w)
 * parv[2] = username   (cgiirc por defecto)
 * parv[3] = hostname   (Hostname)
 * parv[4] = IP         (IP en formato humano)
 */
int m_webirc(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  struct ConfItem *aconf;
  char *password;
#if defined(CRYPT_OPER_PASSWORD)
  char salt[3];
#endif /* CRYPT_OPER_PASSWORD */

  if (IsRegistered(sptr))
    return 0;

  if (IsServer(sptr))
    return 0;

  if (IsServerPort(sptr))
    return exit_client(sptr, sptr, &me, "Use a different port");

  if (parc < 5)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "USER");
    return 0;
  }

  password = parv[1];

  /* Comprobamos las lineas W */
  aconf = find_conf_exact("*", sptr->username,
                          sptr->sockhost, CONF_WEBIRC);

  if (!aconf)
    aconf = find_conf_exact("*", sptr->username,
                            ircd_ntoa_c(sptr), CONF_WEBIRC);
  if (aconf)
  {
    char *encr;
    /* Comprobamos pass */
#if defined(CRYPT_OPER_PASSWORD)
    /* use first two chars of the password they send in as salt */

    /* passwd may be NULL. Head it off at the pass... */
    salt[0] = '\0';
    if (password && aconf->passwd)
    {
      salt[0] = aconf->passwd[0];
      salt[1] = aconf->passwd[1];
      salt[2] = '\0';
      encr = crypt(password, salt);
    }
    else
      encr = "";
#else
    encr = password;
#endif /* CRYPT_OPER_PASSWORD */

    if (strcmp(encr, aconf->passwd))
      return exit_client(sptr, sptr, &me, "WEBIRC Password invalid for your host");

  } else {
    struct db_reg *reg;

    /* Comprobamos tabla w BDD */
    reg = db_buscar_registro(BDD_WEBIRCDB, sptr->sockhost);
    if (!reg)
      reg = db_buscar_registro(BDD_WEBIRCDB, ircd_ntoa_c(sptr));


    if (!reg)
      return exit_client(sptr, sptr, &me, "WEBIRC Not authorized from your host");

    if (*reg->valor == '{')
    {
      /* Formato nuevo JSON */
      json_object *json, *json_pass;
      enum json_tokener_error jerr = json_tokener_success;
      char *pass;

      json = json_tokener_parse_verbose(reg->valor, &jerr);
      if (jerr != json_tokener_success)
        return exit_client(sptr, sptr, &me, "WEBIRC Bad JSON Format from your host");;

      json_object_object_get_ex(json, "pass", &json_pass);
      pass = (char *)json_object_get_string(json_pass);

      if (!pass)
        return exit_client(sptr, sptr, &me, "WEBIRC No password for your host");

      if (strcmp(password, pass))
        return exit_client(sptr, sptr, &me, "WEBIRC Password invalid for your host");
    }
    else
    {
      /* Formato antiguo */
      if (strcmp(password, reg->valor))
        return exit_client(sptr, sptr, &me, "WEBIRC Password invalid for your host");
    }
  }

  /* acceso concedido */

  /* Eliminamos registro de clones */
  IPcheck_connect_fail(sptr);
  IPcheck_disconnect(sptr);

  if (strIsIrcIp(parv[4])) {
    /* Prioridad IPv4 en IP */
    struct in_addr webirc_addr4;

    inet_aton(parv[4], &webirc_addr4);

    memset(&sptr->ip, 0, sizeof(struct irc_in_addr));
    sptr->ip.in6_16[5] = htons(65535);
    sptr->ip.in6_16[6] = htons(ntohl(webirc_addr4.s_addr) >> 16);
    sptr->ip.in6_16[7] = htons(ntohl(webirc_addr4.s_addr) & 65535);
  } else {
    /* IPv6 */
    struct sockaddr_in6 webirc_addr6;

    inet_pton(AF_INET6, parv[4], &(webirc_addr6.sin6_addr));
    memcpy(&(sptr->ip), &(webirc_addr6.sin6_addr), sizeof(struct irc_in_addr));
  }

  /* Volvemos a meter registro de clones con IP nueva */
  /* OJO, no usamos throttle */
  IPcheck_local_connect(sptr);

  SlabStringAllocDup(&(sptr->sockhost), parv[3], HOSTLEN);
  SetWebIRC(sptr);

  return 0;
}

/*
 * m_proxy
 *
 * parv[0] = prefix
 * parv[1] = protocolo (TCP4, TCP6 o UNKNOWN)
 * parv[2] = direccion ip del cliente origen
 * parv[3] = direccion ip del servidor proxy
 * parv[4] = puerto origen del cliente origen
 * parv[5] = puerto destino del servidor proxy
 *
 * Ver: http://www.haproxy.org/download/1.5/doc/proxy-protocol.txt
 */
int m_proxy(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  struct db_reg *reg;

  if (IsRegistered(sptr))
    return 0;

  if (IsWebIRC(sptr) || IsProxy(sptr))
    return 0;

  if (IsServer(sptr))
    return 0;

  if (IsServerPort(sptr))
    return exit_client(sptr, sptr, &me, "Use a different port");

  if (parc < 6)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "PROXY");
    return 0;
  }

  /*
   * Buscamos el registro en la tabla y, que es la tabla de las
   * ips autorizadas a conectar utilizando el protocolo PROXY.
   */
  reg = db_buscar_registro(BDD_PROXYDB, sptr->sockhost);
  if (!reg)
    return exit_client(sptr, sptr, &me, "PROXY Not authorized from your address");

  IPcheck_connect_fail(sptr);
  IPcheck_disconnect(sptr);

  if (strcmp(parv[1], "TCP4") == 0)
  {
    struct in_addr inaddr;

    inet_aton(parv[2], &inaddr);

    memset(&sptr->ip, 0, sizeof(struct irc_in_addr));
    sptr->ip.in6_16[5] = htons(65535);
    sptr->ip.in6_16[6] = htons(ntohl(inaddr.s_addr) >> 16);
    sptr->ip.in6_16[7] = htons(ntohl(inaddr.s_addr) & 65535);
  }
  else if (strcmp(parv[1], "TCP6") == 0) {
#if defined(IPV6)
    struct sockaddr_in6 inaddr6;
    inet_pton(AF_INET6, parv[2], &(inaddr6.sin6_addr));
    memcpy(&(sptr->ip), &(inaddr6.sin6_addr), sizeof(struct irc_in_addr));
#else
    return exit_client(sptr, sptr, &me, "PROXY TCP6 protocol is not supported");
#endif
  }
  else {
    return exit_client(sptr, sptr, &me, "PROXY UNKNOWN protocol");
  }

  IPcheck_local_connect(sptr);

  SlabStringAllocDup(&(sptr->sockhost), parv[2], HOSTLEN);
  SetProxy(sptr);

  return 0;
}


/*
 * m_quit
 *
 * cptr = Quien manda el mensaje de quit
 * sptr = Quien recibe el QUIT
 *
 * parv[0] = sender prefix
 * parv[parc-1] = comment
 */
int m_quit(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  char *comment = (parc > 1 && !BadPtr(parv[parc - 1])) ? parv[parc - 1] : NULL;

  assert(0 != cptr);
  assert(0 != sptr);

  if(IsServer(sptr)) /* Un quit no puede tener de objeivo un servidor */
    return 0;

  if (MyConnect(sptr)) /* Si es una conexion local */
  {
    if(IsRegistered(sptr) && mensaje_quit_personalizado)
      return exit_client(cptr, sptr, &me, mensaje_quit_personalizado);

    if (sptr->user)
    {
      Link *lp;
      for (lp = sptr->user->channel; lp; lp = lp->next)
        if ((can_send(sptr, lp->value.chptr) != 0) || (lp->value.chptr->mode.mode & MODE_NOQUITPARTS))
          return exit_client(cptr, sptr, &me, "Signed off");
    }
    if (comment)
    {
      if(strlen(comment)>QUITLEN)
        comment[QUITLEN]='\0';
 
      return exit_client_msg(cptr, sptr, &me, "Quit: %s", comment);
    }
    else
      return exit_client(cptr, sptr, &me, "Quit");
  }

  /* Si es un quit externo lo reenviamos tal cual */
  return exit_client(cptr, sptr, sptr, comment ? comment : "\0");
}

/*
 * m_kill
 *
 * parv[0] = sender prefix
 * parv[1] = kill victim
 * parv[parc-1] = kill path
 */
int m_kill(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;
  char *inpath = ocultar_servidores ? his.name : cptr->name;
  char *user, *path, *killer;
  int chasing = 0;

  if (!IsPrivileged(cptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }

  if (parc < 3 || *parv[1] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "KILL");
    return 0;
  }

  user = parv[1];
  path = parv[parc - 1];        /* Either defined or NULL (parc >= 3) */

  if (IsAnOper(cptr))
  {
    if (BadPtr(path))
    {
      sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "KILL");
      return 0;
    }
    if (strlen(path) > (size_t)QUITLEN)
      path[QUITLEN] = '\0';
  }

  if (MyUser(sptr) || Protocol(cptr) < 10)
  {
    if (!(acptr = FindClient(user)))
    {
      /*
       * If the user has recently changed nick, we automaticly
       * rewrite the KILL for this new nickname--this keeps
       * servers in synch when nick change and kill collide
       */
      if (!(acptr = get_history(user, (long)15)))
      {
        sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], user);
        return 0;
      }
      sendto_one(sptr, ":%s NOTICE %s :Changed KILL %s into %s",
          me.name, parv[0], user, acptr->name);
      chasing = 1;
    }
  }
  else if (!(acptr = findNUser(user)))
  {
    if (Protocol(cptr) < 10 && IsUser(sptr))
      sendto_one(sptr,
          ":%s NOTICE %s :KILL target disconnected before I got him :(",
          me.name, parv[0]);
    else if (IsUser(sptr))
      sendto_one(sptr,
          "%s " TOK_NOTICE " %s%s :KILL target disconnected before I got him :(",
          NumServ(&me), NumNick(sptr));
    return 0;
  }
  if (MyUser(sptr) && !HasPriv(sptr, MyConnect(acptr) ? PRIV_LOCAL_KILL : PRIV_KILL))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }
  if (IsServer(acptr) || IsMe(acptr))
  {
    sendto_one(sptr, err_str(ERR_CANTKILLSERVER), me.name, parv[0]);
    return 0;
  }

  if (MyUser(sptr) && !MyConnect(acptr) && !HasPriv(sptr, PRIV_KILL))
  {
    sendto_one(sptr, ":%s NOTICE %s :Nick %s isnt on your server",
        me.name, parv[0], acptr->name);
    return 0;
  }

  if (!IsServer(cptr))
  {
    /*
     * The kill originates from this server, initialize path.
     * (In which case the 'path' may contain user suplied
     * explanation ...or some nasty comment, sigh... >;-)
     *
     * ...!operhost!oper
     * ...!operhost!oper (comment)
     */
    if (IsUnixSocket(cptr))     /* Don't use get_client_name syntax */
      inpath = PunteroACadena(me.sockhost);
    else
    {
#if defined(BDD_VIP)
      inpath = get_visiblehost(cptr, NULL, 0);
#else
      inpath = PunteroACadena(cptr->user->host);
#endif
    }
    if (!BadPtr(path))
    {
      sprintf_irc(buf,
          "%s%s (%s)", cptr->name, IsOper(sptr) ? "" : "(L)", path);
      path = buf;
    }
    else
      path = cptr->name;
  }
  else if (BadPtr(path))
    path = "*no-path*";         /* Bogus server sending??? */
  /*
   * Notify all *local* opers about the KILL (this includes the one
   * originating the kill, if from this server--the special numeric
   * reply message is not generated anymore).
   *
   * Note: "acptr->name" is used instead of "user" because we may
   *       have changed the target because of the nickname change.
   */
  if (IsLocOp(sptr) && !MyConnect(acptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }
  sendto_op_mask(IsServer(sptr) ? SNO_SERVKILL : SNO_OPERKILL,
      "Received KILL message for %s. From %s Path: %s!%s",
      acptr->name, parv[0], inpath, path);
#if defined(USE_SYSLOG) && defined(SYSLOG_KILL)
  if (MyUser(acptr))
  {                             /* get more infos when your local
                                   clients are killed -- _dl */
    if (IsServer(sptr))
      syslog(LOG_DEBUG,
          "A local client %s!%s@%s KILLED from %s [%s] Path: %s!%s)",
          acptr->name, PunteroACadena(acptr->user->username),
          PunteroACadena(acptr->user->host), parv[0], sptr->name, inpath, path);
    else
      syslog(LOG_DEBUG,
          "A local client %s!%s@%s KILLED by %s [%s!%s@%s] (%s!%s)",
          acptr->name, PunteroACadena(acptr->user->username),
          PunteroACadena(acptr->user->host), parv[0], sptr->name,
          PunteroACadena(sptr->user->username),
          PunteroACadena(sptr->user->host), inpath, path);
  }
  else if (IsOper(sptr))
    syslog(LOG_DEBUG, "KILL From %s For %s Path %s!%s",
        parv[0], acptr->name, inpath, path);
#endif
  /*
   * And pass on the message to other servers. Note, that if KILL
   * was changed, the message has to be sent to all links, also
   * back.
   * Suicide kills are NOT passed on --SRB
   */
  if (!MyConnect(acptr) || !MyConnect(sptr) || !IsAnOper(sptr))
  {
#if !defined(NO_PROTOCOL9)
    sendto_lowprot_butone(cptr, 9, ":%s KILL %s :%s!%s",
        parv[0], acptr->name, inpath, path);
#endif
    sendto_highprot_butone(cptr, 10, ":%s " TOK_KILL " %s%s :%s!%s",
        parv[0], NumNick(acptr), inpath, path);
#if !defined(NO_PROTOCOL9)
    if (chasing && IsServer(cptr))  /* Can be removed when all are Protocol 10 */
      sendto_one(cptr, ":%s KILL %s :%s!%s",
          me.name, acptr->name, inpath, path);
#endif
    /* We *can* have crossed a NICK with this numeric... --Run */
    /* Note the following situation:
     *  KILL SAA -->       X
     *  <-- S NICK ... SAA | <-- SAA QUIT <-- S NICK ... SAA <-- SQUIT S
     * Where the KILL reaches point X before the QUIT does.
     * This would then *still* cause an orphan because the KILL doesn't reach S
     * (because of the SQUIT), the QUIT is ignored (because of the KILL)
     * and the second NICK ... SAA causes an orphan on the server at the
     * right (which then isn't removed when the SQUIT arrives).
     * Therefore we still need to detect numeric nick collisions too.
     */
    if (MyConnect(acptr) && IsServer(cptr) && Protocol(cptr) > 9)
      sendto_one(cptr, "%s " TOK_KILL " %s%s :%s!%s (Ghost5)",
          NumServ(&me), NumNick(acptr), inpath, path);
    acptr->flags |= FLAGS_KILLED;
  }

  /*
   * Tell the victim she/he has been zapped, but *only* if
   * the victim is on current server--no sense in sending the
   * notification chasing the above kill, it won't get far
   * anyway (as this user don't exist there any more either)
   */
  if (MyConnect(acptr))
    sendto_prefix_one(acptr, sptr, ":%s KILL %s :%s!%s",
        parv[0], acptr->name, inpath, path);
  /*
   * Set FLAGS_KILLED. This prevents exit_one_client from sending
   * the unnecessary QUIT for this. (This flag should never be
   * set in any other place)
   */
  if ((!ocultar_servidores) && MyConnect(acptr) && MyConnect(sptr) && IsAnOper(sptr))
    sprintf_irc(buf2, "Local kill by %s (%s)", sptr->name,
        BadPtr(parv[parc - 1]) ? sptr->name : parv[parc - 1]);
  else
  {
    if ((killer = strchr(path, ' ')))
    {
      while (*killer && *killer != '!')
        killer--;
      if (!*killer)
        killer = path;
      else
        killer++;
    }
    else
      killer = path;
    sprintf_irc(buf2, "Killed (%s)", killer);
  }
  return exit_client(cptr, acptr, sptr, buf2);
}

/*
 * m_away                               - Added 14 Dec 1988 by jto.
 *
 * parv[0] = sender prefix
 * parv[1] = away message
 */
int m_away(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Reg1 char *away, *awy2 = parv[1];

  away = sptr->user->away;

  if (parc < 2 || !*awy2)
  {
    /* Marking as not away */
    if (away)
    {
      RunFree(away);
      sptr->user->away = NULL;
    }
#if !defined(NO_PROTOCOL9)
    sendto_lowprot_butone(cptr, 9, ":%s AWAY", parv[0]);
#endif
    sendto_highprot_butone(cptr, 10, "%s%s " TOK_AWAY, NumNick(sptr));
    if (MyConnect(sptr))
      sendto_one(sptr, rpl_str(RPL_UNAWAY), me.name, parv[0]);
    return 0;
  }

  /* Marking as away */

  if (strlen(awy2) > (size_t)AWAYLEN)
    awy2[AWAYLEN] = '\0';
#if !defined(NO_PROTOCOL9)
  sendto_lowprot_butone(cptr, 9, ":%s AWAY :%s ", parv[0], awy2);
#endif
  sendto_highprot_butone(cptr, 10, "%s%s " TOK_AWAY " :%s ", NumNick(sptr), awy2);

  if (away)
    away = (char *)RunRealloc(away, strlen(awy2) + 1);
  else
    away = (char *)RunMalloc(strlen(awy2) + 1);

  sptr->user->away = away;
  strcpy(away, awy2);
  if (MyConnect(sptr))
    sendto_one(sptr, rpl_str(RPL_NOWAWAY), me.name, parv[0]);
  return 0;
}

/*
 * m_ping
 *
 * parv[0] = sender prefix
 * parv[1] = origin
 * parv[2] = destination
 */
int m_ping(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;
  char *origin, *destination;

  if (parc < 2 || *parv[1] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
    return 0;
  }
  origin = parv[1];
  destination = parv[2];        /* Will get NULL or pointer (parc >= 2!!) */

  acptr = FindClient(origin);
  if (acptr && acptr != sptr)
    origin = cptr->name;

  if (!BadPtr(destination) && strCasediff(destination, me.name) != 0)
  {
    if ((acptr = FindServer(destination)))
    {
      if (Protocol(acptr) < 10)
        sendto_one(acptr, ":%s PING %s :%s", parv[0], origin, destination);
      else {
        if (IsUser(sptr))
          sendto_one(acptr, "%s%s " TOK_PING " %s :%s", NumNick(sptr), origin, destination);
        else
          sendto_one(acptr, "%s " TOK_PING " %s :%s", NumServ(sptr), origin, destination);
      }
    }
    else
    {
      sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
          me.name, parv[0], destination);
      return 0;
    }
  }
  else
    sendto_one(sptr, ":%s PONG %s :%s", me.name, me.name, origin);
  return 0;
}

/*
 * m_pong
 *
 * parv[0] = sender prefix
 * parv[1] = origin
 * parv[2] = destination
 */
int m_pong(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;
  char *origin, *destination;

  if (MyUser(sptr))
    return 0;

  /* Check to see if this is a PONG :cookie reply from an
   * unregistered user.  If so, process it. -record       */

  if ((!IsRegistered(sptr)) && (sptr->cookie != 0) &&
      (!IsCookieVerified(sptr)) && (parc > 1))
  {
    if (atol(parv[parc - 1]) == (long)sptr->cookie)
    {
/*
** Si el usuario tiene pendiente de verificar la cookie es porque ya ha mandado su nick,
** por tanto debe existir forzosamente sptr->name - RyDeN
*/
      assert(sptr->name);

      SetCookieVerified(sptr);
      if (sptr->user && sptr->user->host) /* NICK and USER OK */
        return register_user(cptr, sptr, sptr->name,
            PunteroACadena(sptr->user->username));
    }
    else
    {
      if(IsCookieEncrypted(sptr))
        return exit_client(cptr, sptr, &me, "Invalid PONG message");
      else
        sendto_one(sptr, ":%s %d %s :To connect, type /QUOTE PONG %d",
            me.name, ERR_BADPING, PunteroACadena(sptr->name), sptr->cookie);

    }

    return 0;
  }

  if (parc < 2 || *parv[1] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
    return 0;
  }

  origin = parv[1];
  destination = parv[2];
  cptr->flags &= ~FLAGS_PINGSENT;
  sptr->flags &= ~FLAGS_PINGSENT;

  if (!BadPtr(destination) && strCasediff(destination, me.name) != 0)
  {
    if ((acptr = FindClient(destination)))
    {
      if (MyUser(acptr) || (acptr->serv && Protocol(acptr) < 10))
        sendto_one(acptr, ":%s PONG %s %s", parv[0], origin, destination);
      else
        sendto_one(acptr, "%s " TOK_PONG " %s %s", NumServ(sptr), origin, destination);
    }
    else
    {
      sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
          me.name, parv[0], destination);
      return 0;
    }
  }
#if defined(DEBUGMODE)
  else
    Debug((DEBUG_NOTICE, "PONG: %s %s",
        origin, destination ? destination : "*"));
#endif
  return 0;
}

static char umode_buf[2 * (sizeof(user_modes) +
    sizeof(user_hmodes)) / sizeof(int)];

/*
 * Ampliacion de modos de usuario (sptr->hmodes)
 *                                      1999/07/06 savage@apostols.org
 */
void send_umode_out(aClient *cptr, aClient *sptr, int old, int oldh,
    int registrado)
{
  Reg1 int i;
  Reg2 aClient *acptr;

  send_umode(NULL, sptr, old, SEND_UMODES, oldh, SEND_HMODES);

  if (registrado)
  {
    for (i = highest_fd; i >= 0; i--)
      if ((acptr = loc_clients[i]) && IsServer(acptr) &&
          (acptr != cptr) && (acptr != sptr) && *umode_buf)
      {
        if (Protocol(acptr) < 10)
          sendto_one(acptr, ":%s MODE %s :%s", sptr->name, sptr->name, umode_buf);
        else
          sendto_one(acptr, "%s%s " TOK_MODE " %s :%s", NumNick(sptr), sptr->name, umode_buf);

      }
  }

  if (cptr && MyUser(cptr)) {
    int HMODES;

    if (IsAnOper(sptr)) {
      if (IsAdmin(sptr) || IsCoder(sptr) || sptr == cptr)
        HMODES = ALL_HMODES;
      else
        HMODES = ALL_HMODES & ~HMODES_HIDDEN_OPER;
    } else
      HMODES = ALL_HMODES & ~(HMODES_HIDDEN_USER|HMODES_HIDDEN_OPER);

    send_umode(cptr, sptr, old, ALL_UMODES, oldh, HMODES);
  }
}

/*
 *  m_oper
 *    parv[0] = sender prefix
 *    parv[1] = oper name
 *    parv[2] = oper password
 *    parv[3] = connection class (optional)
 */
int m_oper(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aConfItem *aconf;
  char *name, *password, *encr, *class;
#if defined(CRYPT_OPER_PASSWORD)
  char salt[3];
#endif /* CRYPT_OPER_PASSWORD */

  name = parc > 1 ? parv[1] : NULL;
  password = parc > 2 ? parv[2] : NULL;
  class = parc > 3 ? parv[3] : NULL;

  if (!IsServer(cptr) && (BadPtr(name) || BadPtr(password)))
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "OPER");
    return 0;
  }

  /* if message arrived from server, trust it, and set to oper */

  if ((IsServer(cptr) || IsMe(cptr)) && !IsOper(sptr))
  {
    ++nrof.opers;
    sptr->flags |= FLAGS_OPER;
#if !defined(NO_PROTOCOL9)
    sendto_lowprot_butone(cptr, 9,
        ":%s MODE %s :+o", parv[0], parv[0]);
#endif
    sendto_highprot_butone(cptr, 10,
        "%s%s " TOK_MODE " %s :+o", NumNick(sptr), parv[0]);
    if (IsMe(cptr))
      sendto_one(sptr, rpl_str(RPL_YOUREOPER), me.name, parv[0]);
    return 0;
  }
  else if (IsAnOper(sptr))
  {
    if (MyConnect(sptr))
      sendto_one(sptr, rpl_str(RPL_YOUREOPER), me.name, parv[0]);
    return 0;
  }
  if (!(aconf =
      find_conf_exact(name, PunteroACadena(sptr->username),
      PunteroACadena(sptr->sockhost), CONF_OPS))
      && !(aconf =
      find_conf_exact(name, sptr->username, (char *)ircd_ntoa(&cptr->ip), CONF_OPS)))
  {
    sendto_one(sptr, err_str(ERR_NOOPERHOST), me.name, parv[0]);
    sendto_realops("Failed OPER attempt by %s (%s@%s)",
        parv[0], PunteroACadena(sptr->user->username),
        PunteroACadena(sptr->sockhost));
    return 0;
  }

  if (!BadPtr(class))           /* Clase por /OPER */
  {
    aConfClass *cclass = find_class(atoi(class));

    if (!ConClass(cclass))
    {
      /*
       * Clase invalida
       */
      Debug((DEBUG_NOTICE, "Class %d for %s -> FAILED!", atoi(class),
          sptr->name));
      sendto_one(sptr, err_str(ERR_NOOPERCLASS), me.name, parv[0], atoi(class));
      return 0;

    }
    else if (Links(cclass) >= MaxLinks(cclass) && MaxLinks(cclass) > 0)
    {
      /*
       * Clase llena
       */
      Debug((DEBUG_NOTICE, "Class %d for %s is FULL -> FAILED!",
          atoi(class), sptr->name));
      sendto_one(sptr, err_str(ERR_OPERCLASSFULL), me.name, parv[0],
          atoi(class));
      return 0;

    }

    Debug((DEBUG_NOTICE, "Class %d for %s -> OK!", atoi(class), sptr->name));

  }
  else if (aconf->confClass)    /* Clase por linea O: */
  {

    if (ConfLinks(aconf) >= ConfMaxLinks(aconf) && ConfMaxLinks(aconf) > 0)
    {
      /*
       * Clase llena
       */
      Debug((DEBUG_NOTICE, "Class %d for %s is FULL -> FAILED!",
          ConfClass(aconf), sptr->name));
      sendto_one(sptr, err_str(ERR_OPERCLASSFULL), me.name, parv[0],
          ConfClass(aconf));
      return 0;
    }

    Debug((DEBUG_NOTICE, "Class %d for %s -> OK!", ConfClass(aconf),
        sptr->name));
  }

#if defined(CRYPT_OPER_PASSWORD)
  /* use first two chars of the password they send in as salt */

  /* passwd may be NULL. Head it off at the pass... */
  salt[0] = '\0';
  if (password && aconf->passwd)
  {
    salt[0] = aconf->passwd[0];
    salt[1] = aconf->passwd[1];
    salt[2] = '\0';
    encr = crypt(password, salt);
  }
  else
    encr = "";
#else
  encr = password;
#endif /* CRYPT_OPER_PASSWORD */

  if ((aconf->status & CONF_OPS) && !strcmp(encr, aconf->passwd))
  {
    int old = (sptr->flags & ALL_UMODES);
    int oldh = (sptr->hmodes & ALL_HMODES);
    char *s;

    s = strchr(aconf->host, '@');
    *s++ = '\0';
#if defined(OPER_REMOTE)
    if (aconf->status == CONF_LOCOP)
    {
#else
    if (!MyConnect(sptr) || aconf->status == CONF_LOCOP)
    {
#endif
      ClearOper(sptr);
      SetLocOp(sptr);
    }
    else
    {
      /* prevent someone from being both oper and local oper */
      ClearLocOp(sptr);
      SetOper(sptr);
      sptr->privs = aconf->privs;
      ++nrof.opers;
    }
    *--s = '@';

    sendto_ops("%s (%s@%s) is now operator (%c)", parv[0],
        PunteroACadena(sptr->user->username),
#if defined(BDD_VIP)
        get_visiblehost(sptr, NULL, 0),
#else
        PunteroACadena(sptr->sockhost),
#endif
        IsOper(sptr) ? 'O' : 'o');

    sptr->flags |= (FLAGS_WALLOP | FLAGS_SERVNOTICE | FLAGS_DEBUG);
    set_snomask(sptr, SNO_OPERDEFAULT, SNO_ADD);
    sptr->privs = aconf->privs;
    SetOperCmd(sptr);
    send_umode_out(cptr, sptr, old, oldh, IsRegistered(sptr));
    /*
     * Pone una clase o se pilla de una linea O:
     */
    if ((!BadPtr(class)) || aconf->confClass)
    {
      aConfItem *aconf2;
      Reg1 Link *tmp, *tmp2;
      struct in_addr addr4;

      /*
       * Desligamos las clases que tiene el usuario
       */
      for (tmp = sptr->confs; tmp; tmp = tmp2)
      {
        tmp2 = tmp->next;
        if ((tmp->value.aconf->status & CONF_CLIENT) != 0)
        {
          detach_conf(sptr, tmp->value.aconf);
        }
      }

      /*
       * Creamos una clase y a linkar al usuario
       */
      aconf2 = make_conf();
      aconf2->confClass = make_class();
      DupString(aconf2->name, sptr->name);
      DupString(aconf2->host, PunteroACadena(sptr->sockhost));

      /* Pasamos de irc_in_addr a in_addr */
      addr4.s_addr = (sptr->ip.in6_16[6] | sptr->ip.in6_16[7] << 16);
      aconf2->ipnum = addr4;

      aconf2->status = CONF_CLIENT;

      if (!BadPtr(class))       /* Clase en el /OPER */
        aconf2->confClass = find_class(atoi(class));
      else if (aconf->confClass)  /* Clase en la linea O: */
        aconf2->confClass = find_class(ConfClass(aconf));

      attach_conf(sptr, aconf2);

    }
    else
    {
      /*
       * FATAL ERROR!!!!
       * Nunca deberia ocurrir, por si las moscas lo cortamos.
       */
      return 0;
    }


    sendto_one(sptr, rpl_str(RPL_YOUREOPER), me.name, parv[0]);
#if !defined(CRYPT_OPER_PASSWORD) && (defined(FNAME_OPERLOG) ||\
    (defined(USE_SYSLOG) && defined(SYSLOG_OPER)))
    encr = "";
#endif
#if defined(USE_SYSLOG) && defined(SYSLOG_OPER)
    syslog(LOG_INFO, "OPER (%s) (%s) by (%s!%s@%s)",
        name, encr, parv[0], PunteroACadena(sptr->user->username),
        PunteroACadena(sptr->sockhost));
#endif
#if defined(FNAME_OPERLOG)
    if (IsUser(sptr))
      write_log(FNAME_OPERLOG,
          "%s OPER (%s) (%s) by (%s!%s@%s)\n", myctime(now), name,
          encr, parv[0], PunteroACadena(sptr->user->username),
          PunteroACadena(sptr->sockhost));
#endif
  }
  else
  {
    sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name, parv[0]);
    sendto_realops("Failed OPER attempt by %s (%s@%s)",
        parv[0], PunteroACadena(sptr->user->username),
        PunteroACadena(sptr->sockhost));
  }
  return 0;
}

/*
 * m_pass
 *
 * parv[0] = sender prefix
 * parv[1] = password
 */
int m_pass(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  char *password = parc > 1 ? parv[1] : NULL;
  int len;

  if (BadPtr(password))
  {
    sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "PASS");
    return 0;
  }
  if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
  {
    sendto_one(cptr, err_str(ERR_ALREADYREGISTRED), me.name, parv[0]);
    return 0;
  }
  /*
   * Como no podemos usar MyUser ya que todavia no
   * esta registrado, solo parseamos a los que
   * entran por el puerto de clientes.
   * --zoltan
   */
  if (IsUserPort(cptr))
  {
    char *clave_bdd;
    /*
     * Sintaxis posibles:
     *   PASS :clave_servidor[:clave_bdd]
     *   PASS :clave_bdd
     *
     * CASOS:
     * a) PASS clave_a:clave_b
     *    - Se copia clave_a a cptr->passwd
     *    - Se copia clave_b a cptr->passbdd
     *
     * b) PASS clave_a
     *    - Se copia clave_a a cptr->passwd y a cptr->passbdd
     *
     * --zoltan
     */

    clave_bdd = strchr(password, ':');
    if (clave_bdd)
    {
      *clave_bdd++ = '\0';
    }
    else
    {
      clave_bdd = password;
    }
    if (cptr->passbdd)
      RunFree(cptr->passbdd);
    DupString(cptr->passbdd, clave_bdd);
  }
  len = strlen(password);
  if (len > PASSWDLEN)
  {
    len = PASSWDLEN;
    password[len] = '\0';
  }
  if (cptr->passwd)
    RunFree(cptr->passwd);

  cptr->passwd = RunMalloc(len + 1);
  assert(cptr->passwd);
  strcpy(cptr->passwd, password);

  return 0;
}

/*
 * m_userhost
 *
 * Added by Darren Reed 13/8/91 to aid clients and reduce the need for
 * complicated requests like WHOIS.
 *
 * Returns user/host information only (no spurious AWAY labels or channels).
 *
 * Rewritten to speed it up by Carlo Wood 3/8/97.
 */
int m_userhost(aClient *UNUSED(cptr), aClient *sptr, int parc, char *parv[])
{
  Reg1 char *s;
  Reg2 int i, j = 5;
  char *p = NULL, *sbuf;
  aClient *acptr;

  if (parc < 2)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "USERHOST");
    return 0;
  }

  sbuf = sprintf_irc(sendbuf, rpl_str(RPL_USERHOST), me.name, parv[0]);
  for (i = j, s = strtoken(&p, parv[1], " "); i && s;
      s = strtoken(&p, (char *)NULL, " "), i--)
    if ((acptr = FindUser(s)))
    {
      int viewhost = (sptr == acptr || can_viewhost(sptr, NULL, 0) || !IsHidden(acptr));
      if (i < j)
        *sbuf++ = ' ';
      sbuf = sprintf_irc(sbuf, "%s%s=%c%s@%s", acptr->name,
          IsAnOper(acptr) ? "*" : "", (acptr->user->away) ? '-' : '+',
          PunteroACadena(acptr->user->username),
          viewhost ? PunteroACadena(acptr->user->host) : get_virtualhost(acptr, 0));
    }
    else
    {
      if (i < j)
        sendbufto_one(sptr);
      sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], s);
      sbuf = sprintf_irc(sendbuf, rpl_str(RPL_USERHOST), me.name, parv[0]);
      j = i - 1;
    }
  if (j)
    sendbufto_one(sptr);
  return 0;
}

/*
 * m_userip added by Carlo Wood 3/8/97.
 *
 * The same as USERHOST, but with the IP-number instead of the hostname.
 */
int m_userip(aClient *UNUSED(cptr), aClient *sptr, int parc, char *parv[])
{
  Reg1 char *s;
  Reg3 int i, j = 5;
  char *p = NULL, *sbuf;
  aClient *acptr;

  if (parc < 2)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "USERIP");
    return 0;
  }

  sbuf = sprintf_irc(sendbuf, rpl_str(RPL_USERIP), me.name, parv[0]);
  for (i = j, s = strtoken(&p, parv[1], " "); i && s;
      s = strtoken(&p, (char *)NULL, " "), i--)
    if ((acptr = FindUser(s)))
    {
      int viewhost = (sptr == acptr || can_viewhost(sptr, NULL, 0) || !IsHidden(acptr));
      if (i < j)
        *sbuf++ = ' ';
      sbuf = sprintf_irc(sbuf, "%s%s=%c%s@%s", acptr->name,
          IsAnOper(acptr) ? "*" : "", (acptr->user->away) ? '-' : '+',
          PunteroACadena(acptr->user->username),
          viewhost ? ircd_ntoa_c(acptr) : "::ffff:0.0.0.0");
    }
    else
    {
      if (i < j)
        sendbufto_one(sptr);
      sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], s);
      sbuf = sprintf_irc(sendbuf, rpl_str(RPL_USERIP), me.name, parv[0]);
      j = i - 1;
    }
  if (i < j)
    sendbufto_one(sptr);
  return 0;
}

/*
 * m_ison
 *
 * Added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
 * format:
 * ISON :nicklist
 */

int m_ison(aClient *UNUSED(cptr), aClient *sptr, int parc, char *parv[])
{
  Reg1 aClient *acptr;
  Reg2 char *s, **pav = parv;
  Reg3 size_t len;
  char *p = NULL;

  if (parc < 2)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "ISON");
    return 0;
  }

  sprintf_irc(buf, rpl_str(RPL_ISON), me.name, *parv);
  len = strlen(buf);
  buf[sizeof(buf) - 1] = 0;

  for (s = strtoken(&p, *++pav, " "); s; s = strtoken(&p, NULL, " "))
    if ((acptr = FindUser(s)))
    {
      strncat(buf, acptr->name, sizeof(buf) - 1 - len);
      len += strlen(acptr->name);
      if (len >= sizeof(buf) - 1)
        break;
      strcat(buf, " ");
      len++;
    }
  sendto_one(sptr, "%s", buf);
  return 0;
}

/*
 * m_umode() added 15/10/91 By Darren Reed.
 *
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
int m_umode(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Reg1 int flag;
  Reg2 int *s;
  Reg3 char **p, *m;
  aClient *acptr;
  int what, setflags;
  snomask_t tmpmask = 0;
  int snomask_given = 0;
  int sethmodes;
  int statusbdd = 0;

  what = MODE_ADD;

  if (parc < 2)
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "MODE");
    return 0;
  }

  if (!(acptr = FindUser(parv[1])))
  {
    if (MyConnect(sptr))
      sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL), me.name, parv[0], parv[1]);
    return 0;
  }

  if (IsServer(sptr) || sptr != acptr)
  {
    if (IsServer(cptr))
      sendto_ops_butone(NULL, &me, ":%s WALLOPS :MODE for User %s From %s!%s",
          me.name, parv[1], cptr->name, sptr->name);
    else
      sendto_one(sptr, err_str(ERR_USERSDONTMATCH), me.name, parv[0]);
    return 0;
  }

  if (parc < 3)
  {
    m = buf;
    *m++ = '+';
    for (s = user_modes; (flag = *s) && (m - buf < BUFSIZE - 4); s += 2)
      if (sptr->flags & flag)
        *m++ = (char)(*(s + 1));
    for (s = user_hmodes; (flag = *s) && (m - buf < BUFSIZE - 4); s += 2)
      if (sptr->hmodes & flag)
        *m++ = (char)(*(s + 1));
    *m = '\0';
    sendto_one(sptr, rpl_str(RPL_UMODEIS), me.name, parv[0], buf);
    if ((sptr->flags & FLAGS_SERVNOTICE) && MyConnect(sptr)
        && sptr->snomask !=
        (unsigned int)(IsOper(sptr) ? SNO_OPERDEFAULT : SNO_DEFAULT))
      sendto_one(sptr, rpl_str(RPL_SNOMASK), me.name, parv[0], sptr->snomask,
          sptr->snomask);
    return 0;
  }

  /* find flags already set for user */
  setflags = 0;
  for (s = user_modes; (flag = *s); s += 2)
  {
    if (sptr->flags & flag)
      setflags |= flag;
  }

  sethmodes = 0;
  for (s = user_hmodes; (flag = *s); s += 2)
  {
    if (sptr->hmodes & flag)
      sethmodes |= flag;
  }

  if (MyConnect(sptr))
    tmpmask = sptr->snomask;

  /*
   * parse mode change string(s)
   */
  for (p = &parv[2]; *p; p++)   /* p is changed in loop too */
    for (m = *p; *m; m++)
      switch (*m)
      {
        case '+':
          what = MODE_ADD;
          break;
        case '-':
          what = MODE_DEL;
          break;
        case 's':
          if (*(p + 1) && is_snomask(*(p + 1)))
          {
            snomask_given = 1;
            tmpmask = umode_make_snomask(tmpmask, *++p, what);
            tmpmask &= (IsAnOper(sptr) ? SNO_ALL : SNO_USER);
          }
          else
            tmpmask = (what == MODE_ADD) ?
                (IsAnOper(sptr) ? SNO_OPERDEFAULT : SNO_DEFAULT) : 0;
          if (tmpmask)
            sptr->flags |= FLAGS_SERVNOTICE;
          else
            sptr->flags &= ~FLAGS_SERVNOTICE;
          break;
          /*
           * We may not get these, but they shouldnt be in default:
           */
        case ' ':
        case '\n':
        case '\r':
        case '\t':
          break;
        default:
          for (s = user_modes; (flag = *s); s += 2)
            if (*m == (char)(*(s + 1)))
            {
              if (what == MODE_ADD)
                sptr->flags |= flag;
              else if ((flag & (FLAGS_OPER | FLAGS_LOCOP)))
              {
                sptr->flags &= ~(FLAGS_OPER | FLAGS_LOCOP);
                if (MyConnect(sptr))
                {
                  tmpmask = sptr->snomask & ~SNO_OPER;
                  ClearOperCmd(sptr);
                }
              }
              /* allow either -o or -O to reset all operator status's... */
              else
                sptr->flags &= ~flag;
              break;
            }
          if (flag == 0)
            for (s = user_hmodes; (flag = *s); s += 2)
              if (*m == (char)(*(s + 1)))
              {
                if (what == MODE_ADD)
                  sptr->hmodes |= flag;
                else
                  sptr->hmodes &= ~flag;
                break;
              }
          if (flag == 0 && MyConnect(sptr))
            sendto_one(sptr, err_str(ERR_UMODEUNKNOWNFLAG), me.name, parv[0]);
          break;
      }

  /* Pedimos los flags permitidos */
  if (MyUser(sptr))
    statusbdd = get_status(sptr);

  /*
   * Stop users making themselves operators too easily:
   */

  /* Si lo autoriza la BDD puede hacerse ircop en cualquier momento */
  if (MyUser(sptr) && (!(setflags & FLAGS_OPER))
      && IsOper(sptr) && !(statusbdd & FLAGS_OPER))
    ClearOper(sptr);

/*
  if (!(setflags & FLAGS_OPER) && IsOper(sptr) && !IsServer(cptr))
    ClearOper(sptr);
*/
  if (!(setflags & FLAGS_LOCOP) && IsLocOp(sptr) && !IsServer(cptr))
    sptr->flags &= ~FLAGS_LOCOP;
  if ((setflags & (FLAGS_OPER | FLAGS_LOCOP)) && !IsAnOper(sptr) &&
      MyConnect(sptr))
    det_confs_butmask(sptr, CONF_CLIENT & ~CONF_OPS);

  /* new umode; servers can set it, local users cannot;
   * prevents users from /kick'ing or /mode -o'ing */
  /* el modo +/-r solo se acepta de los Servidores */
  /* Y el modo +/-S solo servidores */
  if (!IsServer(cptr))
  {
    if (sethmodes & HMODE_NICKREGISTERED)
      SetNickRegistered(sptr);
    else
      ClearNickRegistered(sptr);

    if (sethmodes & HMODE_NICKSUSPENDED)
      SetNickSuspended(sptr);
    else
      ClearNickSuspended(sptr);

    if (sethmodes & HMODE_VHOSTPERSO)
      SetVhostPerso(sptr);
    else
      ClearVhostPerso(sptr);

    if (sethmodes & HMODE_SSL)
      SetSSL(sptr);
    else
      ClearSSL(sptr);

    if (sethmodes & HMODE_DOCKING)
      SetDocking(sptr);
    else
      ClearDocking(sptr);

    if (!(sethmodes & HMODE_USERDEAF))
      ClearUserDeaf(sptr);

    if (!(sethmodes & HMODE_USERBITCH))
      ClearUserBitch(sptr);

    if (!(sethmodes & HMODE_USERNOJOIN))
      ClearUserNoJoin(sptr);

    if (sethmodes & HMODE_PENDVALIDATION)
      SetPendValidation(sptr);
    else
      ClearPendValidation(sptr);
  }

  if (MyConnect(sptr) && IsNickRegistered(sptr))
  {                             /* Nick Registrado */

    /* el modo +h solo se lo pueden poner los OPER y quitar a voluntad */
    if (!(sethmodes & HMODE_HELPOP) && IsHelpOp(sptr) &&
        !(statusbdd & HMODE_HELPOP))
      ClearHelpOp(sptr);

    /* el modo +a solo se lo pueden poner los ADMIN y quitar a voluntad */
    if (!(sethmodes & HMODE_ADMIN) && IsAdmin(sptr) &&
        !(statusbdd & HMODE_ADMIN))
      ClearAdmin(sptr);

    /* el modo +C solo se lo pueden poner los CODER y quitar a voluntad */
    if (!(sethmodes & HMODE_CODER) && IsCoder(sptr) &&
        !(statusbdd & HMODE_CODER))
      ClearCoder(sptr);

#if defined(BDD_VIP) && !defined(BDD_VIP2)
    if (MyConnect(sptr))
    {
      if (!db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
      {
        ClearHidden(sptr);
      }
    }
#endif

#if defined(BDD_VIP) && defined(BDD_VIP2)
/*
** Antes no tenia +r, y ahora si.
** Puede ser que tengamos una virtual personalizada propia
*/
    if (!(sethmodes & HMODE_NICKREGISTERED))
    {
      if (db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
        SetVhostPerso(sptr);
      else
        BorraIpVirtualPerso(sptr);
    }
#endif
  }
  else if (MyConnect(sptr))
  {
    /* Nick no registrados de usuarios locales.
     * Los remotos se respetan modos.
     */
    ClearMsgOnlyReg(sptr);
    ClearNoChan(sptr);
    ClearDocking(sptr);
    ClearHelpOp(sptr);
    ClearAdmin(sptr);
    ClearCoder(sptr);
    if (IsOper(sptr))
      --nrof.opers;
    ClearOper(sptr);
    ClearServicesBot(sptr);
#if defined(BDD_VIP) && !defined(BDD_VIP2)
    if (!db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
    {
      ClearHidden(sptr);
    }
#endif
    if (!IsOperCmd(sptr)) {
      ClearHiddenViewer(sptr);
      ClearNoIdle(sptr);
    }
  }

/*
** Lazy Virtual Host
*/
#if defined(BDD_VIP)
  if ((sethmodes & HMODE_HIDDEN) && !IsHidden(sptr))
  {
    if (IsVhostPerso(sptr))
      BorraIpVirtualPerso(sptr);
  }
#endif

/*
** Si somos IRCOPs y hemos puesto o quitado el flag D, lo acepta.
** dfm@unr.com 20090427
*/
  if (!IsServer(cptr) && !IsOper(sptr))
  {
    if(sethmodes & HMODE_USERDEAF)
      SetUserDeaf(sptr);
    else
      ClearUserDeaf(sptr);
  }


/*
** Si somos IRCOPs y hemos puesto o quitado el flag P, lo acepta.
** dfm@unr.com 20090427
*/
  if (!IsServer(cptr) && !IsOper(sptr))
  {
    if(sethmodes & HMODE_USERBITCH)
      SetUserBitch(sptr);
    else
      ClearUserBitch(sptr);
  }

/*
** Si somos IRCOPs y hemos puesto el flag K, lo acepta.
** jcea@argo.es - 16/Dic/97
*/
  if (!(setflags & FLAGS_CHSERV) && !IsServer(cptr)
      && !HasPriv(sptr, PRIV_CHANSERV))
    sptr->flags &= ~FLAGS_CHSERV;

/* MODO +s capado para usuarios */
  if (!(setflags & FLAGS_SERVNOTICE) && !IsServer(cptr)
      && !IsAnOper(sptr))
  {
    sptr->flags &= ~FLAGS_SERVNOTICE;
    set_snomask(sptr, 0, SNO_SET);
  }

    /* el modo +B solo se lo pueden poner los BOT y quitar a voluntad */
  if (!(sethmodes & HMODE_SERVICESBOT) && !IsServer(cptr)
      && !(statusbdd & HMODE_SERVICESBOT)
      && !buscar_uline(cptr->confs, sptr->name))
    ClearServicesBot(sptr);

/*
** El +n solo se lo pueden poner usuarios con +r
*/
  if (MyUser(sptr) && (!(sethmodes & HMODE_NOCHAN))
      && IsNoChan(sptr) && !IsNickRegistered(sptr))
    ClearNoChan(sptr);

/*
** El +X solo se lo pueden poner usuarios autorizados
*/
  if (MyUser(sptr) && (!(sethmodes & HMODE_HIDDENVIEWER))
      && IsHiddenViewer(sptr) && !HasPriv(sptr, PRIV_HIDDEN_VIEWER))
    ClearHiddenViewer(sptr);

/*
** El +W solo se lo pueden poner usuarios autorizados
*/
  if (MyUser(sptr) && (!(sethmodes & HMODE_WHOIS))
      && IsWhois(sptr) && !HasPriv(sptr, PRIV_WHOIS_NOTICE))
    ClearWhois(sptr);

/*
** El +I solo se lo pueden poner usuarios autorizados
*/
  if (MyUser(sptr) && (!(sethmodes & HMODE_NOIDLE))
      && IsNoIdle(sptr) && !HasPriv(sptr, PRIV_HIDE_IDLE))
    ClearNoIdle(sptr);


  /*
   * Compare new flags with old flags and send string which
   * will cause servers to update correctly.
   */

  if ((setflags & FLAGS_OPER) && !IsOper(sptr))
    --nrof.opers;
  if (!(setflags & FLAGS_OPER) && IsOper(sptr))
    ++nrof.opers;

  if ((setflags & FLAGS_INVISIBLE) && !IsInvisible(sptr))
    --nrof.inv_clients;
  if (!(setflags & FLAGS_INVISIBLE) && IsInvisible(sptr))
    ++nrof.inv_clients;
  send_umode_out(cptr, sptr, setflags, sethmodes, IsRegistered(sptr));
  if (MyConnect(sptr))
  {
    if (tmpmask != sptr->snomask)
      set_snomask(sptr, tmpmask, SNO_SET);
    if (sptr->snomask && snomask_given)
      sendto_one(sptr, rpl_str(RPL_SNOMASK), me.name, sptr->name,
          sptr->snomask, sptr->snomask);
  }

  if (IsWatch(sptr))
  {
    chequea_estado_watch(sptr, RPL_LOGON);
    ClearWatch(sptr);
  }

  return 0;
}

/*
 * m_svsumode()
 *
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
*/
int m_svsumode(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;
  Reg1 int flag;
  Reg2 int *s;
  Reg3 char **p, *m;
  int what, setflags;
  int sethmodes;
  snomask_t tmpmask = 0;

  what = MODE_ADD;

  acptr = findNUser(parv[1]);
  if (!acptr)
    acptr = FindUser(parv[1]);
  if (!acptr)
    return 0;


  /* find flags already set for user */
  setflags = 0;
  for (s = user_modes; (flag = *s); s += 2)
    if (acptr->flags & flag)
      setflags |= flag;

  sethmodes = 0;
  for (s = user_hmodes; (flag = *s); s += 2)
    if (acptr->hmodes & flag)
      sethmodes |= flag;

  /*
   * parse mode change string(s)
   */
  for (p = &parv[2]; *p; p++)   /* p is changed in loop too */
    for (m = *p; *m; m++)
      switch (*m)
      {
        case '+':
          what = MODE_ADD;
          break;
        case '-':
          what = MODE_DEL;
          break;
        case 'V':
          if (what == MODE_ADD)
            SetPendValidation(acptr);
          else {
            if (MyConnect(acptr) && IsPendValidation(acptr))
              sendto_one(acptr,
                  ":%s NOTICE %s :*** Se ha validado la conexion, ya puedes utilizar el servicio de Chat :)",
                  bot_nickserv ? bot_nickserv : me.name, acptr->name);

            ClearPendValidation(acptr);
          }
          break;
        /*
         * We may not get these, but they shouldnt be in default:
         */
        case ' ':
        case '\n':
        case '\r':
        case '\t':
          break;
        default:
          for (s = user_modes; (flag = *s); s += 2)
            if (*m == (char)(*(s + 1)))
            {
              if (what == MODE_ADD)
                acptr->flags |= flag;
              else if ((flag & (FLAGS_OPER | FLAGS_LOCOP)))
              {
                acptr->flags &= ~(FLAGS_OPER | FLAGS_LOCOP);
                if (MyConnect(acptr))
                  tmpmask = acptr->snomask & ~SNO_OPER;
              }
              /* allow either -o or -O to reset all operator status's... */
              else
                acptr->flags &= ~flag;
              break;
            }
            if (flag == 0)
              for (s = user_hmodes; (flag = *s); s += 2)
                if (*m == (char)(*(s + 1)))
                {
                  if (what == MODE_ADD)
                    acptr->hmodes |= flag;
                  else
                    acptr->hmodes &= ~flag;
                  break;
                }
            break;
      }


  /*
   * Compare new flags with old flags and send string which
   * will cause servers to update correctly.
   */

  if ((setflags & FLAGS_OPER) && !IsOper(acptr))
    --nrof.opers;
  if (!(setflags & FLAGS_OPER) && IsOper(acptr))
    ++nrof.opers;
  if ((setflags & FLAGS_INVISIBLE) && !IsInvisible(acptr))
    --nrof.inv_clients;
  if (!(setflags & FLAGS_INVISIBLE) && IsInvisible(acptr))
    ++nrof.inv_clients;

  if (MyUser(acptr))
  {
    if (tmpmask != acptr->snomask)
      set_snomask(acptr, tmpmask, SNO_SET);
  }

#if 1 /* ALTERNATIVA SVSMODE A TODOS */

  if (MyUser(acptr))
    send_umode(acptr, acptr, setflags, SEND_UMODES, sethmodes, IsAnOper(acptr) ? SEND_HMODES : SEND_HMODES & ~HMODES_HIDDEN_USER);
#if !defined(NO_PROTOCOL9)
  sendto_lowprot_butone(cptr, 9, ":%s SVSMODE %s %s", acptr->name, acptr->name, parv[2]);
#endif
  sendto_highprot_butone(cptr, 10, "%s " TOK_SVSMODE " %s %s", NumServ(sptr), parv[1], parv[2]);

#else /* ALTERNATIVA SVSMODE AL NODO MAS PROXIMO Y MODE AL RESTO */

  send_umode_out(cptr, acptr, setflags, sethmodes, IsRegistered(acptr));

#endif

  return 0;
}

void mask_user_flags(char *modes, int *addflags, int *addhmodes)
{
  char c, *p;
  int *ip;
  int addf, addh, modof, modoh;
  int i;

  p = modes;
  modof = ~(addf = 0);
  modoh = ~(addh = 0);
  while (!0)
  {
    c = *p++;
    switch (c)
    {
      case '\0':
      case ' ':
      case '\t':
        goto out;
      case '+':
        modof = ~0;
        modoh = ~0;
        break;
      case '-':
        modof = 0;
        modoh = 0;
        break;
      default:
        for (ip = user_modes; *ip; ip += 2)
        {
          if (c == *(ip + 1))
          {
            i = *ip;
            addf |= modof & i;
            break;
          }
        }
        for (ip = user_hmodes; *ip; ip += 2)
        {
          if (c == *(ip + 1))
          {
            i = *ip;
            addh |= modoh & i;
            break;
          }
        }
        break;
    }
  }

out:
  if (addflags)
  {
    *addflags = addf;
  }
  if (addhmodes)
  {
    *addhmodes = addh;
  }
}

/*
 * Build umode string for BURST command
 * --Run
 * Se amplia la funcion para devolver los
 * modos de cptr que ve acptr. Si solo se
 * quiere construir la cadena para el BURST
 * o tener el listado completo de los modos
 * de cptr, se le pasa NULL como segundo
 * parametro.
 * --NiKoLaS
 */
char *umode_str(aClient *cptr, aClient *acptr)
{
  char *m = umode_buf;          /* Maximum string size: "owidg\0" */
  int *s, flag, c_flags;
  int c_hmodes;

  c_hmodes =  cptr->hmodes & SEND_HMODES;

  for (s = user_hmodes; (flag = *s); s += 2)
  {
    if (c_hmodes & flag)
    {
      if(acptr)
      {
        if (flag & HMODES_HIDDEN_OPER)
        {
          if (IsAdmin(acptr) || IsCoder(acptr) || acptr == cptr)
            *m++ = *(s + 1);
        }
        else if (flag & HMODES_HIDDEN_USER)
        {
          if (IsAnOper(acptr))
            *m++ = *(s + 1);
        } else
          *m++ = *(s + 1);
      }
      else
        *m++ = *(s + 1);
    }
  }

  c_flags = cptr->flags & SEND_UMODES;  /* cleaning up the original code */

  for (s = user_modes; (flag = *s); s += 2)
  {
    if ((c_flags & flag))
    {
      if (acptr)
      {
        if ((flag & FLAGS_INVISIBLE) || (flag & FLAGS_WALLOP)
            || (flag & FLAGS_DEBUG))
        {
          if (IsAnOper(acptr) || (acptr == cptr))
            *m++ = *(s + 1);
        }
        else
          *m++ = *(s + 1);
      }
      else
        *m++ = *(s + 1);
    }
  }

  *m = '\0';

  return umode_buf;             /* Note: static buffer, gets
                                   overwritten by send_umode() */
}

/*
 * Send the MODE string for user (user) to connection cptr
 * -avalon
 */
void send_umode(aClient *cptr, aClient *sptr, int old, int sendmask, int oldh,
    int sendhmask)
{
  Reg1 int *s, flag;
  Reg2 char *m;
  int what = MODE_NULL;

  /*
   * Build a string in umode_buf to represent the change in the user's
   * mode between the new (sptr->flag) and 'old'.
   */
  m = umode_buf;
  *m = '\0';
  for (s = user_modes; (flag = *s); s += 2)
  {
    if (MyUser(sptr) && !(flag & sendmask))
      continue;
    if ((flag & old) && !(sptr->flags & flag))
    {
      if (what == MODE_DEL)
        *m++ = *(s + 1);
      else
      {
        what = MODE_DEL;
        *m++ = '-';
        *m++ = *(s + 1);
      }
    }
    else if (!(flag & old) && (sptr->flags & flag))
    {
      if (what == MODE_ADD)
        *m++ = *(s + 1);
      else
      {
        what = MODE_ADD;
        *m++ = '+';
        *m++ = *(s + 1);
      }
    }
  }
  for (s = user_hmodes; (flag = *s); s += 2)
  {
    if (MyUser(sptr) && !(flag & sendhmask))
      continue;
    if ((flag & oldh) && !(sptr->hmodes & flag))
    {
      if (what == MODE_DEL)
        *m++ = *(s + 1);
      else
      {
        what = MODE_DEL;
        *m++ = '-';
        *m++ = *(s + 1);
      }
    }
    else if (!(flag & oldh) && (sptr->hmodes & flag))
    {
      if (what == MODE_ADD)
        *m++ = *(s + 1);
      else
      {
        what = MODE_ADD;
        *m++ = '+';
        *m++ = *(s + 1);
      }
    }
  }
  *m = '\0';
  if (*umode_buf && cptr) {
    if (MyUser(cptr) || Protocol(cptr) < 10)
      sendto_one(cptr, ":%s MODE %s :%s", sptr->name, sptr->name, umode_buf);
    else
      sendto_one(cptr, "%s%s " TOK_MODE " %s :%s", NumNick(sptr), sptr->name, umode_buf);
  }
}

/*
 * Check to see if this resembles a sno_mask.  It is if 1) there is
 * at least one digit and 2) The first digit occurs before the first
 * alphabetic character.
 */
int is_snomask(char *word)
{
  if (word)
  {
    for (; *word; word++)
      if (isDigit(*word))
        return 1;
      else if (isAlpha(*word))
        return 0;
  }
  return 0;
}

/*
 * If it begins with a +, count this as an additive mask instead of just
 * a replacement.  If what == MODE_DEL, "+" has no special effect.
 */
snomask_t umode_make_snomask(snomask_t oldmask, char *arg, int what)
{
  snomask_t sno_what;
  snomask_t newmask;
  if (*arg == '+')
  {
    arg++;
    if (what == MODE_ADD)
      sno_what = SNO_ADD;
    else
      sno_what = SNO_DEL;
  }
  else if (*arg == '-')
  {
    arg++;
    if (what == MODE_ADD)
      sno_what = SNO_DEL;
    else
      sno_what = SNO_ADD;
  }
  else
    sno_what = (what == MODE_ADD) ? SNO_SET : SNO_DEL;
  /* pity we don't have strtoul everywhere */
  newmask = (snomask_t)atoi(arg);
  if (sno_what == SNO_DEL)
    newmask = oldmask & ~newmask;
  else if (sno_what == SNO_ADD)
    newmask |= oldmask;
  return newmask;
}

/*
 * This function sets a Client's server notices mask, according to
 * the parameter 'what'.  This could be even faster, but the code
 * gets mighty hard to read :)
 */
void delfrom_list(aClient *, Link **);
void set_snomask(aClient *cptr, snomask_t newmask, int what)
{
  snomask_t oldmask, diffmask;  /* unsigned please */
  int i;
  Link *tmp;

  oldmask = cptr->snomask;

  if (what == SNO_ADD)
    newmask |= oldmask;
  else if (what == SNO_DEL)
    newmask = oldmask & ~newmask;
  else if (what != SNO_SET)     /* absolute set, no math needed */
    sendto_ops("setsnomask called with %d ?!", what);

  if (IsAnOper(cptr))
    newmask &= SNO_ALL;
  else
    newmask = 0; /* &= SNO_USER; */

  diffmask = oldmask ^ newmask;

  for (i = 0; diffmask >> i; i++)
    if (((diffmask >> i) & 1))
    {
      if (((newmask >> i) & 1))
      {
        tmp = make_link();
        tmp->next = opsarray[i];
        tmp->value.cptr = cptr;
        opsarray[i] = tmp;
      }
      else
        /* not real portable :( */
        delfrom_list(cptr, &opsarray[i]);
    }
  cptr->snomask = newmask;
}

void delfrom_list(aClient *cptr, Link **list)
{
  Link *tmp, *prv = NULL;
  for (tmp = *list; tmp; tmp = tmp->next)
  {
    if (tmp->value.cptr == cptr)
    {
      if (prv)
        prv->next = tmp->next;
      else
        *list = tmp->next;
      free_link(tmp);
      break;
    }
    prv = tmp;
  }
}

/*
 * is_silenced : Does the actual check wether sptr is allowed
 *               to send a message to acptr.
 *               Both must be registered persons.
 * If sptr is silenced by acptr, his message should not be propagated,
 * but more over, if this is detected on a server not local to sptr
 * the SILENCE mask is sent upstream.
 */
int is_silenced(aClient *sptr, aClient *acptr)
{
  Reg1 Link *lp;
  Reg2 anUser *user;
  static char sender[HOSTLEN + NICKLEN + USERLEN + 5];
  static char senderip[SOCKIPLEN + NICKLEN + USERLEN + 5];
#if defined(BDD_VIP)
  static char sendervirtual[100 + HOSTLEN + NICKLEN + USERLEN + 5];
#endif

  if (!(acptr->user) || !(lp = acptr->user->silence) || !(user = sptr->user))
    return 0;
  sprintf_irc(sender, "%s!%s@%s", sptr->name, PunteroACadena(user->username),
      PunteroACadena(user->host));
  sprintf_irc(senderip, "%s!%s@%s", sptr->name, PunteroACadena(user->username),
      ircd_ntoa_c(sptr));
#if defined(BDD_VIP)
  sprintf_irc(sendervirtual, "%s!%s@%s", sptr->name,
      PunteroACadena(user->username), get_virtualhost(sptr, 1));
#endif

  for (; lp; lp = lp->next)
  {
    if ((!(lp->flags & CHFL_SILENCE_IPMASK) && !match(lp->value.cp, sender)) ||
#if defined(BDD_VIP)
        (!(lp->flags & CHFL_SILENCE_IPMASK)
        && !match(lp->value.cp, sendervirtual)) ||
#endif
        ((lp->flags & CHFL_SILENCE_IPMASK) && !match(lp->value.cp, senderip)))
    {
      if (!MyConnect(sptr))
      {
        if (Protocol(sptr->from) < 10)
          sendto_one(sptr->from, ":%s SILENCE %s %s", acptr->name,
              sptr->name, lp->value.cp);
        else
          sendto_one(sptr->from, "%s%s " TOK_SILENCE " %s%s %s", NumNick(acptr),
              NumNick(sptr), lp->value.cp);
      }
      return 1;
    }
  }
  return 0;
}

/*
 * del_silence
 *
 * Removes all silence masks from the list of sptr that fall within `mask'
 * Returns -1 if none where found, 0 otherwise.
 */
int del_silence(aClient *sptr, char *mask)
{
  Reg1 Link **lp;
  Reg2 Link *tmp;
  int ret = -1;

  for (lp = &sptr->user->silence; *lp;)
    if (!mmatch(mask, (*lp)->value.cp))
    {
      tmp = *lp;
      *lp = tmp->next;
      RunFree(tmp->value.cp);
      free_link(tmp);
      ret = 0;
    }
    else
      lp = &(*lp)->next;

  return ret;
}

static int add_silence(aClient *sptr, char *mask)
{
  Reg1 Link *lp, **lpp;
  Reg3 int cnt = 0, len = strlen(mask);
  char *ip_start;

  for (lpp = &sptr->user->silence, lp = *lpp; lp;)
  {
    if (!strCasediff(mask, lp->value.cp))
      return -1;
    if (!mmatch(mask, lp->value.cp))
    {
      Link *tmp = lp;
      *lpp = lp = lp->next;
      RunFree(tmp->value.cp);
      free_link(tmp);
      continue;
    }
    if (MyUser(sptr))
    {
      len += strlen(lp->value.cp);
      if ((len > MAXSILELENGTH) || (++cnt >= MAXSILES))
      {
        sendto_one(sptr, err_str(ERR_SILELISTFULL), me.name, sptr->name, mask);
        return -1;
      }
      else if (!mmatch(lp->value.cp, mask))
        return -1;
    }
    lpp = &lp->next;
    lp = *lpp;
  }
  lp = make_link();
  memset(lp, 0, sizeof(Link));
  lp->next = sptr->user->silence;
  lp->value.cp = (char *)RunMalloc(strlen(mask) + 1);
  strcpy(lp->value.cp, mask);
  if ((ip_start = strrchr(mask, '@')) && check_if_ipmask(ip_start + 1))
    lp->flags = CHFL_SILENCE_IPMASK;
  sptr->user->silence = lp;
  return 0;
}

/*
 * m_silence() - Added 19 May 1994 by Run.
 *
 *   parv[0] = sender prefix
 * From local client:
 *   parv[1] = mask (NULL sends the list)
 * From remote client:
 *   parv[1] = Numeric nick that must be silenced
 *   parv[2] = mask
 */
int m_silence(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Link *lp;
  aClient *acptr;
  char c, *cp;

  if (MyUser(sptr))
  {
    acptr = sptr;
    if (parc < 2 || *parv[1] == '\0' || !strCasecmp(sptr->name, parv[1]))
    {
      if (!(acptr->user))
        return 0;
      for (lp = acptr->user->silence; lp; lp = lp->next)
        sendto_one(sptr, rpl_str(RPL_SILELIST), me.name,
            sptr->name, acptr->name, lp->value.cp);
      sendto_one(sptr, rpl_str(RPL_ENDOFSILELIST), me.name, sptr->name,
          acptr->name);
      return 0;
    }
    cp = parv[1];
    c = *cp;
    if (c == '-' || c == '+')
      cp++;
    else if (!(strchr(cp, '@') || strchr(cp, '.') ||
        strchr(cp, '!') || strchr(cp, '*')))
    {
      sendto_one(sptr, err_str(ERR_SILECANTBESHOWN), me.name, parv[0], parv[1]);
      return -1;
    }
    else
      c = '+';
    cp = pretty_mask(cp);
    if ((c == '-' && !del_silence(sptr, cp)) ||
        (c != '-' && !add_silence(sptr, cp)))
    {
      if (MyUser(sptr) || Protocol(sptr->from) < 10)
        sendto_prefix_one(sptr, sptr, ":%s SILENCE %c%s", parv[0], c, cp);
      else
        sendto_prefix_one(sptr, sptr, "%s%s " TOK_SILENCE " %c%s", NumNick(sptr), c, cp);
      if (c == '-')
      {
#if !defined(NO_PROTOCOL9)
        sendto_lowprot_butone(NULL, 9, ":%s SILENCE * -%s", sptr->name, cp);
#endif
        sendto_highprot_butone(NULL, 10, "%s%s " TOK_SILENCE " * -%s", NumNick(sptr), cp);
      }
    }
  }
  else if (parc < 3 || *parv[2] == '\0')
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SILENCE");
    return -1;
  }
  else
  {
    if (Protocol(cptr) < 10)
      acptr = FindClient(parv[1]);  /* In case of NOTE notice, parv[1] */
    else if (parv[1][1])        /* can be a server */
      acptr = findNUser(parv[1]);
    else
      acptr = FindNServer(parv[1]);

    if (*parv[2] == '-')
    {
      if (!del_silence(sptr, parv[2] + 1))
      {
#if !defined(NO_PROTOCOL9)
        sendto_lowprot_butone(cptr, 9, ":%s SILENCE * %s", parv[0], parv[2]);
#endif
        sendto_highprot_butone(cptr, 10, "%s%s " TOK_SILENCE " * %s", NumNick(sptr), parv[2]);
      }
    }
    else
    {
      add_silence(sptr, parv[2]);
      if (acptr && IsServer(acptr->from))
      {
        if (Protocol(acptr->from) < 10)
          sendto_one(acptr, ":%s SILENCE %s %s", parv[0], acptr->name, parv[2]);
        else if (IsServer(acptr))
          sendto_one(acptr, "%s%s " TOK_SILENCE " %s %s",
              NumNick(sptr), NumServ(acptr), parv[2]);
        else
          sendto_one(acptr, "%s%s " TOK_SILENCE " %s%s %s",
              NumNick(sptr), NumNick(acptr), parv[2]);
      }
    }
  }
  return 0;
}

/*
 * int get_status(sptr)
 *
 * Da los flags segun la tabla o de operadores
 *
 */
int get_status(aClient *sptr)
{
  int status = 0;
  struct db_reg *reg;

  reg = db_buscar_registro(BDD_OPERDB, sptr->name);

  if (reg) {
      if (*reg->valor == '{')
      {
        /* Formato nuevo JSON */
        json_object *json, *json_modes;
        enum json_tokener_error jerr = json_tokener_success;
        char *modes;
        char c, *p;
        int add = 1;

        json = json_tokener_parse_verbose(reg->valor, &jerr);
        if (jerr != json_tokener_success)
          return 0;

        json_object_object_get_ex(json, "modes", &json_modes);
        modes = (char *)json_object_get_string(json_modes);

        if (modes)
        {
          p = modes;

          while (!0)
          {
            c = *p++;
            switch (c)
            {
              case '\0':
              case ' ':
              case '\t':
                return status;

              case '+':
                add = 1;
                break;
              case '-':
                add = 0;
                break;
              case 'a':
                if (add)
                  status |= HMODE_ADMIN;
                break;
              case 'C':
                if (add)
                  status |= HMODE_CODER;
                break;
              case 'h':
                if (add)
                  status |= HMODE_HELPOP;
                break;
              case 'o':
                if (add)
                  status |= FLAGS_OPER;
                break;
              case 'O':
                if (add)
                  status |= FLAGS_LOCOP;
                break;
              case 'B':
                if (add)
                  status |= HMODE_SERVICESBOT;
                break;

              default:
                break;
            }
          }
        }

        return status;

      } else {
        /* Sistema nuevo, por flags */
        switch (*reg->valor)
        {
          case 'a':
            status |= HMODE_ADMIN | FLAGS_OPER;
            break;

          case 'c':
            status |= HMODE_CODER | FLAGS_OPER;
            break;

          case 'b':
            status |= HMODE_SERVICESBOT | FLAGS_OPER;
            break;

          case 'h':
            status |= FLAGS_OPER;

          case 'o':
            status |= FLAGS_OPER;

          case 'p':
            status |= HMODE_HELPOP;
            break;
        }
        return status;
      }
  }

  return 0;
}

#if defined(BDD_VIP)
/*
 * int can_viewhost(sptr, acptr)
 *
 * Da o no autorizacion para ver hosts segun los privilegios.
 *
 */
int can_viewhost(aClient *sptr, aClient *acptr, int audit)
{
  if (!acptr)
    return 0;

  if (!IsHiddenViewer(sptr))
    return 0;

  /* Los Admins y Coders a todos */
  if (IsAdmin(sptr) || IsCoder(sptr))
  {
    if (audit && (sptr != acptr))
      sendto_debug_channel(canal_privsdebug, "[PRIVS-IP] %s ha visto la IP de %s", sptr->name, acptr->name);
    return 1;
  }

  /* El resto solo hacia usuarios */
  if (!IsAnOper(acptr) && !IsAdmin(acptr) && !IsCoder(acptr))
  {
    if (audit && (sptr != acptr))
      sendto_debug_channel(canal_privsdebug, "[PRIVS-IP] %s ha visto la IP de %s", sptr->name, acptr->name);
    return 1;
  }

  return 0;
}

/*
 * char *get_virtualhost(sptr)                  ** MIGRAR A hispano.c **
 *
 * Nos da la virtual de una conexion, aunque no tenga flag +x.
 *
 */
char *get_virtualhost(aClient *sptr, int perso)
{
  if (!IsUser(sptr))
    return "<Que_Pasa?>";       /* esto no deberia salir nunca */

  if (perso && IsVhostPerso(sptr))
  {
    if (!sptr->user->vhostperso)
      make_vhostperso(sptr, 0);
    return sptr->user->vhostperso;
  }

  if (!sptr->user->vhost)
    make_vhost(sptr, 0);
  return sptr->user->vhost;
}

/*
 * char *get_visiblehost(sptr,acptr)               ** MIGRAR A hispano.c **
 *
 * nos da la ip real o la virtual de una conexion dependiendo
 * del userflag +x/-x, del +X/-X y de si somos nosotros mismos
 *
 * Si acptr es NULL, se considera un usuario "anonimo".
 *
 */
char *get_visiblehost(aClient *sptr, aClient *acptr, int audit)
{
  if (!IsUser(sptr) || (acptr && !IsUser(acptr)))
    return "*";                 /* Si el usuario no est� registrado se manda un * como host visible */

  if (!IsHidden(sptr) || (acptr && can_viewhost(acptr, sptr, audit)) || sptr == acptr)
    return PunteroACadena(sptr->user->host);
  else
  {
    if (IsVhostPerso(sptr)) {
      if (!sptr->user->vhostperso)
        make_vhostperso(sptr, 0);
      return sptr->user->vhostperso;
    }

    if (!sptr->user->vhost)
      make_vhost(sptr, 0);
    return sptr->user->vhost;
  }
}

/*
 * make_vhost(acptr, mostrar)                       ** MIGRAR A hispano.c **
 *
 * crea la ip virtual
 *
 */
void make_vhost(aClient *acptr, int mostrar)
{
  unsigned int v[2], x[2];
  int ts = 0;
  char ip_virtual_temporal[HOSTLEN + 1];

  assert(!mostrar || MyUser(acptr));

  if (!clave_de_cifrado_de_ips)
  {
    SlabStringAllocDup(&(acptr->user->vhost), "no.hay.clave.de.cifrado",
        0);
    return;
  }

  /* IPv4 */
  if (irc_in_addr_is_ipv4(&acptr->ip))
  {
    while (1)
    {
      /* resultado */
      x[0] = x[1] = 0;

      v[0] = (clave_de_cifrado_binaria[0] & 0xffff0000) + ts;
      v[1] = ntohs((unsigned long)acptr->ip.in6_16[6]) << 16 | ntohs((unsigned long)acptr->ip.in6_16[7]);

      tea(v, clave_de_cifrado_binaria, x);

      /* formato direccion virtual: qWeRty.AsDfGh.virtual */
      inttobase64(ip_virtual_temporal, x[0], 6);
      ip_virtual_temporal[6] = '.';
      inttobase64(ip_virtual_temporal + 7, x[1], 6);
      strcpy(ip_virtual_temporal + 13, ".virtual");

      /* el nombre de Host es correcto? */
      if (strchr(ip_virtual_temporal, '[') == NULL &&
        strchr(ip_virtual_temporal, ']') == NULL)
        break;                    /* nice host name */
      else
      {
      if (++ts == 65536)
        {                         /* No deberia ocurrir nunca */
          strcpy(ip_virtual_temporal, PunteroACadena(acptr->user->host));
          break;
        }
      }
    }
  } else { /* IPv6 */
    /* resultado */
    x[0] = x[1] = 0;

    v[0] = ntohs((unsigned long)acptr->ip.in6_16[0]) << 16 | ntohs((unsigned long)acptr->ip.in6_16[1]);
    v[1] = ntohs((unsigned long)acptr->ip.in6_16[2]) << 16 | ntohs((unsigned long)acptr->ip.in6_16[3]);

    tea(v, clave_de_cifrado_binaria, x);

    /* formato direccion virtual: qWeRty.AsDfGh.v6 */
    inttobase64(ip_virtual_temporal, x[0], 6);
    ip_virtual_temporal[6] = '.';
    inttobase64(ip_virtual_temporal + 7, x[1], 6);
    strcpy(ip_virtual_temporal + 13, ".v6");
  }

#if defined(BDD_VIP3)
  if (MyConnect(acptr))
  {
    strcpy(ip_virtual_temporal, PunteroACadena(acptr->user->host));
  }
#endif
  SlabStringAllocDup(&(acptr->user->vhost), ip_virtual_temporal, HOSTLEN);

  if (mostrar)
  {
    sendto_one(acptr, rpl_str(RPL_HOSTHIDDEN), me.name, acptr->name,
        acptr->user->vhost);
  }
}

/*
 * make_vhostperso(acptr, mostrar)                  ** MIGRAR A hispano.c **
 *
 * crea la ip virtual personalizada
 *
 */
void make_vhostperso(aClient *acptr, int mostrar)
{
  struct db_reg *reg = NULL;

  assert(!mostrar || MyUser(acptr));

  if ((reg = db_buscar_registro(BDD_IPVIRTUALDB, acptr->name)))
  {
    char *vhost;

    if (*reg->valor == '{')
    {
      /* Formato nuevo JSON */
      json_object *json, *json_vhost;
      enum json_tokener_error jerr = json_tokener_success;

      json = json_tokener_parse_verbose(reg->valor, &jerr);
      if (jerr != json_tokener_success)
        return;

      json_object_object_get_ex(json, "vhost", &json_vhost);
      vhost = (char *)json_object_get_string(json_vhost);

      SlabStringAllocDup(&(acptr->user->vhostperso), vhost, HOSTLEN);
    }
    else
    {
      /* Formato antiguo vhost!vhostcolor */

      /* Copio el valor en memoria para evitar que corte el registro en
       * memoria de la BDD cuando hay un ! de separacion de campos.
       */
      DupString(vhost, reg->valor);
      if (strchr(vhost, '!'))
      {
        /* Corto */
        int i = 0;

        while (vhost[i] != 0) {
          if (vhost[i] == '!')
          {
            vhost[i]=0;
            break;
          }
          i++;
        }
      }
      SlabStringAllocDup(&(acptr->user->vhostperso), vhost, HOSTLEN);
      RunFree(vhost);
    }

    SetVhostPerso(acptr);
    if (mostrar)
      sendto_one(acptr, rpl_str(RPL_HOSTHIDDEN), me.name, acptr->name,
          acptr->user->vhostperso);
  }
}
#endif

/*
 * Privilegios
 */
void set_privs(aClient *sptr)
{
  struct db_reg *reg;

  if (!MyConnect(sptr))
    return;

  reg = db_buscar_registro(BDD_OPERDB, sptr->name);

  if (reg) {
    if (*reg->valor == '{')
    {
      u_int64_t privs;

      /* Formato nuevo JSON */
      json_object *json, *json_privs;
      enum json_tokener_error jerr = json_tokener_success;

      json = json_tokener_parse_verbose(reg->valor, &jerr);
      if (jerr != json_tokener_success)
      {
        sptr->privs = 0;
        return;
      }

      json_object_object_get_ex(json, "privs", &json_privs);
      privs =  json_object_get_int64(json_privs);
      sptr->privs = privs;
    } else {
      sptr->privs = atoll(reg->valor+2);
    }
  } else {
    sptr->privs = 0;
  }
}

uint64_t HasPriv(aClient *sptr, uint64_t priv)
{
     if (!MyUser(sptr) || !IsAnOper(sptr))
         return 0;

     return sptr->privs & priv;
}

/** Array mapping privilege values to names and vice versa. */
static struct {
  char        *name; /**< Name of privilege. */
  uint64_t     priv; /**< Enumeration value of privilege */
} privtab[] = {
/** Helper macro to define an array entry for a privilege. */
#define P(priv)         { #priv, PRIV_ ## priv }
  P(CHAN_LIMIT),     P(MODE_LCHAN),     P(WALK_LCHAN),    P(DEOP_LCHAN),
  P(SHOW_INVIS),     P(SHOW_ALL_INVIS), P(UNLIMIT_QUERY), P(KILL),
  P(LOCAL_KILL),     P(REHASH),         P(RESTART),       P(DIE),
  P(GLINE),          P(LOCAL_GLINE),    P(JUPE),          P(LOCAL_JUPE),
  P(OPMODE),         P(LOCAL_OPMODE),   P(SET),           P(WHOX),
  P(BADCHAN),        P(LOCAL_BADCHAN),  P(SEE_CHAN),      P(PROPAGATE),
  P(DISPLAY),        P(SEE_OPERS),      P(WIDE_GLINE),    P(LIST_CHAN),
  P(FORCE_OPMODE),   P(FORCE_LOCAL_OPMODE), P(APASS_OPMODE), P(WALK_CHAN),
  P(NETWORK),        P(CHANSERV),       P(HIDDEN_VIEWER), P(WHOIS_NOTICE),
  P(HIDE_IDLE),
#undef P
  { 0, 0 }
};

/** Report privileges of \a client to \a to.
 * @param[in] to Client requesting privilege list.
 * @param[in] client Client whos privileges should be listed.
 * @return Zero.
 */
static int
client_report_privs(struct Client *to, struct Client *client)
{
  int i, idx, len, mlen;

  mlen = strlen(me.name) + 10 + strlen(to->name) + strlen(client->name);
  buf[0] = '\0';
  idx = 0;

  for (i = 0; privtab[i].name; i++) {
    if (HasPriv(client, privtab[i].priv)) {
      len = strlen(privtab[i].name);

      if (mlen + idx + len > BUFSIZE)
      {
        sendto_one(to, rpl_str(RPL_PRIVS), me.name, to->name, client->name, buf);
        buf[0] = '\0';
        idx = 0;
      }

      if (idx) {
          strcat(buf, " ");
          idx++;
      }
      strcat(buf, privtab[i].name);
      idx += len;
    }
  }

  if (buf[0] != '\0')
    sendto_one(to, rpl_str(RPL_PRIVS), me.name, to->name, client->name, buf);

  return 0;
}

static int m_privs_local(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;
  char *name;
  char *p = 0;
  int i;

  if (!IsAnOper(sptr))
  {
    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
    return 0;
  }

  if (parc < 2)
    return client_report_privs(sptr, sptr);

  for (i = 1; i < parc; i++) {
    for (name = strtoken(&p, parv[i], " "); name;
         name = strtoken(&p, 0, " ")) {
      if (!(acptr = FindUser(name)))
        sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], name);
      else if (MyUser(acptr))
        client_report_privs(sptr, acptr);
      else
        sendto_one(acptr->from, "%s " TOK_PRIVS " %s%s", NumServ(&me), NumNick(acptr));
    }
  }
  return 0;
}

static int m_privs_remoto(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;
  char *numnick, *p = 0;
  int i;

  if (parc < 2)
    return 0;

  for (i = 1; i < parc; i++) {
    for (numnick = strtoken(&p, parv[i], " "); numnick;
         numnick = strtoken(&p, 0, " ")) {
      if (!(acptr = findNUser(numnick)))
        continue;
      else if (MyUser(acptr))
        client_report_privs(sptr, acptr);
      else
        sendto_one(acptr->from, "%s " TOK_PRIVS " %s%s", NumServ(&me), NumNick(acptr));
    }
  }

  return 0;
}

int m_privs(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  if (!MyConnect(sptr))
    return m_privs_remoto(cptr, sptr, parc, parv);
  else
    return m_privs_local(cptr, sptr, parc, parv);
}


/*
 * 9 de Octubre de 2003, mount@irc-dev.net
 *
 * rename_user(sptr, nuevo_nick)
 *
 * Cambia el nick del cliente LOCAL especificado por el nuevo dado.
 * Si el nick dado es NULL, genera un nick del estilo de "invitado-XXXXXX".
 *
 */
void rename_user(aClient *sptr, char *nick_nuevo)
{
#if defined(BDD_VIP)
  int vhperso = 0;
#endif

  assert(MyConnect(sptr));

  if (!nick_nuevo)
  {
    aClient *acptr;

    {
      unsigned int v[2], k[2], x[2];
      char resultado[NICKLEN + 1];

      k[0] = k[1] = x[0] = x[1] = 0;

      v[0] = base64toint(sptr->yxx);
      v[1] = base64toint(me.yxx);

      acptr = sptr;

      do
      {
        tea(v, k, x);

        v[1] += 4096;

/*
 ** El 'if' que sigue lo necesitamos
 ** para que todos los valores tengan
 ** la misma probabilidad.
 */
        if (x[0] >= 4294000000ul)
          continue;

        sprintf_irc(resultado, "invitado-%.6d", (int)(x[0] % 1000000));

        nick_nuevo = resultado;

        acptr = FindClient(nick_nuevo);
      }
      while (acptr);
    }
  }

  {
    int of, oh;

    of = sptr->flags;
    oh = sptr->hmodes;

    if (IsNickRegistered(sptr) || IsNickSuspended(sptr))
    {                           /* Parche DB69 */
      ClearNickRegistered(sptr);
      ClearNickSuspended(sptr);
      ClearMsgOnlyReg(sptr);
      ClearNoChan(sptr);
      ClearDocking(sptr);
      ClearWhois(sptr);
      ClearHelpOp(sptr);
      ClearAdmin(sptr);
      ClearCoder(sptr);
      if (IsOper(sptr))
        --nrof.opers;
      ClearOper(sptr);
      ClearServicesBot(sptr);

#if defined(BDD_VIP)
#if !defined(BDD_VIP2)
      ClearHidden(sptr);
#endif
#endif
      if (!IsOperCmd(sptr)) {
        ClearHiddenViewer(sptr);
        ClearNoIdle(sptr);
      }
      send_umode_out(sptr, sptr, of, oh, IsRegistered(sptr));
    }
  }

  sendto_op_mask(SNO_SERVICE,
      "Cambiamos el nick '%s' a '%s'", sptr->name, nick_nuevo);

  sptr->lastnick = now;

  /* Esto manda una copia al propio usuario */
  sendto_common_channels(sptr, ":%s NICK :%s", sptr->name, nick_nuevo);

/*
** Lo anterior ya lo manda al usuario, aunque no este en ningun canal
*/
  //sendto_one(sptr, ":%s NICK :%s", sptr->name, nick_nuevo);

  add_history(sptr, 1);
#if !defined(NO_PROTOCOL9)
  sendto_lowprot_butone(NULL, 9,
      ":%s NICK %s " TIME_T_FMT, sptr->name, nick_nuevo, sptr->lastnick);
#endif
  sendto_highprot_butone(NULL, 10,
      "%s%s " TOK_NICK " %s " TIME_T_FMT, NumNick(sptr), nick_nuevo, sptr->lastnick);

  if (sptr->name)
  {
    hRemClient(sptr);
    /*
     * Avisamos a sus contactos que el nick
     * ha salido (ha cambiado de nick).
     */
    chequea_estado_watch(sptr, RPL_LOGOFF);
  }
  SlabStringAllocDup(&(sptr->name), nick_nuevo, 0);
  hAddClient(sptr);

#if defined(BDD_VIP)
  if (IsVhostPerso(sptr)) {
      BorraIpVirtualPerso(sptr);
      vhperso = 1;
  }
#endif

/* 23-Oct-2003: mount@irc-dev.net
 *
 * Ponemos los modos que le corresponda al nuevo usuario...
 */

  {
    struct db_reg *reg;

    reg = db_buscar_registro(ESNET_NICKDB, sptr->name);
    if (reg)
    {
      int of, oh;
      int flagsddb = 0;
      char *automodes = NULL;

      of = sptr->flags;
      oh = sptr->hmodes;

      if (*reg->valor == '{')
      {
        /* Formato nuevo JSON */
        json_object *json, *json_flags, *json_automodes;
        enum json_tokener_error jerr = json_tokener_success;

        json = json_tokener_parse_verbose(reg->valor, &jerr);
        if (jerr == json_tokener_success) {
          json_object_object_get_ex(json, "flags", &json_flags);
          flagsddb = json_object_get_int(json_flags);
          json_object_object_get_ex(json, "automodes", &json_automodes);
          automodes = (char *)json_object_get_string(json_automodes);
        }
      } else {
        /* Formato antiguo pass_flag */
        char c;
        c = reg->valor[strlen(reg->valor) - 1];
        if (c == '+')
          flagsddb = DDB_NICK_SUSPEND;
      }

      /* Rename a un nick registrado */
      if (flagsddb == DDB_NICK_SUSPEND)
        SetNickSuspended(sptr);
      else
        SetNickRegistered(sptr);

#if 0
      /* Antes de nada, encontramos a 'Nick Virtual' */
      sendto_one(cptr,
          ":%s NOTICE %s :*** Renombrando a tu nick anterior.",
          bot_nickserv ? bot_nickserv : me.name, nick_nuevo);
#endif

#if defined(BDD_VIP)
#if defined(BDD_VIP2)           /* Pongo +x al usuario si el ircd esta
                                 * compilado de forma que solo los usuarios
                                 * registrados puedan tener +x, en el caso
                                 * contrario dejo el modo tal como esta.
                                 */
      SetHidden(sptr);
      if (db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
        SetVhostPerso(sptr);
#else
      /* Puede que tenga una ip virtual personalizada */
      if (IsNickSuspended(sptr))
      {
        ClearHidden(sptr);
      }
      else if (db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
      {
        SetHidden(sptr);
        SetVhostPerso(sptr);
      }
      else
      {
        ClearHidden(sptr);
      }
#endif

      if (IsHidden(sptr))
      {
        if (IsVhostPerso(sptr)) {
          make_vhostperso(sptr, 1);
        } else {
            /* Tenia vhost personalizada, ahora mandamos
             * cambio para que el usuario tenga constancia.
             */
          if (vhperso)
            sendto_one(sptr, rpl_str(RPL_HOSTHIDDEN), me.name, sptr->name,
                 get_virtualhost(sptr, 0));
        }
      }
#endif

      if (!IsNickSuspended(sptr))
      {
        int status;

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
            if (!IsOper(sptr))
              nrof.opers++;
            SetOper(sptr);
            sptr->flags |= (FLAGS_WALLOP | FLAGS_SERVNOTICE | FLAGS_DEBUG);
            set_snomask(sptr, SNO_OPERDEFAULT, SNO_ADD);
            set_privs(sptr);
          }
        }

        /* Automodes DDB */
        if (automodes)
        {
            int addflags, addhmodes;

            mask_user_flags(automodes, &addflags, &addhmodes);
            sptr->flags |= addflags;
            sptr->hmodes |= addhmodes;
        }
      }
      send_umode_out(sptr, sptr, of, oh, IsRegistered(sptr));
    }

  }

  /*
   * Avisamos a sus contactos que el nick
   * ha entrado (ha cambiado de nick).
   */
  chequea_estado_watch(sptr, RPL_LOGON);
}

/*
 * Comando GHOST
 * A�adido por zoltan el 5 de Agosto 2001
 * Elimina una conexion fantasma o "ghost" de un nick registrado
 *
 * Sintaxis:
 * GHOST nick password
 *
 * JCEA: la clave es opcional si se ha introducido una como clave de conexion.
 */

int m_ghost(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;
  struct db_reg *reg;
  int clave_ok = 0;
  char *clave;

  if (parc < 2)
  {
    sendto_one(cptr,
        ":%s NOTICE %s :*** Sintaxis incorrecta. Formato: GHOST <nick> [clave]",
        bot_nickserv ? bot_nickserv : me.name, parv[0]);
    return 0;
  }

  if (parc < 3)
  {
    clave = strchr(parv[1], ':');
    if (clave)
    {
      *clave++ = '\0';
    }
    else if (cptr->passbdd)
    {
      clave = cptr->passbdd;
    }
  }
  else
  {
    clave = parv[2];
  }

  if (!clave)
  {
    sendto_one(cptr,
        ":%s NOTICE %s :*** Debes indicar alguna clave, o en el comando o al conectar.",
        bot_nickserv ? bot_nickserv : me.name, parv[0]);
    return 0;
  }

  acptr = FindUser(parv[1]);
  if (!acptr)
  {
    sendto_one(cptr,
        ":%s NOTICE %s :*** El nick %s no est� conectado actualmente.",
        bot_nickserv ? bot_nickserv : me.name, parv[0], parv[1]);
    return 0;
  }

  if (cptr == acptr)
  {
    sendto_one(cptr,
        ":%s NOTICE %s :*** ERROR: No puedes hacer ghost a t� mismo.",
        bot_nickserv ? bot_nickserv : me.name, parv[0]);
    return 0;
  }

  reg = db_buscar_registro(ESNET_NICKDB, acptr->name);
  if (!reg)
  {
    sendto_one(cptr,
        ":%s NOTICE %s :*** El nick %s no est� registrado en la BDD.",
        bot_nickserv ? bot_nickserv : me.name, parv[0], acptr->name);
    return 0;
  }

  if (*reg->valor == '{')
  {
    /* Formato nuevo JSON */
    json_object *json, *json_pass;
    enum json_tokener_error jerr = json_tokener_success;
    char *passddb;

    json = json_tokener_parse_verbose(reg->valor, &jerr);
    if (jerr != json_tokener_success) {
      sendto_one(cptr,
          ":%s NOTICE %s :*** El nick %s est� prohibido.",
          bot_nickserv ? bot_nickserv : me.name, parv[0], acptr->name);
      return 0;
    }

    json_object_object_get_ex(json, "pass", &json_pass);
    passddb = (char *)json_object_get_string(json_pass);
    clave_ok = verifica_clave_nick(reg->clave, passddb, clave);
  } else {
    /* Formato antiguo pass */
    clave_ok = verifica_clave_nick(reg->clave, reg->valor, clave);
  }

  if (!clave_ok)
  {
    sendto_one(cptr, ":%s NOTICE %s :*** Clave incorrecta.",
        bot_nickserv ? bot_nickserv : me.name, parv[0]);
    return 0;
  }

  /* Matamos al usuario con un kill */
  sendto_op_mask(SNO_SERVKILL, "%s ha recibido KILL por comando GHOST de %s.",
      acptr->name, cptr->name);
#if !defined(NO_PROTOCOL9)
  sendto_lowprot_butone(cptr, 9,  /* Kill our old drom outgoing servers */
      ":%s KILL %s :Comando GHOST utilizado por %s",
      me.name, acptr->name, cptr->name);
#endif
  sendto_highprot_butone(cptr, 10,  /* Kill our old drom outgoing servers */
      "%s " TOK_KILL " %s%s :Comando GHOST utilizado por %s",
      NumServ(&me), NumNick(acptr), cptr->name);

  if (MyConnect(acptr))
    sendto_prefix_one(acptr, &me, ":%s KILL %s :Comando GHOST utilizado por %s",
        me.name, acptr->name, cptr->name);

  sprintf_irc(buf2, "Killed (Comando GHOST utilizado por %s)", cptr->name);
  exit_client(cptr, acptr, &me, buf2);

  sendto_one(cptr,
      ":%s NOTICE %s :*** Sesi�n fantasma del nick %s ha sido liberada.",
      bot_nickserv ? bot_nickserv : me.name, parv[0], parv[1]);

  return 1;
}


int m_svsnick(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  aClient *acptr;

  if (!IsServer(cptr) || !IsServer(sptr) || parc < 2)
    return 0;

  if (!buscar_uline(cptr->confs, sptr->name) || (sptr->from != cptr))
  {
    sendto_serv_butone(cptr,
        ":%s DESYNCH :HACK(4): El nodo '%s' dice que '%s' solicita "
        "cambio de nick para '%s'", me.name, cptr->name, sptr->name, parv[1]);
    sendto_op_mask(SNO_HACK4 | SNO_SERVKILL | SNO_SERVICE,
        "HACK(4): El nodo '%s' dice que '%s' solicita "
        "cambio de nick para '%s'", cptr->name, sptr->name, parv[1]);
    return 0;
  }

  acptr = findNUser(parv[1]);
  if (!acptr)
    acptr = FindUser(parv[1]);
  if (!acptr)
    return 0;

  sendto_op_mask(SNO_SERVICE,
        "El nodo '%s' solicita un cambio de nick para '%s'", sptr->name, acptr->name);

  if(!MyUser(acptr))
  {
    if (parc == 3)
      sendcmdto_one(acptr, IsUser(sptr) ? &me : sptr, "SVSNICK", TOK_SVSNICK, ":%s", parv[2]);
    else if(parc == 4)
      sendcmdto_one(acptr, IsUser(sptr) ? &me : sptr, "SVSNICK", TOK_SVSNICK, "%s :%s", parv[2], parv[3]);
    else
      sendcmdto_one(acptr, IsUser(sptr) ? &me : sptr, "SVSNICK", TOK_SVSNICK, "");

    return 0;
  }

  if (parc < 3)
    rename_user(acptr, NULL);
  else {
    char nick[NICKLEN + 2];
    struct db_reg *reg;

    strncpy(nick, parv[2], nicklen + 1);
    nick[nicklen] = 0;

    if (!do_nick_name(nick))
     return 0;

   if (FindUser(nick))
     return 0;

   /* No se puede poner nicks prohibidos */
   reg = db_buscar_registro(ESNET_NICKDB, nick);
   if (reg)
   {
     int flagsddb = 0;

     if (*reg->valor == '{')
     {
       /* Formato nuevo JSON */
       json_object *json, *json_flags;
       enum json_tokener_error jerr = json_tokener_success;

       json = json_tokener_parse_verbose(reg->valor, &jerr);
       if (jerr == json_tokener_success) {
         json_object_object_get_ex(json, "flags", &json_flags);
         flagsddb = json_object_get_int(json_flags);
       }
     } else {
       /* Formato antiguo pass_flag */
       char c;
       c = reg->valor[strlen(reg->valor) - 1];
       if (c == '*')
         flagsddb = DDB_NICK_FORBID;
     }

     if (flagsddb == DDB_NICK_FORBID)
     {
       sendto_serv_butone(cptr,
           ":%s DESYNCH :HACK(4): El nodo '%s' dice que '%s' ha intentado poner un "
           "nick forbid '%s' para el nick '%s'", me.name, cptr->name, sptr->name, parv[2], parv[1]);
       sendto_op_mask(SNO_HACK4 | SNO_SERVKILL | SNO_SERVICE,
           "HACK(4): El nodo '%s' dice que '%s' ha intentado poner un "
           "nick forbid '%s' para el nick '%s'", cptr->name, sptr->name, parv[2], parv[1]);
       return 0;
     }
   }

   rename_user(acptr, nick);
  }

  return 0;
}

/* m_nick stuff... */

/*
 * m_nick_local
 *
 * parv[0] = sender prefix
 * parv[1] = nickname (:clave opcional)
 * parv[2] = clave (opcional)
 *
 * If from server, source is client:
 *   parv[2] = timestamp
 *
 * Source is server:
 *   parv[2] = hopcount
 *   parv[3] = timestamp
 *   parv[4] = username
 *   parv[5] = hostname
 *   parv[6] = umode (optional)
 *   parv[parc-3] = IP#                 <- Only Protocol >= 10
 *   parv[parc-2] = YXX, numeric nick   <- Only Protocol >= 10
 *   parv[parc-1] = info
 *   parv[0] = server
 */
int m_nick_local(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  struct db_reg *reg;
  char hflag = '-';
  int clave_ok = 0;             /* Clave correcta */
  int hacer_ghost = 0;          /* Ha especificado nick! */
  int nick_suspendido = 0;      /* Nick SUSPENDido */
  int nick_equivalentes = 0;
  int nick_autentificado_en_bdd = 0;
  int nick_aleatorio = 0;
  aClient *acptr;
  aClient *server = NULL;
  char nick[NICKLEN + 2];
  Link *lp;
  time_t lastnick = (time_t) 0;
  int differ = 1;
  char *regexp;
  char nick_low[NICKLEN+1];
  char *tmp;
  char *reason = NULL;
  char *automodes = NULL;
#if defined(BDD_VIP)
  int vhperso = 0;
#endif

  assert(MyConnect(sptr));

  ClearWatch(sptr);             /* Nos curamos en salud */

/*
** No dejamos que un usuario cambie de nick varias veces ANTES
** de haber completado su entrada en la red.
*/
  if (!IsRegistered(sptr) && (sptr->cookie != 0) &&
      !IsCookieVerified(sptr))
  {
    return 0;
  }

  nick_autentificado_en_bdd = IsNickRegistered(sptr) || IsNickSuspended(sptr);

  if (parc < 2)
  {
    sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN), me.name, parv[0]);
    return 0;
  }
  else if ((IsServer(sptr) && parc < 8) || (IsServer(cptr) && parc < 3))
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "NICK");
    sendto_ops("bad NICK param count for %s from %s", parv[1], cptr->name);
    return 0;
  }

  /* CASO ESPECIAL
   * NICK *
   * Sirve para poner un nick aleatorio
   */
  if (permite_nicks_random &&(strlen(parv[1]) == 1) && (*parv[1] == '*'))
  {
     parv[1] = nuevo_nick_aleatorio(sptr);
     nick_aleatorio = 1;
  }

  strncpy(nick, parv[1], nicklen + 1);
  nick[nicklen] = 0;

  /*
   * If do_nick_name() returns a null name OR if the server sent a nick
   * name and do_nick_name() changed it in some way (due to rules of nick
   * creation) then reject it. If from a server and we reject it,
   * and KILL it. -avalon 4/4/92
   */
  if (do_nick_name(nick) == 0 || (IsServer(cptr) && strcmp(nick, parv[1])))
  {
    sendto_one(sptr, err_str(ERR_INVALIDNICKNAME), me.name, parv[0], parv[1]);

    if (IsServer(cptr))
    {
      ircstp->is_kill++;
      sendto_ops("Bad Nick: %s From: %s %s", parv[1], parv[0], cptr->name);
      if (Protocol(cptr) < 10)
        sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
            me.name, parv[1], me.name, parv[1], nick, cptr->name);
      else
        sendto_one(cptr, "%s " TOK_KILL " %s :%s (%s <- %s[%s])",
            NumServ(&me), IsServer(sptr) ? parv[parc - 2] : parv[0], me.name,
            parv[1], nick, cptr->name);
      if (!IsServer(sptr))      /* bad nick _change_ */
      {
#if !defined(NO_PROTOCOL9)
        sendto_lowprot_butone(cptr, 9, ":%s KILL %s :%s (%s <- %s!%s@%s)",
            me.name, parv[0], me.name, cptr->name, parv[0],
            sptr->user ? PunteroACadena(sptr->username) : "",
            sptr->user ? sptr->user->server->name : cptr->name);
#endif
        sendto_highprot_butone(cptr, 10, "%s " TOK_KILL " %s :%s (%s <- %s!%s@%s)",
            NumServ(&me), parv[0], me.name, cptr->name,
            parv[0], sptr->user ? PunteroACadena(sptr->username) : "",
            sptr->user ? sptr->user->server->name : cptr->name);
        sptr->flags |= FLAGS_KILLED;
        return exit_client(cptr, sptr, &me, "BadNick");
      }
    }
    return 0;
  }

  /* Calculo el nick en minusculas por si hay que matchearlo en pcre */
  strncpy(nick_low, nick, NICKLEN);
  nick_low[NICKLEN]='\0';
  tmp=nick_low;
  while (*tmp) {
    *tmp=toLower(*tmp);
    *tmp++;
  }
  
  if ((!IsServer(cptr)) && !nick_aleatorio)
  {
    struct db_reg *regj;

    /* Al usar match, hay que iterar */
    for (regj = db_iterador_init(BDD_JUPEDB); regj; regj = db_iterador_next())
    {
      if (!match(regj->clave, nick))
      {
        sendto_one(cptr, ":%s %d %s %s :Nickname is juped - El nick no est� permitido: %s",
            me.name, ERR_NICKNAMEINUSE, BadPtr(parv[0]) ? "*" : parv[0], nick, regj->valor);
        return 0;
      }
      
      /* Si es un digito matcheo por PCRE */
      if(isdigit(regj->clave[0]))
      {        
        regexp=strchr(regj->valor, ':');
        if (regexp && *(regexp+1) && !match_pcre_str(regexp+1, nick_low))
        {           
          /* Corto la cadena para mostrar el mensaje, luego la restauro */
          *regexp='\0';
          sendto_one(cptr, ":%s %d %s %s :Nickname is juped - El nick no est� permitido: %s",
              me.name, ERR_NICKNAMEINUSE, BadPtr(parv[0]) ? "*" : parv[0], nick, regj->valor);
          *regexp=':';
          return 0;
        }
      }
    }
  }

    /* No ponemos MyUser porque podria que aun no sea usuario */
    if (!match("webchat-*", nick) && !IsServer(cptr)) {
      sendto_one(sptr, err_str(ERR_NICKNAMEINUSE), me.name,
          /* parv[0] is empty when connecting */
          BadPtr(parv[0]) ? "*" : parv[0], nick);
      return 0;
    }

  /*
   * Check against nick name collisions.
   *
   * Put this 'if' here so that the nesting goes nicely on the screen :)
   * We check against server name list before determining if the nickname
   * is present in the nicklist (due to the way the below for loop is
   * constructed). -avalon
   */
  if ((acptr = FindServer(nick)))
  {
    sendto_one(sptr, err_str(ERR_NICKNAMEINUSE), me.name,
        BadPtr(parv[0]) ? "*" : parv[0], nick);
    return 0;                   /* NICK message ignored */
  }

  reg = NULL;
  if (!IsServer(cptr))
  {
    char *p = strchr(parv[1], ':');

    if (p != NULL)
    {
      *p++ = '\0';
      parc = 3;
      parv[2] = p;
    }
    else if ((p = strchr(parv[1], '!')))
    {
      /* Soporte GHOST nick!clave */
      hacer_ghost = 1;
      /* Si hemos puesto jcea! , el strlen es 1 y se pilla la clave
       * en el /server.  Si es jcea!clave, el strlen es mayor que 1
       * y se pilla lo que va despues de la ! para la clave.
       */
      if (strlen(p) > 1)
      {
        *p++ = '\0';
        parc = 3;
        parv[2] = p;
      }
    }
    else if (parc > 2 && (*parv[2] == '!'))
    {
      /* Soporte GHOST nick !clave */
      hacer_ghost = 1;
      *parv[2]++ = '\0';
    }
    reg = db_buscar_registro(ESNET_NICKDB, nick);
  }

  if (!(acptr = FindClient(nick)))
    goto nickkilldone;          /* No collisions, all clear... */
  /*
   * If acptr == sptr, then we have a client doing a nick
   * change between *equivalent* nicknames as far as server
   * is concerned (user is changing the case of his/her
   * nickname or somesuch)
   */
  if (acptr == sptr)
  {

    /*
     ** No queremos que mas abajo haya cambios
     ** de modos, ya que el nick es el mismo.
     */
    if (nick_autentificado_en_bdd)
    {
      nick_equivalentes = !0;
    }

    if (strcmp(acptr->name, nick) != 0)
      /*
       * Allows change of case in his/her nick
       */
      goto nickkilldone;        /* -- go and process change */
    else
      /*
       * This is just ':old NICK old' type thing.
       * Just forget the whole thing here. There is
       * no point forwarding it to anywhere,
       * especially since servers prior to this
       * version would treat it as nick collision.
       */
      return 0;                 /* NICK Message ignored */
  }

  /*
   * Note: From this point forward it can be assumed that
   * acptr != sptr (point to different client structures).
   */
  /*
   * If the older one is "non-person", the new entry is just
   * allowed to overwrite it. Just silently drop non-person,
   * and proceed with the nick. This should take care of the
   * "dormant nick" way of generating collisions...
   */
  if (IsUnknown(acptr) && MyConnect(acptr))
  {
    IPcheck_connect_fail(acptr);
    exit_client(cptr, acptr, &me, "Overridden by other sign on");
    goto nickkilldone;
  }
  /*
   * Decide, we really have a nick collision and deal with it
   */
  if (!IsServer(cptr))
  {
    /*
     * Soporte GHOST a traves de NICK nick!password
     * Comprueba si el nick esta registrado, si es un usuario local,
     * no esta haciendo flood y ha puesto la !
     *
     * Si pone la clave correcta, se killea al usuario con sesion fantasma
     * y se propaga el KILL por la red, y se pone el nick de forma automatica;
     * en caso contrario, sigue y saldra el mensaje de nick ocupado.
     *
     * --zoltan
     */

    if (hacer_ghost && reg && (now >= cptr->nextnick))
    {
      char *passddb = NULL;

      if (*reg->valor == '{')
      {
        /* Formato nuevo JSON */
        json_object *json, *json_pass;
        enum json_tokener_error jerr = json_tokener_success;

        json = json_tokener_parse_verbose(reg->valor, &jerr);
        if (jerr != json_tokener_success)
          passddb = NULL;
        else {
          json_object_object_get_ex(json, "pass", &json_pass);
          passddb = (char *)json_object_get_string(json_pass);
        }
      } else {
        /* Formato antiguo pass */
          passddb = (char *)reg->valor;
      }

      if (passddb) {
        if (parc >= 3)
          clave_ok = verifica_clave_nick(reg->clave, passddb, parv[2]);
        else if (cptr->passbdd)
          clave_ok = verifica_clave_nick(reg->clave, passddb, cptr->passbdd);
      }
    }

    if (clave_ok)
    {
      char nickwho[NICKLEN + 2];

      if (cptr->name == NULL)
      {
        /* Ghost durante el establecimiento de la conexion
         * no tenemos informacion del nick antiguo, se a�ade ! al nick
         */
        strncpy(nickwho, nick, sizeof(nickwho));
        nickwho[strlen(nickwho)] = '!';
      }
      else
      {
        /* Ponemos su antiguo nick */
        strncpy(nickwho, cptr->name, sizeof(nickwho));
      }

      /* Matamos al usuario con un kill */
      sendto_op_mask(SNO_SERVKILL,
          "%s ha recibido KILL por %s, liberando sesion fantasma.", acptr->name,
          nickwho);
#if !defined(NO_PROTOCOL9)
      sendto_lowprot_butone(cptr, 9,  /* Kill our old drom outgoing servers */
          ":%s KILL %s :Sesion fantasma liberada por %s",
          me.name, acptr->name, nickwho);
#endif
      sendto_highprot_butone(cptr, 10,  /* Kill our old drom outgoing servers */
          "%s " TOK_KILL " %s%s :Sesion fantasma liberada por %s",
          NumServ(&me), NumNick(acptr), nickwho);

      if (MyConnect(acptr))
        sendto_prefix_one(acptr, &me,
            ":%s KILL %s :Sesion fantasma liberada por %s", me.name,
            acptr->name, nickwho);

      sendto_one(cptr,
          ":%s NOTICE GHOST :*** Sesi�n fantasma del nick %s ha sido liberada.",
          bot_nickserv ? bot_nickserv : me.name, acptr->name);

      sprintf_irc(buf2, "Killed (Sesion fantasma liberada por %s)", nickwho);
      exit_client(cptr, acptr, &me, buf2);

      goto nickkilldone;
    }
    else
    {
      /*
       * No hay ghost y mostrar mensaje de nick en uso
       */

      /*
       * NICK is coming from local client connection. Just
       * send error reply and ignore the command.
       */
      sendto_one(sptr, err_str(ERR_NICKNAMEINUSE), me.name,
          /* parv[0] is empty when connecting */
          BadPtr(parv[0]) ? "*" : parv[0], nick);
      return 0;                 /* NICK message ignored */
    }
  }
  /*
   * NICK was coming from a server connection.
   * This means we have a race condition (two users signing on
   * at the same time), or two net fragments reconnecting with the same nick.
   * The latter can happen because two different users connected
   * or because one and the same user switched server during a net break.
   * If the TimeStamps are equal, we kill both (or only 'new'
   * if it was a ":server NICK new ...").
   * Otherwise we kill the youngest when user@host differ,
   * or the oldest when they are the same.
   * We treat user and ~user as different, because if it wasn't
   * a faked ~user the AUTH wouldn't have added the '~'.
   * --Run
   *
   */
  if (IsServer(sptr))
  {
    /*
     * A new NICK being introduced by a neighbouring
     * server (e.g. message type ":server NICK new ..." received)
     */
    lastnick = atoi(parv[3]);
    differ = (strCasediff(PunteroACadena(acptr->user->username), parv[4]) ||
        strCasediff(PunteroACadena(acptr->user->host), parv[5]));
    sendto_ops("Nick collision on %s (%s " TIME_T_FMT " <- %s " TIME_T_FMT
        " (%s user@host))", acptr->name, acptr->from->name, acptr->lastnick,
        cptr->name, lastnick, differ ? "Different" : "Same");
  }
  else
  {
    /*
     * A NICK change has collided (e.g. message type ":old NICK new").
     */
    lastnick = atoi(parv[2]);
    differ =
        (strCasediff(PunteroACadena(acptr->user->username),
        PunteroACadena(sptr->user->username))
        || strCasediff(PunteroACadena(acptr->user->host),
        PunteroACadena(sptr->user->host)));
    sendto_ops("Nick change collision from %s to %s (%s " TIME_T_FMT " <- %s "
        TIME_T_FMT ")", sptr->name, acptr->name, acptr->from->name,
        acptr->lastnick, cptr->name, lastnick);
  }
  /*
   * Now remove (kill) the nick on our side if it is the youngest.
   * If no timestamp was received, we ignore the incoming nick
   * (and expect a KILL for our legit nick soon ):
   * When the timestamps are equal we kill both nicks. --Run
   * acptr->from != cptr should *always* be true (?).
   */
  if (acptr->from != cptr)
  {
    if ((differ && lastnick >= acptr->lastnick) ||
        (!differ && lastnick <= acptr->lastnick))
    {
      if (!IsServer(sptr))
      {
        ircstp->is_kill++;
#if !defined(NO_PROTOCOL9)
        sendto_lowprot_butone(cptr, 9,  /* Kill old from outgoing servers */
            ":%s KILL %s :%s (%s <- %s (Nick collision))",
            me.name, sptr->name, me.name, acptr->from->name, cptr->name);
#endif
        sendto_highprot_butone(cptr, 10,  /* Kill old from outgoing servers */
            "%s " TOK_KILL " %s%s :%s (%s <- %s (Nick collision))",
            NumServ(&me), NumNick(sptr), me.name, acptr->from->name,
            cptr->name);
        if (IsServer(cptr) && Protocol(cptr) > 9)
          sendto_one(cptr, "%s " TOK_KILL " %s%s :%s (Ghost2)",
              NumServ(&me), NumNick(sptr), me.name);
        sptr->flags |= FLAGS_KILLED;
        exit_client(cptr, sptr, &me, "Nick collision (you're a ghost)");
        sptr=NULL;
      }
      if (lastnick != acptr->lastnick)
        return 0;               /* Ignore the NICK */
    }
    sendto_one(acptr, err_str(ERR_NICKCOLLISION), me.name, acptr->name, nick);
  }
  ircstp->is_kill++;
  acptr->flags |= FLAGS_KILLED;
  if (differ)
  {
#if !defined(NO_PROTOCOL9)
    sendto_lowprot_butone(cptr, 9,  /* Kill our old from outgoing servers */
        ":%s KILL %s :%s (%s <- %s (older nick overruled))",
        me.name, acptr->name, me.name, acptr->from->name, cptr->name);
#endif
    sendto_highprot_butone(cptr, 10,  /* Kill our old from outgoing servers */
        "%s " TOK_KILL " %s%s :%s (%s <- %s (older nick overruled))",
        NumServ(&me), NumNick(acptr), me.name, acptr->from->name, cptr->name);
    if (MyConnect(acptr) && IsServer(cptr) && Protocol(cptr) > 9)
      sendto_one(cptr, "%s%s " TOK_QUIT " :Local kill by %s (Ghost)",
          NumNick(acptr), me.name);
    exit_client(cptr, acptr, &me, "Nick collision (older nick overruled)");
  }
  else
  {
#if !defined(NO_PROTOCOL9)
    sendto_lowprot_butone(cptr, 9,  /* Kill our old from outgoing servers */
        ":%s KILL %s :%s (%s <- %s (nick collision from same user@host))",
        me.name, acptr->name, me.name, acptr->from->name, cptr->name);
#endif
    sendto_highprot_butone(cptr, 10,  /* Kill our old from outgoing servers */
        "%s " TOK_KILL " %s%s :%s (%s <- %s (nick collision from same user@host))",
        NumServ(&me), NumNick(acptr), me.name, acptr->from->name, cptr->name);
    if (MyConnect(acptr) && IsServer(cptr) && Protocol(cptr) > 9)
      sendto_one(cptr,
          "%s%s " TOK_QUIT " :Local kill by %s (Ghost: switched servers too fast)",
          NumNick(acptr), me.name);
    exit_client(cptr, acptr, &me, "Nick collision (You collided yourself)");
  }
  if (lastnick == acptr->lastnick)
    return 0;

  if (!sptr)
    return 0;
  
nickkilldone:

  /*
   * Comprueba si el nick esta registrado, y
   * si lo esta que se ponga la clave correcta
   *
   * Esta comprobacion solo se va a hacer si
   * el usuario es local y no esta haciendo nick flood.
   */
  if (reg && (now >= cptr->nextnick))
  {
    char *nombre;
    int nick_forbid = 0;
    int flagsddb = 0;
    char *passddb = NULL;

    if (*reg->valor == '{')
    {
      /* Formato nuevo JSON */
      json_object *json, *json_flags, *json_reason, *json_pass, *json_automodes;
      enum json_tokener_error jerr = json_tokener_success;

      json = json_tokener_parse_verbose(reg->valor, &jerr);
      if (jerr == json_tokener_success) {
        json_object_object_get_ex(json, "flags", &json_flags);
        flagsddb = json_object_get_int(json_flags);
        json_object_object_get_ex(json, "reason", &json_reason);
        reason = (char *)json_object_get_string(json_reason);
        json_object_object_get_ex(json, "pass", &json_pass);
        passddb = (char *)json_object_get_string(json_pass);
        json_object_object_get_ex(json, "automodes", &json_automodes);
        automodes = (char *)json_object_get_string(json_automodes);
      }
    } else {
      /* Formato antiguo pass_flag
       * Si el ultimo caracter de la clave (reg->valor) contiene:
       *  '+'  El nick esta suspendido.
       *  '*'  El nick esta prohibido (forbid).
       * Cualquier otro caracter, el nick esta activo.
       */
      char c;
      c = reg->valor[strlen(reg->valor) - 1];
      if (c == '+')
        flagsddb = DDB_NICK_SUSPEND;
      else if (c == '*')
        flagsddb = DDB_NICK_FORBID;

      passddb = reg->valor;
    }

    if (flagsddb == DDB_NICK_SUSPEND)
      nick_suspendido = 1;
    else if (flagsddb == DDB_NICK_FORBID)
      nick_forbid = 1;

    /*
     * Si el usuario ha hecho ghost, no se necesita
     * verificar la clave de nuevo ya que se ha hecho
     * antes al hacer el ghost.
     */
    if (!clave_ok && !nick_forbid)
    {
      if (nick_autentificado_en_bdd && !strCasediff(parv[0], nick))
      {
        nick_equivalentes = 1;
        clave_ok = 1;
      }
      else
      {
        if (parc >= 3)
        {
          clave_ok = verifica_clave_nick(reg->clave, passddb, parv[2]);
        }
        else if (cptr->passbdd)
        {
          clave_ok = verifica_clave_nick(reg->clave, passddb, cptr->passbdd);
        }
      }
    }

    if (*parv[0])
      nombre = parv[0];
    else
      nombre = nick;

    if (clave_ok && (!nick_suspendido || (nick_suspendido && permite_nicks_suspend)))
    {
      if (sptr->name == NULL)
      {
        /*
         * Si no tiene nick anterior (acaba de conectarse)
         * mandar este notice.
         * El notice de autentificacion de cuando cambia nick
         * esta mas abajo.
         */
        if (!nick_equivalentes)
          sendto_one(cptr,
              ":%s NOTICE %s :*** Contrase�a aceptada. Bienvenid@ a casa ;)",
              bot_nickserv ? bot_nickserv : me.name, nick);

        if (nick_suspendido)
          sendto_one(cptr,
              ":%s NOTICE %s :*** Tu nick %s est� suspendido. Motivo: %s",
              bot_nickserv ? bot_nickserv : me.name, nick, nick, reason ? reason : "N.D.");
      }
      hflag = '+';

    }
    else if (nick_suspendido && !permite_nicks_suspend)
    {
      sendto_one(cptr,
          ":%s NOTICE %s :*** El nick %s esta suspendido, no puede ser utilizado. Motivo: %s",
          bot_nickserv ? bot_nickserv : me.name, nombre, nick, reason ? reason : "N.D.");
      sendto_one(cptr, ":%s %d %s %s :Nickname is suspended, can not be used - El nick esta suspendido, no puede ser utilizado",
          me.name, ERR_NICKNAMEINUSE, parv[0], nick);
      return 0;
    }
    else if (nick_forbid)
    {
      sendto_one(cptr,
          ":%s NOTICE %s :*** El nick %s est� prohibido, no puede ser utilizado. Motivo: %s",
          bot_nickserv ? bot_nickserv : me.name, nombre, nick, reason ? reason : "N.D.");
      sendto_one(cptr, ":%s %d %s %s :Nickname is forbided, can not be used - El nick est� prohibido, no puede ser utilizado",
          me.name, ERR_NICKNAMEINUSE, parv[0], nick);
      return 0;
    }
    else
    {
      if (parc < 3)
      {
        sendto_one(cptr,
            ":%s NOTICE %s :*** El nick %s est� Registrado, necesitas contrase�a.",
            bot_nickserv ? bot_nickserv : me.name, nombre, nick);
      }
      else
      {
        sendto_one(cptr,
            ":%s NOTICE %s :*** Contrase�a Incorrecta para el nick %s.",
            bot_nickserv ? bot_nickserv : me.name, nombre, nick);
      }

      sendto_one(cptr,
          ":%s NOTICE %s :*** Utiliza \002/NICK %s%sclave\002 para identificarte.",
          bot_nickserv ? bot_nickserv : me.name, nombre, nick, hacer_ghost ? "!" : ":");

      sendto_one(cptr, ":%s %d %s %s :Nickname is registered (missing or wrong password) - "
          "El nick est� registrado (contrase�a ausente o incorrecta)",
          me.name, ERR_NICKREGISTERED, parv[0], nick);

      return 0;
    }
  }

  if (IsServer(sptr))
  {
    int flag, *s;
    char *p;
#if !defined(NO_PROTOCOL9)
    const char *nnp9 = NULL;    /* Init. to avoid compiler warning */
#endif

    /* A server introducing a new client, change source */
    if (!server)
      server = sptr;
#if !defined(NO_PROTOCOL9)
    /*
     * Numeric Nicks does, in contrast to all other protocol enhancements,
     * translation from protocol 9 -> protocol 10 !
     * The reason is that I just can't know what protocol it is when I
     * receive a "MODE #channel +o Run", because I can't 'find' "Run"
     * before I know the protocol, and I can't know the protocol if I
     * first have to find the server of "Run".
     * Therefore, in THIS case, the protocol is determined by the Connected
     * server: cptr.
     */
    if (Protocol(cptr) < 10 && !(nnp9 = CreateNNforProtocol9server(server)))
      return exit_client_msg(cptr, server, &me,
          "Too many clients (> %d) from P09 server (%s)", 64, server->name);
#endif
    sptr = make_client(cptr, STAT_UNKNOWN);
    sptr->hopcount = atoi(parv[2]);
    sptr->lastnick = atoi(parv[3]);
    if (Protocol(cptr) > 9 && parc > 7 && *parv[6] == '+')
      for (p = parv[6] + 1; *p; p++)
      {
        for (s = user_modes; (flag = *s); s += 2)
          if (((char)*(s + 1)) == *p)
          {
            sptr->flags |= flag;
            break;
          }
        for (s = user_hmodes; (flag = *s); s += 2)
          if (((char)*(s + 1)) == *p)
          {
            sptr->hmodes |= flag;
            break;
          }
      }
    /*
     * Set new nick name.
     */
    SlabStringAllocDup(&(sptr->name), nick, 0);
    sptr->user = make_user(sptr);
    sptr->user->server = server;
#if !defined(NO_PROTOCOL9)
    if (Protocol(cptr) < 10)
    {
      SetRemoteNumNick(sptr, nnp9);
      memset(&sptr->ip, 0, sizeof(struct irc_in_addr));
    }
    else
    {
#endif
      SetRemoteNumNick(sptr, parv[parc - 2]);
      base64toip(parv[parc - 3], &sptr->ip);
      /* IP# of remote client */
#if !defined(NO_PROTOCOL9)
    }
#endif
    add_client_to_list(sptr);
    hAddClient(sptr);
#if defined(BDD_VIP)
/*
** Necesitamos hacer la verificacion porque se puede invocar esta
** rutina con un usuario no inicializado del todo (recien conectado)
** que todavia no esta marcado como "user".
*/
    if (IsUser(sptr) && IsVhostPerso(sptr)) {
      BorraIpVirtualPerso(sptr);
      vhperso = 1;
    }
#endif
    server->serv->ghost = 0;    /* :server NICK means end of net.burst */
    SlabStringAllocDup(&(sptr->info), parv[parc - 1], REALLEN);
    SlabStringAllocDup(&(sptr->user->host), parv[5], HOSTLEN);
    return register_user(cptr, sptr, sptr->name, parv[4]);

    /*
     * Avisamos a sus contactos que el nick
     * ha entrado en la red.
     * (Nuevo usuario remoto)
     */
    chequea_estado_watch(sptr, RPL_LOGON);

  }
  else if (sptr->name)
  {
    /*
     * Client changing its nick
     *
     * If the client belongs to me, then check to see
     * if client is on any channels where it is currently
     * banned.  If so, do not allow the nick change to occur.
     */
    if (IsUser(sptr))
    {
      for (lp = cptr->user->channel; lp; lp = lp->next)
        if (can_send(cptr, lp->value.chptr) == MODE_BAN)
        {
          sendto_one(cptr, err_str(ERR_BANNICKCHANGE), me.name, parv[0],
              lp->value.chptr->chname);
          return 0;
        }
      /*
       * Refuse nick change if the last nick change was less
       * then 30 seconds ago. This is intended to get rid of
       * clone bots doing NICK FLOOD. -SeKs
       * If someone didn't change their nick for more then 60 seconds
       * however, allow to do two nick changes immedately after another
       * before limiting the nick flood. -Run
       * Un +k puede cambiarse de nick todas las veces seguidas que
       * quiera sin limitaciones. -NiKoLaS
       */
      if ((now < cptr->nextnick) && !(IsChannelService(cptr)))
      {
        cptr->nextnick += 2;
        /* Send error message */
        sendto_one(cptr, err_str(ERR_NICKTOOFAST),
            me.name, parv[0], parv[1], cptr->nextnick - now);
        /*
         * La siguiente linea, informaba solamente al cliente
         * del cambio de nick, aun cuando este no se permite
         * por ocurrir demasiado rapido. El cliente veia
         * "sunick is now known as sunick"
         */
        // sendto_prefix_one(cptr, cptr, ":%s NICK %s", parv[0], parv[0]);
        /* bounce NICK to user */
        return 0;               /* ignore nick change! */
      }
      else if (IsAnOper(cptr))
      {
        cptr->nextnick = now;
      }
      else
      {
        /* Limit total to 1 change per NICK_DELAY seconds: */
        cptr->nextnick += NICK_DELAY;
        /* However allow _maximal_ 1 extra consecutive nick change: */
        if (cptr->nextnick < now)
          cptr->nextnick = now;
      }

      /*
       * El mensaje de autentificacion y/o suspension del nick
       * ha de salir despues del chequeo de si tiene bans en algun
       * canal, porque si estabas baneado salia primero el mensaje
       * de nick identificado y luego no podias ponerte el nick
       *
       * --zoltan
       */
      if (clave_ok)
      {
        if (!nick_equivalentes)
          sendto_one(cptr,
              ":%s NOTICE %s :*** Contrase�a aceptada. Bienvenid@ a casa ;)",
              bot_nickserv ? bot_nickserv : me.name, sptr->name);

        if (nick_suspendido)
          sendto_one(cptr,
              ":%s NOTICE %s :*** Tu nick %s est� suspendido.",
              bot_nickserv ? bot_nickserv : me.name, sptr->name, nick);
      }

    }
    /*
     * Also set 'lastnick' to current time, if changed.
     */
    if (strCasediff(parv[0], nick))
      sptr->lastnick = (sptr == cptr) ? TStime() : atoi(parv[2]);

    /*
     * Client just changing his/her nick. If he/she is
     * on a channel, send note of change to all clients
     * on that channel. Propagate notice to other servers.
     */
    if (IsUser(sptr))
    {
      if (!nick_equivalentes)
      {
        int of, oh;

        /*
         * Avisamos a sus contactos que el nick
         * ha salido (ha cambiado de nick).
         * (Cambio de nick local y remoto)
         */
        if (!nick_equivalentes)
            chequea_estado_watch(sptr, RPL_LOGOFF);

        of = sptr->flags;
        oh = sptr->hmodes;

        if (IsNickRegistered(sptr) || IsNickSuspended(sptr))
        {                       /* Parche DB69 */
          ClearNickRegistered(sptr);
          ClearNickSuspended(sptr);
          ClearMsgOnlyReg(sptr);
          ClearNoChan(sptr);
          ClearDocking(sptr);
          ClearHelpOp(sptr);
          ClearAdmin(sptr);
          ClearCoder(sptr);
          if (IsOper(sptr))
            --nrof.opers;
          ClearOper(sptr);
          ClearServicesBot(sptr);
#if defined (BDD_VIP)
#if !defined(BDD_VIP2)
          ClearHidden(sptr);
#endif
          if (IsVhostPerso(sptr)) {
            BorraIpVirtualPerso(sptr);
            vhperso = 1;
          }
#endif
          if (!IsOperCmd(sptr)) {
            ClearHiddenViewer(sptr);
            ClearNoIdle(sptr);
          }
          send_umode_out(cptr, sptr, of, oh, IsRegistered(sptr));
        }
      }

      sendto_common_channels(sptr, ":%s NICK :%s", parv[0], nick);

      add_history(sptr, 1);
#if defined(NO_PROTOCOL9)
      sendto_serv_butone(cptr,
          "%s%s " TOK_NICK " %s " TIME_T_FMT, NumNick(sptr), nick, sptr->lastnick);
#else
      sendto_lowprot_butone(cptr, 9,
          ":%s NICK %s " TIME_T_FMT, parv[0], nick, sptr->lastnick);
      sendto_highprot_butone(cptr, 10,
          "%s%s " TOK_NICK " %s " TIME_T_FMT, NumNick(sptr), nick, sptr->lastnick);
#endif
    }
    else
    {                           /* El cambio proviene de otro server */
      sendto_one(sptr, ":%s NICK :%s", parv[0], nick);
    }
    if (sptr->name)
    {
      hRemClient(sptr);
    }
    SlabStringAllocDup(&(sptr->name), nick, 0);
    hAddClient(sptr);
  }
  else
  {
    int of, oh;

    /* Local client setting NICK the first time */

    SlabStringAllocDup(&(sptr->name), nick, 0);
    if (!sptr->user)
    {
      sptr->user = make_user(sptr);
      sptr->user->server = &me;
    }
    SetLocalNumNick(sptr);
    hAddClient(sptr);

    /*
     * If the client hasn't gotten a cookie-ping yet,
     * choose a cookie and send it. -record!jegelhof@cloud9.net
     */
    if (!sptr->cookie)
    {
      do
      {
        sptr->cookie = ircrandom() & 0x7fffffff;
      }
      while ((!sptr->cookie) || IsCookieVerified(sptr));

      sendto_one(cptr, "PING :%u", sptr->cookie);
    }
    else if (sptr->user->host && IsCookieVerified(sptr))
    {
      /*
       * USER and PONG already received, now we have NICK.
       * register_user may reject the client and call exit_client
       * for it - must test this and exit m_nick too !
       */
      sptr->lastnick = TStime();  /* Always local client */
      if (register_user(cptr, sptr, nick,
          PunteroACadena(sptr->user->username)) == CPTR_KILLED)
        return CPTR_KILLED;
    }
/*
** Se llega aqui cuando el usuario entra
*/
    of = sptr->flags;
    oh = sptr->hmodes;
#if defined(BDD_VIP)
#if !defined(BDD_VIP2)
    if (nick_suspendido)
    {
      ClearHidden(sptr);
    }
    else if (db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
    {
      SetHidden(sptr);          /* Tiene una ip virtual personalizada */
      SetVhostPerso(sptr);
    }
    else
    {
      ClearHidden(sptr);        /* desoculto */
    }
#else
    SetHidden(sptr);            /* lo oculto */
#endif /* !defined(BDD_VIP2) */
#endif /* defined(BDD_VIP) */

   if (auto_invisible && !excepcion_invisible)
     SetInvisible(sptr);

   if (find_port_ssl(sptr))
     SetSSL(sptr);

    send_umode_out(cptr, sptr, of, oh, IsRegistered(sptr));
  }

/*
** Se llega aqui cuando hay un cambio de nick o el usuario entra
*/

  if (!nick_equivalentes)
  {
    int of, oh;
    int statusbdd = 0;;

    of = sptr->flags;
    oh = sptr->hmodes;

    if (hflag == '+')
    {
      if (nick_suspendido)      /* por si era nuevo */
        SetNickSuspended(sptr);
      else
        SetNickRegistered(sptr);
#if defined(BDD_VIP)
#if defined(BDD_VIP2)           /* Pongo +x al usuario si el ircd esta
                                 * compilado de forma que solo los usuarios
                                 * registrados puedan tener +x, en el caso
                                 * contrario dejo el modo tal como esta.
                                 */
      SetHidden(sptr);
      if (db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
        SetVhostPerso(sptr);
#else
      /* Puede que tenga una ip virtual personalizada */
      if (nick_suspendido)
      {
        ClearHidden(sptr);
      }
      else if (db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
      {
        SetHidden(sptr);
        SetVhostPerso(sptr);
      }
      else
      {
        ClearHidden(sptr);
      }
#endif
#endif

      statusbdd = get_status(sptr);
      if (statusbdd && !nick_suspendido)
      {
        if (statusbdd & HMODE_ADMIN)
          SetAdmin(sptr);

        if (statusbdd & HMODE_CODER)
          SetCoder(sptr);

        if (statusbdd & HMODE_SERVICESBOT)
          SetServicesBot(sptr);

        if (statusbdd & HMODE_HELPOP)
          SetHelpOp(sptr);

        if (statusbdd & FLAGS_OPER)
        {
          SetOper(sptr);
          nrof.opers++;
          sptr->flags |= (FLAGS_WALLOP | FLAGS_SERVNOTICE | FLAGS_DEBUG);
          set_snomask(sptr, SNO_OPERDEFAULT, SNO_ADD);
          set_privs(sptr);
        }

      } else {
        ClearHelpOp(sptr);
        ClearAdmin(sptr);
        ClearCoder(sptr);
        if (IsOper(sptr))
          --nrof.opers;
        ClearOper(sptr);
        ClearServicesBot(sptr);
      }

      if (automodes && !nick_suspendido)
      {
        int addflags, addhmodes;

        mask_user_flags(automodes, &addflags, &addhmodes);
        sptr->flags |= addflags;
        sptr->hmodes |= addhmodes;
      }
    }
    else
    {
      ClearNickRegistered(sptr);  /* por si era nuevo */
      ClearNickSuspended(sptr);
      ClearMsgOnlyReg(sptr);
      ClearNoChan(sptr);
      ClearDocking(sptr);
      ClearHelpOp(sptr);
      ClearAdmin(sptr);
      ClearCoder(sptr);
      if (IsOper(sptr))
        --nrof.opers;
      ClearOper(sptr);
      ClearServicesBot(sptr);
    }
#if defined(BDD_VIP)
    if (IsUser(sptr))
    {
      if (IsHidden(sptr))
      {
        if (IsVhostPerso(sptr)) {
          make_vhostperso(sptr, 1);
        } else {
          /* Tenia vhost personalizada, ahora mandamos
           * cambio para que el usuario tenga constancia.
           */
          if (vhperso)
            sendto_one(sptr, rpl_str(RPL_HOSTHIDDEN), me.name, sptr->name,
                get_virtualhost(sptr, 0));
        }
      }
    }
#endif /* defined(BDD_VIP) */
    send_umode_out(cptr, sptr, of, oh, IsRegistered(sptr));
  }

  /*
   * Avisamos a sus contactos que el nick
   * ha entrado (ha puesto el nick).
   * (Cambio de nick local y remoto).
   */

  if (IsUser(sptr) && !nick_equivalentes)
  {
    chequea_estado_watch(sptr, RPL_LOGON);
  }

  return 0;
}

/*
 * m_nick_remoto
 *
 * parv[0] = sender prefix
 * parv[1] = nickname (:clave opcional)
 * parv[2] = clave (opcional)
 *
 * If from server, source is client:
 *   parv[2] = timestamp
 *   parv[3] = modes (optional)
 *
 * Source is server:
 *   parv[2] = hopcount
 *   parv[3] = timestamp
 *   parv[4] = username
 *   parv[5] = hostname
 *   parv[6] = umode (optional)
 *   parv[parc-3] = IP#                 <- Only Protocol >= 10
 *   parv[parc-2] = YXX, numeric nick   <- Only Protocol >= 10
 *   parv[parc-1] = info
 *   parv[0] = server
 */
int m_nick_remoto(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  int nick_equivalentes = 0;
  int nick_autentificado_en_bdd = 0;
  aClient *acptr;
  aClient *server = NULL;
  char nick[NICKLEN + 2];
  time_t lastnick = (time_t) 0;
  int differ = 1;

  assert(!MyConnect(sptr));
  assert(IsServer(cptr));

  ClearWatch(sptr);             /* Nos curamos en salud */

  nick_autentificado_en_bdd = IsNickRegistered(sptr) || IsNickSuspended(sptr);

  if ((IsServer(sptr) && parc < 8) || (parc < 3))
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "NICK");
    sendto_ops("bad NICK param count for %s from %s", parv[1], cptr->name);
    return 0;
  }
  strncpy(nick, parv[1], NICKLEN + 1);
  nick[sizeof(nick) - 1] = 0;

  /*
   * If do_nick_name() returns a null name OR if the server sent a nick
   * name and do_nick_name() changed it in some way (due to rules of nick
   * creation) then reject it. If from a server and we reject it,
   * and KILL it. -avalon 4/4/92
   */
  if (do_nick_name(nick) == 0 || (strcmp(nick, parv[1])))
  {
    sendto_one(sptr, err_str(ERR_INVALIDNICKNAME), me.name, parv[0], parv[1]);

    ircstp->is_kill++;
    sendto_ops("Bad Nick: %s From: %s %s", parv[1], parv[0], cptr->name);
    if (Protocol(cptr) < 10)
    {
      sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
          me.name, parv[1], me.name, parv[1], nick, cptr->name);
    }
    else
    {
      sendto_one(cptr, "%s " TOK_KILL " %s :%s (%s <- %s[%s])",
          NumServ(&me), IsServer(sptr) ? parv[parc - 2] : parv[0], me.name,
          parv[1], nick, cptr->name);
    }
    if (!IsServer(sptr))        /* bad nick _change_ */
    {
#if !defined(NO_PROTOCOL9)
      sendto_lowprot_butone(cptr, 9, ":%s KILL %s :%s (%s <- %s!%s@%s)",
          me.name, parv[0], me.name, cptr->name, parv[0],
          sptr->user ? PunteroACadena(sptr->username) : "",
          sptr->user ? sptr->user->server->name : cptr->name);
#endif
      sendto_highprot_butone(cptr, 10, "%s " TOK_KILL " %s :%s (%s <- %s!%s@%s)",
          NumServ(&me), parv[0], me.name, cptr->name,
          parv[0], sptr->user ? PunteroACadena(sptr->username) : "",
          sptr->user ? sptr->user->server->name : cptr->name);
      sptr->flags |= FLAGS_KILLED;
      return exit_client(cptr, sptr, &me, "BadNick");
    }
    return 0;
  }

  /*
   * Check against nick name collisions.
   *
   * Put this 'if' here so that the nesting goes nicely on the screen :)
   * We check against server name list before determining if the nickname
   * is present in the nicklist (due to the way the below for loop is
   * constructed). -avalon
   */
  if ((acptr = FindServer(nick)))
  {
    /*
     * We have a nickname trying to use the same name as
     * a server. Send out a nick collision KILL to remove
     * the nickname. As long as only a KILL is sent out,
     * there is no danger of the server being disconnected.
     * Ultimate way to jupiter a nick ? >;-). -avalon
     */
    sendto_ops("Nick collision on %s(%s <- %s)",
        sptr->name, acptr->from->name, cptr->name);
    ircstp->is_kill++;
    if (Protocol(cptr) < 10)
      sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
          me.name, sptr->name, me.name, acptr->from->name, cptr->name);
    else
      sendto_one(cptr, "%s " TOK_KILL " %s%s :%s (%s <- %s)",
          NumServ(&me), NumNick(sptr), me.name, acptr->from->name,
          /*
           * NOTE: Cannot use get_client_name twice here, it returns static
           *       string pointer--the other info would be lost.
           */
          cptr->name);
    sptr->flags |= FLAGS_KILLED;
    return exit_client(cptr, sptr, &me, "Nick/Server collision");
  }

  if (!(acptr = FindClient(nick)))
    goto nickkilldone;          /* No collisions, all clear... */
  /*
   * If acptr == sptr, then we have a client doing a nick
   * change between *equivalent* nicknames as far as server
   * is concerned (user is changing the case of his/her
   * nickname or somesuch)
   */
  if (acptr == sptr)
  {

    /*
     ** No queremos que mas abajo haya cambios
     ** de modos, ya que el nick es el mismo.
     */
    if (nick_autentificado_en_bdd)
    {
      nick_equivalentes = !0;
    }

    if (strcmp(acptr->name, nick) != 0)
      /*
       * Allows change of case in his/her nick
       */
      goto nickkilldone;        /* -- go and process change */
    else
      /*
       * This is just ':old NICK old' type thing.
       * Just forget the whole thing here. There is
       * no point forwarding it to anywhere,
       * especially since servers prior to this
       * version would treat it as nick collision.
       */
      return 0;                 /* NICK Message ignored */
  }

  /*
   * Note: From this point forward it can be assumed that
   * acptr != sptr (point to different client structures).
   */
  /*
   * If the older one is "non-person", the new entry is just
   * allowed to overwrite it. Just silently drop non-person,
   * and proceed with the nick. This should take care of the
   * "dormant nick" way of generating collisions...
   */
  if (IsUnknown(acptr) && MyConnect(acptr))
  {
    IPcheck_connect_fail(acptr);
    exit_client(cptr, acptr, &me, "Overridden by other sign on");
    goto nickkilldone;
  }

  /*
   * NICK was coming from a server connection.
   * This means we have a race condition (two users signing on
   * at the same time), or two net fragments reconnecting with the same nick.
   * The latter can happen because two different users connected
   * or because one and the same user switched server during a net break.
   * If the TimeStamps are equal, we kill both (or only 'new'
   * if it was a ":server NICK new ...").
   * Otherwise we kill the youngest when user@host differ,
   * or the oldest when they are the same.
   * We treat user and ~user as different, because if it wasn't
   * a faked ~user the AUTH wouldn't have added the '~'.
   * --Run
   *
   */
  if (IsServer(sptr))
  {
    /*
     * A new NICK being introduced by a neighbouring
     * server (e.g. message type ":server NICK new ..." received)
     */
    lastnick = atoi(parv[3]);
    differ = (strCasediff(PunteroACadena(acptr->user->username), parv[4]) ||
        strCasediff(PunteroACadena(acptr->user->host), parv[5]));
    sendto_ops("Nick collision on %s (%s " TIME_T_FMT " <- %s " TIME_T_FMT
        " (%s user@host))", acptr->name, acptr->from->name, acptr->lastnick,
        cptr->name, lastnick, differ ? "Different" : "Same");
  }
  else
  {
    /*
     * A NICK change has collided (e.g. message type ":old NICK new").
     */
    lastnick = atoi(parv[2]);
    differ =
        (strCasediff(PunteroACadena(acptr->user->username),
        PunteroACadena(sptr->user->username))
        || strCasediff(PunteroACadena(acptr->user->host),
        PunteroACadena(sptr->user->host)));
    sendto_ops("Nick change collision from %s to %s (%s " TIME_T_FMT " <- %s "
        TIME_T_FMT ")", sptr->name, acptr->name, acptr->from->name,
        acptr->lastnick, cptr->name, lastnick);
  }
  /*
   * Now remove (kill) the nick on our side if it is the youngest.
   * If no timestamp was received, we ignore the incoming nick
   * (and expect a KILL for our legit nick soon ):
   * When the timestamps are equal we kill both nicks. --Run
   * acptr->from != cptr should *always* be true (?).
   */
  if (acptr->from != cptr)
  {
    if ((differ && lastnick >= acptr->lastnick) ||
        (!differ && lastnick <= acptr->lastnick))
    {
      if (!IsServer(sptr))
      {
        ircstp->is_kill++;
#if !defined(NO_PROTOCOL9)
        sendto_lowprot_butone(cptr, 9,  /* Kill old from outgoing servers */
            ":%s KILL %s :%s (%s <- %s (Nick collision))",
            me.name, sptr->name, me.name, acptr->from->name, cptr->name);
#endif
        sendto_highprot_butone(cptr, 10,  /* Kill old from outgoing servers */
            "%s " TOK_KILL " %s%s :%s (%s <- %s (Nick collision))",
            NumServ(&me), NumNick(sptr), me.name, acptr->from->name,
            cptr->name);
        sptr->flags |= FLAGS_KILLED;
        exit_client(cptr, sptr, &me, "Nick collision (you're a ghost)");
        sptr=NULL;
      }
      if (lastnick != acptr->lastnick)
        return 0;               /* Ignore the NICK */
    }
    sendto_one(acptr, err_str(ERR_NICKCOLLISION), me.name, acptr->name, nick);
  }
  ircstp->is_kill++;
  acptr->flags |= FLAGS_KILLED;
  if (differ)
  {
#if !defined(NO_PROTOCOL9)
    sendto_lowprot_butone(cptr, 9,  /* Kill our old from outgoing servers */
        ":%s KILL %s :%s (%s <- %s (older nick overruled))",
        me.name, acptr->name, me.name, acptr->from->name, cptr->name);
#endif
    sendto_highprot_butone(cptr, 10,  /* Kill our old from outgoing servers */
        "%s " TOK_KILL " %s%s :%s (%s <- %s (older nick overruled))",
        NumServ(&me), NumNick(acptr), me.name, acptr->from->name, cptr->name);
    if (MyConnect(acptr) && Protocol(cptr) > 9)
      sendto_one(cptr, "%s%s " TOK_QUIT " :Local kill by %s (Ghost)",
          NumNick(acptr), me.name);
    exit_client(cptr, acptr, &me, "Nick collision (older nick overruled)");
  }
  else
  {
#if !defined(NO_PROTOCOL9)
    sendto_lowprot_butone(cptr, 9,  /* Kill our old from outgoing servers */
        ":%s KILL %s :%s (%s <- %s (nick collision from same user@host))",
        me.name, acptr->name, me.name, acptr->from->name, cptr->name);
#endif
    sendto_highprot_butone(cptr, 10,  /* Kill our old from outgoing servers */
        "%s " TOK_KILL " %s%s :%s (%s <- %s (nick collision from same user@host))",
        NumServ(&me), NumNick(acptr), me.name, acptr->from->name, cptr->name);
    if (MyConnect(acptr) && Protocol(cptr) > 9)
      sendto_one(cptr,
          "%s%s " TOK_QUIT " :Local kill by %s (Ghost: switched servers too fast)",
          NumNick(acptr), me.name);
    exit_client(cptr, acptr, &me, "Nick collision (You collided yourself)");
  }
  if (lastnick == acptr->lastnick)
    return 0;

  if (!sptr)
    return 0;

nickkilldone:

  if (IsServer(sptr))
  {
    int flag, *s;
    char *p;
#if !defined(NO_PROTOCOL9)
    const char *nnp9 = NULL;    /* Init. to avoid compiler warning */
#endif

    /* A server introducing a new client, change source */
    if (!server)
      server = sptr;
#if !defined(NO_PROTOCOL9)
    /*
     * Numeric Nicks does, in contrast to all other protocol enhancements,
     * translation from protocol 9 -> protocol 10 !
     * The reason is that I just can't know what protocol it is when I
     * receive a "MODE #channel +o Run", because I can't 'find' "Run"
     * before I know the protocol, and I can't know the protocol if I
     * first have to find the server of "Run".
     * Therefore, in THIS case, the protocol is determined by the Connected
     * server: cptr.
     */
    if (Protocol(cptr) < 10 && !(nnp9 = CreateNNforProtocol9server(server)))
      return exit_client_msg(cptr, server, &me,
          "Too many clients (> %d) from P09 server (%s)", 64, server->name);
#endif
    sptr = make_client(cptr, STAT_UNKNOWN);
    sptr->hopcount = atoi(parv[2]);
    sptr->lastnick = atoi(parv[3]);
    if (Protocol(cptr) > 9 && parc > 7 && *parv[6] == '+')
      for (p = parv[6] + 1; *p; p++)
      {
        for (s = user_modes; (flag = *s); s += 2)
          if (((char)*(s + 1)) == *p)
          {
            sptr->flags |= flag;
            break;
          }
        for (s = user_hmodes; (flag = *s); s += 2)
          if (((char)*(s + 1)) == *p)
          {
            sptr->hmodes |= flag;
            break;
          }
      }
    /*
     * Set new nick name.
     */
    SlabStringAllocDup(&(sptr->name), nick, 0);
    sptr->user = make_user(sptr);
    sptr->user->server = server;
#if !defined(NO_PROTOCOL9)
    if (Protocol(cptr) < 10)
    {
      SetRemoteNumNick(sptr, nnp9);
      memset(&sptr->ip, 0, sizeof(struct irc_in_addr));
    }
    else
    {
#endif
      SetRemoteNumNick(sptr, parv[parc - 2]);
      base64toip(parv[parc - 3], &sptr->ip);
      /* IP# of remote client */
#if !defined(NO_PROTOCOL9)
    }
#endif
    add_client_to_list(sptr);
    hAddClient(sptr);

#if defined(BDD_VIP)
  /* Si tiene +r y vhost, lo marcamos */
  if (IsNickRegistered(sptr) && db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
  {
    SetVhostPerso(sptr);
  }
#endif


    server->serv->ghost = 0;    /* :server NICK means end of net.burst */
    SlabStringAllocDup(&(sptr->info), parv[parc - 1], REALLEN);
    SlabStringAllocDup(&(sptr->user->host), parv[5], HOSTLEN);
    return register_user(cptr, sptr, sptr->name, parv[4]);

    /*
     * Avisamos a sus contactos que el nick
     * ha entrado en la red.
     * (Nuevo usuario remoto)
     */
    chequea_estado_watch(sptr, RPL_LOGON);

  }
  else if (sptr->name)
  {
    /*
     * Also set 'lastnick' to current time, if changed.
     */
    if (strCasediff(parv[0], nick))
      sptr->lastnick = (sptr == cptr) ? TStime() : atoi(parv[2]);

    /*
     * Client just changing his/her nick. If he/she is
     * on a channel, send note of change to all clients
     * on that channel. Propagate notice to other servers.
     */
    if (IsUser(sptr))
    {
      if (!nick_equivalentes)
      {
        int of, oh;

        /*
         * Avisamos a sus contactos que el nick
         * ha salido (ha cambiado de nick).
         * (Cambio de nick local y remoto)
         */
        if (!nick_equivalentes)
            chequea_estado_watch(sptr, RPL_LOGOFF);

        of = sptr->flags;
        oh = sptr->hmodes;

        if (IsNickRegistered(sptr) || IsNickSuspended(sptr))
        {                       /* Parche DB69 */
          ClearNickRegistered(sptr);
          ClearNickSuspended(sptr);
          ClearMsgOnlyReg(sptr);
          ClearNoChan(sptr);
          ClearDocking(sptr);
          ClearHelpOp(sptr);
          ClearAdmin(sptr);
          ClearCoder(sptr);
          if (IsOper(sptr))
            --nrof.opers;
          ClearOper(sptr);
          ClearServicesBot(sptr);
          if (!IsOperCmd(sptr)) {
            ClearHiddenViewer(sptr);
            ClearNoIdle(sptr);
          }
          send_umode_out(cptr, sptr, of, oh, IsRegistered(sptr));
        }
      }

      sendto_common_channels(sptr, ":%s NICK :%s", parv[0], nick);

      add_history(sptr, 1);
      if (parc > 3 && *parv[3]=='+') {
        int flag, *s;
        char *p;

        for (p = parv[3] + 1; *p; p++)
        {
          for (s = user_modes; (flag = *s); s += 2)
            if (((char)*(s + 1)) == *p)
            {
              sptr->flags |= flag;
              break;
            }
          for (s = user_hmodes; (flag = *s); s += 2)
            if (((char)*(s + 1)) == *p)
            {
              sptr->hmodes |= flag;
              break;
            }
        }

#if defined(NO_PROTOCOL9)
        sendto_serv_butone(cptr,
            "%s%s " TOK_NICK " %s " TIME_T_FMT " %s", NumNick(sptr), nick, sptr->lastnick, parv[3]);
#else
        sendto_lowprot_butone(cptr, 9,
            ":%s NICK %s " TIME_T_FMT " %s", parv[0], nick, sptr->lastnick, parv[3]);
        sendto_highprot_butone(cptr, 10,
            "%s%s " TOK_NICK " %s " TIME_T_FMT " %s", NumNick(sptr), nick, sptr->lastnick, parv[3]);
#endif
      } else {
#if defined(NO_PROTOCOL9)
        sendto_serv_butone(cptr,
            "%s%s " TOK_NICK " %s " TIME_T_FMT, NumNick(sptr), nick, sptr->lastnick);
#else
        sendto_lowprot_butone(cptr, 9,
            ":%s NICK %s " TIME_T_FMT, parv[0], nick, sptr->lastnick);
        sendto_highprot_butone(cptr, 10,
            "%s%s " TOK_NICK " %s " TIME_T_FMT, NumNick(sptr), nick, sptr->lastnick);
#endif
      }
    }
    else
    { /* Si no es un usuario quien se intenta cambiar el nick salgo */
      return 0;
    }
    if (sptr->name)
    {
      hRemClient(sptr);
    }
    SlabStringAllocDup(&(sptr->name), nick, 0);
    hAddClient(sptr);
#if defined(BDD_VIP)
    if (IsVhostPerso(sptr)) {
      BorraIpVirtualPerso(sptr);
    }
#endif
  }
  else
  {
    return 0;
  }

/*
** Se llega aqui cuando hay un cambio de nick o el usuario entra
*/

  /*
   * Avisamos a sus contactos que el nick
   * ha entrado (ha puesto el nick).
   * (Cambio de nick local y remoto).
   */

  if (IsUser(sptr) && !nick_equivalentes)
  {
    /* Para nicks remotos, no enviamos el */
    /* WATCH para nicks en las tablas 'v' */
    /* y lo marcamos para enviarlo        */
    /* despues desde m_umode              */
    if (!db_buscar_registro(BDD_IPVIRTUALDB, sptr->name))
    {
      chequea_estado_watch(sptr, RPL_LOGON);
    }
    else
    {
      SetWatch(sptr);
    }
  }

  return 0;
}

/*
 * m_nick
 *
 * parv[0] = sender prefix
 * parv[1] = nickname (:clave opcional)
 * parv[2] = clave (opcional)
 *
 * If from server, source is client:
 *   parv[2] = timestamp
 *
 * Source is server:
 *   parv[2] = hopcount
 *   parv[3] = timestamp
 *   parv[4] = username
 *   parv[5] = hostname
 *   parv[6] = umode (optional)
 *   parv[parc-3] = IP#                 <- Only Protocol >= 10
 *   parv[parc-2] = YXX, numeric nick   <- Only Protocol >= 10
 *   parv[parc-1] = info
 *   parv[0] = server
 */
int m_nick(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  if (MyConnect(sptr))
    return m_nick_local(cptr, sptr, parc, parv);
  else
    return m_nick_remoto(cptr, sptr, parc, parv);
}


static char *nuevo_nick_aleatorio(aClient *cptr)
{
  aClient *acptr;
  char *nick_nuevo;
  unsigned int v[2], k[2], x[2];
  char resultado[NICKLEN + 1];

  k[0] = k[1] = x[0] = x[1] = 0;

  v[0] = base64toint(cptr->yxx);
  v[1] = base64toint(me.yxx);

  acptr = cptr;

  do
  {
    tea(v, k, x);

    v[1] += 4096;

/*
 ** El 'if' que sigue lo necesitamos
 ** para que todos los valores tengan
 ** la misma probabilidad.
 */
    if (x[0] >= 4294000000ul)
      continue;

    sprintf_irc(resultado, "invitado-%.6d", (int)(x[0] % 1000000));

    nick_nuevo = resultado;

    acptr = FindClient(nick_nuevo);
  }
  while (acptr);

  return nick_nuevo;
}
