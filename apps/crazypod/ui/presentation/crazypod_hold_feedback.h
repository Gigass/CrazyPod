#ifndef CRAZYPOD_HOLD_FEEDBACK_H
#define CRAZYPOD_HOLD_FEEDBACK_H

#include "lvgl.h"

struct crazypod_hold_feedback {
    lv_obj_t *root;
    lv_obj_t *fill;
};

void crazypod_hold_feedback_reset(
    struct crazypod_hold_feedback *feedback);
void crazypod_hold_feedback_begin(
    struct crazypod_hold_feedback *feedback,
    lv_obj_t *parent, const char *symbol, int duration_ms);
void crazypod_hold_feedback_begin_topbar(
    struct crazypod_hold_feedback *feedback,
    lv_obj_t *parent, const char *symbol, int duration_ms);
void crazypod_hold_feedback_dismiss(
    struct crazypod_hold_feedback *feedback);

#endif
