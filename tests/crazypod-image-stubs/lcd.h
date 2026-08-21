#ifndef CRAZYPOD_IMAGE_TEST_LCD_H
#define CRAZYPOD_IMAGE_TEST_LCD_H

#include <stdint.h>

typedef uint16_t fb_data;

#define LCD_RGBPACK(r, g, b) \
    ((fb_data)((((unsigned)(r) >> 3) << 11) | \
               (((unsigned)(g) >> 2) << 5) | \
               ((unsigned)(b) >> 3)))
#define RGB_UNPACK_RED(x) \
    ((((unsigned)(x) >> 8) & 0xf8) | (((unsigned)(x) >> 13) & 0x07))
#define RGB_UNPACK_GREEN(x) \
    ((((unsigned)(x) >> 3) & 0xfc) | (((unsigned)(x) >> 9) & 0x03))
#define RGB_UNPACK_BLUE(x) \
    ((((unsigned)(x) << 3) & 0xf8) | (((unsigned)(x) >> 2) & 0x07))

#endif
