#ifndef CRAZYPOD_PHOTOS_PREVIEW_H
#define CRAZYPOD_PHOTOS_PREVIEW_H

#include <stdbool.h>

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"

struct crazypod_photos_preview_context {
    lv_obj_t *parent;
    bool defer_media;
    bool *media_deferred;
};

void crazypod_photos_preview_render(
    const struct route_state *state,
    const struct crazypod_photos_preview_context *context);
void crazypod_videos_preview_render(
    const struct route_state *state,
    const struct crazypod_photos_preview_context *context);

#endif
