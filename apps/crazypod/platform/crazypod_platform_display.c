#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "lcd.h"
#include "system.h"

#include "lvgl.h"

#include "crazypod_platform_display.h"

#define DRAW_ROWS 40

static struct crazypod_platform_display_host display_host;
static fb_data draw_buffer[LCD_WIDTH * DRAW_ROWS]
    CACHEALIGN_AT_LEAST_ATTR(16);

extern struct frame_buffer_t lcd_framebuffer_default;

void *crazypod_platform_display_framebuffer(void)
{
    return lcd_framebuffer_default.data;
}

static void display_flush(
    lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    static bool dirty_valid;
    static int dirty_x1;
    static int dirty_y1;
    static int dirty_x2;
    static int dirty_y2;
    fb_data *destination;
    const fb_data *source = (const fb_data *)pixels;
    const lv_draw_buf_t *active_buffer =
        lv_display_get_buf_active(display);
    int source_stride =
        active_buffer->header.stride / sizeof(fb_data);
    int x = area->x1;
    int y = area->y1;
    int width = area->x2 - area->x1 + 1;
    int height = area->y2 - area->y1 + 1;
    int row;

    destination = (fb_data *)lcd_framebuffer_default.data +
                  y * LCD_WIDTH + x;
    for(row = 0; row < height; ++row) {
        memcpy(destination, source,
               (size_t)width * sizeof(fb_data));
        destination += LCD_WIDTH;
        source += source_stride;
    }
    if(display_host.capture_desktop_native != NULL &&
       display_host.capture_desktop_native(area) &&
       display_host.capture_flush != NULL)
        display_host.capture_flush(area);

    if(!dirty_valid) {
        dirty_x1 = area->x1;
        dirty_y1 = area->y1;
        dirty_x2 = area->x2;
        dirty_y2 = area->y2;
        dirty_valid = true;
    }
    else {
        if(area->x1 < dirty_x1)
            dirty_x1 = area->x1;
        if(area->y1 < dirty_y1)
            dirty_y1 = area->y1;
        if(area->x2 > dirty_x2)
            dirty_x2 = area->x2;
        if(area->y2 > dirty_y2)
            dirty_y2 = area->y2;
    }
    if(lv_display_flush_is_last(display)) {
        if(display_host.coverflow_active != NULL &&
           display_host.coverflow_active()) {
            if(display_host.coverflow_invalidate != NULL)
                display_host.coverflow_invalidate();
        }
        else if(display_host.queue_present != NULL) {
            display_host.queue_present(
                dirty_x1, dirty_y1,
                dirty_x2 - dirty_x1 + 1,
                dirty_y2 - dirty_y1 + 1);
        }
        dirty_valid = false;
    }
    lv_display_flush_ready(display);
}

lv_display_t *crazypod_platform_display_init(
    uint32_t (*tick_ms)(void),
    const struct crazypod_platform_display_host *host)
{
    lv_display_t *display;

    memset(&display_host, 0, sizeof(display_host));
    if(host != NULL)
        display_host = *host;
    lv_init();
    lv_tick_set_cb(tick_ms);
    display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(
        display, draw_buffer, NULL, sizeof(draw_buffer),
        LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, display_flush);
    lv_display_set_antialiasing(display, true);
    return display;
}

#endif
