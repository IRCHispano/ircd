dnl aclocal.m4 generated automatically by aclocal 1.4a

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

dnl
dnl Macro: unet_PIPE_CFLAGS
dnl
dnl   If the compiler understands -pipe, add it to CFLAGS if not already
dnl   there.
dnl
AC_DEFUN(unet_PIPE_CFLAGS,
[AC_MSG_CHECKING([if the compiler understands -pipe])
unet_cv_pipe_flags="$ac_cv_prog_gcc"
if test "$ac_cv_prog_gcc" = no; then
  OLDCFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -pipe"
  AC_TRY_COMPILE(,,unet_cv_pipe_flags=yes,)
  CFLAGS="$OLDCFLAGS"
fi
AC_MSG_RESULT($unet_cv_pipe_flags)
if test "$unet_cv_pipe_flags" = yes ; then
  x=`echo $CFLAGS | grep 'pipe' 2>/dev/null`
  if test "$x" = "" ; then
    CFLAGS="$CFLAGS -pipe"
  fi
fi
])

dnl
dnl Macro: unet_NONBLOCKING
dnl
dnl   Check whether we have posix, bsd or sysv non-blocking sockets and
dnl   define respectively NBLOCK_POSIX, NBLOCK_BSD or NBLOCK_SYSV.
dnl
AC_DEFUN(unet_NONBLOCKING,
[dnl Do we have posix, bsd or sysv non-blocking stuff ?
AC_CACHE_CHECK([for posix non-blocking], unet_cv_sys_nonblocking_posix,
[AC_TRY_RUN([#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <signal.h>
$ac_cv_type_signal alarmed() { exit(1); }
int main(void)
{
  char b[12];
  struct sockaddr x;
  size_t l = sizeof(x);
  int f = socket(AF_INET, SOCK_DGRAM, 0);
  if (f >= 0 && !(fcntl(f, F_SETFL, O_NONBLOCK)))
  {
    signal(SIGALRM, alarmed);
    alarm(2);
    recvfrom(f, b, 12, 0, &x, &l);
    alarm(0);
    exit(0);
  }
  exit(1);
}], unet_cv_sys_nonblocking_posix=yes, unet_cv_sys_nonblocking_posix=no)])
if test $unet_cv_sys_nonblocking_posix = yes; then
  AC_DEFINE(NBLOCK_POSIX)
else
AC_CACHE_CHECK([for bsd non-blocking], unet_cv_sys_nonblocking_bsd,
[AC_TRY_RUN([#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <signal.h>
$ac_cv_type_signal alarmed() { exit(1); }
int main(void)
{
  char b[12];
  struct sockaddr x;
  size_t l = sizeof(x);
  int f = socket(AF_INET, SOCK_DGRAM, 0);
  if (f >= 0 && !(fcntl(f, F_SETFL, O_NDELAY)))
  {
    signal(SIGALRM, alarmed);
    alarm(2);
    recvfrom(f, b, 12, 0, &x, &l);
    alarm(0);
    exit(0);
  }
  exit(1);
}], unet_cv_sys_nonblocking_bsd=yes, unet_cv_sys_nonblocking_bsd=no)])
if test $unet_cv_sys_nonblocking_bsd = yes; then
  AC_DEFINE(NBLOCK_BSD)
else
  AC_DEFINE(NBLOCK_SYSV)
fi
fi])

dnl
dnl Macro: unet_SIGNALS
dnl
dnl   Check if we have posix signals, reliable bsd signals or
dnl   unreliable sysv signals and define respectively POSIX_SIGNALS,
dnl   BSD_RELIABLE_SIGNALS or SYSV_UNRELIABLE_SIGNALS.
dnl
AC_DEFUN(unet_SIGNALS,
[dnl Do we have posix signals, reliable bsd signals or unreliable sysv signals ?
AC_CACHE_CHECK([for posix signals], unet_cv_sys_signal_posix,
[AC_TRY_COMPILE([#include <signal.h>],
[sigaction(SIGTERM, (struct sigaction *)0L, (struct sigaction *)0L)],
unet_cv_sys_signal_posix=yes, unet_cv_sys_signal_posix=no)])
if test $unet_cv_sys_signal_posix = yes; then
  AC_DEFINE(POSIX_SIGNALS)
else
AC_CACHE_CHECK([for bsd reliable signals], unet_cv_sys_signal_bsd,
[AC_TRY_RUN([#include <signal.h>
int calls = 0;
$ac_cv_type_signal handler()
{
  if (calls) return;
  calls++;
  kill(getpid(), SIGTERM);
  sleep(1);
}
int main(void)
{
  signal(SIGTERM, handler);
  kill(getpid(), SIGTERM);
  exit (0);
}], unet_cv_sys_signal_bsd=yes, unet_cv_sys_signal_bsd=no)])
if test $unet_cv_sys_signal_bsd = yes; then
  AC_DEFINE(BSD_RELIABLE_SIGNALS)
else
  AC_DEFINE(SYSV_UNRELIABLE_SIGNALS)
fi
fi])

dnl
dnl Macro: unet_DEFINE_SIZE_T_FMT
dnl
dnl Define SIZE_T_FMT to be "%u" or "%lu", whichever seems more appropriate.
dnl
AC_DEFUN(unet_DEFINE_SIZE_T_FMT,
[dnl Make educated guess :/, if size_t is a long or not
AC_CHECK_SIZEOF(size_t)dnl
AC_MSG_CHECKING(printf format of size_t)
if test "$ac_cv_sizeof_size_t" = 4 ; then
  AC_MSG_RESULT("%u")
  AC_DEFINE(SIZE_T_FMT, "%u")
else
  AC_MSG_RESULT("%lu")
  AC_DEFINE(SIZE_T_FMT, "%lu")
fi])

dnl
dnl Macro: unet_DEFINE_TIME_T_FMT
dnl
dnl Try to figure out if time_t is an int or not, and if so define
dnl TIME_T_FMT to be "%u", otherwise define it to be "%lu".
dnl Likewise define STIME_T_FMT for the signed format.
dnl
AC_DEFUN(unet_DEFINE_TIME_T_FMT,
[dnl Make educated guess :/, if time_t is a long or not
AC_MSG_CHECKING(size of time_t)
AC_CACHE_VAL(unet_cv_sizeof_time_t,
[AC_TRY_RUN([#include <stdio.h>
#include <sys/types.h>
main()
{
  FILE *f=fopen("conftestval", "w");
  if (!f) exit(1);
  fprintf(f, "%d\n", sizeof(time_t));
  exit(0);
}], unet_cv_sizeof_time_t=`cat conftestval`,
unet_cv_sizeof_time_t=0, unet_cv_sizeof_time_t=0)])
if test "$unet_cv_sizeof_time_t" = 0 ; then
  AC_MSG_RESULT(unknown)
  AC_DEFINE(TIME_T_FMT, "%lu")
  AC_DEFINE(STIME_T_FMT, "%ld")
else
  AC_MSG_RESULT([$unet_cv_sizeof_time_t])
  AC_MSG_CHECKING(printf format of time_t)
  if test "$unet_cv_sizeof_time_t" = "$ac_cv_sizeof_long" ; then
    AC_MSG_RESULT("%lu")
    AC_DEFINE(TIME_T_FMT, "%lu")
    AC_DEFINE(STIME_T_FMT, "%ld")
  else
    AC_MSG_RESULT("%u")
    AC_DEFINE(TIME_T_FMT, "%u")
    AC_DEFINE(STIME_T_FMT, "%d")
  fi
fi])

dnl
dnl Macro: unet_CHECK_TYPE_SIZES
dnl
dnl Check the size of several types and define a valid int16_t and int32_t.
dnl
AC_DEFUN(unet_CHECK_TYPE_SIZES,
[dnl Check type sizes
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
if test "$ac_cv_sizeof_int" = 2 ; then
  AC_CHECK_TYPE(int16_t, int)
  AC_CHECK_TYPE(u_int16_t, unsigned int)
elif test "$ac_cv_sizeof_short" = 2 ; then
  AC_CHECK_TYPE(int16_t, short)
  AC_CHECK_TYPE(u_int16_t, unsigned short)
else
  AC_MSG_ERROR([Cannot find a type with size of 16 bits])
fi
if test "$ac_cv_sizeof_int" = 4 ; then
  AC_CHECK_TYPE(int32_t, int)
  AC_CHECK_TYPE(u_int32_t, unsigned int)
elif test "$ac_cv_sizeof_short" = 4 ; then
  AC_CHECK_TYPE(int32_t, short)
  AC_CHECK_TYPE(u_int32_t, unsigned short)
elif test "$ac_cv_sizeof_long" = 4 ; then
  AC_CHECK_TYPE(int32_t, long)
  AC_CHECK_TYPE(u_int32_t, unsigned long)
else
  AC_MSG_ERROR([Cannot find a type with size of 32 bits])
fi])

dnl
dnl Macro: unet_FUNC_POLL_SYSCALL
dnl
dnl Try to figure out if we have a system call poll (not if it is emulated).
dnl Manical laughter...
dnl
AC_DEFUN(unet_FUNC_POLL_SYSCALL,
[AC_CHECK_HEADERS(poll.h)dnl
if test -z "$unet_cv_func_poll_syscall" ; then
  AC_MSG_CHECKING([if poll is a system call (please wait)])
else
  AC_MSG_CHECKING([if poll is a system call])
fi
AC_CACHE_VAL(unet_cv_func_poll_syscall,
[unet_cv_func_poll_syscall=no
dnl No need to go through the trouble when we don't have poll.h:
changequote(, )dnl
if test "$ac_cv_header_poll_h" = yes; then
  unet_dirs=`find /usr/include/sys -type f -name '*.h' -exec egrep '^#include <[^/]*/.*>' {} \; | sed -e 's/^.*<//' -e 's%/.*$%%' | sort | uniq`
  for i in $unet_dirs ; do
    if test "$unet_cv_func_poll_syscall" = no ; then
      unet_files=`ls /usr/include/$i/*.h 2> /dev/null`
      if test -n "$unet_files" ; then
	for j in $unet_files ; do
	  if test "$unet_cv_func_poll_syscall" = no ; then
	    unet_line=`egrep '^#define[[:space:]]+[[:alnum:]_]*[Pp][Oo][Ll][Ll]' $j`
	    if test -n "$unet_line" ; then
	      unet_sig=`echo "$unet_line" | sed -e 's/poll/fork/g' -e 's/POLL/FORK/g' -e 's/[[:space:]]//g' -e 's%/\*.*\*/%%g' -e 's/[0-9]//g'`
	      unet_set=`for k in "$unet_sig" ; do echo $k; done | sed -e 's% %|%g'`
	      unet_match=`sed -e 's/[[:space:]]//g' -e 's%/\*.*\*/%%g' -e 's/[0-9]//g' $j | egrep "$unet_set"`
	      if test -n "$unet_match" ; then
		unet_cv_func_poll_syscall=yes
	      fi
	    fi
	  fi
	done
      fi
    fi
  done
fi
changequote([, ])dnl
])
AC_MSG_RESULT([$unet_cv_func_poll_syscall])
])


# serial 1

# @defmac AC_PROG_CC_STDC
# @maindex PROG_CC_STDC
# @ovindex CC
# If the C compiler in not in ANSI C mode by default, try to add an option
# to output variable @code{CC} to make it so.  This macro tries various
# options that select ANSI C on some system or another.  It considers the
# compiler to be in ANSI C mode if it handles function prototypes correctly.
#
# If you use this macro, you should check after calling it whether the C
# compiler has been set to accept ANSI C; if not, the shell variable
# @code{am_cv_prog_cc_stdc} is set to @samp{no}.  If you wrote your source
# code in ANSI C, you can make an un-ANSIfied copy of it by using the
# program @code{ansi2knr}, which comes with Ghostscript.
# @end defmac

AC_DEFUN(AM_PROG_CC_STDC,
[AC_REQUIRE([AC_PROG_CC])
AC_BEFORE([$0], [AC_C_INLINE])
AC_BEFORE([$0], [AC_C_CONST])
dnl Force this before AC_PROG_CPP.  Some cpp's, eg on HPUX, require
dnl a magic option to avoid problems with ANSI preprocessor commands
dnl like #elif.
dnl FIXME: can't do this because then AC_AIX won't work due to a
dnl circular dependency.
dnl AC_BEFORE([$0], [AC_PROG_CPP])
AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(am_cv_prog_cc_stdc,
[am_cv_prog_cc_stdc=no
ac_save_CC="$CC"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc -D__EXTENSIONS__
for ac_arg in "" -qlanglvl=ansi -std1 "-Aa -D_HPUX_SOURCE" "-Xc -D__EXTENSIONS__"
do
  CC="$ac_save_CC $ac_arg"
  AC_TRY_COMPILE(
[#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
/* Most of the following tests are stolen from RCS 5.7's src/conf.sh.  */
struct buf { int x; };
FILE * (*rcsopen) (struct buf *, struct stat *, int);
static char *e (p, i)
     char **p;
     int i;
{
  return p[i];
}
static char *f (char * (*g) (char **, int), char **p, ...)
{
  char *s;
  va_list v;
  va_start (v,p);
  s = g (p, va_arg (v,int));
  va_end (v);
  return s;
}
int test (int i, double x);
struct s1 {int (*f) (int a);};
struct s2 {int (*f) (double a);};
int pairnames (int, char **, FILE *(*)(struct buf *, struct stat *, int), int, int);
int argc;
char **argv;
], [
return f (e, argv, 0) != argv[0]  ||  f (e, argv, 1) != argv[1];
],
[am_cv_prog_cc_stdc="$ac_arg"; break])
done
CC="$ac_save_CC"
])
if test -z "$am_cv_prog_cc_stdc"; then
  AC_MSG_RESULT([none needed])
else
  AC_MSG_RESULT($am_cv_prog_cc_stdc)
fi
case "x$am_cv_prog_cc_stdc" in
  x|xno) ;;
  *) CC="$CC $am_cv_prog_cc_stdc" ;;
esac
])

dnl Written by John Hawkinson <jhawk@mit.edu>. This code is in the Public
dnl Domain.
dnl
dnl This test is for network applications that need socket() and
dnl gethostbyname() -ish functions.  Under Solaris, those applications need to
dnl link with "-lsocket -lnsl".  Under IRIX, they should *not* link with
dnl "-lsocket" because libsocket.a breaks a number of things (for instance:
dnl gethostbyname() under IRIX 5.2, and snoop sockets under most versions of
dnl IRIX).
dnl
dnl Unfortunately, many application developers are not aware of this, and
dnl mistakenly write tests that cause -lsocket to be used under IRIX.  It is
dnl also easy to write tests that cause -lnsl to be used under operating
dnl systems where neither are necessary (or useful), such as SunOS 4.1.4, which
dnl uses -lnsl for TLI.
dnl
dnl This test exists so that every application developer does not test this in
dnl a different, and subtly broken fashion.
dnl
dnl It has been argued that this test should be broken up into two seperate
dnl tests, one for the resolver libraries, and one for the libraries necessary
dnl for using Sockets API. Unfortunately, the two are carefully intertwined and
dnl allowing the autoconf user to use them independantly potentially results in
dnl unfortunate ordering dependancies -- as such, such component macros would
dnl have to carefully use indirection and be aware if the other components were
dnl executed. Since other autoconf macros do not go to this trouble, and almost
dnl no applications use sockets without the resolver, this complexity has not
dnl been implemented.
dnl
dnl The check for libresolv is in case you are attempting to link statically
dnl and happen to have a libresolv.a lying around (and no libnsl.a).
dnl

AC_DEFUN(AC_LIBRARY_NET, [
   # Most operating systems have gethostbyname() in the default searched
   # libraries (i.e. libc):
   AC_CHECK_FUNC(gethostbyname, ,
     # Some OSes (eg. Solaris) place it in libnsl:
     AC_CHECK_LIB(nsl, gethostbyname, ,
       # Some strange OSes (SINIX) have it in libsocket:
       AC_CHECK_LIB(socket, gethostbyname, ,
          # Unfortunately libsocket sometimes depends on libnsl.
          # AC_CHECK_LIB's API is essentially broken so the following
          # ugliness is necessary:
          AC_CHECK_LIB(socket, gethostbyname,
             LIBS="-lsocket -lnsl $LIBS",
               AC_CHECK_LIB(resolv, gethostbyname),
             -lnsl)
       )
     )
   )
  AC_CHECK_FUNC(socket, , AC_CHECK_LIB(socket, socket, ,
    AC_CHECK_LIB(socket, socket, LIBS="-lsocket -lnsl $LIBS", , -lnsl)))
  ])


