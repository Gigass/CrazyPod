#ifndef CRAZYPOD_LCD_H
#define CRAZYPOD_LCD_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

void crazypod_lcd_show_boot_logo(void);
const lv_image_dsc_t *crazypod_lcd_boot_logo_image(void);
void crazypod_lcd_show_panic(const char *message);
void crazypod_lcd_draw_video_controls(
    const char *title, uint32_t elapsed_seconds,
    uint32_t duration_seconds, int volume,
    bool paused, const char *message);

#endif
