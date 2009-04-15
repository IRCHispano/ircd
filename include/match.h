#if !defined(MATCH_H)
#define MATCH_H

#include "pcre.h"

/*=============================================================================
 * System headers used by this header file
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
extern char *collapse(char *pattern);

extern int matchcomp(char *cmask, int *minlen, int *charset, const char *mask);
extern int matchexec(const char *string, const char *cmask, int minlen);
extern int matchdecomp(char *mask, const char *cmask);
extern int mmexec(const char *wcm, int wminlen, const char *rcm, int rminlen);
extern int matchcompIP(struct in_mask *imask, const char *mask);
extern int match_pcre(pcre *re, char *subject);
extern int match_pcre_str(char *regexp, char *subject);

#endif /* MATCH_H */
