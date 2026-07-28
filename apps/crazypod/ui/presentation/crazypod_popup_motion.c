#include "crazypod_popup_motion.h"

static void popup_y_anim(void *target, int32_t value)
{
    lv_obj_set_y(target, value);
}

static void popup_opa_anim(void *target, int32_t value)
{
    lv_obj_set_style_opa(target, (lv_opa_t)value, 0);
}

void crazypod_popup_animate(lv_obj_t *panel, int target_y)
{
    lv_anim_t animation;

    lv_obj_set_y(panel, target_y + 9);
    lv_obj_set_style_opa(panel, 120, 0);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, panel);
    lv_anim_set_exec_cb(&animation, popup_y_anim);
    lv_anim_set_values(&animation, target_y + 9, target_y);
    lv_anim_set_duration(&animation, 180);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, panel);
    lv_anim_set_exec_cb(&animation, popup_opa_anim);
    lv_anim_set_values(&animation, 120, LV_OPA_COVER);
    lv_anim_set_duration(&animation, 160);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}
