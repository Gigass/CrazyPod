#ifndef CRAZYPOD_MINIAPP_SCREEN_H
#define CRAZYPOD_MINIAPP_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

void crazypod_miniapp_screen_reset(void);
bool crazypod_miniapp_screen_attached(lv_obj_t *parent);
void crazypod_miniapp_screen_render(lv_obj_t *parent, uint32_t accent);

#endif
