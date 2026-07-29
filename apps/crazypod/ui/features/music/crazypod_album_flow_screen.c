#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_coverflow.h"
#include "../../../crazypod_music.h"
#include "../../presentation/crazypod_empty_state.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_album_flow_screen.h"

#define COLOR_WHITE 0xFFFFFF

static lv_obj_t *album_title;
static lv_obj_t *album_artist;
static lv_obj_t *album_position;
static int displayed_album = -1;

static lv_obj_t *make_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_label(
        parent, text, font, color, opacity);
}

static void set_metadata(int album_index)
{
    const struct crazypod_album *album =
        crazypod_music_album(album_index);
    char position[32];

    lv_label_set_text(
        album_title, album != NULL ? album->title : "");
    lv_label_set_text(
        album_artist, album != NULL ? album->artist : "");
    snprintf(position, sizeof(position), "%d / %d",
             album_index + 1, crazypod_music_album_count());
    lv_label_set_text(album_position, position);
    displayed_album = album_index;
}

void crazypod_album_flow_screen_reset(void)
{
    album_title = NULL;
    album_artist = NULL;
    album_position = NULL;
    displayed_album = -1;
}

void crazypod_album_flow_screen_render(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font)
{
    int count = crazypod_music_album_count();

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    if(count <= 0) {
        crazypod_empty_state_render(
            parent, LV_SYMBOL_AUDIO, "No Albums",
            "Add local music and rescan.");
        return;
    }

    album_title = make_label(
        parent, "", metadata_font, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(album_title, 260);
    lv_obj_set_height(album_title, 18);
    lv_obj_set_style_text_align(
        album_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(album_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(album_title, 30, 195);
    album_artist = make_label(
        parent, "", metadata_font, COLOR_WHITE, 145);
    lv_obj_set_width(album_artist, 260);
    lv_obj_set_height(album_artist, 16);
    lv_obj_set_style_text_align(
        album_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(album_artist, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(album_artist, 30, 213);
    album_position = make_label(
        parent, "", &lv_font_montserrat_8, COLOR_WHITE, 70);
    lv_obj_set_width(album_position, 60);
    lv_obj_set_style_text_align(
        album_position, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(album_position, 130, 228);

    set_metadata(state->selected);
    crazypod_coverflow_enter(state->selected);
}

int crazypod_album_flow_screen_sync(void)
{
    int album_index;

    if(album_title == NULL || album_artist == NULL ||
       album_position == NULL)
        return -1;
    album_index = crazypod_coverflow_center_album();
    if(album_index != displayed_album)
        set_metadata(album_index);
    return album_index;
}

#endif
