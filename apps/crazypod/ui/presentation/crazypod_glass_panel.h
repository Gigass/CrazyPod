#ifndef CRAZYPOD_GLASS_PANEL_H
#define CRAZYPOD_GLASS_PANEL_H

#include "lvgl.h"

#include "crazypod_glass_sampler.h"

lv_obj_t *crazypod_glass_panel_create(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, enum crazypod_glass_material material,
    const lv_image_dsc_t *descriptor);

#endif
