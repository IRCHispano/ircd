/* $Id$ */

#include "sys.h"
#include "ircd_alloc.h"
#include "slab_alloc.h"

#include <string.h>


char *SlabStringAlloc(size_t size)
{
  return MyMalloc(size);
}

void SlabStringAllocDup(char **old, char *new, size_t max_len)
{
  int len;

  if (*old)
    MyFree(*old);

  len = strlen(new);
  if ((!max_len) || (len <= max_len))
  {
    *old = MyMalloc(len + 1);
    strcpy(*old, new);
    return;
  }

/* Nos pasamos de taman~o */
  *old = MyMalloc(max_len + 1);
  strncpy(*old, new, max_len);
  (*old)[max_len] = '\0';
}

void SlabStringFree(char *string)
{
  MyFree(string);
}
