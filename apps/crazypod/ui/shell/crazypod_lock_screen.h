#ifndef CRAZYPOD_LOCK_SCREEN_H
#define CRAZYPOD_LOCK_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

struct crazypod_lock_screen_callbacks {
    void (*play_wheel_feedback)(long button);
    void (*previous_track)(void);
    void (*toggle_playback)(void);
    void (*next_track)(void);
    void (*refresh_media)(void);
    void (*unlocked)(void);
    bool (*lock_inhibited)(void);
};

struct crazypod_lock_media_snapshot {
    bool active;
    bool playing;
    const char *track_path;
    const char *title;
    const char *artist;
    const char *album;
    uint32_t elapsed_ms;
    uint32_t length_ms;
    const lv_image_dsc_t *artwork;
    unsigned artwork_generation;
};

lv_obj_t *crazypod_lock_screen_create(
    lv_obj_t *parent,
    const struct crazypod_lock_screen_callbacks *callbacks);
void crazypod_lock_screen_refresh_appearance(void);
void crazypod_lock_screen_refresh_clock(void);
void crazypod_lock_screen_update_media(
    const struct crazypod_lock_media_snapshot *snapshot);
void crazypod_lock_screen_show(bool turn_display_off);
void crazypod_lock_screen_process(void);
bool crazypod_lock_screen_handle_button(long button, intptr_t data);
bool crazypod_lock_screen_is_locked(void);
bool crazypod_lock_screen_motion_active(void);
void crazypod_lock_screen_initialize_backlight_state(void);
#ifdef SIMULATOR
void crazypod_lock_screen_simulator_set_progress(int progress);
#endif

#endif
