#if !defined(SUPPORT_H)
#define SUPPORT_H

#include <netinet/in.h>
struct irc_in_addr;

/*=============================================================================
 * Proto types
 */

#if !defined(HAVE_STRTOKEN)
extern char *strtoken(char **save, char *str, char *fs);
#endif
extern void dumpcore(const char *pattern, ...)
    __attribute__ ((format(printf, 1, 2)));
extern char *inetntoa(struct in_addr in);
extern char *ircd_ntoa_c(aClient *cptr);
extern const char* ircd_ntoa(const struct irc_in_addr* addr);
extern const char* ircd_ntoa_r(char* buf, const struct irc_in_addr* addr);
#define ircd_aton(ADDR, STR) ipmask_parse((STR), (ADDR), NULL)
extern int ipmask_parse(const char *in, struct irc_in_addr *mask, unsigned char *bits_ptr);
extern int check_if_ipmask(const char *mask);
extern void write_log(const char *filename, const char *pattern, ...);
extern struct irc_in_addr *client_addr(aClient *cptr);
#endif /* SUPPORT_H */
