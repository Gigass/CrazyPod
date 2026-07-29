#include "config.h"

#ifdef IPOD_6G

#include "crazypod_empty_state.h"
#include "crazypod_overlay_glass.h"
#include "crazypod_ui_widgets.h"

#define COLOR_WHITE 0xFFFFFF
#define EMPTY_POPUP_X 35
#define EMPTY_POPUP_Y 32
#define EMPTY_POPUP_WIDTH 250
#define EMPTY_POPUP_HEIGHT 176

void crazypod_empty_state_render(
    lv_obj_t *parent, const char *symbol,
    const char *title, const char *message)
{
    bool has_symbol = symbol != NULL && symbol[0] != '\0';
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *label;

    if(parent == NULL || title == NULL || message == NULL)
        return;
    root = crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, 36);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_CLICKABLE);
    crazypod_overlay_glass_prepare(true);
    panel = crazypod_overlay_glass_panel(
        root, EMPTY_POPUP_X, EMPTY_POPUP_Y,
        EMPTY_POPUP_WIDTH, EMPTY_POPUP_HEIGHT);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    if(has_symbol) {
        label = crazypod_ui_widget_label(
            panel, symbol, &lv_font_montserrat_24,
            COLOR_WHITE, 155);
        lv_obj_set_width(label, EMPTY_POPUP_WIDTH);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 0, 30);
    }
    label = crazypod_ui_widget_label(
        panel, title, &lv_font_montserrat_12,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, EMPTY_POPUP_WIDTH - 28);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 14, has_symbol ? 78 : 63);
    label = crazypod_ui_widget_label(
        panel, message, &lv_font_montserrat_8,
        COLOR_WHITE, 135);
    lv_obj_set_width(label, EMPTY_POPUP_WIDTH - 28);
    lv_obj_set_height(label, 24);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_pos(label, 14, has_symbol ? 104 : 89);
}

#endif
