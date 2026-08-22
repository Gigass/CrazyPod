#include "config.h"

#ifdef IPOD_6G

#include "lvgl.h"

#include "../../crazypod_state.h"
#include "../presentation/crazypod_glass_slots.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_notification.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_INFO 0x58C7FA
#define COLOR_SUCCESS 0x32D583
#define COLOR_ERROR 0xFF5A67
#define NOTIFICATION_X 45
#define NOTIFICATION_Y 176
#define NOTIFICATION_WIDTH 230
#define NOTIFICATION_HEIGHT 46
#define NOTIFICATION_RADIUS 14
#define NOTIFICATION_SHADOW_MARGIN 14
#define NOTIFICATION_DURATION_MS 1700
#define ERROR_DURATION_MS 2400
#define ENTER_DURATION_MS 150
#define EXIT_DURATION_MS 140
#define FLASH_DURATION_MS 140
#define FLASH_OPACITY 108

static lv_obj_t *notification;
static lv_obj_t *flash;

static void set_object_opacity(void *target, int32_t value)
{
    lv_obj_set_style_opa(target, (lv_opa_t)value, 0);
}

static void set_object_y(void *target, int32_t value)
{
    lv_obj_set_y(target, value);
}

static void set_flash_opacity(void *target, int32_t value)
{
    lv_obj_set_style_bg_opa(target, (lv_opa_t)value, 0);
}

static uint32_t notification_color(
    enum crazypod_notification_kind kind)
{
    if(kind == CRAZYPOD_NOTIFICATION_SUCCESS)
        return COLOR_SUCCESS;
    if(kind == CRAZYPOD_NOTIFICATION_ERROR)
        return COLOR_ERROR;
    return COLOR_INFO;
}

static const char *notification_symbol(
    enum crazypod_notification_kind kind)
{
    if(kind == CRAZYPOD_NOTIFICATION_SUCCESS)
        return LV_SYMBOL_OK;
    if(kind == CRAZYPOD_NOTIFICATION_ERROR)
        return LV_SYMBOL_CLOSE;
    return LV_SYMBOL_BELL;
}

void crazypod_notification_dismiss(void)
{
    if(notification == NULL)
        return;
    lv_anim_delete(notification, NULL);
    lv_obj_delete(notification);
}

void crazypod_notification_show_for(
    enum crazypod_notification_kind kind,
    const char *message, uint32_t duration_ms)
{
    uint32_t accent;
    lv_obj_t *icon_background;
    lv_obj_t *icon;
    lv_obj_t *label;
    const lv_font_t *font;
    int line_height;

    if(message == NULL || message[0] == '\0')
        return;
    if(duration_ms < EXIT_DURATION_MS + 100)
        duration_ms = EXIT_DURATION_MS + 100;
    crazypod_notification_dismiss();
    accent = notification_color(kind);
    notification = crazypod_glass_slot_panel(
        CRAZYPOD_GLASS_SLOT_INFO_TOAST,
        crazypod_glass_slot_prepare_frame(
            CRAZYPOD_GLASS_SLOT_INFO_TOAST,
            NOTIFICATION_X, NOTIFICATION_Y,
            NOTIFICATION_WIDTH, NOTIFICATION_HEIGHT,
            CRAZYPOD_GLASS_INFO_TOAST),
        lv_layer_top(),
        NOTIFICATION_X, NOTIFICATION_Y,
        NOTIFICATION_WIDTH, NOTIFICATION_HEIGHT,
        NOTIFICATION_RADIUS, CRAZYPOD_GLASS_INFO_TOAST);
    lv_obj_null_on_delete(&notification);
    lv_obj_remove_flag(notification, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(notification);

    icon_background = crazypod_ui_widget_box(
        notification, 10, 10, 26, 26,
        LV_RADIUS_CIRCLE, accent, 34);
    lv_obj_remove_flag(icon_background, LV_OBJ_FLAG_CLICKABLE);
    icon = crazypod_ui_widget_label(
        icon_background, notification_symbol(kind),
        &lv_font_montserrat_12, accent, LV_OPA_COVER);
    lv_obj_center(icon);

    label = crazypod_ui_widget_label(
        notification, message, &lv_font_montserrat_10,
        COLOR_WHITE, LV_OPA_COVER);
    font = lv_obj_get_style_text_font(label, 0);
    line_height = lv_font_get_line_height(font);
    lv_obj_set_pos(label, 46, (NOTIFICATION_HEIGHT - line_height) / 2);
    lv_obj_set_size(label, NOTIFICATION_WIDTH - 58, line_height);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);

    if(crazypod_state_reduce_motion()) {
        lv_obj_delete_delayed(notification, duration_ms);
    }
    else {
        lv_anim_t animation;

        lv_obj_set_y(notification, NOTIFICATION_Y + 7);
        lv_obj_set_style_opa(notification, LV_OPA_TRANSP, 0);

        lv_anim_init(&animation);
        lv_anim_set_var(&animation, notification);
        lv_anim_set_exec_cb(&animation, set_object_y);
        lv_anim_set_values(
            &animation, NOTIFICATION_Y + 7, NOTIFICATION_Y);
        lv_anim_set_duration(&animation, ENTER_DURATION_MS);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);

        lv_anim_init(&animation);
        lv_anim_set_var(&animation, notification);
        lv_anim_set_exec_cb(&animation, set_object_opacity);
        lv_anim_set_values(
            &animation, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&animation, ENTER_DURATION_MS);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);

        lv_anim_init(&animation);
        lv_anim_set_var(&animation, notification);
        lv_anim_set_exec_cb(&animation, set_object_opacity);
        lv_anim_set_values(
            &animation, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_delay(
            &animation, duration_ms - EXIT_DURATION_MS);
        lv_anim_set_duration(&animation, EXIT_DURATION_MS);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
        lv_anim_set_completed_cb(
            &animation, lv_obj_delete_anim_completed_cb);
        lv_anim_start(&animation);
    }
}

void crazypod_notification_show(
    enum crazypod_notification_kind kind,
    const char *message)
{
    crazypod_notification_show_for(
        kind, message,
        kind == CRAZYPOD_NOTIFICATION_ERROR
            ? ERROR_DURATION_MS : NOTIFICATION_DURATION_MS);
}

void crazypod_notification_flash(void)
{
    lv_anim_t animation;

    if(flash != NULL) {
        lv_anim_delete(flash, NULL);
        lv_obj_delete(flash);
    }
    flash = crazypod_ui_widget_box(
        lv_layer_top(), 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, COLOR_WHITE, FLASH_OPACITY);
    lv_obj_null_on_delete(&flash);
    lv_obj_remove_flag(flash, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(flash);
    if(crazypod_state_reduce_motion()) {
        lv_obj_delete_delayed(flash, 70);
        return;
    }
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, flash);
    lv_anim_set_exec_cb(&animation, set_flash_opacity);
    lv_anim_set_values(
        &animation, FLASH_OPACITY, LV_OPA_TRANSP);
    lv_anim_set_duration(&animation, FLASH_DURATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(
        &animation, lv_obj_delete_anim_completed_cb);
    lv_anim_start(&animation);
}

bool crazypod_notification_bounds(
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
    if(notification == NULL)
        return false;
    lv_obj_get_coords(notification, &area);
    *left = area.x1 - NOTIFICATION_SHADOW_MARGIN;
    *top = area.y1 - NOTIFICATION_SHADOW_MARGIN;
    *right = area.x2 + NOTIFICATION_SHADOW_MARGIN + 1;
    *bottom = area.y2 + NOTIFICATION_SHADOW_MARGIN + 1;
    return true;
}

#endif
