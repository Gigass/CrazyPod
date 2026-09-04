#ifndef CRAZYPOD_PLAYBACK_H
#define CRAZYPOD_PLAYBACK_H

#include "lvgl.h"

#include "../../../crazypod_music.h"

struct crazypod_playback_host {
    void (*render)(bool transition);
};

void crazypod_playback_configure(
    const struct crazypod_playback_host *host);
void crazypod_playback_initialize(void);
bool crazypod_playback_commands_ready(void);
void crazypod_playback_headphone_changed(bool inserted);
int crazypod_playback_initial_album_index(void);
void crazypod_playback_toggle(void);
void crazypod_playback_next(void);
void crazypod_playback_previous_or_restart(void);
bool crazypod_playback_seek_begin(int direction);
void crazypod_playback_seek_step(void);
void crazypod_playback_seek_finish(void);
void crazypod_playback_toggle_async(void);
void crazypod_playback_stop_async(void);
void crazypod_playback_next_async(void);
void crazypod_playback_previous_or_restart_async(void);
void crazypod_playback_select_async(int queue_index);
bool crazypod_playback_select_music_async(
    enum crazypod_music_scope scope, int group_index,
    int selected_index, const char *query);
void crazypod_playback_refresh_lock_screen(void);
void crazypod_playback_request_refresh_after_unlock(
    uint32_t present_sequence, bool animate_capsule);
bool crazypod_playback_refresh_after_unlock_pending(void);
void crazypod_playback_service_after_unlock(
    uint32_t present_sequence);
void crazypod_playback_update_timer(lv_timer_t *timer);
void crazypod_playback_process_artwork(void);
void crazypod_playback_process_media(void);
void crazypod_playback_tick_wave(long now);
void crazypod_playback_sync_album_flow(void);
void crazypod_playback_warm_album_flow(
    long now, bool locked);

#endif
