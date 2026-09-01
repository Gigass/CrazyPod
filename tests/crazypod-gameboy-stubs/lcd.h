#ifndef CRAZYPOD_GAMEBOY_TEST_LCD_H
#define CRAZYPOD_GAMEBOY_TEST_LCD_H
#include <stdint.h>
typedef uint16_t fb_data;
#define FB_RGBPACK_LCD(r, g, b) (((r) << 11) | ((g) << 5) | (b))
#endif
