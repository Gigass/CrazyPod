#ifndef CRAZYPOD_WALLPAPER_H
#define CRAZYPOD_WALLPAPER_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef void (*crazypod_wallpaper_progress_cb)(
    int progress, void *user_data);

enum crazypod_wallpaper_apply_result {
    CRAZYPOD_WALLPAPER_APPLY_OK = 0,
    CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE,
    CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED,
    CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED,
    CRAZYPOD_WALLPAPER_APPLY_CACHE_OPEN_FAILED,
    CRAZYPOD_WALLPAPER_APPLY_CACHE_WRITE_FAILED,
    CRAZYPOD_WALLPAPER_APPLY_CACHE_PUBLISH_FAILED,
    CRAZYPOD_WALLPAPER_APPLY_SETTINGS_FAILED,
    CRAZYPOD_WALLPAPER_APPLY_ACTIVATE_FAILED,
};

enum crazypod_wallpaper_target {
    CRAZYPOD_WALLPAPER_HOME = 0,
    CRAZYPOD_WALLPAPER_MENU,
    CRAZYPOD_WALLPAPER_LOCK,
};

void crazypod_wallpaper_init(void);
void crazypod_wallpaper_reload_custom(void);
bool crazypod_wallpaper_select(
    enum crazypod_wallpaper_target target, const char *path);
enum crazypod_wallpaper_apply_result crazypod_wallpaper_apply_crop(
    enum crazypod_wallpaper_target target, const char *path,
    const lv_image_dsc_t *source,
    int crop_x, int crop_y, int crop_width, int crop_height,
    crazypod_wallpaper_progress_cb progress_cb,
    void *progress_user_data);
int crazypod_wallpaper_crop_max_zoom(
    const lv_image_dsc_t *source);
void crazypod_wallpaper_clear(enum crazypod_wallpaper_target target);
bool crazypod_wallpaper_prepare_frosted_capsule(
    uint32_t tint, unsigned tint_opa);
bool crazypod_wallpaper_prepare_frosted_lock_media(
    uint32_t tint, unsigned tint_opa);
const lv_image_dsc_t *crazypod_default_wallpaper(void);
const lv_image_dsc_t *crazypod_custom_home_wallpaper(void);
const lv_image_dsc_t *crazypod_custom_menu_wallpaper(void);
const lv_image_dsc_t *crazypod_custom_lock_wallpaper(void);
const lv_image_dsc_t *crazypod_frosted_wallpaper_capsule(void);
const lv_image_dsc_t *crazypod_frosted_lock_media(void);

#endif
