#ifndef CRAZYPOD_LCD_H
#define CRAZYPOD_LCD_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

void crazypod_lcd_show_boot_logo(void);
const lv_image_dsc_t *crazypod_lcd_boot_logo_image(void);
void crazypod_lcd_show_panic(const char *message);
/* Native overlays use the same localized glyph renderer as video. */
void crazypod_lcd_draw_text(const char *text, int x, int y,
                            int maximum_x, uint32_t color);
void crazypod_lcd_draw_video_frame(
    unsigned char * const source[3],
    int source_x, int source_y, int stride,
    int destination_x, int destination_y,
    int width, int height,
    bool controls_visible, const char *title,
    uint32_t elapsed_seconds, uint32_t duration_seconds,
    int volume, bool paused, const char *message);

#endif
