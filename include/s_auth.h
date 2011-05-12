#if !defined(S_AUTH_H)
#define S_AUTH_H

/*=============================================================================
 * Proto types
 */

extern void start_auth(struct Client *cptr);
extern void send_authports(struct Client *cptr);
extern void read_authports(struct Client *cptr);

#endif /* S_AUTH_H */
