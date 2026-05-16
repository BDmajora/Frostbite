/*
 * snowfall/src/input.c — Keyboard input via libinput + xkbcommon.
 */

#include "sf_input.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libinput.h>
#include <libudev.h>
#include <xkbcommon/xkbcommon.h>

/* ------------------------------------------------------------------ */
/* Internal state                                                     */
/* ------------------------------------------------------------------ */

#define KEY_QUEUE_SIZE 32

struct sf_input {
    struct libinput       *li;
    struct udev           *udev;
    struct xkb_context    *xkb_ctx;
    struct xkb_keymap     *xkb_map;
    struct xkb_state      *xkb_state;
    sf_key_event_t         queue[KEY_QUEUE_SIZE];
    int                    q_head;
    int                    q_tail;
};

/* ------------------------------------------------------------------ */
/* libinput interface callbacks                                       */
/* ------------------------------------------------------------------ */

static int open_restricted(const char *path, int flags, void *data) {
    (void)data;
    int fd = open(path, flags | O_CLOEXEC);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *data) {
    (void)data;
    close(fd);
}

static const struct libinput_interface li_iface = {
    .open_restricted  = open_restricted,
    .close_restricted = close_restricted,
};

/* ------------------------------------------------------------------ */
/* Queue helpers                                                      */
/* ------------------------------------------------------------------ */

static void queue_push(sf_input_t *inp, sf_key_event_t ev) {
    int next = (inp->q_head + 1) % KEY_QUEUE_SIZE;
    if (next == inp->q_tail) return; /* full — drop */
    inp->queue[inp->q_head] = ev;
    inp->q_head = next;
}

/* ------------------------------------------------------------------ */
/* Key translation                                                    */
/* ------------------------------------------------------------------ */

static void handle_key_event(sf_input_t *inp,
                             struct libinput_event_keyboard *kev) {
    uint32_t key   = libinput_event_keyboard_get_key(kev);
    int      state = libinput_event_keyboard_get_key_state(kev);

    /* xkbcommon wants evdev code + 8 */
    xkb_keycode_t kc = key + 8;

    if (state == LIBINPUT_KEY_STATE_PRESSED) {
        xkb_state_update_key(inp->xkb_state, kc, XKB_KEY_DOWN);
    } else {
        xkb_state_update_key(inp->xkb_state, kc, XKB_KEY_UP);
        return; /* only act on press */
    }

    xkb_keysym_t sym = xkb_state_key_get_one_sym(inp->xkb_state, kc);

    sf_key_event_t ev = { .action = SF_KEY_NONE, .ch = 0 };

    switch (sym) {
    case XKB_KEY_Return:    ev.action = SF_KEY_ENTER;     break;
    case XKB_KEY_KP_Enter:  ev.action = SF_KEY_ENTER;     break;
    case XKB_KEY_BackSpace: ev.action = SF_KEY_BACKSPACE;  break;
    case XKB_KEY_Tab:       ev.action = SF_KEY_TAB;        break;
    case XKB_KEY_ISO_Left_Tab: ev.action = SF_KEY_TAB;     break;
    case XKB_KEY_Escape:    ev.action = SF_KEY_ESCAPE;     break;
    case XKB_KEY_Up:        ev.action = SF_KEY_UP;         break;
    case XKB_KEY_Down:      ev.action = SF_KEY_DOWN;       break;
    case XKB_KEY_Left:      ev.action = SF_KEY_LEFT;       break;
    case XKB_KEY_Right:     ev.action = SF_KEY_RIGHT;      break;
    default: {
        /* Try to get a UTF-32 character. */
        char buf[8];
        int len = xkb_state_key_get_utf8(inp->xkb_state, kc, buf, sizeof(buf));
        if (len > 0 && (unsigned char)buf[0] >= 0x20) {
            /* Decode first UTF-8 codepoint. */
            uint32_t cp = 0;
            unsigned char c0 = (unsigned char)buf[0];
            if (c0 < 0x80)       cp = c0;
            else if (c0 < 0xe0)  cp = ((c0 & 0x1f) << 6)
                                    | (buf[1] & 0x3f);
            else if (c0 < 0xf0)  cp = ((c0 & 0x0f) << 12)
                                    | ((buf[1] & 0x3f) << 6)
                                    | (buf[2] & 0x3f);
            else                 cp = ((c0 & 0x07) << 18)
                                    | ((buf[1] & 0x3f) << 12)
                                    | ((buf[2] & 0x3f) << 6)
                                    | (buf[3] & 0x3f);
            ev.action = SF_KEY_CHAR;
            ev.ch     = cp;
        }
        break;
    }
    }

    if (ev.action != SF_KEY_NONE)
        queue_push(inp, ev);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

sf_input_t *sf_input_create(void) {
    sf_input_t *inp = calloc(1, sizeof(*inp));
    if (!inp) return NULL;

    inp->udev = udev_new();
    if (!inp->udev) goto fail;

    inp->li = libinput_udev_create_context(&li_iface, NULL, inp->udev);
    if (!inp->li) goto fail;

    if (libinput_udev_assign_seat(inp->li, "seat0") < 0)
        goto fail;

    /* xkbcommon setup — default keymap. */
    inp->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!inp->xkb_ctx) goto fail;

    struct xkb_rule_names names = { 0 }; /* system defaults */
    inp->xkb_map = xkb_keymap_new_from_names(inp->xkb_ctx, &names,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!inp->xkb_map) goto fail;

    inp->xkb_state = xkb_state_new(inp->xkb_map);
    if (!inp->xkb_state) goto fail;

    return inp;

fail:
    sf_input_destroy(inp);
    return NULL;
}

int sf_input_fd(sf_input_t *inp) {
    return libinput_get_fd(inp->li);
}

void sf_input_dispatch(sf_input_t *inp) {
    libinput_dispatch(inp->li);

    struct libinput_event *ev;
    while ((ev = libinput_get_event(inp->li)) != NULL) {
        if (libinput_event_get_type(ev) == LIBINPUT_EVENT_KEYBOARD_KEY) {
            struct libinput_event_keyboard *kev =
                libinput_event_get_keyboard_event(ev);
            handle_key_event(inp, kev);
        }
        libinput_event_destroy(ev);
    }
}

bool sf_input_next_key(sf_input_t *inp, sf_key_event_t *out) {
    if (inp->q_tail == inp->q_head) return false;
    *out = inp->queue[inp->q_tail];
    inp->q_tail = (inp->q_tail + 1) % KEY_QUEUE_SIZE;
    return true;
}

void sf_input_destroy(sf_input_t *inp) {
    if (!inp) return;
    if (inp->xkb_state) xkb_state_unref(inp->xkb_state);
    if (inp->xkb_map)   xkb_keymap_unref(inp->xkb_map);
    if (inp->xkb_ctx)   xkb_context_unref(inp->xkb_ctx);
    if (inp->li)         libinput_unref(inp->li);
    if (inp->udev)       udev_unref(inp->udev);
    free(inp);
}
