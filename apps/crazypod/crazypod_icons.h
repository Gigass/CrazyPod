#ifndef CRAZYPOD_ICONS_H
#define CRAZYPOD_ICONS_H

#include <stdint.h>

#include "lvgl.h"

#define CRAZYPOD_ICON_COUNT 14

void crazypod_icons_init(void);
void crazypod_icons_load_theme(int theme);
const lv_image_dsc_t *crazypod_icon_get(int index);
const uint8_t *crazypod_icon_get_premultiplied(int index);

#endif
