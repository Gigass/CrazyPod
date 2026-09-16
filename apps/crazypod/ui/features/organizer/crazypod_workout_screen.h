#ifndef CRAZYPOD_WORKOUT_SCREEN_H
#define CRAZYPOD_WORKOUT_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

void crazypod_workout_screen_render_ready(
    lv_obj_t *content, int activity);
/* True when the live workout could be ticked in place, so the caller
 * does not have to rebuild the pane for a changed second. */
bool crazypod_workout_screen_refresh_active(
    int activity, bool running, uint32_t seconds);
void crazypod_workout_screen_forget(void);
void crazypod_workout_screen_render_active(
    lv_obj_t *content, int activity, bool running, uint32_t seconds);
void crazypod_workout_screen_render_summary(lv_obj_t *content);
void crazypod_workout_screen_render_detail(
    lv_obj_t *content, int workout_index);

#endif
