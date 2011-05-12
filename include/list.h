#if !defined(LIST_H)
#define LIST_H

#include "h.h"

struct Client;
struct Connection;


/*=============================================================================
 * General defines
 */

/*=============================================================================
 * Macro's
 */

/* ============================================================================
 * Structures
 */

struct SLink {
  struct SLink *next;
  union {
    struct Client *cptr;
    struct Channel *chptr;
    struct ConfItem *aconf;
#if defined(WATCH)
    struct Watch *wptr;
#endif
    char *cp;
    struct {
      char *banstr;
      char *who;
      time_t when;
    } ban;
  } value;
  unsigned int flags;
};

struct DLink {
  struct DLink *next;
  struct DLink *prev;
  union {
    struct Client *cptr;
    struct Channel *chptr;
    struct ConfItem *aconf;
    char *cp;
  } value;
};

#if defined(WATCH)
struct Watch {
  struct Watch *next;
  struct SLink *watch;          /* Cadena de punteros a lista aUser */
  char *nick;                   /* Nick */
  time_t lasttime;              /* TS de ultimo cambio de estado del nick */
};

#endif /* WATCH */

/*=============================================================================
 * Proto types
 */

extern void free_link(Link *lp);
extern struct SLink *make_link(void);
extern struct SLink *find_user_link(struct SLink *lp, aClient *ptr);
extern void init_list(int maxconn);
extern void initlists(void);
extern void outofmemory(void);
extern struct Client *make_client(struct Client *from, int status);
extern void free_connection(struct Connection *con);
extern void free_client(struct Client *cptr);
extern struct Server *make_server(struct Client *cptr);
extern void remove_client_from_list(struct Client *cptr);
extern void add_client_to_list(struct Client *cptr);
extern struct DLink *add_dlink(struct DLink **lpp, struct Client *cp);
extern void remove_dlink(struct DLink **lpp, struct DLink *lp);
extern struct ConfItem *make_conf(void);
extern void delist_conf(struct ConfItem *aconf);
extern void free_conf(struct ConfItem *aconf);
extern aGline *make_gline(int is_ipmask, char *host, char *reason, char *name,
    time_t expire, time_t lastmod, time_t lifetime);
extern aGline *find_gline(aClient *cptr, aGline **pgline);
extern void free_gline(aGline *agline, aGline *pgline);
extern void send_listinfo(aClient *cptr, char *name);
#if defined(BADCHAN)
extern int bad_channel(char *name);
#endif
#if defined(WATCH)
extern aWatch *make_watch(char *nick);
extern void free_watch(aWatch * wptr);
#endif /* WATCH */

#endif /* LIST_H */
