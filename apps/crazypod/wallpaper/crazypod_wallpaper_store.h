#ifndef CRAZYPOD_WALLPAPER_STORE_H
#define CRAZYPOD_WALLPAPER_STORE_H

#include "lcd.h"
#include "lvgl.h"

#include "../crazypod_wallpaper.h"

void crazypod_wallpaper_store_init(void);
bool crazypod_wallpaper_store_load(
    enum crazypod_wallpaper_target target,
    const char *source_path, fb_data *pixels,
    lv_image_dsc_t *descriptor);
bool crazypod_wallpaper_store_save(
    enum crazypod_wallpaper_target target,
    const char *source_path, const fb_data *pixels,
    enum crazypod_wallpaper_apply_result *error);
void crazypod_wallpaper_store_remove(
    enum crazypod_wallpaper_target target);

#endif
