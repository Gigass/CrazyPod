#ifndef CRAZYPOD_BOOT_LOGO_H
#define CRAZYPOD_BOOT_LOGO_H

#include "lcd.h"

#define CRAZYPOD_BOOT_LOGO_WIDTH 33
#define CRAZYPOD_BOOT_LOGO_HEIGHT 40

const fb_data *crazypod_boot_logo_pixels(void);
void crazypod_boot_logo_draw(fb_data *framebuffer,
                             int framebuffer_width,
                             int framebuffer_height,
                             int framebuffer_stride);

#endif
