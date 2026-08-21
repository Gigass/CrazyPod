#ifndef CRAZYPOD_OVERLAY_GLASS_H
#define CRAZYPOD_OVERLAY_GLASS_H

#include <stdbool.h>

#include "lvgl.h"

void crazypod_overlay_glass_configure(
    void (*boost)(int ticks));
void crazypod_overlay_glass_prepare(bool refresh);
lv_obj_t *crazypod_overlay_glass_panel(
    lv_obj_t *parent, int x, int y, int width, int height);
lv_obj_t *crazypod_overlay_glass_headphone_panel(
    lv_obj_t *parent, int x, int y, int width, int height);

#endif
