#ifndef CRAZYPOD_BOOK_IMAGE_H
#define CRAZYPOD_BOOK_IMAGE_H

#include <stdint.h>

#include "lvgl.h"

const lv_image_dsc_t *crazypod_book_image_get(
    int book_index, uint32_t page_offset,
    int max_width, int max_height);
void crazypod_book_image_reset(void);

#endif
