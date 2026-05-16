/*
 * snowfall/src/session.c — Wayland session discovery and launch.
 */

#include "sf_session.h"

#include <dirent.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SESSIONS_DIR "/usr/share/wayland-sessions"

/* ------------------------------------------------------------------ */
/* Desktop file parsing                                               */
/* ------------------------------------------------------------------ */

static int parse_desktop_file(const char *path, sf_session_entry_t *out) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[512];
    int got_name = 0, got_exec = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline. */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        if (strncmp(line, "Name=", 5) == 0 && !got_name) {
            snprintf(out->name, SF_NAME_LEN, "%s", line + 5);
            got_name = 1;
        } else if (strncmp(line, "Exec=", 5) == 0 && !got_exec) {
            snprintf(out->exec, SF_EXEC_LEN, "%s", line + 5);
            got_exec = 1;
        }

        if (got_name && got_exec) break;
    }

    fclose(f);
    return (got_name && got_exec) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

int sf_session_discover(sf_session_list_t *list) {
    memset(list, 0, sizeof(*list));

    DIR *dir = opendir(SESSIONS_DIR);
    if (!dir) return 0; /* no sessions dir is not an error */

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (list->count >= SF_MAX_SESSIONS) break;

        /* Only .desktop files. */
        const char *dot = strrchr(ent->d_name, '.');
        if (!dot || strcmp(dot, ".desktop") != 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", SESSIONS_DIR, ent->d_name);

        sf_session_entry_t entry;
        if (parse_desktop_file(path, &entry) == 0) {
            list->entries[list->count++] = entry;
        }
    }

    closedir(dir);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Session launch                                                     */
/* ------------------------------------------------------------------ */

static void setup_xdg_runtime(uid_t uid) {
    /* Ensure XDG_RUNTIME_DIR exists. */
    char dir[128];
    snprintf(dir, sizeof(dir), "/run/user/%u", uid);
    mkdir(dir, 0700);
    chown(dir, uid, uid);
    setenv("XDG_RUNTIME_DIR", dir, 1);
}

int sf_session_launch(const sf_session_entry_t *session,
                      const sf_auth_result_t *auth) {
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid > 0) {
        /* Parent: wait for compositor to exit, then return so the
         * greeter can restart (show the login screen again). */
        int status;
        waitpid(pid, &status, 0);
        return 0;
    }

    /* Child: drop privileges and exec the compositor. */

    /* Start a new session (detach from the greeter's controlling tty). */
    setsid();

    /* Set up groups. */
    initgroups(auth->home, auth->gid);
    setgid(auth->gid);
    setuid(auth->uid);

    /* Environment. */
    clearenv();
    setenv("HOME", auth->home, 1);
    setenv("USER", auth->home, 1); /* fixed below */
    setenv("SHELL", auth->shell, 1);
    setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
    setup_xdg_runtime(auth->uid);

    /* Fix USER — should be the username, not home dir.
     * We can extract it from the session entry's context,
     * but simpler to just re-read from the uid. */
    struct passwd *pw = getpwuid(auth->uid);
    if (pw) setenv("USER", pw->pw_name, 1);

    /* XDG_SESSION_TYPE for Wayland compositors. */
    setenv("XDG_SESSION_TYPE", "wayland", 1);

    /* cd to home. */
    if (chdir(auth->home) < 0)
        chdir("/");

    /* exec the session command via shell so it can be "sway -c ..." */
    execl("/bin/sh", "/bin/sh", "-c", session->exec, (char *)NULL);

    /* If exec failed... */
    _exit(127);
}
