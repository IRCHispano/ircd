#if !defined(BSD_H)
#define BSD_H

/*=============================================================================
 * Proto types
 */

extern int deliver_it(aClient *cptr, const char *str, int len);

extern int writecalls;
extern int writeb[10];

#endif
