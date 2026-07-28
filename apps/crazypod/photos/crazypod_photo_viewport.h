#ifndef CRAZYPOD_PHOTO_VIEWPORT_H
#define CRAZYPOD_PHOTO_VIEWPORT_H

#include "lvgl.h"

void crazypod_photo_viewport_reset(void);
const lv_image_dsc_t *crazypod_photo_viewport_render(
    int index, const lv_image_dsc_t *source,
    int zoom_percent, int *pan_x, int *pan_y);
const lv_image_dsc_t *crazypod_photo_viewport_render_crop(
    const lv_image_dsc_t *source, int center_y);

#endif
