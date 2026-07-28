#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "../../../crazypod_artwork.h"
#include "../../../crazypod_music.h"
#include "../../../crazypod_playlist.h"
#include "../../presentation/crazypod_preview_motion.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_music_root_preview.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_CYAN 0x26CFF5
#define CRAZYPOD_METADATA_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_MENU_NOW_ARTWORK_SIZE 68
#define CRAZYPOD_MENU_FLOW_ARTWORK_SIZE 58
#define CRAZYPOD_MENU_FLOW_ARTWORK_SLOT_BASE \
    (CRAZYPOD_COVERFLOW_ARTWORK_SLOTS - 3)
#define CRAZYPOD_MENU_ALBUM_ARTWORK_SIZE 67
#define CRAZYPOD_MENU_ARTWORK_PRIORITY 20

static const char *music_menu_titles[] = {
    "Now Playing", "Album Flow", "All Music", "Playlists",
    "Artists", "Albums", "Songs", "Search"
};
static bool defer_media;

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

static const struct crazypod_track *current_track(void)
{
    const char *path = crazypod_queue_path(crazypod_queue_index());
    int index = crazypod_music_find_track(path);
    return crazypod_music_track(index);
}

static lv_obj_t *make_procedural_record_sleeve(
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
    lv_obj_t *sleeve = crazypod_ui_widget_box(
        parent, x, y, size, size, size > 40 ? 5 : 3,
        primary, LV_OPA_COVER);
    lv_obj_t *label;
    const lv_image_dsc_t *descriptor = NULL;

    if(track != NULL) {
        if(defer_media) {
            descriptor = crazypod_artwork_load_priority(
                artwork_slot, track, size,
                CRAZYPOD_MENU_ARTWORK_PRIORITY);
            if(descriptor == NULL)
                *crazypod_preview_motion_media_deferred_flag() = true;
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
    crazypod_ui_widget_box(
        sleeve, 3, 4, 2, size - 8, 1,
        0xFFFFFF, 36);
    label = crazypod_ui_widget_box(
        sleeve, size / 3, size / 3,
        size / 3, size / 3,
        LV_RADIUS_CIRCLE, 0xF2E7CA, 210);
    crazypod_ui_widget_box(
        label, size / 9, size / 9,
        size / 9, size / 9,
        LV_RADIUS_CIRCLE, 0x25282A, 235);
    crazypod_ui_widget_box(sleeve, size / 7, size - size / 5,
             size * 3 / 7, 2, 1,
             0xFFFFFF, 105);
    crazypod_ui_widget_box(sleeve, size - size / 4, size - size / 5,
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
static lv_obj_t *make_music_initial_cover(
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
    lv_obj_t *cover = crazypod_ui_widget_box(
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
    crazypod_ui_widget_box(cover, 3, 4, 3, size - 8, 1,
             0xF7FBFC, track != NULL ? 74 : 38);
    badge = crazypod_ui_widget_box(
        cover, size / 2 - size / 5, size / 2 - size / 5,
        size * 2 / 5, size * 2 / 5,
        LV_RADIUS_CIRCLE, 0x111619,
        track != NULL ? 145 : 72);
    crazypod_ui_widget_box(
        badge, size / 5 - 2, size / 5 - 2, 4, 4,
        LV_RADIUS_CIRCLE, 0xE9F0F2,
        track != NULL ? 180 : 75);
    label = crazypod_ui_widget_label(
        badge, initial, CRAZYPOD_METADATA_FONT,
        COLOR_WHITE, track != NULL ? 235 : 100);
    lv_obj_center(label);
    crazypod_ui_widget_box(
        cover, size / 4, size - 6, size / 2, 2, 1,
        0xF4F7F8, track != NULL ? 68 : 30);
    return cover;
}
void crazypod_music_root_preview_render(
    lv_obj_t *parent, int selected, bool defer_media_value)
{
    lv_obj_t *text_panel;
    lv_obj_t *title;
    lv_obj_t *detail;
    lv_obj_t *stage;
    lv_obj_t *part;
    char count_text[96];
    int count = 0;
    int i;

    defer_media = defer_media_value;
    switch(selected) {
    case 0: {
        const struct crazypod_track *track = current_track();
        lv_obj_t *disc = crazypod_ui_widget_box(
            parent, 242, 69, 59, 59,
            LV_RADIUS_CIRCLE, 0xC7D1D8, 235);
        lv_obj_set_style_border_width(disc, 2, 0);
        lv_obj_set_style_border_color(
            disc, lv_color_hex(0xF8FFFF), 0);
        lv_obj_set_style_border_opa(disc, 105, 0);
        part = crazypod_ui_widget_box(
            disc, 4, 4, 51, 51, LV_RADIUS_CIRCLE,
            0xD9E4E8, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(0xF7FCFF), 0);
        lv_obj_set_style_border_opa(part, 96, 0);
        crazypod_ui_widget_box(disc, 9, 9, 41, 41, LV_RADIUS_CIRCLE,
                 0x627582, 215);
        part = crazypod_ui_widget_box(
            disc, 14, 14, 31, 31, LV_RADIUS_CIRCLE,
            0x738693, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(0xD7E5EA), 0);
        lv_obj_set_style_border_opa(part, 65, 0);
        crazypod_ui_widget_box(disc, 23, 23, 13, 13, LV_RADIUS_CIRCLE,
                 0x101820, LV_OPA_COVER);
        crazypod_ui_widget_box(disc, 27, 27, 5, 5, LV_RADIUS_CIRCLE,
                 0xE9F2F5, 195);
        crazypod_preview_make_plinth(
            parent, 184, 143, 118, 0xB9C3C7, 0x283035);
        stage = crazypod_ui_widget_box(parent, 190, 50, 82, 91, 5,
                         0xD9E4E8, 58);
        lv_obj_set_style_border_width(stage, 2, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xEAFBFF), 0);
        lv_obj_set_style_border_opa(stage, 145, 0);
        crazypod_preview_add_bevel(
            stage, 82, 91, 0xF4FBFD, 0x182025);
        crazypod_preview_add_fastener(stage, 3, 3, 0xB7C1C5);
        crazypod_preview_add_fastener(stage, 74, 3, 0xB7C1C5);
        make_procedural_record_sleeve(
            stage, track, 7, 7, CRAZYPOD_MENU_NOW_ARTWORK_SIZE, 0,
            CRAZYPOD_PREVIEW_ARTWORK_SLOT, false);
        part = crazypod_ui_widget_box(stage, 8, 80, 66, 4,
                        LV_RADIUS_CIRCLE, 0x20313A, 95);
        crazypod_ui_widget_box(part, 0, 0, track != NULL ? 24 : 3, 4,
                 LV_RADIUS_CIRCLE, COLOR_CYAN, 225);
        for(i = 0; i < 4; ++i)
            crazypod_ui_widget_box(parent, 199 + i * 18, 146, 10, 3,
                     LV_RADIUS_CIRCLE, COLOR_CYAN,
                     i < 2 ? 210 : 55);
        crazypod_preview_motion_register(
            disc, 24, 0, 214, 110, 0, 20, 260,
            -24, 0, 214, -80);
        crazypod_preview_motion_register(
            stage, 10, -8, 230, -25, 0, 0, 240,
            -9, 5, 230, 20);
        count = crazypod_music_track_count();
        break;
    }
    case 1: {
        count = crazypod_music_album_count();
        static const int positions[] = { 180, 211, 242 };
        static const int rotations[] = { -90, 0, 90 };
        crazypod_preview_make_plinth(
            parent, 178, 143, 122, 0xC5CED2, 0x242A2D);
        crazypod_ui_widget_box(parent, 183, 139, 112, 3,
                 LV_RADIUS_CIRCLE, 0xAEB9BD, 215);
        for(i = 0; i < 3; ++i) {
            const struct crazypod_track *track =
                i < count ? crazypod_music_album_track(i, 0) : NULL;
            part = make_procedural_record_sleeve(
                parent, track, positions[i],
                i == 1 ? 55 : 69,
                CRAZYPOD_MENU_FLOW_ARTWORK_SIZE, i,
                CRAZYPOD_MENU_FLOW_ARTWORK_SLOT_BASE + i, false);
            lv_obj_set_style_transform_rotation(
                part, rotations[i], 0);
            crazypod_preview_motion_register(
                part, (i - 1) * 24, 12, 178,
                rotations[i] + (i - 1) * 80, 0,
                i * 40, 280,
                (i - 1) * -20, 8, 184,
                rotations[i] + (i - 1) * -60);
        }
        part = crazypod_ui_widget_box(parent, 257, 108, 34, 34,
                        LV_RADIUS_CIRCLE, 0xD7E2E8, 205);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(0xF4FBFD), 0);
        lv_obj_set_style_border_opa(part, 115, 0);
        crazypod_ui_widget_box(part, 5, 5, 24, 24,
                 LV_RADIUS_CIRCLE, 0xAAB7BD, 85);
        crazypod_ui_widget_box(part, 12, 12, 10, 10,
                 LV_RADIUS_CIRCLE, 0x25313A, 245);
        crazypod_ui_widget_box(part, 15, 15, 4, 4,
                 LV_RADIUS_CIRCLE, 0xE8EFF1, 210);
        crazypod_preview_motion_register(
            part, 20, 0, 190, 180, 0, 80, 260,
            18, 0, 190, 260);
        break;
    }
    case 2: {
        static const int positions[] = { 7, 31, 55, 79 };
        static const int heights[] = { 28, 23, 27, 21 };
        static const int rotations[] = { -24, 13, -10, 20 };
        lv_obj_t *cavity;
        lv_obj_t *rail;

        count = crazypod_music_track_count();
        crazypod_preview_make_plinth(
            parent, 175, 153, 130, 0xBFC9CD, 0x22282B);
        stage = crazypod_ui_widget_box(parent, 176, 47, 128, 106, 8,
                         0x394247, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            stage, lv_color_hex(0x171C1F), 0);
        lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(stage, 2, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xAAB5BA), 0);
        lv_obj_set_style_border_opa(stage, 135, 0);
        crazypod_preview_add_bevel(
            stage, 128, 106, 0xECF3F5, 0x080A0B);
        crazypod_preview_add_fastener(stage, 5, 5, 0xB8C2C6);
        crazypod_preview_add_fastener(stage, 118, 5, 0xB8C2C6);
        crazypod_ui_widget_box(stage, 16, 7, 96, 3, 1,
                 0xD5DEE1, 72);
        title = crazypod_ui_widget_label(
            stage, "A  /  Z", &lv_font_montserrat_8,
            0xD9E3E6, 125);
        lv_obj_set_width(title, 64);
        lv_obj_set_style_text_align(
            title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(title, 32, 10);
        cavity = crazypod_ui_widget_box(
            stage, 5, 20, 118, 77, 5,
            0x07090A, 238);
        lv_obj_set_style_border_width(cavity, 1, 0);
        lv_obj_set_style_border_color(
            cavity, lv_color_hex(0x7E8A8F), 0);
        lv_obj_set_style_border_opa(cavity, 82, 0);
        crazypod_ui_widget_box(cavity, 4, 4, 110, 2, 1,
                 0xB9C4C8, 35);
        for(i = 0; i < 4; ++i) {
            int track_index = -1;
            const struct crazypod_track *track = NULL;

            if(i < count) {
                track_index = count > 4
                    ? i * (count - 1) / 3 : i;
                track = crazypod_music_track(track_index);
            }
            part = make_music_initial_cover(
                stage, track, positions[i], heights[i],
                46, track_index >= 0 ? track_index : i);
            lv_obj_set_style_transform_rotation(
                part, rotations[i], 0);
            crazypod_preview_motion_register(
                part, (i - 1) * 5, 34 + i * 3,
                210, rotations[i], 0,
                i * 35, 260,
                (i - 1) * 7, 28,
                210, rotations[i]);
        }
        rail = crazypod_ui_widget_box(stage, 5, 78, 118, 20, 4,
                        0x2C3438, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            rail, lv_color_hex(0x111619), 0);
        lv_obj_set_style_bg_grad_dir(rail, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(rail, 1, 0);
        lv_obj_set_style_border_color(
            rail, lv_color_hex(0x9DA8AD), 0);
        lv_obj_set_style_border_opa(rail, 112, 0);
        crazypod_preview_add_bevel(
            rail, 118, 20, 0xE1E8EA, 0x050708);
        crazypod_preview_add_fastener(rail, 6, 7, 0xAAB4B8);
        crazypod_preview_add_fastener(rail, 107, 7, 0xAAB4B8);
        crazypod_ui_widget_box(rail, 28, 8, 62, 4, 2,
                 0x060809, 210);
        crazypod_ui_widget_box(rail, 39, 9, 40, 2, 1,
                 COLOR_CYAN, 105);
        crazypod_preview_motion_register(
            stage, 0, 13, 232, 0, 0, 0, 260,
            -8, 8, 232, 0);
        break;
    }
    case 3: {
        count = crazypod_music_playlist_count();
        stage = crazypod_ui_widget_box(parent, 184, 60, 112, 84, 9,
                         0xB7A986, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            stage, lv_color_hex(0x544936), 0);
        lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(stage, 2, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xD8C99F), 0);
        lv_obj_set_style_border_opa(stage, 125, 0);
        crazypod_preview_add_bevel(
            stage, 112, 84, 0xE8D9B4, 0x20170F);
        crazypod_preview_add_fastener(stage, 5, 5, 0xB9A982);
        crazypod_preview_add_fastener(stage, 102, 5, 0xB9A982);
        crazypod_ui_widget_box(stage, 13, 11, 86, 15, 2, 0xE8E0CA, 215);
        title = crazypod_ui_widget_label(
            stage, "PLAYLIST / A",
            &lv_font_montserrat_8, 0x443A2E, 155);
        lv_obj_set_pos(title, 25, 14);
        part = crazypod_ui_widget_box(stage, 15, 32, 82, 36, 4,
                        0x24211D, 235);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(0xB7A986), 0);
        lv_obj_set_style_border_opa(part, 72, 0);
        for(i = 0; i < 2; ++i) {
            lv_obj_t *reel = crazypod_ui_widget_box(
                part, 12 + i * 43, 6, 24, 24,
                LV_RADIUS_CIRCLE, 0xD8D1BC, 235);
            crazypod_ui_widget_box(reel, 7, 7, 10, 10,
                     LV_RADIUS_CIRCLE, 0x34312C, 240);
            crazypod_ui_widget_box(reel, 10, 10, 4, 4,
                     LV_RADIUS_CIRCLE, 0xE6DDC8, 185);
            crazypod_preview_motion_register(
                reel, 0, -5, 184, (i ? 120 : -120), 0,
                60 + i * 30, 240,
                0, 4, 184, i ? 140 : -140);
        }
        crazypod_ui_widget_box(part, 30, 16, 22, 2,
                 LV_RADIUS_CIRCLE, 0x8F7651, 115);
        crazypod_ui_widget_box(stage, 47, 70, 18, 8, 2,
                 0xBFC6C8, 220);
        crazypod_ui_widget_box(stage, 54, 70, 3, 8, 1,
                 0x34383A, 190);
        crazypod_ui_widget_box(stage, 27, 76, 58, 2,
                 LV_RADIUS_CIRCLE, 0xF4D35E, 145);
        crazypod_preview_motion_register(
            stage, 0, -13, 228, -18, 0, 0, 260,
            0, -9, 228, 15);
        break;
    }
    case 4: {
        count = crazypod_music_artist_count();
        crazypod_ui_widget_box(parent, 184, 55, 112, 92, 46,
                 0xD8B96B, 22);
        stage = crazypod_ui_widget_box(
            parent, 211, 47, 58, 70, 25,
            0xC7D0D4, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(stage, 2, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0x818B90), 0);
        lv_obj_set_style_border_opa(stage, 165, 0);
        part = crazypod_ui_widget_box(parent, 218, 53, 44, 58, 21,
                        0xB8C1C6, 245);
        lv_obj_set_style_bg_grad_color(
            part, lv_color_hex(0x3A4146), 0);
        lv_obj_set_style_bg_grad_dir(part, LV_GRAD_DIR_HOR, 0);
        for(i = 0; i < 5; ++i)
            crazypod_ui_widget_box(part, 8, 10 + i * 8, 28, 2,
                     LV_RADIUS_CIRCLE, 0x171A1D, 145);
        for(i = 0; i < 3; ++i)
            crazypod_ui_widget_box(part, 11 + i * 10, 8, 1, 42,
                     0, 0xF0F4F5, 45);
        crazypod_ui_widget_box(parent, 228, 109, 24, 8, 3,
                 0x7A858A, 235);
        stage = crazypod_ui_widget_box(parent, 237, 107, 6, 36, 3,
                         0xAEB7BC, 245);
        crazypod_preview_make_plinth(
            parent, 213, 139, 54, 0xD6DEE1, 0x697277);
        crazypod_ui_widget_box(parent, 264, 142, 23, 2,
                 LV_RADIUS_CIRCLE, 0x6A7174, 120);
        crazypod_preview_motion_register(
            part, 0, 22, 204, 0, 0, 20, 280,
            0, 20, 204, 0);
        crazypod_preview_motion_register(
            stage, 0, 15, 224, 0, 0, 0, 240,
            0, 12, 224, 0);
        break;
    }
    case 5: {
        const struct crazypod_track *track =
            crazypod_music_album_count() > 0
                ? crazypod_music_album_track(0, 0) : NULL;
        count = crazypod_music_album_count();
        stage = crazypod_ui_widget_box(parent, 178, 62, 124, 83, 8,
                         0x50463B, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            stage, lv_color_hex(0x201C18), 0);
        lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0x8D785F), 0);
        lv_obj_set_style_border_opa(stage, 135, 0);
        crazypod_preview_add_bevel(
            stage, 124, 83, 0xAE9471, 0x120D09);
        crazypod_preview_add_fastener(stage, 5, 5, 0x8F826F);
        crazypod_preview_add_fastener(stage, 114, 72, 0x8F826F);
        part = crazypod_ui_widget_box(stage, 55, 12, 62, 62,
                        LV_RADIUS_CIRCLE, 0x111111, LV_OPA_COVER);
        crazypod_ui_widget_box(part, 10, 10, 42, 42,
                 LV_RADIUS_CIRCLE, 0x252525, 235);
        crazypod_ui_widget_box(part, 16, 16, 30, 30,
                 LV_RADIUS_CIRCLE, 0x171717, 235);
        crazypod_ui_widget_box(part, 25, 25, 12, 12,
                 LV_RADIUS_CIRCLE, 0xD8A94D, 245);
        crazypod_ui_widget_box(part, 29, 29, 4, 4,
                 LV_RADIUS_CIRCLE, 0xF5E8CA, 225);
        crazypod_preview_motion_register(
            part, 0, 0, 192, -180, 0, 20, 300,
            0, 0, 192, 180);
        part = make_procedural_record_sleeve(
            parent, track, 183, 54,
            CRAZYPOD_MENU_ALBUM_ARTWORK_SIZE, 23,
            CRAZYPOD_PREVIEW_ARTWORK_SLOT, false);
        crazypod_preview_motion_register(
            part, -28, 0, 224, -35, 0, 0, 280,
            -25, 0, 224, -45);
        part = crazypod_ui_widget_box(stage, 105, 8, 4, 54, 2,
                        0xD8D4C8, 240);
        lv_obj_set_style_transform_rotation(part, 160, 0);
        crazypod_ui_widget_box(stage, 99, 5, 15, 15,
                 LV_RADIUS_CIRCLE, 0x969A9A, 245);
        crazypod_ui_widget_box(stage, 104, 10, 5, 5,
                 LV_RADIUS_CIRCLE, 0x303234, 230);
        crazypod_ui_widget_box(stage, 98, 57, 18, 7, 3, 0xD8D4C8, 235);
        crazypod_preview_motion_register(
            part, 8, -7, 220, 310, 0, 80, 240,
            8, -7, 220, 310);
        break;
    }
    case 6: {
        count = crazypod_music_track_count();
        stage = crazypod_ui_widget_box(parent, 181, 61, 118, 84, 7,
                         0x78634D, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            stage, lv_color_hex(0x3B2D23), 0);
        lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xB99A74), 0);
        lv_obj_set_style_border_opa(stage, 120, 0);
        crazypod_preview_add_bevel(
            stage, 118, 84, 0xC7AB87, 0x1B120D);
        crazypod_preview_add_fastener(stage, 4, 4, 0x9A836D);
        crazypod_preview_add_fastener(stage, 109, 4, 0x9A836D);
        crazypod_ui_widget_box(stage, 8, 9, 102, 12, 3,
                 0x251F1A, 210);
        crazypod_ui_widget_box(stage, 13, 13, 34, 3, 1,
                 0xD8C7A7, 115);
        for(i = 3; i >= 0; --i) {
            part = crazypod_ui_widget_box(stage, 16 + i * 4, 28 + i * 12,
                            82, 29, 3, 0xF1E4C9, LV_OPA_COVER);
            lv_obj_set_style_border_width(part, 1, 0);
            lv_obj_set_style_border_color(
                part, lv_color_hex(0xC8B997), 0);
            lv_obj_set_style_border_opa(part, 105, 0);
            crazypod_ui_widget_box(part, 58, 2, 16, 4, 1,
                     0xD6C39F, 165);
            crazypod_ui_widget_box(part, 8, 8, 47 - i * 3, 2, 1,
                     0x6A5C4D, 105);
            crazypod_ui_widget_box(part, 8, 15, 60, 2, 1,
                     0x6A5C4D, 70);
            crazypod_preview_motion_register(
                part, 0, 18 + i * 5, 230, i * 12, 0,
                (3 - i) * 30, 240,
                0, 19 + i * 4, 230, i * -12);
        }
        crazypod_preview_motion_register(
            stage, 0, 8, 238, 0, 0, 0, 220,
            0, -6, 238, 0);
        break;
    }
    default: {
        count = crazypod_music_track_count();
        stage = crazypod_ui_widget_box(parent, 187, 58, 99, 91, 5,
                         0xF0E6D2, LV_OPA_COVER);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xC7B99C), 0);
        lv_obj_set_style_border_opa(stage, 135, 0);
        crazypod_preview_add_bevel(
            stage, 99, 91, 0xFFFFFF, 0x8D806D);
        crazypod_preview_add_paper_rules(
            stage, 99, 18, 5, 13, 0x6E7780);
        crazypod_ui_widget_box(stage, 13, 9, 42, 3, 1,
                 0x384A58, 120);
        crazypod_ui_widget_box(stage, 84, 3, 10, 10, 1,
                 0xD4C7AC, 210);
        part = crazypod_ui_widget_box(parent, 218, 68, 45, 45,
                        LV_RADIUS_CIRCLE, 0x8DD9EA, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(part, 7, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(COLOR_CYAN), 0);
        lv_obj_set_style_border_opa(part, 235, 0);
        title = crazypod_ui_widget_box(
            part, 6, 6, 33, 33, LV_RADIUS_CIRCLE,
            0xD7F5FA, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(title, 1, 0);
        lv_obj_set_style_border_color(
            title, lv_color_hex(0xE9FCFF), 0);
        lv_obj_set_style_border_opa(title, 115, 0);
        title = crazypod_ui_widget_box(parent, 258, 107, 8, 34, 4,
                         0x76858C, 245);
        crazypod_preview_add_bevel(
            title, 8, 34, 0xE3EAEC, 0x262E31);
        crazypod_ui_widget_box(title, 2, 22, 4, 8, 2,
                 0x23282A, 220);
        crazypod_preview_motion_register(
            stage, 0, 10, 236, 0, 0, 0, 240,
            -8, 4, 236, 0);
        crazypod_preview_motion_register(
            part, 38, -4, 190, 120, 0, 40, 300,
            34, 5, 190, 160);
        break;
    }
    }

    text_panel = crazypod_preview_make_text_panel(parent, 158, 58);
    title = crazypod_ui_widget_label(
        text_panel,
        selected == 0 && current_track() != NULL
            ? current_track()->title : music_menu_titles[selected],
        &lv_font_montserrat_12,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(title, 132);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(title, 4, 6);
    if(selected == 0 && current_track() != NULL)
        snprintf(count_text, sizeof(count_text), "%s",
                 current_track()->artist);
    else if(selected == 7)
        snprintf(count_text, sizeof(count_text),
                 "%d local tracks", count);
    else
        snprintf(count_text, sizeof(count_text), "%d %s", count,
                 selected == 3 ? "playlists" :
                 selected == 4 ? "artists" :
                 selected == 5 || selected == 1 ? "albums" : "songs");
    detail = crazypod_ui_widget_label(text_panel, count_text,
                        &lv_font_montserrat_8,
                        COLOR_WHITE, 125);
    lv_obj_set_width(detail, 132);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(detail, 4, 32);
    crazypod_preview_motion_register(
        text_panel, 0, 10, 246, 0, 0, 70, 220,
        0, 6, 246, 0);
}

#endif
