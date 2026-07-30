#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_artwork.h"
#include "../../../crazypod_music.h"
#include "../../../crazypod_playlist.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "crazypod_music_item_preview.h"

#define COLOR_CYAN 0x55D6E7
#define COLOR_WHITE 0xFFFFFF

enum crazypod_preview_icon {
    CRAZYPOD_PREVIEW_ICON_ARTIST,
    CRAZYPOD_PREVIEW_ICON_PLAYLISTS
};

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

static lv_obj_t *make_preview_icon_part(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, lv_opa_t opacity)
{
    return make_box(
        parent, x, y, width, height, radius,
        COLOR_CYAN, opacity);
}

static lv_obj_t *make_music_preview_icon(
    lv_obj_t *parent, enum crazypod_preview_icon icon, int x, int y)
{
    lv_obj_t *stage = make_box(
        parent, x, y, 96, 96, 24, 0x102A38, 188);

    lv_obj_set_style_bg_grad_color(
        stage, lv_color_hex(0x07131B), 0);
    lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(stage, 1, 0);
    lv_obj_set_style_border_color(
        stage, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_border_opa(stage, 58, 0);
    if(icon == CRAZYPOD_PREVIEW_ICON_ARTIST) {
        make_preview_icon_part(
            stage, 29, 13, 30, 30, LV_RADIUS_CIRCLE, 245);
        make_preview_icon_part(
            stage, 17, 48, 54, 27, LV_RADIUS_CIRCLE, 225);
        make_box(stage, 27, 62, 34, 14, 7, 0x102A38, 188);
    }
    else {
        int row;

        for(row = 0; row < 3; ++row) {
            make_preview_icon_part(
                stage, 17, 20 + row * 20, 10, 10,
                LV_RADIUS_CIRCLE, 235);
            make_preview_icon_part(
                stage, 34, 22 + row * 20, 38, 6, 3, 220);
        }
    }
    return stage;
}

static void create_artwork(
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int size, int slot)
{
    const lv_image_dsc_t *descriptor =
        track != NULL
            ? crazypod_artwork_load(slot, track, size) : NULL;
    lv_obj_t *card = make_box(
        parent, x, y, size, size, 7,
        artwork_color(track != NULL ? track->album : "", 0),
        LV_OPA_COVER);

    lv_obj_set_style_bg_grad_color(
        card,
        lv_color_hex(artwork_color(
            track != NULL ? track->artist : "", 1)), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_shadow_width(card, 5, 0);
    lv_obj_set_style_shadow_offset_y(card, 2, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, 55, 0);
    lv_obj_set_style_clip_corner(card, true, 0);
    if(descriptor != NULL) {
        lv_obj_t *image = lv_image_create(card);

        lv_image_set_src(image, descriptor);
        lv_obj_center(image);
    }
    else {
        lv_obj_t *symbol = make_label(
            card, LV_SYMBOL_AUDIO, &lv_font_montserrat_16,
            COLOR_WHITE, 210);

        lv_obj_center(symbol);
    }
}

static void format_time_ms(
    unsigned long milliseconds, char *buffer, size_t size)
{
    unsigned long seconds = milliseconds / 1000;

    snprintf(buffer, size, "%u:%02u",
             (unsigned)((seconds / 60) % 10000),
             (unsigned)(seconds % 60));
}

void crazypod_music_item_preview_render(
    lv_obj_t *parent, const struct route_state *state,
    const struct crazypod_track *track,
    const lv_font_t *metadata_font)
{
    lv_obj_t *text_panel = crazypod_preview_make_text_panel(parent, 153, 70);
    lv_obj_t *title;
    lv_obj_t *detail;
    char text[96];

    if(state->route == MUSIC_ROUTE_ARTISTS) {
        const char *artist = crazypod_music_artist(state->selected);
        int count = crazypod_music_artist_track_count(state->selected);
        make_music_preview_icon(
            parent, CRAZYPOD_PREVIEW_ICON_ARTIST, 192, 52);
        title = make_label(text_panel, artist != NULL ? artist : "",
                           metadata_font,
                           COLOR_WHITE, LV_OPA_COVER);
        snprintf(text, sizeof(text), CP_FMT("%d songs"), count);
        detail = make_label(text_panel, text, &lv_font_montserrat_8,
                            COLOR_WHITE, 130);
    }
    else if(state->route == MUSIC_ROUTE_ALBUMS) {
        const struct crazypod_album *album =
            crazypod_music_album(state->selected);
        track = crazypod_music_album_track(state->selected, 0);
        create_artwork(parent, track, 204, 76, 72,
                       CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        title = make_label(text_panel,
                           album != NULL ? album->title : "",
                           metadata_font,
                           COLOR_WHITE, LV_OPA_COVER);
        detail = make_label(text_panel,
                            album != NULL ? album->artist : "",
                            metadata_font, COLOR_WHITE, 135);
    }
    else if(state->route == MUSIC_ROUTE_PLAYLISTS) {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(state->selected);
        make_music_preview_icon(
            parent, CRAZYPOD_PREVIEW_ICON_PLAYLISTS, 192, 52);
        title = make_label(text_panel,
                           playlist != NULL ? playlist->name : "",
                           metadata_font,
                           COLOR_WHITE, LV_OPA_COVER);
        snprintf(text, sizeof(text), CP_FMT("%d songs"),
                 playlist != NULL ? playlist->track_count : 0);
        detail = make_label(text_panel, text, &lv_font_montserrat_8,
                            COLOR_WHITE, 130);
    }
    else {
        char duration[16];
        create_artwork(parent, track, 204, 72, 72,
                       CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        title = make_label(text_panel,
                           track != NULL ? track->title : CP_TR("No Track"),
                           metadata_font,
                           COLOR_WHITE, LV_OPA_COVER);
        detail = make_label(text_panel,
                            track != NULL ? track->artist : "",
                            metadata_font, COLOR_WHITE, 155);
        if(track != NULL) {
            format_time_ms(track->duration_ms, duration, sizeof(duration));
            snprintf(text, sizeof(text), "%s  " LV_SYMBOL_BULLET "  %s",
                     track->album, duration);
            {
                lv_obj_t *album = make_label(text_panel, text,
                                              metadata_font,
                                              COLOR_WHITE, 95);
                lv_obj_set_width(album, 126);
                lv_obj_set_height(album, 16);
                lv_label_set_long_mode(album, LV_LABEL_LONG_MODE_DOTS);
                lv_obj_set_style_text_align(album, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_pos(album, 7, 49);
            }
        }
    }

    lv_obj_set_width(title, 126);
    lv_obj_set_height(title, 18);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 7, 6);
    lv_obj_set_width(detail, 126);
    lv_obj_set_height(detail, 16);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(detail, 7, 29);
}



#endif
