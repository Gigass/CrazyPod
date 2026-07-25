#ifndef CRAZYPOD_WALLPAPER_H
#define CRAZYPOD_WALLPAPER_H

#include <stdbool.h>

#include "lvgl.h"

void crazypod_wallpaper_init(void);
void crazypod_wallpaper_reload_custom(void);
bool crazypod_wallpaper_select(bool menu, const char *path);
bool crazypod_wallpaper_apply_crop(
    bool menu, const char *path,
    const lv_image_dsc_t *source,
    int crop_x, int crop_y, int crop_width, int crop_height);
void crazypod_wallpaper_clear(bool menu);
bool crazypod_wallpaper_prepare_frosted_capsule(void);
const lv_image_dsc_t *crazypod_default_wallpaper(void);
const lv_image_dsc_t *crazypod_custom_home_wallpaper(void);
const lv_image_dsc_t *crazypod_custom_menu_wallpaper(void);
const lv_image_dsc_t *crazypod_frosted_wallpaper_capsule(void);

#endif
