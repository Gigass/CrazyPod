#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>

#include "lvgl.h"

#include "../../../crazypod_book_cover.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "../photos/crazypod_photos_feature.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "crazypod_book_preview_cover.h"

static lv_obj_t *make_box(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_box(
        parent, x, y, width, height, radius, color, opacity);
}

static lv_obj_t *make_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_label(
        parent, text, font, color, opacity);
}

static uint32_t text_hash(const char *text)
{
    uint32_t hash = 2166136261u;

    if(text == NULL)
        return hash;
    while(*text != '\0') {
        hash ^= (unsigned char)*text++;
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t artwork_color(const char *text)
{
    static const uint32_t palette[] = {
        0x8A2BE2, 0x1D78F2, 0xE5446D, 0xE4812C,
        0x13A48C, 0x5A55D6, 0xB0388E, 0x276A82
    };

    return palette[text_hash(text) %
                   (sizeof(palette) / sizeof(palette[0]))];
}

lv_obj_t *crazypod_book_preview_cover_create(
    lv_obj_t *parent, const struct crazypod_book *book,
    int x, int y, int width, int height)
{
    int book_index = crazypod_book_index(book);
    const lv_image_dsc_t *image =
        book_index >= 0 && width >= 50
            ? crazypod_book_cover_get(
                  book_index, width, height) : NULL;
    uint32_t color = book != NULL
        ? artwork_color(book->path) : 0x70462A;
    lv_obj_t *cover = make_box(
        parent, x, y, width, height, 4,
        image != NULL ? 0x090806 : color, LV_OPA_COVER);
    lv_obj_t *label;
    int spine_width = width > 50 ? 7 : 4;

    if(image != NULL) {
        crazypod_photos_feature_render_image(
            cover, image, 0, 0, width, height);
        lv_obj_set_style_border_width(cover, 1, 0);
        lv_obj_set_style_border_color(
            cover, lv_color_hex(0xD8D0C2), 0);
        lv_obj_set_style_border_opa(cover, 105, 0);
        crazypod_preview_add_bevel(
            cover, width, height, 0xFFFFFF, 0x090604);
        return cover;
    }

    lv_obj_set_style_bg_grad_color(
        cover, lv_color_hex((color & 0xFEFEFEu) >> 1), 0);
    lv_obj_set_style_bg_grad_dir(cover, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(cover, 1, 0);
    lv_obj_set_style_border_color(
        cover, lv_color_hex(0xE0C48D), 0);
    lv_obj_set_style_border_opa(cover, 125, 0);
    crazypod_preview_add_bevel(
        cover, width, height, 0xF6E5BC, 0x160C08);
    make_box(cover, spine_width, 5, 2, height - 10, 1,
             0x1A1010, 82);
    make_box(cover, spine_width + 3, 5, 1, height - 10, 0,
             0xF3E2B5, 48);
    if(width - spine_width >= 18) {
        make_box(cover, spine_width + 7, 13,
                 width - spine_width - 12, 2, 1,
                 0xF3E2B5, 145);
        make_box(cover, spine_width + 7, 20,
                 width - spine_width - 14, 1, 0,
                 0xF3E2B5, 92);
    }
    if(width > 26)
        make_box(cover, width - 4, 7, 2, height - 14, 0,
                 0xF4E8CB, 115);
    if(width >= 60) {
        label = make_label(
            cover,
            book != NULL && book->title[0] != '\0'
                ? book->title : "BOOK",
            &lv_font_montserrat_8,
            0xF7E7BE, 205);
        lv_obj_set_pos(label, spine_width + 7, height / 2 - 4);
        lv_obj_set_width(label, width - spine_width - 13);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    }
    return cover;
}



#endif
