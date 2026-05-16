/*
 * snowfall/include/sf_ui_state.h — Login screen UI state.
 *
 * Pure data + logic: no rendering, no I/O.  Tracks which field is
 * focused, the text content of the username and password fields,
 * which session is selected, the user list, and any status message.
 */

#ifndef SF_UI_STATE_H
#define SF_UI_STATE_H

#include "sf_input.h"
#include "sf_session.h"

#include <stdbool.h>

#define SF_FIELD_MAX_LEN 128
#define SF_STATUS_LEN    256
#define SF_MAX_USERS     32
#define SF_USERNAME_LEN  64

typedef enum {
    SF_FOCUS_USER,      /* user selection list */
    SF_FOCUS_PASSWORD,  /* password text field */
    SF_FOCUS_SESSION,   /* session picker      */
} sf_focus_t;

typedef struct {
    char name[SF_USERNAME_LEN];
} sf_user_entry_t;

typedef struct {
    /* Input fields. */
    char            password[SF_FIELD_MAX_LEN];
    int             password_len;

    /* User list. */
    sf_user_entry_t users[SF_MAX_USERS];
    int             user_count;
    int             user_index;       /* currently selected user */

    /* Session list (populated externally). */
    sf_session_list_t sessions;
    int             session_index;    /* currently selected session */

    /* Focus. */
    sf_focus_t      focus;

    /* Status / error message shown to the user. */
    char            status[SF_STATUS_LEN];

    /* Set to true when the user presses Enter on the password field. */
    bool            submit;
} sf_ui_state_t;

/* Initialize to defaults (focus on user, empty fields). */
void sf_ui_init(sf_ui_state_t *ui);

/* Discover human users from /etc/passwd and populate ui->users. */
void sf_ui_load_users(sf_ui_state_t *ui);

/* Feed a key event into the state machine. Updates fields, focus,
 * selection, and sets ui->submit when the user confirms. */
void sf_ui_handle_key(sf_ui_state_t *ui, const sf_key_event_t *ev);

/* Set a status message (e.g. "Authentication failed"). */
void sf_ui_set_status(sf_ui_state_t *ui, const char *msg);

/* Clear the password field (after a failed attempt). */
void sf_ui_clear_password(sf_ui_state_t *ui);

#endif /* SF_UI_STATE_H */
