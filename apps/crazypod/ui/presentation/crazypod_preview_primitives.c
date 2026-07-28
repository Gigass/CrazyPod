#include "config.h"

#ifdef IPOD_6G

#include "crazypod_preview_primitives.h"
#include "crazypod_ui_widgets.h"

#define TEXT_PANEL_WIDTH 140
#define TEXT_PANEL_MAX_HEIGHT 70

void crazypod_preview_add_bevel(
    lv_obj_t *object, int width, int height,
    uint32_t light, uint32_t dark)
{
    if(object == NULL || width < 8 || height < 8)
        return;
    crazypod_ui_widget_box(
        object, 3, 2, width - 6, 1, 0, light, 72);
    crazypod_ui_widget_box(
        object, 3, height - 3, width - 6, 1, 0, dark, 112);
}

void crazypod_preview_add_fastener(
    lv_obj_t *parent, int x, int y, uint32_t metal)
{
    lv_obj_t *fastener = crazypod_ui_widget_box(
        parent, x, y, 5, 5, LV_RADIUS_CIRCLE,
        metal, LV_OPA_COVER);

    lv_obj_set_style_border_width(fastener, 1, 0);
    lv_obj_set_style_border_color(
        fastener, lv_color_hex(0x1C2022), 0);
    lv_obj_set_style_border_opa(fastener, 125, 0);
    crazypod_ui_widget_box(
        fastener, 1, 2, 3, 1, 0, 0x303538, 185);
}

lv_obj_t *crazypod_preview_make_plinth(
    lv_obj_t *parent, int x, int y, int width,
    uint32_t top, uint32_t base)
{
    lv_obj_t *plinth = crazypod_ui_widget_box(
        parent, x, y, width, 9, 3, base, LV_OPA_COVER);

    lv_obj_set_style_border_width(plinth, 1, 0);
    lv_obj_set_style_border_color(
        plinth, lv_color_hex(0x0B0D0E), 0);
    lv_obj_set_style_border_opa(plinth, 185, 0);
    crazypod_ui_widget_box(
        plinth, 4, 1, width - 8, 2, 1, top, 205);
    crazypod_ui_widget_box(
        plinth, 8, 6, width - 16, 1, 0, 0x000000, 145);
    return plinth;
}

void crazypod_preview_add_paper_rules(
    lv_obj_t *paper, int width, int top, int count,
    int spacing, uint32_t ink)
{
    int index;

    crazypod_ui_widget_box(
        paper, 9, top - 3, 1, count * spacing - 1,
        0, 0xC96F64, 62);
    for(index = 0; index < count; ++index)
        crazypod_ui_widget_box(
            paper, 13, top + index * spacing,
            width - 21 - (index == count - 1 ? 12 : 0),
            1, 0, ink, index == 0 ? 105 : 68);
}

lv_obj_t *crazypod_preview_make_text_panel(
    lv_obj_t *parent, int y, int height)
{
    lv_obj_t *panel;

    if(height > TEXT_PANEL_MAX_HEIGHT)
        height = TEXT_PANEL_MAX_HEIGHT;
    panel = crazypod_ui_widget_box(
        parent, 170, y, TEXT_PANEL_WIDTH, height,
        9, 0x11171A, 242);
    lv_obj_set_style_bg_grad_color(
        panel, lv_color_hex(0x060809), 0);
    lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(
        panel, lv_color_hex(0x89959A), 0);
    lv_obj_set_style_border_opa(panel, 72, 0);
    crazypod_preview_add_bevel(
        panel, TEXT_PANEL_WIDTH, height, 0xEEF5F7, 0x000000);
    crazypod_preview_add_fastener(panel, 5, 5, 0xAEB7BB);
    crazypod_preview_add_fastener(
        panel, TEXT_PANEL_WIDTH - 10, 5, 0xAEB7BB);
    return panel;
}

#endif
