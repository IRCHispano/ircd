/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/flagset.h
 *
 * Copyright (C) 2002-2014 IRC-Dev Development Team <devel@irc-dev.net>
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
 * @brief Structures and macros for handling sets of boolean flags.
 * @version $Id: flagset.h,v 1.1 2007-04-22 13:56:19 zolty Exp $
 */

#ifndef INCLUDED_flagset_h
#define INCLUDED_flagset_h

/** Single element in a flag bitset array. */
typedef unsigned long flagpage_t;

/** Number of bits in a flagpage_t. */
#define FLAGSET_NBITS (8 * sizeof(flagpage_t))
/** Element number for flag \a flag. */
#define FLAGSET_INDEX(flag) ((flag) / FLAGSET_NBITS)
/** Element bit for flag \a flag. */
#define FLAGSET_MASK(flag) (1ul<<((flag) % FLAGSET_NBITS))

/** Declare a flagset structure of a particular size. */
#define DECLARE_FLAGSET(name,max) \
  struct name \
  { \
    unsigned long bits[((max + FLAGSET_NBITS - 1) / FLAGSET_NBITS)]; \
  }

/** Test whether a flag is set in a flagset. */
#define FlagHas(set,flag) ((set)->bits[FLAGSET_INDEX(flag)] & FLAGSET_MASK(flag))
/** Set a flag in a flagset. */
#define FlagSet(set,flag) ((set)->bits[FLAGSET_INDEX(flag)] |= FLAGSET_MASK(flag))
/** Clear a flag in a flagset. */
#define FlagClr(set,flag) ((set)->bits[FLAGSET_INDEX(flag)] &= ~FLAGSET_MASK(flag))

#endif /* ndef INCLUDED_flagset_h */
