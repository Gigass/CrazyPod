#ifndef CRAZYPOD_PHOTO_CACHE_H
#define CRAZYPOD_PHOTO_CACHE_H

#include <stdbool.h>
#include <stdint.h>

#include "lcd.h"
#include "lvgl.h"

void crazypod_photo_cache_init(void);
bool crazypod_photo_cache_load(
    bool view, const char *path, uint32_t source_size,
    uint32_t source_mtime, lv_image_dsc_t *descriptor,
    fb_data *pixels);
void crazypod_photo_cache_store(
    bool view, const char *path, uint32_t source_size,
    uint32_t source_mtime, const lv_image_dsc_t *descriptor);

#endif
