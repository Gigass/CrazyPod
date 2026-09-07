#ifndef CRAZYPOD_BOOK_PNG_H
#define CRAZYPOD_BOOK_PNG_H

#include <stdbool.h>
#include <stddef.h>

#include "lcd.h"

struct crazypod_book_png_info {
    int width;
    int height;
    size_t workspace_size;
};

/* Inspect without retaining the PNG or its decoded pixels in memory. */
bool crazypod_book_png_inspect(
    const char *path, struct crazypod_book_png_info *info);

/* Decode to RGB565 while keeping only two source scanlines in workspace. */
bool crazypod_book_png_decode(
    const char *path, int max_width, int max_height,
    fb_data *pixels, int *width, int *height,
    void *workspace, size_t workspace_size);

#endif
