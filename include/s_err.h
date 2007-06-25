#if !defined(S_ERR_H)
#define S_ERR_H

/*=============================================================================
 * Proto types
 */

extern char *err_str(int numeric);
extern char *rpl_str(int numeric);
#if defined(WATCH)
extern char *watch_str(int numeric);
#endif

#endif /* S_ERR_H */
