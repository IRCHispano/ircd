#if !defined(LIST_H)
#define LIST_H

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
    aClient *cptr;
    struct Channel *chptr;
    struct ConfItem *aconf;
#if defined(WATCH)
    aWatch *wptr;
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

struct DSlink {
  struct DSlink *next;
  struct DSlink *prev;
  union {
    aClient *cptr;
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
extern Link *make_link(void);
extern Link *find_user_link(Link *lp, aClient *ptr);
extern void initlists(void);
extern void outofmemory(void);
extern aClient *make_client(aClient *from, int status);
extern void free_client(aClient *cptr);
extern struct User *make_user(aClient *cptr);
extern struct Server *make_server(aClient *cptr);
extern void free_user(struct User *user);
extern void remove_client_from_list(aClient *cptr);
extern void add_client_to_list(aClient *cptr);
extern Dlink *add_dlink(Dlink **lpp, aClient *cp);
extern void remove_dlink(Dlink **lpp, Dlink *lp);
extern struct ConfItem *make_conf(void);
extern void delist_conf(struct ConfItem *aconf);
extern void free_conf(struct ConfItem *aconf);
extern aGline *make_gline(int is_ipmask, char *host, char *reason, char *name,
    time_t expire);
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
