#ifndef CRAZYPOD_UI_WIDGETS_H
#define CRAZYPOD_UI_WIDGETS_H

#include <stdint.h>

#include "lvgl.h"

enum crazypod_ui_icon {
    CRAZYPOD_UI_ICON_HEART = 0,
    CRAZYPOD_UI_ICON_PLAY,
    CRAZYPOD_UI_ICON_SHUFFLE,
    CRAZYPOD_UI_ICON_REPEAT,
    CRAZYPOD_UI_ICON_REPEAT_ONE,
    CRAZYPOD_UI_ICON_FILE,
    CRAZYPOD_UI_ICON_BARS,
    CRAZYPOD_UI_ICON_LIST,
    CRAZYPOD_UI_ICON_AUDIO,
};

enum crazypod_ui_row_label_role {
    CRAZYPOD_UI_ROW_LABEL_TEXT = 0,
    CRAZYPOD_UI_ROW_LABEL_MARKER,
};

void crazypod_ui_widget_make_plain(lv_obj_t *obj);
const lv_font_t *crazypod_ui_widget_resolve_font(
    const char *text, const lv_font_t *font);
lv_obj_t *crazypod_ui_widget_label(lv_obj_t *parent, const char *text,
                                   const lv_font_t *font, uint32_t color,
                                   lv_opa_t opacity);
void crazypod_ui_widget_set_label_text(lv_obj_t *label, const char *text);
void crazypod_ui_widget_align_row_label(
    lv_obj_t *label, int x,
    enum crazypod_ui_row_label_role role);
lv_obj_t *crazypod_ui_widget_box(lv_obj_t *parent, int x, int y,
                                 int width, int height, int radius,
                                 uint32_t color, lv_opa_t opacity);
void crazypod_ui_widget_pixel_heart(lv_obj_t *parent, int x, int y,
                                    int unit, uint32_t color,
                                    lv_opa_t opacity);
lv_obj_t *crazypod_ui_widget_icon(lv_obj_t *parent, int x, int y,
                                  enum crazypod_ui_icon icon,
                                  uint32_t color, lv_opa_t opacity);
void crazypod_ui_widget_icon_set(lv_obj_t *obj,
                                 enum crazypod_ui_icon icon);
void crazypod_ui_widget_icon_set_color(lv_obj_t *obj, uint32_t color);

#endif
