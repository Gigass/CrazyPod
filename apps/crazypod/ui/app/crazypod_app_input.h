#ifndef CRAZYPOD_APP_INPUT_H
#define CRAZYPOD_APP_INPUT_H

#include <stdint.h>

#include "../navigation/crazypod_feature_dispatcher.h"
#include "../shell/crazypod_system_event.h"

struct crazypod_app_input_host {
    const struct crazypod_feature_bindings *feature_bindings;
    struct crazypod_system_event_actions system_events;
    bool (*power_prompt_visible)(void);
    bool (*handle_power_prompt)(
        long base, bool repeated, intptr_t data);
    bool (*usb_prompt_visible)(void);
    bool (*handle_usb_prompt)(
        long base, bool repeated, intptr_t data);
    bool (*headphone_prompt_visible)(void);
    bool (*handle_headphone_prompt)(
        long base, bool repeated, intptr_t data);
    bool (*handle_power_hold)(long button);
    bool (*handle_lock)(long button, intptr_t data);
    void (*close_product)(void);
    void (*render)(bool transition);
    bool (*handle_confirmation)(
        const struct route_state *state);
    void (*previous_track)(void);
    void (*toggle_playback)(void);
    void (*open_now_playing)(void);
    void (*begin_music_scan)(void);
};

void crazypod_app_input_configure(
    const struct crazypod_app_input_host *host);
void crazypod_app_input_handle(
    long button, intptr_t data, long now);
int crazypod_app_input_wait_ticks(long now);
void crazypod_app_input_tick(long now, bool locked);

#endif
