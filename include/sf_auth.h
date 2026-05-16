/*
 * snowfall/include/sf_auth.h — PAM authentication.
 *
 * Thin wrapper around libpam. Authenticates a username/password pair
 * and, on success, opens a PAM session so environment variables
 * (XDG_RUNTIME_DIR, etc.) are set for the compositor.
 */

#ifndef SF_AUTH_H
#define SF_AUTH_H

#include <stdbool.h>
#include <sys/types.h>

/* Result of an authentication attempt. */
typedef struct {
    bool    ok;         /* true = authenticated                      */
    uid_t   uid;        /* user's UID (valid only when ok == true)   */
    gid_t   gid;        /* user's primary GID                        */
    char    home[256];  /* home directory path                        */
    char    shell[256]; /* login shell                                */
    char    err[256];   /* human-readable error (when ok == false)    */
} sf_auth_result_t;

/* Authenticate `user` with `password` via PAM.
 * Opens a session on success (caller must call sf_auth_close_session
 * when the compositor exits, though for a login manager that execs
 * the compositor, this typically never happens). */
sf_auth_result_t sf_auth_login(const char *user, const char *password);

#endif /* SF_AUTH_H */
