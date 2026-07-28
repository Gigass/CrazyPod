#ifndef CRAZYPOD_PREVIEW_PRIMITIVES_H
#define CRAZYPOD_PREVIEW_PRIMITIVES_H

#include <stdint.h>

#include "lvgl.h"

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

#endif
