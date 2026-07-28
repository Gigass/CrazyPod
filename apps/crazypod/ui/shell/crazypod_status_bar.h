#ifndef CRAZYPOD_STATUS_BAR_H
#define CRAZYPOD_STATUS_BAR_H

#include <stdint.h>

#include "lvgl.h"

#define CRAZYPOD_STATUS_BAR_COUNT 2

void crazypod_status_bar_create(int index, lv_obj_t *screen);
void crazypod_status_bars_update(void);
void crazypod_status_bar_set_palette(
    int index, uint32_t foreground, uint32_t background);
void crazypod_status_bar_foreground(int index);

#endif
