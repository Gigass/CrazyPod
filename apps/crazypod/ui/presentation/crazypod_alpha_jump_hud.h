#ifndef CRAZYPOD_ALPHA_JUMP_HUD_H
#define CRAZYPOD_ALPHA_JUMP_HUD_H

#include <stdbool.h>

#include "lvgl.h"

void crazypod_alpha_jump_hud_configure(lv_obj_t *parent);
void crazypod_alpha_jump_hud_show(
    char key, long now, long duration_ticks);
void crazypod_alpha_jump_hud_tick(long now, bool active);
void crazypod_alpha_jump_hud_reset(void);

#endif
