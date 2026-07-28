#ifndef CRAZYPOD_BOOKS_SCREEN_H
#define CRAZYPOD_BOOKS_SCREEN_H

#include <stdint.h>

#include "lvgl.h"

void crazypod_books_screen_render_reader(
    lv_obj_t *content, int book_index, uint32_t page_offset,
    const char *page_text, uint32_t page_color, uint32_t ink_color);
void crazypod_books_screen_render_stats(lv_obj_t *content);
void crazypod_books_screen_render_info(lv_obj_t *content,
                                       int book_index);

#endif

