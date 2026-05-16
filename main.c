/*
 * snowfall/main.c — Login manager entry point.
 *
 * Lifecycle:
 *   1. Open DRM (grabs master → snowcone exits)
 *   2. Open input
 *   3. Discover users and sessions
 *   4. Render loop: draw UI, poll for keys, update state
 *   5. On submit: authenticate via PAM
 *   6. On success: drop DRM master, launch compositor
 *   7. When compositor exits: loop back to step 1
 */

#include "sf_auth.h"
#include "sf_drm.h"
#include "sf_input.h"
#include "sf_renderer.h"
#include "sf_session.h"
#include "sf_ui_state.h"

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;

static void sig_handler(int sig) {
    (void)sig;
    g_stop = 1;
}

/* ------------------------------------------------------------------ */
/* One greeter session                                                */
/* ------------------------------------------------------------------ */

static int run_greeter(void) {
    sf_drm_t drm;
    if (sf_drm_open(&drm) < 0) {
        fprintf(stderr, "snowfall: failed to open DRM\n");
        return -1;
    }

    sf_input_t *inp = sf_input_create();
    if (!inp) {
        fprintf(stderr, "snowfall: failed to open input\n");
        sf_drm_close(&drm);
        return -1;
    }

    sf_ui_state_t ui;
    sf_ui_init(&ui);
    sf_ui_load_users(&ui);
    sf_session_discover(&ui.sessions);

    if (ui.sessions.count == 0) {
        sf_ui_set_status(&ui, "No sessions found — check /usr/share/wayland-sessions/");
    }

    /* Initial render. */
    sf_render(&drm, &ui);
    sf_drm_flush(&drm, 0, 0, drm.width, drm.height);

    /* Event loop. */
    struct pollfd pfd = {
        .fd     = sf_input_fd(inp),
        .events = POLLIN,
    };

    while (!g_stop) {
        int ret = poll(&pfd, 1, 100 /* ms — for cursor blink etc. */);
        if (ret < 0) {
            if (g_stop) break;
            continue;
        }

        if (pfd.revents & POLLIN) {
            sf_input_dispatch(inp);

            sf_key_event_t kev;
            while (sf_input_next_key(inp, &kev)) {
                sf_ui_handle_key(&ui, &kev);
            }
        }

        /* Check for submit. */
        if (ui.submit) {
            ui.submit = false;

            if (ui.user_count == 0) {
                sf_ui_set_status(&ui, "No users available");
            } else if (ui.sessions.count == 0) {
                sf_ui_set_status(&ui, "No sessions available");
            } else {
                const char *username = ui.users[ui.user_index].name;
                sf_ui_set_status(&ui, "Authenticating...");
                sf_render(&drm, &ui);
                sf_drm_flush(&drm, 0, 0, drm.width, drm.height);

                sf_auth_result_t auth = sf_auth_login(username, ui.password);

                if (auth.ok) {
                    sf_ui_set_status(&ui, "Launching session...");
                    sf_render(&drm, &ui);
                    sf_drm_flush(&drm, 0, 0, drm.width, drm.height);

                    /* Clean up before launching. */
                    sf_input_destroy(inp);
                    sf_drm_drop_master(&drm);
                    sf_drm_close(&drm);

                    /* Launch compositor (blocks until it exits). */
                    sf_session_launch(
                        &ui.sessions.entries[ui.session_index],
                        &auth);

                    /* Compositor exited — return so main() restarts
                     * the greeter loop. */
                    return 0;
                } else {
                    sf_ui_set_status(&ui, auth.err);
                    sf_ui_clear_password(&ui);
                }
            }
        }

        /* Re-render. */
        sf_render(&drm, &ui);
        sf_drm_flush(&drm, 0, 0, drm.width, drm.height);
    }

    sf_input_destroy(inp);
    sf_drm_close(&drm);
    return g_stop ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    signal(SIGTERM, sig_handler);
    signal(SIGINT,  sig_handler);

    fprintf(stderr, "snowfall: starting login manager\n");

    /* Greeter restart loop: after the user's compositor exits, show
     * the login screen again. Stops on SIGTERM (OpenRC shutdown). */
    while (!g_stop) {
        int ret = run_greeter();
        if (ret < 0) {
            /* DRM or input failure — wait a bit before retrying
             * so we don't spin-loop on a broken system. */
            fprintf(stderr, "snowfall: greeter failed, retrying in 2s\n");
            sleep(2);
        }
        /* ret == 0: compositor exited normally, restart greeter.
         * ret == 1: SIGTERM received, exit. */
        if (ret == 1) break;
    }

    fprintf(stderr, "snowfall: exiting\n");
    return 0;
}
