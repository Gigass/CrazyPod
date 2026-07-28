#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>

#include "../../crazypod_music.h"
#include "crazypod_artwork_widget.h"
#include "crazypod_ui_widgets.h"

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

static uint32_t artwork_color(const char *text, int variant)
{
    static const uint32_t palette[] = {
        0x8A2BE2, 0x1D78F2, 0xE5446D, 0xE4812C,
        0x13A48C, 0x5A55D6, 0xB0388E, 0x276A82
    };

    return palette[(text_hash(text) + (uint32_t)variant * 3u) %
                   (sizeof(palette) / sizeof(palette[0]))];
}

lv_obj_t *crazypod_artwork_widget_create(
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int display_size,
    const lv_image_dsc_t *descriptor, bool scale_descriptor)
{
    lv_obj_t *card = crazypod_ui_widget_box(
        parent, x, y, display_size, display_size,
        display_size > 80 ? 0 : 7,
        artwork_color(track != NULL ? track->album : "", 0),
        LV_OPA_COVER);

    lv_obj_set_style_bg_grad_color(
        card, lv_color_hex(artwork_color(
            track != NULL ? track->artist : "", 1)), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_shadow_width(card, display_size > 80 ? 12 : 5, 0);
    lv_obj_set_style_shadow_offset_y(card, display_size > 80 ? 6 : 2, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, display_size > 80 ? 100 : 55, 0);
    lv_obj_set_style_clip_corner(card, true, 0);

    if(descriptor != NULL) {
        lv_obj_t *image = lv_image_create(card);

        lv_image_set_src(image, descriptor);
        if(scale_descriptor && descriptor->header.w != display_size)
            lv_image_set_scale(
                image, (uint32_t)display_size * LV_SCALE_NONE /
                    descriptor->header.w);
        lv_obj_center(image);
    }
    else {
        lv_obj_t *symbol = crazypod_ui_widget_label(
            card, LV_SYMBOL_AUDIO,
            display_size > 80
                ? &lv_font_montserrat_24
                : &lv_font_montserrat_16,
            0xFFFFFF, 210);

        lv_obj_center(symbol);
    }
    return card;
}

#endif
