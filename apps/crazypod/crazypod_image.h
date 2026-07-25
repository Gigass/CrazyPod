#ifndef CRAZYPOD_IMAGE_H
#define CRAZYPOD_IMAGE_H

#include <stdbool.h>

#include "lcd.h"
#include "lvgl.h"

void crazypod_image_init(void);
void crazypod_image_decode_lock(void);
void crazypod_image_decode_unlock(void);
bool crazypod_image_configure_rgb565(
    lv_image_dsc_t *descriptor, const fb_data *pixels,
    int width, int height);
bool crazypod_image_scale_rgb565(
    const fb_data *source, int source_width, int source_height,
    int source_stride, fb_data *destination,
    int destination_width, int destination_height);

#endif
