/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/packet.c
 *
 * Copyright (C) 2002-2015 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Input packet handling functions.
 * @version $Id$
 */
#include "config.h"

#include "packet.h"
#include "client.h"
#include "ircd.h"
#include "ircd_chattr.h"
#include "ircd_log.h"
#include "ircd_zlib.h"
#include "parse.h"
#include "s_bsd.h"
#include "s_misc.h"
#include "send.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

/** Add a certain number of bytes to a client's received statistics.
 * @param[in,out] cptr Client to update.
 * @param[in] length Number of newly received bytes to add.
 */
static void update_bytes_received(struct Client* cptr, unsigned int length)
{
  cli_receiveB(&me)  += length;     /* Update bytes received */
  cli_receiveB(cptr) += length;
}

/** Add one message to a client's received statistics.
 * @param[in,out] cptr Client to update.
 */
static void update_messages_received(struct Client* cptr)
{
  ++(cli_receiveM(&me));
  ++(cli_receiveM(cptr));
}

/** Handle received data from a directly connected server.
 * @param[in] cptr Peer server that sent us data.
 * @param[in] buffer Input buffer.
 * @param[in] length Number of bytes in input buffer.
 * @return 1 on success or CPTR_KILLED if the client is squit.
 */
int server_dopacket(struct Client* cptr, const char* buffer, int length)
{
  const char* src;
  char*       endp;
  char*       client_buffer;
#if defined(USE_ZLIB)
  int microburst = 0;
  char buf_comp[BUFSIZE];
  int compr = cli_connect(cptr)->zlib_negociation & ZLIB_IN;

  zlib_microburst_init();
#endif

  assert(0 != cptr);

  update_bytes_received(cptr, length);

#if defined(USE_ZLIB)
  src = buffer;
  client_buffer = cli_buffer(cptr);

  if (compr) {
    cli_connect(cptr)->comp_in->avail_in = length;
    cli_connect(cptr)->comp_in_total_in += length;
    cli_connect(cptr)->comp_in->next_in = (char *)buffer;
  }

 do
 {
 /* Bucle de compresion */
   if (compr)
    {
      long length_out = cli_connect(cptr)->comp_in->total_out;

      cli_connect(cptr)->comp_in->avail_out = BUFSIZE;
      src = cli_connect(cptr)->comp_in->next_out = buf_comp;
      if (inflate(cli_connect(cptr)->comp_in, Z_SYNC_FLUSH) != Z_OK)
        return exit_client(cptr, cptr, &me, "Error compresion");
      length = BUFSIZE - cli_connect(cptr)->comp_in->avail_out;

      length_out -= cli_connect(cptr)->comp_in->total_out;
      if (length_out < 0)
        length_out = -length_out;
      cli_connect(cptr)->comp_in_total_out += length_out;
    }

    endp = client_buffer + cli_count(cptr);

#else /* !USE_ZLIB */
  client_buffer = cli_buffer(cptr);
  endp = client_buffer + cli_count(cptr);
  src = buffer;
#endif

  while (length-- > 0) {
    *endp = *src++;
    /*
     * Yuck.  Stuck.  To make sure we stay backward compatible,
     * we must assume that either CR or LF terminates the message
     * and not CR-LF.  By allowing CR or LF (alone) into the body
     * of messages, backward compatibility is lost and major
     * problems will arise. - Avalon
     */
    if (IsEol(*endp)) {
      if (endp == client_buffer)
        continue;               /* Skip extra LF/CR's */
      *endp = '\0';

      update_messages_received(cptr);

#if defined(USE_ZLIB)
      if (length && !microburst)
      {
        microburst = !0;
        zlib_microburst_start();
      }
#endif

      if (parse_server(cptr, cli_buffer(cptr), endp) == CPTR_KILLED) {
#if defined(USE_ZLIB)
        if (microburst)
          zlib_microburst_complete();
#endif
        return CPTR_KILLED;
      }
      /*
       *  Socket is dead so exit
       */
      if (IsDead(cptr)) {
#if defined(USE_ZLIB)
        if (microburst)
          zlib_microburst_complete();
#endif
        return exit_client(cptr, cptr, &me, cli_info(cptr));
      }
      endp = client_buffer;

#if defined(USE_ZLIB)
      /* Se empieza a recibir comprimido aqui */
      if ((!compr) && (cli_connect(cptr)->zlib_negociation & ZLIB_IN))
      {
        compr = !0;
        while ((--length >= 0) && ((*src == '\n') || (*src == '\r')))
          src++;
        cli_connect(cptr)->comp_in->avail_in = ++length;
        cli_connect(cptr)->comp_in->next_in = (char *)src;
        length = 0;           /* Fuerza una nueva pasada */
      }
#endif
    }
    else if (endp < client_buffer + BUFSIZE)
      ++endp;                   /* There is always room for the null */
  }
  cli_count(cptr) = endp - cli_buffer(cptr);
#if defined(USE_ZLIB)
 }
  while (compr && (cli_connect(cptr)->comp_in->avail_in > 0)); /* Bucle de compresion */
  if (microburst)
    zlib_microburst_complete();
#endif

  return 1;
}

/** Handle received data from a new (unregistered) connection.
 * @param[in] cptr Unregistered connection that sent us data.
 * @param[in] buffer Input buffer.
 * @param[in] length Number of bytes in input buffer.
 * @return 1 on success or CPTR_KILLED if the client is squit.
 */
int connect_dopacket(struct Client *cptr, const char *buffer, int length)
{
  const char* src;
  char*       endp;
  char*       client_buffer;

  assert(0 != cptr);

  update_bytes_received(cptr, length);

  client_buffer = cli_buffer(cptr);
  endp = client_buffer + cli_count(cptr);
  src = buffer;

  while (length-- > 0)
  {
    *endp = *src++;
    /*
     * Yuck.  Stuck.  To make sure we stay backward compatible,
     * we must assume that either CR or LF terminates the message
     * and not CR-LF.  By allowing CR or LF (alone) into the body
     * of messages, backward compatibility is lost and major
     * problems will arise. - Avalon
     */
    if (IsEol(*endp))
    {
      /* Skip extra LF/CR's */
      if (endp == client_buffer)
        continue;
      *endp = '\0';

      update_messages_received(cptr);

      if (parse_client(cptr, cli_buffer(cptr), endp) == CPTR_KILLED)
        return CPTR_KILLED;
      /* Socket is dead so exit */
      if (IsDead(cptr))
        return exit_client(cptr, cptr, &me, cli_info(cptr));
      else if (IsServer(cptr))
      {
        cli_count(cptr) = 0;
        return server_dopacket(cptr, src, length);
      }
      endp = client_buffer;
    }
    else if (endp < client_buffer + BUFSIZE)
      /* There is always room for the null */
      ++endp;
  }
  cli_count(cptr) = endp - cli_buffer(cptr);
  return 1;
}

/** Handle received data from a local client.
 * @param[in] cptr Local client that sent us data.
 * @param[in] length Total number of bytes in client's input buffer.
 * @return 1 on success or CPTR_KILLED if the client is squit.
 */
int client_dopacket(struct Client *cptr, unsigned int length)
{
  assert(0 != cptr);

  update_bytes_received(cptr, length);
  update_messages_received(cptr);

  if (CPTR_KILLED == parse_client(cptr, cli_buffer(cptr), cli_buffer(cptr) + length))
    return CPTR_KILLED;
  else if (IsDead(cptr))
    return exit_client(cptr, cptr, &me, cli_info(cptr));

  return 1;
}
