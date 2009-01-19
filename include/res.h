#if !defined(RES_H)
#define RES_H

#include <netinet/in.h>
#include <netdb.h>
#if defined(HPUX)
#if !defined(h_errno)
extern int h_errno;
#endif
#endif
#include "../libevent/event.h"
#include "list.h"

/*=============================================================================
 * General defines
 */

#if !defined(INADDR_NONE)
#define INADDR_NONE 0xffffffff
#endif

/*=============================================================================
 * Proto types
 */

extern int init_resolver(void);
extern void del_queries(char *cp);
extern void add_local_domain(char *hname, int size);
extern struct hostent *gethost_byname(char *name, Link *lp);
extern struct hostent *gethost_byaddr(struct in_addr *addr, Link *lp);
extern struct hostent *get_res(char *lp);
extern void flush_cache(void);
extern int m_dns(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern size_t cres_mem(aClient *sptr);
extern void event_expire_cache_callback(int fd, short event, struct event *ev);
extern void event_timeout_query_list_callback(int fd, short event, struct event *ev);

#endif /* RES_H */
