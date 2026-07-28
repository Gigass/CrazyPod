#ifndef CRAZYPOD_GLASS_SAMPLER_H
#define CRAZYPOD_GLASS_SAMPLER_H

#include <stdbool.h>
#include <stdint.h>

#include "lcd.h"
#include "lvgl.h"

enum crazypod_glass_material {
    CRAZYPOD_GLASS_POPUP = 0,
    CRAZYPOD_GLASS_MENU_PANEL,
    CRAZYPOD_GLASS_MENU_TOPBAR,
    CRAZYPOD_GLASS_TEXT_PANEL,
    CRAZYPOD_GLASS_HOME_CAPSULE,
    CRAZYPOD_GLASS_INFO_TOAST,
};

typedef void (*crazypod_glass_boost_callback)(int ticks);

uint32_t crazypod_glass_material_tint(
    enum crazypod_glass_material material);
lv_opa_t crazypod_glass_material_tint_opa(
    enum crazypod_glass_material material);
lv_opa_t crazypod_glass_material_border_opa(
    enum crazypod_glass_material material);
lv_opa_t crazypod_glass_material_shadow_opa(
    enum crazypod_glass_material material);
bool crazypod_glass_render_descriptor(
    const fb_data *source, int source_width, int source_height,
    int source_stride, int source_x, int source_y,
    int width, int height, enum crazypod_glass_material material,
    fb_data *render_pixels, lv_image_dsc_t *descriptor,
    crazypod_glass_boost_callback boost);

#endif
