#ifndef CRAZYPOD_REMOTE_MULTITAP_H
#define CRAZYPOD_REMOTE_MULTITAP_H

#include <stdbool.h>

enum crazypod_remote_multitap_action {
    CRAZYPOD_REMOTE_MULTITAP_NONE = 0,
    CRAZYPOD_REMOTE_MULTITAP_PLAY_PAUSE,
    CRAZYPOD_REMOTE_MULTITAP_NEXT,
    CRAZYPOD_REMOTE_MULTITAP_PREVIOUS,
};

struct crazypod_remote_multitap_state {
    unsigned int tap_count;
    bool press_active;
    bool press_invalid;
    long press_start;
    long deadline;
};

void crazypod_remote_multitap_reset(
    struct crazypod_remote_multitap_state *state);
bool crazypod_remote_multitap_is_down(long button);
enum crazypod_remote_multitap_action
crazypod_remote_multitap_handle_down(
    struct crazypod_remote_multitap_state *state,
    long button, long now, long window_ticks);
enum crazypod_remote_multitap_action
crazypod_multitap_handle_button(
    struct crazypod_remote_multitap_state *state,
    long button, long button_mask,
    long now, long window_ticks);
enum crazypod_remote_multitap_action
crazypod_remote_multitap_tick(
    struct crazypod_remote_multitap_state *state, long now);
enum crazypod_remote_multitap_action
crazypod_remote_multitap_flush(
    struct crazypod_remote_multitap_state *state);
int crazypod_remote_multitap_wait_ticks(
    const struct crazypod_remote_multitap_state *state,
    long now, int fallback);

#endif
