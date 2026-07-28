#ifndef CRAZYPOD_PLAYBACK_H
#define CRAZYPOD_PLAYBACK_H

#include "lvgl.h"

struct crazypod_playback_host {
    void (*render)(bool transition);
};

void crazypod_playback_configure(
    const struct crazypod_playback_host *host);
void crazypod_playback_initialize(void);
int crazypod_playback_initial_album_index(void);
void crazypod_playback_toggle(void);
void crazypod_playback_previous_or_restart(void);
void crazypod_playback_update_timer(lv_timer_t *timer);
void crazypod_playback_process_artwork(void);
void crazypod_playback_process_media(void);
void crazypod_playback_tick_wave(long now);
void crazypod_playback_sync_album_flow(void);
void crazypod_playback_warm_album_flow(
    long now, bool locked);

#endif
