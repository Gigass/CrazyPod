#ifndef CRAZYPOD_DESKTOP_NATIVE_H
#define CRAZYPOD_DESKTOP_NATIVE_H

#include <stdbool.h>

#include "lvgl.h"

void crazypod_desktop_native_reset(void);
void crazypod_desktop_native_invalidate(bool discard_backdrop);
void crazypod_desktop_native_capture_flush(const lv_area_t *area);
void crazypod_desktop_native_render(
    int position_q8, int base_size, bool blocked);

#endif
