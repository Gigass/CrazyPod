#ifndef CRAZYPOD_IMAGE_H
#define CRAZYPOD_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lcd.h"
#include "lvgl.h"

#define CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE 4

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
size_t crazypod_image_glass_sample_pixels(
    int destination_width, int destination_height);
bool crazypod_image_render_glass_rgb565(
    const fb_data *source, int source_width, int source_height,
    int source_stride, int source_x, int source_y,
    int source_region_width, int source_region_height,
    uint32_t tint, unsigned tint_opa,
    fb_data *sample_pixels, fb_data *scratch_pixels,
    size_t sample_capacity, fb_data *destination,
    int destination_width, int destination_height);

#endif
