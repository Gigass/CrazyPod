#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "crazypod_hold_feedback.h"
#include "crazypod_ui_widgets.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_PROGRESS 0x47E69A
#define HOLD_FEEDBACK_WIDTH 200
#define HOLD_FEEDBACK_HEIGHT 34
#define HOLD_FEEDBACK_INSET 3

static void fill_width_anim(void *target, int32_t value)
{
    lv_obj_set_width(target, value);
}

void crazypod_hold_feedback_reset(
    struct crazypod_hold_feedback *feedback)
{
    if(feedback != NULL)
        memset(feedback, 0, sizeof(*feedback));
}

void crazypod_hold_feedback_dismiss(
    struct crazypod_hold_feedback *feedback)
{
    if(feedback == NULL)
        return;
    if(feedback->fill != NULL &&
       lv_obj_is_valid(feedback->fill))
        lv_anim_delete(feedback->fill, fill_width_anim);
    if(feedback->root != NULL &&
       lv_obj_is_valid(feedback->root))
        lv_obj_delete(feedback->root);
    crazypod_hold_feedback_reset(feedback);
}

void crazypod_hold_feedback_begin(
    struct crazypod_hold_feedback *feedback,
    lv_obj_t *parent, const char *symbol, int duration_ms)
{
    const int inner_width =
        HOLD_FEEDBACK_WIDTH - 2 * HOLD_FEEDBACK_INSET;
    lv_obj_t *label;
    lv_anim_t animation;
    int label_height;

    if(feedback == NULL || parent == NULL ||
       !lv_obj_is_valid(parent))
        return;
    crazypod_hold_feedback_dismiss(feedback);
    if(duration_ms < 1)
        duration_ms = 1;
    feedback->root = crazypod_ui_widget_box(
        parent,
        (LCD_WIDTH - HOLD_FEEDBACK_WIDTH) / 2,
        LCD_HEIGHT - HOLD_FEEDBACK_HEIGHT - 12,
        HOLD_FEEDBACK_WIDTH, HOLD_FEEDBACK_HEIGHT,
        12, 0x111118, 226);
    lv_obj_remove_flag(feedback->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(feedback->root, 1, 0);
    lv_obj_set_style_border_color(
        feedback->root, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(feedback->root, 45, 0);
    feedback->fill = crazypod_ui_widget_box(
        feedback->root,
        HOLD_FEEDBACK_INSET, HOLD_FEEDBACK_INSET,
        1, HOLD_FEEDBACK_HEIGHT - 2 * HOLD_FEEDBACK_INSET,
        9, COLOR_PROGRESS, 78);
    label = crazypod_ui_widget_label(
        feedback->root,
        symbol != NULL ? symbol : LV_SYMBOL_BULLET,
        &lv_font_montserrat_12,
        COLOR_WHITE, LV_OPA_COVER);
    label_height = lv_font_get_line_height(
        &lv_font_montserrat_12);
    lv_obj_set_size(label, inner_width, label_height);
    lv_obj_set_pos(
        label, HOLD_FEEDBACK_INSET,
        (HOLD_FEEDBACK_HEIGHT - label_height) / 2);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_move_foreground(feedback->root);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, feedback->fill);
    lv_anim_set_exec_cb(&animation, fill_width_anim);
    lv_anim_set_values(&animation, 1, inner_width);
    lv_anim_set_duration(&animation, duration_ms);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_start(&animation);
}

#endif
