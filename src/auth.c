/*
 * snowfall/src/auth.c — PAM authentication.
 */

#include "sf_auth.h"

#include <grp.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* PAM conversation callback                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *password;
} conv_data_t;

static int pam_conversation(int num_msg, const struct pam_message **msg,
                            struct pam_response **resp, void *appdata) {
    conv_data_t *cd = appdata;

    struct pam_response *replies = calloc(num_msg, sizeof(*replies));
    if (!replies) return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; i++) {
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
            replies[i].resp = strdup(cd->password);
            if (!replies[i].resp) {
                free(replies);
                return PAM_BUF_ERR;
            }
            break;
        case PAM_TEXT_INFO:
        case PAM_ERROR_MSG:
            /* Ignore informational messages. */
            break;
        default:
            free(replies);
            return PAM_CONV_ERR;
        }
    }

    *resp = replies;
    return PAM_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

sf_auth_result_t sf_auth_login(const char *user, const char *password) {
    sf_auth_result_t result;
    memset(&result, 0, sizeof(result));

    conv_data_t cd = { .password = password };
    struct pam_conv conv = {
        .conv     = pam_conversation,
        .appdata_ptr = &cd,
    };

    pam_handle_t *pamh = NULL;
    int rc = pam_start("snowfall", user, &conv, &pamh);
    if (rc != PAM_SUCCESS) {
        snprintf(result.err, sizeof(result.err),
                 "PAM init failed: %s", pam_strerror(pamh, rc));
        return result;
    }

    rc = pam_authenticate(pamh, 0);
    if (rc != PAM_SUCCESS) {
        snprintf(result.err, sizeof(result.err),
                 "Authentication failed");
        pam_end(pamh, rc);
        return result;
    }

    rc = pam_acct_mgmt(pamh, 0);
    if (rc != PAM_SUCCESS) {
        snprintf(result.err, sizeof(result.err),
                 "Account check failed: %s", pam_strerror(pamh, rc));
        pam_end(pamh, rc);
        return result;
    }

    /* Open session (sets XDG_RUNTIME_DIR, etc. via pam_systemd or
     * pam_elogind, depending on what's installed). */
    rc = pam_open_session(pamh, 0);
    if (rc != PAM_SUCCESS) {
        snprintf(result.err, sizeof(result.err),
                 "Session open failed: %s", pam_strerror(pamh, rc));
        pam_end(pamh, rc);
        return result;
    }

    /* Look up the user's UID, GID, home. */
    struct passwd *pw = getpwnam(user);
    if (!pw) {
        snprintf(result.err, sizeof(result.err), "getpwnam failed");
        pam_end(pamh, PAM_SUCCESS);
        return result;
    }

    result.ok  = true;
    result.uid = pw->pw_uid;
    result.gid = pw->pw_gid;
    snprintf(result.home, sizeof(result.home), "%s", pw->pw_dir);
    snprintf(result.shell, sizeof(result.shell), "%s", pw->pw_shell);

    /* We intentionally do NOT call pam_end here. The PAM session
     * should stay open for the lifetime of the user's compositor.
     * In practice, since we exec the compositor, the session gets
     * cleaned up when the process tree exits and the login manager
     * is restarted by OpenRC. */

    return result;
}
