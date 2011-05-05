#include "pcre.h"

#define OVECCOUNT 3

extern int match_pcre(pcre *re, char *subject);
extern int match_pcre_str(char *regexp, char *subject);
extern int match_pcre_ci(pcre *re, char *subject);
