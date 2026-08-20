#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "lvgl.h"

#include "../presentation/crazypod_popup_layout.h"
#include "../presentation/crazypod_popup_motion.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_screenshot_feedback.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_PANEL 0x101116
#define COLOR_SUCCESS 0x32D583
#define COLOR_FAILURE 0xFF5A67
#define FLASH_DURATION_MS 180
#define PROMPT_DURATION_MS 1500
#define PROMPT_Y 42
#define PROMPT_SHADOW_MARGIN 14

static lv_obj_t *prompt;
static lv_obj_t *flash;

static void flash_opacity(void *target, int32_t value)
{
    lv_obj_set_style_bg_opa(target, (lv_opa_t)value, 0);
}

static void start_flash(void)
{
    if(flash != NULL) {
        lv_anim_delete(flash, NULL);
        lv_obj_delete(flash);
    }
    flash = crazypod_ui_widget_box(
        lv_layer_top(), 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, COLOR_WHITE, 178);
    lv_anim_t animation;

    lv_obj_null_on_delete(&flash);
    lv_obj_remove_flag(flash, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(flash);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, flash);
    lv_anim_set_exec_cb(&animation, flash_opacity);
    lv_anim_set_values(&animation, 178, LV_OPA_TRANSP);
    lv_anim_set_duration(&animation, FLASH_DURATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(
        &animation, lv_obj_delete_anim_completed_cb);
    lv_anim_start(&animation);
}

static lv_obj_t *create_prompt(bool saved)
{
    uint32_t accent = saved ? COLOR_SUCCESS : COLOR_FAILURE;
    const char *message = saved
        ? CP_TR("Saved to Photos")
        : CP_TR("Screenshot failed");
    int text_width = crazypod_popup_text_width(
        message, &lv_font_montserrat_10);
    int height = lv_font_get_line_height(
        &lv_font_montserrat_12) + 26;
    struct crazypod_popup_geometry geometry =
        crazypod_popup_centered_geometry(
            crazypod_popup_clamp_width(
                text_width + 26, 14,
                148, LCD_WIDTH - 32),
            height);
    lv_obj_t *panel = crazypod_ui_widget_box(
        lv_layer_top(), geometry.x, PROMPT_Y,
        geometry.width, geometry.height, 13,
        COLOR_PANEL, 244);
    lv_obj_t *icon;
    lv_obj_t *label;

    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(
        panel, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(panel, 45, 0);
    lv_obj_set_style_shadow_width(panel, 14, 0);
    lv_obj_set_style_shadow_offset_y(panel, 5, 0);
    lv_obj_set_style_shadow_color(
        panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(panel, 125, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    icon = crazypod_ui_widget_label(
        panel, saved ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE,
        &lv_font_montserrat_12, accent, LV_OPA_COVER);
    lv_obj_set_pos(
        icon, 14,
        (geometry.height -
         lv_font_get_line_height(&lv_font_montserrat_12)) / 2);
    label = crazypod_ui_widget_label(
        panel, message,
        &lv_font_montserrat_10, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, geometry.width - 54);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(
        label, 40,
        (geometry.height -
         lv_font_get_line_height(&lv_font_montserrat_10)) / 2);
    return panel;
}

void crazypod_screenshot_feedback_show(bool saved)
{
    if(prompt != NULL) {
        lv_anim_delete(prompt, NULL);
        lv_obj_delete(prompt);
    }

    start_flash();
    prompt = create_prompt(saved);
    lv_obj_null_on_delete(&prompt);
    lv_obj_move_foreground(prompt);
    crazypod_popup_animate(prompt, lv_obj_get_y(prompt));
    lv_obj_delete_delayed(prompt, PROMPT_DURATION_MS);
}

bool crazypod_screenshot_feedback_bounds(
    int *left, int *top, int *right, int *bottom)
{
    lv_area_t area;

    if(left == NULL || top == NULL || right == NULL || bottom == NULL)
        return false;
    if(flash != NULL) {
        *left = 0;
        *top = 0;
        *right = LCD_WIDTH;
        *bottom = LCD_HEIGHT;
        return true;
    }
    if(prompt == NULL)
        return false;
    lv_obj_get_coords(prompt, &area);
    *left = area.x1 - PROMPT_SHADOW_MARGIN;
    *top = area.y1 - PROMPT_SHADOW_MARGIN;
    *right = area.x2 + PROMPT_SHADOW_MARGIN + 1;
    *bottom = area.y2 + PROMPT_SHADOW_MARGIN + 1;
    return true;
}

#endif
