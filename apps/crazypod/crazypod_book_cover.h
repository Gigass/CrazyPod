#ifndef CRAZYPOD_BOOK_COVER_H
#define CRAZYPOD_BOOK_COVER_H

#include "lvgl.h"

const lv_image_dsc_t *crazypod_book_cover_get(
    int book_index, int max_width, int max_height);
void crazypod_book_cover_reset(void);

#endif
