#ifndef CRAZYPOD_CORNER_MASKS_H
#define CRAZYPOD_CORNER_MASKS_H

#include "lvgl.h"

#define CRAZYPOD_CORNER_MASK_SCREEN_COUNT 3

void crazypod_corner_masks_attach(lv_obj_t *screen, int screen_index);
void crazypod_corner_masks_refresh(void);

#endif
