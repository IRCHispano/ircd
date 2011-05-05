#include "pcre_match.h"
#include "sys.h"
#include "h.h"
#include "s_debug.h"
#include "struct.h"
#include "common.h"
#include "match.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "res.h"
#include "ircd_chattr.h"
#include "ircd_string.h"

int match_pcre(pcre *re, char *subject)
{
  int rc,i;
  int ovector[OVECCOUNT];
  char *substring_start;
  int substring_length;
  int subject_length;

  if(re==NULL || subject==NULL)
    return 1;

  if(rc = pcre_exec(re, NULL, subject, strlen(subject), 0, 0, ovector, OVECCOUNT)<0)
    return 1;

  if (rc == 0)
    rc = OVECCOUNT/3;


  subject_length = strlen(subject);
  for (i = 0; i < rc; i++)
  {
    substring_start = subject + ovector[2*i];
    substring_length = ovector[2*i+1] - ovector[2*i];
    // Alguna subcadena coincide del todo
    if(subject_length == substring_length && !strncmp(substring_start, subject, substring_length))
      return 0;
  }

  return 1;
}

int match_pcre_str(char *regexp, char *subject)
{
  pcre *re;
  const char *error_str;
  int erroffset;
  int res;

  if(subject==NULL)
    return 1;

  if((re=pcre_compile(regexp, 0, &error_str, &erroffset, NULL))==NULL)
    return 1;

  res=match_pcre(re, subject);
  MyFree(re);

  return res;
}

int match_pcre_ci(pcre *re, char *subject) {
  char *low=NULL, *tmp=NULL;
  int res;

  DupString(low, subject);

  if(low==NULL)
    return 0;

  tmp=low;

  while (*tmp) {
    *tmp=ToLower(*tmp);
    *tmp++;
  }

  res = match_pcre(re, low);

  MyFree(low);
  return res;
}