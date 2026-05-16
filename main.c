/*
 * snowfall/main.c — Login manager entry point.
 *
 * Lifecycle:
 *   1. Politely ask snowcone (if any) to exit via SIGUSR1.
 *   2. Open DRM (acquire_master grabs master; snowcone's own watchdog
 *      will also detect the loss as a fallback).
 *   3. Open input.
 *   4. Discover users and sessions.
 *   5. Render loop: draw UI, poll for keys, update state.
 *   6. On submit: authenticate via PAM.
 *   7. On success: drop DRM master, launch compositor.
 *   8. When compositor exits: loop back to step 1.
 *
 * Why we signal snowcone explicitly even though snowcone polls for
 * master loss:
 *
 *   - The poll runs every ~200ms. Signal delivery is faster, so the
 *     transition is more visually clean (no extra splash frame after
 *     we already have master).
 *   - It cooperates with snowcone's own teardown path, which skips
 *     the dirty-flush ioctl when it knows it lost master. If we just
 *     yanked master out from under it, snowcone might still try to
 *     flush a black frame on the way out and briefly fight us for
 *     the framebuffer.
 *   - Belt and braces: if signal delivery is dropped or snowcone has
 *     already exited, no harm done — kill() to a missing pid is just
 *     ESRCH, which we ignore.
 */

#include "sf_auth.h"
#include "sf_drm.h"
#include "sf_input.h"
#include "sf_renderer.h"
#include "sf_session.h"
#include "sf_ui_state.h"

#include <dirent.h>
#include <errno.h>
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
/* snowcone handoff                                                   */
/* ------------------------------------------------------------------ */

/*
 * Walk /proc looking for processes named "snowcone" and send them
 * SIGUSR1, which snowcone's signal handler treats as a clean-exit
 * request.
 *
 * We avoid shelling out to pkill because:
 *   - it adds a runtime dep on procps that we'd otherwise not need
 *   - it forks, which is wasteful when /proc is right there
 *   - we want to be precise: only match exact comm "snowcone", not
 *     anything containing the substring
 *
 * Errors are non-fatal — if /proc is unreadable or no match is found,
 * snowcone's master-loss watchdog will still cause it to exit shortly
 * after we acquire master.
 */
static void signal_snowcone_exit(void) {
    DIR *d = opendir("/proc");
    if (!d) return;

    struct dirent *ent;
    int signaled = 0;
    while ((ent = readdir(d)) != NULL) {
        /* Numeric pid dirs only. */
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;

        /* /proc/<pid>/comm — pid is at most 7 digits on Linux, but
         * dirent->d_name is 256, so size the buffer to satisfy the
         * compiler's worst-case format-truncation analysis. */
        char path[280];
        snprintf(path, sizeof(path), "/proc/%s/comm", ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char comm[32] = {0};
        if (fgets(comm, sizeof(comm), f)) {
            /* Strip trailing newline. */
            size_t n = strlen(comm);
            if (n > 0 && comm[n-1] == '\n') comm[n-1] = '\0';

            if (strcmp(comm, "snowcone") == 0) {
                pid_t pid = (pid_t)atoi(ent->d_name);
                if (pid > 0 && kill(pid, SIGUSR1) == 0) {
                    signaled++;
                }
            }
        }
        fclose(f);
    }
    closedir(d);

    if (signaled > 0) {
        fprintf(stderr, "snowfall: signaled %d snowcone process(es) to exit\n",
                signaled);
    }
}

/* ------------------------------------------------------------------ */
/* One greeter session                                                */
/* ------------------------------------------------------------------ */

static int run_greeter(void) {
    /* Politely ask snowcone to exit before we try to take master.
     * acquire_master's internal retry loop will absorb whatever
     * cleanup time snowcone needs. */
    signal_snowcone_exit();

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

    /* Greeter restart loop. Three exit paths:
     *   run_greeter() == 0  → compositor exited normally, restart.
     *   run_greeter() == 1  → SIGTERM received, exit.
     *   run_greeter() == -1 → DRM/input failure, brief backoff + retry.
     *
     * The backoff after a -1 is short (500ms) because acquire_master
     * already does its own ~2.5s of inline retries inside sf_drm_open.
     * The outer sleep here is just to keep us from spinning if some
     * unrelated failure (e.g. /dev/dri removed by a driver reload)
     * is keeping DRM open from working at all. */
    while (!g_stop) {
        int ret = run_greeter();
        if (ret < 0) {
            fprintf(stderr, "snowfall: greeter failed, retrying shortly\n");
            usleep(500 * 1000); /* 500ms */
            continue;
        }
        if (ret == 1) break; /* SIGTERM */
        /* ret == 0: compositor exited, loop back to show greeter. */
    }

    fprintf(stderr, "snowfall: exiting\n");
    return 0;
}