#ifndef CRAZYPOD_PLATFORM_DISPLAY_H
#define CRAZYPOD_PLATFORM_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

struct crazypod_platform_display_host {
    bool (*capture_desktop_native)(const lv_area_t *area);
    void (*capture_flush)(const lv_area_t *area);
    bool (*coverflow_active)(void);
    void (*coverflow_invalidate)(void);
    void (*queue_present)(
        int x, int y, int width, int height);
};

lv_display_t *crazypod_platform_display_init(
    uint32_t (*tick_ms)(void),
    const struct crazypod_platform_display_host *host);
void *crazypod_platform_display_framebuffer(void);

#endif
