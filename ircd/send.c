/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/send.c
 *
 * Copyright (C) 2002-2017 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Send messages to certain targets.
 * @version $Id$
 */
#include "config.h"

#include "send.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_snprintf.h"
#include "ircd_string.h"
#include "list.h"
#include "match.h"
#include "msg.h"
#include "numnicks.h"
#include "parse.h"
#include "s_bsd.h"
#include "s_debug.h"
#include "s_misc.h"
#include "s_user.h"
#include "struct.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */
#include <stdio.h>
#include <string.h>

/** Last used marker value. */
static int sentalong_marker;
/** Array of users with the corresponding server notice mask bit set. */
struct SLink *opsarray[32];     /* don't use highest bit unless you change
                   atoi to strtoul in sendto_op_mask() */
/** Linked list of all connections with data queued to send. */
static struct Connection *send_queues;

static void vsendto_opmask(struct Client *one, unsigned int mask,
               const char *pattern, va_list vl);

/*
 * dead_link
 *
 * An error has been detected. The link *must* be closed,
 * but *cannot* call ExitClient (m_bye) from here.
 * Instead, mark it with FLAG_DEADSOCKET. This should
 * generate ExitClient from the main loop.
 *
 * If 'notice' is not NULL, it is assumed to be a format
 * for a message to local opers. It can contain only one
 * '%s', which will be replaced by the sockhost field of
 * the failing link.
 *
 * Also, the notice is skipped for "uninteresting" cases,
 * like Persons and yet unknown connections...
 */
/** Mark a client as dead, even if they are not the current message source.
 * This is done by setting the DEADSOCKET flag on the user and letting the
 * main loop perform the actual exit logic.
 * @param[in,out] to Client being killed.
 * @param[in] notice Message for local opers.
 */
static void dead_link(struct Client *to, char *notice)
{
  SetFlag(to, FLAG_DEADSOCKET);
  /*
   * If because of BUFFERPOOL problem then clean dbuf's now so that
   * notices don't hurt operators below.
   */
  DBufClear(&(cli_recvQ(to)));
  MsgQClear(&(cli_sendQ(to)));
  client_drop_sendq(cli_connect(to));

  /*
   * Keep a copy of the last comment, for later use...
   */
  ircd_strncpy(cli_info(to), notice, REALLEN);

  if (!IsUser(to) && !IsUnknown(to) && !HasFlag(to, FLAG_CLOSING))
    sendto_opmask(0, SNO_OLDSNO, "%s for %s", cli_info(to), cli_name(to));
  Debug((DEBUG_ERROR, cli_info(to)));
}

/** Test whether we can send to a client.
 * @param[in] to Client we want to send to.
 * @return Non-zero if we can send to the client.
 */
static int can_send(struct Client* to)
{
  assert(0 != to);
  return (IsDead(to) || IsMe(to) || -1 == cli_fd(to)) ? 0 : 1;
}

/** Close the connection with the highest sendq.
 * This should be called when we need to free buffer memory.
 * @param[in] servers_too If non-zero, consider killing servers, too.
 */
void
kill_highest_sendq(int servers_too)
{
  int i;
  unsigned int highest_sendq = 0;
  struct Client *highest_client = 0;

  for (i = HighestFd; i >= 0; i--)
  {
    if (!LocalClientArray[i] || (!servers_too && cli_serv(LocalClientArray[i])))
      continue; /* skip servers */

    /* If this sendq is higher than one we last saw, remember it */
    if (MsgQLength(&(cli_sendQ(LocalClientArray[i]))) > highest_sendq)
    {
      highest_client = LocalClientArray[i];
      highest_sendq = MsgQLength(&(cli_sendQ(highest_client)));
    }
  }

  if (highest_client)
    dead_link(highest_client, "Buffer allocation error");
}

/*
 * flush_connections
 *
 * Used to empty all output buffers for all connections. Should only
 * be called once per scan of connections. There should be a select in
 * here perhaps but that means either forcing a timeout or doing a poll.
 * When flushing, all we do is empty the obuffer array for each local
 * client and try to send it. if we cant send it, it goes into the sendQ
 * -avalon
 */
/** Flush data queued for one or all connections.
 * @param[in] cptr Client to flush (if NULL, do all).
 */
void flush_connections(struct Client* cptr)
{
  if (cptr) {
    send_queued(cptr);
  }
  else {
    struct Connection* con;
    for (con = send_queues; con; con = con_next(con)) {
      assert(0 < MsgQLength(&(con_sendQ(con))));
      send_queued(con_client(con));
    }
  }
}

/*
 * send_queued
 *
 * This function is called from the main select-loop (or whatever)
 * when there is a chance that some output would be possible. This
 * attempts to empty the send queue as far as possible...
 */
/** Attempt to send data queued for a client.
 * @param[in] to Client to send data to.
 */
void send_queued(struct Client *to)
{
  assert(0 != to);
  assert(0 != cli_local(to));

  if (IsBlocked(to) || !can_send(to))
    return;                     /* Don't bother */

  while (MsgQLength(&(cli_sendQ(to))) > 0) {
    unsigned int len;

    if ((len = deliver_it(to, &(cli_sendQ(to))))) {
      msgq_delete(&(cli_sendQ(to)), len);
      cli_lastsq(to) = MsgQLength(&(cli_sendQ(to))) / 1024;
      if (IsBlocked(to)) {
    update_write(to);
        return;
      }
    }
    else {
      if (IsDead(to)) {
        char tmp[512];
        sprintf(tmp,"Write error: %s",(strerror(cli_error(to))) ? (strerror(cli_error(to))) : "Unknow error" );
        dead_link(to, tmp);
      }
      return;
    }
  }

  /* Ok, sendq is now empty... */
  client_drop_sendq(cli_connect(to));
  update_write(to);
}

/** Try to send a buffer to a client, queueing it if needed.
 * @param[in,out] to Client to send message to.
 * @param[in] buf Message to send.
 * @param[in] prio If non-zero, send as high priority.
 */
void send_buffer(struct Client* to, struct MsgBuf* buf, int prio)
{
  assert(0 != to);
  assert(0 != buf);

  if (cli_from(to))
    to = cli_from(to);

  if (!can_send(to))
    /*
     * This socket has already been marked as dead
     */
    return;

  if (MsgQLength(&(cli_sendQ(to))) > get_sendq(to)) {
    if (IsServer(to))
      sendto_opmask(0, SNO_OLDSNO, "Max SendQ limit exceeded for %C: %zu > %zu",
                    to, MsgQLength(&(cli_sendQ(to))), get_sendq(to));
    dead_link(to, "Max sendQ exceeded");
    return;
  }

  Debug((DEBUG_SEND, "Sending [%p] to %s", buf, cli_name(to)));

#if defined(USE_SSL)
  if (cli_socket(to).s_ssl)
      prio = 0;
#endif

  msgq_add(&(cli_sendQ(to)), buf, prio);
  client_add_sendq(cli_connect(to), &send_queues);
  update_write(to);

  /*
   * Update statistics. The following is slightly incorrect
   * because it counts messages even if queued, but bytes
   * only really sent. Queued bytes get updated in SendQueued.
   */
  ++(cli_sendM(to));
  ++(cli_sendM(&me));
  /*
   * This little bit is to stop the sendQ from growing too large when
   * there is no need for it to. Thus we call send_queued() every time
   * 2k has been added to the queue since the last non-fatal write.
   * Also stops us from deliberately building a large sendQ and then
   * trying to flood that link with data (possible during the net
   * relinking done by servers with a large load).
   */
  if (MsgQLength(&(cli_sendQ(to))) / 1024 > cli_lastsq(to))
    send_queued(to);
}

/*
 * Send a msg to all ppl on servers/hosts that match a specified mask
 * (used for enhanced PRIVMSGs)
 *
 *  addition -- Armin, 8jun90 (gruner@informatik.tu-muenchen.de)
 */

/** Check whether a client matches a target mask.
 * @param[in] from Client trying to send a message (ignored).
 * @param[in] one Client being considered as a target.
 * @param[in] mask Mask for matching against.
 * @param[in] addr IP address prefix to match against.
 * @param[in] nbits Number of bits in \a addr (> 128 if none valid).
 * @param[in] what Type of match (either MATCH_HOST or MATCH_SERVER).
 * @return Non-zero if \a one matches, zero if not.
 */
static int match_it(struct Client *from, struct Client *one, const char *mask,
                    struct irc_in_addr *addr, unsigned char nbits, int what)
{
  switch (what)
  {
    case MATCH_HOST:
      return ((nbits <= 128 && ipmask_check(&cli_ip(one), addr, nbits)) ||
        match(mask, cli_user(one)->host) == 0 ||
        (HasHiddenHost(one) && match(mask, cli_user(one)->realhost) == 0));
    case MATCH_SERVER:
    default:
      return (match(mask, cli_name(cli_user(one)->server)) == 0);
  }
}

/** Send an unprefixed line to a client.
 * @param[in] to Client receiving message.
 * @param[in] pattern Format string of message.
 */
void sendrawto_one(struct Client *to, const char *pattern, ...)
{
  struct MsgBuf *mb;
  va_list vl;

  va_start(vl, pattern);
  mb = msgq_vmake(to, pattern, vl);
  va_end(vl);

  send_buffer(to, mb, 0);

  msgq_clean(mb);
}

#if defined(DDB)
/** Send a bot command to a single client.
 * @param[in] botname Bot sending the command.
 * @param[in] cmd Long name of command (used if \a to is a user).
 * @param[in] tok Short name of command (used if \a to is a server).
 * @param[in] to Destination of command.
 * @param[in] pattern Format string for command arguments.
 */
void sendcmdbotto_one(const char *botname, const char *cmd, const char *tok,
                      struct Client *to, const char *pattern, ...)
{
  struct VarData vd;
  struct MsgBuf *mb;

  to = cli_from(to);

  vd.vd_format = pattern; /* set up the struct VarData for %v */
  va_start(vd.vd_args, pattern);

  mb = msgq_make(to, ":%s %s %v", botname, cmd, &vd);

  va_end(vd.vd_args);

  send_buffer(to, mb, 0);

  msgq_clean(mb);

}
#endif /* defined(DDB) */

/** Send a (prefixed) command to a single client.
 * @param[in] from Client sending the command.
 * @param[in] cmd Long name of command (used if \a to is a user).
 * @param[in] tok Short name of command (used if \a to is a server).
 * @param[in] to Destination of command.
 * @param[in] pattern Format string for command arguments.
 */
void sendcmdto_one(struct Client *from, const char *cmd, const char *tok,
           struct Client *to, const char *pattern, ...)
{
  struct VarData vd;
  struct MsgBuf *mb;

  to = cli_from(to);

  vd.vd_format = pattern; /* set up the struct VarData for %v */
  va_start(vd.vd_args, pattern);

  mb = msgq_make(to, "%:#C %s %v", from, IsServer(to) || IsMe(to) ? tok : cmd,
         &vd);

  va_end(vd.vd_args);

  send_buffer(to, mb, 0);

  msgq_clean(mb);
}

/**
 * Send a (prefixed) command to a single client in the priority queue.
 * @param[in] from Client sending the command.
 * @param[in] cmd Long name of command (used if \a to is a user).
 * @param[in] tok Short name of command (used if \a to is a server).
 * @param[in] to Destination of command.
 * @param[in] pattern Format string for command arguments.
 */
void sendcmdto_prio_one(struct Client *from, const char *cmd, const char *tok,
            struct Client *to, const char *pattern, ...)
{
  struct VarData vd;
  struct MsgBuf *mb;

  to = cli_from(to);

  vd.vd_format = pattern; /* set up the struct VarData for %v */
  va_start(vd.vd_args, pattern);

  mb = msgq_make(to, "%:#C %s %v", from, IsServer(to) || IsMe(to) ? tok : cmd,
         &vd);

  va_end(vd.vd_args);

  send_buffer(to, mb, 1);

  msgq_clean(mb);
}

/**
 * Send a (prefixed) command to all servers matching or not matching a
 * flag but one.
 * @param[in] from Client sending the command.
 * @param[in] cmd Long name of command (ignored).
 * @param[in] tok Short name of command.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] require Only send to servers with this Flag bit set.
 * @param[in] forbid Do not send to servers with this Flag bit set.
 * @param[in] pattern Format string for command arguments.
 */
void sendcmdto_flag_serv(struct Client *from, const char *cmd,
                         const char *tok, struct Client *one,
                         int require, int forbid,
                         const char *pattern, ...)
{
  struct VarData vd;
  struct MsgBuf *mb;
  struct DLink *lp;

  vd.vd_format = pattern; /* set up the struct VarData for %v */
  va_start(vd.vd_args, pattern);

  /* use token */
  mb = msgq_make(&me, "%C %s %v", from, tok, &vd);
  va_end(vd.vd_args);

  /* canonicalize 'one' pointer */
  if (one)
    one = cli_from(one);

  /* send it to our downlinks */
  for (lp = cli_serv(&me)->down; lp; lp = lp->next) {
    if (lp->value.cptr == one)
      continue;
    if ((require < FLAG_LAST_FLAG) && !HasFlag(lp->value.cptr, require))
      continue;
    if ((forbid < FLAG_LAST_FLAG) && HasFlag(lp->value.cptr, forbid))
      continue;
    send_buffer(lp->value.cptr, mb, 0);
  }

  msgq_clean(mb);
}

/**
 * Send a (prefixed) command to all servers but one.
 * @param[in] from Client sending the command.
 * @param[in] cmd Long name of command (ignored).
 * @param[in] tok Short name of command.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] pattern Format string for command arguments.
 */
void sendcmdto_serv(struct Client *from, const char *cmd,
                    const char *tok, struct Client *one,
                    const char *pattern, ...)
{
  struct VarData vd;
  struct MsgBuf *mb;
  struct DLink *lp;

  vd.vd_format = pattern; /* set up the struct VarData for %v */
  va_start(vd.vd_args, pattern);

  /* use token */
  mb = msgq_make(&me, "%C %s %v", from, tok, &vd);
  va_end(vd.vd_args);

  /* canonicalize 'one' pointer */
  if (one)
    one = cli_from(one);

  /* send it to our downlinks */
  for (lp = cli_serv(&me)->down; lp; lp = lp->next) {
    if (lp->value.cptr == one)
      continue;
    send_buffer(lp->value.cptr, mb, 0);
  }

  msgq_clean(mb);
}

/** Safely increment the sentalong marker.
 * This increments the sentalong marker.  Since new connections will
 * have con_sentalong() == 0, and to avoid confusion when the counter
 * wraps, we reset all sentalong markers to zero when the sentalong
 * marker hits zero.
 * @param[in,out] one Client to mark with new sentalong marker (if any).
 */
static void
bump_sentalong(struct Client *one)
{
  if (!++sentalong_marker)
  {
    int ii;
    for (ii = 0; ii < HighestFd; ++ii)
      if (LocalClientArray[ii])
        cli_sentalong(LocalClientArray[ii]) = 0;
    ++sentalong_marker;
  }
  if (one)
    cli_sentalong(one) = sentalong_marker;
}

/** Send a (prefixed) command to all channels that \a from is on.
 * @param[in] from Client originating the command.
 * @param[in] cmd Long name of command.
 * @param[in] tok Short name of command.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] pattern Format string for command arguments.
 */
void sendcmdto_common_channels(struct Client *from, const char *cmd,
                               const char *tok, struct Client *one,
                               const char *pattern, ...)
{
  struct VarData vd;
  struct MsgBuf *mb;
  struct Membership *chan;
  struct Membership *member;

  assert(0 != from);
  assert(0 != cli_from(from));
  assert(0 != pattern);
  assert(!IsServer(from) && !IsMe(from));

  vd.vd_format = pattern; /* set up the struct VarData for %v */

  va_start(vd.vd_args, pattern);

  /* build the buffer */
  mb = msgq_make(0, "%:#C %s %v", from, cmd, &vd);
  va_end(vd.vd_args);

  bump_sentalong(from);
  /*
   * loop through from's channels, and the members on their channels
   */
  for (chan = cli_user(from)->channel; chan; chan = chan->next_channel) {
    if (IsZombie(chan) || IsDelayedJoin(chan))
      continue;
    for (member = chan->channel->members; member;
     member = member->next_member)
      if (MyConnect(member->user)
          && -1 < cli_fd(cli_from(member->user))
          && member->user != one
          && cli_sentalong(member->user) != sentalong_marker) {
    cli_sentalong(member->user) = sentalong_marker;
    send_buffer(member->user, mb, 0);
      }
  }

  if (MyConnect(from) && from != one)
    send_buffer(from, mb, 0);

  msgq_clean(mb);
}

#if defined(DDB)
/** Send a bot command to all local users on a channel.
 * @param[in] botname Bot sending the command.
 * @param[in] cmd Long name of command.
 * @param[in] tok Short name of command (ignored).
 * @param[in] to Destination channel.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] skip Bitmask of SKIP_DEAF, SKIP_NONOPS, SKIP_NONVOICES indicating which clients to skip.
 * @param[in] pattern Format string for command arguments.
 */
void sendcmdbotto_channel(const char *botname, const char *cmd,
                          const char *tok, struct Channel *to,
                          struct Client *one, unsigned int skip,
                          const char *pattern, ...)
{
  struct VarData vd;
  struct MsgBuf *mb;
  struct Membership *member;

  vd.vd_format = pattern; /* set up the struct VarData for %v */
  va_start(vd.vd_args, pattern);

  /* build the buffer */
  mb = msgq_make(0, ":%s %s %v", botname, cmd, &vd);
  va_end(vd.vd_args);

  /* send the buffer to each local channel member */
  for (member = to->members; member; member = member->next_member) {
    if (!MyConnect(member->user)
        || member->user == one
        || IsZombie(member)
        || (skip & SKIP_DEAF && IsDeaf(member->user))
#if defined(DDB) || defined(SERVICES)
        || (skip & SKIP_NONOPS && !IsChanOwner(member) && !IsChanOp(member))
        || (skip & SKIP_NONVOICES && !IsChanOwner(member) && !IsChanOp(member) && !HasVoice(member)))
#else
        || (skip & SKIP_NONOPS && !IsChanOp(member))
        || (skip & SKIP_NONVOICES && !IsChanOp(member) && !HasVoice(member)))
#endif
        continue;
      send_buffer(member->user, mb, 0);
  }

  msgq_clean(mb);
}
#endif /* defined(DDB) */

/** Send a (prefixed) command to all users on this channel, except for
 * \a one and those matching \a skip.
 * @warning \a pattern must not contain %v.
 * @param[in] from Client originating the command.
 * @param[in] cmd Long name of command.
 * @param[in] tok Short name of command.
 * @param[in] to Destination channel.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] skip Bitmask of SKIP_NONOPS, SKIP_NONVOICES, SKIP_DEAF, SKIP_BURST, SKIP_SERVERS.
 * @param[in] pattern Format string for command arguments.
 */
void sendcmdto_channel(struct Client *from, const char *cmd,
                       const char *tok, struct Channel *to,
                       struct Client *one, unsigned int skip,
                       const char *pattern, ...)
{
  struct Membership *member;
  struct VarData vd;
  struct MsgBuf *user_mb;
  struct MsgBuf *serv_mb;

  vd.vd_format = pattern;

  /* Build buffer to send to users */
  va_start(vd.vd_args, pattern);
  user_mb = msgq_make(0, skip & (SKIP_NONOPS | SKIP_NONVOICES) ? "%:#C %s @%v" : "%:#C %s %v",
                      from, cmd, &vd);
  va_end(vd.vd_args);

  /* Build buffer to send to servers */
  if ((skip & SKIP_SERVERS) || IsLocalChannel(to->chname))
    serv_mb = NULL;
  else
  {
    va_start(vd.vd_args, pattern);
    serv_mb = msgq_make(&me, skip & SKIP_NONOPS ? "%C %s @%v" : "%C %s %v",
                        from, tok, &vd);
    va_end(vd.vd_args);
  }

  /* send buffer along! */
  bump_sentalong(one);
  for (member = to->members; member; member = member->next_member) {
    /* skip duplicates, zombies, and flagged users... */
    if (cli_sentalong(member->user) == sentalong_marker ||
        IsZombie(member) ||
        (skip & SKIP_DEAF && IsDeaf(member->user)) ||
#if defined(DDB) || defined(SERVICES)
        (skip & SKIP_NONOPS && !IsChanOwner(member) && !IsChanOp(member)) ||
        (skip & SKIP_NONVOICES && !IsChanOwner(member) && !IsChanOp(member) && !HasVoice(member)) ||
#else
        (skip & SKIP_NONOPS && !IsChanOp(member)) ||
        (skip & SKIP_NONVOICES && !IsChanOp(member) && !HasVoice(member)) ||
#endif
        (skip & SKIP_COLOUR && !IsStripColour(member->user)) ||
        (skip & SKIP_NOCOLOUR && IsStripColour(member->user)) ||
        (skip & SKIP_BURST && IsBurstOrBurstAck(cli_from(member->user))) ||
        !(serv_mb || MyUser(member->user)) ||
        cli_fd(cli_from(member->user)) < 0)
      continue;
    cli_sentalong(member->user) = sentalong_marker;

    /* pick right buffer to send */
    send_buffer(member->user, MyConnect(member->user) ? user_mb : serv_mb, 0);
  }

  msgq_clean(user_mb);
  if (serv_mb)
    msgq_clean(serv_mb);
}

/** Send a (prefixed) WALL of type \a type to all users except \a one.
 * @warning \a pattern must not contain %v.
 * @param[in] from Source of the command.
 * @param[in] type One of WALL_DESYNCH, WALL_WALLOPS or WALL_WALLUSERS.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] pattern Format string for command arguments.
 */
void sendwallto_group(struct Client *from, int type, struct Client *one,
                      const char *pattern, ...)
{
  struct VarData vd;
  struct Client *cptr;
  struct MsgBuf *mb;
  struct DLink *lp;
  char *prefix=NULL;
  char *tok=NULL;
  int his_wallops;
  int i;

  vd.vd_format = pattern;

  /* Build buffer to send to users */
  va_start(vd.vd_args, pattern);
  switch (type) {
        case WALL_DESYNCH:
        prefix="";
        tok=TOK_DESYNCH;
        break;
        case WALL_WALLOPS:
        prefix="* ";
        tok=TOK_WALLOPS;
        break;
        case WALL_WALLUSERS:
        prefix="$ ";
        tok=TOK_WALLUSERS;
        break;
    default:
        assert(0);
  }
  mb = msgq_make(0, "%:#C " MSG_WALLOPS " :%s%v", from, prefix,&vd);
  va_end(vd.vd_args);

  /* send buffer along! */
  his_wallops = feature_bool(FEAT_HIS_WALLOPS);
  for (i = 0; i <= HighestFd; i++)
  {
    if (!(cptr = LocalClientArray[i]) ||
    (cli_fd(cli_from(cptr)) < 0) ||
    (type == WALL_DESYNCH && !SendDebug(cptr)) ||
    (type == WALL_WALLOPS &&
         (!SendWallops(cptr) || (his_wallops && !IsAnOper(cptr)))) ||
        (type == WALL_WALLUSERS && !SendWallops(cptr)))
      continue; /* skip it */
    send_buffer(cptr, mb, 1);
  }

  msgq_clean(mb);

  /* Build buffer to send to servers */
  va_start(vd.vd_args, pattern);
  mb = msgq_make(&me, "%C %s :%v", from, tok, &vd);
  va_end(vd.vd_args);

  /* send buffer along! */
  for (lp = cli_serv(&me)->down; lp; lp = lp->next) {
    if (one && lp->value.cptr == cli_from(one))
      continue;
    send_buffer(lp->value.cptr, mb, 1);
  }

  msgq_clean(mb);
}

/** Send a (prefixed) command to all users matching \a to as \a who.
 * @warning \a pattern must not contain %v.
 * @param[in] from Source of the command.
 * @param[in] cmd Long name of command.
 * @param[in] tok Short name of command.
 * @param[in] to Destination host/server mask.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] who Type of match for \a to (either MATCH_HOST or MATCH_SERVER).
 * @param[in] pattern Format string for command arguments.
 */
void sendcmdto_match(struct Client *from, const char *cmd,
                     const char *tok, const char *to,
                     struct Client *one, unsigned int who,
                     const char *pattern, ...)
{
  struct VarData vd;
  struct irc_in_addr addr;
  struct Client *cptr;
  struct MsgBuf *user_mb;
  struct MsgBuf *serv_mb;
  unsigned char nbits;

  vd.vd_format = pattern;

  /* See if destination looks like an IP mask. */
  if (!ipmask_parse(to, &addr, &nbits))
    nbits = 255;

  /* Build buffer to send to users */
  va_start(vd.vd_args, pattern);
/*
TODO-ZOLTAN: Revisar el tema de Globales
  if (IsUser(from) && IsService(cli_user(from)->server))
*/
  user_mb = msgq_make(0, "%:#C %s %v", from, cmd, &vd);
/*
  else
  {
    char *mask, *msg;
    mask = (char *)va_arg(vd.vd_args, char *);
    msg = (char *)va_arg(vd.vd_args, char *);

    user_mb = msgq_make(0, "%:#C %s :*** Global Message -> (%s): %s",
                        from, cmd, mask, msg);
  }
*/
  va_end(vd.vd_args);

  /* Build buffer to send to servers */
  va_start(vd.vd_args, pattern);
  serv_mb = msgq_make(&me, "%C %s %v", from, tok, &vd);
  va_end(vd.vd_args);

  /* send buffer along */
  bump_sentalong(one);
  for (cptr = GlobalClientList; cptr; cptr = cli_next(cptr)) {
    if (cli_sentalong(cptr) == sentalong_marker ||
        !IsRegistered(cptr) ||
        IsServer(cptr) ||
    !match_it(from, cptr, to, &addr, nbits, who) ||
        cli_fd(cli_from(cptr)) < 0)
      continue; /* skip it */
    cli_sentalong(cptr) = sentalong_marker;

    if (MyConnect(cptr)) /* send right buffer */
      send_buffer(cptr, user_mb, 0);
    else
      send_buffer(cptr, serv_mb, 0);
  }

  msgq_clean(user_mb);
  msgq_clean(serv_mb);
}

/** Send a server notice to all users subscribing to the indicated \a
 * mask except for \a one.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] mask One of the SNO_* constants.
 * @param[in] pattern Format string for server notice.
 */
void sendto_opmask(struct Client *one, unsigned int mask,
                   const char *pattern, ...)
{
  va_list vl;

  va_start(vl, pattern);
  vsendto_opmask(one, mask, pattern, vl);
  va_end(vl);
}

/** Send a server notice to all users subscribing to the indicated \a
 * mask except for \a one, rate-limited to once per 30 seconds.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] mask One of the SNO_* constants.
 * @param[in,out] rate Pointer to the last time the message was sent.
 * @param[in] pattern Format string for server notice.
 */
void sendto_opmask_ratelimited(struct Client *one, unsigned int mask,
                               time_t *rate, const char *pattern, ...)
{
  va_list vl;

  if ((CurrentTime - *rate) < 30)
    return;
  else
    *rate = CurrentTime;

  va_start(vl, pattern);
  vsendto_opmask(one, mask, pattern, vl);
  va_end(vl);
}

/** Send a server notice to all users subscribing to the indicated \a
 * mask except for \a one.
 * @param[in] one Client direction to skip (or NULL).
 * @param[in] mask One of the SNO_* constants.
 * @param[in] pattern Format string for server notice.
 * @param[in] vl Argument list for format string.
 */
static void vsendto_opmask(struct Client *one, unsigned int mask,
               const char *pattern, va_list vl)
{
  struct VarData vd;
  struct MsgBuf *mb;
  int i = 0; /* so that 1 points to opsarray[0] */
  struct SLink *opslist;

  while ((mask >>= 1))
    i++;

  if (!(opslist = opsarray[i]))
    return;

  /*
   * build string; I don't want to bother with client nicknames, so I hope
   * this is ok...
   */
  vd.vd_format = pattern;
  va_copy(vd.vd_args, vl);
  mb = msgq_make(0, ":%s " MSG_NOTICE " * :*** Notice -- %v", cli_name(&me),
         &vd);

  for (; opslist; opslist = opslist->next)
    if (opslist->value.cptr != one)
      send_buffer(opslist->value.cptr, mb, 0);

  msgq_clean(mb);
}

/** Send a server notice to all local users on this server.
 * @param[in] pattern Format string for server notice.
 */
void sendto_lusers(const char *pattern, ...)
{
  struct VarData vd;
  struct Client *cptr;
  struct MsgBuf *mb;
  int i;

  /* Build the message we're going to send... */
  vd.vd_format = pattern;
  va_start(vd.vd_args, pattern);
  mb = msgq_make(0, ":%s " MSG_NOTICE " * :*** Notice -- %v", cli_name(&me),
         &vd);
  va_end(vd.vd_args);

  /* send it along */
  for (i = 0; i <= HighestFd; i++) {
    if (!(cptr = LocalClientArray[i]) || !IsUser(cptr))
      continue; /* skip empty slots... */

    send_buffer(cptr, mb, 1); /* send with high priority */
  }

  msgq_clean(mb); /* clean up after ourselves */
}

#if 0
/*
 * Envio un comando a otro servidor o usuario
 */
void sendcmdto_one(struct Client *to, struct Client *from, char *cmd, char *token, const char *pattern, ...)
{
  va_list vl;
  
  /* Evito que el mensaje vuelva al origen */
  if(cli_from(from)==cli_from(to))
    return;
  
#if !defined(NO_PROTOCOL9)
  if (Protocol(cli_from(to)) > 9)
  {
#endif
    if (IsUser(from)) {
      if(IsUser(to))
        sprintf_irc(sendbuf, "%s%s %s %s%s ", NumNick(from), token, NumNick(to));
      else
        sprintf_irc(sendbuf, "%s%s %s %s ", NumNick(from), token, NumServ(to));
    } else {
      if(IsUser(to))
        sprintf_irc(sendbuf, "%s %s %s%s ", NumServ(from), token, NumNick(to));
      else
        sprintf_irc(sendbuf, "%s %s %s ", NumServ(from), token, NumServ(to));
    }
#if !defined(NO_PROTOCOL9)
  } else 
    sprintf_irc(sendbuf, ":%s %s %s ", from->name, cmd, to->name);
#endif

  if(!pattern || !*pattern)
    sendbuf[strlen(sendbuf)-1]='\0';
  else
  {
    va_start(vl, pattern);
    vsprintf_irc(sendbuf + strlen(sendbuf), pattern, vl);
    va_end(vl);
  }
  
  sendbufto_one(to);
}

void vsendto_one(struct Client *to, char *pattern, va_list vl)
{
  va_list vlcopy;
  va_copy(vlcopy,vl);
  vsprintf_irc(sendbuf, pattern, vlcopy);
  sendbufto_one(to);
}

static void vsendto_prefix_one(struct Client *to, struct Client *from,
    char *pattern, va_list vlorig)
{
  va_list vl;
  va_copy(vl,vlorig);
  if (to && from && MyUser(to) && IsUser(from))
  {
    static char sender[HOSTLEN + NICKLEN + USERLEN + 5];
    char *par;
    int flag = 0;
    Reg3 anUser *user = from->cli_user;

    par = va_arg(vl, char *);
    strcpy(sender, from->name);

#if defined(ESNET_NEG)
    if (user && !(to->cli_connect->negociacion & USER_TOK))
#else
    if (user)
#endif
    {
      if (user->username)
      {
        strcat(sender, "!");
        strcat(sender, user->username);
      }
      if (user->host && !MyConnect(from))
      {
        strcat(sender, "@");
#if defined(BDD_VIP)
        strcat(sender, get_visiblehost(from, NULL));
#else
        strcat(sender, user->host);
#endif
        flag = 1;
      }
    }
    /*
     * Flag is used instead of strchr(sender, '@') for speed and
     * also since username/nick may have had a '@' in them. -avalon
     */
#if defined(ESNET_NEG)
    if (!flag && MyConnect(from) && user->host && !(to->cli_connect->negociacion & USER_TOK))
#else
    if (!flag && MyConnect(from) && user->host)
#endif
    {
      strcat(sender, "@");
#if defined(BDD_VIP)
      strcat(sender, get_visiblehost(from, NULL));
#else
      if (IsUnixSocket(from))
        strcat(sender, user->host);
      else
        strcat(sender, from->sockhost);
#endif
    }
    *sendbuf = ':';
    strcpy(&sendbuf[1], sender);
    /* Assuming 'pattern' always starts with ":%s ..." */
    vsprintf_irc(sendbuf + strlen(sendbuf), &pattern[3], vl);
  }
  else
    vsprintf_irc(sendbuf, pattern, vl);
  sendbufto_one(to);
}

static void vsendto_prefix_one2(struct Client *to, struct Client *from,
    char *pattern, ...)
{
  va_list vl;

  va_start(vl, pattern);
  vsendto_prefix_one(to, from, pattern, vl);
  va_end(vl);
}


/*
 * sendto_match_butone
 *
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 *
 * jcea@argo.es - 1999/12/16:
 *
 * OJO: PATRON A "PIN~ON FIJO"
 */
void sendto_match_butone(struct Client *one, struct Client *from, char *mask, int what, ...)
{
  va_list vl;
  Reg1 int i;
  Reg2 struct Client *cptr, *acptr;

  char *fuente;
  char *comando;
  char *destino;
  char *fuente2;
  char *mask2;
  char *mensaje;

  va_start(vl, what);

  fuente = va_arg(vl, char *);
  comando = va_arg(vl, char *);
  destino = va_arg(vl, char *);
  fuente2 = va_arg(vl, char *);
  mask2 = va_arg(vl, char *);
  mensaje = va_arg(vl, char *);

  for (i = 0; i <= highest_fd; i++)
  {
    if (!(cptr = loc_clients[i]))
      continue;                 /* that clients are not mine */
    if (cptr == one)            /* must skip the origin !! */
      continue;
    if (IsServer(cptr))
    {
      for (acptr = client; acptr; acptr = acptr->cli_next)
        if (IsUser(acptr) && match_it(acptr, mask, what) && cli_from(acptr) == cptr)
          break;
      /* a person on that server matches the mask, so we
       *  send *one* msg to that server ...
       */
      if (acptr == NULL)
        continue;
      /* ... but only if there *IS* a matching person */
    }
    /* my client, does he match ? */
    else if (!(IsUser(cptr) && match_it(cptr, mask, what)))
      continue;

    if (IsServer(cptr))
    {
      vsendto_prefix_one2(cptr, from, ":%s %s %s :%s", fuente, comando, mask2,
          mensaje);
    }
    else
    {
      vsendto_prefix_one2(cptr, from,
          ":%s %s %s :*** Mensaje Global (%s -> %s): %s", fuente, comando,
          PunteroACadena(cptr->name), fuente2, mask2, mensaje);
    }
  }
  va_end(vl);

  return;
}
#endif
