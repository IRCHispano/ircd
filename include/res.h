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

/** Structure to store an IP address. */
struct irc_in_addr
{
  unsigned short in6_16[8]; /**< IPv6 encoded parts, little-endian. */
};

/** Structure to store an IP address and port number. */
struct irc_sockaddr
{
  struct irc_in_addr addr; /**< IP address. */
  unsigned short port;     /**< Port number, host-endian. */
};

/** Evaluate to non-zero if \a ADDR is an unspecified (all zeros) address. */
#define irc_in_addr_unspec(ADDR) (((ADDR)->in6_16[0] == 0) \
                                  && ((ADDR)->in6_16[1] == 0) \
                                  && ((ADDR)->in6_16[2] == 0) \
                                  && ((ADDR)->in6_16[3] == 0) \
                                  && ((ADDR)->in6_16[4] == 0) \
                                  && ((ADDR)->in6_16[6] == 0) \
                                  && ((ADDR)->in6_16[7] == 0) \
                                  && ((ADDR)->in6_16[5] == 0 \
                                      || (ADDR)->in6_16[5] == 65535))

/** Evaluate to non-zero if \a ADDR is a valid address (not all 0s and not all 1s). */
#define irc_in_addr_valid(ADDR) (((ADDR)->in6_16[0] && ((ADDR)->in6_16[0] != 65535)) \
                                 || (ADDR)->in6_16[1] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[2] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[3] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[4] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[5] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[6] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[7] != (ADDR)->in6_16[0])

/** Evaluate to non-zero if \a ADDR (of type struct irc_in_addr) is an IPv4 address. */
#define irc_in_addr_is_ipv4(ADDR) (!(ADDR)->in6_16[0] && !(ADDR)->in6_16[1] && !(ADDR)->in6_16[2] \
                                   && !(ADDR)->in6_16[3] && !(ADDR)->in6_16[4] \
                                   && ((!(ADDR)->in6_16[5] && (ADDR)->in6_16[6]) \
                                       || (ADDR)->in6_16[5] == 65535))

/** Evaluate to non-zero if \a A is a different IP than \a B. */
#define irc_in_addr_cmp(A,B) (irc_in_addr_is_ipv4(A) ? ((A)->in6_16[6] != (B)->in6_16[6] \
                                  || (A)->in6_16[7] != (B)->in6_16[7] || !irc_in_addr_is_ipv4(B)) \
                              : memcmp((A), (B), sizeof(struct irc_in_addr)))

/** Evaluate to non-zero if \a ADDR is a loopback address. */
#define irc_in_addr_is_loopback(ADDR) (!(ADDR)->in6_16[0] && !(ADDR)->in6_16[1] && !(ADDR)->in6_16[2] \
                                       && !(ADDR)->in6_16[3] && !(ADDR)->in6_16[4] \
                                       && ((!(ADDR)->in6_16[5] \
                                            && ((!(ADDR)->in6_16[6] && (ADDR)->in6_16[7] == htons(1)) \
                                                || (ntohs((ADDR)->in6_16[6]) & 0xff00) == 0x7f00)) \
                                           || (((ADDR)->in6_16[5] == 65535) \
                                               && (ntohs((ADDR)->in6_16[6]) & 0xff00) == 0x7f00)))

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
