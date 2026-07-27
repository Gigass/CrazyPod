#include "config.h"

#ifdef IPOD_6G

#include "crazypod_miniapp_font.h"

/*
 * The packaged Montserrat subset omits the five non-ASCII calculator
 * symbols. Keep this font deliberately small and fall back to Montserrat for
 * every other label glyph.
 */
static const uint8_t symbol_bitmap[] = {
    /* U+00B1 "±" */
    0x08, 0x04, 0x1f, 0xc1, 0x00, 0x80, 0x01, 0xfc, 0x00, 0x00, 0x00,

    /* U+00D7 "×" */
    0x80, 0xa0, 0x88, 0x82, 0x80, 0x80, 0xa0, 0x88, 0x82, 0x80, 0x80,

    /* U+00F7 "÷" */
    0x08, 0x04, 0x00, 0x0f, 0xe0, 0x00, 0x40, 0x20, 0x00, 0x00, 0x00,

    /* U+2212 "−" */
    0x00, 0x00, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00,

    /* U+232B "⌫" */
    0x3f, 0xa0, 0x64, 0xb1, 0x19, 0x2a, 0x04, 0xfe, 0x00, 0x00, 0x00
};

static const lv_font_fmt_txt_glyph_dsc_t symbol_glyphs[] = {
    { .bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0,
      .ofs_x = 0, .ofs_y = 0 },
    { .bitmap_index = 0, .adv_w = 160, .box_w = 9, .box_h = 9,
      .ofs_x = 0, .ofs_y = 0 },
    { .bitmap_index = 11, .adv_w = 160, .box_w = 9, .box_h = 9,
      .ofs_x = 0, .ofs_y = 0 },
    { .bitmap_index = 22, .adv_w = 160, .box_w = 9, .box_h = 9,
      .ofs_x = 0, .ofs_y = 0 },
    { .bitmap_index = 33, .adv_w = 160, .box_w = 9, .box_h = 9,
      .ofs_x = 0, .ofs_y = 0 },
    { .bitmap_index = 44, .adv_w = 160, .box_w = 9, .box_h = 9,
      .ofs_x = 0, .ofs_y = 0 }
};

static const uint16_t symbol_codepoints[] = {
    0x0000, 0x0026, 0x0046, 0x2161, 0x227a
};

static const lv_font_fmt_txt_cmap_t symbol_cmaps[] = {
    {
        .range_start = 0x00b1,
        .range_length = 0x227b,
        .glyph_id_start = 1,
        .unicode_list = symbol_codepoints,
        .glyph_id_ofs_list = NULL,
        .list_length = 5,
        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};

static const lv_font_fmt_txt_dsc_t symbol_font_dsc = {
    .glyph_bitmap = symbol_bitmap,
    .glyph_dsc = symbol_glyphs,
    .cmaps = symbol_cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = LV_FONT_FMT_TXT_PLAIN,
    .stride = 0
};

const lv_font_t crazypod_miniapp_symbol_font = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 11,
    .base_line = 2,
    .subpx = LV_FONT_SUBPX_NONE,
    .kerning = LV_FONT_KERNING_NORMAL,
    .static_bitmap = 0,
    .underline_position = -1,
    .underline_thickness = 1,
    .dsc = &symbol_font_dsc,
    .fallback = &lv_font_montserrat_10
};

#endif
