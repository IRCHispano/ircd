/*
 * IRC - Internet Relay Chat, ircd/s_bsd.c
 * Copyright (C) 1990 Jarkko Oikarinen and
 *                    University of Oulu, Computing Center
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
#include <stdlib.h>
#include <sys/socket.h>
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if defined(SOL2)
#include <sys/filio.h>
#endif
#if defined(UNIXPORT)
#include <sys/un.h>
#endif
#include <stdio.h>
#if defined(HAVE_STROPTS_H)
#include <stropts.h>
#endif
#include <signal.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <utmp.h>
#include <sys/resource.h>
#if defined(USE_SYSLOG)
#include <syslog.h>
#endif
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <assert.h>

#include "h.h"
#include "s_debug.h"
#include "res.h"
#include "struct.h"
#include "s_bsd.h"
#include "s_serv.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "s_bdd.h"
#include "m_config.h"
#include "s_misc.h"
#include "s_bsd.h"
#include "hash.h"
#include "s_err.h"
#include "ircd.h"
#include "support.h"
#include "s_auth.h"
#include "class.h"
#include "packet.h"
#include "s_ping.h"
#include "channel.h"
#include "version.h"
#include "parse.h"
#include "common.h"
#include "bsd.h"
#include "numnicks.h"
#include "s_user.h"
#include "sprintf_irc.h"
#include "querycmds.h"
#include "IPcheck.h"
#include "msg.h"
#include "slab_alloc.h"
#include "ircd_alloc.h"
#include "ircd_string.h"
#include "ircd_chattr.h"

RCSTAG_CC("$Id$");

#define IP_LOOKUP_START ":%s NOTICE IP_LOOKUP :*** Looking up your hostname...\r\n"
#define IP_LOOKUP_OK ":%s NOTICE IP_LOOKUP :*** Found your hostname.\r\n"
#define IP_LOOKUP_CACHE ":%s NOTICE IP_LOOKUP :*** Found your hostname (CACHED!).\r\n"
#define IP_LOOKUP_FAIL ":%s NOTICE IP_LOOKUP :*** Couldn't resolve your hostname.\r\n"
#define IP_LOOKUP_BAD ":%s NOTICE IP_LOOKUP :*** IP# Mismatch: %s != %s[%08x], ignoring hostname.\r\n"



#if !defined(IN_LOOPBACKNET)
#define IN_LOOPBACKNET	0x7f
#endif

struct event evudp;
struct event evres;


aClient *loc_clients[MAXCONNECTIONS];
int highest_fd = 0, udpfd = -1, resfd = -1;
unsigned int readcalls = 0;
static struct sockaddr_in mysk;
static void polludp();

static struct sockaddr *connect_inet(aConfItem *, aClient *, int *);
static int completed_connection(aClient *);
static int check_init(aClient *, char *);
static void do_dns_async(), set_sock_opts(int, aClient *);
#if defined(UNIXPORT)
static struct sockaddr *connect_unix(aConfItem *, aClient *, int *);
static void add_unixconnection(aClient *, int);
static char unixpath[256];
#endif
static char readbuf[8192];
#if defined(VIRTUAL_HOST)
struct sockaddr_in vserv;
#endif
static int running_in_background;

#if defined(GODMODE)
#if !defined(NODNS)
#define NODNS
#endif
#if !defined(NOFLOODCONTROL)
#define NOFLOODCONTROL
#endif
#endif

/*
 * Try and find the correct name to use with getrlimit() for setting the max.
 * number of files allowed to be open by this process.
 */
#if defined(RLIMIT_FDMAX)
#define RLIMIT_FD_MAX	RLIMIT_FDMAX
#else
#if defined(RLIMIT_NOFILE)
#define RLIMIT_FD_MAX RLIMIT_NOFILE
#else
#if defined(RLIMIT_OPEN_MAX)
#define RLIMIT_FD_MAX RLIMIT_OPEN_MAX
#else
#undef RLIMIT_FD_MAX
#endif
#endif
#endif

/*
 * Actualiza la hora actual
 */
void update_now(void) {
#if defined(pyr)
  struct timeval nowt;
#endif

#if defined(pyr)
  gettimeofday(&nowt, NULL);
  now = nowt.tv_sec;
#else
  now = time(NULL);
#endif
}

/*
 * Cannot use perror() within daemon. stderr is closed in
 * ircd and cannot be used. And, worse yet, it might have
 * been reassigned to a normal connection...
 */

/*
 * report_error
 *
 * This a replacement for perror(). Record error to log and
 * also send a copy to all *LOCAL* opers online.
 *
 * text    is a *format* string for outputting error. It must
 *         contain only two '%s', the first will be replaced
 *         by the sockhost from the cptr, and the latter will
 *         be taken from strerror(errno).
 *
 * cptr    if not NULL, is the *LOCAL* client associated with
 *         the error.
 */
void report_error(char *text, aClient *cptr)
{
  Reg1 int errtmp = errno;      /* debug may change 'errno' */
  Reg2 char *host;
  int err;
  socklen_t len = sizeof(err);

  host = (cptr) ? PunteroACadena(cptr->name) : "";

  Debug((DEBUG_ERROR, text, host, strerror(errtmp)));

  /*
   * Get the *real* error from the socket (well try to anyway..).
   * This may only work when SO_DEBUG is enabled but its worth the
   * gamble anyway.
   */
#if defined(SO_ERROR) && !defined(SOL2)
  if (cptr && !IsMe(cptr) && cptr->fd >= 0)
    if (!getsockopt(cptr->fd, SOL_SOCKET, SO_ERROR, (OPT_TYPE *)&err, &len))
      if (err)
        errtmp = err;
#endif
  sendto_ops(text, host, strerror(errtmp));
#if defined(USE_SYSLOG)
  syslog(LOG_WARNING, text, host, strerror(errtmp));
#endif
  if (!running_in_background)
  {
    fprintf(stderr, text, host, strerror(errtmp));
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  return;
}

/*
 * inetport
 *
 * Create a socket in the AF_INET domain, bind it to the port given in
 * 'port' and listen to it.  Connections are accepted to this socket
 * depending on the IP# mask given by 'name'.  Returns the fd of the
 * socket created or -1 on error.
 */
int inetport(aClient *cptr, char *name, unsigned short int port, char *virtual)
{
  static struct sockaddr_in server;
  struct in_addr addr4;
  int ad[4], opt;
  socklen_t len = sizeof(server);
  char ipname[20];
  char ipvirtual[20];

  ad[0] = ad[1] = ad[2] = ad[3] = 0;

  /*
   * do it this way because building ip# from separate values for each
   * byte requires endian knowledge or some nasty messing. Also means
   * easy conversion of "*" 0.0.0.0 or 134.* to 134.0.0.0 :-)
   */
  sscanf(name, "%d.%d.%d.%d", &ad[0], &ad[1], &ad[2], &ad[3]);
  sprintf_irc(ipname, "%d.%d.%d.%d", ad[0], ad[1], ad[2], ad[3]);

  if(virtual && *virtual)
  {
    sscanf(virtual, "%d.%d.%d.%d", &ad[0], &ad[1], &ad[2], &ad[3]);
    sprintf_irc(ipvirtual, "%d.%d.%d.%d", ad[0], ad[1], ad[2], ad[3]);
  }
  
  if (cptr != &me)
  {
    char temp_sockhost[HOSTLEN + 1];
    sprintf(temp_sockhost, "%-.42s.%u", name, port);
    SlabStringAllocDup(&(cptr->sockhost), temp_sockhost, HOSTLEN);
    SlabStringAllocDup(&(cptr->name), PunteroACadena(me.name), 0);
  }
  /*
   * At first, open a new socket
   */
  if (cptr->fd == -1)
  {
    cptr->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cptr->fd < 0 && errno == EAGAIN)
    {
      sendto_ops("opening stream socket %s: No more sockets", cptr->name);
      return -1;
    }
  }
  if (cptr->fd < 0)
  {
    report_error("opening stream socket %s: %s", cptr);
    return -1;
  }
  else if (cptr->fd >= MAXCLIENTS)
  {
    sendto_ops("No more connections allowed (%s)", cptr->name);
    close(cptr->fd);
    return -1;
  }

  opt = 1;
  setsockopt(cptr->fd, SOL_SOCKET, SO_REUSEADDR, (OPT_TYPE *)&opt, sizeof(opt));

  /*
   * Bind a port to listen for new connections if port is non-null,
   * else assume it is already open and try get something from it.
   */
  if (port)
  {
    server.sin_family = AF_INET;
#if !defined(VIRTUAL_HOST)
    server.sin_addr.s_addr = INADDR_ANY;
#else
    if(virtual && *virtual)
      server.sin_addr.s_addr = inet_addr(ipvirtual);
    else
      server.sin_addr = vserv.sin_addr;
#endif
    server.sin_port = htons(port);
    if (bind(cptr->fd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
      report_error("binding stream socket %s: %s", cptr);
      close(cptr->fd);
      return -1;
    }
  }
  if (getsockname(cptr->fd, (struct sockaddr *)&server, &len))
  {
#if defined(DEBUGMODE)
    report_error("getsockname failed for %s: %s", cptr);
#endif
    close(cptr->fd);
    return -1;
  }

  if (cptr == &me)              /* KLUDGE to get it work... */
  {
    char buf[1024];

    sprintf_irc(buf, rpl_str(RPL_MYPORTIS), PunteroACadena(me.name), "*",
        ntohs(server.sin_port));
    write(1, buf, strlen(buf));
  }
  if (cptr->fd > highest_fd)
    highest_fd = cptr->fd;

  /* Pasamos de in_addr a irc_in_addr */
  addr4.s_addr = inet_addr(ipname);
  memset(&cptr->ip, 0, sizeof(struct irc_in_addr));
  cptr->ip.in6_16[5] = htons(65535);
  cptr->ip.in6_16[6] = htons(ntohl(addr4.s_addr) >> 16);
  cptr->ip.in6_16[7] = htons(ntohl(addr4.s_addr) & 65535);

  cptr->port = ntohs(server.sin_port);
  listen(cptr->fd, 128);        /* Use listen port backlog of 128 */
  loc_clients[cptr->fd] = cptr;

  CreateREvent(cptr, event_connection_callback);

  return 0;
}

#if defined(UNIXPORT)
/*
 * unixport
 *
 * Create a socket and bind it to a filename which is comprised of the path
 * (directory where file is placed) and port (actual filename created).
 * Set directory permissions as rwxr-xr-x so other users can connect to the
 * file which is 'forced' to rwxrwxrwx (different OS's have different need of
 * modes so users can connect to the socket).
 */
int unixport(aClient *cptr, char *path, unsigned short int port)
{
  struct sockaddr_un un;

  cptr->fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (cptr->fd == -1 && errno == EAGAIN)
  {
    sendto_ops("error opening unix domain socket %s: No more sockets",
        cptr->name);
    return -1;
  }
  if (cptr->fd == -1)
  {
    report_error("error opening unix domain socket %s: %s", cptr);
    return -1;
  }
  else if (cptr->fd >= MAXCLIENTS)
  {
    sendto_ops("No more connections allowed (%s)", cptr->name);
    close(cptr->fd);
    cptr->fd = -1;
    return -1;
  }

  un.sun_family = AF_UNIX;
#if HAVE_MKDIR
  mkdir(path, 0755);
#else
  if (chmod(path, 0755) == -1)
  {
    sendto_ops("error 'chmod 0755 %s': %s", path, strerror(errno));
#if defined(USE_SYSLOG)
    syslog(LOG_WARNING, "error 'chmod 0755 %s': %s", path, strerror(errno));
#endif
    close(cptr->fd);
    cptr->fd = -1;
    return -1;
  }
#endif
  sprintf_irc(unixpath, "%s/%u", path, port);
  unlink(unixpath);
  strncpy(un.sun_path, unixpath, sizeof(un.sun_path) - 1);
  un.sun_path[sizeof(un.sun_path) - 1] = 0;
  SlabStringAllocDup(&(cptr->name), me.name, 0);
  errno = 0;
  get_sockhost(cptr, unixpath);

  if (bind(cptr->fd, (struct sockaddr *)&un, strlen(unixpath) + 2) == -1)
  {
    report_error("error binding unix socket %s: %s", cptr);
    close(cptr->fd);
    return -1;
  }
  if (cptr->fd > highest_fd)
    highest_fd = cptr->fd;
  listen(cptr->fd, 5);
  chmod(unixpath, 0777);
  cptr->flags |= FLAGS_UNIX;
  cptr->port = 0;
  loc_clients[cptr->fd] = cptr;

  CreateREvent(cptr, event_connection_callback);

  return 0;
}

#endif

/*
 * init_sys
 */
void init_sys(void)
{
  Reg1 int fd;
#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_FD_MAX)
  struct rlimit limit;

  if (!getrlimit(RLIMIT_FD_MAX, &limit))
  {
#if defined(pyr)
    if (limit.rlim_cur < MAXCONNECTIONS)
#else
    if (limit.rlim_max < MAXCONNECTIONS)
#endif
    {
      fprintf(stderr, "ircd fd table too big\n");
      fprintf(stderr, "Hard Limit: " LIMIT_FMT " IRC max: %d\n",
#if defined(pyr)
          limit.rlim_cur,
#else
          limit.rlim_max,
#endif
          (int)MAXCONNECTIONS);
      fprintf(stderr, "Fix MAXCONNECTIONS\n");
      exit(-1);
    }
#if !defined(pyr)
    limit.rlim_cur = limit.rlim_max;  /* make soft limit the max */
    if (setrlimit(RLIMIT_FD_MAX, &limit) == -1)
    {
      fprintf(stderr, "error setting max fd's to " LIMIT_FMT "\n",
          limit.rlim_cur);
      exit(-1);
    }
#endif
  }
#endif /* defined(HAVE_SETRLIMIT) && defined(RLIMIT_FD_MAX) */
#if defined(DEBUGMODE)
  if (1)
  {
    static char logbuf[BUFSIZ];
#if SETVBUF_REVERSED
    setvbuf(stderr, _IOLBF, logbuf, sizeof(logbuf));
#else
    setvbuf(stderr, logbuf, _IOLBF, sizeof(logbuf));
#endif
  }
#endif

  for (fd = 3; fd < MAXCONNECTIONS; fd++)
  {
    close(fd);
    loc_clients[fd] = NULL;
  }
  loc_clients[1] = NULL;
  close(1);

  if (bootopt & BOOT_TTY)       /* debugging is going to a tty */
    goto init_dgram;
  if (!(bootopt & BOOT_DEBUG))
    close(2);

  if (((bootopt & BOOT_CONSOLE) || isatty(0)) && !(bootopt & BOOT_INETD))
  {
    if (fork())
      exit(0);
    running_in_background = 1;
#if defined(TIOCNOTTY)
    if ((fd = open("/dev/tty", O_RDWR)) >= 0)
    {
      ioctl(fd, TIOCNOTTY, (char *)NULL);
      close(fd);
    }
#endif
#if defined(HPUX) || defined(SOL2) || defined(_SEQUENT_) || \
    defined(_POSIX_SOURCE) || defined(SVR4)
    setsid();
#else
    setpgid(0, 0);
#endif
    close(0);                   /* fd 0 opened by inetd */
    loc_clients[0] = NULL;
  }
init_dgram:
  event_init();
  resfd = init_resolver();
  if(resfd>=0) {
    event_set(&evres, resfd, EV_READ|EV_PERSIST, (void *)event_async_dns_callback, NULL);
    if(event_add(&evres, NULL)==-1)
      Debug((DEBUG_ERROR, "ERROR: event_add EV_READ (event_async_dns_callback) fd = %d", resfd));
  }

  return;
}

void write_pidfile(void)
{
#if defined(PPATH)
  int fd;
  char buff[20];
  if ((fd = open(PPATH, O_CREAT | O_WRONLY, 0600)) >= 0)
  {
    memset(buff, 0, sizeof(buff));
    sprintf(buff, "%5d\n", (int)getpid());
    if (write(fd, buff, strlen(buff)) == -1)
      Debug((DEBUG_NOTICE, "Error writing to pid file %s", PPATH));
    close(fd);
    return;
  }
#if defined(DEBUGMODE)
  else
    Debug((DEBUG_NOTICE, "Error opening pid file \"%s\": %s",
        PPATH, strerror(errno)));
#endif
#endif
}

/*
 * Initialize the various name strings used to store hostnames. This is set
 * from either the server's sockhost (if client fd is a tty or localhost)
 * or from the ip# converted into a string. 0 = success, -1 = fail.
 */
static int check_init(aClient *cptr, char *sockn)
{
  struct sockaddr_in sk;
  socklen_t len = sizeof(struct sockaddr_in);
  sockn[HOSTLEN] = 0;

#if defined(UNIXPORT)
  if (IsUnixSocket(cptr))
  {
    strncpy(sockn, PunteroACadena(cptr->acpt->sockhost), HOSTLEN);
    get_sockhost(cptr, sockn);
    return 0;
  }
#endif

  /* If descriptor is a tty, special checking... */
  if (isatty(cptr->fd))
  {
    strncpy(sockn, me.name, HOSTLEN);
    memset(&sk, 0, sizeof(struct sockaddr_in));
  }
  else if (getpeername(cptr->fd, (struct sockaddr *)&sk, &len) == -1)
  {
    report_error("connect failure: %s %s", cptr);
    return -1;
  }
  strcpy(sockn, inetntoa(sk.sin_addr));

  if (inet_netof(sk.sin_addr) == IN_LOOPBACKNET)
  {
    cptr->hostp = NULL;
    strncpy(sockn, me.name, HOSTLEN);
  }

  /* Pasamos de sockaddr_in a irc_in_addr */
  memset(&cptr->ip, 0, sizeof(struct irc_in_addr));
  cptr->ip.in6_16[5] = htons(65535);
  cptr->ip.in6_16[6] = htons(ntohl(sk.sin_addr.s_addr) >> 16);
  cptr->ip.in6_16[7] = htons(ntohl(sk.sin_addr.s_addr) & 65535);

  cptr->port = ntohs(sk.sin_port);

  return 0;
}

/*
 * Ordinary client access check. Look for conf lines which have the same
 * status as the flags passed.
 */
enum AuthorizationCheckResult check_client(aClient *cptr)
{
  char buf[HOSTLEN + 1 + 1024];
  char host_buf[HOSTLEN + 1];
  int num_clones = -1;
  int iline = 0;
  struct db_reg *reg;
  Reg3 const char *hname;
  static char sockname[HOSTLEN + 1];
  Reg2 struct hostent *hp = NULL;
  Reg3 int i;
  enum AuthorizationCheckResult acr;
  struct in_addr addr4;

  /* Pasamos de irc_in_addr a in_addr */
  addr4.s_addr = (cptr->ip.in6_16[6] | cptr->ip.in6_16[7] << 16);

  ClearAccess(cptr);
  Debug((DEBUG_DNS, "ch_cl: check access for %s[%s]",
      cptr->name, ircd_ntoa(&cptr->ip)));

  if (check_init(cptr, sockname))
    return ACR_BAD_SOCKET;

  if (!IsUnixSocket(cptr))
    hp = cptr->hostp;
  /*
   * Verify that the host to ip mapping is correct both ways and that
   * the ip#(s) for the socket is listed for the host.
   */
  if (hp)
  {
    for (i = 0; hp->h_addr_list[i]; i++)
      if (!memcmp(hp->h_addr_list[i], &addr4, sizeof(struct in_addr)))
        break;
    if (!hp->h_addr_list[i])
    {
      sendto_op_mask(SNO_IPMISMATCH, "IP# Mismatch: %s != %s[%08x]",
          ircd_ntoa(&cptr->ip), hp->h_name, *((unsigned int *)hp->h_addr));
#ifndef HISPANO_WEBCHAT
      if (IsUserPort(cptr))
      {
        sprintf_irc(sendbuf, IP_LOOKUP_BAD,
            me.name, ircd_ntoa(&cptr->ip), hp->h_name,
            *((unsigned int *)hp->h_addr));
        write(cptr->fd, sendbuf, strlen(sendbuf));
      }
#endif
      hp = NULL;
    }
  }

/*
** Gestion Distribuida de Clones
*/

#if defined(BDD_CLONES)

  if (hp)
  {
    for (i = 0, hname = hp->h_name; hname; hname = hp->h_aliases[i++])
    {
      strncpy(host_buf, hname, HOSTLEN);
      host_buf[HOSTLEN] = '\0';
      num_clones = IPbusca_clones(host_buf);
      if (num_clones != -1)
        break;                  /* Tenemos un HIT! */
    }
/*
** Nos quedamos con el nombre canonico
*/
    strncpy(host_buf, hp->h_name, HOSTLEN);
    host_buf[HOSTLEN] = '\0';
  }
  else
  {
    strcpy(host_buf, ircd_ntoa(&cptr->ip));
    num_clones = IPbusca_clones(host_buf);
  }

/*
** Clones permitidos para los
** clientes en general, sin Iline.
*/
  if (num_clones == -1)
  {
    num_clones = numero_maximo_de_clones_por_defecto;
    if (!num_clones)
      num_clones = -1;
    iline = 0;
  }
  else
    iline = !0;                 /* Tiene Iline */
  if ((num_clones != -1) && (IPcheck_nr(cptr) > num_clones))
  {
/*
** Informacion adicional bajo base de datos
*/
    buf[0] = '\0';
    reg = db_buscar_registro(BDD_CONFIGDB, BDD_MENSAJE_DE_DEMASIADOS_CLONES);
    if (reg)
    {
      strcpy(buf, ". ");
      strcat(buf, reg->valor);
    }
/*
** Ojo con pasarle un buffer que, a su vez, contenga "%"
*/
    sendto_one(cptr,
        ":%s NOTICE %s :En esta red solo se permiten %d clones para tu IP (%s)%s",
        me.name, cptr->name, num_clones, host_buf, buf);

/*
** Si tiene Iline no debemos permitir
** Throttle, ya que se supone que es
** un usuario legitimo de multiples
** conexiones
*/
    if (iline)
      IPcheck_connect_fail(cptr);
    return ACR_TOO_MANY_FROM_IP;
  }
#endif

  if ((acr = attach_Iline(cptr, hp, sockname)))
  {
    Debug((DEBUG_DNS, "ch_cl: access denied: %s[%s]", cptr->name, sockname));
    return acr;
  }

  Debug((DEBUG_DNS, "ch_cl: access ok: %s[%s]", cptr->name, sockname));

  if (inet_netof(addr4) == IN_LOOPBACKNET || IsUnixSocket(cptr) ||
      inet_netof(addr4) == inet_netof(mysk.sin_addr))
  {
    ircstp->is_loc++;
  }
  return ACR_OK;
}

#define CFLAG	CONF_CONNECT_SERVER

/*
 * check_server()
 *
 * Check access for a server given its name (passed in cptr struct).
 * Must check for all C lines which have a name which matches the
 * name given and a host which matches. A host alias which is the
 * same as the server name is also acceptable in the host field of a
 * C line.
 *
 * Returns
 *  0 = Success
 * -1 = Access denied
 * -2 = Bad socket.
 */
int check_server(aClient *cptr)
{
  Reg1 const char *name;
  Reg2 aConfItem *c_conf = NULL;
  struct hostent *hp = NULL;
  Link *lp;
  char abuff[HOSTLEN + USERLEN + 2];
  char sockname[HOSTLEN + 1], fullname[HOSTLEN + 1];
  int i;
  struct in_addr addr4;

  /* Pasamos de irc_in_addr a in_addr */
  addr4.s_addr = (cptr->ip.in6_16[6] | cptr->ip.in6_16[7] << 16);

  name = cptr->name;
  Debug((DEBUG_DNS, "sv_cl: check access for %s[%s]", name,
      PunteroACadena(cptr->sockhost)));

  if (IsUnknown(cptr) && !attach_confs(cptr, name, CFLAG))
  {
    Debug((DEBUG_DNS, "No C lines for %s", name));
    return -1;
  }
  lp = cptr->confs;
  /*
   * We initiated this connection so the client should have a C and N
   * line already attached after passing through the connec_server()
   * function earlier.
   */
  if (IsConnecting(cptr) || IsHandshake(cptr))
  {
    c_conf = find_conf(lp, name, CFLAG);
    if (!c_conf)
    {
      sendto_ops("Connecting Error: %s", name);
      det_confs_butmask(cptr, 0);
      return -1;
    }
  }
#if defined(UNIXPORT)
  if (IsUnixSocket(cptr))
  {
    if (!c_conf)
      c_conf = find_conf(lp, name, CFLAG);
  }
#endif

  /*
   * If the servername is a hostname, either an alias (CNAME) or
   * real name, then check with it as the host. Use gethostbyname()
   * to check for servername as hostname.
   */
  if (!IsUnixSocket(cptr) && !cptr->hostp)
  {
    Reg1 aConfItem *aconf;

    aconf = count_clines(lp);
    if (aconf)
    {
      Reg1 char *s;
      Link lin;

      /*
       * Do a lookup for the CONF line *only* and not
       * the server connection else we get stuck in a
       * nasty state since it takes a SERVER message to
       * get us here and we cant interrupt that very well.
       */
      ClearAccess(cptr);
      lin.value.aconf = aconf;
      lin.flags = ASYNC_CONF;
      update_nextdnscheck(0);
      //nextdnscheck = 1;
      if ((s = strchr(aconf->host, '@')))
        s++;
      else
        s = aconf->host;
      Debug((DEBUG_DNS, "sv_ci:cache lookup (%s)", s));
      hp = gethost_byname(s, &lin);
    }
  }

  lp = cptr->confs;

  ClearAccess(cptr);
  if (check_init(cptr, sockname))
    return -2;

check_serverback:
  if (hp)
  {
    for (i = 0; hp->h_addr_list[i]; i++)
      if (!memcmp(hp->h_addr_list[i], &addr4, sizeof(struct in_addr)))
        break;
    if (!hp->h_addr_list[i])
    {
      sendto_op_mask(SNO_IPMISMATCH, "IP# Mismatch: %s != %s[%08x]",
          ircd_ntoa(&cptr->ip), hp->h_name, *((unsigned int *)hp->h_addr));
      hp = NULL;
    }
  }
  else if (cptr->hostp)
  {
    hp = cptr->hostp;
    goto check_serverback;
  }

  if (hp)
    /*
     * If we are missing a C line from above, search for
     * it under all known hostnames we have for this ip#.
     */
    for (i = 0, name = hp->h_name; name; name = hp->h_aliases[i++])
    {
      strncpy(fullname, name, sizeof(fullname) - 1);
      fullname[sizeof(fullname) - 1] = 0;
      add_local_domain(fullname, HOSTLEN - strlen(fullname));
      Debug((DEBUG_DNS, "sv_cl: gethostbyaddr: %s->%s", sockname, fullname));
      sprintf_irc(abuff, "%s@%s", PunteroACadena(cptr->username), fullname);
      if (!c_conf)
        c_conf = find_conf_host(lp, abuff, CFLAG);
      if (c_conf)
      {
        get_sockhost(cptr, fullname);
        break;
      }
    }
  name = cptr->name;

  /*
   * Check for C lines with the hostname portion the ip number
   * of the host the server runs on. This also checks the case where
   * there is a server connecting from 'localhost'.
   */
  if (IsUnknown(cptr) && !c_conf)
  {
    sprintf_irc(abuff, "%s@%s", PunteroACadena(cptr->username), sockname);
    if (!c_conf)
      c_conf = find_conf_host(lp, abuff, CFLAG);
  }
  /*
   * Attach by IP# only if all other checks have failed.
   * It is quite possible to get here with the strange things that can
   * happen when using DNS in the way the irc server does. -avalon
   */
  if (!hp)
  {
    if (!c_conf)
      c_conf =
          find_conf_ip(lp, (char *)&addr4, PunteroACadena(cptr->username),
          CFLAG);
  }
  else
    for (i = 0; hp->h_addr_list[i]; i++)
    {
      if (!c_conf)
        c_conf =
            find_conf_ip(lp, hp->h_addr_list[i], PunteroACadena(cptr->username),
            CFLAG);
    }
  /*
   * detach all conf lines that got attached by attach_confs()
   */
  det_confs_butmask(cptr, 0);
  /*
   * if no C lines, then deny access
   */
  if (!c_conf)
  {
    get_sockhost(cptr, sockname);
    Debug((DEBUG_DNS, "sv_cl: access denied: %s[%s@%s] c %p",
        name, PunteroACadena(cptr->username), PunteroACadena(cptr->sockhost),
        c_conf));
    return -1;
  }
  /*
   * attach the C lines to the client structure for later use.
   */
  attach_conf(cptr, c_conf);
  attach_confs(cptr, name, CONF_HUB | CONF_LEAF | CONF_UWORLD);

  if ((c_conf->ipnum.s_addr == INADDR_NONE) && !IsUnixSocket(cptr))
    memcpy(&c_conf->ipnum, &addr4, sizeof(struct in_addr));
  if (!IsUnixSocket(cptr))
    get_sockhost(cptr, c_conf->host);

  Debug((DEBUG_DNS, "sv_cl: access ok: %s[%s]", name,
      PunteroACadena(cptr->sockhost)));
  return 0;
}

#undef	CFLAG

/*
 * completed_connection
 *
 * Complete non-blocking connect()-sequence. Check access and
 * terminate connection, if trouble detected.
 *
 * Return  TRUE, if successfully completed
 *        FALSE, if failed and ClientExit
 */
static int completed_connection(aClient *cptr)
{
  aConfItem *aconf;
  time_t newts;
  aClient *acptr;
  int i;

  aconf = find_conf(cptr->confs, cptr->name, CONF_CONNECT_SERVER);
  if (!aconf)
  {
    sendto_ops("Lost C-Line for %s", cptr->name);
    return -1;
  }
  if (!BadPtr(aconf->passwd))
    sendto_one(cptr, "PASS :%s", aconf->passwd);

  make_server(cptr);
  /* Create a unique timestamp */
  newts = TStime();
  for (i = highest_fd; i >= 0; i--)
  {
    if (!(acptr = loc_clients[i]) || (!IsServer(acptr) && !IsHandshake(acptr)))
      continue;
    if (acptr->serv->timestamp >= newts)
      newts = acptr->serv->timestamp + 1;
  }
  cptr->serv->timestamp = newts;
  SetHandshake(cptr);
  /* Make us timeout after twice the timeout for DNS look ups */
  cptr->lasttime = now;
  cptr->flags |= FLAGS_PINGSENT;

#if defined(ESNET_NEG)
  envia_config_req(cptr);
#endif

  sendto_one(cptr,
      "SERVER %s 1 " TIME_T_FMT " " TIME_T_FMT " J%s %s%s +%s :%s",
      my_name_for_link(me.name, aconf), me.serv->timestamp, newts,
      MAJOR_PROTOCOL, NumServCap(&me),
#if defined(HUB)
      "h",
#else
      "",
#endif
      PunteroACadena(me.info));
  tx_num_serie_dbs(cptr);

  if (!IsDead(cptr))
    start_auth(cptr);

  return (IsDead(cptr)) ? -1 : 0;
}

/*
 * close_connection
 *
 * Close the physical connection. This function must make
 * MyConnect(cptr) == FALSE, and set cptr->from == NULL.
 */
void close_connection(aClient *cptr)
{
  Reg1 aConfItem *aconf;
  Reg2 int i, j;
  int empty = cptr->fd;

  if (IsServer(cptr))
  {
    ircstp->is_sv++;
    ircstp->is_sbs += cptr->sendB;
    ircstp->is_sbr += cptr->receiveB;
    ircstp->is_sks += cptr->sendK;
    ircstp->is_skr += cptr->receiveK;
    ircstp->is_sti += now - cptr->firsttime;
    if (ircstp->is_sbs > 1023)
    {
      ircstp->is_sks += (ircstp->is_sbs >> 10);
      ircstp->is_sbs &= 0x3ff;
    }
    if (ircstp->is_sbr > 1023)
    {
      ircstp->is_skr += (ircstp->is_sbr >> 10);
      ircstp->is_sbr &= 0x3ff;
    }
  }
  else if (IsUser(cptr))
  {
    ircstp->is_cl++;
    ircstp->is_cbs += cptr->sendB;
    ircstp->is_cbr += cptr->receiveB;
    ircstp->is_cks += cptr->sendK;
    ircstp->is_ckr += cptr->receiveK;
    ircstp->is_cti += now - cptr->firsttime;
    if (ircstp->is_cbs > 1023)
    {
      ircstp->is_cks += (ircstp->is_cbs >> 10);
      ircstp->is_cbs &= 0x3ff;
    }
    if (ircstp->is_cbr > 1023)
    {
      ircstp->is_ckr += (ircstp->is_cbr >> 10);
      ircstp->is_cbr &= 0x3ff;
    }
  }
  else
    ircstp->is_ni++;

#if defined(ESNET_NEG) && defined(ZLIB_ESNET)
/*
** Siempre es una conexion nuestra
*/
    if (cptr->negociacion & ZLIB_ESNET_IN)
    {
      inflateEnd(cptr->comp_in);
      MyFree(cptr->comp_in);
    }
    if (cptr->negociacion & ZLIB_ESNET_OUT)
    {
      deflateEnd(cptr->comp_out);
      MyFree(cptr->comp_out);
    }
#endif
  
  /*
   * Remove outstanding DNS queries.
   */
  del_queries((char *)cptr);
  /*
   * If the connection has been up for a long amount of time, schedule
   * a 'quick' reconnect, else reset the next-connect cycle.
   */

  if ((aconf = find_conf_exact(cptr->name, PunteroACadena(cptr->username),
      PunteroACadena(cptr->sockhost), CONF_CONNECT_SERVER)))
  {
    /*
     * Reschedule a faster reconnect, if this was a automaticly
     * connected configuration entry. (Note that if we have had
     * a rehash in between, the status has been changed to
     * CONF_ILLEGAL). But only do this if it was a "good" link.
     */
    aconf->hold = now;
    aconf->hold += (aconf->hold - cptr->since > HANGONGOODLINK) ?
        HANGONRETRYDELAY : ConfConFreq(aconf);
  }

  if (cptr->authfd >= 0) {
    close(cptr->authfd);
    DelRWAuthEvent(cptr);
  }

  if (cptr->fd >= 0)
  {
    flush_connections(cptr->fd);
    loc_clients[cptr->fd] = NULL;
    close(cptr->fd);
    DelClientEvent(cptr);
    cptr->fd = -2;
  }

  DBufClear(&cptr->sendQ);
  DBufClear(&cptr->recvQ);
  if (cptr->passwd)
  {
    MyFree(cptr->passwd);
    cptr->passwd = NULL;
  }
  set_snomask(cptr, 0, SNO_SET);
  /*
   * Clean up extra sockets from P-lines which have been discarded.
   */
  if (cptr->acpt != &me && cptr->acpt != cptr)
  {
    aconf = cptr->acpt->confs->value.aconf;
    if (aconf->clients > 0)
      aconf->clients--;
    if (!aconf->clients && IsIllegal(aconf))
      close_connection(cptr->acpt);
  }

  for (; highest_fd > 0; highest_fd--)
    if (loc_clients[highest_fd])
      break;

  det_confs_butmask(cptr, 0);

  /*
   * fd remap to keep loc_clients[i] filled at the bottom.
   */
  if (empty > 0)
    if ((j = highest_fd) > (i = empty) && !IsLog(loc_clients[j]))
    {
      if (IsListening(loc_clients[j]))
        return;
      if (dup2(j, i) == -1)
        return;

      loc_clients[i] = loc_clients[j];
      loc_clients[i]->fd = i;
      DelRWEvent(loc_clients[i]);
      // Renumero tambien los eventos
      if(loc_clients[i]->evread)
      {
        loc_clients[i]->evread->ev_fd=i;
        UpdateRead(loc_clients[i]);
      }
      if(loc_clients[i]->evwrite)
      {
        loc_clients[i]->evwrite->ev_fd=i;
        UpdateWrite(loc_clients[i]);
      }

      loc_clients[j] = NULL;
      close(j);
      while (!loc_clients[highest_fd])
        highest_fd--;
    }

  return;
}

/*
 *  set_sock_opts
 */
static void set_sock_opts(int fd, aClient *cptr)
{
  socklen_t opt;
#if defined(SO_REUSEADDR)
  opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
      (OPT_TYPE *)&opt, sizeof(opt)) < 0)
    report_error("setsockopt(SO_REUSEADDR) %s: %s", cptr);
#endif
#if defined(SO_USELOOPBACK)
  opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_USELOOPBACK,
      (OPT_TYPE *)&opt, sizeof(opt)) < 0)
  {
#if defined(DEBUGMODE)
    report_error("setsockopt(SO_USELOOPBACK) %s: %s", cptr);
#endif
  }
#endif
#if defined(SO_RCVBUF)
  opt = 8192;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (OPT_TYPE *)&opt, sizeof(opt)) < 0)
  {
#if defined(DEBUGMODE)
    report_error("setsockopt(SO_RCVBUF) %s: %s", cptr);
#endif /* DEBUGMODE */
  }
#endif
#if defined(SO_SNDBUF)
#if defined(_SEQUENT_)
/*
 * Seems that Sequent freezes up if the receving buffer is a different size
 * to the sending buffer (maybe a tcp window problem too).
 */
  opt = 8192;
#else
  opt = 8192;
#endif
  if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (OPT_TYPE *)&opt, sizeof(opt)) < 0)
  {
#if defined(DEBUGMODE)
    report_error("setsockopt(SO_SNDBUF) %s: %s", cptr);
#endif
  }
#endif
#if defined(IP_OPTIONS) && defined(IPPROTO_IP)
  {
    char *s = readbuf, *t = readbuf + sizeof(readbuf) / 2;

    opt = sizeof(readbuf) / 8;
    if (getsockopt(fd, IPPROTO_IP, IP_OPTIONS, (OPT_TYPE *)t, &opt) < 0)
    {
#if defined(DEBUGMODE)
      report_error("getsockopt(IP_OPTIONS) %s: %s", cptr);
#endif
    }
    else if (opt > 0 && opt != sizeof(readbuf) / 8)
    {
      for (*readbuf = '\0'; opt > 0; opt--, s += 3)
        sprintf(s, "%02x:", *t++);
      *s = '\0';
    }
    if (setsockopt(fd, IPPROTO_IP, IP_OPTIONS, (OPT_TYPE *)NULL, 0) < 0)
    {
#if defined(DEBUGMODE)
      report_error("setsockopt(IP_OPTIONS) %s: %s", cptr);
#endif
    }
  }
#endif
}

int get_sockerr(aClient *cptr)
{
  int errtmp = errno, err = 0;
  socklen_t len = sizeof(err);
#if defined(SO_ERROR) && !defined(SOL2)
  if (cptr->fd >= 0)
    if (!getsockopt(cptr->fd, SOL_SOCKET, SO_ERROR, (OPT_TYPE *)&err, &len))
      if (err)
        errtmp = err;
#endif
  return errtmp;
}

/*
 * set_non_blocking
 *
 * Set the client connection into non-blocking mode. If your
 * system doesn't support this, you can make this a dummy
 * function (and get all the old problems that plagued the
 * blocking version of IRC--not a problem if you are a
 * lightly loaded node...)
 */
void set_non_blocking(int fd, aClient *cptr)
{
  int res;
#if !defined(NBLOCK_SYSV)
  int nonb = 0;
#endif

  /*
   * NOTE: consult ALL your relevant manual pages *BEFORE* changing
   * these ioctl's. There are quite a few variations on them,
   * as can be seen by the PCS one. They are *NOT* all the same.
   * Heed this well. - Avalon.
   */
#if defined(NBLOCK_POSIX)
  nonb |= O_NONBLOCK;
#endif
#if defined(NBLOCK_BSD)
  nonb |= O_NDELAY;
#endif
#if defined(NBLOCK_SYSV)
  /* This portion of code might also apply to NeXT. -LynX */
  res = 1;

  if (ioctl(fd, FIONBIO, &res) < 0)
  {
#if defined(DEBUGMODE)
    report_error("ioctl(fd,FIONBIO) failed for %s: %s", cptr);
#endif
  }
#else
  if ((res = fcntl(fd, F_GETFL, 0)) == -1)
  {
#if defined(DEBUGMODE)
    report_error("fcntl(fd, F_GETFL) failed for %s: %s", cptr);
#endif
  }
  else if (fcntl(fd, F_SETFL, res | nonb) == -1)
  {
#if defined(DEBUGMODE)
    report_error("fcntl(fd, F_SETL, nonb) failed for %s: %s", cptr);
#endif
  }
#endif
  return;
}

extern unsigned short server_port;

/*
 * Creates a client which has just connected to us on the given fd.
 * The sockhost field is initialized with the ip# of the host.
 * The client is added to the linked list of clients but isnt added to any
 * hash tables yet since it doesn't have a name.
 */
aClient *add_connection(aClient *cptr, int fd, int type)
{
  Link lin;
  aClient *acptr;
  aConfItem *aconf = NULL;
  acptr =
      make_client(NULL,
      (cptr->port == server_port) ? STAT_UNKNOWN_SERVER : STAT_UNKNOWN_USER);

  if (cptr != &me)
    aconf = cptr->confs->value.aconf;
  /*
   * Removed preliminary access check. Full check is performed in
   * m_server and m_user instead. Also connection time out help to
   * get rid of unwanted connections.
   */
  if (type == ADCON_TTY)        /* If descriptor is a tty,
                                   special checking... */
    get_sockhost(acptr, PunteroACadena(cptr->sockhost));
  else
  {
    Reg1 char *s, *t;
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr_in);

    if (getpeername(fd, (struct sockaddr *)&addr, &len) == -1)
    {
      report_error("Failed in connecting to %s: %s", cptr);
      add_con_refuse:
      ircstp->is_ref++;
      acptr->fd = -2;
      free_client(acptr);
      close(fd);
      return NULL;
    }
    /* Don't want to add "Failed in connecting to" here.. */
    if (aconf && IsIllegal(aconf))
      goto add_con_refuse;

 
    /*
     * Copy ascii address to 'sockhost' just in case. Then we
     * have something valid to put into error messages...
     */
    get_sockhost(acptr, inetntoa(addr.sin_addr));

    /* Pasamos de sockaddr_in a irc_in_addr */
    memset(&acptr->ip, 0, sizeof(struct irc_in_addr));
    acptr->ip.in6_16[5] = htons(65535);
    acptr->ip.in6_16[6] = htons(ntohl(addr.sin_addr.s_addr) >> 16);
    acptr->ip.in6_16[7] = htons(ntohl(addr.sin_addr.s_addr) & 65535);
    acptr->port = ntohs(addr.sin_port);

    /*
     * Check that this socket (client) is allowed to accept
     * connections from this IP#.
     */
    for (s = (char *)&cptr->ip, t = (char *)&acptr->ip, len = 4;
        len > 0; len--, s++, t++)
    {
      if (!*s)
        continue;
      if (*s != *t)
        break;
    }

    if (len)
      goto add_con_refuse;


  if (aconf)
    aconf->clients++;
  acptr->fd = fd;
  if (fd > highest_fd)
    highest_fd = fd;
  loc_clients[fd] = acptr;
  acptr->acpt = cptr;
  Count_newunknown(nrof);
  add_client_to_list(acptr);
  set_non_blocking(acptr->fd, acptr);
  set_sock_opts(acptr->fd, acptr);
  CreateClientEvent(acptr);
  
  /*
   * Add this local client to the IPcheck registry.
   * If it is a connection to a user port and if the site has been throttled,
   * reject the user.
   */
#ifdef HISPANO_WEBCHAT
  /* No ponemos throttle */
  IPcheck_local_connect(acptr);
#else
  if (IPcheck_local_connect(acptr) == -1 && IsUserPort(acptr))
  {
    ircstp->is_ref++;
    exit_client(cptr, acptr, &me,
        "Your host is trying to (re)connect too fast -- throttled");
    return NULL;
  }
#endif
  
  
  lin.flags = ASYNC_CLIENT;
  lin.value.cptr = acptr;
#if defined(NODNS)
  if (!strcmp("127.0.0.1", inetntoa(addr.sin_addr)))
  {
    static struct hostent lhe = { "localhost", NULL, 0, 0, NULL };
    acptr->hostp = &lhe;
  }
  else
  {
#endif
    Debug((DEBUG_DNS, "lookup %s", inetntoa(addr.sin_addr)));
    acptr->hostp = gethost_byaddr(&acptr->ip, &lin);
    if (!acptr->hostp)
    {
      SetDNS(acptr);
#ifndef HISPANO_WEBCHAT
      if (IsUserPort(acptr))
      {
        sprintf_irc(sendbuf, IP_LOOKUP_START, me.name);
        write(fd, sendbuf, strlen(sendbuf));
      }
#endif
    }
#ifndef HISPANO_WEBCHAT
    else if (IsUserPort(acptr))
    {
      sprintf_irc(sendbuf, IP_LOOKUP_CACHE, me.name);
      write(fd, sendbuf, strlen(sendbuf));
    }
#endif
    update_nextdnscheck(0);
    //nextdnscheck = 1;
#if defined(NODNS)
  }
#endif
}

#if defined(NODNS)
  if (!DoingAuth(acptr))
    SetAccess(acptr);
#endif

  start_auth(acptr);
  return acptr;
}

#if defined(UNIXPORT)
static void add_unixconnection(aClient *cptr, int fd)
{
  aClient *acptr;
  aConfItem *aconf = NULL;

  acptr = make_client(NULL, STAT_UNKNOWN);

  /*
   * Copy ascii address to 'sockhost' just in case. Then we
   * have something valid to put into error messages...
   */
  SlabStringAllocDup(&(acptr->sockhost), PunteroACadena(me.name), HOSTLEN);
  if (cptr != &me)
    aconf = cptr->confs->value.aconf;
  if (aconf)
  {
    if (IsIllegal(aconf))
    {
      ircstp->is_ref++;
      acptr->fd = -2;
      free_client(acptr);
      close(fd);
      return;
    }
    else
      aconf->clients++;
  }
  acptr->fd = fd;
  if (fd > highest_fd)
    highest_fd = fd;
  loc_clients[fd] = acptr;
  acptr->acpt = cptr;
  SetUnixSock(acptr);
  memcpy(&acptr->ip, &me.ip, sizeof(struct in_addr));

  Count_newunknown(nrof);
  add_client_to_list(acptr);
  set_non_blocking(acptr->fd, acptr);
  set_sock_opts(acptr->fd, acptr);

  CreateClientEvent(acptr);

  SetAccess(acptr);
  return;
}

#endif

/*
 * read_packet
 *
 * Read a 'packet' of data from a connection and process it.  Read in 8k
 * chunks to give a better performance rating (for server connections).
 * Do some tricky stuff for client connections to make sure they don't do
 * any flooding >:-) -avalon
 */
static int read_packet(aClient *cptr, int socket_ready)
{
  size_t dolen = 0;
  int length = 0;
  int done;
  int ping = IsRegistered(cptr) ? get_client_ping(cptr) : CONNECTTIMEOUT;

  if (socket_ready && !(IsUser(cptr) && DBufLength(&cptr->recvQ) > 6090))
  {
    errno = 0;
    length = recv(cptr->fd, readbuf, sizeof(readbuf), 0);

    cptr->lasttime = now;
    UpdateCheckPing(cptr, ping);
    if (cptr->lasttime > cptr->since)
      cptr->since = cptr->lasttime;
    cptr->flags &= ~(FLAGS_PINGSENT | FLAGS_NONL);
    /*
     * If not ready, fake it so it isnt closed
     */
    if (length == -1 && ((errno == EWOULDBLOCK) || (errno == EAGAIN)))
      return 1;
    if (length <= 0)
      return length;
  }

  /*
   * For server connections, we process as many as we can without
   * worrying about the time of day or anything :)
   */
  if (IsServer(cptr) || IsConnecting(cptr) || IsHandshake(cptr))
  {
    if (length > 0)
      if ((done = dopacket(cptr, readbuf, length)))
        return done;
  }
  else
  {
    /*
     * Before we even think of parsing what we just read, stick
     * it on the end of the receive queue and do it when its
     * turn comes around.
     */
    if (!dbuf_put(NULL, &cptr->recvQ, readbuf, length))
      return exit_client(cptr, cptr, &me, "dbuf_put fail");

    if (DoingDNS(cptr) || DoingAuth(cptr))
      return 1;

#if !defined(NOFLOODCONTROL)
    if (IsUser(cptr) && DBufLength(&cptr->recvQ) > CLIENT_FLOOD
#if defined(CS_NO_FLOOD_ESNET)
        && !IsChannelService(cptr)
#endif
        )
      return exit_client(cptr, cptr, &me, "Excess Flood");
#endif

    while (DBufLength(&cptr->recvQ) && (!NoNewLine(cptr))
#if !defined(NOFLOODCONTROL)
#if defined(CS_NO_FLOOD_ESNET)
        && (IsChannelService(cptr) || cptr->since - now < 10)
#else
        && (IsTrusted(cptr) || cptr->since - now < 10)
#endif
#endif
        )
    {
      /*
       * If it has become registered as a Server
       * then skip the per-message parsing below.
       */
      if (IsServer(cptr))
      {
        /*
         * XXX - this blindly deletes data if no cr/lf is received at
         * the end of a lot of messages and the data stored in the
         * dbuf is greater than sizeof(readbuf)
         */
        dolen = dbuf_get(&cptr->recvQ, readbuf, sizeof(readbuf));
        if (0 == dolen)
          break;
        if ((done = dopacket(cptr, readbuf, dolen)))
          return done;
        break;
      }
      dolen = dbuf_getmsg(&cptr->recvQ, cptr->buffer, BUFSIZE);
      /*
       * Devious looking...whats it do ? well..if a client
       * sends a *long* message without any CR or LF, then
       * dbuf_getmsg fails and we pull it out using this
       * loop which just gets the next 512 bytes and then
       * deletes the rest of the buffer contents.
       * -avalon
       */
      if (0 == dolen)
      {
        if (DBufLength(&cptr->recvQ) < 510)
        {
          cptr->flags |= FLAGS_NONL;
          break;
        }
        DBufClear(&cptr->recvQ);
/*        sendto_one(cptr, err_str(ERR_INPUTTOOLONG), me.name, cptr->name); */
        break;
      }
      else if (CPTR_KILLED == client_dopacket(cptr, dolen))
        return CPTR_KILLED;
    }
  }

  if(DBufLength(&cptr->recvQ) && !NoNewLine(cptr)) // Si hay datos pendientes
    UpdateTimer(cptr, 2); // Programo una relectura

  return 1;
}

/*
 * test_listen_port
 *
 * Revisa si se esta escuchando en ese puerto para evitar
 * intentar escuchar dos veces en el mismo puerto
 *
 * Si ya estoy escuchando en el lo marco como configuracion
 * legal para que no se cerrada por close_listeners
 *
 * -- FreeMind 20081206
 */
int test_listen_port(aConfItem *aconf) {
  aClient *acptr;
  int i;

  for (i = highest_fd; i >= 0; i--)
  {
    if (!(acptr = loc_clients[i]))
      continue;

    if (!IsMe(acptr) || acptr == &me || !IsListening(acptr))
      continue;

    if (acptr->confs->value.aconf->port == aconf->port) {
      acptr->confs->value.aconf->status &= ~CONF_ILLEGAL;
      return 1;
    }
  }

  return 0;
}

/*
 * event_async_dns_callback
 *
 * Gestion de evento para resolver dns
 *
 * -- FreeMind 20081214
 */
void event_async_dns_callback(int fd, short event, void *arg)
{
  Debug((DEBUG_DEBUG, "event_async_dns_callback event: %d", (int)event));

  assert(event & EV_READ);

  update_now();

  if (resfd >= 0) // LEO DNS
    do_dns_async();
}

/*
 * event_udp_callback
 *
 * Gestion de evento para paquetes udp
 *
 * -- FreeMind 20081214
 */
void event_udp_callback(int fd, short event, void *arg)
{
  Debug((DEBUG_DEBUG, "event_udp_callback event: %d", (int)event));

  assert(event & EV_READ);

  update_now();

  if (udpfd >= 0) // COMPRUEBO SI HAY UDP PARA LEER
    polludp();
}

/*
 * event_ping_callback
 *
 * Gestion de evento para pings udp
 *
 * -- FreeMind 20081214
 */
void event_ping_callback(int fd, short event, aClient *cptr)
{
  Debug((DEBUG_DEBUG, "event_ping_callback event: %d", (int)event));

  assert(IsPing(cptr));
  assert((event & EV_READ) || (event & EV_TIMEOUT));

  update_now();

  if (!IsPing(cptr))
    return;

  if (event & EV_READ)  // HAY DATOS PENDIENTES DE LEER
  {
    read_ping(cptr);          /* This can MyFree(cptr) ! */
    return;
  }

  cptr->lasttime = now;
  send_ping(cptr);          /* This can MyFree(cptr) ! */
}

/*
 * event_auth_callback
 *
 * Gestion de evento para chequeo ident
 *
 * -- FreeMind 20081214
 */
void event_auth_callback(int fd, short event, aClient *cptr)
{
  Debug((DEBUG_DEBUG, "event_auth_callback event: %d", (int)event));

  assert((event & EV_READ) || (event & EV_WRITE));

  update_now();

  if (cptr->authfd < 0)
    return;

  if (event & EV_WRITE)  // HAY DATOS PENDIENTES DE ESCRIBIR
    send_authports(cptr);
  else if (event & EV_READ)  // HAY DATOS PENDIENTES DE LEER
    read_authports(cptr);
}

void deadsocket(aClient *cptr) {
  if(!IsServer(cptr) && mensaje_quit_personalizado)
    exit_client(cptr, cptr, &me, mensaje_quit_personalizado);
  else
    exit_client(cptr, cptr, &me,
      IsDead(cptr) ? LastDeadComment(cptr) : strerror(get_sockerr(cptr)));
}

/*
 * event_client_read_callback
 *
 * Gestion de evento para lectura de mensajes de clientes (servidores incluidos)
 *
 * -- FreeMind 20081214
 */
void event_client_read_callback(int fd, short event, aClient *cptr) {
  int length=1;

  Debug((DEBUG_DEBUG, "event_client_read_callback event: %d", (int)event));

  assert((event & EV_READ) || (event & EV_TIMEOUT));
  assert(cptr->fd<0 || (cptr->fd == cptr->evread->ev_fd));

#if defined(DEBUGMODE)
  assert(!IsLog(cptr));
#endif

  update_now();

  if(IsDead(cptr)) {
    deadsocket(cptr);
    return;
  }

  if (!IsDead(cptr))
    length = read_packet(cptr, event & EV_READ ? 1 : 0);
  if ((length != CPTR_KILLED) && IsDead(cptr)) { // ERROR LECTURA/ESCRITURA
    deadsocket(cptr);
    return ;
  }

  if (length > 0) // Si hay datos pendientes salgo
    return;

  /*
   * ...hmm, with non-blocking sockets we might get
   * here from quite valid reasons, although.. why
   * would select report "data available" when there
   * wasn't... So, this must be an error anyway...  --msa
   * actually, EOF occurs when read() returns 0 and
   * in due course, select() returns that fd as ready
   * for reading even though it ends up being an EOF. -avalon
   */
  Debug((DEBUG_ERROR, "READ ERROR: fd = %d %d %d", fd, errno, length));

  if (length == CPTR_KILLED)
    return;

  if ((IsServer(cptr) || IsHandshake(cptr)) && errno == 0 && length == 0) // EOF DE SERVIDOR
    exit_client_msg(cptr, cptr, &me, "Server %s closed the connection (%s)",
        PunteroACadena(cptr->name), cptr->serv->last_error_msg);
  else {
    if(mensaje_quit_personalizado)
      exit_client(cptr, cptr, &me, mensaje_quit_personalizado);
    else
      exit_client_msg(cptr, cptr, &me, "Read error: %s", // ERROR DE LECTURA DE CLIENTE
        (length < 0) ? strerror(get_sockerr(cptr)) : "EOF from client");
  }
}

/*
 * event_client_write_callback
 *
 * Gestion de evento para escritura de mensajes de clientes (servidores incluidos)
 *
 * -- FreeMind 20081226
 */

void event_client_write_callback(int fd, short event, aClient *cptr) {
  int write_err = 0;

  Debug((DEBUG_DEBUG, "event_client_write_callback event: %d", (int)event));

  assert(event & EV_WRITE);
  assert(cptr->fd < 0 || (cptr->fd == cptr->evwrite->ev_fd));

#if defined(DEBUGMODE)
  assert(!IsLog(cptr));
#endif

  update_now();

  if(IsDead(cptr)) {
    deadsocket(cptr);
    return;
  }

  /*
   *  ...room for writing, empty some queue then...
   */
  cptr->flags &= ~FLAGS_BLOCKED;
  if (IsConnecting(cptr))
    write_err = completed_connection(cptr);
  if (!write_err)
    {
      if (cptr->listing && DBufLength(&cptr->sendQ) < 2048)
        list_next_channels(cptr);
      send_queued(cptr);
    }
  if (IsDead(cptr) || write_err) // ERROR DE LECTURA/ESCRITURA
    {
      deadsocket(cptr);
      return;
    }
}


/*
 * event_connection_callback
 *
 * Gestion de evento para nuevas conexiones
 *
 * -- FreeMind 20081214
 */
void event_connection_callback(int loc_fd, short event, aClient *cptr)
{
  int fd;

  Debug((DEBUG_DEBUG, "event_connection_callback event: %d", (int)event));

  update_now();

  assert(IsListening(cptr));

  // ENTRA UNA NUEVA CONEXION
  {
    cptr->lasttime = now;
    /*
     * There may be many reasons for error return, but
     * in otherwise correctly working environment the
     * probable cause is running out of file descriptors
     * (EMFILE, ENFILE or others?). The man pages for
     * accept don't seem to list these as possible,
     * although it's obvious that it may happen here.
     * Thus no specific errors are tested at this
     * point, just assume that connections cannot
     * be accepted until some old is closed first.
     */
    if ((fd = accept(loc_fd, NULL, NULL)) < 0)
      {
        if (errno != EWOULDBLOCK)
          {
#if defined(DEBUGMODE)
            report_error("accept() failed%s: %s", NULL);
#endif
          }
        return;
      }
#if defined(USE_SYSLOG) && defined(SYSLOG_CONNECTS)
    {                         /* get an early log of all connections   --dl */
      static struct sockaddr_in peer;
      static int len;
      len = sizeof(peer);
      getpeername(fd, (struct sockaddr *)&peer, &len);
      syslog(LOG_DEBUG, "Conn: %s", inetntoa(peer.sin_addr));
    }
#endif
    ircstp->is_ac++;
    if (fd >= MAXCLIENTS)
      {
        /* Don't send more messages then one every 10 minutes */
        static int count;
        static time_t last_time;
        ircstp->is_ref++;
        ++count;
        if (last_time < now - (time_t) 600)
          {
            if (count > 0)
              {
                if (!last_time)
                  last_time = me.since;
                sendto_ops
                ("All connections in use!  Had to refuse %d clients in the last "
                    STIME_T_FMT " minutes", count, (now - last_time) / 60);
              }
            else
              sendto_ops("All connections in use. (%s)", PunteroACadena(cptr->name));
            count = 0;
            last_time = now;
          }
        send(fd, "ERROR :All connections in use\r\n", 32, 0);
        close(fd);
        return;
      }
    /*
     * Use of add_connection (which never fails :) meLazy
     */
#if defined(UNIXPORT)
    if (IsUnixSocket(cptr))
      add_unixconnection(cptr, fd);
    else
#endif
      if (!add_connection(cptr, fd, ADCON_SOCKET))
        return;
    //nextping = now;
    if (!cptr->acpt)
      cptr->acpt = &me;
  }
}
/*
 * event_checkping_callback
 *
 * Gestion de checkeo de pings
 *
 * -- FreeMind 20081224
 */
void event_checkping_callback(int fd, short event, aClient *cptr)
{
  int ping, rflag=0;

  Debug((DEBUG_DEBUG, "event_checkping_callback event: %d", (int)event));

  assert(event & EV_TIMEOUT);
  assert(!IsMe(cptr));
  assert(!IsLog(cptr));
  assert(!IsPing(cptr));

  update_now();

  /*
   * Note: No need to notify opers here.
   * It's already done when "FLAGS_DEADSOCKET" is set.
   */
  if (IsDead(cptr))
  {
    deadsocket(cptr);
    return;
  }

#if defined(R_LINES) && defined(R_LINES_OFTEN)
  rflag = IsUser(cptr) ? find_restrict(cptr) : 0;
#endif
  ping = IsRegistered(cptr) ? get_client_ping(cptr) : CONNECTTIMEOUT;
  UpdateCheckPing(cptr, ping);
  Debug((DEBUG_DEBUG, "c(%s)=%d p %d r %d a %d",
      PunteroACadena(cptr->name), cptr->status, ping, rflag, (int)(now - cptr->lasttime)));
  /*
   * Ok, so goto's are ugly and can be avoided here but this code
   * is already indented enough so I think its justified. -avalon
   */
  if (!rflag && IsRegistered(cptr) && (ping >= now - cptr->lasttime))
    return;
  /*
   * If the server hasnt talked to us in 2*ping seconds
   * and it has a ping time, then close its connection.
   * If the client is a user and a KILL line was found
   * to be active, close this connection too.
   */
  if (rflag ||
      ((now - cptr->lasttime) >= (2 * ping) &&
          (cptr->flags & FLAGS_PINGSENT)) ||
          (!IsRegistered(cptr) && !IsHandshake(cptr) &&
              (now - cptr->firsttime) >= ping))
    {
      if (!IsRegistered(cptr) && (DoingDNS(cptr) || DoingAuth(cptr)))
        {
          Debug((DEBUG_NOTICE, "%s/%s timeout %s", DoingDNS(cptr) ? "DNS" : "",
              DoingAuth(cptr) ? "AUTH" : "", get_client_name(cptr, FALSE)));
          if (cptr->authfd >= 0)
            {
              close(cptr->authfd);
              cptr->authfd = -1;
              cptr->count = 0;
              *cptr->buffer = '\0';
            }
          del_queries((char *)cptr);
          ClearAuth(cptr);
          ClearDNS(cptr);
          SetAccess(cptr);
          cptr->firsttime = now;
          cptr->lasttime = now;
          return;
        }
      if (IsServer(cptr) || IsConnecting(cptr) || IsHandshake(cptr))
        {
          sendto_ops("No response from %s, closing link", PunteroACadena(cptr->name));
          exit_client(cptr, cptr, &me, "Ping timeout");
          return;
        }
      /*
       * This is used for KILL lines with time restrictions
       * on them - send a message to the user being killed first.
       */
#if defined(R_LINES) && defined(R_LINES_OFTEN)
      else if (IsUser(cptr) && rflag)
        {
          sendto_ops("Restricting %s, closing link.",
              get_client_name(cptr, FALSE));
          exit_client(cptr, cptr, &me, "R-lined");
        }
#endif
      else
        {
          if (!IsRegistered(cptr) && cptr->name && cptr->user->username)
            {
              sendto_one(cptr,
                  ":%s %d %s :Your client may not be compatible with this server.",
                  me.name, ERR_BADPING, cptr->name);
              sendto_one(cptr,
                  ":%s %d %s :Compatible clients are available at "
                  "ftp://ftp.irc.org/irc/clients",
                  me.name, ERR_BADPING, cptr->name);
              
            }

          if(IsRegistered(cptr) && mensaje_quit_personalizado)
            exit_client(cptr, cptr, &me, mensaje_quit_personalizado);
          else
            exit_client(cptr, cptr, &me, "Ping timeout");
        }
      return;
    }
  else if (IsRegistered(cptr) && (cptr->flags & FLAGS_PINGSENT) == 0)
    {
      /*
       * If we havent PINGed the connection and we havent
       * heard from it in a while, PING it to make sure
       * it is still alive.
       */
      cptr->flags |= FLAGS_PINGSENT;
      /* not nice but does the job */
      cptr->lasttime = now - ping;
      if (IsUser(cptr))
        sendto_one(cptr, "PING :%s", me.name);
      else {
#if !defined(NO_PROTOCOL9)
        if (Protocol(cptr) < 10)
          sendto_one(cptr, ":%s PING :%s", me.name, me.name);
        else
#endif
          sendto_one(cptr, "%s " TOK_PING " :%s", NumServ(&me), me.name);
      }
    }
}

/*
 * add_listener
 *
 * Create a new client which is essentially the stub like 'me' to be used
 * for a socket that is passive (listen'ing for connections to be accepted).
 */
int add_listener(aConfItem *aconf)
{
  aClient *cptr;
  
  if(bootopt & BOOT_BDDCHECK)
    return 0;

  cptr = make_client(NULL, STAT_ME);
  cptr->flags = FLAGS_LISTEN;
  cptr->acpt = cptr;
  cptr->from = cptr;
  SlabStringAllocDup(&(cptr->name), aconf->host, 0);
#if defined(UNIXPORT)
  if (*aconf->host == '/')
  {
    if (unixport(cptr, aconf->host, aconf->port))
      cptr->fd = -2;
  }
  else
#endif
  if(test_listen_port(aconf)) { // Si intento a~adir un puerto que ya escucha
    free_client(cptr);
    return 0;
  }

  if (inetport(cptr, aconf->host, aconf->port, aconf->name))
    cptr->fd = -2;

  if (cptr->fd >= 0)
  {
    cptr->confs = make_link();
    cptr->confs->next = NULL;
    cptr->confs->value.aconf = aconf;
    set_non_blocking(cptr->fd, cptr);
  }
  else
    free_client(cptr);
  return 0;
}

/*
 * close_listeners
 *
 * Close and free all clients which are marked as having their socket open
 * and in a state where they can accept connections.  Unix sockets have
 * the path to the socket unlinked for cleanliness.
 */
void close_listeners(void)
{
  Reg1 aClient *cptr;
  Reg2 int i;
  Reg3 aConfItem *aconf;

  /*
   * close all 'extra' listening ports we have and unlink the file
   * name if it was a unix socket.
   */
  for (i = highest_fd; i >= 0; i--)
  {
    if (!(cptr = loc_clients[i]))
      continue;
    if (!IsMe(cptr) || cptr == &me || !IsListening(cptr))
      continue;
    aconf = cptr->confs->value.aconf;

    if (IsIllegal(aconf) && aconf->clients == 0)
    {
#if defined(UNIXPORT)
      if (IsUnixSocket(cptr))
      {
        sprintf_irc(unixpath, "%s/%u", aconf->host, aconf->port);
        unlink(unixpath);
      }
#endif
      if(cptr->evread)
        event_del(cptr->evread);

      close_connection(cptr);
    }
  }
}

/*
 * connect_server
 */
int connect_server(aConfItem *aconf, aClient *by, struct hostent *hp)
{
  Reg1 struct sockaddr *svp;
  Reg2 aClient *cptr, *c2ptr;
  Reg3 char *s;
  int errtmp, len;

  Debug((DEBUG_NOTICE, "Connect to %s[%s] @%s",
      aconf->name, aconf->host, inetntoa(aconf->ipnum)));

  if ((c2ptr = FindClient(aconf->name)))
  {
    if (IsServer(c2ptr) || IsMe(c2ptr))
    {
      sendto_ops("Server %s already present from %s",
          aconf->name, c2ptr->from->name);
      if (by && IsUser(by) && !MyUser(by))
      {
#if !defined(NO_PROTOCOL9)
        if (Protocol(by->from) < 10)
          sendto_one(by, ":%s NOTICE %s :Server %s already present from %s",
              me.name, by->name, aconf->name, c2ptr->from->name);
        else
#endif
          sendto_one(by, "%s " TOK_NOTICE " %s%s :Server %s already present from %s",
              NumServ(&me), NumNick(by), aconf->name, c2ptr->from->name);
      }
      return -1;
    }
    else if (IsHandshake(c2ptr) || IsConnecting(c2ptr))
    {
      if (by && IsUser(by))
      {
        if (MyUser(by) 
#if !defined(NO_PROTOCOL9)
            || Protocol(by->from) < 10
#endif
        )
          sendto_one(by, ":%s NOTICE %s :Connection to %s already in progress",
              me.name, by->name, c2ptr->name);
        else
          sendto_one(by,
              "%s " TOK_NOTICE " %s%s :Connection to %s already in progress",
              NumServ(&me), NumNick(by), c2ptr->name);
      }
      return -1;
    }
  }

  /*
   * If we dont know the IP# for this host and itis a hostname and
   * not a ip# string, then try and find the appropriate host record.
   */
  if ((!aconf->ipnum.s_addr)
#if defined(UNIXPORT)
      && ((aconf->host[2]) != '/')  /* needed for Unix domain -- dl */
#endif
      )
  {
    Link lin;

    lin.flags = ASYNC_CONNECT;
    lin.value.aconf = aconf;
    update_nextdnscheck(0);
    //nextdnscheck = 1;
    s = strchr(aconf->host, '@');
    s++;                        /* should NEVER be NULL */
    if ((aconf->ipnum.s_addr = inet_addr(s)) == INADDR_NONE)
    {
      aconf->ipnum.s_addr = INADDR_ANY;
      hp = gethost_byname(s, &lin);
      Debug((DEBUG_NOTICE, "co_sv: hp %p ac %p na %s ho %s",
          hp, aconf, aconf->name, s));
      if (!hp)
        return 0;
      memcpy(&aconf->ipnum, hp->h_addr, sizeof(struct in_addr));
    }
  }
  cptr = make_client(NULL, STAT_UNKNOWN);
  cptr->hostp = hp;
  /*
   * Copy these in so we have something for error detection.
   */
  SlabStringAllocDup(&(cptr->name), aconf->name, HOSTLEN);
  SlabStringAllocDup(&(cptr->sockhost), aconf->host, HOSTLEN);

#if defined(UNIXPORT)
  if (aconf->host[2] == '/')    /* (/ starts a 2), Unix domain -- dl */
    svp = connect_unix(aconf, cptr, &len);
  else
    svp = connect_inet(aconf, cptr, &len);
#else
  svp = connect_inet(aconf, cptr, &len);
#endif

  if (!svp)
  {
    if (cptr->fd >= 0) {
      close(cptr->fd);
    }
    cptr->fd = -2;
    if (by && IsUser(by) && !MyUser(by))
    {
#if !defined(NO_PROTOCOL9)
      if (Protocol(by->from) < 10)
        sendto_one(by, ":%s NOTICE %s :Couldn't connect to %s",
            me.name, by->name, cptr->name);
      else
#endif
        sendto_one(by, "%s " TOK_NOTICE " %s%s :Couldn't connect to %s",
            NumServ(&me), NumNick(by), cptr->name);
    }
    free_client(cptr);
    return -1;
  }

  set_non_blocking(cptr->fd, cptr);
  set_sock_opts(cptr->fd, cptr);

  if (connect(cptr->fd, svp, len) < 0 && errno != EINPROGRESS)
  {
    int err = get_sockerr(cptr);
    errtmp = errno;             /* other system calls may eat errno */
    report_error("Connect to host %s failed: %s", cptr);
    if (by && IsUser(by) && !MyUser(by))
    {
#if !defined(NO_PROTOCOL9)
      if (Protocol(by->from) < 10)
        sendto_one(by, ":%s NOTICE %s :Connect to host %s failed: %s",
            me.name, by->name, cptr->name, strerror(err));
      else
#endif
        sendto_one(by, "%s " TOK_NOTICE " %s%s :Connect to host %s failed: %s",
            NumServ(&me), NumNick(by), cptr->name, strerror(err));
    }
    close(cptr->fd);
    cptr->fd = -2;
    free_client(cptr);
    errno = errtmp;
    if (errno == EINTR)
      errno = ETIMEDOUT;
    return -1;
  }

  /*
   * Attach config entries to client here rather than in
   * completed_connection. This to avoid null pointer references
   * when name returned by gethostbyaddr matches no C lines
   * (could happen in 2.6.1a when host and servername differ).
   * No need to check access and do gethostbyaddr calls.
   * There must at least be one as we got here C line...  meLazy
   */
  attach_confs_host(cptr, aconf->host, CONF_CONNECT_SERVER);

  if (!find_conf_host(cptr->confs, aconf->host, CONF_CONNECT_SERVER))
  {
    sendto_ops("Host %s is not enabled for connecting: no C-line", aconf->name);
    if (by && IsUser(by) && !MyUser(by))
    {
#if !defined(NO_PROTOCOL9)
      if (Protocol(by->from) < 10)
        sendto_one(by,
            ":%s NOTICE %s :Connect to host %s failed: no C-line",
            me.name, by->name, cptr->name);
      else
#endif
        sendto_one(by,
            "%s " TOK_NOTICE " %s%s :Connect to host %s failed: no C-line",
            NumServ(&me), NumNick(by), cptr->name);
    }
    det_confs_butmask(cptr, 0);
    close(cptr->fd);
    cptr->fd = -2;
    free_client(cptr);
    return (-1);
  }
  /*
   * The socket has been connected or connect is in progress.
   */
  make_server(cptr);
  if (by && IsUser(by))
  {
    char temp_buffer[NUMNICKLEN + 1];
    sprintf_irc(temp_buffer, "%s%s", NumNick(by));
    SlabStringAllocDup(&(cptr->serv->by), temp_buffer, NUMNICKLEN);
    if (cptr->serv->user)
      free_user(cptr->serv->user);
    cptr->serv->user = by->user;
    by->user->refcnt++;
  }
  else
  {
    if (cptr->serv->user)
      free_user(cptr->serv->user);
    cptr->serv->user = NULL;
  }
  cptr->serv->up = &me;
  if (cptr->fd > highest_fd)
    highest_fd = cptr->fd;
  loc_clients[cptr->fd] = cptr;
  cptr->acpt = &me;
  SetConnecting(cptr);

  get_sockhost(cptr, aconf->host);
  Count_newunknown(nrof);
  add_client_to_list(cptr);
  hAddClient(cptr);
  //nextping = now;

  return 0;
}

static struct sockaddr *connect_inet(aConfItem *aconf, aClient *cptr, int *lenp)
{
  static struct sockaddr_in server;
  Reg3 struct hostent *hp;

  /*
   * Might as well get sockhost from here, the connection is attempted
   * with it so if it fails its useless.
   */
  cptr->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (cptr->fd == -1 && errno == EAGAIN)
  {
    sendto_ops("opening stream socket to server %s: No more sockets",
        cptr->name);
    return NULL;
  }
  if (cptr->fd == -1)
  {
    report_error("opening stream socket to server %s: %s", cptr);
    return NULL;
  }
  if (cptr->fd >= MAXCLIENTS)
  {
    sendto_ops("No more connections allowed (%s)", cptr->name);
    return NULL;
  }
  mysk.sin_port = 0;

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  get_sockhost(cptr, aconf->host);

#if defined(VIRTUAL_HOST)
  mysk.sin_addr = vserv.sin_addr;
#endif

  /*
   * Bind to a local IP# (with unknown port - let unix decide) so
   * we have some chance of knowing the IP# that gets used for a host
   * with more than one IP#.
   */
  /* No we don't bind it, not all OS's can handle connecting with
   * an already bound socket, different ip# might occur anyway
   * leading to a freezing select() on this side for some time.
   * I had this on my Linux 1.1.88 --Run
   */
#if defined(VIRTUAL_HOST)
  /*
   * No, we do bind it if we have virtual host support. If we don't
   * explicitly bind it, it will default to IN_ADDR_ANY and we lose
   * due to the other server not allowing our base IP --smg
   */
  if (bind(cptr->fd, (struct sockaddr *)&mysk, sizeof(mysk)) == -1)
  {
    report_error("error binding to local port for %s: %s", cptr);
    return NULL;
  }
#endif

  /*
   * By this point we should know the IP# of the host listed in the
   * conf line, whether as a result of the hostname lookup or the ip#
   * being present instead. If we dont know it, then the connect fails.
   */
  if (IsDigit(*aconf->host) && (aconf->ipnum.s_addr == INADDR_NONE))
    aconf->ipnum.s_addr = inet_addr(aconf->host);
  if (aconf->ipnum.s_addr == INADDR_NONE)
  {
    hp = cptr->hostp;
    if (!hp)
    {
      Debug((DEBUG_FATAL, "%s: unknown host", aconf->host));
      return NULL;
    }
    memcpy(&aconf->ipnum, hp->h_addr, sizeof(struct in_addr));
  }
  memcpy(&server.sin_addr, &aconf->ipnum, sizeof(struct in_addr));
  memcpy(&cptr->ip, &aconf->ipnum, sizeof(struct in_addr));
  server.sin_port = htons(((aconf->port > 0) ? aconf->port : portnum));
  *lenp = sizeof(server);

  CreateClientEvent(cptr);

  return (struct sockaddr *)&server;
}

#if defined(UNIXPORT)
/*
 * connect_unix
 *
 * Build a socket structure for cptr so that it can connet to the unix
 * socket defined by the conf structure aconf.
 */
static struct sockaddr *connect_unix(aConfItem *aconf, aClient *cptr, int *lenp)
{
  static struct sockaddr_un sock;

  cptr->fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (cptr->fd == -1 && errno == EAGAIN)
  {
    sendto_ops("Unix domain connect to host %s failed: No more sockets",
        cptr->name);
    return NULL;
  }
  if (cptr->fd == -1)
  {
    report_error("Unix domain connect to host %s failed: %s", cptr);
    return NULL;
  }
  else if (cptr->fd >= MAXCLIENTS)
  {
    sendto_ops("No more connections allowed (%s)", cptr->name);
    return NULL;
  }

  get_sockhost(cptr, aconf->host);
  /* +2 needed for working Unix domain -- dl */
  strncpy(sock.sun_path, aconf->host + 2, sizeof(sock.sun_path) - 1);
  sock.sun_path[sizeof(sock.sun_path) - 1] = 0;
  sock.sun_family = AF_UNIX;
  *lenp = strlen(sock.sun_path) + 2;

  SetUnixSock(cptr);

  CreateClientEvent(cptr);

  return (struct sockaddr *)&sock;
}

#endif

/*
 * Find the real hostname for the host running the server (or one which
 * matches the server's name) and its primary IP#.  Hostname is stored
 * in the client structure passed as a pointer.
 */
void get_my_name(aClient *cptr)
{
  struct ConfItem *aconf = find_me();
  /*
   * Setup local socket structure to use for binding to.
   */
  memset(&mysk, 0, sizeof(mysk));
  mysk.sin_family = AF_INET;
  mysk.sin_addr.s_addr = INADDR_ANY;

  if (!aconf || BadPtr(aconf->host))
    return;

  SlabStringAllocDup(&(me.name), aconf->host, HOSTLEN);

  if (!BadPtr(aconf->passwd) && 0 != strcmp(aconf->passwd, "*"))
  {
    mysk.sin_addr.s_addr = inet_addr(aconf->passwd);
    if (INADDR_NONE == mysk.sin_addr.s_addr)
      mysk.sin_addr.s_addr = INADDR_ANY;
#if defined(VIRTUAL_HOST)
    memcpy(&vserv, &mysk, sizeof(struct sockaddr_in));
#endif
  }
  Debug((DEBUG_DEBUG, "local name is %s", get_client_name(&me, TRUE)));
}

/*
 * Setup a UDP socket and listen for incoming packets
 */
int setup_ping(void)
{
  struct sockaddr_in from;
  int on = 1;

  memset(&from, 0, sizeof(from));
#if defined(VIRTUAL_HOST)
  from.sin_addr = vserv.sin_addr;
#else
  from.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
  from.sin_port = htons(atoi(UDP_PORT));
  from.sin_family = AF_INET;

  if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    Debug((DEBUG_ERROR, "socket udp : %s", strerror(errno)));
    return -1;
  }
  if (setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR,
      (OPT_TYPE *)&on, sizeof(on)) == -1)
  {
#if defined(USE_SYSLOG)
    syslog(LOG_ERR, "setsockopt udp fd %d : %m", udpfd);
#endif
    Debug((DEBUG_ERROR, "setsockopt so_reuseaddr : %s", strerror(errno)));
    event_del(&evudp);
    close(udpfd);
    udpfd = -1;
    return -1;
  }
  on = 0;
  setsockopt(udpfd, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on));
  if(udpfd>=0) {
    event_set(&evudp, udpfd, EV_READ|EV_PERSIST, (void *)event_udp_callback, NULL);
    if(event_add(&evudp, NULL)==-1)
      Debug((DEBUG_ERROR, "ERROR: event_add EV_READ (event_udp_callback) fd = %d", udpfd));
  }
  if (bind(udpfd, (struct sockaddr *)&from, sizeof(from)) == -1)
  {
#if defined(USE_SYSLOG)
    syslog(LOG_ERR, "bind udp.%d fd %d : %m", from.sin_port, udpfd);
#endif
    Debug((DEBUG_ERROR, "bind : %s", strerror(errno)));
    event_del(&evudp);
    close(udpfd);
    udpfd = -1;
    return -1;
  }
  if (fcntl(udpfd, F_SETFL, FNDELAY) == -1)
  {
    Debug((DEBUG_ERROR, "fcntl fndelay : %s", strerror(errno)));
    event_del(&evudp);
    close(udpfd);
    udpfd = -1;
    return -1;
  }
  return udpfd;
}

/*
 * max # of pings set to 15/sec.
 */
static void polludp(void)
{
  Reg1 char *s;
  struct sockaddr_in from;
  int n;
  socklen_t fromlen = sizeof(from);
  static time_t last = 0;
  static int cnt = 0, mlen = 0;

  /*
   * find max length of data area of packet.
   */
  if (!mlen)
  {
    mlen = sizeof(readbuf) - strlen(me.name) - strlen(version);
    mlen -= 6;
    if (mlen < 0)
      mlen = 0;
  }
  Debug((DEBUG_DEBUG, "udp poll"));

  n = recvfrom(udpfd, readbuf, mlen, 0, (struct sockaddr *)&from, &fromlen);
  if (now == last)
    if (++cnt > 14)
      return;
  cnt = 0;
  last = now;

  if (n == -1)
  {
    if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
      return;
    else
    {
      report_error("udp port recvfrom (%s): %s", &me);
      return;
    }
  }
  ircstp->is_udp++;
  if (n < 19)
    return;

  s = readbuf + n;
  /*
   * attach my name and version for the reply
   */
  *readbuf |= 1;
  strcpy(s, me.name);
  s += strlen(s) + 1;
  strcpy(s, version);
  s += strlen(s);
  sendto(udpfd, readbuf, s - readbuf, 0,
      (struct sockaddr *)&from, sizeof(from));
  return;
}

/*
 * do_dns_async
 *
 * Called when the fd returned from init_resolver() has been selected for
 * reading.
 */
static void do_dns_async(void)
{
  static Link ln;
  aClient *cptr;
  aConfItem *aconf;
  struct hostent *hp;

  ln.flags = ASYNC_NONE;
  hp = get_res((char *)&ln);

  Debug((DEBUG_DNS, "%p = get_res(%d,%p)", hp, ln.flags, ln.value.cptr));

  switch (ln.flags)
  {
    case ASYNC_NONE:
      /*
       * No reply was processed that was outstanding or had a client
       * still waiting.
       */
      break;
    case ASYNC_CLIENT:
      if ((cptr = ln.value.cptr))
      {
        del_queries((char *)cptr);
#ifndef HISPANO_WEBCHAT
        if (IsUserPort(cptr))
        {
          if (hp)
          {
            sprintf_irc(sendbuf, IP_LOOKUP_OK, me.name);
            write(cptr->fd, sendbuf, strlen(sendbuf));
          }
          else
          {
            sprintf_irc(sendbuf, IP_LOOKUP_FAIL, me.name);
            write(cptr->fd, sendbuf, strlen(sendbuf));
          }
        }
#endif
        ClearDNS(cptr);
        if (!DoingAuth(cptr))
          SetAccess(cptr);
        cptr->hostp = hp;
      }
      break;
    case ASYNC_CONNECT:
      aconf = ln.value.aconf;
      if (hp && aconf)
      {
        memcpy(&aconf->ipnum, hp->h_addr, sizeof(struct in_addr));
        connect_server(aconf, NULL, hp);
      }
      else
        sendto_ops("Connect to %s failed: host lookup",
            (aconf) ? aconf->host : "unknown");
      break;
    case ASYNC_PING:
      cptr = ln.value.cptr;
      del_queries((char *)cptr);
      if (hp)
      {
        memcpy(&cptr->ip, hp->h_addr, sizeof(struct in_addr));
        if (ping_server(cptr) == -1)
          end_ping(cptr);
      }
      else
      {
        sendto_ops("Udp ping to %s failed: host lookup",
            PunteroACadena(cptr->sockhost));
        end_ping(cptr);
      }
      break;
    case ASYNC_CONF:
      aconf = ln.value.aconf;
      if (hp && aconf)
        memcpy(&aconf->ipnum, hp->h_addr, sizeof(struct in_addr));
      break;
    default:
      break;
  }
}
