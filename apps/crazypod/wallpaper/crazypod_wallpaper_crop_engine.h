#ifndef CRAZYPOD_WALLPAPER_CROP_ENGINE_H
#define CRAZYPOD_WALLPAPER_CROP_ENGINE_H

#include "lcd.h"
#include "lvgl.h"

#include "../crazypod_wallpaper.h"

int crazypod_wallpaper_crop_engine_max_zoom(
    const lv_image_dsc_t *source);
enum crazypod_wallpaper_apply_result
crazypod_wallpaper_crop_engine_render(
    const char *path, const lv_image_dsc_t *preview,
    int crop_x, int crop_y, int crop_width, int crop_height,
    fb_data *destination,
    crazypod_wallpaper_progress_cb progress_cb,
    void *progress_user_data);

#endif
