dnl Prefered emacs editing mode: -*- shell-script -*-
dnl
dnl Process this file with autoconf to produce a configure script.
dnl
dnl IRC-Dev IRCD - An advanced and innovative IRC Daemon, configure
dnl
dnl Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
dnl Copyright (C) 2005  Kevin L. Mitchell <klmitch@mit.edu>
dnl Copyright (C) 2001  Kevin L. Mitchell <klmitch@mit.edu>
dnl Copyright (c) 1997, by Carlo Wood <carlo@runaway.xs4all.nl>
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
dnl
dnl  @(#)$Id: configure.ac,v 1.3 2007-11-11 21:53:03 zolty Exp $
dnl

AC_INIT([IRC-Hispano IRC Daemon],[u2.10.H.11],[devel@irc-dev.net],[ircd-irchispano])
AC_PREREQ(2.61)

AC_COPYRIGHT(
[This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA])

AC_REVISION($Revision: 1.3 $)

AC_CONFIG_SRCDIR([ircd/ircd.c])
AM_INIT_AUTOMAKE([foreign])

dnl Set the default prefix
AC_PREFIX_DEFAULT([$HOME])

dnl Define configuration header file and other output files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile ircd/Makefile])

dnl What type of system is this?
AC_CANONICAL_HOST

dnl Check for a compiler and make sure to ANSIfy it
AC_PROG_CC

dnl Look up the lexer and yaccer
AM_PROG_LEX
AC_PROG_YACC

dnl Make sure that lex is flex (or something sufficiently close)
echo $ECHO_N "checking $LEX compatibility... " >&6
if ! $LEX ${srcdir}/test.l >&5 ; then
  AC_MSG_ERROR([Unable to run $LEX on ${srcdir}/test.l, try using flex 2.5 or newer.])
elif ! $CC -c $CFLAGS $CPPFLAGS lex.yy.c >&5 ; then
  rm -f lex.yy.c
  AC_MSG_ERROR([Unable to compile lex.yy.c, check config.log for $LEX errors.])
else
  rm -f lex.yy.c lex.yy.o
  echo "${ECHO_T}okay" >&6
fi

dnl Let's find the library containing crypt...
AC_SEARCH_LIBS(crypt, [descrypt crypt], ,
[AC_MSG_ERROR([Unable to find library containing crypt()])])

dnl Do the checks necessary to figure out -lnsl / -lsocket stuff
AC_LIBRARY_NET


dnl VIEJO
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([malloc.h sys/malloc.h fcntl.h string.h strings.h sys/file.h sys/ioctl.h sys/time.h syslog unistd.h memory.h errno.h net/errno.h sys/cdefs.h])
AC_C_CONST
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_UID_T
unet_CHECK_TYPE_SIZES
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_FUNC_SETVBUF_REVERSED
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(strchr memcpy memmove)
AC_CHECK_FUNCS(gethostname gettimeofday mkdir strerror strtoken)
AC_CHECK_FUNCS(select socket uname)
AC_CHECK_FUNCS(setrlimit inet_netof getrusage times res_init)

dnl FIN VIEJO

dnl Check out header files
AC_HEADER_STDC
AC_CHECK_HEADERS([crypt.h poll.h inttypes.h stdint.h sys/devpoll.h \
		  sys/epoll.h sys/event.h sys/param.h sys/resource.h \
		  sys/socket.h])

dnl Checks for typedefs, structures, compiler characteristics, etc.
AC_C_CONST
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_UID_T
unet_CHECK_TYPE_SIZES
AC_CHECK_TYPE([struct sockaddr_in6],
	      [unet_have_sockaddr_in6=yes],
	      [unet_have_sockaddr_in6=no],
[#include <sys/types.h>
#include <netinet/in.h>])
AC_CHECK_TYPE([struct ssl_st],
              [unet_have_ssl=yes],
              [unet_have_ssl=no],
[#include <openssl/ssl.h>])

dnl Check for socklen_t.  In traditional BSD this is an int, but some
dnl OSes use a different type.  Test until we find something that will
dnl work properly.  Test borrowed from a patch submitted for Python.
AC_CHECK_TYPE([socklen_t], ,[
  AC_MSG_CHECKING([for socklen_t equivalent])
  AC_CACHE_VAL([curl_cv_socklen_t_equiv],
  [
dnl Systems have either "struct sockaddr*" or "void*" as second
dnl arg to getpeername.
    curl_cv_socklen_t_equiv=
    for arg2 in "struct sockaddr" void ; do
      for t in int size_t unsigned long "unsigned long" ; do
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/socket.h>
int getpeername (int $arg2 *, $t *);]], [[$t len;
  getpeername(0, 0, &len);]])],[curl_cv_socklen_t_equiv="$t"
  break],[])
      done
    done
  ])
  AC_MSG_RESULT($curl_cv_socklen_t_equiv)
  AC_DEFINE_UNQUOTED(socklen_t, $curl_cv_socklen_t_equiv,
      [type to use in place of socklen_t if not defined])],
  [#include <sys/types.h>
#include<sys/socket.h>])

dnl Checks for library functions.
AC_CHECK_FUNCS([kqueue setrlimit getrusage times])

dnl Figure out non-blocking and signals
unet_NONBLOCKING
unet_SIGNALS

dnl Check OS for os_dep files.
AC_MSG_CHECKING(for OS-dependent information)
case "$host" in
    *-linux*)
	AC_MSG_RESULT([Linux ($host) found.])
	unet_poll_syscall=yes
	;;

    *-solaris*)
	AC_MSG_RESULT([Solaris ($host) found.])
	if test x"$ac_cv_header_poll_h" = xyes; then
	    unet_poll_syscall=yes
	else
	    unet_poll_syscall=no
	fi
	;;

    *-sunos*)
	AC_MSG_RESULT([Solaris ($host) found.])
	unet_poll_syscall=no
	;;

    *-openbsd*)
	AC_MSG_RESULT([OpenBSD ($host) found.])
	if test x"$ac_cv_header_poll_h" = xyes; then
	    unet_poll_syscall=yes
	else
	    unet_poll_syscall=no
	fi
	;;

    *-*bsd*)
	AC_MSG_RESULT([Generic BSD ($host) found.])
	if test x"$ac_cv_header_poll_h" = xyes; then
	    unet_poll_syscall=yes
	else
	    unet_poll_syscall=no
	fi
	;;

    *-darwin*)
	AC_MSG_RESULT([Darwin (Mac OS X) ($host) found.])
	unet_poll_syscall=no
	;;

    *)
	AC_MSG_RESULT([Unknown system type $host found.])
	AC_MSG_WARN([Unknown OS type; using generic routines.])
	unet_poll_syscall=no
	;;
esac

dnl Check user configuration options

dnl Start with --enable-poll
unet_TOGGLE([poll], [$unet_poll_syscall],
    [Force poll to be used regardless of whether or not it is a system call],
    [whether to enable use of poll()],
[# Force poll to be disabled if there is no poll.h
if test x"$ac_cv_header_poll_h" != xyes; then
    unet_cv_enable_poll=no
fi])

# Set up the conditionals
if test x"$unet_cv_enable_poll" = xyes; then
    AC_DEFINE([USE_POLL], 1, [Specify whether or not to use poll()])
fi
AM_CONDITIONAL(ENGINE_POLL, [test x"$unet_cv_enable_poll" = xyes])

dnl Now --disable-devpoll...
unet_TOGGLE([devpoll], yes, [Disable the /dev/poll-based engine],
    [whether to enable the /dev/poll event engine],
[# Prohibit /dev/poll support if the header is unavailable
if test x"$ac_cv_header_sys_devpoll_h" = xno; then
    unet_cv_enable_devpoll=no
fi])

# Set up the conditionals
if test x"$unet_cv_enable_devpoll" = xyes; then
    AC_DEFINE([USE_DEVPOLL], 1, [Define to enable the /dev/poll engine])
fi
AM_CONDITIONAL(ENGINE_DEVPOLL, [test x"$unet_cv_enable_devpoll" = xyes])

dnl --disable-kqueue...
unet_TOGGLE([kqueue], yes, [Disable the kqueue-based engine],
    [whether to enable the kqueue event engine],
[# Prohibit kqueue() support if the header or functions are unavailable
if test x"$ac_cv_header_sys_event_h" = xno -o x"$ac_cv_func_kqueue" = xno; then
    unet_cv_enable_kqueue=no
fi])

# Set up the conditionals
if test x"$unet_cv_enable_kqueue" = xyes; then
    AC_DEFINE([USE_KQUEUE], 1, [Define to enable the kqueue engine])
fi
AM_CONDITIONAL(ENGINE_KQUEUE, [test x"$unet_cv_enable_kqueue" = xyes])

dnl ...and --disable-epoll
unet_TOGGLE([epoll], yes, [Disable the epoll-based engine],
    [whether to enable the epoll event engine],
[# Prohibit epoll() support if the header or functions are unavailable
if test x"$ac_cv_header_sys_epoll_h" = xno -o x"$ac_cv_func_epoll" = xno; then
    unet_cv_enable_epoll=no
fi])

# Set up the conditionals
if test x"$unet_cv_enable_epoll" = xyes; then
    AC_DEFINE([USE_EPOLL], 1, [Define to enable the epoll engine])

    # Must also check to see if we need to provide the function bodies
dnl XXX Should rework this check
    AC_MSG_CHECKING([whether epoll functions are properly defined])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <sys/epoll.h>],
				    [epoll_create(10);])],
	[AC_MSG_RESULT([yes])],
	[AC_MSG_RESULT([no])
	 AC_DEFINE([EPOLL_NEED_BODY], 1,
		[Define to implement epoll system calls])])
fi
AM_CONDITIONAL(ENGINE_EPOLL, [test x"$unet_cv_enable_epoll" = xyes])

dnl Is debugging mode requested?
unet_TOGGLE([debug], no, [Enable debugging mode],
    [whether to enable debug mode])

# Set the preprocessor symbol
if test x"$unet_cv_enable_debug" = xyes; then
    AC_DEFINE([DEBUGMODE], 1, [Enable debugging code])
fi

dnl What about assertions?
unet_TOGGLE([asserts], yes, [Disable assertion checking],
    [whether to enable asserts])

# Set the preprocessor symbol
if test x"$unet_cv_enable_asserts" = xno; then
    AC_DEFINE([NDEBUG], 1, [Disable assertion checking])
fi

dnl how about IPv6 support?
unet_TOGGLE([ipv6], $unet_have_sockaddr_in6, [Disable IPv6 support],
    [whether to enable IPv6],
[# Prohibit IPv6 support if sockaddr_in6 doesn't exist
if test x"$unet_have_sockaddr_in6" = xno; then
    unet_cv_enable_ipv6=no
fi])

# Set the preprocessor symbol
if test x"$unet_cv_enable_ipv6" = xyes; then
    AC_DEFINE([IPV6], 1, [Enable IPv6 support])
fi

dnl how about ZLIB support?
unet_TOGGLE([zlib], yes, [Disable ZLIB support],
    [whether to enable ZLIB])

# Set the preprocessor symbol
if test x"$unet_cv_enable_zlib" = xyes; then
    AC_DEFINE([USE_ZLIB], 1, [Enable ZLIB support])
    LIBS="$LIBS -L../libs/zlib -lz"
    CFLAGS="$CFLAGS -I../libs/zlib"
fi

#Temporal
LIBS="$LIBS -L../libs/libevent/.libs -levent"
CFLAGS="$CFLAGS -I../libs/libevent"

dnl how about SSL support?
#Temporal
unet_cv_enable_ssl=no
unet_TOGGLE([ssl], $unet_have_ssl, [Disable SSL support],
    [whether to enable SSL],
[# Prohibit SSL support if ssl doesn't exist
if test x"$unet_have_ssl" = xno; then
    unet_cv_enable_ssl=no
fi])

# Set the preprocessor symbol
if test x"$unet_cv_enable_ssl" = xyes; then
  AC_MSG_CHECKING([for OpenSSL includes])
  AC_ARG_WITH(openssl-includes,
  [  --with-openssl-includes Specify location of OpenSSL header files (default /usr/include)],
  [base_ssl_inc=$withval],
  [base_ssl_inc=/usr/include])
  [unet_cv_with_openssl_inc_prefix=$base_ssl_inc]
  AC_MSG_RESULT([$unet_cv_with_openssl_inc_prefix])
  AC_DEFINE_UNQUOTED(SSL_INCLUDES_PATH, "$base_ssl_inc",
  [Path name used as a base for the ssl include files.])

  AC_MSG_CHECKING([for OpenSSL libraries])
  AC_ARG_WITH(openssl-libs,
  [  --with-openssl-libs     Specify location of OpenSSL libs (default /usr/lib)],
  [unet_cv_with_openssl_prefix=$withval],
  [unet_cv_with_openssl_prefix=/usr/lib])
  AC_MSG_RESULT([$unet_cv_with_openssl_prefix])
  AC_DEFINE_UNQUOTED(SSL_LIBS_PATH, "$unet_cv_with_openssl_prefix",
  [Path name used as a base for the ssl lib files.])

  AC_MSG_CHECKING([for Kerberos includes])
  AC_ARG_WITH(kerberos-includes,
  [  --with-kerb-includes    Specify location of Kerberos includes (default /usr/kerberos/include)],
  [unet_cv_with_kerberos_prefix=$withval],
  [unet_cv_with_kerberos_prefix=/usr/kerberos/include])
  AC_MSG_RESULT([$unet_cv_with_kerberos_prefix])

  save_CFLAGS=$CFLAGS;
  CFLAGS="-lcrypto";

  unet_cv_enable_ssl="no";
  OPENSSL_LDFLAGS="";
  AC_CHECK_LIB(ssl, SSL_read, [
    AC_CHECK_LIB(crypto, X509_new, [
      AC_CHECK_HEADERS($base_ssl_inc/openssl/ssl.h $base_ssl_inc/openssl/err.h, [
        unet_cv_enable_ssl="yes";
        OPENSSL_LDFLAGS="-lcrypto -lssl"
        SSL_C=ssl.c
        AC_SUBST(SSL_C)
      ])
    ])
  ])
  CFLAGS=$save_CFLAGS

  if test "x$unet_cv_enable_ssl" = xyes; then
    AC_DEFINE([USE_SSL], 1, [Enable SSL support])

    LIBS="$LIBS -L$unet_cv_with_openssl_prefix $OPENSSL_LDFLAGS"
    CFLAGS="$CFLAGS -I$unet_cv_with_openssl_inc_prefix -I$unet_cv_with_kerberos_prefix"
  fi
fi

dnl Should we force inlines?
unet_TOGGLE([inlines], yes, [Disable inlining for a few critical functions],
    [whether to enable inlining for a few critical functions])

# Set the preprocessor symbol
if test x"$unet_cv_enable_inlines" = xyes; then
    AC_DEFINE([FORCEINLINE], 1, [Force inlining for a few critical functions])
fi

dnl How to copy one va_list to another?
AC_CACHE_CHECK([for va_copy], unet_cv_c_va_copy,
    [AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <stdarg.h>],
	[va_list ap1, ap2; va_copy(ap1, ap2);])],
    [unet_cv_c_va_copy="yes"],
    [unet_cv_c_va_copy="no"])])

if test "$unet_cv_c_va_copy" = "yes" ; then
  AC_DEFINE([HAVE_VA_COPY], 1, [Define if we have va_copy])
fi

AC_CACHE_CHECK([for __va_copy], unet_cv_c___va_copy,
    [AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <stdarg.h>],
	[va_list ap1, ap2; __va_copy(ap1, ap2);])],
    [unet_cv_c___va_copy="yes"],
    [unet_cv_c___va_copy="no"])])

if test "$unet_cv_c___va_copy" = "yes" ; then
  AC_DEFINE([HAVE___VA_COPY], 1, [Define if we have __va_copy])
fi

dnl --with-domain lets us set the domain name for some statistics-gathering
unet_domain=
if test -f /etc/resolv.conf; then
    unet_domain=`awk '/^domain/ { print $2; exit }' /etc/resolv.conf`
    if test x"$unet_domain" = x; then
	unet_domain=`awk '/^search/ { print $2; exit }' /etc/resolv.conf`
    fi
fi

unet_VALUE([domain], $unet_domain, [Domain name to use for local statistics],
    [for site domain name],
[# Sorry, but the domain *must* be set!
if test x"$unet_cv_with_domain" = x -o x"$unet_cv_with_domain" = xno; then
    AC_MSG_ERROR([Unable to determine server DNS domain])
fi])

AC_DEFINE_UNQUOTED([DOMAINNAME], "*$unet_cv_with_domain",
    [Domain name to be used for some statistics gathering])

dnl --with-maxcon allows us to set the maximum connections
unet_maxcon=`ulimit -Hn`
if test x"$unet_maxcon" = xunlimited; then
    unet_maxcon=`ulimit -Sn`
fi
unet_maxcon=`expr $unet_maxcon - 4`

unet_VALUE([maxcon], $unet_maxcon,
    [Maximum number of connections server will accept],
    [max connections],
[# Fix up a spurious --with-maxcon
if test x"$unet_cv_with_maxcon" = xyes -o x"$unet_cv_with_maxcon" = xno; then
    unet_cv_with_maxcon=$unet_maxcon
fi])

AC_DEFINE_UNQUOTED(MAXCONNECTIONS, $unet_cv_with_maxcon,
    [Maximum number of network connections])

dnl Report configuration
dnl AC_CONFIG_COMMANDS([default-1],[[echo "
dnl ircu is now hopefully configured for your system.

dnl   Host system:         $host_os
dnl   Prefix:              $prefix
dnl   Asserts:             $unet_cv_enable_asserts
dnl   Debug:               $unet_cv_enable_debug

dnl   Domain:              $unet_cv_with_domain
dnl   Maximum connections: $unet_cv_with_maxcon

dnl   poll() engine:       $unet_cv_enable_poll
dnl   kqueue() engine:     $unet_cv_enable_kqueue
dnl   /dev/poll engine:    $unet_cv_enable_devpoll
dnl   epoll() engine:      $unet_cv_enable_epoll
dnl "]],[[]])

dnl Output everything...
dnl AC_OUTPUT



dnl IRC Network environment
dnl Default DDB
ircdev_cv_environment_ddb=yes;

dnl --with-ddb-environment allows us to use DDB Environment
AC_MSG_CHECKING([whether to use DDB Environment (Distributed DataBase)])
AC_ARG_WITH([ddb-environment],
[  --with-ddb-environment      Use the DDB Environment (Distributed DataBase)],
[ircdev_cv_environment_ddb=$with_ddb_environment],
[AC_CACHE_VAL(ircdev_cv_environment_ddb,
[ircdev_cv_environment_ddb=no])])
AC_MSG_RESULT([$ircdev_cv_environment_ddb])

dnl --with-services-environment allows us to use Services Environment
AC_MSG_CHECKING([whether to use Services Environment (Ex TerraIRCU)])
AC_ARG_WITH([services-environment],
[  --with-services-environment Use the Services Environment (Ex TerraIRCU)],
[ircdev_cv_environment_services=$with_services_environment],
[AC_CACHE_VAL(ircdev_cv_environment_services,
[ircdev_cv_environment_services=no])])
AC_MSG_RESULT([$ircdev_cv_environment_services])

dnl --with-undernet-environment allows us to use Undernet Environment
AC_MSG_CHECKING([whether to use Undernet Environment])
AC_ARG_WITH([undernet-environment],
[  --with-undernet-environment Use the Undernet Environment],
[ircdev_cv_environment_undernet=$with_undernet_environment],
[AC_CACHE_VAL(ircdev_cv_environment_undernet,
[ircdev_cv_environment_undernet=no])])
AC_MSG_RESULT([$ircdev_cv_environment_undernet])

dnl if test x"$ircdev_cv_environment_ddb" != "xno" ; then
    AC_DEFINE([DDB], , [DDB Environment (Distributed DataBase)])
    AM_CONDITIONAL(ENVIRONMENT_DDB, [test x"$ircdev_cv_environment_ddb" = xyes])
dnl elif test x"$ircdev_cv_environment_services" != "xno" ; then
dnl    AC_DEFINE([SERVICES], , [Services Environment (Ex-TerraIRCU)])
dnl    AM_CONDITIONAL(ENVIRONMENT_SERVICES, [test x"$ircdev_cv_environment_services" = xyes])
dnl elif test x"$ircdev_cv_environment_undernet" != "xno" ; then
dnl    AC_DEFINE([UNDERNET], , [UnderNet Environment])
dnl    AM_CONDITIONAL(ENVIRONMENT_UNDERNET, [test x"$ircdev_cv_environment_undernet" = xyes])
dnl fi

dnl Finally really generate all output files:
AC_OUTPUT

dnl Report configuration
echo "IRC-Hispano ircd is now hopefully configured for your system."
echo ""
echo "  Host system:         $host_os"
echo "  Engines supported:"
if test x"$unet_cv_enable_epoll" = xyes; then         
echo "                       epoll()"
fi
if test x"$unet_cv_enable_kqueue" = xyes; then        
echo "                       kqueue()"
fi
if test x"$unet_cv_enable_devpoll" = xyes; then   
echo "                       /dev/poll"
fi
if test x"$unet_cv_enable_poll" = xyes; then
echo "                       poll()"
fi
echo ""
echo "  Asserts:             $unet_cv_enable_asserts"
dnl echo "  Warnings:            $unet_cv_enable_warnings"
echo "  Debug:               $unet_cv_enable_debug"
dnl echo "  Leak Detect:         $unet_cv_with_leak_detect"
echo "  IPv6:                $unet_cv_enable_ipv6"
echo "  ZLIB:                $unet_cv_enable_zlib"
echo "  SSL:                 $unet_cv_enable_ssl"
dnl echo "  Profile:             $unet_cv_enable_profile"
dnl echo "  Pedantic:            $unet_cv_enable_pedantic"
echo "  Inlines:             $unet_cv_enable_inlines"
dnl echo "  Debugging symbols:   $unet_cv_enable_symbols"
echo ""
echo "  Prefix:              $prefix"
dnl echo "  Binaries path:       $unet_bindir"
dnl echo "  IRCD Binary:         $unet_spath"
dnl echo "  Lib path:            $unet_libdir"
dnl echo "  Data path:           $unet_cv_with_dpath"
dnl echo "  Config file:         $unet_cpath"
dnl echo "  Debug log file:      $unet_lpath"
dnl echo "  Owner/mode:          $unet_cv_with_owner.$unet_cv_with_group ($unet_cv_with_mode)"
dnl echo "  Chroot:              $unet_cv_with_chroot"
echo ""
echo "  Maximum connections: $unet_cv_with_maxcon"
echo "  Domain:              $unet_cv_with_domain"
echo ""
if test x"$ircdev_cv_environment_ddb" = xyes; then
echo "  IRC Environment:     DDB (Distributed DataBase)"
elif test x"$ircdev_cv_environment_services" = xyes; then
echo "  IRC Environment:     Services (Ex TerraIRCU)"
elif test x"$ircdev_cv_environment_unet" = xyes; then
echo "  IRC Environment:     Undernet"
else
echo "  IRC Environment:     None (BUG, mail to zoltan@irc-dev.net)"
fi
echo ""
echo "Type \"make\" to compile IRC-Hispano IRCD"
