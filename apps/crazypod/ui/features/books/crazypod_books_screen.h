#ifndef CRAZYPOD_BOOKS_SCREEN_H
#define CRAZYPOD_BOOKS_SCREEN_H

#include <stdint.h>

#include "lvgl.h"

#define CRAZYPOD_BOOKS_READER_TOP 28
#define CRAZYPOD_BOOKS_READER_TOOLBAR_TOP 206
#define CRAZYPOD_BOOKS_READER_MARGIN 16
#define CRAZYPOD_BOOKS_READER_BOTTOM_MARGIN 8
#define CRAZYPOD_BOOKS_READER_LINE_SPACE 2

const lv_font_t *crazypod_books_screen_reader_font(unsigned size);
unsigned crazypod_books_screen_reader_line_height(unsigned size);
unsigned crazypod_books_screen_measure_width(
    uint32_t codepoint, uint32_t next_codepoint, void *context);

void crazypod_books_screen_render_reader(
    lv_obj_t *content, int book_index, uint32_t page_offset,
    const char *page_text, uint32_t page_color, uint32_t ink_color,
    bool toolbar_visible);
void crazypod_books_screen_render_stats(lv_obj_t *content);
void crazypod_books_screen_render_info(lv_obj_t *content,
                                       int book_index);

#endif
