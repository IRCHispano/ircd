/*
 * IRC - Internet Relay Chat, common/packet.c
 * Copyright (C) 1990  Jarkko Oikarinen and
 *                     University of Oulu, Computing Center
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
#include "h.h"
#include "s_debug.h"
#include "struct.h"
#include "s_misc.h"
#include "s_bsd.h"
#include "ircd.h"
#include "msg.h"
#include "parse.h"
#include "send.h"
#include "packet.h"
#include "s_serv.h"
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
#include "dbuf.h"
#endif

#include <assert.h>

RCSTAG_CC("$Id: packet.c,v 1.1.1.1 1999/11/16 05:13:14 codercom Exp $");

void actualiza_contadores(aClient *cptr, int length)
{
  aClient *acpt = cptr->acpt;

  me.receiveB += length;        /* Update bytes received */
  cptr->receiveB += length;
  if (cptr->receiveB > 1023)
  {
    cptr->receiveK += (cptr->receiveB >> 10);
    cptr->receiveB &= 0x03ff;   /* 2^10 = 1024, 3ff = 1023 */
  }
  if (acpt != &me)
  {
    acpt->receiveB += length;
    if (acpt->receiveB > 1023)
    {
      acpt->receiveK += (acpt->receiveB >> 10);
      acpt->receiveB &= 0x03ff;
    }
  }
  else if (me.receiveB > 1023)
  {
    me.receiveK += (me.receiveB >> 10);
    me.receiveB &= 0x03ff;
  }
}

/*
 * dopacket
 *
 *    cptr - pointer to client structure for which the buffer data
 *           applies.
 *    buffer - pointer to the buffer containing the newly read data
 *    length - number of valid bytes of data in the buffer
 *
 *  Note:
 *    It is implicitly assumed that dopacket is called only
 *    with cptr of "local" variation, which contains all the
 *    necessary fields (buffer etc..)
 */
int dopacket(aClient *cptr, char *buffer, int length)
{
  Reg1 char *ch1;
  Reg2 char *ch2;
  char *cptrbuf;
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
  int microburst = 0;
  char buf_comp[BUFSIZE];
  int compr = cptr->negociacion & ZLIB_ESNET_IN;

  inicializa_microburst();
#endif

  actualiza_contadores(cptr, length);

  ch2 = buffer;
  cptrbuf = cptr->buffer;

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
  if (compr)
  {
    cptr->comp_in->avail_in = length;
    cptr->comp_in_total_in += length;
    cptr->comp_in->next_in = buffer;
  }
#endif

  do
  {                             /* Bucle de compresion */
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
    if (compr)
    {
      long length_out = cptr->comp_in->total_out;

      cptr->comp_in->avail_out = BUFSIZE;
      ch2 = cptr->comp_in->next_out = buf_comp;
      if (inflate(cptr->comp_in, Z_SYNC_FLUSH) != Z_OK)
        return exit_client(cptr, cptr, &me, "Error compresion");
      length = BUFSIZE - cptr->comp_in->avail_out;

      length_out -= cptr->comp_in->total_out;
      if (length_out < 0)
        length_out = -length_out;
      cptr->comp_in_total_out += length_out;
    }
#endif

    ch1 = cptrbuf + cptr->count;

    while (--length >= 0)
    {
      char g;

      g = (*ch1 = *ch2++);
      /*
       * Yuck.  Stuck.  To make sure we stay backward compatible,
       * we must assume that either CR or LF terminates the message
       * and not CR-LF.  By allowing CR or LF (alone) into the body
       * of messages, backward compatibility is lost and major
       * problems will arise. - Avalon
       */
      if (g < '\16' && (g == '\n' || g == '\r'))
      {
        if (ch1 == cptrbuf)
          continue;             /* Skip extra LF/CR's */
        *ch1 = '\0';
        me.receiveM += 1;       /* Update messages received */
        cptr->receiveM += 1;
        if (cptr->acpt != &me)
          cptr->acpt->receiveM += 1;

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
        if (length && !microburst)
        {
          microburst = !0;
          inicia_microburst();
        }
#endif

        if (IsServer(cptr))
        {
          if (parse_server(cptr, cptr->buffer, ch1) == CPTR_KILLED)
          {
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
            if (microburst)
              completa_microburst();
#endif
            return CPTR_KILLED;
          }
        }
        else if (parse_client(cptr, cptr->buffer, ch1) == CPTR_KILLED)
        {
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
          if (microburst)
            completa_microburst();
#endif
          return CPTR_KILLED;
        }
        /*
         *  Socket is dead so exit
         */
        if (IsDead(cptr))
        {
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
          if (microburst)
            completa_microburst();
#endif
          return exit_client(cptr, cptr, &me, LastDeadComment(cptr));
        }
        ch1 = cptrbuf;
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
/* Se empieza a recibir comprimido aqui */
        if ((!compr) && (cptr->negociacion & ZLIB_ESNET_IN))
        {
          compr = !0;
          while ((--length >= 0) && ((*ch2 == '\n') || (*ch2 == '\r')))
            ch2++;
          cptr->comp_in->avail_in = ++length;
          cptr->comp_in->next_in = ch2;
          length = 0;           /* Fuerza una nueva pasada */
        }
#endif
      }
      else if (ch1 < cptrbuf + (sizeof(cptr->buffer) - 1))
        ch1++;                  /* There is always room for the null */
    }
    cptr->count = ch1 - cptr->buffer;
#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
  }
  while (compr && (cptr->comp_in->avail_in > 0)); /* Bucle de compresion */
  if (microburst)
    completa_microburst();
#else
  }
  while (0);
#endif
  return 0;
}

/*
 * client_dopacket - handle client messages
 */
int client_dopacket(aClient *cptr, size_t length)
{
  assert(0 != cptr);

  me.receiveB += length;        /* Update bytes received */
  cptr->receiveB += length;

  if (cptr->receiveB > 1023)
  {
    cptr->receiveK += (cptr->receiveB >> 10);
    cptr->receiveB &= 0x03ff;   /* 2^10 = 1024, 3ff = 1023 */
  }
  if (me.receiveB > 1023)
  {
    me.receiveK += (me.receiveB >> 10);
    me.receiveB &= 0x03ff;
  }
  cptr->count = 0;

  ++me.receiveM;                /* Update messages received */
  ++cptr->receiveM;

  if (CPTR_KILLED == parse_client(cptr, cptr->buffer, cptr->buffer + length))
    return CPTR_KILLED;
  else if (IsDead(cptr))
    return exit_client(cptr, cptr, &me, LastDeadComment(cptr));

  return 0;
}
