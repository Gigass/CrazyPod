#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "button.h"

#include "crazypod_remote_multitap.h"

static enum crazypod_remote_multitap_action action_for_taps(
    unsigned int tap_count)
{
    if(tap_count == 1)
        return CRAZYPOD_REMOTE_MULTITAP_PLAY_PAUSE;
    if(tap_count == 2)
        return CRAZYPOD_REMOTE_MULTITAP_NEXT;
    if(tap_count >= 3)
        return CRAZYPOD_REMOTE_MULTITAP_PREVIOUS;
    return CRAZYPOD_REMOTE_MULTITAP_NONE;
}

static enum crazypod_remote_multitap_action finish_taps(
    struct crazypod_remote_multitap_state *state)
{
    enum crazypod_remote_multitap_action action;

    if(state == NULL)
        return CRAZYPOD_REMOTE_MULTITAP_NONE;
    action = action_for_taps(state->tap_count);
    state->tap_count = 0;
    state->deadline = 0;
    return action;
}

void crazypod_remote_multitap_reset(
    struct crazypod_remote_multitap_state *state)
{
    if(state != NULL)
        memset(state, 0, sizeof(*state));
}

bool crazypod_remote_multitap_is_down(long button)
{
#ifdef IPOD_ACCESSORY_PROTOCOL
    return (button & (BUTTON_RC_VOL_DOWN | BUTTON_RC_DOWN)) != 0;
#else
    (void)button;
    return false;
#endif
}

enum crazypod_remote_multitap_action
crazypod_multitap_handle_button(
    struct crazypod_remote_multitap_state *state,
    long button, long button_mask,
    long now, long window_ticks)
{
    enum crazypod_remote_multitap_action action =
        CRAZYPOD_REMOTE_MULTITAP_NONE;
    bool release;
    bool repeated;

    if(state == NULL || button_mask == 0 ||
       (button & button_mask) == 0)
        return CRAZYPOD_REMOTE_MULTITAP_NONE;
    if(window_ticks < 1)
        window_ticks = 1;
    if(!state->press_active && state->tap_count > 0 &&
       (long)(now - state->deadline) >= 0)
        action = finish_taps(state);

    release = (button & BUTTON_REL) != 0;
    repeated = (button & BUTTON_REPEAT) != 0;
    if(release) {
        if(!state->press_active)
            return action;
        state->press_active = false;
        if(state->press_invalid ||
           (long)(now - state->press_start) > window_ticks) {
            state->press_invalid = false;
            state->tap_count = 0;
            state->deadline = 0;
            return action;
        }
        ++state->tap_count;
        if(state->tap_count >= 3)
            return finish_taps(state);
        state->deadline = now + window_ticks;
        return action;
    }
    if(repeated) {
        if(state->press_active) {
            state->press_invalid = true;
            state->tap_count = 0;
            state->deadline = 0;
        }
        return action;
    }
    if(!state->press_active) {
        state->press_active = true;
        state->press_invalid = false;
        state->press_start = now;
    }
    return action;
}

enum crazypod_remote_multitap_action
crazypod_remote_multitap_handle_down(
    struct crazypod_remote_multitap_state *state,
    long button, long now, long window_ticks)
{
#ifdef IPOD_ACCESSORY_PROTOCOL
    return crazypod_multitap_handle_button(
        state, button,
        BUTTON_RC_VOL_DOWN | BUTTON_RC_DOWN,
        now, window_ticks);
#else
    (void)state;
    (void)button;
    (void)now;
    (void)window_ticks;
    return CRAZYPOD_REMOTE_MULTITAP_NONE;
#endif
}

enum crazypod_remote_multitap_action
crazypod_remote_multitap_tick(
    struct crazypod_remote_multitap_state *state, long now)
{
    if(state == NULL || state->press_active ||
       state->tap_count == 0 ||
       (long)(now - state->deadline) < 0)
        return CRAZYPOD_REMOTE_MULTITAP_NONE;
    return finish_taps(state);
}

enum crazypod_remote_multitap_action
crazypod_remote_multitap_flush(
    struct crazypod_remote_multitap_state *state)
{
    if(state == NULL)
        return CRAZYPOD_REMOTE_MULTITAP_NONE;
    if(state->press_active) {
        crazypod_remote_multitap_reset(state);
        return CRAZYPOD_REMOTE_MULTITAP_NONE;
    }
    return finish_taps(state);
}

int crazypod_remote_multitap_wait_ticks(
    const struct crazypod_remote_multitap_state *state,
    long now, int fallback)
{
    long remaining;

    if(state == NULL || state->press_active ||
       state->tap_count == 0)
        return fallback;
    remaining = state->deadline - now;
    if(remaining <= 0)
        return 1;
    if(remaining < fallback)
        return (int)remaining;
    return fallback;
}

#endif
