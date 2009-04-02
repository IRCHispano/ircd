#if !defined(OPERCMDS_H)
#define OPERCMDS_H

/*=============================================================================
 * General defines
 */

/*-----------------------------------------------------------------------------
 * Macro's
 */

#define GLINE_ACTIVE	1
#define GLINE_IPMASK	2
#define GLINE_LOCAL	4
#define GLINE_REALNAME  8

/*
 * G-line macros.
 */

#define GlineIsActive(g)	((g)->gflags & GLINE_ACTIVE)
#define GlineIsIpMask(g)	((g)->gflags & GLINE_IPMASK)
#define GlineIsLocal(g)		((g)->gflags & GLINE_LOCAL)
#define GlineIsRealName(g)      ((g)->gflags & GLINE_REALNAME)

#define SetActive(g)		((g)->gflags |= GLINE_ACTIVE)
#define ClearActive(g)		((g)->gflags &= ~GLINE_ACTIVE)
#define SetGlineIsIpMask(g)	((g)->gflags |= GLINE_IPMASK)
#define SetGlineIsLocal(g)	((g)->gflags |= GLINE_LOCAL)
#define SetGlineRealName(g)     ((g)->gflags |= GLINE_REALNAME)



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
