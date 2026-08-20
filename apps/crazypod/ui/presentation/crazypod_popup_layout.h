#ifndef CRAZYPOD_POPUP_LAYOUT_H
#define CRAZYPOD_POPUP_LAYOUT_H

#include "lvgl.h"

struct crazypod_popup_geometry {
    int x;
    int y;
    int width;
    int height;
};

int crazypod_popup_text_width(
    const char *text, const lv_font_t *font);
int crazypod_popup_wrapped_text_height(
    const char *text, const lv_font_t *font,
    int width, int line_space);
int crazypod_popup_clamp_width(
    int content_width, int horizontal_padding,
    int minimum_width, int maximum_width);
struct crazypod_popup_geometry crazypod_popup_centered_geometry(
    int width, int height);

#endif
