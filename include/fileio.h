/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/fileio.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1998 Thomas Helvey <tomh@inxpress.net>
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
/** @file fileio.h
 * @brief ANSI FILE* clone API declarations.
 * @version $Id: fileio.h,v 1.5 2007-04-19 22:53:46 zolty Exp $
 */
#ifndef INCLUDED_fileio_h
#define INCLUDED_fileio_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>          /* size_t */
#define INCLUDED_sys_types_h
#endif

struct stat;

/** A mirror of the ANSI FILE struct, but it works for any
 * file descriptor. FileBufs are allocated when a file is opened with
 * fbopen, and they are freed when the file is closed using fbclose.
 * (Some OSes limit the range of file descriptors in a FILE*, for
 * example to fit in "char".)
 */
typedef struct FileBuf FBFILE;

/*
 * open a file and return a FBFILE*, see fopen(3)
 */
extern FBFILE *fbopen(const char *filename, const char *mode);
/*
 * close a file opened with fbopen, see fclose(3)
 */
extern void fbclose(FBFILE * fb);
/*
 * return the next character from the file, EOF on end of file
 * see fgetc(3)
 */
extern int fbgetc(FBFILE * fb);
/*
 * return next string in a file up to and including the newline character
 * see fgets(3)
 */
extern char *fbgets(char *buf, size_t len, FBFILE * fb);
/*
 * write a null terminated string to a file, see fputs(3)
 */
extern int fbputs(const char *str, FBFILE * fb);
/*
 * return the status of the file associated with fb, see fstat(3)
 */
extern int fbstat(struct stat *sb, FBFILE * fb);

#endif /* INCLUDED_fileio_h */
