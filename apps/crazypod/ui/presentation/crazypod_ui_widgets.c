#include "crazypod_ui_widgets.h"

void crazypod_ui_widget_make_plain(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
}

lv_obj_t *crazypod_ui_widget_label(lv_obj_t *parent, const char *text,
                                   const lv_font_t *font, uint32_t color,
                                   lv_opa_t opacity)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_remove_style_all(label);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_opa(label, opacity, 0);
    return label;
}

lv_obj_t *crazypod_ui_widget_box(lv_obj_t *parent, int x, int y,
                                 int width, int height, int radius,
                                 uint32_t color, lv_opa_t opacity)
{
    lv_obj_t *box = lv_obj_create(parent);

    crazypod_ui_widget_make_plain(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_radius(box, radius, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(box, opacity, 0);
    return box;
}

void crazypod_ui_widget_pixel_heart(lv_obj_t *parent, int x, int y,
                                    int unit, uint32_t color,
                                    lv_opa_t opacity)
{
    if(unit < 1)
        unit = 1;
    crazypod_ui_widget_box(parent, x + unit, y, 2 * unit, unit, 0,
                           color, opacity);
    crazypod_ui_widget_box(parent, x + 5 * unit, y, 2 * unit, unit, 0,
                           color, opacity);
    crazypod_ui_widget_box(parent, x, y + unit, 8 * unit, 2 * unit, 0,
                           color, opacity);
    crazypod_ui_widget_box(parent, x + unit, y + 3 * unit,
                           6 * unit, unit, 0, color, opacity);
    crazypod_ui_widget_box(parent, x + 2 * unit, y + 4 * unit,
                           4 * unit, unit, 0, color, opacity);
    crazypod_ui_widget_box(parent, x + 3 * unit, y + 5 * unit,
                           2 * unit, unit, 0, color, opacity);
}
