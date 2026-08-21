#ifndef CRAZYPOD_WALLPAPER_CROP_SCREEN_H
#define CRAZYPOD_WALLPAPER_CROP_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

struct crazypod_wallpaper_crop_view {
    lv_obj_t *progress_fill;
    lv_obj_t *progress_label;
};

void crazypod_wallpaper_crop_screen_render(
    lv_obj_t *parent, uint32_t white_color, uint32_t cyan_color);
void crazypod_wallpaper_crop_screen_reset(void);
bool crazypod_wallpaper_crop_screen_progress_ready(void);
void crazypod_wallpaper_crop_screen_update_progress(
    int fill_width, const char *text);
void crazypod_wallpaper_crop_screen_set_progress_error(bool error);

#endif
