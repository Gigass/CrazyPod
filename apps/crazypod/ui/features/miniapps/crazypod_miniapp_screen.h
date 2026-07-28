#ifndef CRAZYPOD_MINIAPP_SCREEN_H
#define CRAZYPOD_MINIAPP_SCREEN_H

#include <stdint.h>

#include "lvgl.h"

void crazypod_miniapp_screen_reset(void);
void crazypod_miniapp_screen_render(lv_obj_t *parent, uint32_t accent);

#endif
