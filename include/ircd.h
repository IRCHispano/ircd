#if !defined(IRCD_H)
#define IRCD_H

/*=============================================================================
 * Macro's
 */

#define TStime() (now + TSoffset)
#define BadPtr(x) (!(x) || (*(x) == '\0'))

/* Miscellaneous defines */

#define UDP_PORT	"7007"
#define MINOR_PROTOCOL	"09"
#define MAJOR_PROTOCOL	"10"
#define BASE_VERSION	"u2.10"

/* Flags for bootup options (command line flags) */

#define BOOT_CONSOLE	1
#define BOOT_QUICK	2
#define BOOT_DEBUG	4
#define BOOT_INETD	8
#define BOOT_TTY	16
#define BOOT_AUTODIE	32

#define AR_TTL          600           /* TTL in seconds for dns cache entries */

/*=============================================================================
 * Proto types
 */

extern RETSIGTYPE s_die(HANDLER_ARG(int sig));
extern RETSIGTYPE s_restart(HANDLER_ARG(int sig));

extern void restart(char *mesg);
extern void server_reboot(void);
extern void update_nextdnscheck(int timeout);
extern void update_nextconnect(int timeout);
extern void update_nextexpire(int timeout);

extern aClient me;
extern aClient his;
extern time_t now;
extern aClient *client;
extern time_t TSoffset;
extern unsigned int bootopt;
extern time_t nextdnscheck;
extern time_t nextconnect;
extern int dorehash;
extern time_t nextping;
extern unsigned short int portnum;
extern char *configfile;
extern int debuglevel;
extern char *debugmode;
extern int nicklen;

extern struct timeval tm_nextdnscheck;
extern struct timeval tm_nextexpire;

#endif /* IRCD_H */
