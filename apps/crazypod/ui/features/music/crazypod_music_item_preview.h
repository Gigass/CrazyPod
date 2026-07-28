#ifndef CRAZYPOD_MUSIC_ITEM_PREVIEW_H
#define CRAZYPOD_MUSIC_ITEM_PREVIEW_H

#include "lvgl.h"

#include "../../../crazypod_music.h"
#include "../../navigation/crazypod_ui_routes.h"

void crazypod_music_item_preview_render(
    lv_obj_t *parent, const struct route_state *state,
    const struct crazypod_track *track,
    const lv_font_t *metadata_font);

#endif
