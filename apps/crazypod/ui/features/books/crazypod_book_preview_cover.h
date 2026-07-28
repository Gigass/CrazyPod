#ifndef CRAZYPOD_BOOK_PREVIEW_COVER_H
#define CRAZYPOD_BOOK_PREVIEW_COVER_H

#include "lvgl.h"

#include "../../../crazypod_books.h"

lv_obj_t *crazypod_book_preview_cover_create(
    lv_obj_t *parent, const struct crazypod_book *book,
    int x, int y, int width, int height);

#endif
