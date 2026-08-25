#include "config.h"

#ifdef IPOD_6G

#include "crazypod_empty_state.h"
#include "crazypod_overlay_glass.h"
#include "crazypod_popup_layout.h"
#include "crazypod_ui_widgets.h"

#define COLOR_WHITE 0xFFFFFF

void crazypod_empty_state_render(
    lv_obj_t *parent, const char *symbol,
    const char *title, const char *message)
{
    bool has_symbol = symbol != NULL && symbol[0] != '\0';
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *label;
    struct crazypod_popup_geometry geometry;
    int content_width;
    int measured_width;
    int message_height;
    int y;

    if(parent == NULL || title == NULL || message == NULL)
        return;
    measured_width = crazypod_popup_text_width(
        title, &lv_font_montserrat_12);
    content_width = crazypod_popup_text_width(
        message, &lv_font_montserrat_8);
    if(content_width > measured_width)
        measured_width = content_width;
    geometry = crazypod_popup_centered_geometry(
        crazypod_popup_clamp_width(
            measured_width, 18, 168, LCD_WIDTH - 32),
        1);
    content_width = geometry.width - 28;
    message_height = crazypod_popup_wrapped_text_height(
        message, &lv_font_montserrat_8,
        content_width, 2);
    geometry = crazypod_popup_centered_geometry(
        geometry.width,
        16 + (has_symbol ? 38 : 0) +
        lv_font_get_line_height(&lv_font_montserrat_12) +
        8 + message_height + 16);
    root = crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, LV_OPA_TRANSP);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_CLICKABLE);
    crazypod_overlay_glass_prepare(true);
    panel = crazypod_overlay_glass_panel(
        root, geometry.x, geometry.y,
        geometry.width, geometry.height);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    y = 16;
    if(has_symbol) {
        label = crazypod_ui_widget_label(
            panel, symbol, &lv_font_montserrat_24,
            COLOR_WHITE, 155);
        lv_obj_set_width(label, geometry.width);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 0, y);
        y += 38;
    }
    label = crazypod_ui_widget_label(
        panel, title, &lv_font_montserrat_12,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, content_width);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 14, y);
    y += lv_font_get_line_height(
        &lv_font_montserrat_12) + 8;
    label = crazypod_ui_widget_label(
        panel, message, &lv_font_montserrat_8,
        COLOR_WHITE, 135);
    lv_obj_set_width(label, content_width);
    lv_obj_set_height(label, message_height);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(label, 2, 0);
    lv_obj_set_pos(label, 14, y);
}

#endif
