#ifndef CRAZYPOD_ARTWORK_PALETTE_H
#define CRAZYPOD_ARTWORK_PALETTE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

struct crazypod_artwork_palette {
    uint32_t primary;
    uint32_t secondary;
    uint32_t highlight;
};

void crazypod_artwork_palette_fallback(
    struct crazypod_artwork_palette *palette,
    uint32_t primary, uint32_t secondary);
bool crazypod_artwork_palette_extract(
    const lv_image_dsc_t *artwork,
    struct crazypod_artwork_palette *palette);

#endif
