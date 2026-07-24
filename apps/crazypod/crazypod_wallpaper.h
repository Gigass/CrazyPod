#ifndef CRAZYPOD_WALLPAPER_H
#define CRAZYPOD_WALLPAPER_H

#include <stdbool.h>

#include "lvgl.h"

void crazypod_wallpaper_init(void);
bool crazypod_wallpaper_prepare_frosted_capsule(void);
const lv_image_dsc_t *crazypod_default_wallpaper(void);
const lv_image_dsc_t *crazypod_frosted_wallpaper_capsule(void);

#endif
