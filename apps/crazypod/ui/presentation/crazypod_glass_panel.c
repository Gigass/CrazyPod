#include "config.h"

#ifdef IPOD_6G

#include "lvgl.h"

#include "crazypod_ui_widgets.h"
#include "crazypod_glass_panel.h"

#define COLOR_PANEL 0x1B1B22
#define COLOR_WHITE 0xFFFFFF

lv_obj_t *crazypod_glass_panel_create(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, enum crazypod_glass_material material,
    const lv_image_dsc_t *descriptor)
{
    lv_obj_t *panel = crazypod_ui_widget_box(
        parent, x, y, width, height,
        radius, COLOR_PANEL, LV_OPA_COVER);
    lv_obj_t *tint;
    lv_obj_t *border;
    lv_opa_t shadow_opa =
        crazypod_glass_material_shadow_opa(material);

    lv_obj_set_style_clip_corner(panel, true, 0);
    if(shadow_opa > 0) {
        lv_obj_set_style_shadow_width(panel, 12, 0);
        lv_obj_set_style_shadow_offset_y(panel, 6, 0);
        lv_obj_set_style_shadow_color(
            panel, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(panel, shadow_opa, 0);
    }
    if(descriptor != NULL &&
       descriptor->header.magic == LV_IMAGE_HEADER_MAGIC) {
        lv_obj_t *image = lv_image_create(panel);
        lv_image_set_src(image, descriptor);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }
    tint = crazypod_ui_widget_box(
        panel, 0, 0, width, height, radius,
        crazypod_glass_material_tint(material),
        crazypod_glass_material_tint_opa(material));
    lv_obj_remove_flag(tint, LV_OBJ_FLAG_CLICKABLE);
    border = crazypod_ui_widget_box(
        panel, 0, 0, width, height, radius,
        COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(border, 1, 0);
    lv_obj_set_style_border_color(
        border, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(
        border, crazypod_glass_material_border_opa(material), 0);
    lv_obj_remove_flag(border, LV_OBJ_FLAG_CLICKABLE);
    return panel;
}

#endif
