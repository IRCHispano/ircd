#if !defined(S_SERV_H)
#define S_SERV_H

#include "struct.h"
#include "client.h"
/*=============================================================================
 * General defines
 */

/*-----------------------------------------------------------------------------
 * Macro's
 */

#if defined(CHECK_TS_LINKS)
#define CHECK_TS_MAX_LINKS     30 /* 30 segundos de desfase maximo
                                   * permitido en los TS
                                   */
#endif

#define GLINE_BURST_TIME       86400 /* Tiempo de modificacion para
                                        reenvio de GLINEs en burst
                                        86400 segundos (1 dia) */


/*=============================================================================
 * Proto types
 */

extern int m_server(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_server_estab(aClient *cptr, aConfItem *aconf, aConfItem *bconf, time_t start_timestamp);
extern int m_error(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_end_of_burst(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_end_of_burst_ack(aClient *cptr, aClient *sptr,
    int parc, char *parv[]);
extern int m_desynch(aClient *cptr, aClient *sptr, int parc, char *parv[]);

extern unsigned int max_connection_count, max_client_count;
extern unsigned int max_global_count;
extern time_t max_client_count_TS, max_global_count_TS;

#endif /* S_SERV_H */
