/*
 * snowfall/include/sf_session.h — Wayland session discovery and launch.
 *
 * Scans /usr/share/wayland-sessions/ for .desktop files, parses their
 * Name= and Exec= fields, and provides a list the UI can display.
 * On successful auth, launches the chosen session as the target user.
 */

#ifndef SF_SESSION_H
#define SF_SESSION_H

#include "sf_auth.h"

#define SF_MAX_SESSIONS 16
#define SF_NAME_LEN     64
#define SF_EXEC_LEN     256

typedef struct {
    char name[SF_NAME_LEN]; /* human-readable name, e.g. "Sway"     */
    char exec[SF_EXEC_LEN]; /* Exec= value, e.g. "sway"            */
} sf_session_entry_t;

typedef struct {
    sf_session_entry_t entries[SF_MAX_SESSIONS];
    int                count;
} sf_session_list_t;

/* Scan the wayland-sessions directory and populate the list.
 * Returns 0 on success (even if count == 0). */
int sf_session_discover(sf_session_list_t *list);

/* Fork, drop privileges to `auth->uid`:`auth->gid`, set up the
 * environment (HOME, XDG_RUNTIME_DIR, etc.), and exec the session's
 * Exec= command. This function does not return on success.
 * On failure it returns -1. */
int sf_session_launch(const sf_session_entry_t *session,
                      const sf_auth_result_t *auth);

#endif /* SF_SESSION_H */
