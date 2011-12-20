#if !defined(OPERCMDS_H)
#define OPERCMDS_H

#include "res.h"
#include "pcre.h"

/*=============================================================================
 * General defines
 */

/*-----------------------------------------------------------------------------
 * Macro's
 */

#define GLINE_ACTIVE	  0x01
#define GLINE_IPMASK 	  0x02
#define GLINE_LOCAL	  0x04
#define GLINE_REALNAME    0x08 /* GLINE realname */
#define GLINE_REALNAME_CI 0x10 /* GLINE de realname case insensitive */

/*
 * G-line macros.
 */

#define GlineIsActive(g)	((g)->gflags & GLINE_ACTIVE)
#define GlineIsIpMask(g)	((g)->gflags & GLINE_IPMASK)
#define GlineIsLocal(g)		((g)->gflags & GLINE_LOCAL)
#define GlineIsRealName(g)      ((g)->gflags & GLINE_REALNAME)
#define GlineIsRealNameCI(g)    ((g)->gflags & GLINE_REALNAME_CI)


#define SetActive(g)		((g)->gflags |= GLINE_ACTIVE)
#define ClearActive(g)		((g)->gflags &= ~GLINE_ACTIVE)
#define SetGlineIsIpMask(g)	((g)->gflags |= GLINE_IPMASK)
#define SetGlineIsLocal(g)	((g)->gflags |= GLINE_LOCAL)
#define SetGlineRealName(g)     ((g)->gflags |= GLINE_REALNAME)
#define SetGlineRealNameCI(g)   ((g)->gflags |= GLINE_REALNAME_CI)

/*=============================================================================
 * Structures
 */

struct Gline {
  struct Gline *next;
  char *host;
  char *reason;
  char *name;
  time_t expire;
  time_t lastmod;
  time_t lifetime;
  struct irc_in_addr gl_addr;   /**< IP address (for IP-based G-lines). */
  unsigned char gl_bits;    /**< Usable bits in gl_addr. */
  pcre *re;
  unsigned int gflags;
};

/*=============================================================================
 * Proto types
 */

extern int m_rehash(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_restart(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_die(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_squit(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_stats(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_connect(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_wallops(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_time(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_settime(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_rping(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_rpong(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_trace(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_close(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_gline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#endif /* OPERCMDS_H */
