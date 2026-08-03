#ifndef CRAZYPOD_RUNTIME_FONT_H
#define CRAZYPOD_RUNTIME_FONT_H

#include <stdbool.h>

#include "lvgl.h"

enum crazypod_font_family {
    CRAZYPOD_FONT_FAMILY_SYSTEM = 0,
    CRAZYPOD_FONT_FAMILY_SERIF,
    CRAZYPOD_FONT_FAMILY_MONO,
    CRAZYPOD_FONT_FAMILY_COUNT
};

enum crazypod_font_style {
    CRAZYPOD_FONT_STYLE_NORMAL = 0,
    CRAZYPOD_FONT_STYLE_ITALIC
};

bool crazypod_runtime_font_init(void);

/* Resolve one immutable Noto font instance. size is the requested RN ppem;
 * weight is 100..900; line_height is 0 for the font's natural line box. */
const lv_font_t *crazypod_runtime_font_resolve(
    enum crazypod_font_family family, unsigned size, unsigned weight,
    enum crazypod_font_style style, unsigned line_height);

const lv_font_t *crazypod_runtime_font(void);
const lv_font_t *crazypod_runtime_font_at_size(unsigned size);
bool crazypod_runtime_fonts_ready(void);
void crazypod_runtime_font_error_clear(void);
const char *crazypod_runtime_font_last_error(void);

const lv_font_t *crazypod_runtime_asset_font(
    const char *key, const char *path);
void crazypod_runtime_asset_fonts_reset(void);

#endif
