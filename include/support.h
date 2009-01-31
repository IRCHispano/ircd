#if !defined(SUPPORT_H)
#define SUPPORT_H

#include <netinet/in.h>

/*=============================================================================
 * Proto types
 */

#if !defined(HAVE_STRTOKEN)
extern char *strtoken(char **save, char *str, char *fs);
#endif
extern void dumpcore(const char *pattern, ...)
    __attribute__ ((format(printf, 1, 2)));
extern char *inetntoa(struct in_addr in);
extern char *inetntoa_c(aClient *cptr);
extern int check_if_ipmask(const char *mask);
extern void write_log(const char *filename, const char *pattern, ...);
extern struct in_addr client_addr(aClient *cptr);
#endif /* SUPPORT_H */
