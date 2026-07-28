#ifndef CRAZYPOD_SCREEN_CORNERS_H
#define CRAZYPOD_SCREEN_CORNERS_H

#include "lvgl.h"

void crazypod_screen_corners_create(
    lv_obj_t *screen, int screen_index);
void crazypod_screen_corners_refresh(void);

#endif
