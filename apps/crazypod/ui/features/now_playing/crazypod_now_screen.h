#ifndef CRAZYPOD_NOW_SCREEN_H
#define CRAZYPOD_NOW_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

#include "../../../crazypod_music.h"

typedef int (*crazypod_now_artwork_slot_provider)(
    const struct crazypod_track *track);
typedef uint32_t (*crazypod_now_fallback_color_provider)(
    const struct crazypod_track *track);
typedef lv_obj_t *(*crazypod_now_artwork_renderer)(
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int display_size,
    const lv_image_dsc_t *descriptor, bool scale_descriptor);

struct crazypod_now_screen_context {
    lv_obj_t *parent;
    const struct crazypod_track *track;
    bool lyrics_mode;
    const lv_font_t *metadata_font;
    uint32_t primary_color;
    crazypod_now_artwork_slot_provider artwork_slot;
    crazypod_now_fallback_color_provider fallback_color;
    crazypod_now_artwork_renderer render_artwork;
    void (*boost)(int ticks);
    lv_event_cb_t draw_wave;
};

struct crazypod_now_screen_view {
    lv_obj_t *lyrics_previous;
    lv_obj_t *lyrics_current;
    lv_obj_t *lyrics_next;
    lv_obj_t *wave_surface;
    lv_obj_t *progress_marker;
    lv_obj_t *elapsed;
    lv_obj_t *remaining;
    bool wave_playing;
    long wave_tick;
};

void crazypod_now_screen_render(
    const struct crazypod_now_screen_context *context);
void crazypod_now_screen_reset(void);
const char *crazypod_now_screen_rendered_track_path(void);
int crazypod_now_screen_wave_phase(void);
void crazypod_now_screen_tick_wave(long now, bool active, bool blocked);
void crazypod_now_screen_update_playback(
    uint32_t elapsed_ms, uint32_t length_ms);

#endif
