#ifndef CRAZYPOD_DESKTOP_NATIVE_H
#define CRAZYPOD_DESKTOP_NATIVE_H

#include <stdbool.h>

#include "lvgl.h"

#define CRAZYPOD_DESKTOP_NATIVE_TOP 40
#define CRAZYPOD_DESKTOP_NATIVE_BOTTOM 143

void crazypod_desktop_native_reset(void);
void crazypod_desktop_native_invalidate(bool discard_backdrop);
void crazypod_desktop_native_capture_flush(const lv_area_t *area);
void crazypod_desktop_native_render(
    int left_app_index, int center_app_index, int right_app_index,
    int base_size, bool blocked);

#endif
