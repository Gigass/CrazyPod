#ifndef CRAZYPOD_PREVIEW_PRIMITIVES_H
#define CRAZYPOD_PREVIEW_PRIMITIVES_H

#include <stdint.h>

#include "lvgl.h"

#define CRAZYPOD_PREVIEW_PANE_X 160
#define CRAZYPOD_PREVIEW_PANE_Y 32
#define CRAZYPOD_PREVIEW_PANE_WIDTH 160
#define CRAZYPOD_PREVIEW_VISUAL_CENTER_Y 100
#define CRAZYPOD_PREVIEW_CAPTION_Y 166
#define CRAZYPOD_PREVIEW_CAPTION_HEIGHT 52

int crazypod_preview_centered_x(int width);
int crazypod_preview_visual_y(int height);
void crazypod_preview_add_bevel(
    lv_obj_t *object, int width, int height,
    uint32_t light, uint32_t dark);
void crazypod_preview_add_fastener(
    lv_obj_t *parent, int x, int y, uint32_t metal);
lv_obj_t *crazypod_preview_make_plinth(
    lv_obj_t *parent, int x, int y, int width,
    uint32_t top, uint32_t base);
void crazypod_preview_add_paper_rules(
    lv_obj_t *paper, int width, int top, int count,
    int spacing, uint32_t ink);
lv_obj_t *crazypod_preview_make_text_panel(
    lv_obj_t *parent, int y, int height);
lv_obj_t *crazypod_preview_make_caption(
    lv_obj_t *parent,
    const char *title, const lv_font_t *title_font,
    const char *detail, const lv_font_t *detail_font);

#endif
