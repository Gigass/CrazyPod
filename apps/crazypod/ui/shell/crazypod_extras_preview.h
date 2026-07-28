#ifndef CRAZYPOD_EXTRAS_PREVIEW_H
#define CRAZYPOD_EXTRAS_PREVIEW_H

#include "lvgl.h"

#include "../navigation/crazypod_ui_routes.h"

void crazypod_extras_preview_render(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font);

#endif
