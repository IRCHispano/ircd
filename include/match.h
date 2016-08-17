#if !defined(MATCH_H)
#define MATCH_H

#include "pcre.h"

/*=============================================================================
 * System headers used by this header file
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
struct irc_in_addr;

#define OVECCOUNT 3

/*=============================================================================
 * Structures
 */

struct in_mask {
  struct in_addr bits;
  struct in_addr mask;
  int fall;
};

/*=============================================================================
 * Proto types
 */

extern int mmatch(const char *old_mask, const char *new_mask);
extern int match(const char *ma, const char *na);
extern int match_case(const char *ma, const char *na);
extern char *collapse(char *pattern);

extern int matchcomp(char *cmask, int *minlen, int *charset, const char *mask);
extern int matchexec(const char *string, const char *cmask, int minlen);
extern int matchdecomp(char *mask, const char *cmask);
extern int mmexec(const char *wcm, int wminlen, const char *rcm, int rminlen);
extern int matchcompIP(struct in_mask *imask, const char *mask);
extern int match_pcre(pcre *re, char *subject);
extern int match_pcre_str(char *regexp, char *subject);
extern int match_pcre_ci(pcre *re, char *subject);

extern int ipmask_check(const struct irc_in_addr *addr, const struct irc_in_addr *mask, unsigned char bits);

#endif /* MATCH_H */
