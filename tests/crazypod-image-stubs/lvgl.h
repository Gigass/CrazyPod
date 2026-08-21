#ifndef CRAZYPOD_IMAGE_TEST_LVGL_H
#define CRAZYPOD_IMAGE_TEST_LVGL_H

#include <stdint.h>

#define LV_IMAGE_HEADER_MAGIC 0x19
#define LV_COLOR_FORMAT_RGB565 0x12

typedef struct {
    uint32_t magic;
    uint32_t cf;
    uint32_t flags;
    uint32_t w;
    uint32_t h;
    uint32_t stride;
} lv_image_header_t;

typedef struct {
    lv_image_header_t header;
    uint32_t data_size;
    const uint8_t *data;
    const void *reserved;
    const void *reserved_2;
} lv_image_dsc_t;

#endif
