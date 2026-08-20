#ifndef CRAZYPOD_DESKTOP_NATIVE_H
#define CRAZYPOD_DESKTOP_NATIVE_H

#include <stdbool.h>

#include "lvgl.h"

#define CRAZYPOD_DESKTOP_NATIVE_TOP 40
#define CRAZYPOD_DESKTOP_NATIVE_BOTTOM 143
#define CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE 5

void crazypod_desktop_native_reset(void);
void crazypod_desktop_native_invalidate(bool discard_backdrop);
void crazypod_desktop_native_invalidate_icons(void);
lv_obj_t *crazypod_desktop_native_create_modal_underlay(
    lv_obj_t *parent);
void crazypod_desktop_native_preserve_modal_underlay(void);
void crazypod_desktop_native_capture_flush(const lv_area_t *area);
void crazypod_desktop_native_render(
    const int *app_indices, const int *centers_x, int icon_count,
    int icon_size, bool blocked);

#endif
