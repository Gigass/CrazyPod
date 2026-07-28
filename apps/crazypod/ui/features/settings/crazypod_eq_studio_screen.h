#ifndef CRAZYPOD_EQ_STUDIO_SCREEN_H
#define CRAZYPOD_EQ_STUDIO_SCREEN_H

#include <stdint.h>

#include "lvgl.h"

#include "crazypod_eq_studio_controller.h"

void crazypod_eq_studio_screen_render(
    lv_obj_t *parent,
    const struct crazypod_eq_studio_model *model,
    const lv_font_t *metadata_font,
    uint32_t primary_color);

#endif
