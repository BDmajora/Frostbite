/*
 * snowfall/src/ui_state.c — Login screen state machine.
 *
 * Pure logic: no rendering, no I/O, no DRM, no PAM.
 * Receives key events, updates fields and focus.
 */

#include "sf_ui_state.h"

#include <pwd.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Init                                                               */
/* ------------------------------------------------------------------ */

void sf_ui_init(sf_ui_state_t *ui) {
    memset(ui, 0, sizeof(*ui));
    ui->focus = SF_FOCUS_USER;
}

/* ------------------------------------------------------------------ */
/* User discovery                                                     */
/* ------------------------------------------------------------------ */

void sf_ui_load_users(sf_ui_state_t *ui) {
    /* Enumerate human users: UID >= 1000, has a real shell,
     * and isn't "nobody". */
    ui->user_count = 0;

    setpwent();
    struct passwd *pw;
    while ((pw = getpwent()) != NULL) {
        if (ui->user_count >= SF_MAX_USERS) break;
        if (pw->pw_uid < 1000) continue;
        if (pw->pw_uid == 65534) continue; /* nobody */

        /* Skip nologin shells. */
        const char *sh = pw->pw_shell;
        if (!sh || !sh[0]) continue;
        if (strstr(sh, "nologin") || strstr(sh, "/false")) continue;

        snprintf(ui->users[ui->user_count].name, SF_USERNAME_LEN,
                 "%s", pw->pw_name);
        ui->user_count++;
    }
    endpwent();

    /* Fallback: if no users found, leave the list empty.
     * The renderer will show a "no users found" message. */
}

/* ------------------------------------------------------------------ */
/* Key handling                                                       */
/* ------------------------------------------------------------------ */

static void append_char(char *buf, int *len, int max, uint32_t ch) {
    /* Simple ASCII append. For full UTF-8 we'd encode here,
     * but passwords and usernames are overwhelmingly ASCII. */
    if (*len >= max - 1) return;
    if (ch > 127) return; /* skip non-ASCII for now */
    buf[*len] = (char)ch;
    (*len)++;
    buf[*len] = '\0';
}

static void delete_char(char *buf, int *len) {
    if (*len <= 0) return;
    (*len)--;
    buf[*len] = '\0';
}

static void handle_user_focus(sf_ui_state_t *ui, const sf_key_event_t *ev) {
    switch (ev->action) {
    case SF_KEY_UP:
        if (ui->user_index > 0) ui->user_index--;
        break;
    case SF_KEY_DOWN:
        if (ui->user_index < ui->user_count - 1) ui->user_index++;
        break;
    case SF_KEY_TAB:
    case SF_KEY_ENTER:
        ui->focus = SF_FOCUS_PASSWORD;
        break;
    default:
        break;
    }
}

static void handle_password_focus(sf_ui_state_t *ui,
                                  const sf_key_event_t *ev) {
    switch (ev->action) {
    case SF_KEY_CHAR:
        append_char(ui->password, &ui->password_len,
                    SF_FIELD_MAX_LEN, ev->ch);
        break;
    case SF_KEY_BACKSPACE:
        delete_char(ui->password, &ui->password_len);
        break;
    case SF_KEY_ENTER:
        ui->submit = true;
        break;
    case SF_KEY_TAB:
        ui->focus = SF_FOCUS_SESSION;
        break;
    case SF_KEY_ESCAPE:
        ui->focus = SF_FOCUS_USER;
        break;
    default:
        break;
    }
}

static void handle_session_focus(sf_ui_state_t *ui,
                                 const sf_key_event_t *ev) {
    switch (ev->action) {
    case SF_KEY_LEFT:
    case SF_KEY_UP:
        if (ui->session_index > 0) ui->session_index--;
        break;
    case SF_KEY_RIGHT:
    case SF_KEY_DOWN:
        if (ui->session_index < ui->sessions.count - 1)
            ui->session_index++;
        break;
    case SF_KEY_TAB:
        ui->focus = SF_FOCUS_USER;
        break;
    case SF_KEY_ENTER:
        ui->focus = SF_FOCUS_PASSWORD;
        break;
    case SF_KEY_ESCAPE:
        ui->focus = SF_FOCUS_PASSWORD;
        break;
    default:
        break;
    }
}

void sf_ui_handle_key(sf_ui_state_t *ui, const sf_key_event_t *ev) {
    ui->status[0] = '\0'; /* clear status on any keypress */

    switch (ui->focus) {
    case SF_FOCUS_USER:     handle_user_focus(ui, ev);     break;
    case SF_FOCUS_PASSWORD: handle_password_focus(ui, ev); break;
    case SF_FOCUS_SESSION:  handle_session_focus(ui, ev);  break;
    }
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

void sf_ui_set_status(sf_ui_state_t *ui, const char *msg) {
    snprintf(ui->status, SF_STATUS_LEN, "%s", msg);
}

void sf_ui_clear_password(sf_ui_state_t *ui) {
    memset(ui->password, 0, SF_FIELD_MAX_LEN);
    ui->password_len = 0;
}
