#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "audio.h"
#include "backlight.h"
#include "button.h"
#include "file.h"
#include "kernel.h"
#include "lcd.h"
#include "powermgmt.h"
#include "playlist.h"
#include "settings.h"
#include "sound.h"
#include "system.h"
#include "timefuncs.h"
#include "usb.h"

#include "lvgl.h"

#include "crazypod_artwork.h"
#include "crazypod_appearance.h"
#include "crazypod_coverflow.h"
#include "crazypod_icons.h"
#include "crazypod_music.h"
#include "crazypod_playlist.h"
#include "crazypod_presets.h"
#include "crazypod_state.h"
#include "crazypod_ui.h"
#include "crazypod_wallpaper.h"

#define CRAZYPOD_APP_COUNT 14
#define CRAZYPOD_SCREEN_COUNT 2
#define CRAZYPOD_DRAW_ROWS 40
#define CRAZYPOD_ROUTE_DEPTH 8
#define CRAZYPOD_VISIBLE_ROWS 7
#define CRAZYPOD_EDITOR_CHAR_COUNT 36
#define CRAZYPOD_SEARCH_QUERY_SIZE 33
#define CRAZYPOD_ALBUM_FLOW_CARD_COUNT 5
#define CRAZYPOD_ALBUM_FLOW_COVER_SIZE 120
#define CRAZYPOD_PREVIEW_ARTWORK_SLOT 16
#define CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT 18
#define CRAZYPOD_CAPSULE_ARTWORK_SLOT 19
#define CRAZYPOD_METADATA_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_GESTURE_SETTLE_TICKS \
    ((HZ * 80 / 1000) > 0 ? (HZ * 80 / 1000) : 1)
#define CRAZYPOD_MOTION_FRAME_TICKS \
    ((HZ / 30) > 0 ? (HZ / 30) : 1)
#define CRAZYPOD_DESKTOP_NATIVE_TOP 40
#define CRAZYPOD_DESKTOP_NATIVE_BOTTOM 143
#define CRAZYPOD_DESKTOP_NATIVE_HEIGHT \
    (CRAZYPOD_DESKTOP_NATIVE_BOTTOM - CRAZYPOD_DESKTOP_NATIVE_TOP)

#define COLOR_DETAIL  0x08080D
#define COLOR_PANEL   0x1B1B22
#define COLOR_WHITE   0xFFFFFF
#define COLOR_MUTED   0x9A9AA4
#define COLOR_ROSE    0xC94A78
#define COLOR_VIOLET  0x744BC8
#define COLOR_CYAN    0x26CFF5

enum crazypod_route {
    MUSIC_ROUTE_MENU,
    MUSIC_ROUTE_ALBUM_FLOW,
    MUSIC_ROUTE_ALL,
    MUSIC_ROUTE_PLAYLISTS,
    MUSIC_ROUTE_PLAYLIST_SONGS,
    MUSIC_ROUTE_ARTISTS,
    MUSIC_ROUTE_ARTIST_SONGS,
    MUSIC_ROUTE_ALBUMS,
    MUSIC_ROUTE_ALBUM_SONGS,
    MUSIC_ROUTE_SONGS,
    MUSIC_ROUTE_SEARCH,
    MUSIC_ROUTE_SEARCH_RESULTS,
    MUSIC_ROUTE_QUEUE,
    MUSIC_ROUTE_NOW_PLAYING,
    DIY_ROUTE_MENU,
    DIY_ROUTE_PRESETS,
    DIY_ROUTE_PRESET_LIBRARY,
    DIY_ROUTE_PRESET_ACTIONS,
    DIY_ROUTE_PRESET_EDIT,
    DIY_ROUTE_PRESET_RENAME,
    DIY_ROUTE_ICONS,
    DIY_ROUTE_DETAILS,
    DIY_ROUTE_BACKGROUNDS,
};

struct route_state {
    enum crazypod_route route;
    int selected;
    int group;
};

struct crazypod_app {
    const char *name;
    const char *symbol;
    uint32_t color;
    lv_obj_t *cell;
    lv_obj_t *tile;
    lv_obj_t *image;
    lv_obj_t *reflection_clip;
    lv_obj_t *reflection_image;
    lv_obj_t *symbol_label;
};

struct status_bar {
    lv_obj_t *time;
    lv_obj_t *battery_fill;
    lv_obj_t *charge;
    lv_obj_t *playing;
};

struct album_flow_card {
    lv_obj_t *root;
    lv_obj_t *cover;
    lv_obj_t *image;
    lv_obj_t *symbol;
    lv_obj_t *reflection_clip;
    lv_obj_t *reflection_image;
    int album_index;
    int artwork_slot;
};

struct menu_view_state {
    bool valid;
    enum crazypod_route route;
    lv_obj_t *rows[CRAZYPOD_VISIBLE_ROWS];
    lv_obj_t *labels[CRAZYPOD_VISIBLE_ROWS];
    lv_obj_t *markers[CRAZYPOD_VISIBLE_ROWS];
    lv_obj_t *circles[CRAZYPOD_VISIBLE_ROWS];
    lv_obj_t *icons[CRAZYPOD_VISIBLE_ROWS];
    lv_obj_t *scroll_thumb;
};

static struct crazypod_app apps[CRAZYPOD_APP_COUNT] = {
    { "Music",       LV_SYMBOL_AUDIO,      0xFF2E54, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Podcasts",    LV_SYMBOL_VOLUME_MAX, 0xA95BDE, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Mini Apps",   LV_SYMBOL_LIST,       0xFF9F0A, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Shuffle",     LV_SYMBOL_SHUFFLE,    0xFF375F, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Lock",        LV_SYMBOL_EYE_CLOSE,  0x59606B, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Camera",      LV_SYMBOL_IMAGE,      0x18B8EF, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Photos",      LV_SYMBOL_IMAGE,      0x3478F6, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Customize",   LV_SYMBOL_EDIT,       0xBF5AF2, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Fitness",     LV_SYMBOL_CHARGE,     0x30D158, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Voice Memos", LV_SYMBOL_VOLUME_MAX, 0xFF453A, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Books",       LV_SYMBOL_FILE,       0xFF9F0A, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Notes",       LV_SYMBOL_EDIT,       0xFFD60A, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Extras",      LV_SYMBOL_DIRECTORY,  0x64D2FF, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Settings",    LV_SYMBOL_SETTINGS,   0x8E8E93, NULL, NULL, NULL, NULL, NULL, NULL },
};

static const char *music_menu_titles[] = {
    "Now Playing", "Album Flow", "All Music", "Playlists",
    "Artists", "Albums", "Songs", "Search"
};

static const char *music_menu_symbols[] = {
    LV_SYMBOL_AUDIO, LV_SYMBOL_IMAGE, LV_SYMBOL_LOOP, LV_SYMBOL_LIST,
    LV_SYMBOL_HOME, LV_SYMBOL_DIRECTORY, LV_SYMBOL_AUDIO, LV_SYMBOL_EYE_OPEN
};

static const char *const diy_menu_titles[] = {
    "Presets", "Icons", "Details", "Backgrounds"
};

static const char *const diy_menu_symbols[] = {
    LV_SYMBOL_SAVE, LV_SYMBOL_IMAGE, LV_SYMBOL_SETTINGS, LV_SYMBOL_DIRECTORY
};

static const char *const editor_characters[CRAZYPOD_EDITOR_CHAR_COUNT] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

static const char *const preset_actions[] = {
    "Apply", "Export", "Edit"
};

static const char *const preset_edit_actions[] = {
    "Rename", "Update from Current", "Delete"
};

static const char *const diy_detail_titles[] = {
    "Player", "Icon Size", "Glow", "Highlight",
    "Primary", "Secondary"
};

static const char *const diy_background_titles[] = {
    "Home", "Menu"
};

static struct status_bar status_bars[CRAZYPOD_SCREEN_COUNT];
static struct route_state route_stack[CRAZYPOD_ROUTE_DEPTH];
static lv_obj_t *desktop_screen;
static lv_obj_t *desktop_wallpaper;
static lv_obj_t *desktop_carousel;
static lv_obj_t *desktop_title;
static lv_obj_t *desktop_indicators[CRAZYPOD_APP_COUNT];
static lv_obj_t *product_screen;
static lv_obj_t *product_content;
static lv_obj_t *desktop_capsule;
static lv_obj_t *desktop_capsule_glass;
static lv_obj_t *desktop_capsule_track;
static lv_obj_t *desktop_capsule_artist;
static lv_obj_t *desktop_capsule_progress;
static lv_obj_t *desktop_capsule_artwork;
static lv_obj_t *desktop_capsule_artwork_image;
static lv_obj_t *desktop_capsule_artwork_symbol;
static struct album_flow_card
    album_flow_cards[CRAZYPOD_ALBUM_FLOW_CARD_COUNT];
static lv_obj_t *album_flow_title;
static lv_obj_t *album_flow_artist;
static lv_obj_t *album_flow_position;
static lv_obj_t *now_progress_fill;
static lv_obj_t *now_elapsed;
static lv_obj_t *now_remaining;
static lv_obj_t *music_loading_title;
static lv_obj_t *music_loading_detail;
static lv_group_t *desktop_group;
static int selected_app;
static int route_depth;
static bool product_active;
static bool music_library_loaded;
static bool music_scan_screen;
static bool music_scan_start_failed;
static bool music_scan_pending;
static bool music_artwork_preparing;
static bool usb_storage_active;
static bool cpu_is_boosted;
static bool route_render_pending;
static bool desktop_motion_active;
static bool desktop_native_dirty;
static bool desktop_native_backdrop_ready;
static unsigned music_scan_generation_seen;
static unsigned preview_artwork_generation_seen;
static unsigned now_artwork_generation_seen;
static unsigned capsule_artwork_generation_seen;
static long route_render_due;
static long boost_until;
static long music_scan_not_before;
static long last_desktop_motion_tick;
static int desktop_position_q8;
static int desktop_velocity_q8;
static struct menu_view_state menu_view;
static char search_query[CRAZYPOD_SEARCH_QUERY_SIZE];
static char preset_name_editor[CRAZYPOD_PRESET_NAME_SIZE];
static char rendered_track_path[MAX_PATH];
static char desktop_capsule_artwork_path[MAX_PATH];
static fb_data draw_buffer[LCD_WIDTH * CRAZYPOD_DRAW_ROWS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data desktop_native_backdrop[
    LCD_WIDTH * CRAZYPOD_DESKTOP_NATIVE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);

extern struct frame_buffer_t lcd_framebuffer_default;

static int appearance_tile_size(void);
static void layout_desktop_carousel(bool animated);
static void refresh_menu_rows(const struct route_state *state);

static void set_cpu_boost(bool enabled)
{
    if(cpu_is_boosted == enabled)
        return;
    cpu_boost(enabled);
    cpu_is_boosted = enabled;
}

static void keep_cpu_boosted(int ticks)
{
    long deadline = current_tick + ticks;

    set_cpu_boost(true);
    if(TIME_AFTER(deadline, boost_until))
        boost_until = deadline;
}

static void set_plain_object(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color,
                            lv_opa_t opacity)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_opa(label, opacity, 0);
    return label;
}

static lv_obj_t *make_box(lv_obj_t *parent, int x, int y, int width,
                          int height, int radius, uint32_t color,
                          lv_opa_t opacity)
{
    lv_obj_t *box = lv_obj_create(parent);
    set_plain_object(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_radius(box, radius, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(box, opacity, 0);
    return box;
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

static uint32_t highlight_primary(void)
{
    return crazypod_appearance_color(
        crazypod_appearance_get()->primary_color);
}

static uint32_t highlight_secondary(void)
{
    return crazypod_appearance_color(
        crazypod_appearance_get()->secondary_color);
}

static void refresh_desktop_capsule_material(void)
{
    const lv_image_dsc_t *glass =
        crazypod_frosted_wallpaper_capsule();

    if(desktop_capsule == NULL || desktop_capsule_glass == NULL)
        return;
    if(crazypod_appearance_get()->home_background == 0 &&
       glass != NULL) {
        lv_image_set_src(desktop_capsule_glass, glass);
        lv_obj_remove_flag(desktop_capsule_glass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(desktop_capsule, LV_OPA_TRANSP, 0);
    }
    else {
        lv_obj_add_flag(desktop_capsule_glass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(
            desktop_capsule, lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(desktop_capsule, 34, 0);
    }
}

static void refresh_desktop_appearance(void)
{
    int i;

    if(desktop_screen == NULL)
        return;
    if(crazypod_appearance_get()->home_background == 0 &&
       crazypod_default_wallpaper() != NULL) {
        lv_obj_remove_flag(desktop_wallpaper, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(desktop_screen,
                                  lv_color_hex(0x141419), 0);
    }
    else {
        lv_obj_add_flag(desktop_wallpaper, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(
            desktop_screen,
            lv_color_hex(crazypod_appearance_home_color()), 0);
    }

    crazypod_icons_load_theme(crazypod_appearance_get()->icon_theme);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        const lv_image_dsc_t *descriptor = crazypod_icon_get(i);
        if(apps[i].tile == NULL)
            continue;
        if(descriptor != NULL && apps[i].image != NULL) {
            lv_image_set_src(apps[i].image, descriptor);
            lv_image_set_src(apps[i].reflection_image, descriptor);
            lv_obj_remove_flag(apps[i].image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(apps[i].reflection_clip,
                               LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(apps[i].symbol_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(apps[i].tile, LV_OPA_TRANSP, 0);
        }
        else {
            lv_obj_add_flag(apps[i].image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(apps[i].reflection_clip,
                            LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(apps[i].symbol_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(apps[i].tile, LV_OPA_COVER, 0);
        }
    }
    refresh_desktop_capsule_material();
    desktop_native_backdrop_ready = false;
    layout_desktop_carousel(false);
    lv_obj_invalidate(desktop_screen);
}

static void format_time_ms(unsigned long milliseconds, char *buffer,
                           size_t size)
{
    unsigned long seconds = milliseconds / 1000;
    unsigned minutes = (unsigned)((seconds / 60) % 10000);
    unsigned remainder = (unsigned)(seconds % 60);
    snprintf(buffer, size, "%u:%02u", minutes, remainder);
}

static const struct crazypod_track *current_track(void)
{
    const char *path = crazypod_queue_path(crazypod_queue_index());
    int index = crazypod_music_find_track(path);
    return crazypod_music_track(index);
}

static int initial_album_index(void)
{
    const struct crazypod_track *track = current_track();
    int count = crazypod_music_album_count();
    int i;

    if(track == NULL || count <= 0)
        return 0;
    for(i = 0; i < count; ++i) {
        const struct crazypod_album *album = crazypod_music_album(i);
        if(album != NULL &&
           strcmp(album->title, track->album) == 0 &&
           strcmp(album->artist, track->album_artist) == 0)
            return i;
    }
    return 0;
}

static void update_status_bars(lv_timer_t *timer)
{
    char time_text[8];
    struct tm *now = get_time();
    int level = battery_level();
    bool charging = false;
    bool playing = (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
                   (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    int i;

    (void)timer;
    if(level < 0)
        level = 0;
    if(level > 100)
        level = 100;

#if CONFIG_CHARGING >= CHARGING_MONITOR
    charging = charge_state > DISCHARGING;
#endif

    snprintf(time_text, sizeof(time_text), "%02d:%02d",
             now->tm_hour, now->tm_min);

    for(i = 0; i < CRAZYPOD_SCREEN_COUNT; ++i) {
        lv_label_set_text(status_bars[i].time, time_text);
        lv_obj_set_width(status_bars[i].battery_fill,
                         level > 0 ? 3 + (21 * level / 100) : 0);
        if(charging)
            lv_obj_remove_flag(status_bars[i].charge, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(status_bars[i].charge, LV_OBJ_FLAG_HIDDEN);
        if(playing)
            lv_obj_remove_flag(status_bars[i].playing, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(status_bars[i].playing, LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_status_bar(lv_obj_t *screen, struct status_bar *bar)
{
    lv_obj_t *battery;
    lv_obj_t *battery_cap;

    bar->time = make_label(screen, "00:00", &lv_font_montserrat_12,
                           COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(bar->time, 34, 10);

    bar->playing = make_label(screen, LV_SYMBOL_PLAY,
                              &lv_font_montserrat_10,
                              COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(bar->playing, 241, 11);
    lv_obj_add_flag(bar->playing, LV_OBJ_FLAG_HIDDEN);

    battery = make_box(screen, 258, 11, 27, 12, 3, COLOR_WHITE, 64);
    bar->battery_fill = make_box(battery, 1, 1, 24, 10, 2,
                                 COLOR_WHITE, LV_OPA_COVER);
    bar->charge = make_label(battery, LV_SYMBOL_CHARGE,
                             &lv_font_montserrat_8,
                             COLOR_DETAIL, LV_OPA_COVER);
    lv_obj_center(bar->charge);

    battery_cap = make_box(screen, 287, 15, 2, 5, 1, COLOR_WHITE, 128);
    (void)battery_cap;
}

static int appearance_tile_size(void)
{
    static const int sizes[] = { 88, 96, 104, 112, 120 };
    return sizes[crazypod_appearance_get()->icon_scale];
}

static int interpolate_pose(const int *values, int absolute_q8)
{
    int segment = absolute_q8 / 256;
    int fraction = absolute_q8 & 255;

    if(segment >= 3)
        return values[3];
    return values[segment] +
           (values[segment + 1] - values[segment]) * fraction / 256;
}

static void update_desktop_selection_chrome(void)
{
    int indicator_x = 95;
    int i;

    if(desktop_title != NULL)
        lv_label_set_text(desktop_title, apps[selected_app].name);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        int width = i == selected_app ? 14 : 5;
        if(desktop_indicators[i] == NULL)
            continue;
        lv_obj_set_pos(desktop_indicators[i], indicator_x, 166);
        lv_obj_set_size(desktop_indicators[i], width, 4);
        lv_obj_set_style_bg_opa(desktop_indicators[i],
                                i == selected_app ? LV_OPA_COVER : 89, 0);
        indicator_x += width + 4;
    }
}

static fb_data desktop_blend565(fb_data foreground, fb_data background,
                                int alpha)
{
    int fr = RGB_UNPACK_RED(foreground);
    int fg = RGB_UNPACK_GREEN(foreground);
    int fb = RGB_UNPACK_BLUE(foreground);
    int br = RGB_UNPACK_RED(background);
    int bg = RGB_UNPACK_GREEN(background);
    int bb = RGB_UNPACK_BLUE(background);

    return LCD_RGBPACK(
        (fr * alpha + br * (256 - alpha)) >> 8,
        (fg * alpha + bg * (256 - alpha)) >> 8,
        (fb * alpha + bb * (256 - alpha)) >> 8);
}

static void draw_desktop_placeholder(int app_index, int center_x,
                                     int center_y, int size, int opacity)
{
    fb_data *pixels = (fb_data *)lcd_framebuffer_default.data;
    uint32_t rgb = apps[app_index].color;
    fb_data color = LCD_RGBPACK((rgb >> 16) & 0xff,
                                (rgb >> 8) & 0xff,
                                rgb & 0xff);
    int left = center_x - size / 2;
    int top = center_y - size / 2;
    int y;

    for(y = 0; y < size; ++y) {
        int py = top + y;
        int x;
        if(py < CRAZYPOD_DESKTOP_NATIVE_TOP ||
           py >= CRAZYPOD_DESKTOP_NATIVE_BOTTOM)
            continue;
        for(x = 0; x < size; ++x) {
            int px = left + x;
            fb_data *destination;
            if(px < 0 || px >= LCD_WIDTH)
                continue;
            destination = pixels + py * LCD_WIDTH + px;
            *destination = desktop_blend565(
                color, *destination, opacity);
        }
    }
}

static inline void sample_icon_bilinear(
    const uint8_t *source, int source_width, int source_height,
    int source_stride, int source_x_q16, int source_y_q16,
    uint8_t *filtered)
{
    const uint8_t *samples[4];
    uint32_t weights[4];
    int sx = source_x_q16 >> 16;
    int sy = source_y_q16 >> 16;
    int sx1;
    int sy1;
    int fx;
    int fy;
    int channel;

    if(sx < 0)
        sx = 0;
    if(sy < 0)
        sy = 0;
    if(sx >= source_width)
        sx = source_width - 1;
    if(sy >= source_height)
        sy = source_height - 1;
    sx1 = sx + 1 < source_width ? sx + 1 : sx;
    sy1 = sy + 1 < source_height ? sy + 1 : sy;
    fx = (source_x_q16 >> 8) & 255;
    fy = (source_y_q16 >> 8) & 255;
    weights[0] = (uint32_t)(256 - fx) * (256 - fy);
    weights[1] = (uint32_t)fx * (256 - fy);
    weights[2] = (uint32_t)(256 - fx) * fy;
    weights[3] = (uint32_t)fx * fy;
    samples[0] = source + sy * source_stride + sx * 4;
    samples[1] = source + sy * source_stride + sx1 * 4;
    samples[2] = source + sy1 * source_stride + sx * 4;
    samples[3] = source + sy1 * source_stride + sx1 * 4;
    for(channel = 0; channel < 4; ++channel) {
        uint32_t sum =
            samples[0][channel] * weights[0] +
            samples[1][channel] * weights[1] +
            samples[2][channel] * weights[2] +
            samples[3][channel] * weights[3];
        filtered[channel] = sum >> 16;
    }
}

static inline fb_data blend_icon_premultiplied(
    const uint8_t *color, fb_data background, int opacity)
{
    int scale = opacity + 1;
    int alpha = color[3] * scale >> 8;
    int red = color[2] * scale >> 8;
    int green = color[1] * scale >> 8;
    int blue = color[0] * scale >> 8;
    int inverse = 256 - alpha;

    red += RGB_UNPACK_RED(background) * inverse >> 8;
    green += RGB_UNPACK_GREEN(background) * inverse >> 8;
    blue += RGB_UNPACK_BLUE(background) * inverse >> 8;
    return LCD_RGBPACK(red, green, blue);
}

static void draw_desktop_icon(int app_index, int center_x, int center_y,
                              int size, int opacity)
{
    const lv_image_dsc_t *image = crazypod_icon_get(app_index);
    const uint8_t *source =
        crazypod_icon_get_premultiplied(app_index);
    fb_data *pixels = (fb_data *)lcd_framebuffer_default.data;
    int source_width;
    int source_height;
    int source_stride;
    int left = center_x - size / 2;
    int top = center_y - size / 2;
    int source_y_q16;
    int source_y_step;
    int y;

    if(image == NULL || source == NULL ||
       image->header.cf != LV_COLOR_FORMAT_ARGB8888) {
        draw_desktop_placeholder(app_index, center_x, center_y,
                                 size, opacity);
        return;
    }
    source_width = image->header.w;
    source_height = image->header.h;
    source_stride = image->header.stride;
    source_y_step = (source_height << 16) / size;
    source_y_q16 =
        ((source_height << 15) / size) - 32768;
    for(y = 0; y < size; ++y) {
        int py = top + y;
        int source_x_q16 =
            ((source_width << 15) / size) - 32768;
        int source_x_step = (source_width << 16) / size;
        int x;
        if(py < CRAZYPOD_DESKTOP_NATIVE_TOP ||
           py >= CRAZYPOD_DESKTOP_NATIVE_BOTTOM) {
            source_y_q16 += source_y_step;
            continue;
        }
        for(x = 0; x < size; ++x) {
            int px = left + x;
            uint8_t filtered[4];
            fb_data *destination;
            if(px >= 0 && px < LCD_WIDTH) {
                sample_icon_bilinear(
                    source, source_width, source_height,
                    source_stride, source_x_q16,
                    source_y_q16, filtered);
                if(filtered[3] > 0) {
                    destination =
                        pixels + py * LCD_WIDTH + px;
                    *destination =
                        blend_icon_premultiplied(
                            filtered, *destination, opacity);
                }
            }
            source_x_q16 += source_x_step;
        }
        source_y_q16 += source_y_step;
    }

    {
        int reflection_height = size > 80 ? 12 : 9;
        int reflection_top = center_y + size / 2 + 2;
        int reflection_y;
        for(reflection_y = 0;
            reflection_y < reflection_height;
            ++reflection_y) {
            int py = reflection_top + reflection_y;
            int source_y_reflected_q16 =
                ((source_height - 1) << 16) -
                ((reflection_y * source_height) << 16) /
                    (reflection_height * 2);
            int source_x_q16 =
                ((source_width << 15) / size) - 32768;
            int source_x_step = (source_width << 16) / size;
            int fade = (reflection_height - reflection_y) * 42 /
                       reflection_height;
            int x;
            if(py < CRAZYPOD_DESKTOP_NATIVE_TOP ||
               py >= CRAZYPOD_DESKTOP_NATIVE_BOTTOM)
                continue;
            for(x = 0; x < size; ++x) {
                int px = left + x;
                uint8_t filtered[4];
                fb_data *destination;
                if(px >= 0 && px < LCD_WIDTH) {
                    sample_icon_bilinear(
                        source, source_width, source_height,
                        source_stride, source_x_q16,
                        source_y_reflected_q16, filtered);
                    if(filtered[3] > 0) {
                        destination =
                            pixels + py * LCD_WIDTH + px;
                        *destination =
                            blend_icon_premultiplied(
                                filtered, *destination,
                                opacity * fade >> 8);
                    }
                }
                source_x_q16 += source_x_step;
            }
        }
    }
}

static void render_desktop_carousel_native(void)
{
    static const int size_percent[] = { 100, 72, 56, 44 };
    static const int opacity_pose[] = { 255, 220, 150, 80 };
    fb_data *framebuffer =
        (fb_data *)lcd_framebuffer_default.data;
    int base_size = appearance_tile_size();
    int distance_layer;
    int row;

    if(product_active || !desktop_native_dirty)
        return;

    if(!desktop_native_backdrop_ready) {
        for(row = 0; row < CRAZYPOD_DESKTOP_NATIVE_HEIGHT; ++row) {
            memcpy(desktop_native_backdrop + row * LCD_WIDTH,
                   framebuffer +
                       (CRAZYPOD_DESKTOP_NATIVE_TOP + row) * LCD_WIDTH,
                   LCD_WIDTH * sizeof(fb_data));
        }
        desktop_native_backdrop_ready = true;
    }
    for(row = 0; row < CRAZYPOD_DESKTOP_NATIVE_HEIGHT; ++row) {
        memcpy(framebuffer +
                   (CRAZYPOD_DESKTOP_NATIVE_TOP + row) * LCD_WIDTH,
               desktop_native_backdrop + row * LCD_WIDTH,
               LCD_WIDTH * sizeof(fb_data));
    }

    for(distance_layer = 2; distance_layer >= 0; --distance_layer) {
        int i;
        for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
            int distance_q8 = i * 256 - desktop_position_q8;
            int absolute_q8 =
                distance_q8 < 0 ? -distance_q8 : distance_q8;
            int rounded_distance = (absolute_q8 + 128) / 256;
            int direction = distance_q8 < 0 ? -1 : 1;
            int center_x;
            int center_y;
            int icon_size;
            int opacity;

            if(absolute_q8 > 640 ||
               rounded_distance != distance_layer)
                continue;
            center_x = 160 + direction *
                (absolute_q8 <= 256
                    ? 92 * absolute_q8 / 256
                    : 92 + 46 * (absolute_q8 - 256) / 256);
            center_y = 91 + 4 * absolute_q8 / 256;
            icon_size = base_size *
                interpolate_pose(size_percent, absolute_q8) / 100;
            opacity = interpolate_pose(opacity_pose, absolute_q8);
            draw_desktop_icon(i, center_x, center_y,
                              icon_size, opacity);
        }
    }

    lcd_update_rect(0, CRAZYPOD_DESKTOP_NATIVE_TOP, LCD_WIDTH,
                    CRAZYPOD_DESKTOP_NATIVE_HEIGHT);
    desktop_native_dirty = false;
}

static void layout_desktop_carousel(bool animated)
{
    update_desktop_selection_chrome();
    if(animated) {
        desktop_motion_active = true;
        keep_cpu_boosted(HZ / 4);
    }
    else {
        desktop_position_q8 = selected_app * 256;
        desktop_velocity_q8 = 0;
        desktop_motion_active = false;
    }
    desktop_native_dirty = true;
}

static void tick_desktop_carousel(void)
{
    int target;
    int delta;

    if(!desktop_motion_active ||
       (last_desktop_motion_tick != 0 &&
        current_tick - last_desktop_motion_tick <
            CRAZYPOD_MOTION_FRAME_TICKS))
        return;

    target = selected_app * 256;
    delta = target - desktop_position_q8;
    if(delta == 0 && desktop_velocity_q8 == 0) {
        desktop_motion_active = false;
        return;
    }
    desktop_velocity_q8 =
        desktop_velocity_q8 * 8 / 16 + delta * 3 / 16;
    if(desktop_velocity_q8 == 0 && delta != 0)
        desktop_velocity_q8 = delta < 0 ? -1 : 1;
    desktop_position_q8 += desktop_velocity_q8;
    delta = target - desktop_position_q8;
    if((delta < 0 ? -delta : delta) <= 2 &&
       (desktop_velocity_q8 < 0
            ? -desktop_velocity_q8
            : desktop_velocity_q8) <= 2) {
        desktop_position_q8 = target;
        desktop_velocity_q8 = 0;
        desktop_motion_active = false;
    }
    desktop_native_dirty = true;
    last_desktop_motion_tick = current_tick;
}

static void app_focus_event(lv_event_t *event)
{
    struct crazypod_app *app = lv_event_get_user_data(event);

    if(lv_event_get_code(event) != LV_EVENT_FOCUSED)
        return;
    selected_app = (int)(app - apps);
    layout_desktop_carousel(true);
}

static void move_desktop_selection(int direction)
{
    int next = selected_app + direction;

    if(next < 0)
        next = 0;
    if(next >= CRAZYPOD_APP_COUNT)
        next = CRAZYPOD_APP_COUNT - 1;
    if(next == selected_app)
        return;
    selected_app = next;
    layout_desktop_carousel(true);
}

static void create_launcher_app(int index)
{
    struct crazypod_app *app = &apps[index];
    const lv_image_dsc_t *descriptor = crazypod_icon_get(index);

    app->cell = lv_obj_create(desktop_carousel);
    set_plain_object(app->cell);
    lv_obj_set_size(app->cell, 120, 110);
    lv_obj_set_style_bg_opa(app->cell, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(app->cell, LV_OBJ_FLAG_CLICKABLE);

    app->tile = make_box(app->cell, 0, 0, 120, 100, 18, app->color,
                         descriptor != NULL ? LV_OPA_TRANSP : LV_OPA_COVER);
    lv_obj_set_style_shadow_width(app->tile,
                                  descriptor != NULL ? 0 : 18, 0);
    lv_obj_set_style_shadow_offset_y(app->tile, 10, 0);
    lv_obj_set_style_shadow_color(app->tile, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(app->tile, 110, 0);
    lv_obj_remove_flag(app->tile, LV_OBJ_FLAG_CLICKABLE);

    app->image = lv_image_create(app->tile);
    if(descriptor != NULL)
        lv_image_set_src(app->image, descriptor);
    lv_obj_center(app->image);
    lv_obj_remove_flag(app->image, LV_OBJ_FLAG_CLICKABLE);

    app->reflection_clip = lv_obj_create(app->cell);
    set_plain_object(app->reflection_clip);
    lv_obj_set_style_bg_opa(app->reflection_clip, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(app->reflection_clip, LV_OBJ_FLAG_CLICKABLE);
    app->reflection_image = lv_image_create(app->reflection_clip);
    if(descriptor != NULL)
        lv_image_set_src(app->reflection_image, descriptor);
    lv_image_set_rotation(app->reflection_image, 1800);
    lv_obj_remove_flag(app->reflection_image, LV_OBJ_FLAG_CLICKABLE);

    app->symbol_label = make_label(
        app->tile, app->symbol, &lv_font_montserrat_16,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_center(app->symbol_label);
    if(descriptor != NULL) {
        lv_obj_add_flag(app->symbol_label, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(app->image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(app->reflection_clip, LV_OBJ_FLAG_HIDDEN);
    }

    lv_group_add_obj(desktop_group, app->cell);
    lv_obj_add_event_cb(app->cell, app_focus_event, LV_EVENT_FOCUSED, app);
}

static void create_now_playing_capsule(void)
{
    lv_obj_t *capsule;
    lv_obj_t *glass_border;
    lv_obj_t *progress_track;
    lv_obj_t *wave_ball;
    lv_obj_t *wave;

    desktop_capsule = make_box(desktop_screen, 8, 174, 304, 58, 29,
                               COLOR_WHITE, LV_OPA_TRANSP);
    capsule = desktop_capsule;
    lv_obj_set_style_clip_corner(capsule, true, 0);
    desktop_capsule_glass = lv_image_create(capsule);
    lv_obj_set_pos(desktop_capsule_glass, 0, 0);
    lv_obj_remove_flag(desktop_capsule_glass, LV_OBJ_FLAG_CLICKABLE);
    refresh_desktop_capsule_material();

    desktop_capsule_artwork = make_box(
        capsule, 9, 8, 42, 42, 9, 0x941FFC, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        desktop_capsule_artwork, lv_color_hex(0x2E5CFA), 0);
    lv_obj_set_style_bg_grad_dir(
        desktop_capsule_artwork, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_clip_corner(desktop_capsule_artwork, true, 0);

    desktop_capsule_artwork_image =
        lv_image_create(desktop_capsule_artwork);
    lv_obj_center(desktop_capsule_artwork_image);
    lv_obj_remove_flag(desktop_capsule_artwork_image,
                       LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(desktop_capsule_artwork_image, LV_OBJ_FLAG_HIDDEN);

    desktop_capsule_artwork_symbol = make_label(
        desktop_capsule_artwork, LV_SYMBOL_AUDIO,
        &lv_font_montserrat_16, COLOR_WHITE, 220);
    lv_obj_center(desktop_capsule_artwork_symbol);
    desktop_capsule_artwork_path[0] = '\0';

    desktop_capsule_track = make_label(
        capsule, "No Track", CRAZYPOD_METADATA_FONT,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(desktop_capsule_track, 60, 7);
    lv_obj_set_width(desktop_capsule_track, 171);
    lv_obj_set_height(desktop_capsule_track, 17);
    lv_label_set_long_mode(desktop_capsule_track, LV_LABEL_LONG_MODE_DOTS);

    desktop_capsule_artist = make_label(
        capsule, "Local Music", CRAZYPOD_METADATA_FONT,
        COLOR_WHITE, 190);
    lv_obj_set_pos(desktop_capsule_artist, 60, 25);
    lv_obj_set_width(desktop_capsule_artist, 171);
    lv_obj_set_height(desktop_capsule_artist, 17);
    lv_label_set_long_mode(desktop_capsule_artist,
                           LV_LABEL_LONG_MODE_DOTS);

    progress_track = make_box(capsule, 60, 45, 171, 3,
                              LV_RADIUS_CIRCLE, COLOR_WHITE, 31);
    desktop_capsule_progress = make_box(
        progress_track, 0, 0, 6, 3, LV_RADIUS_CIRCLE,
        0x2ECC71, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(desktop_capsule_progress,
                                   lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_bg_grad_dir(desktop_capsule_progress,
                                 LV_GRAD_DIR_HOR, 0);

    wave_ball = make_box(capsule, 245, 8, 42, 42,
                         LV_RADIUS_CIRCLE, 0x2ECC71, 215);
    lv_obj_set_style_bg_grad_color(wave_ball,
                                   lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_bg_grad_dir(wave_ball, LV_GRAD_DIR_HOR, 0);
    wave = make_label(wave_ball, LV_SYMBOL_VOLUME_MAX,
                      &lv_font_montserrat_16,
                      COLOR_WHITE, LV_OPA_COVER);
    lv_obj_center(wave);

    glass_border = make_box(capsule, 0, 0, 304, 58, 29,
                            COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(glass_border, 1, 0);
    lv_obj_set_style_border_color(
        glass_border, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(glass_border, 58, 0);
    lv_obj_remove_flag(glass_border, LV_OBJ_FLAG_CLICKABLE);
}

static void update_desktop_capsule_artwork(
    const struct crazypod_track *track)
{
    const lv_image_dsc_t *descriptor = NULL;

    if(desktop_capsule_artwork == NULL)
        return;

    if(track != NULL) {
        descriptor = crazypod_artwork_load(
            CRAZYPOD_CAPSULE_ARTWORK_SLOT, track, 42);
        if(strcmp(desktop_capsule_artwork_path, track->path) != 0) {
            snprintf(desktop_capsule_artwork_path,
                     sizeof(desktop_capsule_artwork_path),
                     "%s", track->path);
            lv_obj_set_style_bg_color(
                desktop_capsule_artwork,
                lv_color_hex(artwork_color(track->album, 0)), 0);
            lv_obj_set_style_bg_grad_color(
                desktop_capsule_artwork,
                lv_color_hex(artwork_color(track->artist, 1)), 0);
        }
    }
    else {
        if(desktop_capsule_artwork_path[0] == '\0')
            return;
        desktop_capsule_artwork_path[0] = '\0';
        lv_obj_set_style_bg_color(
            desktop_capsule_artwork, lv_color_hex(0x941FFC), 0);
        lv_obj_set_style_bg_grad_color(
            desktop_capsule_artwork, lv_color_hex(0x2E5CFA), 0);
    }

    if(descriptor != NULL) {
        lv_image_set_src(desktop_capsule_artwork_image, descriptor);
        lv_obj_remove_flag(desktop_capsule_artwork_image,
                           LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(desktop_capsule_artwork_symbol,
                        LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(desktop_capsule_artwork_image,
                        LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(desktop_capsule_artwork_symbol,
                           LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_desktop(void)
{
    const lv_image_dsc_t *wallpaper;
    int i;

    desktop_screen = lv_obj_create(NULL);
    set_plain_object(desktop_screen);
    lv_obj_set_style_bg_color(
        desktop_screen,
        lv_color_hex(crazypod_appearance_home_color()), 0);
    lv_obj_set_style_bg_opa(desktop_screen, LV_OPA_COVER, 0);
    wallpaper = crazypod_default_wallpaper();
    desktop_wallpaper = lv_image_create(desktop_screen);
    if(wallpaper != NULL)
        lv_image_set_src(desktop_wallpaper, wallpaper);
    if(wallpaper == NULL ||
       crazypod_appearance_get()->home_background != 0)
        lv_obj_add_flag(desktop_wallpaper, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(desktop_wallpaper, 0, 0);
    lv_obj_remove_flag(desktop_wallpaper, LV_OBJ_FLAG_CLICKABLE);
    create_status_bar(desktop_screen, &status_bars[0]);

    desktop_group = lv_group_create();
    lv_group_set_wrap(desktop_group, false);
    desktop_carousel = lv_obj_create(desktop_screen);
    set_plain_object(desktop_carousel);
    lv_obj_set_pos(desktop_carousel, 0, 42);
    lv_obj_set_size(desktop_carousel, LCD_WIDTH, 116);
    lv_obj_set_style_bg_opa(desktop_carousel, LV_OPA_TRANSP, 0);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i)
        create_launcher_app(i);
    lv_obj_add_flag(desktop_carousel, LV_OBJ_FLAG_HIDDEN);

    desktop_title = make_label(desktop_screen, apps[0].name,
                               &lv_font_montserrat_16,
                               COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(desktop_title, LCD_WIDTH);
    lv_obj_set_style_text_align(desktop_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(desktop_title, 0, 143);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        desktop_indicators[i] = make_box(
            desktop_screen, 0, 166, 5, 4,
            LV_RADIUS_CIRCLE, COLOR_WHITE, 89);
    }
    create_now_playing_capsule();
    selected_app = 0;
    desktop_native_dirty = true;
    desktop_native_backdrop_ready = false;
    layout_desktop_carousel(false);
    lv_group_focus_obj(apps[0].cell);
}

static void create_product_screen(void)
{
    product_screen = lv_obj_create(desktop_screen);
    set_plain_object(product_screen);
    lv_obj_set_pos(product_screen, 0, 0);
    lv_obj_set_size(product_screen, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_color(product_screen, lv_color_hex(COLOR_DETAIL), 0);
    lv_obj_set_style_bg_opa(product_screen, LV_OPA_COVER, 0);

    product_content = lv_obj_create(product_screen);
    set_plain_object(product_content);
    lv_obj_set_pos(product_content, 0, 0);
    lv_obj_set_size(product_content, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(product_content, LV_OPA_TRANSP, 0);
    create_status_bar(product_screen, &status_bars[1]);

    lv_obj_add_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(product_screen);
}

static struct route_state *current_route(void)
{
    return &route_stack[route_depth - 1];
}

static int route_item_count(const struct route_state *state)
{
    switch(state->route) {
    case MUSIC_ROUTE_MENU:
        return 8;
    case MUSIC_ROUTE_ALBUM_FLOW:
    case MUSIC_ROUTE_ALBUMS:
        return crazypod_music_album_count();
    case MUSIC_ROUTE_ALL:
    case MUSIC_ROUTE_SONGS:
        return crazypod_music_track_count();
    case MUSIC_ROUTE_SEARCH:
        return CRAZYPOD_EDITOR_CHAR_COUNT + 4;
    case MUSIC_ROUTE_SEARCH_RESULTS:
        return crazypod_music_search_count(search_query);
    case MUSIC_ROUTE_PLAYLISTS:
        return crazypod_music_playlist_count();
    case MUSIC_ROUTE_PLAYLIST_SONGS: {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(state->group);
        return playlist != NULL ? playlist->track_count : 0;
    }
    case MUSIC_ROUTE_ARTISTS:
        return crazypod_music_artist_count();
    case MUSIC_ROUTE_ARTIST_SONGS:
        return crazypod_music_artist_track_count(state->group);
    case MUSIC_ROUTE_ALBUM_SONGS:
        return crazypod_music_album_track_count(state->group);
    case MUSIC_ROUTE_QUEUE:
        return crazypod_queue_count();
    case MUSIC_ROUTE_NOW_PLAYING:
        return 1;
    case DIY_ROUTE_MENU:
        return 4;
    case DIY_ROUTE_PRESETS:
        return 3;
    case DIY_ROUTE_PRESET_LIBRARY:
        return crazypod_preset_count();
    case DIY_ROUTE_PRESET_ACTIONS: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);
        return preset != NULL && preset->builtin ? 2 : 3;
    }
    case DIY_ROUTE_PRESET_EDIT:
        return 3;
    case DIY_ROUTE_PRESET_RENAME:
        return CRAZYPOD_EDITOR_CHAR_COUNT + 3;
    case DIY_ROUTE_ICONS:
        return CRAZYPOD_ICON_THEME_COUNT;
    case DIY_ROUTE_DETAILS:
        return 6;
    case DIY_ROUTE_BACKGROUNDS:
        return 2;
    }
    return 0;
}

static const struct crazypod_track *route_track(
    const struct route_state *state, int index)
{
    switch(state->route) {
    case MUSIC_ROUTE_ALL:
    case MUSIC_ROUTE_SONGS:
        return crazypod_music_track(index);
    case MUSIC_ROUTE_SEARCH_RESULTS:
        return crazypod_music_search_track(search_query, index);
    case MUSIC_ROUTE_PLAYLIST_SONGS:
        return crazypod_music_playlist_track(state->group, index);
    case MUSIC_ROUTE_ARTIST_SONGS:
        return crazypod_music_artist_track(state->group, index);
    case MUSIC_ROUTE_ALBUM_SONGS:
        return crazypod_music_album_track(state->group, index);
    case MUSIC_ROUTE_QUEUE: {
        const char *path = crazypod_queue_path(index);
        return crazypod_music_track(crazypod_music_find_track(path));
    }
    default:
        return NULL;
    }
}

static const char *route_item_title(const struct route_state *state, int index)
{
    const struct crazypod_track *track;
    switch(state->route) {
    case MUSIC_ROUTE_MENU:
        return index >= 0 && index < 8 ? music_menu_titles[index] : "";
    case MUSIC_ROUTE_SEARCH:
        if(index >= 0 && index < CRAZYPOD_EDITOR_CHAR_COUNT)
            return editor_characters[index];
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT)
            return "Space";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            return "Backspace";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 2)
            return "Clear";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 3)
            return "Show Results";
        return "";
    case MUSIC_ROUTE_PLAYLISTS: {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(index);
        return playlist != NULL ? playlist->name : "";
    }
    case MUSIC_ROUTE_ARTISTS:
        return crazypod_music_artist(index);
    case MUSIC_ROUTE_ALBUMS: {
        const struct crazypod_album *album = crazypod_music_album(index);
        static char album_label[148];
        int i;
        bool duplicate_title = false;
        if(album == NULL)
            return "";
        for(i = 0; i < crazypod_music_album_count(); ++i) {
            const struct crazypod_album *other = crazypod_music_album(i);
            if(i != index && other != NULL &&
               strcmp(other->title, album->title) == 0) {
                duplicate_title = true;
                break;
            }
        }
        if(duplicate_title)
            snprintf(album_label, sizeof(album_label), "%s · %s",
                     album->artist, album->title);
        else
            snprintf(album_label, sizeof(album_label), "%s", album->title);
        return album_label;
    }
    case DIY_ROUTE_MENU:
        return index >= 0 && index < 4 ? diy_menu_titles[index] : "";
    case DIY_ROUTE_PRESETS:
        return index == 0 ? "Save" :
               index == 1 ? "Saved" :
               index == 2 ? "Import" : "";
    case DIY_ROUTE_PRESET_LIBRARY: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(index);
        return preset != NULL ? preset->name : "";
    }
    case DIY_ROUTE_PRESET_ACTIONS: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);
        if(preset != NULL && preset->builtin)
            return index == 0 ? "Apply" : index == 1 ? "Export" : "";
        return index >= 0 && index < 3 ? preset_actions[index] : "";
    }
    case DIY_ROUTE_PRESET_EDIT:
        return index >= 0 && index < 3
            ? preset_edit_actions[index] : "";
    case DIY_ROUTE_PRESET_RENAME:
        if(index >= 0 && index < CRAZYPOD_EDITOR_CHAR_COUNT)
            return editor_characters[index];
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT)
            return "Space";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            return "Backspace";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 2)
            return "Save Name";
        return "";
    case DIY_ROUTE_ICONS:
        return crazypod_icon_theme_name(index);
    case DIY_ROUTE_DETAILS:
        return index >= 0 && index < 6 ? diy_detail_titles[index] : "";
    case DIY_ROUTE_BACKGROUNDS:
        return index >= 0 && index < 2
            ? diy_background_titles[index] : "";
    default:
        track = route_track(state, index);
        return track != NULL ? track->title : "";
    }
}

static const char *route_title(const struct route_state *state)
{
    switch(state->route) {
    case MUSIC_ROUTE_MENU: return "MUSIC";
    case MUSIC_ROUTE_ALL: return "ALL MUSIC";
    case MUSIC_ROUTE_PLAYLISTS: return "PLAYLISTS";
    case MUSIC_ROUTE_PLAYLIST_SONGS: {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(state->group);
        return playlist != NULL ? playlist->name : "PLAYLIST";
    }
    case MUSIC_ROUTE_ARTISTS: return "ARTISTS";
    case MUSIC_ROUTE_ARTIST_SONGS:
        return crazypod_music_artist(state->group);
    case MUSIC_ROUTE_ALBUMS: return "ALBUMS";
    case MUSIC_ROUTE_ALBUM_SONGS: {
        const struct crazypod_album *album =
            crazypod_music_album(state->group);
        return album != NULL ? album->title : "ALBUM";
    }
    case MUSIC_ROUTE_SONGS: return "SONGS";
    case MUSIC_ROUTE_SEARCH: return "SEARCH";
    case MUSIC_ROUTE_SEARCH_RESULTS: return "RESULTS";
    case MUSIC_ROUTE_QUEUE: return "UP NEXT";
    case MUSIC_ROUTE_ALBUM_FLOW: return "ALBUMS";
    case MUSIC_ROUTE_NOW_PLAYING: return "NOW PLAYING";
    case DIY_ROUTE_MENU: return "CUSTOMIZE";
    case DIY_ROUTE_PRESETS: return "PRESETS";
    case DIY_ROUTE_PRESET_LIBRARY: return "SAVED";
    case DIY_ROUTE_PRESET_ACTIONS: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);
        return preset != NULL ? preset->name : "PRESET";
    }
    case DIY_ROUTE_PRESET_EDIT: return "EDIT";
    case DIY_ROUTE_PRESET_RENAME: return "RENAME";
    case DIY_ROUTE_ICONS: return "ICONS";
    case DIY_ROUTE_DETAILS: return "DETAILS";
    case DIY_ROUTE_BACKGROUNDS: return "BACKGROUNDS";
    }
    return "";
}

static lv_obj_t *create_artwork_cached(lv_obj_t *parent,
                                       const struct crazypod_track *track,
                                       int x, int y, int display_size,
                                       int cache_size, int slot)
{
    lv_obj_t *card = make_box(parent, x, y,
                              display_size, display_size,
                              display_size > 80 ? 0 : 7,
                              artwork_color(track != NULL ? track->album : "",
                                            0),
                              LV_OPA_COVER);
    const lv_image_dsc_t *descriptor =
        track != NULL
            ? crazypod_artwork_load(slot, track, cache_size) : NULL;

    lv_obj_set_style_bg_grad_color(
        card, lv_color_hex(artwork_color(track != NULL ? track->artist : "",
                                         1)), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_shadow_width(
        card, display_size > 80 ? 12 : 5, 0);
    lv_obj_set_style_shadow_offset_y(
        card, display_size > 80 ? 6 : 2, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(
        card, display_size > 80 ? 100 : 55, 0);
    lv_obj_set_style_clip_corner(card, true, 0);

    if(descriptor != NULL) {
        lv_obj_t *image = lv_image_create(card);
        lv_image_set_src(image, descriptor);
        if(descriptor->header.w != display_size)
            lv_image_set_scale(
                image,
                (uint32_t)display_size * LV_SCALE_NONE /
                    descriptor->header.w);
        lv_obj_center(image);
    }
    else {
        lv_obj_t *symbol = make_label(card, LV_SYMBOL_AUDIO,
                                      display_size > 80
                                          ? &lv_font_montserrat_24
                                          : &lv_font_montserrat_16,
                                      COLOR_WHITE, 210);
        lv_obj_center(symbol);
    }
    return card;
}

static lv_obj_t *create_artwork(lv_obj_t *parent,
                                const struct crazypod_track *track,
                                int x, int y, int size, int slot)
{
    return create_artwork_cached(parent, track, x, y, size, size, slot);
}

static void create_panel_backgrounds(void)
{
    lv_obj_t *left = make_box(product_content, 8, 40, 148, 192, 12,
                              crazypod_appearance_menu_color(), 230);
    lv_obj_t *right = make_box(product_content, 164, 40, 148, 192, 12,
                               crazypod_appearance_menu_color(), 184);
    lv_obj_set_style_border_width(left, 1, 0);
    lv_obj_set_style_border_color(left, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(left, 22, 0);
    lv_obj_set_style_border_width(right, 1, 0);
    lv_obj_set_style_border_color(right, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(right, 22, 0);
}

static void render_empty_state(const char *title, const char *message)
{
    lv_obj_t *symbol;
    lv_obj_t *label;

    symbol = make_label(product_content, LV_SYMBOL_AUDIO,
                        &lv_font_montserrat_24, COLOR_WHITE, 80);
    lv_obj_set_pos(symbol, 148, 96);
    label = make_label(product_content, title, &lv_font_montserrat_12,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 260);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 30, 137);
    label = make_label(product_content, message, &lv_font_montserrat_8,
                       COLOR_WHITE, 105);
    lv_obj_set_width(label, 260);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 30, 158);
}

static void render_root_preview(int selected)
{
    lv_obj_t *title;
    lv_obj_t *detail;
    char count_text[96];
    int count = 0;

    switch(selected) {
    case 0: {
        const struct crazypod_track *track = current_track();
        create_artwork(product_content, track, 200, 76, 80,
                       CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        title = make_label(product_content,
                           track != NULL ? track->title : "No Track",
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, LV_OPA_COVER);
        lv_obj_set_pos(title, 177, 167);
        lv_obj_set_width(title, 126);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
        detail = make_label(product_content,
                            track != NULL ? track->artist : "Local Music",
                            CRAZYPOD_METADATA_FONT,
                            COLOR_WHITE, 130);
        lv_obj_set_pos(detail, 177, 186);
        lv_obj_set_width(detail, 126);
        lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
        break;
    }
    case 1:
    {
        int album_index = initial_album_index();
        count = crazypod_music_album_count();
        create_artwork_cached(
            product_content,
            count > 0
                ? crazypod_music_album_track(album_index, 0) : NULL,
            204, 82, 72, 120,
            CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        crazypod_coverflow_warm(album_index);
        break;
    }
    case 2: count = crazypod_music_track_count(); break;
    case 3: count = crazypod_music_playlist_count(); break;
    case 4: count = crazypod_music_artist_count(); break;
    case 5: count = crazypod_music_album_count(); break;
    case 6: count = crazypod_music_track_count(); break;
    case 7: count = crazypod_music_track_count(); break;
    }

    if(selected != 0) {
        title = make_label(product_content, music_menu_titles[selected],
                           &lv_font_montserrat_12,
                           COLOR_WHITE, LV_OPA_COVER);
        lv_obj_set_width(title, 132);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(title, 174, selected == 1 ? 166 : 111);
        if(selected == 7)
            snprintf(count_text, sizeof(count_text),
                     "%d local tracks", count);
        else
            snprintf(count_text, sizeof(count_text), "%d %s", count,
                     selected == 3 ? "playlists" :
                     selected == 4 ? "artists" :
                     selected == 5 || selected == 1 ? "albums" : "songs");
        detail = make_label(product_content, count_text,
                            &lv_font_montserrat_8,
                            COLOR_WHITE, 125);
        lv_obj_set_width(detail, 132);
        lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(detail, 174, selected == 1 ? 187 : 136);
        if(selected != 1) {
            lv_obj_t *symbol = make_label(
                product_content, music_menu_symbols[selected],
                &lv_font_montserrat_24, COLOR_CYAN, 205);
            lv_obj_set_pos(symbol, 228, 79);
        }
    }
}

static void render_item_preview(const struct route_state *state)
{
    const struct crazypod_track *track =
        route_track(state, state->selected);
    lv_obj_t *title;
    lv_obj_t *detail;
    char text[96];

    if(state->route == MUSIC_ROUTE_ARTISTS) {
        const char *artist = crazypod_music_artist(state->selected);
        int count = crazypod_music_artist_track_count(state->selected);
        lv_obj_t *symbol = make_label(product_content, LV_SYMBOL_HOME,
                                      &lv_font_montserrat_24,
                                      COLOR_CYAN, 210);
        lv_obj_set_pos(symbol, 228, 83);
        title = make_label(product_content, artist != NULL ? artist : "",
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, LV_OPA_COVER);
        snprintf(text, sizeof(text), "%d songs", count);
        detail = make_label(product_content, text, &lv_font_montserrat_8,
                            COLOR_WHITE, 130);
    }
    else if(state->route == MUSIC_ROUTE_ALBUMS) {
        const struct crazypod_album *album =
            crazypod_music_album(state->selected);
        track = crazypod_music_album_track(state->selected, 0);
        create_artwork(product_content, track, 204, 76, 72,
                       CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        title = make_label(product_content,
                           album != NULL ? album->title : "",
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, LV_OPA_COVER);
        detail = make_label(product_content,
                            album != NULL ? album->artist : "",
                            CRAZYPOD_METADATA_FONT, COLOR_WHITE, 135);
    }
    else if(state->route == MUSIC_ROUTE_PLAYLISTS) {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(state->selected);
        lv_obj_t *symbol = make_label(product_content, LV_SYMBOL_LIST,
                                      &lv_font_montserrat_24,
                                      COLOR_CYAN, 210);
        lv_obj_set_pos(symbol, 228, 83);
        title = make_label(product_content,
                           playlist != NULL ? playlist->name : "",
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, LV_OPA_COVER);
        snprintf(text, sizeof(text), "%d songs",
                 playlist != NULL ? playlist->track_count : 0);
        detail = make_label(product_content, text, &lv_font_montserrat_8,
                            COLOR_WHITE, 130);
    }
    else {
        char duration[16];
        create_artwork(product_content, track, 204, 72, 72,
                       CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        title = make_label(product_content,
                           track != NULL ? track->title : "No Track",
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, LV_OPA_COVER);
        detail = make_label(product_content,
                            track != NULL ? track->artist : "",
                            CRAZYPOD_METADATA_FONT, COLOR_WHITE, 155);
        if(track != NULL) {
            format_time_ms(track->duration_ms, duration, sizeof(duration));
            snprintf(text, sizeof(text), "%s  " LV_SYMBOL_BULLET "  %s",
                     track->album, duration);
            {
                lv_obj_t *album = make_label(product_content, text,
                                              CRAZYPOD_METADATA_FONT,
                                              COLOR_WHITE, 95);
                lv_obj_set_width(album, 126);
                lv_obj_set_height(album, 16);
                lv_label_set_long_mode(album, LV_LABEL_LONG_MODE_DOTS);
                lv_obj_set_style_text_align(album, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_pos(album, 177, 207);
            }
        }
    }

    lv_obj_set_width(title, 126);
    lv_obj_set_height(title, 18);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 177, 158);
    lv_obj_set_width(detail, 126);
    lv_obj_set_height(detail, 16);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(detail, 177, 181);
}

static const char *diy_current_value(const struct route_state *state)
{
    const struct crazypod_appearance *value = crazypod_appearance_get();
    static const char *const icon_sizes[] = {
        "80%", "90%", "100%", "110%", "120%"
    };
    static const char *const glows[] = {
        "Off", "Low", "Medium", "High"
    };

    if(state->route == DIY_ROUTE_ICONS)
        return state->selected == value->icon_theme
            ? "Current selection" : "Select to switch now";
    if(state->route == DIY_ROUTE_DETAILS) {
        switch(state->selected) {
        case 0:
            return value->player_style == 0
                ? "Liquid Glass" : "Gaussian Blur";
        case 1: return icon_sizes[value->icon_scale];
        case 2: return glows[value->glow];
        case 3:
            return value->highlight_style == 0 ? "Solid" : "Gradient";
        case 4:
            return crazypod_appearance_color_name(value->primary_color);
        case 5:
            return crazypod_appearance_color_name(value->secondary_color);
        }
    }
    if(state->route == DIY_ROUTE_BACKGROUNDS) {
        int color = state->selected == 0
            ? value->home_background : value->menu_background;
        return color == 0 ? "Default"
                          : crazypod_appearance_color_name(color - 1);
    }
    return "";
}

static void render_editor_preview(const char *value, const char *empty_text,
                                  const char *detail)
{
    lv_obj_t *card;
    lv_obj_t *symbol;
    lv_obj_t *label;

    card = make_box(product_content, 181, 78, 118, 64, 14,
                    highlight_primary(), 210);
    lv_obj_set_style_bg_grad_color(
        card, lv_color_hex(highlight_secondary()), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_HOR, 0);
    symbol = make_label(card, LV_SYMBOL_KEYBOARD,
                        &lv_font_montserrat_16, COLOR_WHITE, 225);
    lv_obj_set_pos(symbol, 10, 9);
    label = make_label(card,
                       value != NULL && value[0] != '\0'
                           ? value : empty_text,
                       &lv_font_montserrat_12, COLOR_WHITE,
                       value != NULL && value[0] != '\0' ? 255 : 130);
    lv_obj_set_pos(label, 10, 35);
    lv_obj_set_width(label, 98);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    label = make_label(product_content, detail, &lv_font_montserrat_8,
                       COLOR_WHITE, 125);
    lv_obj_set_pos(label, 181, 158);
    lv_obj_set_width(label, 118);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

static void render_diy_preview(const struct route_state *state)
{
    const char *title = route_item_title(state, state->selected);
    const char *detail = diy_current_value(state);
    const char *symbol = LV_SYMBOL_SETTINGS;
    uint32_t swatch_color = highlight_primary();
    lv_obj_t *glyph;
    lv_obj_t *label;
    lv_obj_t *swatch;

    if(state->route == DIY_ROUTE_MENU) {
        symbol = diy_menu_symbols[state->selected];
        detail = state->selected == 0 ? "Save and reuse appearances" :
                 state->selected == 1 ? "16 complete icon themes" :
                 state->selected == 2 ? "Player, glow and highlights" :
                                        "Home and menu surfaces";
    }
    else if(state->route == DIY_ROUTE_PRESETS) {
        symbol = state->selected == 0 ? LV_SYMBOL_SAVE :
                 state->selected == 1 ? LV_SYMBOL_COPY :
                                        LV_SYMBOL_DOWNLOAD;
        detail = state->selected == 0
            ? "Store the complete current appearance"
            : state->selected == 1
                ? "Apply, export or edit saved appearances"
                : "Copy import.upodtheme to /.crazypod";
    }
    else if(state->route == DIY_ROUTE_PRESET_LIBRARY) {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->selected);
        symbol = LV_SYMBOL_COPY;
        detail = preset != NULL && preset->builtin
            ? "Built-in appearance" : "User appearance";
    }
    else if(state->route == DIY_ROUTE_PRESET_ACTIONS) {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);
        title = preset != NULL ? preset->name : "Preset";
        symbol = LV_SYMBOL_SAVE;
        detail = route_item_title(state, state->selected);
    }
    else if(state->route == DIY_ROUTE_PRESET_EDIT) {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);
        title = preset != NULL ? preset->name : "Preset";
        symbol = LV_SYMBOL_EDIT;
        detail = route_item_title(state, state->selected);
    }
    else if(state->route == DIY_ROUTE_PRESET_RENAME) {
        render_editor_preview(preset_name_editor, "New name",
                              "Wheel selects characters; center adds.");
        return;
    }
    else if(state->route == DIY_ROUTE_ICONS) {
        symbol = LV_SYMBOL_IMAGE;
    }
    else if(state->route == DIY_ROUTE_BACKGROUNDS) {
        int surface = state->selected == 0
            ? crazypod_appearance_get()->home_background
            : crazypod_appearance_get()->menu_background;
        symbol = LV_SYMBOL_DIRECTORY;
        swatch_color = surface == 0
            ? (state->selected == 0 ? 0x141419 : 0x08080D)
            : crazypod_appearance_color(surface - 1);
    }
    else if(state->route == DIY_ROUTE_DETAILS && state->selected == 5) {
        swatch_color = highlight_secondary();
    }

    swatch = make_box(product_content, 204, 76, 72, 72, 16,
                      swatch_color, LV_OPA_COVER);
    if(state->route != DIY_ROUTE_BACKGROUNDS &&
       crazypod_appearance_get()->highlight_style != 0) {
        lv_obj_set_style_bg_grad_color(
            swatch, lv_color_hex(highlight_secondary()), 0);
        lv_obj_set_style_bg_grad_dir(swatch, LV_GRAD_DIR_HOR, 0);
    }
    glyph = make_label(swatch, symbol, &lv_font_montserrat_24,
                       COLOR_WHITE, 225);
    lv_obj_center(glyph);

    label = make_label(product_content, title != NULL ? title : "",
                       &lv_font_montserrat_12,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 177, 163);
    label = make_label(product_content, detail, &lv_font_montserrat_8,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 177, 187);
}

static void render_menu_screen(const struct route_state *state)
{
    int count = route_item_count(state);
    int start;
    int row;
    lv_obj_t *header;

    menu_view.valid = true;
    menu_view.route = state->route;
    create_panel_backgrounds();
    header = make_label(product_content, route_title(state),
                        CRAZYPOD_METADATA_FONT,
                        COLOR_WHITE, 85);
    lv_obj_set_pos(header, 19, 40);
    lv_obj_set_width(header, 128);
    lv_obj_set_height(header, 16);
    lv_label_set_long_mode(header, LV_LABEL_LONG_MODE_DOTS);

    if(count <= 0) {
        render_empty_state("Nothing Here", "Add local music and rescan.");
        return;
    }

    start = count <= CRAZYPOD_VISIBLE_ROWS ? 0 :
            state->selected - CRAZYPOD_VISIBLE_ROWS / 2;
    if(start < 0)
        start = 0;
    if(start > count - CRAZYPOD_VISIBLE_ROWS)
        start = count - CRAZYPOD_VISIBLE_ROWS;

    for(row = 0; row < CRAZYPOD_VISIBLE_ROWS; ++row) {
        int index = start + row;
        int y = 52 + row * 25;
        bool selected = index == state->selected;
        lv_obj_t *row_box;
        lv_obj_t *label;
        lv_obj_t *marker;
        const char *title;
        int text_x = 12;
        int text_width = 120;

        if(index >= count)
            break;
        row_box = make_box(product_content, 12, y, 140, 25, 8,
                           selected ? highlight_primary() : COLOR_PANEL,
                           selected ? 220 : LV_OPA_TRANSP);
        menu_view.rows[row] = row_box;
        if(selected) {
            if(crazypod_appearance_get()->highlight_style != 0) {
                lv_obj_set_style_bg_grad_color(
                    row_box, lv_color_hex(highlight_secondary()), 0);
                lv_obj_set_style_bg_grad_dir(row_box, LV_GRAD_DIR_HOR, 0);
            }
            lv_obj_set_style_border_width(row_box, 1, 0);
            lv_obj_set_style_border_color(row_box,
                                           lv_color_hex(COLOR_WHITE), 0);
            lv_obj_set_style_border_opa(row_box, 90, 0);
        }

        if(state->route == MUSIC_ROUTE_MENU ||
           state->route == DIY_ROUTE_MENU) {
            const char *icon_text = state->route == MUSIC_ROUTE_MENU
                ? music_menu_symbols[index] : diy_menu_symbols[index];
            lv_obj_t *circle = make_box(row_box, 6, 2, 21, 21,
                                        LV_RADIUS_CIRCLE, COLOR_WHITE,
                                        selected ? 45 : 18);
            lv_obj_t *icon = make_label(circle, icon_text,
                                        &lv_font_montserrat_10,
                                        COLOR_WHITE,
                                        selected ? 255 : 170);
            menu_view.circles[row] = circle;
            menu_view.icons[row] = icon;
            lv_obj_center(icon);
            text_x = 34;
            text_width = 88;
        }
        title = route_item_title(state, index);
        if(title == NULL)
            title = "";
        label = make_label(row_box, title, CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, selected ? 255 : 195);
        lv_obj_set_pos(label, text_x, 5);
        lv_obj_set_width(label, text_width);
        lv_obj_set_height(label, 16);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        menu_view.labels[row] = label;

        marker = make_label(row_box,
                            state->route == DIY_ROUTE_ICONS &&
                            index == crazypod_appearance_get()->icon_theme
                                ? LV_SYMBOL_OK :
                            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET,
                            &lv_font_montserrat_8,
                            COLOR_WHITE, selected ? 205 : 90);
        lv_obj_set_pos(marker, 128, 8);
        menu_view.markers[row] = marker;
    }

    if(count > CRAZYPOD_VISIBLE_ROWS) {
        int track_height = 170;
        int thumb_height = track_height * CRAZYPOD_VISIBLE_ROWS / count;
        int thumb_y;
        lv_obj_t *bar;
        if(thumb_height < 12)
            thumb_height = 12;
        thumb_y = 54 + (track_height - thumb_height) * state->selected /
                  (count - 1);
        bar = make_box(product_content, 153, 54, 2, track_height, 1,
                       COLOR_WHITE, 25);
        (void)bar;
        bar = make_box(product_content, 153, thumb_y, 2, thumb_height, 1,
                       COLOR_WHITE, 155);
        menu_view.scroll_thumb = bar;
    }

    if(state->route == MUSIC_ROUTE_SEARCH)
        render_editor_preview(search_query, "Any track",
                              "Searches title, artist and album.");
    else if(state->route == MUSIC_ROUTE_MENU)
        render_root_preview(state->selected);
    else if(state->route >= DIY_ROUTE_MENU)
        render_diy_preview(state);
    else
        render_item_preview(state);
}

static void refresh_menu_rows(const struct route_state *state)
{
    int count = route_item_count(state);
    int start;
    int row;

    if(!menu_view.valid || menu_view.route != state->route || count <= 0)
        return;

    start = count <= CRAZYPOD_VISIBLE_ROWS ? 0 :
            state->selected - CRAZYPOD_VISIBLE_ROWS / 2;
    if(start < 0)
        start = 0;
    if(start > count - CRAZYPOD_VISIBLE_ROWS)
        start = count - CRAZYPOD_VISIBLE_ROWS;

    for(row = 0; row < CRAZYPOD_VISIBLE_ROWS; ++row) {
        int index = start + row;
        bool selected = index == state->selected;
        lv_obj_t *row_box = menu_view.rows[row];
        lv_obj_t *label = menu_view.labels[row];
        lv_obj_t *marker = menu_view.markers[row];
        const char *title;

        if(row_box == NULL)
            continue;
        if(index >= count) {
            lv_obj_add_flag(row_box, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(row_box, LV_OBJ_FLAG_HIDDEN);
        title = route_item_title(state, index);
        lv_label_set_text(label, title != NULL ? title : "");
        lv_obj_set_style_text_opa(label, selected ? 255 : 195, 0);
        lv_obj_set_style_bg_color(
            row_box,
            lv_color_hex(selected ? highlight_primary() : COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(
            row_box, selected ? 220 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_box, selected ? 1 : 0, 0);
        if(selected &&
           crazypod_appearance_get()->highlight_style != 0) {
            lv_obj_set_style_bg_grad_color(
                row_box, lv_color_hex(highlight_secondary()), 0);
            lv_obj_set_style_bg_grad_dir(
                row_box, LV_GRAD_DIR_HOR, 0);
        }

        if(menu_view.circles[row] != NULL) {
            const char *icon_text =
                state->route == MUSIC_ROUTE_MENU
                    ? music_menu_symbols[index]
                    : diy_menu_symbols[index];
            lv_obj_set_style_bg_opa(menu_view.circles[row],
                                    selected ? 45 : 18, 0);
            lv_label_set_text(menu_view.icons[row], icon_text);
            lv_obj_set_style_text_opa(menu_view.icons[row],
                                      selected ? 255 : 170, 0);
        }
        lv_label_set_text(
            marker,
            state->route == DIY_ROUTE_ICONS &&
            index == crazypod_appearance_get()->icon_theme
                ? LV_SYMBOL_OK :
            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET);
        lv_obj_set_style_text_opa(marker, selected ? 205 : 90, 0);
    }

    if(menu_view.scroll_thumb != NULL && count > 1) {
        int track_height = 170;
        int thumb_height = track_height * CRAZYPOD_VISIBLE_ROWS / count;
        int thumb_y;
        if(thumb_height < 12)
            thumb_height = 12;
        thumb_y = 54 + (track_height - thumb_height) *
                         state->selected / (count - 1);
        lv_obj_set_y(menu_view.scroll_thumb, thumb_y);
        lv_obj_set_height(menu_view.scroll_thumb, thumb_height);
    }
}

#if 0
static void album_flow_scale_x_anim(void *target, int32_t value)
{
    lv_obj_set_style_transform_scale_x(target, value, 0);
}

static void album_flow_scale_y_anim(void *target, int32_t value)
{
    lv_obj_set_style_transform_scale_y(target, value, 0);
}

static void album_flow_opa_anim(void *target, int32_t value)
{
    lv_obj_set_style_opa(target, (lv_opa_t)value, 0);
}

static void start_album_flow_anim(lv_obj_t *target,
                                  lv_anim_exec_xcb_t callback,
                                  int32_t from, int32_t to)
{
    lv_anim_t animation;

    lv_anim_delete(target, callback);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, target);
    lv_anim_set_exec_cb(&animation, callback);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_duration(&animation, 240);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static void album_flow_set_pose(struct album_flow_card *card, int relative,
                                bool animated)
{
    static const int center_x[] = { -80, -12, 55, 160, 265, 332, 400 };
    static const int center_y[] = { 116, 114, 112, 108, 112, 114, 116 };
    static const int scale_x[] = { 55, 85, 145, 256, 145, 85, 55 };
    static const int scale_y[] = { 120, 150, 190, 256, 190, 150, 120 };
    static const int opacity[] = { 0, 35, 130, 255, 130, 35, 0 };
    int pose = relative + 3;
    int target_x;
    int target_y;

    if(pose < 0)
        pose = 0;
    if(pose > 6)
        pose = 6;
    target_x = center_x[pose] - CRAZYPOD_ALBUM_FLOW_COVER_SIZE / 2;
    target_y = center_y[pose] - CRAZYPOD_ALBUM_FLOW_COVER_SIZE / 2;
    lv_obj_remove_flag(card->root, LV_OBJ_FLAG_HIDDEN);

    if(animated) {
        start_album_flow_anim(card->root, carousel_x_anim,
                              lv_obj_get_x(card->root), target_x);
        start_album_flow_anim(card->root, carousel_y_anim,
                              lv_obj_get_y(card->root), target_y);
        start_album_flow_anim(
            card->root, album_flow_scale_x_anim,
            lv_obj_get_style_transform_scale_x(card->root, 0),
            scale_x[pose]);
        start_album_flow_anim(
            card->root, album_flow_scale_y_anim,
            lv_obj_get_style_transform_scale_y(card->root, 0),
            scale_y[pose]);
        start_album_flow_anim(card->root, album_flow_opa_anim,
                              lv_obj_get_style_opa(card->root, 0),
                              opacity[pose]);
    }
    else {
        lv_obj_set_pos(card->root, target_x, target_y);
        lv_obj_set_style_transform_scale_x(card->root, scale_x[pose], 0);
        lv_obj_set_style_transform_scale_y(card->root, scale_y[pose], 0);
        lv_obj_set_style_opa(card->root, (lv_opa_t)opacity[pose], 0);
    }
}

static void album_flow_set_content(struct album_flow_card *card,
                                   int album_index)
{
    const struct crazypod_track *track =
        crazypod_music_album_track(album_index, 0);
    const lv_image_dsc_t *descriptor = track != NULL
        ? crazypod_artwork_load(card->artwork_slot, track,
                               CRAZYPOD_ALBUM_FLOW_COVER_SIZE)
        : NULL;

    card->album_index = album_index;
    lv_obj_set_style_bg_color(
        card->cover,
        lv_color_hex(artwork_color(track != NULL ? track->album : "", 0)),
        0);
    lv_obj_set_style_bg_grad_color(
        card->cover,
        lv_color_hex(artwork_color(track != NULL ? track->artist : "", 1)),
        0);

    if(descriptor != NULL) {
        int reflection_x =
            (CRAZYPOD_ALBUM_FLOW_COVER_SIZE - descriptor->header.w) / 2;
        lv_image_set_src(card->image, descriptor);
        lv_obj_center(card->image);
        lv_image_set_src(card->reflection_image, descriptor);
        lv_obj_set_pos(card->reflection_image, reflection_x, 0);
        lv_obj_remove_flag(card->image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(card->reflection_clip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card->symbol, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(card->image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card->reflection_clip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(card->symbol, LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_album_flow_card(struct album_flow_card *card, int slot)
{
    card->root = lv_obj_create(product_content);
    set_plain_object(card->root);
    lv_obj_set_size(card->root, CRAZYPOD_ALBUM_FLOW_COVER_SIZE, 148);
    lv_obj_set_style_bg_opa(card->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_pivot_x(
        card->root, CRAZYPOD_ALBUM_FLOW_COVER_SIZE / 2, 0);
    lv_obj_set_style_transform_pivot_y(
        card->root, CRAZYPOD_ALBUM_FLOW_COVER_SIZE / 2, 0);
    lv_obj_remove_flag(card->root, LV_OBJ_FLAG_CLICKABLE);

    card->cover = make_box(
        card->root, 0, 0, CRAZYPOD_ALBUM_FLOW_COVER_SIZE,
        CRAZYPOD_ALBUM_FLOW_COVER_SIZE, 7, 0x282832, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(card->cover, lv_color_hex(0x101018), 0);
    lv_obj_set_style_bg_grad_dir(card->cover, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_clip_corner(card->cover, true, 0);
    lv_obj_set_style_shadow_offset_y(card->cover, 5, 0);
    lv_obj_set_style_shadow_color(
        card->cover, lv_color_hex(highlight_primary()), 0);
    if(crazypod_appearance_get()->glow == 0) {
        lv_obj_set_style_shadow_opa(card->cover, LV_OPA_TRANSP, 0);
    }
    else {
        lv_obj_set_style_shadow_width(
            card->cover, 6 + crazypod_appearance_get()->glow * 5, 0);
        lv_obj_set_style_shadow_opa(
            card->cover, 30 + crazypod_appearance_get()->glow * 35, 0);
    }

    card->image = lv_image_create(card->cover);
    lv_obj_add_flag(card->image, LV_OBJ_FLAG_HIDDEN);
    card->symbol = make_label(card->cover, LV_SYMBOL_AUDIO,
                              &lv_font_montserrat_24,
                              COLOR_WHITE, 210);
    lv_obj_center(card->symbol);

    card->reflection_clip = lv_obj_create(card->root);
    set_plain_object(card->reflection_clip);
    lv_obj_set_pos(card->reflection_clip, 0, 122);
    lv_obj_set_size(card->reflection_clip,
                    CRAZYPOD_ALBUM_FLOW_COVER_SIZE, 22);
    lv_obj_set_style_bg_opa(card->reflection_clip, LV_OPA_TRANSP, 0);
    card->reflection_image = lv_image_create(card->reflection_clip);
    lv_image_set_rotation(card->reflection_image, 1800);
    lv_obj_set_style_image_opa(card->reflection_image, 42, 0);
    lv_obj_add_flag(card->reflection_clip, LV_OBJ_FLAG_HIDDEN);

    card->album_index = -1;
    card->artwork_slot = slot;
}

static void update_album_flow(bool animated, int direction)
{
    struct route_state *state = current_route();
    struct album_flow_card *visible[CRAZYPOD_ALBUM_FLOW_CARD_COUNT] = {
        NULL, NULL, NULL, NULL, NULL
    };
    bool claimed[CRAZYPOD_ALBUM_FLOW_CARD_COUNT] = {
        false, false, false, false, false
    };
    int count = crazypod_music_album_count();
    int relative;
    int i;

    for(relative = -2; relative <= 2; ++relative) {
        int desired = state->selected + relative;
        int visible_index = relative + 2;
        struct album_flow_card *card = NULL;
        bool content_changed = false;

        if(desired < 0 || desired >= count)
            continue;
        for(i = 0; i < CRAZYPOD_ALBUM_FLOW_CARD_COUNT; ++i) {
            if(!claimed[i] && album_flow_cards[i].album_index == desired) {
                card = &album_flow_cards[i];
                claimed[i] = true;
                break;
            }
        }
        if(card == NULL) {
            for(i = 0; i < CRAZYPOD_ALBUM_FLOW_CARD_COUNT; ++i) {
                if(!claimed[i] &&
                   (album_flow_cards[i].album_index <
                        state->selected - 2 ||
                    album_flow_cards[i].album_index >
                        state->selected + 2)) {
                    card = &album_flow_cards[i];
                    claimed[i] = true;
                    content_changed = card->album_index != desired;
                    break;
                }
            }
        }
        if(card == NULL) {
            for(i = 0; i < CRAZYPOD_ALBUM_FLOW_CARD_COUNT; ++i) {
                if(!claimed[i]) {
                    card = &album_flow_cards[i];
                    claimed[i] = true;
                    content_changed = card->album_index != desired;
                    break;
                }
            }
        }
        if(card == NULL)
            continue;

        if(content_changed) {
            int spawn_relative = direction >= 0 ? 3 : -3;
            album_flow_set_content(card, desired);
            album_flow_set_pose(card,
                                animated ? spawn_relative : relative,
                                false);
        }
        album_flow_set_pose(card, relative, animated);
        visible[visible_index] = card;
    }

    for(i = 0; i < CRAZYPOD_ALBUM_FLOW_CARD_COUNT; ++i) {
        if(!claimed[i])
            lv_obj_add_flag(album_flow_cards[i].root, LV_OBJ_FLAG_HIDDEN);
    }
    for(relative = 2; relative >= 0; --relative) {
        int left = 2 - relative;
        int right = 2 + relative;
        if(visible[left] != NULL)
            lv_obj_move_foreground(visible[left]->root);
        if(right != left && visible[right] != NULL)
            lv_obj_move_foreground(visible[right]->root);
    }
    lv_obj_move_foreground(album_flow_title);
    lv_obj_move_foreground(album_flow_artist);
    lv_obj_move_foreground(album_flow_position);

    {
        const struct crazypod_album *album =
            crazypod_music_album(state->selected);
        char position[32];
        lv_label_set_text(album_flow_title,
                          album != NULL ? album->title : "");
        lv_label_set_text(album_flow_artist,
                          album != NULL ? album->artist : "");
        snprintf(position, sizeof(position), "%d / %d",
                 state->selected + 1, count);
        lv_label_set_text(album_flow_position, position);
    }
}
#endif

static void render_album_flow(const struct route_state *state)
{
    int count = crazypod_music_album_count();
    const struct crazypod_album *album;
    char position[32];

    lv_obj_set_style_bg_color(product_content, lv_color_hex(0x050509), 0);
    lv_obj_set_style_bg_opa(product_content, LV_OPA_COVER, 0);
    if(count <= 0) {
        render_empty_state("No Albums", "Add local music and rescan.");
        return;
    }

    album_flow_title = make_label(product_content, "",
                                  CRAZYPOD_METADATA_FONT,
                                  COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(album_flow_title, 260);
    lv_obj_set_height(album_flow_title, 18);
    lv_obj_set_style_text_align(album_flow_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(album_flow_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(album_flow_title, 30, 195);

    album_flow_artist = make_label(product_content, "",
                                   CRAZYPOD_METADATA_FONT,
                                   COLOR_WHITE, 145);
    lv_obj_set_width(album_flow_artist, 260);
    lv_obj_set_height(album_flow_artist, 16);
    lv_obj_set_style_text_align(album_flow_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(album_flow_artist, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(album_flow_artist, 30, 213);

    album_flow_position = make_label(product_content, "",
                                     &lv_font_montserrat_8,
                                     COLOR_WHITE, 70);
    lv_obj_set_width(album_flow_position, 60);
    lv_obj_set_style_text_align(album_flow_position,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(album_flow_position, 130, 228);

    album = crazypod_music_album(state->selected);
    lv_label_set_text(album_flow_title,
                      album != NULL ? album->title : "");
    lv_label_set_text(album_flow_artist,
                      album != NULL ? album->artist : "");
    snprintf(position, sizeof(position), "%d / %d",
             state->selected + 1, count);
    lv_label_set_text(album_flow_position, position);
    crazypod_coverflow_enter(state->selected);
}

static void render_now_playing(void)
{
    const struct crazypod_track *track = current_track();
    lv_obj_t *backdrop;
    lv_obj_t *shade;
    lv_obj_t *title;
    lv_obj_t *artist;
    lv_obj_t *mode;
    lv_obj_t *heart;
    int i;

    rendered_track_path[0] = '\0';
    if(track != NULL)
        snprintf(rendered_track_path, sizeof(rendered_track_path),
                 "%s", track->path);

    backdrop = make_box(product_content, 0, 0, 320, 240, 0,
                        artwork_color(track != NULL ? track->album : "", 0),
                        LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        backdrop,
        lv_color_hex(artwork_color(track != NULL ? track->artist : "", 1)),
        0);
    lv_obj_set_style_bg_grad_dir(backdrop, LV_GRAD_DIR_VER, 0);
    shade = make_box(product_content, 0, 0, 320, 240, 0, 0x050508,
                     crazypod_appearance_get()->player_style == 0
                         ? 142 : 188);
    (void)shade;

    create_artwork(product_content, track, 80, 14, 160,
                   CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);

    title = make_label(product_content,
                       track != NULL ? track->title : "No Track",
                       CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(title, 240);
    lv_obj_set_height(title, 18);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(title, 40, 123);
    artist = make_label(product_content,
                        track != NULL ? track->artist : "Local Music",
                        CRAZYPOD_METADATA_FONT,
                        COLOR_WHITE, 195);
    lv_obj_set_width(artist, 220);
    lv_obj_set_height(artist, 17);
    lv_obj_set_style_text_align(artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(artist, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(artist, 50, 146);

    heart = make_label(product_content, LV_SYMBOL_PLUS,
                       &lv_font_montserrat_12,
                       COLOR_WHITE, 205);
    lv_obj_set_pos(heart, 132, 163);
    mode = make_label(product_content,
                      crazypod_queue_repeat() == REPEAT_ONE ? "1" :
                      crazypod_queue_repeat() == REPEAT_ALL ? LV_SYMBOL_LOOP :
                      crazypod_queue_shuffle() ? LV_SYMBOL_SHUFFLE :
                      LV_SYMBOL_PLAY,
                      &lv_font_montserrat_12,
                      crazypod_queue_repeat() != REPEAT_OFF ||
                      crazypod_queue_shuffle() ? COLOR_CYAN : COLOR_WHITE,
                      220);
    lv_obj_set_pos(mode, 174, 163);

    for(i = 0; i < 18; ++i) {
        int height = 5 + ((i * 7 + 11) % 19);
        lv_obj_t *wave = make_box(product_content, 20 + i * 16,
                                  198 - height / 2, 3, height, 2,
                                  i % 3 == 0 ? COLOR_CYAN : COLOR_WHITE,
                                  i % 3 == 0 ? 190 : 80);
        (void)wave;
    }
    now_progress_fill = make_box(product_content, 18, 196, 4, 4,
                                 LV_RADIUS_CIRCLE, COLOR_WHITE,
                                 LV_OPA_COVER);
    now_elapsed = make_label(product_content, "0:00",
                             &lv_font_montserrat_8,
                             COLOR_WHITE, 180);
    lv_obj_set_pos(now_elapsed, 30, 218);
    now_remaining = make_label(product_content, "-0:00",
                               &lv_font_montserrat_8,
                               COLOR_WHITE, 180);
    lv_obj_set_width(now_remaining, 60);
    lv_obj_set_style_text_align(now_remaining, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(now_remaining, 230, 218);
}

static void animate_content_entrance(void)
{
    lv_obj_set_x(product_content, 0);
    lv_obj_set_style_opa(product_content, LV_OPA_COVER, 0);
    lv_obj_invalidate(product_content);
}

static void render_current_route(bool transition)
{
    struct route_state *state = current_route();
    int i;

    if(crazypod_coverflow_active())
        crazypod_coverflow_leave();
    memset(&menu_view, 0, sizeof(menu_view));
    route_render_pending = false;
    now_progress_fill = NULL;
    now_elapsed = NULL;
    now_remaining = NULL;
    music_loading_title = NULL;
    music_loading_detail = NULL;
    album_flow_title = NULL;
    album_flow_artist = NULL;
    album_flow_position = NULL;
    for(i = 0; i < CRAZYPOD_ALBUM_FLOW_CARD_COUNT; ++i) {
        album_flow_cards[i].root = NULL;
        album_flow_cards[i].cover = NULL;
        album_flow_cards[i].image = NULL;
        album_flow_cards[i].symbol = NULL;
        album_flow_cards[i].reflection_clip = NULL;
        album_flow_cards[i].reflection_image = NULL;
        album_flow_cards[i].album_index = -1;
        album_flow_cards[i].artwork_slot = i;
    }
    lv_obj_clean(product_content);
    lv_obj_set_pos(product_content, 0, 0);
    lv_obj_set_style_bg_color(
        product_content,
        lv_color_hex(crazypod_appearance_menu_color()), 0);
    lv_obj_set_style_bg_opa(product_content, LV_OPA_COVER, 0);
    make_box(product_content, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
             crazypod_appearance_menu_color(), LV_OPA_COVER);

    if(state->route == MUSIC_ROUTE_NOW_PLAYING)
        render_now_playing();
    else if(state->route == MUSIC_ROUTE_ALBUM_FLOW)
        render_album_flow(state);
    else
        render_menu_screen(state);

    lv_obj_invalidate(product_content);
    if(transition)
        animate_content_entrance();
    lv_obj_move_foreground(status_bars[1].time);
    lv_obj_move_foreground(status_bars[1].playing);
}

static void render_loading(void)
{
    lv_obj_t *symbol;
    char detail[64];

    lv_obj_clean(product_content);
    lv_obj_set_style_bg_color(product_content, lv_color_hex(COLOR_DETAIL), 0);
    lv_obj_set_style_bg_opa(product_content, LV_OPA_COVER, 0);
    symbol = make_label(product_content, LV_SYMBOL_REFRESH,
                        &lv_font_montserrat_24,
                        COLOR_CYAN, LV_OPA_COVER);
    lv_obj_set_pos(symbol, 148, 91);
    music_loading_title = make_label(
        product_content,
        music_scan_start_failed
            ? "Library Scan Failed"
            : music_artwork_preparing
                ? "Preparing Album Artwork"
                : "Building Music Library",
        &lv_font_montserrat_12,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(music_loading_title, 260);
    lv_obj_set_style_text_align(music_loading_title,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(music_loading_title, 30, 132);
    if(music_artwork_preparing) {
        snprintf(detail, sizeof(detail), "%d / %d albums",
                 crazypod_artwork_library_prime_completed(),
                 crazypod_artwork_library_prime_total());
    }
    else {
        snprintf(detail, sizeof(detail), "%s",
                 music_scan_start_failed
                     ? "No background thread was available"
                     : "Reading local files and metadata");
    }
    music_loading_detail = make_label(
        product_content, detail, &lv_font_montserrat_8,
        COLOR_WHITE, 110);
    lv_obj_set_width(music_loading_detail, 260);
    lv_obj_set_style_text_align(music_loading_detail,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(music_loading_detail, 30, 155);
}

static void begin_music_scan(void)
{
    if(crazypod_music_is_scanning() ||
       music_artwork_preparing ||
       crazypod_music_scan_generation() !=
           music_scan_generation_seen) {
        music_library_loaded = false;
        music_scan_screen = true;
        music_scan_start_failed = false;
        render_loading();
        lv_refr_now(NULL);
        return;
    }
    if(!crazypod_music_is_scanning() &&
       crazypod_music_track_count() > 0) {
        music_library_loaded = true;
        music_scan_screen = false;
        render_current_route(true);
        return;
    }

    music_library_loaded = false;
    music_scan_screen = true;
    music_scan_start_failed = false;
    music_scan_generation_seen = crazypod_music_scan_generation();
    render_loading();
    lv_refr_now(NULL);

    music_scan_pending = true;
    music_scan_not_before = current_tick;
}

static void service_music_scan(void)
{
    if(!music_scan_pending || usb_storage_active ||
       crazypod_music_is_scanning() ||
       TIME_BEFORE(current_tick, music_scan_not_before))
        return;

    music_scan_pending = false;
    music_scan_generation_seen = crazypod_music_scan_generation();
    music_artwork_preparing = false;
    crazypod_artwork_cancel_library_prime();
    if(!crazypod_music_scan_async()) {
        music_scan_start_failed = true;
        if(music_scan_screen)
            render_loading();
    }
}

static void open_music(void)
{
    product_active = true;
    lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(product_screen);
    set_cpu_boost(true);

    route_depth = 1;
    route_stack[0].route = MUSIC_ROUTE_MENU;
    route_stack[0].selected = 0;
    route_stack[0].group = -1;
    if(!crazypod_music_is_scanning() &&
       !music_artwork_preparing &&
       crazypod_music_scan_generation() ==
           music_scan_generation_seen &&
       crazypod_music_track_count() > 0)
        music_library_loaded = true;
    if(!music_library_loaded) {
        begin_music_scan();
        return;
    }
    render_current_route(true);
}

static void open_diy(void)
{
    product_active = true;
    lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(product_screen);
    set_cpu_boost(true);

    route_depth = 1;
    route_stack[0].route = DIY_ROUTE_MENU;
    route_stack[0].selected = 0;
    route_stack[0].group = -1;
    render_current_route(true);
}

static void render_placeholder(const struct crazypod_app *app)
{
    lv_obj_t *tile;
    lv_obj_t *symbol;
    lv_obj_t *title;
    lv_obj_t *detail;

    lv_obj_clean(product_content);
    lv_obj_set_style_bg_color(product_content, lv_color_hex(COLOR_DETAIL), 0);
    lv_obj_set_style_bg_opa(product_content, LV_OPA_COVER, 0);
    tile = make_box(product_content, 132, 82, 56, 56, 16,
                    app->color, LV_OPA_COVER);
    symbol = make_label(tile, app->symbol, &lv_font_montserrat_24,
                        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_center(symbol);
    title = make_label(product_content, app->name,
                       &lv_font_montserrat_16,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 40, 151);
    detail = make_label(product_content, "Coming soon",
                        &lv_font_montserrat_10,
                        COLOR_WHITE, 128);
    lv_obj_set_width(detail, 160);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(detail, 80, 176);
}

static void open_placeholder(const struct crazypod_app *app)
{
    product_active = true;
    route_depth = 0;
    lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(product_screen);
    render_placeholder(app);
    animate_content_entrance();
}

static void close_product(void)
{
    if(!product_active)
        return;
    product_active = false;
    route_depth = 0;
    lv_obj_add_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(product_screen);
    lv_obj_invalidate(desktop_screen);
    desktop_native_backdrop_ready = false;
    desktop_native_dirty = true;
    layout_desktop_carousel(false);
    lv_refr_now(NULL);
}

static void push_route_selected(enum crazypod_route route, int group,
                                int selected)
{
    if(route_depth >= CRAZYPOD_ROUTE_DEPTH)
        return;
    route_stack[route_depth].route = route;
    route_stack[route_depth].selected = selected;
    route_stack[route_depth].group = group;
    ++route_depth;
    render_current_route(true);
}

static void push_route(enum crazypod_route route, int group)
{
    push_route_selected(route, group, 0);
}

static void pop_route(void)
{
    if(route_depth > 1) {
        --route_depth;
        render_current_route(true);
    }
    else {
        close_product();
    }
}

static void play_selected_track(struct route_state *state)
{
    bool started = false;
    switch(state->route) {
    case MUSIC_ROUTE_ALL:
    case MUSIC_ROUTE_SONGS:
        started = crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0,
                                      state->selected);
        break;
    case MUSIC_ROUTE_PLAYLIST_SONGS:
        started = crazypod_music_play(CRAZYPOD_SCOPE_PLAYLIST,
                                      state->group, state->selected);
        break;
    case MUSIC_ROUTE_ARTIST_SONGS:
        started = crazypod_music_play(CRAZYPOD_SCOPE_ARTIST,
                                      state->group, state->selected);
        break;
    case MUSIC_ROUTE_ALBUM_SONGS:
        started = crazypod_music_play(CRAZYPOD_SCOPE_ALBUM,
                                      state->group, state->selected);
        break;
    case MUSIC_ROUTE_QUEUE:
        if(state->selected >= 0 &&
           state->selected < crazypod_queue_count()) {
            playlist_start(state->selected, 0, 0);
            started = true;
        }
        break;
    case MUSIC_ROUTE_SEARCH_RESULTS:
        started = crazypod_music_play_search(search_query, state->selected);
        break;
    default:
        break;
    }
    if(started)
    {
        crazypod_state_forget_resume();
        crazypod_state_mark_dirty();
        push_route(MUSIC_ROUTE_NOW_PLAYING, -1);
    }
}

static void editor_append(char *buffer, size_t size, const char *text)
{
    size_t used = strlen(buffer);
    size_t available;
    size_t copied = 0;

    if(used >= size - 1 || text == NULL)
        return;
    available = size - used - 1;
    while(copied < available && text[copied] != '\0') {
        buffer[used + copied] = text[copied];
        ++copied;
    }
    buffer[used + copied] = '\0';
}

static void editor_backspace(char *buffer)
{
    size_t length = strlen(buffer);

    if(length > 0)
        buffer[length - 1] = '\0';
}

static void activate_selected(void)
{
    struct route_state *state = current_route();
    switch(state->route) {
    case MUSIC_ROUTE_MENU:
        switch(state->selected) {
        case 0: push_route(MUSIC_ROUTE_NOW_PLAYING, -1); break;
        case 1:
            push_route_selected(MUSIC_ROUTE_ALBUM_FLOW, -1,
                                initial_album_index());
            break;
        case 2: push_route(MUSIC_ROUTE_ALL, -1); break;
        case 3: push_route(MUSIC_ROUTE_PLAYLISTS, -1); break;
        case 4: push_route(MUSIC_ROUTE_ARTISTS, -1); break;
        case 5: push_route(MUSIC_ROUTE_ALBUMS, -1); break;
        case 6: push_route(MUSIC_ROUTE_SONGS, -1); break;
        case 7:
            search_query[0] = '\0';
            push_route(MUSIC_ROUTE_SEARCH, -1);
            break;
        }
        break;
    case MUSIC_ROUTE_SEARCH:
        if(state->selected < CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_append(search_query, sizeof(search_query),
                          editor_characters[state->selected]);
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_append(search_query, sizeof(search_query), " ");
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            editor_backspace(search_query);
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT + 2)
            search_query[0] = '\0';
        else
            push_route(MUSIC_ROUTE_SEARCH_RESULTS, -1);
        if(current_route()->route == MUSIC_ROUTE_SEARCH)
            render_current_route(false);
        break;
    case MUSIC_ROUTE_PLAYLISTS:
        push_route(MUSIC_ROUTE_PLAYLIST_SONGS, state->selected);
        break;
    case MUSIC_ROUTE_ARTISTS:
        push_route(MUSIC_ROUTE_ARTIST_SONGS, state->selected);
        break;
    case MUSIC_ROUTE_ALBUMS:
    case MUSIC_ROUTE_ALBUM_FLOW:
        push_route(MUSIC_ROUTE_ALBUM_SONGS, state->selected);
        break;
    case MUSIC_ROUTE_NOW_PLAYING:
        push_route(MUSIC_ROUTE_QUEUE, -1);
        if(crazypod_queue_count() > 0)
            current_route()->selected = crazypod_queue_index();
        render_current_route(false);
        break;
    case DIY_ROUTE_MENU:
        if(state->selected == 0)
            push_route(DIY_ROUTE_PRESETS, -1);
        else if(state->selected == 1)
            push_route(DIY_ROUTE_ICONS, -1);
        else if(state->selected == 2)
            push_route(DIY_ROUTE_DETAILS, -1);
        else
            push_route(DIY_ROUTE_BACKGROUNDS, -1);
        break;
    case DIY_ROUTE_PRESETS:
        if(state->selected == 0) {
            int index = crazypod_preset_save_current();
            if(index >= 0)
                push_route(DIY_ROUTE_PRESET_ACTIONS, index);
            else
                render_current_route(false);
        }
        else if(state->selected == 1)
            push_route(DIY_ROUTE_PRESET_LIBRARY, -1);
        else {
            int index = crazypod_preset_import();
            if(index >= 0)
                push_route(DIY_ROUTE_PRESET_ACTIONS, index);
            else
                render_current_route(false);
        }
        break;
    case DIY_ROUTE_PRESET_LIBRARY:
        push_route(DIY_ROUTE_PRESET_ACTIONS, state->selected);
        break;
    case DIY_ROUTE_PRESET_ACTIONS: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);
        if(preset == NULL) {
            pop_route();
            break;
        }
        if(state->selected == 0) {
            crazypod_preset_apply(state->group);
            refresh_desktop_appearance();
            render_current_route(false);
        }
        else if(state->selected == 1) {
            crazypod_preset_export(state->group);
            render_current_route(false);
        }
        else if(!preset->builtin)
            push_route(DIY_ROUTE_PRESET_EDIT, state->group);
        break;
    }
    case DIY_ROUTE_PRESET_EDIT:
        if(state->selected == 0) {
            preset_name_editor[0] = '\0';
            push_route(DIY_ROUTE_PRESET_RENAME, state->group);
        }
        else if(state->selected == 1) {
            crazypod_preset_update(state->group);
            render_current_route(false);
        }
        else if(crazypod_preset_delete(state->group)) {
            route_depth -= 2;
            if(route_depth < 1)
                route_depth = 1;
            if(current_route()->route == DIY_ROUTE_PRESET_LIBRARY &&
               current_route()->selected >= crazypod_preset_count())
                current_route()->selected =
                    crazypod_preset_count() > 0
                        ? crazypod_preset_count() - 1 : 0;
            render_current_route(false);
        }
        break;
    case DIY_ROUTE_PRESET_RENAME:
        if(state->selected < CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_append(preset_name_editor, sizeof(preset_name_editor),
                          editor_characters[state->selected]);
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_append(preset_name_editor, sizeof(preset_name_editor), " ");
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            editor_backspace(preset_name_editor);
        else if(crazypod_preset_rename(state->group,
                                       preset_name_editor)) {
            pop_route();
        }
        if(current_route()->route == DIY_ROUTE_PRESET_RENAME)
            render_current_route(false);
        break;
    case DIY_ROUTE_ICONS:
        crazypod_appearance_set_icon_theme(state->selected);
        refresh_desktop_appearance();
        render_current_route(false);
        break;
    case DIY_ROUTE_DETAILS: {
        static const enum crazypod_appearance_field fields[] = {
            CRAZYPOD_APPEARANCE_PLAYER_STYLE,
            CRAZYPOD_APPEARANCE_ICON_SCALE,
            CRAZYPOD_APPEARANCE_GLOW,
            CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE,
            CRAZYPOD_APPEARANCE_PRIMARY,
            CRAZYPOD_APPEARANCE_SECONDARY,
        };
        crazypod_appearance_cycle(fields[state->selected]);
        refresh_desktop_appearance();
        render_current_route(false);
        break;
    }
    case DIY_ROUTE_BACKGROUNDS:
        crazypod_appearance_cycle(
            state->selected == 0
                ? CRAZYPOD_APPEARANCE_HOME_BACKGROUND
                : CRAZYPOD_APPEARANCE_MENU_BACKGROUND);
        refresh_desktop_appearance();
        render_current_route(false);
        break;
    default:
        play_selected_track(state);
        break;
    }
}

static void move_selection(int direction)
{
    struct route_state *state = current_route();
    int count = route_item_count(state);

    if(state->route == MUSIC_ROUTE_NOW_PLAYING) {
        int next_volume = global_status.volume + direction * 2;
        if(next_volume < sound_min(SOUND_VOLUME))
            next_volume = sound_min(SOUND_VOLUME);
        if(next_volume > sound_max(SOUND_VOLUME))
            next_volume = sound_max(SOUND_VOLUME);
        sound_set_volume(next_volume);
        global_status.volume = next_volume;
        crazypod_state_mark_dirty();
        return;
    }
    if(count <= 0)
        return;
    keep_cpu_boosted(HZ / 3);
    if(state->route == MUSIC_ROUTE_ALBUM_FLOW) {
        const struct crazypod_album *album;
        char position[32];
        int next = crazypod_coverflow_step(direction);
        if(next == state->selected)
            return;
        state->selected = next;
        album = crazypod_music_album(next);
        lv_label_set_text(album_flow_title,
                          album != NULL ? album->title : "");
        lv_label_set_text(album_flow_artist,
                          album != NULL ? album->artist : "");
        snprintf(position, sizeof(position), "%d / %d", next + 1, count);
        lv_label_set_text(album_flow_position, position);
        return;
    }
    state->selected = (state->selected + direction) % count;
    if(state->selected < 0)
        state->selected += count;
    if(menu_view.valid && menu_view.route == state->route) {
        refresh_menu_rows(state);
        route_render_due = current_tick + CRAZYPOD_GESTURE_SETTLE_TICKS;
    }
    else {
        route_render_due = current_tick;
    }
    route_render_pending = true;
}

static void toggle_playback(void)
{
    int status = audio_status();
    if(crazypod_queue_count() <= 0) {
        if(crazypod_music_track_count() > 0)
            crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0);
        crazypod_state_forget_resume();
        crazypod_state_mark_dirty();
        return;
    }
    if(status & AUDIO_STATUS_PAUSE)
        audio_resume();
    else if(status & AUDIO_STATUS_PLAY)
        audio_pause();
    else
        playlist_start(crazypod_queue_index(),
                       crazypod_state_take_resume_elapsed(), 0);
}

static void cycle_playback_mode(void)
{
    if(!crazypod_queue_shuffle() &&
       crazypod_queue_repeat() == REPEAT_OFF) {
        crazypod_queue_set_shuffle(true);
    }
    else if(crazypod_queue_shuffle()) {
        crazypod_queue_set_shuffle(false);
        crazypod_queue_set_repeat(REPEAT_ALL);
    }
    else if(crazypod_queue_repeat() == REPEAT_ALL) {
        crazypod_queue_set_repeat(REPEAT_ONE);
    }
    else {
        crazypod_queue_set_repeat(REPEAT_OFF);
    }
    crazypod_state_mark_dirty();
    if(route_depth > 0 &&
       current_route()->route == MUSIC_ROUTE_NOW_PLAYING)
        render_current_route(false);
}

static void update_playback_ui(lv_timer_t *timer)
{
    const struct crazypod_track *track;
    struct mp3entry *id3 = NULL;

    (void)timer;
    if(!crazypod_music_is_scanning() &&
       crazypod_music_scan_generation() != music_scan_generation_seen) {
        music_scan_generation_seen = crazypod_music_scan_generation();
        music_library_loaded = false;
        if(crazypod_music_track_count() > 0) {
            music_artwork_preparing = true;
            crazypod_artwork_prime_library();
            crazypod_coverflow_warm(initial_album_index());
            if(music_scan_screen)
                render_loading();
        }
        else {
            music_artwork_preparing = false;
            if(music_scan_screen) {
                music_scan_screen = false;
                if(product_active && route_depth > 0)
                    render_current_route(true);
            }
        }
    }
    if(music_artwork_preparing) {
        if(music_loading_detail != NULL) {
            char progress[48];
            snprintf(progress, sizeof(progress), "%d / %d albums",
                     crazypod_artwork_library_prime_completed(),
                     crazypod_artwork_library_prime_total());
            lv_label_set_text(music_loading_detail, progress);
        }
        if(!crazypod_artwork_library_priming()) {
            music_artwork_preparing = false;
            music_library_loaded =
                crazypod_music_track_count() > 0;
            crazypod_coverflow_warm(initial_album_index());
            if(music_scan_screen) {
                music_scan_screen = false;
                if(product_active && route_depth > 0)
                    render_current_route(true);
            }
        }
    }
    if(music_scan_screen) {
        return;
    }

    track = current_track();
    if(track != NULL) {
        lv_label_set_text(desktop_capsule_track, track->title);
        lv_label_set_text(desktop_capsule_artist, track->artist);
    }
    else {
        lv_label_set_text(desktop_capsule_track, "No Track");
        lv_label_set_text(desktop_capsule_artist, "Local Music");
    }
    update_desktop_capsule_artwork(track);

    id3 = audio_current_track();
    if(id3 != NULL && id3->length > 0 &&
       desktop_capsule_progress != NULL) {
        int capsule_width = 171 * id3->elapsed / id3->length;
        if(capsule_width < 6)
            capsule_width = 6;
        if(capsule_width > 171)
            capsule_width = 171;
        lv_obj_set_width(desktop_capsule_progress, capsule_width);
    }
    else if(desktop_capsule_progress != NULL) {
        lv_obj_set_width(desktop_capsule_progress, 6);
    }

    if(route_depth <= 0 ||
       current_route()->route != MUSIC_ROUTE_NOW_PLAYING)
        return;

    if(track != NULL && strcmp(rendered_track_path, track->path) != 0) {
        render_current_route(false);
        return;
    }
    if(track == NULL && rendered_track_path[0] != '\0') {
        render_current_route(false);
        return;
    }

    if(id3 != NULL && id3->length > 0 && now_progress_fill != NULL) {
        int width = 284 * id3->elapsed / id3->length;
        char elapsed[16];
        char remaining[16];
        unsigned long left = id3->length > id3->elapsed
            ? id3->length - id3->elapsed : 0;
        if(width < 4)
            width = 4;
        if(width > 284)
            width = 284;
        lv_obj_set_x(now_progress_fill, 18 + width);
        format_time_ms(id3->elapsed, elapsed, sizeof(elapsed));
        format_time_ms(left, remaining + 1, sizeof(remaining) - 1);
        remaining[0] = '-';
        lv_label_set_text(now_elapsed, elapsed);
        lv_label_set_text(now_remaining, remaining);
    }
}

static void process_artwork_updates(void)
{
    unsigned generation = crazypod_artwork_slot_generation(
        CRAZYPOD_CAPSULE_ARTWORK_SLOT);

    if(generation != capsule_artwork_generation_seen) {
        capsule_artwork_generation_seen = generation;
        update_desktop_capsule_artwork(current_track());
    }
    if(!product_active || route_depth <= 0 || route_render_pending ||
       crazypod_coverflow_active() ||
       current_route()->route >= DIY_ROUTE_MENU)
        return;

    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING) {
        generation = crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);
        if(generation == now_artwork_generation_seen)
            return;
        now_artwork_generation_seen = generation;
    }
    else {
        generation = crazypod_artwork_slot_generation(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        if(generation == preview_artwork_generation_seen)
            return;
        preview_artwork_generation_seen = generation;
    }
    render_current_route(false);
}

static void update_persistent_state(lv_timer_t *timer)
{
    (void)timer;
    crazypod_state_tick();
}

static int wheel_step(intptr_t data, int maximum)
{
    int step = 1;

#ifdef HAVE_WHEEL_ACCELERATION
    step = button_apply_acceleration((unsigned int)data);
#else
    (void)data;
#endif
    if(step < 1)
        step = 1;
    if(step > maximum)
        step = maximum;
    return step;
}

static void handle_button(long button, intptr_t data)
{
    long base;
    bool repeated;

    if(button == BUTTON_NONE)
        return;
    if(button & SYS_EVENT) {
        if(button == SYS_USB_CONNECTED) {
            usb_storage_active = true;
            music_scan_pending = true;
            music_scan_not_before = current_tick + HZ / 2;
            music_library_loaded = false;
            crazypod_artwork_suspend();
            crazypod_music_cancel_scan();
            crazypod_state_save(true);
            usb_acknowledge(SYS_USB_CONNECTED_ACK, data);
        }
        else if(button == SYS_USB_DISCONNECTED) {
            usb_storage_active = false;
            crazypod_artwork_resume();
            music_scan_pending = true;
            music_scan_not_before = current_tick + HZ / 2;
            music_library_loaded = false;
        }
        else if(button == SYS_POWEROFF) {
            crazypod_state_save(true);
            shutdown_hw(SHUTDOWN_POWER_OFF);
        }
        else if(button == SYS_REBOOT) {
            crazypod_state_save(true);
            shutdown_hw(SHUTDOWN_REBOOT);
        }
        return;
    }

    if(button & BUTTON_REL)
        return;
    repeated = (button & BUTTON_REPEAT) != 0;
    base = button & ~BUTTON_REPEAT;
    backlight_on();

    if(product_active && route_depth > 0 && music_scan_screen) {
        if(base == BUTTON_MENU && !repeated)
            close_product();
        return;
    }

    if(!product_active) {
        if(base == BUTTON_SCROLL_FWD) {
            int count = wheel_step(data, 4);
            move_desktop_selection(count);
        }
        else if(base == BUTTON_SCROLL_BACK) {
            int count = wheel_step(data, 4);
            move_desktop_selection(-count);
        }
        else if(base == BUTTON_RIGHT)
            move_desktop_selection(1);
        else if(base == BUTTON_LEFT)
            move_desktop_selection(-1);
        else if(base == BUTTON_SELECT) {
            if(selected_app == 0)
                open_music();
            else if(selected_app == 3) {
                open_music();
                if(crazypod_music_track_count() > 0) {
                    crazypod_queue_set_shuffle(true);
                    crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0);
                    crazypod_state_forget_resume();
                    crazypod_state_mark_dirty();
                    push_route(MUSIC_ROUTE_NOW_PLAYING, -1);
                }
            }
            else if(selected_app == 7)
                open_diy();
            else
                open_placeholder(&apps[selected_app]);
        }
        else if(base == BUTTON_PLAY) {
            if(crazypod_queue_count() > 0) {
                product_active = true;
                lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(product_screen);
                route_depth = 1;
                route_stack[0].route = MUSIC_ROUTE_NOW_PLAYING;
                route_stack[0].selected = 0;
                route_stack[0].group = -1;
                render_current_route(true);
            }
            else {
                toggle_playback();
            }
        }
        return;
    }

    if(route_depth <= 0) {
        if(base == BUTTON_MENU)
            close_product();
        return;
    }

    if(base == BUTTON_SCROLL_FWD)
        move_selection(wheel_step(
            data,
            current_route()->route == MUSIC_ROUTE_ALBUM_FLOW ? 4 : 12));
    else if(base == BUTTON_SCROLL_BACK)
        move_selection(-wheel_step(
            data,
            current_route()->route == MUSIC_ROUTE_ALBUM_FLOW ? 4 : 12));
    else if(base == BUTTON_RIGHT) {
        if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING)
            audio_next();
        else
            move_selection(1);
    }
    else if(base == BUTTON_LEFT) {
        if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING)
            audio_prev();
        else
            move_selection(-1);
    }
    else if(base == BUTTON_SELECT) {
        if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING && repeated)
            cycle_playback_mode();
        else if(!repeated)
            activate_selected();
    }
    else if(base == BUTTON_MENU) {
        if(repeated && current_route()->route == MUSIC_ROUTE_MENU) {
            begin_music_scan();
        }
        else if(!repeated)
            pop_route();
    }
    else if(base == BUTTON_PLAY)
        toggle_playback();
}

static uint32_t rockbox_tick_ms(void)
{
    return (uint32_t)((current_tick * 1000L) / HZ);
}

static void display_flush(lv_display_t *display, const lv_area_t *area,
                          uint8_t *pixels)
{
    static bool dirty_valid;
    static int dirty_x1;
    static int dirty_y1;
    static int dirty_x2;
    static int dirty_y2;
    fb_data *destination;
    const fb_data *source = (const fb_data *)pixels;
    const lv_draw_buf_t *active_buffer = lv_display_get_buf_active(display);
    int source_stride = active_buffer->header.stride / sizeof(fb_data);
    int x = area->x1;
    int y = area->y1;
    int width = area->x2 - area->x1 + 1;
    int height = area->y2 - area->y1 + 1;
    int row;

    destination = (fb_data *)lcd_framebuffer_default.data +
                  y * LCD_WIDTH + x;
    for(row = 0; row < height; ++row) {
        memcpy(destination, source, (size_t)width * sizeof(fb_data));
        destination += LCD_WIDTH;
        source += source_stride;
    }
    if(!product_active &&
       area->y1 < CRAZYPOD_DESKTOP_NATIVE_BOTTOM &&
       area->y2 >= CRAZYPOD_DESKTOP_NATIVE_TOP) {
        if(desktop_native_backdrop_ready) {
            fb_data *framebuffer =
                (fb_data *)lcd_framebuffer_default.data;
            int left = area->x1 < 0 ? 0 : area->x1;
            int right = area->x2 >= LCD_WIDTH
                ? LCD_WIDTH - 1 : area->x2;
            int top = area->y1 < CRAZYPOD_DESKTOP_NATIVE_TOP
                ? CRAZYPOD_DESKTOP_NATIVE_TOP : area->y1;
            int bottom = area->y2 >= CRAZYPOD_DESKTOP_NATIVE_BOTTOM
                ? CRAZYPOD_DESKTOP_NATIVE_BOTTOM - 1 : area->y2;
            int copy_width = right - left + 1;
            int copy_y;

            for(copy_y = top; copy_y <= bottom; ++copy_y) {
                memcpy(
                    desktop_native_backdrop +
                        (copy_y - CRAZYPOD_DESKTOP_NATIVE_TOP) *
                            LCD_WIDTH + left,
                    framebuffer + copy_y * LCD_WIDTH + left,
                    (size_t)copy_width * sizeof(fb_data));
            }
        }
        desktop_native_dirty = true;
    }

    if(!dirty_valid) {
        dirty_x1 = area->x1;
        dirty_y1 = area->y1;
        dirty_x2 = area->x2;
        dirty_y2 = area->y2;
        dirty_valid = true;
    }
    else {
        if(area->x1 < dirty_x1)
            dirty_x1 = area->x1;
        if(area->y1 < dirty_y1)
            dirty_y1 = area->y1;
        if(area->x2 > dirty_x2)
            dirty_x2 = area->x2;
        if(area->y2 > dirty_y2)
            dirty_y2 = area->y2;
    }

    /*
     * LVGL renders one frame through several partial-buffer flushes.  Sending
     * each partial area to the panel immediately makes an animated object
     * visible at several positions in the same physical frame.  Compose all
     * areas in Rockbox's framebuffer first, then present the completed frame
     * with one LCD transfer.
     */
    if(lv_display_flush_is_last(display)) {
        lcd_update_rect(dirty_x1, dirty_y1,
                        dirty_x2 - dirty_x1 + 1,
                        dirty_y2 - dirty_y1 + 1);
        dirty_valid = false;
    }

    lv_display_flush_ready(display);
}

static void process_deferred_route_render(void)
{
    if(!route_render_pending ||
       TIME_BEFORE(current_tick, route_render_due))
        return;
    if(product_active && route_depth > 0)
        render_current_route(false);
    else
        route_render_pending = false;
}

void crazypod_ui_run(void)
{
    lv_display_t *display;

    lcd_set_viewport(NULL);
    lv_init();
    lv_tick_set_cb(rockbox_tick_ms);
    crazypod_artwork_init();
    crazypod_icons_init();
    crazypod_wallpaper_init();

    display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, display_flush);
    lv_display_set_antialiasing(display, true);

    create_desktop();
    create_product_screen();
    update_status_bars(NULL);
    lv_timer_create(update_status_bars, 1000, NULL);
    lv_timer_create(update_playback_ui, 250, NULL);
    lv_timer_create(update_persistent_state, 1000, NULL);

    lv_screen_load(desktop_screen);
    lv_refr_now(display);
    set_cpu_boost(true);
    boost_until = current_tick + HZ / 2;
    preview_artwork_generation_seen =
        crazypod_artwork_slot_generation(CRAZYPOD_PREVIEW_ARTWORK_SLOT);
    now_artwork_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);
    capsule_artwork_generation_seen =
        crazypod_artwork_slot_generation(CRAZYPOD_CAPSULE_ARTWORK_SLOT);

    music_scan_generation_seen = crazypod_music_scan_generation();
    music_library_loaded = false;
    music_scan_pending = true;
    usb_storage_active = false;
    music_scan_not_before = current_tick + HZ;

    while(true) {
        long button;
        int drained = 0;

        button = button_get_w_tmo(1);
        while(button != BUTTON_NONE && drained < 16) {
            intptr_t data = button_get_data();
            handle_button(button, data);
            ++drained;
            button = button_get_w_tmo(0);
        }
        service_music_scan();
        process_deferred_route_render();
        process_artwork_updates();
        tick_desktop_carousel();
        lv_timer_handler();
        render_desktop_carousel_native();
        crazypod_coverflow_tick();
        if(crazypod_artwork_busy())
            keep_cpu_boosted(HZ / 10);
        if(!lv_anim_count_running() &&
           !desktop_motion_active &&
           !crazypod_music_is_scanning() &&
           !crazypod_coverflow_active() &&
           !crazypod_artwork_busy() &&
           !TIME_BEFORE(current_tick, boost_until))
            set_cpu_boost(false);
    }
}

#endif
