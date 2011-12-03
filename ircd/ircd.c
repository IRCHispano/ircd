/*
 * IRC - Internet Relay Chat, ircd/ircd.c
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
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#include <sys/stat.h>
#include <pwd.h>
#include <signal.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if defined(HPUX)
#define _KERNEL
#endif
#include <sys/resource.h>
#if defined(HPUX)
#undef _KERNEL
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#if defined(USE_SYSLOG)
#include <syslog.h>
#endif
#if defined(CHROOTDIR)
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#endif
#if defined(VIRTUAL_HOST)
#include <sys/socket.h>         /* Needed for AF_INET on some OS */
#endif

#include <assert.h>

#include "h.h"
#include "s_debug.h"
#include "res.h"
#include "struct.h"
#include "s_serv.h"
#include "send.h"
#include "ircd.h"
#include "s_conf.h"
#include "class.h"
#include "s_misc.h"
#include "parse.h"
#include "match.h"
#include "s_bsd.h"
#include "crule.h"
#include "userload.h"
#include "numeric.h"
#include "hash.h"
#include "bsd.h"
#include "version.h"
#include "whowas.h"
#include "numnicks.h"
#include "IPcheck.h"
#include "s_bdd.h"
#include "slab_alloc.h"
#include "network.h"
#include "msg.h"
#include "random.h"

RCSTAG_CC("$Id$");

extern void init_counters(void);

aClient me;                     /* That's me */
aClient his;			/* me con ocultacion */
aClient *client = &me;          /* Pointer to beginning of Client list */
time_t TSoffset = 0;            /* Global variable; Offset of timestamps to
                                   system clock */

char **myargv;
unsigned short int portnum = 0; /* Server port number, listening this */
char *configfile = CPATH;       /* Server configuration file */
int debuglevel = -1;            /* Server debug level */
unsigned int bootopt = 0;       /* Server boot option flags */
char *debugmode = "";           /*  -"-    -"-   -"-  */
int dorehash = 0;
int restartFlag = 0;
static char *dpath = DPATH;
int nicklen = 30;

struct event   ev_nextconnect;
struct event   ev_nextdnscheck;
struct event   ev_nextexpire;
struct timeval tm_nextconnect;
struct timeval tm_nextdnscheck;
struct timeval tm_nextexpire;

time_t now;                     /* Updated every time we leave select(),

                                   and used everywhere else */

struct event ev_sighup;
struct event ev_sigterm;
struct event ev_sigint;


RETSIGTYPE s_die(HANDLER_ARG(int UNUSED(sig)))
{
#if defined(USE_SYSLOG)
  syslog(LOG_CRIT, "Server Killed By SIGTERM");
#endif
  flush_connections(me.fd);
  exit(-1);
}

RETSIGTYPE s_die2(HANDLER_ARG(int UNUSED(sig)))
{
#if defined(BDD_MMAP)
  db_persistent_commit();
#endif

#if defined(__cplusplus)
  s_die(0);
#else
  s_die();
#endif
}

static RETSIGTYPE s_rehash(HANDLER_ARG(int UNUSED(sig)))
{
  dorehash = 1;
  event_loopbreak();
}

#if defined(USE_SYSLOG)
void restart(char *mesg)
#else
void restart(char *UNUSED(mesg))
#endif
{
#if defined(USE_SYSLOG)
  syslog(LOG_WARNING, "Restarting Server because: %s", mesg);
#endif
  server_reboot();
}

RETSIGTYPE s_restart(HANDLER_ARG(int UNUSED(sig)))
{
  restartFlag = 1;
  event_loopbreak();
}

void server_reboot(void)
{
  Reg1 int i;

  sendto_ops("Aieeeee!!!  Restarting server...");
  Debug((DEBUG_NOTICE, "Restarting server..."));
  flush_connections(me.fd);

#if defined(BDD_MMAP)
  db_persistent_commit();
#endif

  /*
   * fd 0 must be 'preserved' if either the -d or -i options have
   * been passed to us before restarting.
   */
#if defined(USE_SYSLOG)
  closelog();
#endif
  for (i = 3; i < MAXCONNECTIONS; i++)
    close(i);
  if (!(bootopt & (BOOT_TTY | BOOT_DEBUG)))
    close(2);
  close(1);
  if ((bootopt & BOOT_CONSOLE) || isatty(0))
    close(0);

  if (!(bootopt & BOOT_INETD))
    execv(SPATH, myargv);
#if defined(USE_SYSLOG)
  /* Have to reopen since it has been closed above */

  openlog(myargv[0], LOG_PID | LOG_NDELAY, LOG_FACILITY);
  syslog(LOG_CRIT, "execv(%s,%s) failed: %m\n", SPATH, myargv[0]);
  closelog();
#endif
  Debug((DEBUG_FATAL, "Couldn't restart server \"%s\": %s",
      SPATH, strerror(errno)));
  exit(-1);
}

/*
 * try_connections
 *
 * Scan through configuration and try new connections.
 *
 * Returns the calendar time when the next call to this
 * function should be made latest. (No harm done if this
 * is called earlier or later...)
 */
void event_try_connections_callback(int fd, short event, struct event *ev)
{
  Reg1 aConfItem *aconf;
  Reg2 aClient *cptr;
  aConfItem **pconf;
  int connecting, confrq;
  time_t next = 0;
  aConfClass *cltmp;
  aConfItem *cconf, *con_conf = NULL;
  unsigned int con_class = 0;
  
  Debug((DEBUG_DEBUG, "event_try_connections_callback event: %d", (int)event));

  assert(event & EV_TIMEOUT);
  
  update_now();

  connecting = FALSE;
  Debug((DEBUG_NOTICE, "Connection check at   : %s", myctime(now)));
  for (aconf = conf; aconf; aconf = aconf->next)
  {
    /* Also when already connecting! (update holdtimes) --SRB */
    if (!(aconf->status & CONF_CONNECT_SERVER) || aconf->port == 0)
      continue;
    cltmp = aconf->confClass;
    /*
     * Skip this entry if the use of it is still on hold until
     * future. Otherwise handle this entry (and set it on hold
     * until next time). Will reset only hold times, if already
     * made one successfull connection... [this algorithm is
     * a bit fuzzy... -- msa >;) ]
     */

    if ((aconf->hold > now))
    {
      if ((next > aconf->hold) || (next == 0))
        next = aconf->hold;
      continue;
    }

    confrq = get_con_freq(cltmp);
    aconf->hold = now + confrq;
    /*
     * Found a CONNECT config with port specified, scan clients
     * and see if this server is already connected?
     */
    cptr = FindServer(aconf->name);

    if (!cptr && (Links(cltmp) < MaxLinks(cltmp)) &&
        (!connecting || (ConClass(cltmp) > con_class)))
    {
      /* Check connect rules to see if we're allowed to try */
      for (cconf = conf; cconf; cconf = cconf->next)
        if ((cconf->status & CONF_CRULE) &&
            (match(cconf->host, aconf->name) == 0))
          if (crule_eval(cconf->passwd))
            break;
      if (!cconf)
      {
        con_class = ConClass(cltmp);
        con_conf = aconf;
        /* We connect only one at time... */
        connecting = TRUE;
      }
    }
    if ((next > aconf->hold) || (next == 0))
      next = aconf->hold;
  }
  if (connecting)
  {
    if (con_conf->next)         /* are we already last? */
    {
      /* Put the current one at the end and make sure we try all connections */
      for (pconf = &conf; (aconf = *pconf); pconf = &(aconf->next))
        if (aconf == con_conf)
          *pconf = aconf->next;
      (*pconf = con_conf)->next = 0;
    }
    if (connect_server(con_conf, (aClient *)NULL, (struct hostent *)NULL) == 0)
      sendto_ops("Connection to %s activated.", con_conf->name);
  }
  Debug((DEBUG_NOTICE, "Next connection check : %s", myctime(next)));
  
  update_nextconnect((next > now) ? (next - now) : (AR_TTL));
}

/*
 * bad_command
 *
 * This is called when the commandline is not acceptable.
 * Give error message and exit without starting anything.
 */
static int bad_command(void)
{
  printf("Usage: ircd %s[-h servername] [-p portnumber] [-x loglevel] [-t] [-b]\n",
#if defined(CMDLINE_CONFIG)
      "[-f config] "
#else
      ""
#endif
      );
  printf("Server not started\n\n");
  return (-1);
}

static void sigalrm_handler(int sig)
{
  // NO OP
}

static void setup_signals(void)
{
  struct sigaction act;

  act.sa_handler = SIG_IGN;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGPIPE);
  sigaddset(&act.sa_mask, SIGALRM);
#ifdef  SIGWINCH
  sigaddset(&act.sa_mask, SIGWINCH);
  sigaction(SIGWINCH, &act, 0);
#endif
  sigaction(SIGPIPE, &act, 0);

  act.sa_handler = sigalrm_handler;
  sigaction(SIGALRM, &act, 0);
  
  signal_set(&ev_sighup,  SIGHUP,  (void *)s_rehash,  NULL);
  signal_set(&ev_sigterm, SIGTERM, (void *)s_die2,    NULL);
  signal_set(&ev_sigint,  SIGINT,  (void *)s_restart, NULL);

  assert(signal_add(&ev_sighup,  NULL)!=-1);
  assert(signal_add(&ev_sigterm, NULL)!=-1);
  assert(signal_add(&ev_sigint,  NULL)!=-1);
}

/*
 * open_debugfile
 *
 * If the -t option is not given on the command line when the server is
 * started, all debugging output is sent to the file set by LPATH in config.h
 * Here we just open that file and make sure it is opened to fd 2 so that
 * any fprintf's to stderr also goto the logfile.  If the debuglevel is not
 * set from the command line by -x, use /dev/null as the dummy logfile as long
 * as DEBUGMODE has been defined, else dont waste the fd.
 */
static void open_debugfile(void)
{
#if defined(DEBUGMODE)
  int fd;
  aClient *cptr;

  if (debuglevel >= 0)
  {
    cptr = make_client(NULL, STAT_LOG);
    cptr->fd = 2;
    cptr->port = debuglevel;
    cptr->flags = 0;
    cptr->acpt = cptr;
    loc_clients[2] = cptr;

    if (NULL != me.name)
      SlabStringAllocDup(&(cptr->sockhost), me.name, HOSTLEN);

    printf("isatty = %d ttyname = %#x\n", isatty(2), (unsigned int)ttyname(2));
    if (!(bootopt & BOOT_TTY))  /* leave debugging output on fd 2 */
    {
      if ((fd = creat(LOGFILE, 0600)) < 0)
        if ((fd = open("/dev/null", O_WRONLY)) < 0)
          exit(-1);
      if (fd != 2)
      {
        dup2(fd, 2);
        close(fd);
      }
      SlabStringAllocDup(&(cptr->name), LOGFILE, 0);
    }
    else if (isatty(2) && ttyname(2))
    {
      SlabStringAllocDup(&(cptr->name), ttyname(2), 0);
    }
    else
    {
      SlabStringAllocDup(&(cptr->name), "FD2-Pipe", 0);
    }
    Debug((DEBUG_FATAL, "Debug: File <%s> Level: %u at %s",
        cptr->name, cptr->port, myctime(now)));
  }
  else
    loc_clients[2] = NULL;
#endif
  return;
}

void update_nextdnscheck(int timeout) {
  assert(timeout<now);
  event_del(&ev_nextdnscheck);
  evutil_timerclear(&tm_nextdnscheck);
  tm_nextdnscheck.tv_usec=0;
  tm_nextdnscheck.tv_sec=timeout;
  Debug((DEBUG_DEBUG, "update_nextdnscheck timeout: %d", timeout));
  assert(evtimer_add(&ev_nextdnscheck, &tm_nextdnscheck)!=-1);
}

void update_nextconnect(int timeout) {
  assert(timeout<now);
  event_del(&ev_nextconnect);
  evutil_timerclear(&tm_nextconnect);
  tm_nextconnect.tv_usec=0;
  tm_nextconnect.tv_sec=timeout;
  Debug((DEBUG_DEBUG, "update_nextconnect timeout: %d", timeout));
  assert(evtimer_add(&ev_nextconnect, &tm_nextconnect)!=-1);  
}

void update_nextexpire(int timeout) {
  assert(timeout<now);
  event_del(&ev_nextexpire);
  evutil_timerclear(&tm_nextexpire);
  tm_nextexpire.tv_usec=0;
  tm_nextexpire.tv_sec=timeout;
  Debug((DEBUG_DEBUG, "update_nextexpire timeout: %d", timeout));
  assert(evtimer_add(&ev_nextexpire, &tm_nextexpire)!=-1);
}

void init_timers(void)
{
  event_del(&ev_nextconnect);
  event_del(&ev_nextdnscheck);
  event_del(&ev_nextexpire);
  evtimer_set(&ev_nextconnect,  (void *)event_try_connections_callback, (void *)&ev_nextconnect);
  evtimer_set(&ev_nextdnscheck, (void *)event_timeout_query_list_callback, (void *)&ev_nextdnscheck);
  evtimer_set(&ev_nextexpire,   (void *)event_expire_cache_callback, (void *)&ev_nextexpire);
  update_nextdnscheck(0);
  update_nextconnect(0);
  update_nextexpire(0);
}

int main(int argc, char *argv[])
{
  unsigned short int portarg = 0;
  uid_t uid;
  uid_t euid;
#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_CORE)
  struct rlimit corelim;
#endif

  uid = getuid();
  euid = geteuid();
  now = time(NULL);
  autoseed();
  
#if defined(CHROOTDIR)
  if (chdir(DPATH))
  {
    fprintf(stderr, "Fail: Cannot chdir(%s): %s\n", DPATH, strerror(errno));
    exit(-1);
  }
  res_init();
  if (chroot(DPATH))
  {
    fprintf(stderr, "Fail: Cannot chroot(%s): %s\n", DPATH, strerror(errno));
    exit(5);
  }
  dpath = "/";
#endif /*CHROOTDIR */

  myargv = argv;
  umask(077);                   /* better safe than sorry --SRB */
  memset(&me, 0, sizeof(me));
#if defined(VIRTUAL_HOST)
  memset(&vserv, 0, sizeof(vserv));
#endif

  initload();

#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_CORE)
  if (getrlimit(RLIMIT_CORE, &corelim))
  {
    fprintf(stderr, "Read of rlimit core size failed: %s\n", strerror(errno));
    corelim.rlim_max = RLIM_INFINITY; /* Try to recover */
  }
  corelim.rlim_cur = corelim.rlim_max;
  if (setrlimit(RLIMIT_CORE, &corelim))
    fprintf(stderr, "Setting rlimit core size failed: %s\n", strerror(errno));
#endif

  /*
   * All command line parameters have the syntax "-fstring"
   * or "-f string" (e.g. the space is optional). String may
   * be empty. Flag characters cannot be concatenated (like
   * "-fxyz"), it would conflict with the form "-fstring".
   */
  while (--argc > 0 && (*++argv)[0] == '-')
  {
    char *p = argv[0] + 1;
    int flag = *p++;

    if (flag == '\0' || *p == '\0')
    {
      if (argc > 1 && argv[1][0] != '-')
      {
        p = *++argv;
        argc -= 1;
      }
      else
        p = "";
    }

    switch (flag)
    {
      case 'a':
        bootopt |= BOOT_AUTODIE;
        break;
      case 'b':
        bootopt |= (BOOT_BDDCHECK | BOOT_TTY);
        break;        
      case 'c':
        bootopt |= BOOT_CONSOLE;
        break;
      case 'q':
        bootopt |= BOOT_QUICK;
        break;
      case 'd':
        if (euid != uid)
          setuid((uid_t) uid);
        dpath = p;
        break;
#if defined(CMDLINE_CONFIG)
      case 'f':
        if (euid != uid)
          setuid((uid_t) uid);
        configfile = p;
        break;
#endif
      case 'h':
        /* El nombre propio es un valor que no cambiara a lo largo de la
         * ejecucion del programa, por lo que le damos un valor fijo --RyDeN
         */
        SlabStringAllocDup(&(me.name), p, HOSTLEN);
        break;
      case 'i':
        bootopt |= BOOT_INETD | BOOT_AUTODIE;
        break;
      case 'p':
        if ((portarg = atoi(p)) > 0)
          portnum = portarg;
        break;
      case 't':
        if (euid != uid)
          setuid((uid_t) uid);
        bootopt |= BOOT_TTY;
        break;
      case 'v':
        printf("ircd %s\n", version);
        exit(0);
#if defined(VIRTUAL_HOST)
      case 'w':
      {
        struct hostent *hep;
        if (!(hep = gethostbyname(p)))
        {
          fprintf(stderr, "%s: Error creating virtual host \"%s\": %d",
              argv[0], p, h_errno);
          return -1;
        }
        if (hep->h_addrtype == AF_INET && hep->h_addr_list[0] &&
            !hep->h_addr_list[1])
        {
          memcpy(&vserv.sin_addr, hep->h_addr_list[0], sizeof(struct in_addr));
          vserv.sin_family = AF_INET;
        }
        else
        {
          fprintf(stderr, "%s: Error creating virtual host \"%s\": "
              "Use -w <IP-number of interface>\n", argv[0], p);
          return -1;
        }
        break;
      }
#endif
      case 'x':
#if defined(DEBUGMODE)
        if (euid != uid)
          setuid((uid_t) uid);
        debuglevel = atoi(p);
        debugmode = *p ? p : "0";
        bootopt |= BOOT_DEBUG;
        break;
#else
        fprintf(stderr, "%s: DEBUGMODE must be defined for -x y\n", myargv[0]);
        exit(0);
#endif
      default:
        bad_command();
        break;
    }
  }

  if (chdir(dpath))
  {
    fprintf(stderr, "Fail: Cannot chdir(%s): %s\n", dpath, strerror(errno));
    exit(-1);
  }

#if !defined(IRC_UID)
  if ((uid != euid) && !euid)
  {
    fprintf(stderr,
        "ERROR: do not run ircd setuid root. Make it setuid a normal user.\n");
    exit(-1);
  }
#endif

#if !defined(CHROOTDIR) || (defined(IRC_UID) && defined(IRC_GID))
#if !defined(_AIX)
  if (euid != uid)
  {
    setuid((uid_t) uid);
    setuid((uid_t) euid);
  }
#endif

  if ((int)getuid() == 0)
  {
#if defined(IRC_UID) && defined(IRC_GID)

    /* run as a specified user */
    fprintf(stderr, "WARNING: running ircd with uid = %d\n", IRC_UID);
    fprintf(stderr, "	      changing to gid %d.\n", IRC_GID);
    setgid(IRC_GID);
    setuid(IRC_UID);
#else
    /* check for setuid root as usual */
    fprintf(stderr,
        "ERROR: do not run ircd setuid root. Make it setuid a normal user.\n");
    exit(-1);
#endif
  }
#endif /*CHROOTDIR/UID/GID */

  if (argc > 0)
    return bad_command();       /* This should exit out */

#if HAVE_UNISTD_H
  /* Sanity checks */
  {
    char c;
    char *path;

    c = 'S';
    path = SPATH;
    if (access(path, X_OK) == 0)
    {
      c = 'C';
      path = CPATH;
      if (access(path, R_OK) == 0)
      {
        c = 'M';
        path = MPATH;
        if (access(path, R_OK) == 0)
        {
          c = 'R';
          path = RPATH;
          if (access(path, R_OK) == 0)
          {
#if !defined(DEBUG)
            c = 0;
#else
            c = 'L';
            path = LPATH;
            if (access(path, W_OK) == 0)
              c = 0;
#endif
          }
        }
      }
    }
    if (c)
    {
      fprintf(stderr, "Check on %cPATH (%s) failed: %s\n",
          c, path, strerror(errno));
      fprintf(stderr,
          "Please create file and/or rerun `make config' and recompile to correct this.\n");
#if defined(CHROOTDIR)
      fprintf(stderr,
          "Keep in mind that all paths are relative to CHROOTDIR.\n");
#endif
      exit(-1);
    }
  }
#endif

  
  hash_init();
#if defined(DEBUGMODE)
  initlists();
#endif
  initclass();
  initwhowas();
  initmsgtree();
  initstats();
  open_debugfile();
  if (portnum == 0)
    portnum = PORTNUM;
  me.port = portnum;
  init_sys();
  setup_signals();
  
  me.flags = FLAGS_LISTEN;
  if ((bootopt & BOOT_INETD))
  {
    me.fd = 0;
    loc_clients[0] = &me;
    me.flags = FLAGS_LISTEN;
  }
  else
    me.fd = -1;

#if defined(USE_SYSLOG)
  openlog(myargv[0], LOG_PID | LOG_NDELAY, LOG_FACILITY);
#endif
  if (initconf(bootopt) == -1)
  {
    Debug((DEBUG_FATAL, "Failed in reading configuration file %s", configfile));
    printf("Couldn't open configuration file %s\n", configfile);
    exit(-1);
  }
  
  if(!(bootopt & BOOT_BDDCHECK))
  {
    if (!(bootopt & BOOT_INETD))
    {
      static char star[] = "*";
      aConfItem *aconf;

      if ((aconf = find_me()) && portarg == 0 && aconf->port != 0)
        portnum = aconf->port;
      Debug((DEBUG_ERROR, "Port = %u", portnum));
      if (inetport(&me, star, portnum, aconf->passwd))
        exit(1);
    }
    else if (inetport(&me, "*", 0, NULL))
      exit(1);
  }

  read_tlines();
  rmotd = read_motd(RPATH);
  motd = read_motd(MPATH);
  setup_ping();
  get_my_name(&me);

  now = time(NULL);
  me.hopcount = 0;
  me.authfd = -1;
  me.confs = NULL;
  me.next = NULL;
  me.user = NULL;
  me.from = &me;
  SetMe(&me);
#if defined(WEBCHAT)
  SetWebXXXClient(&me);
#endif
  make_server(&me);
  /* Abuse own link timestamp as start timestamp: */
  me.serv->timestamp = TStime();
  me.serv->prot = atoi(MAJOR_PROTOCOL);
  me.serv->up = &me;
  me.serv->down = NULL;

  SetYXXCapacity(&me, MAXCLIENTS);

  me.lasttime = me.since = me.firsttime = now;
  hAddClient(&me);

/* Ocultacion */

  SlabStringAllocDup(&(his.name), SERVER_NAME, HOSTLEN);
  SlabStringAllocDup(&(his.info), SERVER_INFO, REALLEN);

  check_class();
  write_pidfile();

  init_counters();
  
  init_timers();

  if(bootopt & BOOT_BDDCHECK)
  {
    initdb();
    exit(0);
  }
  
  Debug((DEBUG_NOTICE, "Server ready..."));
#if defined(USE_SYSLOG)
  syslog(LOG_NOTICE, "Server Ready");
#endif
  initdb();
  
  

  for (;;)
  {
    event_dispatch();
    update_now();
    
    assert(dorehash || restartFlag);

    Debug((DEBUG_DEBUG, "Got message(s)"));

    if (dorehash)
    {
      rehash(&me, 1);
      dorehash = 0;
    }
    if (restartFlag)
      server_reboot();
  }
}
