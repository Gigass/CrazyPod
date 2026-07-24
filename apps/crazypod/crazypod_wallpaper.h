#ifndef CRAZYPOD_WALLPAPER_H
#define CRAZYPOD_WALLPAPER_H

#include "lvgl.h"

void crazypod_wallpaper_init(void);
const lv_image_dsc_t *crazypod_default_wallpaper(void);
const lv_image_dsc_t *crazypod_frosted_wallpaper_capsule(void);

#endif
