#ifndef CRAZYPOD_ARTWORK_WIDGET_H
#define CRAZYPOD_ARTWORK_WIDGET_H

#include "lvgl.h"

struct crazypod_track;

lv_obj_t *crazypod_artwork_widget_create(
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int display_size,
    const lv_image_dsc_t *descriptor,
    bool scale_descriptor);

#endif
