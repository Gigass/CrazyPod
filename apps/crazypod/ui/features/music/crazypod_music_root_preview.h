#ifndef CRAZYPOD_MUSIC_ROOT_PREVIEW_H
#define CRAZYPOD_MUSIC_ROOT_PREVIEW_H

#include <stdbool.h>

#include "lvgl.h"

void crazypod_music_root_preview_render(
    lv_obj_t *parent, int selected, bool defer_media);

#endif
