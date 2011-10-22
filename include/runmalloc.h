/*
 * runmalloc.h
 *
 * (C) Copyright 1996 - 1997, Carlo Wood (carlo@runaway.xs4all.nl)
 *
 * Headerfile of runmalloc.c
 *
 */

#if !defined(RUNMALLOC_H)
#define RUNMALLOC_H

#if defined(DEBUGMALLOC)

#if defined(MEMMAGICNUMS) && !defined(MEMSIZESTATS)
#define MEMSIZESTATS
#endif
#if !defined(MEMLEAKSTATS)
#undef MEMTIMESTATS
#endif

/*=============================================================================
 * Proto types
 */

#if defined(MEMLEAKSTATS)
extern void *RunMalloc_memleak(size_t size, int line, const char *filename);
extern void *RunCalloc_memleak(size_t nmemb, size_t size,
    int line, const char *filename);
extern void *RunRealloc_memleak(void *ptr, size_t size,
    int line, const char *filename);
struct Client;
extern void report_memleak_stats(struct Client *sptr, int parc, char *parv[]);
#define RunMalloc(x) RunMalloc_memleak(x, __LINE__, __FILE__)
#define RunCalloc(x,y) RunCalloc_memleak(x,y, __LINE__, __FILE__)
#define RunRealloc(x,y) RunRealloc_memleak(x,y, __LINE__, __FILE__)
#define _RunMalloc RunMalloc_memleak
#define _RunCalloc RunCalloc_memleak
#define _RunRealloc RunRealloc_memleak
#else
extern void *RunMalloc(size_t size);
extern void *RunCalloc(size_t nmemb, size_t size);
extern void *RunRealloc(void *ptr, size_t size);
#define _RunMalloc RunMalloc
#define _RunCalloc RunCalloc
#define _RunRealloc RunRealloc
#endif
extern int RunFree_test(void *ptr);
extern void RunFree(void *ptr);
#if defined(MEMSIZESTATS)
extern unsigned int get_alloc_cnt(void);
extern size_t get_mem_size(void);
#endif
#define _RunFree RunFree

#else /* !DEBUGMALLOC */

#include <stdlib.h>

#undef MEMSIZESTATS
#undef MEMMAGICNUMS
#undef MEMLEAKSTATS
#undef MEMTIMESTATS

#define Debug_malloc(x)
#define RunMalloc(x) malloc(x)
#define RunCalloc(x,y) calloc(x,y)
#define RunRealloc(x,y) realloc(x,y)
#define RunFree(x) free(x)

#define _RunMalloc malloc
#define _RunCalloc calloc
#define _RunRealloc realloc
#define _RunFree free

#endif /* DEBUGMALLOC */

#endif /* RUNMALLOC_H */
