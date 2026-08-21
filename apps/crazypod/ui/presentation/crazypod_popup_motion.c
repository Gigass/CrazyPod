#include "crazypod_popup_motion.h"

void crazypod_popup_animate(lv_obj_t *panel, int target_y)
{
    /* Centered popups are anchored; moving them changes their final frame. */
    lv_obj_set_y(panel, target_y);
    lv_obj_set_style_opa(panel, LV_OPA_COVER, 0);
}
