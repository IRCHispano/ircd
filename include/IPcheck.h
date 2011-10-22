#if !defined(IPCHECK_H)
#define IPCHECK_H

/*=============================================================================
 * Proto types
 */

extern int IPcheck_local_connect(aClient *cptr);
extern void IPcheck_connect_fail(aClient *cptr);
extern void IPcheck_connect_succeeded(aClient *cptr);
extern int IPcheck_remote_connect(aClient *cptr, const char *hostname,
    int is_burst);
extern void IPcheck_disconnect(aClient *cptr);
extern unsigned short IPcheck_nr(aClient *cptr);

#if defined(BDD_CLONES)
extern int IPbusca_clones(char *host);
extern int IPbusca_clones_cptr(aClient *cptr);
#endif

#endif /* IPCHECK_H */
