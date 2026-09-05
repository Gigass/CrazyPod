#ifndef CRAZYPOD_FONT_TEST_LVGL_H
#define CRAZYPOD_FONT_TEST_LVGL_H
#include <stdbool.h>
#include <stdint.h>
#define LV_FONT_SUBPX_NONE 0
#define LV_FONT_KERNING_NONE 0
#define LV_FONT_GLYPH_FORMAT_A8 0
typedef struct lv_font_t lv_font_t;
typedef struct {
    const lv_font_t *resolved_font;
    unsigned adv_w, box_w, box_h, format;
    union { uint32_t index; } gid;
} lv_font_glyph_dsc_t;
typedef struct { struct { unsigned stride; } header; uint8_t *data; }
    lv_draw_buf_t;
struct lv_font_t {
    bool (*get_glyph_dsc)(const lv_font_t *, lv_font_glyph_dsc_t *,
                          uint32_t, uint32_t);
    const void *(*get_glyph_bitmap)(lv_font_glyph_dsc_t *, lv_draw_buf_t *);
    void *dsc;
    const lv_font_t *fallback;
    int32_t line_height, base_line;
    int subpx, kerning;
};
#endif
