/* $Id: slab_alloc.h,v 1.4 2004/02/04 13:26:29 jcea Exp $ */

#include <sys/types.h>          /* size_t */

char *SlabStringAlloc(size_t size);
void SlabStringAllocDup(char **old, char *new, size_t max_len);
void SlabStringFree(char *string);
