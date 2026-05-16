/*
 * snowfall/include/sf_input.h — Keyboard input via libinput + xkbcommon.
 *
 * Handles udev seat open, libinput context creation, and translates
 * raw key events into Unicode codepoints or named actions.
 */

#ifndef SF_INPUT_H
#define SF_INPUT_H

#include <stdbool.h>
#include <stdint.h>

/* Named key actions that the UI state machine cares about. */
typedef enum {
    SF_KEY_NONE = 0,
    SF_KEY_CHAR,        /* a printable character — see sf_key_event_t.ch  */
    SF_KEY_BACKSPACE,
    SF_KEY_ENTER,
    SF_KEY_TAB,
    SF_KEY_ESCAPE,
    SF_KEY_UP,
    SF_KEY_DOWN,
    SF_KEY_LEFT,
    SF_KEY_RIGHT,
} sf_key_action_t;

typedef struct {
    sf_key_action_t action;
    uint32_t        ch;     /* UTF-32 codepoint when action == SF_KEY_CHAR */
} sf_key_event_t;

/* Opaque handle. */
typedef struct sf_input sf_input_t;

/* Create the input context (opens /dev/input/event* via udev).
 * Returns NULL on failure. */
sf_input_t *sf_input_create(void);

/* Get the libinput fd for poll(). */
int sf_input_fd(sf_input_t *inp);

/* Dispatch pending events. Call after poll() indicates readability. */
void sf_input_dispatch(sf_input_t *inp);

/* Pop the next translated key event, or return false if the queue is empty. */
bool sf_input_next_key(sf_input_t *inp, sf_key_event_t *out);

/* Destroy the input context. */
void sf_input_destroy(sf_input_t *inp);

#endif /* SF_INPUT_H */
