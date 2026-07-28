#ifndef CRAZYPOD_MUSIC_PREVIEW_COVER_H
#define CRAZYPOD_MUSIC_PREVIEW_COVER_H

#include <stdbool.h>

#include "lvgl.h"

#include "../../../crazypod_music.h"

struct crazypod_music_preview_cover_context {
    bool defer_media;
    bool *media_deferred;
    const lv_font_t *metadata_font;
};

lv_obj_t *crazypod_music_preview_sleeve_create(
    const struct crazypod_music_preview_cover_context *context,
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int size, int seed,
    int artwork_slot, bool cache_only);
lv_obj_t *crazypod_music_preview_initial_cover_create(
    const struct crazypod_music_preview_cover_context *context,
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int size, int seed);

#endif
