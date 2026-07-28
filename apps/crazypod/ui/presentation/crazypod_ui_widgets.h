#ifndef CRAZYPOD_UI_WIDGETS_H
#define CRAZYPOD_UI_WIDGETS_H

#include <stdint.h>

#include "lvgl.h"

void crazypod_ui_widget_make_plain(lv_obj_t *obj);
lv_obj_t *crazypod_ui_widget_label(lv_obj_t *parent, const char *text,
                                   const lv_font_t *font, uint32_t color,
                                   lv_opa_t opacity);
lv_obj_t *crazypod_ui_widget_box(lv_obj_t *parent, int x, int y,
                                 int width, int height, int radius,
                                 uint32_t color, lv_opa_t opacity);
void crazypod_ui_widget_pixel_heart(lv_obj_t *parent, int x, int y,
                                    int unit, uint32_t color,
                                    lv_opa_t opacity);

#endif
