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
#define HOLD_FEEDBACK_TOPBAR_WIDTH 112
#define HOLD_FEEDBACK_TOPBAR_HEIGHT 20
#define HOLD_FEEDBACK_TOPBAR_INSET 2
#define HOLD_FEEDBACK_TOPBAR_Y 6

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

static void begin_hold_feedback(
    struct crazypod_hold_feedback *feedback,
    lv_obj_t *parent, const char *symbol, int duration_ms,
    int width, int height, int inset, int radius,
    const lv_font_t *font, bool topbar)
{
    const int inner_width = width - 2 * inset;
    const int max_fill_width =
        inner_width - (topbar ? 2 : 0);
    const int fill_height =
        height - 2 * inset - (topbar ? 2 : 0);
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
        parent, 0, 0,
        width, height, radius, 0x111118, 226);
    lv_obj_align(
        feedback->root,
        topbar ? LV_ALIGN_TOP_MID : LV_ALIGN_CENTER,
        0, topbar ? HOLD_FEEDBACK_TOPBAR_Y : 0);
    lv_obj_remove_flag(feedback->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(feedback->root, 1, 0);
    lv_obj_set_style_border_color(
        feedback->root, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(feedback->root, 45, 0);
    feedback->fill = crazypod_ui_widget_box(
        feedback->root,
        inset, inset,
        1, fill_height,
        radius - inset,
        topbar ? COLOR_WHITE : COLOR_PROGRESS, 78);
    label = crazypod_ui_widget_label(
        feedback->root,
        symbol != NULL ? symbol : LV_SYMBOL_BULLET,
        font,
        COLOR_WHITE, LV_OPA_COVER);
    label_height = lv_font_get_line_height(font);
    lv_obj_set_size(label, inner_width, label_height);
    lv_obj_set_pos(
        label, inset, (height - label_height) / 2);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_move_foreground(feedback->root);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, feedback->fill);
    lv_anim_set_exec_cb(&animation, fill_width_anim);
    lv_anim_set_values(&animation, 1, max_fill_width);
    lv_anim_set_duration(&animation, duration_ms);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_start(&animation);
}

void crazypod_hold_feedback_begin(
    struct crazypod_hold_feedback *feedback,
    lv_obj_t *parent, const char *symbol, int duration_ms)
{
    begin_hold_feedback(
        feedback, parent, symbol, duration_ms,
        HOLD_FEEDBACK_WIDTH, HOLD_FEEDBACK_HEIGHT,
        HOLD_FEEDBACK_INSET, 12,
        &lv_font_montserrat_12, false);
}

void crazypod_hold_feedback_begin_topbar(
    struct crazypod_hold_feedback *feedback,
    lv_obj_t *parent, const char *symbol, int duration_ms)
{
    begin_hold_feedback(
        feedback, parent, symbol, duration_ms,
        HOLD_FEEDBACK_TOPBAR_WIDTH,
        HOLD_FEEDBACK_TOPBAR_HEIGHT,
        HOLD_FEEDBACK_TOPBAR_INSET, 8,
        &lv_font_montserrat_10, true);
}

#endif
