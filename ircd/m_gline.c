/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/m_gline.c
 *
 * Copyright (C) 2002-2017 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2000 Kevin L. Mitchell <klmitch@mit.edu>
 * Copyright (C) 1996-1997 Carlo Wood
 * Copyright (C) 1990 Jarkko Oikarinen
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
 * @brief Handlers for GLINE command.
 * @version $Id: m_gline.c,v 1.10 2007-12-11 23:38:25 zolty Exp $
 */
#include "config.h"

#include "client.h"
#include "gline.h"
#include "hash.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "match.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "s_conf.h"
#include "s_debug.h"
#include "s_misc.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdlib.h>
#include <string.h>

#define PASTWATCH	157680000	/* number of seconds in 5 years */

/*
 * If the expiration value, interpreted as an absolute timestamp, is
 * more recent than 5 years in the past, we interpret it as an
 * absolute timestamp; otherwise, we assume it's relative and convert
 * it to an absolute timestamp.  Either way, the output of this macro
 * is an absolute timestamp--not guaranteed to be a *valid* timestamp,
 * but you can't have everything in a macro ;)
 */
#define abs_expire(exp)							\
  ((exp) >= TStime() - PASTWATCH ? (exp) : (exp) + TStime())

/** Handle a GLINE message from a server.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the target server numnick or "*" for all servers
 * \li \a parv[2] is the G-line mask (preceded by modifier flags)
 * \li \a parv[3] is the G-line lifetime in seconds
 * \li \a parv[4] (optional) is the G-line's last modification time
 * \li \a parv[\a parc - 1] is the G-line comment
 *
 * If the issuer is a server or there is no timestamp, the issuer must
 * be flagged as a UWorld server.  In this case, if the '-' modifier
 * flag is used, the G-line lifetime and all following arguments may
 * be omitted.
 *
 * Three modifier flags are recognized, and must be present in this
 * order:
 * \li '!' Indicates an G-line that an oper forcibly applied.
 * \li '-' Indicates that the following G-line should be removed.
 * \li '+' (exclusive of '-') indicates that the G-line should be
 *   activated.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int
ms_gline(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Client *acptr = 0;
  struct Gline *agline = 0;
  unsigned int flags = 0;
  enum GlineAction action = GLINE_MODIFY;
  time_t expire = 0, lastmod = 0, lifetime = 0;
  char *mask = parv[2], *target = parv[1], *reason = "No reason", *tmp = 0;

  if (parc < 3)
    return need_more_params(sptr, "GLINE");

  if (IsServer(sptr))
    flags |= GLINE_FORCE;

  if (*mask == '!') {
    mask++;
    flags |= GLINE_OPERFORCE; /* assume oper had WIDE_GLINE */
  }

  switch (*mask) { /* handle +, -, <, and > */
  case '+': /* activate the G-line */
    action = GLINE_ACTIVATE;
    mask++;
    break;

  case '-': /* deactivate the G-line */
    action = GLINE_DEACTIVATE;
    mask++;
    break;

  case '>': /* locally activate the G-line */
    action = GLINE_LOCAL_ACTIVATE;
    mask++;
    break;

  case '<': /* locally deactivate the G-line */
    action = GLINE_LOCAL_DEACTIVATE;
    mask++;
    break;
  }

  /* Is there no mask left? */
  if (mask[0] == '\0')
    return need_more_params(sptr, "GLINE");

  /* Now, let's figure out if it's a local or global G-line */
  if (action == GLINE_LOCAL_ACTIVATE || action == GLINE_LOCAL_DEACTIVATE ||
      (target[0] == '*' && target[1] == '\0'))
    flags |= GLINE_GLOBAL;
  else
    flags |= GLINE_LOCAL;

  /* now figure out if we need to resolve a server */
  if ((action == GLINE_LOCAL_ACTIVATE || action == GLINE_LOCAL_DEACTIVATE ||
       (flags & GLINE_LOCAL)) && !(acptr = FindNServer(target)))
    return 0; /* no such server, jump out */

  /* If it's a local activate/deactivate and server isn't me, propagate it */
  if ((action == GLINE_LOCAL_ACTIVATE || action == GLINE_LOCAL_DEACTIVATE) &&
      !IsMe(acptr)) {
    Debug((DEBUG_DEBUG, "I am forwarding a local change to a global gline "
	   "to a remote server; target %s, mask %s, operforce %s, action %c",
	   target, mask, flags & GLINE_OPERFORCE ? "YES" : "NO",
	   action == GLINE_LOCAL_ACTIVATE ? '>' : '<'));

    sendcmdto_one(sptr, CMD_GLINE, acptr, "%C %s%c%s", acptr,
		  flags & GLINE_OPERFORCE ? "!" : "",
		  action == GLINE_LOCAL_ACTIVATE ? '>' : '<', mask);

    return 0; /* all done */
  }

  /* Next, try to find the G-line... */
  if ((flags & GLINE_GLOBAL) || IsMe(acptr)) /* don't bother if it's not me! */
    agline = gline_find(mask, flags | GLINE_ANY | GLINE_EXACT);

  /* We now have all the pieces to tell us what we've got; let's put
   * it all together and convert the rest of the arguments.
   */

  /* Handle the local G-lines first... */
  if (flags & GLINE_LOCAL) {
    assert(acptr);

    /* normalize the action, first */
    if (action == GLINE_LOCAL_ACTIVATE || action == GLINE_MODIFY)
      action = GLINE_ACTIVATE;
    else if (action == GLINE_LOCAL_DEACTIVATE)
      action = GLINE_DEACTIVATE;

    if (action == GLINE_ACTIVATE) { /* get expiration and reason */
      if (parc < 5) /* check parameter count... */
	return need_more_params(sptr, "GLINE");

      expire = atoi(parv[3]); /* get expiration... */
      expire = abs_expire(expire); /* convert to absolute... */
      reason = parv[parc - 1]; /* and reason */

      if (IsMe(acptr)) {
	if (agline) /* G-line already exists, so let's ignore it... */
	  return 0;

	/* OK, create the local G-line */
	Debug((DEBUG_DEBUG, "I am creating a local G-line here; target %s, "
	       "mask %s, operforce %s, action %s, expire %Tu, reason: %s",
	       target, mask, flags & GLINE_OPERFORCE ? "YES" : "NO",
	       action == GLINE_ACTIVATE ? "+" : "-", expire, reason));

	return gline_add(cptr, sptr, mask, reason, expire, lastmod,
			 lifetime, flags | GLINE_ACTIVE);
      }
    } else if (IsMe(acptr)) { /* destroying a local G-line */
      if (!agline) /* G-line doesn't exist, so let's complain... */
	return send_reply(sptr, ERR_NOSUCHGLINE, mask);

      /* Let's now destroy the G-line */;
      Debug((DEBUG_DEBUG, "I am destroying a local G-line here; target %s, "
	     "mask %s, operforce %s, action %s", target, mask,
	     flags & GLINE_OPERFORCE ? "YES" : "NO",
	     action == GLINE_ACTIVATE ? "+" : "-"));

      return gline_destroy(cptr, sptr, agline);
    }

    /* OK, we've converted arguments; if it's not for us, forward */
    /* UPDATE NOTE: Once all servers are updated to u2.10.12.11, the
     * format string in this sendcmdto_one() may be updated to omit
     * <lastmod> for GLINE_ACTIVATE and to omit <expire>, <lastmod>,
     * and <reason> for GLINE_DEACTIVATE.
     */
    assert(!IsMe(acptr));

    Debug((DEBUG_DEBUG, "I am forwarding a local G-line to a remote server; "
	   "target %s, mask %s, operforce %s, action %c, expire %Tu, "
	   "lastmod %Tu, reason: %s", target, mask,
	   flags & GLINE_OPERFORCE ? "YES" : "NO",
	   action == GLINE_ACTIVATE ? '+' :  '-', expire, TStime(),
	   reason));

    sendcmdto_one(sptr, CMD_GLINE, acptr, "%C %s%c%s %Tu %Tu :%s",
		  acptr, flags & GLINE_OPERFORCE ? "!" : "",
		  action == GLINE_ACTIVATE ? '+' : '-', mask,
		  expire - TStime(), TStime(), reason);

    return 0; /* all done */
  }

  /* can't modify a G-line that doesn't exist, so remap to activate */
  if (!agline && action == GLINE_MODIFY)
    action = GLINE_ACTIVATE;

  /* OK, let's figure out what other parameters we may have... */
  switch (action) {
  case GLINE_LOCAL_ACTIVATE: /* locally activating a G-line */
  case GLINE_LOCAL_DEACTIVATE: /* locally deactivating a G-line */
    if (!agline) /* no G-line to locally activate or deactivate? */
      return send_reply(sptr, ERR_NOSUCHGLINE, mask);
    lastmod = agline->gl_lastmod;
    break; /* no additional parameters to manipulate */

  case GLINE_ACTIVATE: /* activating a G-line */
  case GLINE_DEACTIVATE: /* deactivating a G-line */
    /* in either of these cases, we have at least a lastmod parameter */
    if (parc < 4)
      return need_more_params(sptr, "GLINE");
    else if (parc == 4) /* lastmod only form... */
      lastmod = atoi(parv[3]);
    /*FALLTHROUGH*/
  case GLINE_MODIFY: /* modifying a G-line */
    /* convert expire and lastmod, look for lifetime and reason */
    if (parc > 4) { /* protect against fall-through from 4-param form */
      expire = atoi(parv[3]); /* convert expiration and lastmod */
      expire = abs_expire(expire);
      lastmod = atoi(parv[4]);

      flags |= GLINE_EXPIRE; /* we have an expiration time update */

      if (parc > 6) { /* no question, have a lifetime and reason */
	lifetime = atoi(parv[5]);
	reason = parv[parc - 1];

	flags |= GLINE_LIFETIME | GLINE_REASON;
      } else if (parc == 6) { /* either a lifetime or a reason */
	if (!agline || /* gline creation, has to be the reason */
	    /* trial-convert as lifetime, and if it doesn't fully convert,
	     * it must be the reason */
	    (!(lifetime = strtoul(parv[5], &tmp, 10)) && !*tmp)) {
	  lifetime = 0;
	  reason = parv[5];

	  flags |= GLINE_REASON; /* have a reason update */
	} else if (lifetime)
	  flags |= GLINE_LIFETIME; /* have a lifetime update */
      }
    }
  }

  if (!lastmod) /* must have a lastmod parameter by now */
    return need_more_params(sptr, "GLINE");

  Debug((DEBUG_DEBUG, "I have a global G-line I am acting upon now; "
	 "target %s, mask %s, operforce %s, action %s, expire %Tu, "
	 "lastmod %Tu, lifetime %Tu, reason: %s; gline %s!  (fields "
	 "present: %s %s %s)", target, mask,
	 flags & GLINE_OPERFORCE ? "YES" : "NO",
	 action == GLINE_ACTIVATE ? "+" :
	 (action == GLINE_DEACTIVATE ? "-" :
	  (action == GLINE_LOCAL_ACTIVATE ? ">" :
	   (action == GLINE_LOCAL_DEACTIVATE ? "<" : "(MODIFY)"))),
	 expire, lastmod, lifetime, reason,
	 agline ? "EXISTS" : "does not exist",
	 flags & GLINE_EXPIRE ? "expire" : "",
	 flags & GLINE_LIFETIME ? "lifetime" : "",
	 flags & GLINE_REASON ? "reason" : ""));

  /* OK, at this point, we have converted all available parameters.
   * Let's actually do the action!
   */
  if (agline)
    return gline_modify(cptr, sptr, agline, action, reason, expire,
			lastmod, lifetime, flags);

  assert(action != GLINE_LOCAL_ACTIVATE);
  assert(action != GLINE_LOCAL_DEACTIVATE);
  assert(action != GLINE_MODIFY);

  if (!expire) { /* Cannot *add* a G-line we don't have, but try hard */
   Debug((DEBUG_DEBUG, "Propagating G-line %s for G-line we don't have",
	   action == GLINE_ACTIVATE ? "activation" : "deactivation"));

    /* propagate the G-line, even though we don't have it */
    sendcmdto_serv(sptr, CMD_GLINE, cptr, "* %c%s %Tu",
		   action == GLINE_ACTIVATE ? '+' : '-',
		   mask, lastmod);

    return 0;
  }

  return gline_add(cptr, sptr, mask, reason, expire, lastmod, lifetime,
		   flags | ((action == GLINE_ACTIVATE) ? GLINE_ACTIVE : 0));
}

/** Handle a GLINE message from an operator.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the G-line mask (preceded by modifier flags)
 * \li \a parv[2] (optional) is the target server numnick or '*'
 * \li \a parv[N+1] is the G-line lifetime in seconds
 * \li \a parv[\a parc - 1] is the G-line comment
 *
 * Three modifier flags are recognized, and must be present in this
 * order:
 * \li '!' Indicates an G-line that an oper forcibly applied.
 * \li '-' Indicates that the following G-line should be removed.
 * \li '+' (exclusive of '-') indicates that the G-line should be
 *   activated.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int
mo_gline(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Client *acptr = 0;
  struct Gline *agline = 0;
  unsigned int flags = 0;
  enum GlineAction action = GLINE_MODIFY;
  time_t expire = 0;
  char *mask = parv[1], *target = 0, *reason = 0, *end;

  if (parc < 2)
    return gline_list(sptr, 0);

  if (*mask == '!') {
    mask++;

    if (HasPriv(sptr, PRIV_WIDE_GLINE))
      flags |= GLINE_OPERFORCE;
  }

  switch (*mask) { /* handle +, -, <, and > */
  case '+': /* activate the G-line */
    action = GLINE_ACTIVATE;
    mask++;
    break;

  case '-': /* deactivate the G-line */
    action = GLINE_DEACTIVATE;
    mask++;
    break;

  case '>': /* locally activate the G-line */
    action = GLINE_LOCAL_ACTIVATE;
    mask++;
    break;

  case '<': /* locally deactivate the G-line */
    action = GLINE_LOCAL_DEACTIVATE;
    mask++;
    break;
  }

  /* OK, let's figure out the parameters... */
  switch (action) {
  case GLINE_MODIFY: /* no specific action on the G-line... */
    if (parc == 2) /* user wants a listing of a specific G-line */
      return gline_list(sptr, mask);
    else if (parc < 4) /* must have target and expire, minimum */
      return need_more_params(sptr, "GLINE");

    target = parv[2]; /* get the target... */
    expire = strtol(parv[3], &end, 10) + TStime(); /* and the expiration */
    if (*end != '\0')
      return send_reply(sptr, SND_EXPLICIT | ERR_BADEXPIRE, "%s :Bad expire time", parv[3]);

    flags |= GLINE_EXPIRE; /* remember that we got an expire time */

    if (parc > 4) { /* also got a reason... */
      reason = parv[parc - 1];
      flags |= GLINE_REASON;
    }

    /* target is not global, interpolate action and require reason */
    if (target[0] != '*' || target[1] != '\0') {
      if (!reason) /* have to have a reason for this */
	return need_more_params(sptr, "GLINE");

      action = GLINE_ACTIVATE;
    }
    break;

  case GLINE_LOCAL_ACTIVATE: /* locally activate a G-line */
  case GLINE_LOCAL_DEACTIVATE: /* locally deactivate a G-line */
    if (parc > 2) { /* if target is available, pick it */
      target = parv[2];
      if (target[0] == '*' && target[1] == '\0')
        return send_reply(sptr, ERR_NOSUCHSERVER, target);
    }
    break;

  case GLINE_ACTIVATE: /* activating/adding a G-line */
  case GLINE_DEACTIVATE: /* deactivating/removing a G-line */
    if (parc < 3)
      return need_more_params(sptr, "GLINE");

    if (parc > 3) {
      /* get expiration and target */
      reason = parv[parc - 1];
      expire = strtol(parv[parc - 2], &end, 10) + TStime();
      if (*end != '\0')
        return send_reply(sptr, SND_EXPLICIT | ERR_BADEXPIRE, "%s :Bad expire time", parv[parc - 2]);

      flags |= GLINE_EXPIRE | GLINE_REASON; /* remember that we got 'em */

      if (parc > 4) /* also have a target! */
	target = parv[2];
    } else {
      target = parv[2]; /* target has to be present, and has to be '*' */

      if (target[0] != '*' || target[1] != '\0')
	return need_more_params(sptr, "GLINE");
    }
    break;
  }

  /* Is there no mask left? */
  if (mask[0] == '\0')
    return need_more_params(sptr, "GLINE");

  /* Now let's figure out which is the target server */
  if (!target) /* no target, has to be me... */
    acptr = &me;
  /* if it's not '*', look up the server */
  else if ((target[0] != '*' || target[1] != '\0') &&
	   !(acptr = find_match_server(target)))
    return send_reply(sptr, ERR_NOSUCHSERVER, target);

  /* Now, is the G-line local or global? */
  if (action == GLINE_LOCAL_ACTIVATE || action == GLINE_LOCAL_DEACTIVATE ||
      !acptr)
    flags |= GLINE_GLOBAL;
  else /* it's some form of local G-line */
    flags |= GLINE_LOCAL;

  /* If it's a local activate/deactivate and server isn't me, propagate it */
  if ((action == GLINE_LOCAL_ACTIVATE || action == GLINE_LOCAL_DEACTIVATE) &&
      !IsMe(acptr)) {
    /* check for permissions... */
    if (!feature_bool(FEAT_CONFIG_OPERCMDS))
      return send_reply(sptr, ERR_DISABLED, "GLINE");
    else if (!HasPriv(sptr, PRIV_GLINE))
      return send_reply(sptr, ERR_NOPRIVILEGES);

    Debug((DEBUG_DEBUG, "I am forwarding a local change to a global gline "
	   "to a remote server; target %s, mask %s, operforce %s, action %c",
	   cli_name(acptr), mask, flags & GLINE_OPERFORCE ? "YES" : "NO",
	   action == GLINE_LOCAL_ACTIVATE ? '>' : '<'));

    sendcmdto_one(sptr, CMD_GLINE, acptr, "%C %s%c%s", acptr,
		  flags & GLINE_OPERFORCE ? "!" : "",
		  action == GLINE_LOCAL_ACTIVATE ? '>' : '<', mask);

    return 0; /* all done */
  }

  /* Next, try to find the G-line... */
  if ((flags & GLINE_GLOBAL) || IsMe(acptr)) /* don't bother if it's not me! */
    agline = gline_find(mask, flags | GLINE_ANY | GLINE_EXACT);

  /* We now have all the pieces to tell us what we've got; let's put
   * it all together and convert the rest of the arguments.
   */

  /* Handle the local G-lines first... */
  if (flags & GLINE_LOCAL) {
    assert(acptr);

    /* normalize the action, first */
    if (action == GLINE_LOCAL_ACTIVATE || action == GLINE_MODIFY)
      action = GLINE_ACTIVATE;
    else if (action == GLINE_LOCAL_DEACTIVATE)
      action = GLINE_DEACTIVATE;

    /* If it's not for us, forward */
    /* UPDATE NOTE: Once all servers are updated to u2.10.12.11, the
     * format string in this sendcmdto_one() may be updated to omit
     * <lastmod> for GLINE_ACTIVATE and to omit <expire>, <lastmod>,
     * and <reason> for GLINE_DEACTIVATE.
     */

    if (!IsMe(acptr)) {
      /* check for permissions... */
      if (!feature_bool(FEAT_CONFIG_OPERCMDS))
	return send_reply(sptr, ERR_DISABLED, "GLINE");
      else if (!HasPriv(sptr, PRIV_GLINE))
	return send_reply(sptr, ERR_NOPRIVILEGES);

      Debug((DEBUG_DEBUG, "I am forwarding a local G-line to a remote "
	     "server; target %s, mask %s, operforce %s, action %c, "
	     "expire %Tu, reason %s", target, mask,
	     flags & GLINE_OPERFORCE ? "YES" : "NO",
	     action == GLINE_ACTIVATE ? '+' : '-', expire, reason));

      sendcmdto_one(sptr, CMD_GLINE, acptr, "%C %s%c%s %Tu %Tu :%s",
		    acptr, flags & GLINE_OPERFORCE ? "!" : "",
		    action == GLINE_ACTIVATE ? '+' : '-', mask,
		    expire - TStime(), TStime(), reason);

      return 0; /* all done */
    }

    /* check local G-line permissions... */
    if (!HasPriv(sptr, PRIV_LOCAL_GLINE))
      return send_reply(sptr, ERR_NOPRIVILEGES);

    /* let's handle activation... */
    if (action == GLINE_ACTIVATE) {
      if (agline) /* G-line already exists, so let's ignore it... */
	return 0;

      /* OK, create the local G-line */
      Debug((DEBUG_DEBUG, "I am creating a local G-line here; target %s, "
	     "mask %s, operforce %s, action  %s, expire %Tu, reason: %s",
	     target, mask, flags & GLINE_OPERFORCE ? "YES" : "NO",
	     action == GLINE_ACTIVATE ? "+" : "-", expire, reason));

      return gline_add(cptr, sptr, mask, reason, expire, 0, 0,
		       flags | GLINE_ACTIVE);
    } else { /* OK, it's a deactivation/destruction */
      if (!agline) /* G-line doesn't exist, so let's complain... */
	return send_reply(sptr, ERR_NOSUCHGLINE, mask);

      /* Let's now destroy the G-line */
      Debug((DEBUG_DEBUG, "I am destroying a local G-line here; target %s, "
	     "mask %s, operforce %s, action %s", target, mask,
	     flags & GLINE_OPERFORCE ? "YES" : "NO",
	     action == GLINE_ACTIVATE ? "+" : "-"));

      return gline_destroy(cptr, sptr, agline);
    }
  }

  /* can't modify a G-line that doesn't exist...
   * (and if we are creating a new one, we need a reason and expiration)
   */
  if (!agline &&
      (action == GLINE_MODIFY || action == GLINE_LOCAL_ACTIVATE ||
       action == GLINE_LOCAL_DEACTIVATE || !reason || !expire))
    return send_reply(sptr, ERR_NOSUCHGLINE, mask);

  /* check for G-line permissions... */
  if (action == GLINE_LOCAL_ACTIVATE || action == GLINE_LOCAL_DEACTIVATE) {
    /* only need local privileges for locally-limited status changes */
    if (!HasPriv(sptr, PRIV_LOCAL_GLINE))
      return send_reply(sptr, ERR_NOPRIVILEGES);
  } else { /* global privileges required */
    if (!feature_bool(FEAT_CONFIG_OPERCMDS))
      return send_reply(sptr, ERR_DISABLED, "GLINE");
    else if (!HasPriv(sptr, PRIV_GLINE))
      return send_reply(sptr, ERR_NOPRIVILEGES);
  }

  Debug((DEBUG_DEBUG, "I have a global G-line I am acting upon now; "
	 "target %s, mask %s, operforce %s, action %s, expire %Tu, "
	 "reason: %s; gline %s!  (fields present: %s %s)", target, 
	 mask, flags & GLINE_OPERFORCE ? "YES" : "NO",
	 action == GLINE_ACTIVATE ? "+" :
	 (action == GLINE_DEACTIVATE ? "-" :
	  (action == GLINE_LOCAL_ACTIVATE ? ">" :
	   (action == GLINE_LOCAL_DEACTIVATE ? "<" : "(MODIFY)"))),
	 expire, reason, agline ? "EXISTS" : "does not exist",
	 flags & GLINE_EXPIRE ? "expire" : "",
	 flags & GLINE_REASON ? "reason" : ""));

  if (agline) /* modifying an existing G-line */
    return gline_modify(cptr, sptr, agline, action, reason, expire,
			TStime(), 0, flags);

  assert(action != GLINE_LOCAL_ACTIVATE);
  assert(action != GLINE_LOCAL_DEACTIVATE);
  assert(action != GLINE_MODIFY);

  /* create a new G-line */
  return gline_add(cptr, sptr, mask, reason, expire, TStime(), 0,
		   flags | ((action == GLINE_ACTIVATE) ? GLINE_ACTIVE : 0));
}

/** Handle a GLINE message from a normal client.
 *
 * \a parv has the following elements:
 * \li \a parv[1] is the target for which to show G-lines.
 *
 * See @ref m_functions for discussion of the arguments.
 * @param[in] cptr Client that sent us the message.
 * @param[in] sptr Original source of message.
 * @param[in] parc Number of arguments.
 * @param[in] parv Argument vector.
 */
int
m_gline(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  if (parc < 2)
    return send_reply(sptr, ERR_NOSUCHGLINE, "");

  return gline_list(sptr, parv[1]);
}
