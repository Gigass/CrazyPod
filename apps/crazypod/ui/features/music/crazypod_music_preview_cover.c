#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>

#include "lvgl.h"

#include "../../../crazypod_artwork.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "crazypod_music_preview_cover.h"

#define COLOR_WHITE 0xFFFFFF
#define CRAZYPOD_MENU_ARTWORK_PRIORITY 20

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

static uint32_t artwork_color(const char *text, int variant)
{
    static const uint32_t palette[] = {
        0x8A2BE2, 0x1D78F2, 0xE5446D, 0xE4812C,
        0x13A48C, 0x5A55D6, 0xB0388E, 0x276A82
    };

    return palette[(text_hash(text) + (uint32_t)variant * 3u) %
                   (sizeof(palette) / sizeof(palette[0]))];
}

lv_obj_t *crazypod_music_preview_sleeve_create(
    const struct crazypod_music_preview_cover_context *context,
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int size, int seed,
    int artwork_slot, bool cache_only)
{
    const char *primary_key =
        track != NULL && track->album[0] != '\0'
            ? track->album : "CrazyPod";
    const char *secondary_key =
        track != NULL && track->artist[0] != '\0'
            ? track->artist : "Local Music";
    uint32_t primary = artwork_color(primary_key, seed);
    uint32_t secondary = artwork_color(secondary_key, seed + 11);
    lv_obj_t *sleeve = make_box(
        parent, x, y, size, size, size > 40 ? 5 : 3,
        primary, LV_OPA_COVER);
    lv_obj_t *label;
    const lv_image_dsc_t *descriptor = NULL;

    if(track != NULL) {
        if(context->defer_media) {
            descriptor = crazypod_artwork_load_priority(
                artwork_slot, track, size,
                CRAZYPOD_MENU_ARTWORK_PRIORITY);
            if(descriptor == NULL)
                if(context->media_deferred != NULL)
                    *context->media_deferred = true;
        }
        else {
            descriptor = cache_only
                ? crazypod_artwork_load_cached_priority(
                      artwork_slot, track, size, 180)
                : crazypod_artwork_load(
                      artwork_slot, track, size);
        }
    }

    lv_obj_set_style_bg_grad_color(
        sleeve, lv_color_hex(secondary), 0);
    lv_obj_set_style_bg_grad_dir(
        sleeve, seed % 2 == 0
            ? LV_GRAD_DIR_HOR : LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(sleeve, 1, 0);
    lv_obj_set_style_border_color(
        sleeve, lv_color_hex(0xF3F5F6), 0);
    lv_obj_set_style_border_opa(sleeve, 62, 0);
    crazypod_preview_add_bevel(
        sleeve, size, size, 0xFFFFFF, 0x111315);
    make_box(
        sleeve, 3, 4, 2, size - 8, 1,
        0xFFFFFF, 36);
    label = make_box(
        sleeve, size / 3, size / 3,
        size / 3, size / 3,
        LV_RADIUS_CIRCLE, 0xF2E7CA, 210);
    make_box(
        label, size / 9, size / 9,
        size / 9, size / 9,
        LV_RADIUS_CIRCLE, 0x25282A, 235);
    make_box(sleeve, size / 7, size - size / 5,
             size * 3 / 7, 2, 1,
             0xFFFFFF, 105);
    make_box(sleeve, size - size / 4, size - size / 5,
             size / 10, 2, 1, 0xFFFFFF, 62);
    lv_obj_set_style_clip_corner(sleeve, true, 0);
    if(descriptor != NULL) {
        lv_obj_t *image = lv_image_create(sleeve);
        lv_image_set_src(image, descriptor);
        lv_obj_center(image);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }
    return sleeve;
}

static void music_preview_title_initial(
    const struct crazypod_track *track, char initial[5])
{
    const char *text =
        track != NULL && track->title[0] != '\0'
            ? track->title : NULL;
    unsigned char first;
    int length = 1;
    int index;

    initial[0] = '-';
    initial[1] = '\0';
    if(text == NULL)
        return;
    while(*text != '\0') {
        first = (unsigned char)*text;
        if(first >= 0x80 ||
           (first >= '0' && first <= '9') ||
           (first >= 'A' && first <= 'Z') ||
           (first >= 'a' && first <= 'z'))
            break;
        ++text;
    }
    if(*text == '\0')
        return;

    first = (unsigned char)*text;
    if(first < 0x80) {
        initial[0] =
            first >= 'a' && first <= 'z'
                ? (char)(first - 'a' + 'A') : (char)first;
        initial[1] = '\0';
        return;
    }
    if((first & 0xE0) == 0xC0)
        length = 2;
    else if((first & 0xF0) == 0xE0)
        length = 3;
    else if((first & 0xF8) == 0xF0)
        length = 4;
    for(index = 0; index < length && text[index] != '\0'; ++index)
        initial[index] = text[index];
    initial[index] = '\0';
}

lv_obj_t *crazypod_music_preview_initial_cover_create(
    const struct crazypod_music_preview_cover_context *context,
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int size, int seed)
{
    const char *primary_key =
        track != NULL && track->title[0] != '\0'
            ? track->title : "Local Music";
    const char *secondary_key =
        track != NULL && track->artist[0] != '\0'
            ? track->artist : primary_key;
    uint32_t primary = artwork_color(primary_key, seed);
    uint32_t secondary = artwork_color(secondary_key, seed + 17);
    lv_obj_t *cover = make_box(
        parent, x, y, size, size, size > 32 ? 5 : 4,
        primary, track != NULL ? LV_OPA_COVER : 105);
    lv_obj_t *badge;
    lv_obj_t *label;
    char initial[5];

    music_preview_title_initial(track, initial);
    lv_obj_set_style_bg_grad_color(
        cover, lv_color_hex(secondary), 0);
    lv_obj_set_style_bg_grad_dir(
        cover, seed % 2 == 0
            ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(cover, 1, 0);
    lv_obj_set_style_border_color(
        cover, lv_color_hex(0xF2F5F6), 0);
    lv_obj_set_style_border_opa(cover, 62, 0);
    crazypod_preview_add_bevel(
        cover, size, size, 0xF7FBFC, 0x101315);
    make_box(cover, 3, 4, 3, size - 8, 1,
             0xF7FBFC, track != NULL ? 74 : 38);
    badge = make_box(
        cover, size / 2 - size / 5, size / 2 - size / 5,
        size * 2 / 5, size * 2 / 5,
        LV_RADIUS_CIRCLE, 0x111619,
        track != NULL ? 145 : 72);
    make_box(
        badge, size / 5 - 2, size / 5 - 2, 4, 4,
        LV_RADIUS_CIRCLE, 0xE9F0F2,
        track != NULL ? 180 : 75);
    label = make_label(
        badge, initial, context->metadata_font,
        COLOR_WHITE, track != NULL ? 235 : 100);
    lv_obj_center(label);
    make_box(
        cover, size / 4, size - 6, size / 2, 2, 1,
        0xF4F7F8, track != NULL ? 68 : 30);
    return cover;
}



#endif
