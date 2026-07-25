#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "audio.h"
#include "backlight.h"
#include "button.h"
#include "dir.h"
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
#include "src/misc/cache/instance/lv_image_cache.h"

#include "crazypod_artwork.h"
#include "crazypod_appearance.h"
#include "crazypod_coverflow.h"
#include "crazypod_icons.h"
#include "crazypod_lyrics.h"
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
#define CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT 17
#define CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT 18
#define CRAZYPOD_CAPSULE_ARTWORK_SLOT 19
#define CRAZYPOD_NOW_ARTWORK_CACHE_SIZE \
    CRAZYPOD_COVERFLOW_ARTWORK_SIZE
#define CRAZYPOD_NOW_LYRICS_COVER_SIZE 108
#define CRAZYPOD_NOW_POPUP_X 35
#define CRAZYPOD_NOW_POPUP_Y 32
#define CRAZYPOD_NOW_POPUP_WIDTH 250
#define CRAZYPOD_NOW_POPUP_HEIGHT 176
#define CRAZYPOD_NOW_POPUP_RADIUS 18
#define CRAZYPOD_NOW_GLASS_SCALE 4
#define CRAZYPOD_NOW_GLASS_WIDTH 63
#define CRAZYPOD_NOW_GLASS_HEIGHT 44
#define CRAZYPOD_NOW_BACKDROP_WIDTH 40
#define CRAZYPOD_NOW_BACKDROP_HEIGHT 30
#define CRAZYPOD_NOW_PRESENTATION_BANKS 2
#define CRAZYPOD_NOW_SPECTRUM_BAR_COUNT 72
#define CRAZYPOD_NOW_SHADE_COLOR 0x050508
#define CRAZYPOD_NOW_SHADE_OPA 118
#define CRAZYPOD_WALLPAPER_DIRECTORY "/Pictures"
#define CRAZYPOD_WALLPAPER_FILE_COUNT 128
#define CRAZYPOD_SCREEN_RADIUS_MAX 32
#define CRAZYPOD_METADATA_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_GESTURE_SETTLE_TICKS \
    ((HZ * 80 / 1000) > 0 ? (HZ * 80 / 1000) : 1)
#define CRAZYPOD_MOTION_FRAME_TICKS \
    ((HZ / 30) > 0 ? (HZ / 30) : 1)
#define CRAZYPOD_NOW_WAVE_FRAME_TICKS \
    ((HZ / 5) > 0 ? (HZ / 5) : 1)
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
    DIY_ROUTE_CHOICES,
    DIY_ROUTE_BACKGROUNDS,
    DIY_ROUTE_BACKGROUND_CHOICES,
    DIY_ROUTE_WALLPAPER_FILES,
    DIY_ROUTE_LAYOUT,
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

enum now_playing_overlay {
    NOW_OVERLAY_NONE,
    NOW_OVERLAY_ACTIONS,
    NOW_OVERLAY_QUEUE,
    NOW_OVERLAY_VOLUME,
};

enum now_playing_action {
    NOW_ACTION_QUEUE,
    NOW_ACTION_PLAYBACK,
    NOW_ACTION_LYRICS,
    NOW_ACTION_VOLUME,
    NOW_ACTION_COUNT,
};

struct now_queue_popup_view {
    lv_obj_t *mode;
    lv_obj_t *count;
    lv_obj_t *empty;
    lv_obj_t *rows[3];
    lv_obj_t *icons[3];
    lv_obj_t *titles[3];
    lv_obj_t *artists[3];
};

struct now_actions_popup_view {
    lv_obj_t *queue_row;
    lv_obj_t *queue_icon;
    lv_obj_t *queue_label;
    lv_obj_t *cells[3];
    lv_obj_t *cell_icons[3];
    lv_obj_t *cell_labels[3];
    lv_obj_t *detail;
};

struct now_volume_popup_view {
    lv_obj_t *fill;
    lv_obj_t *percent;
    lv_obj_t *icon;
};

static struct crazypod_app apps[CRAZYPOD_APP_COUNT] = {
    { "Music",       LV_SYMBOL_AUDIO,      0xFF2E54, NULL, NULL, NULL, NULL },
    { "Podcasts",    LV_SYMBOL_VOLUME_MAX, 0xA95BDE, NULL, NULL, NULL, NULL },
    { "Mini Apps",   LV_SYMBOL_LIST,       0xFF9F0A, NULL, NULL, NULL, NULL },
    { "Shuffle",     LV_SYMBOL_SHUFFLE,    0xFF375F, NULL, NULL, NULL, NULL },
    { "Lock",        LV_SYMBOL_EYE_CLOSE,  0x59606B, NULL, NULL, NULL, NULL },
    { "Camera",      LV_SYMBOL_IMAGE,      0x18B8EF, NULL, NULL, NULL, NULL },
    { "Photos",      LV_SYMBOL_IMAGE,      0x3478F6, NULL, NULL, NULL, NULL },
    { "Customize",   LV_SYMBOL_EDIT,       0xBF5AF2, NULL, NULL, NULL, NULL },
    { "Fitness",     LV_SYMBOL_CHARGE,     0x30D158, NULL, NULL, NULL, NULL },
    { "Voice Memos", LV_SYMBOL_VOLUME_MAX, 0xFF453A, NULL, NULL, NULL, NULL },
    { "Books",       LV_SYMBOL_FILE,       0xFF9F0A, NULL, NULL, NULL, NULL },
    { "Notes",       LV_SYMBOL_EDIT,       0xFFD60A, NULL, NULL, NULL, NULL },
    { "Extras",      LV_SYMBOL_DIRECTORY,  0x64D2FF, NULL, NULL, NULL, NULL },
    { "Settings",    LV_SYMBOL_SETTINGS,   0x8E8E93, NULL, NULL, NULL, NULL },
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
    "Presets", "Icons", "Details", "Backgrounds", "Layout"
};

static const char *const diy_menu_symbols[] = {
    LV_SYMBOL_SAVE, LV_SYMBOL_IMAGE, LV_SYMBOL_SETTINGS,
    LV_SYMBOL_DIRECTORY, LV_SYMBOL_SHUFFLE
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
    "Icon Size", "Glow", "Highlight", "Primary", "Secondary"
};

static const enum crazypod_appearance_field diy_detail_fields[] = {
    CRAZYPOD_APPEARANCE_ICON_SCALE,
    CRAZYPOD_APPEARANCE_GLOW,
    CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE,
    CRAZYPOD_APPEARANCE_PRIMARY,
    CRAZYPOD_APPEARANCE_SECONDARY,
};

static const char *const diy_layout_titles[] = {
    "Screen Top", "Screen Bottom"
};

static const enum crazypod_appearance_field diy_layout_fields[] = {
    CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS,
    CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS,
};

static const int diy_screen_radius_values[] = {
    0, 4, 8, 12, 16, 20, 24, 32
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
static int album_flow_displayed_album = -1;
static lv_obj_t *now_progress_fill;
static lv_obj_t *now_elapsed;
static lv_obj_t *now_remaining;
static lv_obj_t *now_wave_surface;
static lv_obj_t *now_lyrics_previous;
static lv_obj_t *now_lyrics_current;
static lv_obj_t *now_lyrics_next;
static lv_obj_t *music_loading_title;
static lv_obj_t *music_loading_detail;
static lv_group_t *desktop_group;
static int selected_app;
static int route_depth;
static bool product_active;
static bool music_library_loaded;
static bool music_scan_screen;
static bool music_scan_start_failed;
static bool music_artwork_cache_failed;
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
static unsigned now_prefetch_artwork_generation_seen;
static unsigned now_artwork_generation_seen;
static unsigned capsule_artwork_generation_seen;
static long route_render_due;
static long boost_until;
static long music_scan_not_before;
static long last_desktop_motion_tick;
static int desktop_position_q8;
static int desktop_velocity_q8;
static struct menu_view_state menu_view;
static struct now_queue_popup_view now_queue_view;
static struct now_actions_popup_view now_actions_view;
static struct now_volume_popup_view now_volume_view;
static enum now_playing_overlay now_overlay;
static int now_action_selected;
static int now_queue_selected;
static unsigned now_queue_generation_seen;
static lv_obj_t *now_overlay_root;
static lv_obj_t *now_overlay_panel;
static char search_query[CRAZYPOD_SEARCH_QUERY_SIZE];
static char preset_name_editor[CRAZYPOD_PRESET_NAME_SIZE];
static char rendered_track_path[MAX_PATH];
static char now_prefetch_track_path[MAX_PATH];
static char now_presentation_track_path[
    CRAZYPOD_NOW_PRESENTATION_BANKS][MAX_PATH];
static char desktop_capsule_artwork_path[MAX_PATH];
static char diy_wallpaper_files[CRAZYPOD_WALLPAPER_FILE_COUNT][MAX_PATH];
static int diy_wallpaper_file_count;
static lv_obj_t *screen_corner_masks[CRAZYPOD_SCREEN_COUNT][4];
static uint8_t screen_corner_pixels[4][
    CRAZYPOD_SCREEN_RADIUS_MAX * CRAZYPOD_SCREEN_RADIUS_MAX * 4]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t screen_corner_descriptors[4];
static fb_data draw_buffer[LCD_WIDTH * CRAZYPOD_DRAW_ROWS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data desktop_native_backdrop[
    LCD_WIDTH * CRAZYPOD_DESKTOP_NATIVE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data now_glass_pixels[
    CRAZYPOD_NOW_GLASS_WIDTH * CRAZYPOD_NOW_GLASS_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data now_glass_scratch[
    CRAZYPOD_NOW_GLASS_WIDTH * CRAZYPOD_NOW_GLASS_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data now_glass_render_pixels[
    CRAZYPOD_NOW_POPUP_WIDTH * CRAZYPOD_NOW_POPUP_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t now_glass_descriptor;
static bool now_glass_valid;
static fb_data now_backdrop_pixels[
    CRAZYPOD_NOW_BACKDROP_WIDTH * CRAZYPOD_NOW_BACKDROP_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data now_backdrop_scratch[
    CRAZYPOD_NOW_BACKDROP_WIDTH * CRAZYPOD_NOW_BACKDROP_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data now_backdrop_render_pixels[
    CRAZYPOD_NOW_PRESENTATION_BANKS][LCD_WIDTH * LCD_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t now_backdrop_descriptor[
    CRAZYPOD_NOW_PRESENTATION_BANKS];
static fb_data now_lyrics_cover_pixels[
    CRAZYPOD_NOW_PRESENTATION_BANKS]
    [CRAZYPOD_NOW_LYRICS_COVER_SIZE * CRAZYPOD_NOW_LYRICS_COVER_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t now_lyrics_cover_descriptor[
    CRAZYPOD_NOW_PRESENTATION_BANKS];
static uint32_t now_presentation_text_color[
    CRAZYPOD_NOW_PRESENTATION_BANKS];
static bool now_presentation_valid[
    CRAZYPOD_NOW_PRESENTATION_BANKS];
static int now_presentation_active_bank = -1;
static int now_wave_phase;
static long last_now_wave_tick;
static bool now_wave_playing_seen;
static bool now_lyrics_mode;

extern struct frame_buffer_t lcd_framebuffer_default;

static int appearance_tile_size(void);
static void layout_desktop_carousel(bool animated);
static void refresh_menu_rows(const struct route_state *state);
static struct route_state *current_route(void);
static void render_current_route(bool transition);
static void cycle_playback_mode(void);
static void show_now_actions_popup(void);
static void show_now_queue_popup(void);
static void show_now_volume_popup(void);
static void dismiss_now_overlay(bool refresh_now_playing);

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

static int appearance_field_value(enum crazypod_appearance_field field)
{
    const struct crazypod_appearance *value = crazypod_appearance_get();

    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_THEME: return value->icon_theme;
    case CRAZYPOD_APPEARANCE_ICON_SCALE: return value->icon_scale;
    case CRAZYPOD_APPEARANCE_GLOW: return value->glow;
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE:
        return value->highlight_style;
    case CRAZYPOD_APPEARANCE_PRIMARY: return value->primary_color;
    case CRAZYPOD_APPEARANCE_SECONDARY: return value->secondary_color;
    case CRAZYPOD_APPEARANCE_HOME_BACKGROUND:
        return value->home_background;
    case CRAZYPOD_APPEARANCE_MENU_BACKGROUND:
        return value->menu_background;
    case CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS:
        return value->screen_top_radius;
    case CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS:
        return value->screen_bottom_radius;
    }
    return 0;
}

static int appearance_choice_count(enum crazypod_appearance_field field)
{
    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_SCALE: return 5;
    case CRAZYPOD_APPEARANCE_GLOW: return 4;
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE: return 2;
    case CRAZYPOD_APPEARANCE_PRIMARY:
    case CRAZYPOD_APPEARANCE_SECONDARY:
        return CRAZYPOD_APPEARANCE_COLOR_COUNT;
    case CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS:
    case CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS:
        return (int)(sizeof(diy_screen_radius_values) /
                     sizeof(diy_screen_radius_values[0]));
    default:
        return 0;
    }
}

static int appearance_choice_value(enum crazypod_appearance_field field,
                                   int index)
{
    if(field == CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS ||
       field == CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS)
        return diy_screen_radius_values[index];
    return index;
}

static int appearance_choice_index(enum crazypod_appearance_field field)
{
    int current = appearance_field_value(field);
    int count = appearance_choice_count(field);
    int index;

    for(index = 0; index < count; ++index) {
        if(appearance_choice_value(field, index) == current)
            return index;
    }
    return 0;
}

static const char *appearance_choice_title(
    enum crazypod_appearance_field field, int index)
{
    static char radius_text[16];
    static const char *const icon_sizes[] = {
        "80%", "90%", "100%", "110%", "120%"
    };
    static const char *const glows[] = {
        "Off", "Low", "Medium", "High"
    };
    static const char *const highlights[] = {
        "Solid", "Gradient"
    };

    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_SCALE:
        return index >= 0 && index < 5 ? icon_sizes[index] : "";
    case CRAZYPOD_APPEARANCE_GLOW:
        return index >= 0 && index < 4 ? glows[index] : "";
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE:
        return index >= 0 && index < 2 ? highlights[index] : "";
    case CRAZYPOD_APPEARANCE_PRIMARY:
    case CRAZYPOD_APPEARANCE_SECONDARY:
        return crazypod_appearance_color_name(index);
    case CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS:
    case CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS:
        snprintf(radius_text, sizeof(radius_text), "%d px",
                 appearance_choice_value(field, index));
        return radius_text;
    default:
        return "";
    }
}

static const char *appearance_field_title(
    enum crazypod_appearance_field field)
{
    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_SCALE: return "ICON SIZE";
    case CRAZYPOD_APPEARANCE_GLOW: return "GLOW";
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE: return "HIGHLIGHT";
    case CRAZYPOD_APPEARANCE_PRIMARY: return "PRIMARY";
    case CRAZYPOD_APPEARANCE_SECONDARY: return "SECONDARY";
    case CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS: return "SCREEN TOP";
    case CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS: return "SCREEN BOTTOM";
    default: return "OPTIONS";
    }
}

static const char *path_basename(const char *path)
{
    const char *name;

    if(path == NULL)
        return "";
    name = strrchr(path, '/');
    return name != NULL ? name + 1 : path;
}

static bool wallpaper_file_supported(const char *name)
{
    size_t length = name != NULL ? strlen(name) : 0;
    const char *extension;

    if(length < 4)
        return false;
    extension = strrchr(name, '.');
    return extension != NULL &&
           (strcasecmp(extension, ".jpg") == 0 ||
            strcasecmp(extension, ".jpeg") == 0 ||
            strcasecmp(extension, ".bmp") == 0);
}

static void scan_wallpaper_files(void)
{
    DIR *directory;
    struct DIRENT *entry;
    int i;

    diy_wallpaper_file_count = 0;
    mkdir(CRAZYPOD_WALLPAPER_DIRECTORY);
    directory = opendir(CRAZYPOD_WALLPAPER_DIRECTORY);
    if(directory == NULL)
        return;
    while(diy_wallpaper_file_count < CRAZYPOD_WALLPAPER_FILE_COUNT &&
          (entry = readdir(directory)) != NULL) {
        struct dirinfo info = dir_get_info(directory, entry);
        size_t directory_length =
            strlen(CRAZYPOD_WALLPAPER_DIRECTORY);
        size_t name_length = strlen(entry->d_name);
        char *path;

        if((info.attribute & ATTR_DIRECTORY) ||
           !wallpaper_file_supported(entry->d_name))
            continue;
        if(directory_length + 1 + name_length >= MAX_PATH)
            continue;
        path = diy_wallpaper_files[diy_wallpaper_file_count];
        memcpy(path, CRAZYPOD_WALLPAPER_DIRECTORY, directory_length);
        path[directory_length] = '/';
        memcpy(path + directory_length + 1,
               entry->d_name, name_length + 1);
        ++diy_wallpaper_file_count;
    }
    closedir(directory);

    for(i = 1; i < diy_wallpaper_file_count; ++i) {
        char current[MAX_PATH];
        int position = i;

        snprintf(current, sizeof(current), "%s",
                 diy_wallpaper_files[i]);
        while(position > 0 &&
              strcasecmp(path_basename(diy_wallpaper_files[position - 1]),
                         path_basename(current)) > 0) {
            snprintf(diy_wallpaper_files[position], MAX_PATH, "%s",
                     diy_wallpaper_files[position - 1]);
            --position;
        }
        snprintf(diy_wallpaper_files[position], MAX_PATH, "%s", current);
    }
}

static void rebuild_screen_corner_descriptors(void)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    int corner;

    for(corner = 0; corner < 4; ++corner) {
        int radius = corner < 2
            ? appearance->screen_top_radius
            : appearance->screen_bottom_radius;
        lv_image_dsc_t *descriptor = &screen_corner_descriptors[corner];
        uint8_t *pixels = screen_corner_pixels[corner];
        int center_x = (corner == 0 || corner == 2) ? radius - 1 : 0;
        int center_y = corner < 2 ? radius - 1 : 0;
        int inner_radius = radius > 0 ? radius - 1 : 0;
        int inner_squared = inner_radius * inner_radius;
        int outer_squared = radius * radius;
        int y;

        if(descriptor->header.magic == LV_IMAGE_HEADER_MAGIC)
            lv_image_cache_drop(descriptor);
        memset(descriptor, 0, sizeof(*descriptor));
        if(radius <= 0)
            continue;
        for(y = 0; y < radius; ++y) {
            int x;
            for(x = 0; x < radius; ++x) {
                int dx = x - center_x;
                int dy = y - center_y;
                int distance_squared = dx * dx + dy * dy;
                unsigned alpha;
                uint8_t *pixel = pixels + (y * radius + x) * 4;

                if(distance_squared <= inner_squared)
                    alpha = 0;
                else if(distance_squared >= outer_squared)
                    alpha = 255;
                else
                    alpha = (unsigned)(distance_squared - inner_squared) *
                            255 / (outer_squared - inner_squared);
                pixel[0] = 0;
                pixel[1] = 0;
                pixel[2] = 0;
                pixel[3] = alpha;
            }
        }
        descriptor->header.magic = LV_IMAGE_HEADER_MAGIC;
        descriptor->header.cf = LV_COLOR_FORMAT_ARGB8888;
        descriptor->header.w = radius;
        descriptor->header.h = radius;
        descriptor->header.stride = radius * 4;
        descriptor->data_size = radius * radius * 4;
        descriptor->data = pixels;
    }
}

static void refresh_screen_corner_masks(void)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    int screen;

    rebuild_screen_corner_descriptors();
    for(screen = 0; screen < CRAZYPOD_SCREEN_COUNT; ++screen) {
        int corner;
        for(corner = 0; corner < 4; ++corner) {
            lv_obj_t *mask = screen_corner_masks[screen][corner];
            int radius = corner < 2
                ? appearance->screen_top_radius
                : appearance->screen_bottom_radius;

            if(mask == NULL)
                continue;
            if(radius <= 0) {
                lv_obj_add_flag(mask, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_image_set_src(mask, &screen_corner_descriptors[corner]);
            lv_obj_set_size(mask, radius, radius);
            lv_obj_set_pos(mask,
                           (corner == 0 || corner == 2)
                               ? 0 : LCD_WIDTH - radius,
                           corner < 2 ? 0 : LCD_HEIGHT - radius);
            lv_obj_remove_flag(mask, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(mask);
            lv_obj_invalidate(mask);
        }
    }
}

static void create_screen_corner_masks(lv_obj_t *screen, int screen_index)
{
    int corner;

    for(corner = 0; corner < 4; ++corner) {
        lv_obj_t *mask = lv_image_create(screen);
        screen_corner_masks[screen_index][corner] = mask;
        lv_obj_remove_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    }
    refresh_screen_corner_masks();
}

static void refresh_desktop_capsule_material(void)
{
    const lv_image_dsc_t *glass =
        crazypod_frosted_wallpaper_capsule();

    if(desktop_capsule == NULL)
        return;
    if(crazypod_appearance_get()->home_wallpaper[0] == '\0' &&
       crazypod_appearance_get()->home_background == 0 &&
       glass != NULL) {
        if(desktop_capsule_glass == NULL) {
            desktop_capsule_glass = lv_image_create(desktop_capsule);
            lv_obj_set_pos(desktop_capsule_glass, 0, 0);
            lv_obj_remove_flag(
                desktop_capsule_glass, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_move_to_index(desktop_capsule_glass, 0);
        }
        lv_image_set_src(desktop_capsule_glass, glass);
        lv_obj_remove_flag(desktop_capsule_glass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(desktop_capsule, LV_OPA_TRANSP, 0);
    }
    else {
        if(desktop_capsule_glass != NULL)
            lv_obj_add_flag(
                desktop_capsule_glass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(
            desktop_capsule, lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(desktop_capsule, 34, 0);
    }
}

static void refresh_desktop_appearance(void)
{
    const lv_image_dsc_t *custom =
        crazypod_custom_home_wallpaper();
    int i;

    if(desktop_screen == NULL)
        return;
    if(custom != NULL) {
        lv_image_set_src(desktop_wallpaper, custom);
        lv_obj_remove_flag(desktop_wallpaper, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(desktop_screen,
                                  lv_color_hex(0x141419), 0);
    }
    else if(crazypod_appearance_get()->home_wallpaper[0] == '\0' &&
            crazypod_appearance_get()->home_background == 0 &&
            crazypod_default_wallpaper() != NULL) {
        lv_image_set_src(desktop_wallpaper,
                         crazypod_default_wallpaper());
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
            lv_obj_remove_flag(apps[i].image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(apps[i].symbol_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(apps[i].tile, LV_OPA_TRANSP, 0);
        }
        else {
            lv_obj_add_flag(apps[i].image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(apps[i].symbol_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(apps[i].tile, LV_OPA_COVER, 0);
        }
    }
    refresh_desktop_capsule_material();
    desktop_native_backdrop_ready = false;
    layout_desktop_carousel(false);
    refresh_screen_corner_masks();
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

static const struct crazypod_track *queue_track(int queue_index)
{
    const char *path = crazypod_queue_path(queue_index);
    int library_index = crazypod_music_find_track(path);

    return crazypod_music_track(library_index);
}

static void prefetch_now_queue_artwork(int queue_index)
{
    const struct crazypod_track *track = queue_track(queue_index);

    if(track == NULL) {
        now_prefetch_track_path[0] = '\0';
        return;
    }
    snprintf(now_prefetch_track_path,
             sizeof(now_prefetch_track_path),
             "%s", track->path);
    (void)crazypod_artwork_load_priority(
        CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT,
        track, CRAZYPOD_NOW_ARTWORK_CACHE_SIZE, 0);
    keep_cpu_boosted(HZ / 5);
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

static void scale_rgb565_bilinear(const fb_data *source,
                                  int source_width, int source_height,
                                  fb_data *destination,
                                  int destination_width,
                                  int destination_height)
{
    int y;

    for(y = 0; y < destination_height; ++y) {
        int source_y_q8 =
            y * (source_height - 1) * 256 /
            (destination_height - 1);
        int y0 = source_y_q8 >> 8;
        int y1 = y0 + 1 < source_height ? y0 + 1 : y0;
        int fy = source_y_q8 & 255;
        int x;
        for(x = 0; x < destination_width; ++x) {
            int source_x_q8 =
                x * (source_width - 1) * 256 /
                (destination_width - 1);
            int x0 = source_x_q8 >> 8;
            int x1 = x0 + 1 < source_width ? x0 + 1 : x0;
            int fx = source_x_q8 & 255;
            fb_data p00 = source[y0 * source_width + x0];
            fb_data p10 = source[y0 * source_width + x1];
            fb_data p01 = source[y1 * source_width + x0];
            fb_data p11 = source[y1 * source_width + x1];
            unsigned red0 =
                RGB_UNPACK_RED(p00) * (256 - fx) +
                RGB_UNPACK_RED(p10) * fx;
            unsigned red1 =
                RGB_UNPACK_RED(p01) * (256 - fx) +
                RGB_UNPACK_RED(p11) * fx;
            unsigned green0 =
                RGB_UNPACK_GREEN(p00) * (256 - fx) +
                RGB_UNPACK_GREEN(p10) * fx;
            unsigned green1 =
                RGB_UNPACK_GREEN(p01) * (256 - fx) +
                RGB_UNPACK_GREEN(p11) * fx;
            unsigned blue0 =
                RGB_UNPACK_BLUE(p00) * (256 - fx) +
                RGB_UNPACK_BLUE(p10) * fx;
            unsigned blue1 =
                RGB_UNPACK_BLUE(p01) * (256 - fx) +
                RGB_UNPACK_BLUE(p11) * fx;
            destination[y * destination_width + x] =
                LCD_RGBPACK(
                    (red0 * (256 - fy) + red1 * fy) >> 16,
                    (green0 * (256 - fy) + green1 * fy) >> 16,
                    (blue0 * (256 - fy) + blue1 * fy) >> 16);
        }
    }
}

static void configure_rgb565_descriptor(lv_image_dsc_t *descriptor,
                                        const fb_data *pixels,
                                        int width, int height)
{
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->header.magic = LV_IMAGE_HEADER_MAGIC;
    descriptor->header.cf = LV_COLOR_FORMAT_RGB565;
    descriptor->header.w = width;
    descriptor->header.h = height;
    descriptor->header.stride = width * sizeof(fb_data);
    descriptor->data_size =
        (size_t)width * height * sizeof(fb_data);
    descriptor->data = (const uint8_t *)pixels;
}

static bool prepare_now_cover(const lv_image_dsc_t *source, int bank)
{
    const fb_data *source_pixels;
    int width;
    int height;

    if(source == NULL || source->data == NULL ||
       bank < 0 || bank >= CRAZYPOD_NOW_PRESENTATION_BANKS ||
       source->header.cf != LV_COLOR_FORMAT_RGB565)
        return false;
    width = source->header.w;
    height = source->header.h;
    if(width <= 1 || height <= 1 ||
       source->header.stride != width * sizeof(fb_data))
        return false;

    if(now_lyrics_cover_descriptor[bank].header.magic ==
       LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&now_lyrics_cover_descriptor[bank]);

    source_pixels = (const fb_data *)source->data;
    scale_rgb565_bilinear(
        source_pixels, width, height,
        now_lyrics_cover_pixels[bank],
        CRAZYPOD_NOW_LYRICS_COVER_SIZE,
        CRAZYPOD_NOW_LYRICS_COVER_SIZE);
    configure_rgb565_descriptor(
        &now_lyrics_cover_descriptor[bank],
        now_lyrics_cover_pixels[bank],
        CRAZYPOD_NOW_LYRICS_COVER_SIZE,
        CRAZYPOD_NOW_LYRICS_COVER_SIZE);
    return true;
}

static void prepare_now_overlay_glass(void)
{
    const fb_data *framebuffer =
        (const fb_data *)lcd_framebuffer_default.data;
    int y;

    keep_cpu_boosted(HZ / 2);
    /*
     * Capture the already-rendered Now Playing surface at quarter size.
     * The popup reuses this frozen material; no blur work runs while the
     * wheel is moving and the full 250x176 source never needs a second copy.
     */
    lv_refr_now(NULL);
    for(y = 0; y < CRAZYPOD_NOW_GLASS_HEIGHT; ++y) {
        int source_y = CRAZYPOD_NOW_POPUP_Y +
                       y * CRAZYPOD_NOW_GLASS_SCALE;
        int x;
        for(x = 0; x < CRAZYPOD_NOW_GLASS_WIDTH; ++x) {
            int source_x = CRAZYPOD_NOW_POPUP_X +
                           x * CRAZYPOD_NOW_GLASS_SCALE;
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            unsigned samples = 0;
            int sy;

            for(sy = 0; sy < CRAZYPOD_NOW_GLASS_SCALE; ++sy) {
                int py = source_y + sy;
                int sx;
                if(py >= LCD_HEIGHT)
                    break;
                for(sx = 0; sx < CRAZYPOD_NOW_GLASS_SCALE; ++sx) {
                    int px = source_x + sx;
                    fb_data pixel;
                    if(px >= LCD_WIDTH)
                        break;
                    pixel = framebuffer[py * LCD_WIDTH + px];
                    red += RGB_UNPACK_RED(pixel);
                    green += RGB_UNPACK_GREEN(pixel);
                    blue += RGB_UNPACK_BLUE(pixel);
                    ++samples;
                }
            }
            if(samples == 0)
                samples = 1;
            now_glass_pixels[
                y * CRAZYPOD_NOW_GLASS_WIDTH + x] =
                    LCD_RGBPACK(red / samples,
                                green / samples,
                                blue / samples);
        }
    }

    for(y = 0; y < CRAZYPOD_NOW_GLASS_HEIGHT; ++y) {
        int x;
        for(x = 0; x < CRAZYPOD_NOW_GLASS_WIDTH; ++x) {
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            unsigned samples = 0;
            int offset;

            for(offset = -2; offset <= 2; ++offset) {
                int sample_x = x + offset;
                fb_data pixel;
                if(sample_x < 0)
                    sample_x = 0;
                if(sample_x >= CRAZYPOD_NOW_GLASS_WIDTH)
                    sample_x = CRAZYPOD_NOW_GLASS_WIDTH - 1;
                pixel = now_glass_pixels[
                    y * CRAZYPOD_NOW_GLASS_WIDTH + sample_x];
                red += RGB_UNPACK_RED(pixel);
                green += RGB_UNPACK_GREEN(pixel);
                blue += RGB_UNPACK_BLUE(pixel);
                ++samples;
            }
            now_glass_scratch[
                y * CRAZYPOD_NOW_GLASS_WIDTH + x] =
                    LCD_RGBPACK(red / samples,
                                green / samples,
                                blue / samples);
        }
    }

    for(y = 0; y < CRAZYPOD_NOW_GLASS_HEIGHT; ++y) {
        int x;
        for(x = 0; x < CRAZYPOD_NOW_GLASS_WIDTH; ++x) {
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            unsigned samples = 0;
            int offset;
            fb_data blurred;
            fb_data tint = LCD_RGBPACK(18, 19, 27);

            for(offset = -2; offset <= 2; ++offset) {
                int sample_y = y + offset;
                fb_data pixel;
                if(sample_y < 0)
                    sample_y = 0;
                if(sample_y >= CRAZYPOD_NOW_GLASS_HEIGHT)
                    sample_y = CRAZYPOD_NOW_GLASS_HEIGHT - 1;
                pixel = now_glass_scratch[
                    sample_y * CRAZYPOD_NOW_GLASS_WIDTH + x];
                red += RGB_UNPACK_RED(pixel);
                green += RGB_UNPACK_GREEN(pixel);
                blue += RGB_UNPACK_BLUE(pixel);
                ++samples;
            }
            blurred = LCD_RGBPACK(red / samples,
                                  green / samples,
                                  blue / samples);
            now_glass_pixels[
                y * CRAZYPOD_NOW_GLASS_WIDTH + x] =
                    desktop_blend565(tint, blurred, 104);
        }
    }

    scale_rgb565_bilinear(
        now_glass_pixels,
        CRAZYPOD_NOW_GLASS_WIDTH, CRAZYPOD_NOW_GLASS_HEIGHT,
        now_glass_render_pixels,
        CRAZYPOD_NOW_POPUP_WIDTH, CRAZYPOD_NOW_POPUP_HEIGHT);

    memset(&now_glass_descriptor, 0, sizeof(now_glass_descriptor));
    now_glass_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    now_glass_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
    now_glass_descriptor.header.w = CRAZYPOD_NOW_POPUP_WIDTH;
    now_glass_descriptor.header.h = CRAZYPOD_NOW_POPUP_HEIGHT;
    now_glass_descriptor.header.stride =
        CRAZYPOD_NOW_POPUP_WIDTH * sizeof(fb_data);
    now_glass_descriptor.data_size = sizeof(now_glass_render_pixels);
    now_glass_descriptor.data =
        (const uint8_t *)now_glass_render_pixels;
    now_glass_valid = true;
}

static bool prepare_now_backdrop(const lv_image_dsc_t *artwork, int bank)
{
    const fb_data *source;
    int source_stride;
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    int y;

    if(artwork == NULL ||
       bank < 0 || bank >= CRAZYPOD_NOW_PRESENTATION_BANKS ||
       artwork->header.cf != LV_COLOR_FORMAT_RGB565 ||
       artwork->header.w <= 0 || artwork->header.h <= 0)
        return false;

    source = (const fb_data *)artwork->data;
    source_stride = artwork->header.stride / sizeof(fb_data);
    crop_x = 0;
    crop_y = 0;
    crop_width = artwork->header.w;
    crop_height = artwork->header.h;
    if(crop_width * 3 > crop_height * 4) {
        int target_width = crop_height * 4 / 3;
        crop_x = (crop_width - target_width) / 2;
        crop_width = target_width;
    }
    else {
        int target_height = crop_width * 3 / 4;
        crop_y = (crop_height - target_height) / 2;
        crop_height = target_height;
    }

    for(y = 0; y < CRAZYPOD_NOW_BACKDROP_HEIGHT; ++y) {
        int sy0 = crop_y +
                  y * crop_height / CRAZYPOD_NOW_BACKDROP_HEIGHT;
        int sy1 = crop_y +
                  (y + 1) * crop_height /
                      CRAZYPOD_NOW_BACKDROP_HEIGHT;
        int x;
        if(sy1 <= sy0)
            sy1 = sy0 + 1;
        for(x = 0; x < CRAZYPOD_NOW_BACKDROP_WIDTH; ++x) {
            int sx0 = crop_x +
                      x * crop_width / CRAZYPOD_NOW_BACKDROP_WIDTH;
            int sx1 = crop_x +
                      (x + 1) * crop_width /
                          CRAZYPOD_NOW_BACKDROP_WIDTH;
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            unsigned samples = 0;
            int sy;
            if(sx1 <= sx0)
                sx1 = sx0 + 1;
            for(sy = sy0; sy < sy1; ++sy) {
                int sx;
                for(sx = sx0; sx < sx1; ++sx) {
                    fb_data pixel = source[sy * source_stride + sx];
                    red += RGB_UNPACK_RED(pixel);
                    green += RGB_UNPACK_GREEN(pixel);
                    blue += RGB_UNPACK_BLUE(pixel);
                    ++samples;
                }
            }
            if(samples == 0)
                samples = 1;
            now_backdrop_pixels[
                y * CRAZYPOD_NOW_BACKDROP_WIDTH + x] =
                    LCD_RGBPACK(red / samples,
                                green / samples,
                                blue / samples);
        }
    }

    for(y = 0; y < CRAZYPOD_NOW_BACKDROP_HEIGHT; ++y) {
        int x;
        for(x = 0; x < CRAZYPOD_NOW_BACKDROP_WIDTH; ++x) {
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            int offset;
            for(offset = -2; offset <= 2; ++offset) {
                int sample_x = x + offset;
                fb_data pixel;
                if(sample_x < 0)
                    sample_x = 0;
                if(sample_x >= CRAZYPOD_NOW_BACKDROP_WIDTH)
                    sample_x = CRAZYPOD_NOW_BACKDROP_WIDTH - 1;
                pixel = now_backdrop_pixels[
                    y * CRAZYPOD_NOW_BACKDROP_WIDTH + sample_x];
                red += RGB_UNPACK_RED(pixel);
                green += RGB_UNPACK_GREEN(pixel);
                blue += RGB_UNPACK_BLUE(pixel);
            }
            now_backdrop_scratch[
                y * CRAZYPOD_NOW_BACKDROP_WIDTH + x] =
                    LCD_RGBPACK(red / 5, green / 5, blue / 5);
        }
    }

    for(y = 0; y < CRAZYPOD_NOW_BACKDROP_HEIGHT; ++y) {
        int x;
        for(x = 0; x < CRAZYPOD_NOW_BACKDROP_WIDTH; ++x) {
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            int offset;
            for(offset = -2; offset <= 2; ++offset) {
                int sample_y = y + offset;
                fb_data pixel;
                if(sample_y < 0)
                    sample_y = 0;
                if(sample_y >= CRAZYPOD_NOW_BACKDROP_HEIGHT)
                    sample_y = CRAZYPOD_NOW_BACKDROP_HEIGHT - 1;
                pixel = now_backdrop_scratch[
                    sample_y * CRAZYPOD_NOW_BACKDROP_WIDTH + x];
                red += RGB_UNPACK_RED(pixel);
                green += RGB_UNPACK_GREEN(pixel);
                blue += RGB_UNPACK_BLUE(pixel);
            }
            now_backdrop_pixels[
                y * CRAZYPOD_NOW_BACKDROP_WIDTH + x] =
                    LCD_RGBPACK(red / 5, green / 5, blue / 5);
        }
    }

    scale_rgb565_bilinear(
        now_backdrop_pixels,
        CRAZYPOD_NOW_BACKDROP_WIDTH, CRAZYPOD_NOW_BACKDROP_HEIGHT,
        now_backdrop_render_pixels[bank], LCD_WIDTH, LCD_HEIGHT);
    if(now_backdrop_descriptor[bank].header.magic ==
       LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&now_backdrop_descriptor[bank]);
    configure_rgb565_descriptor(
        &now_backdrop_descriptor[bank],
        now_backdrop_render_pixels[bank],
        LCD_WIDTH, LCD_HEIGHT);
    return true;
}

static unsigned now_shaded_luminance(unsigned red,
                                     unsigned green,
                                     unsigned blue)
{
    red = (red * (255 - CRAZYPOD_NOW_SHADE_OPA) +
           5 * CRAZYPOD_NOW_SHADE_OPA + 127) / 255;
    green = (green * (255 - CRAZYPOD_NOW_SHADE_OPA) +
             5 * CRAZYPOD_NOW_SHADE_OPA + 127) / 255;
    blue = (blue * (255 - CRAZYPOD_NOW_SHADE_OPA) +
            8 * CRAZYPOD_NOW_SHADE_OPA + 127) / 255;
    return (54 * red + 183 * green + 19 * blue) >> 8;
}

static uint32_t now_contrast_color(unsigned luminance)
{
    return luminance >= 118 ? 0x09090D : COLOR_WHITE;
}

static uint32_t now_presentation_contrast_color(int bank)
{
    const fb_data *pixels = now_backdrop_render_pixels[bank];
    unsigned long luminance = 0;
    unsigned samples = 0;
    int y;

    /*
     * Sample the actual right-side information region after applying the
     * same dark shade used by the LVGL tree. The blurred backdrop is already
     * spatially smooth, so a small fixed grid is enough and runs only once
     * when a new presentation is published.
     */
    for(y = 68; y <= 148; y += 8) {
        int x;
        for(x = 144; x <= 296; x += 8) {
            fb_data pixel = pixels[y * LCD_WIDTH + x];
            unsigned red = RGB_UNPACK_RED(pixel);
            unsigned green = RGB_UNPACK_GREEN(pixel);
            unsigned blue = RGB_UNPACK_BLUE(pixel);

            luminance += now_shaded_luminance(red, green, blue);
            ++samples;
        }
    }
    if(samples == 0)
        return COLOR_WHITE;
    return now_contrast_color(luminance / samples);
}

static uint32_t now_fallback_contrast_color(
    const struct crazypod_track *track)
{
    uint32_t first = artwork_color(
        track != NULL ? track->album : "", 0);
    uint32_t second = artwork_color(
        track != NULL ? track->artist : "", 1);
    unsigned red =
        (((first >> 16) & 0xff) + ((second >> 16) & 0xff)) / 2;
    unsigned green =
        (((first >> 8) & 0xff) + ((second >> 8) & 0xff)) / 2;
    unsigned blue =
        ((first & 0xff) + (second & 0xff)) / 2;

    return now_contrast_color(
        now_shaded_luminance(red, green, blue));
}

static bool prepare_now_presentation(
    const lv_image_dsc_t *artwork, const char *track_path)
{
    int bank;

    if(artwork == NULL || track_path == NULL)
        return false;
    bank = now_presentation_active_bank == 0 ? 1 : 0;
    now_presentation_valid[bank] = false;
    keep_cpu_boosted(HZ / 2);
    if(!prepare_now_cover(artwork, bank) ||
       !prepare_now_backdrop(artwork, bank))
        return false;

    now_presentation_text_color[bank] =
        now_presentation_contrast_color(bank);
    snprintf(now_presentation_track_path[bank],
             sizeof(now_presentation_track_path[bank]),
             "%s", track_path);
    now_presentation_valid[bank] = true;
    now_presentation_active_bank = bank;
    return true;
}

static void draw_now_wave_event(lv_event_t *event)
{
    static const uint8_t bar_height[
        CRAZYPOD_NOW_SPECTRUM_BAR_COUNT] = {
         2,  4,  3,  7,  5, 10,  4, 13,  6, 18,  8, 12,
        22,  7, 15, 10, 24, 12,  8, 17,  6, 20, 11,  5,
        14,  9, 23, 13,  7, 19, 10, 16,  6, 21, 12,  8,
        18,  5, 14,  9, 24, 11, 17,  7, 20, 13,  6, 15,
        10, 22,  8, 16,  5, 19, 12,  7, 23,  9, 14,  6,
        18, 11,  4, 15,  8, 12,  5, 10,  3,  7,  4,  2
    };
    lv_obj_t *surface = lv_event_get_target(event);
    lv_layer_t *layer;
    lv_area_t area;
    lv_draw_rect_dsc_t bar;
    bool playing;
    int index;

    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN)
        return;
    layer = lv_event_get_layer(event);
    lv_obj_get_coords(surface, &area);
    playing = (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
              (audio_status() & AUDIO_STATUS_PAUSE) == 0;

    lv_draw_rect_dsc_init(&bar);
    bar.bg_color = lv_color_hex(COLOR_WHITE);
    bar.bg_opa = playing ? 220 : 120;
    bar.radius = 0;
    for(index = 0;
        index < CRAZYPOD_NOW_SPECTRUM_BAR_COUNT;
        ++index) {
        lv_area_t bar_area;
        int height;

        if(playing) {
            int modulation =
                ((index * 13 + now_wave_phase * 7) ^
                 (index * 3 + now_wave_phase)) % 5;
            height = bar_height[index] + modulation - 2;
            if(height < 2)
                height = 2;
            if(height > 25)
                height = 25;
        }
        else {
            height = 2 + bar_height[index] % 3;
        }
        bar_area.x1 = area.x1 + index * 4;
        bar_area.x2 = bar_area.x1 + 1;
        bar_area.y2 = area.y2;
        bar_area.y1 = bar_area.y2 - height + 1;
        lv_draw_rect(layer, &bar, &bar_area);
    }
}

static void tick_now_playing_wave(void)
{
    bool playing;

    if(!product_active || route_depth <= 0 ||
       current_route()->route != MUSIC_ROUTE_NOW_PLAYING ||
       now_wave_surface == NULL || now_overlay != NOW_OVERLAY_NONE)
        return;
    playing = (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
              (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    if(!playing && !now_wave_playing_seen)
        return;
    if(TIME_BEFORE(current_tick,
                   last_now_wave_tick + CRAZYPOD_NOW_WAVE_FRAME_TICKS))
        return;
    last_now_wave_tick = current_tick;
    now_wave_playing_seen = playing;
    if(playing)
        now_wave_phase = (now_wave_phase + 1) & 0x7fff;
    lv_obj_invalidate(now_wave_surface);
}

static lv_obj_t *make_now_glass_panel(int x, int y, int width, int height)
{
    lv_obj_t *panel = make_box(
        now_overlay_root, x, y, width, height,
        CRAZYPOD_NOW_POPUP_RADIUS, COLOR_PANEL, LV_OPA_COVER);
    lv_obj_t *tint;
    lv_obj_t *border;

    lv_obj_set_style_clip_corner(panel, true, 0);
    lv_obj_set_style_shadow_width(panel, 12, 0);
    lv_obj_set_style_shadow_offset_y(panel, 6, 0);
    lv_obj_set_style_shadow_color(panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(panel, 92, 0);

    if(now_glass_valid) {
        lv_obj_t *image = lv_image_create(panel);
        lv_image_set_src(image, &now_glass_descriptor);
        lv_obj_center(image);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }

    tint = make_box(panel, 0, 0, width, height,
                    CRAZYPOD_NOW_POPUP_RADIUS, 0x11131A, 92);
    lv_obj_remove_flag(tint, LV_OBJ_FLAG_CLICKABLE);
    border = make_box(panel, 0, 0, width, height,
                      CRAZYPOD_NOW_POPUP_RADIUS,
                      COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(border, 1, 0);
    lv_obj_set_style_border_color(
        border, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(border, 38, 0);
    lv_obj_remove_flag(border, LV_OBJ_FLAG_CLICKABLE);
    return panel;
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

}

static void render_desktop_carousel_native(void)
{
    static const int size_percent[] = { 100, 72, 56, 44 };
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
            draw_desktop_icon(i, center_x, center_y,
                              icon_size, 255);
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

    app->symbol_label = make_label(
        app->tile, app->symbol, &lv_font_montserrat_16,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_center(app->symbol_label);
    if(descriptor != NULL) {
        lv_obj_add_flag(app->symbol_label, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(app->image, LV_OBJ_FLAG_HIDDEN);
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
                               COLOR_WHITE, 34);
    capsule = desktop_capsule;

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
    const lv_image_dsc_t *wallpaper =
        crazypod_custom_home_wallpaper();
    int i;

    desktop_screen = lv_obj_create(NULL);
    set_plain_object(desktop_screen);
    lv_obj_set_style_bg_color(
        desktop_screen,
        lv_color_hex(crazypod_appearance_home_color()), 0);
    lv_obj_set_style_bg_opa(desktop_screen, LV_OPA_COVER, 0);
    if(wallpaper == NULL &&
       crazypod_appearance_get()->home_wallpaper[0] == '\0' &&
       crazypod_appearance_get()->home_background == 0)
        wallpaper = crazypod_default_wallpaper();
    desktop_wallpaper = lv_image_create(desktop_screen);
    if(wallpaper != NULL)
        lv_image_set_src(desktop_wallpaper, wallpaper);
    if(wallpaper == NULL)
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
    create_screen_corner_masks(desktop_screen, 0);
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
    create_screen_corner_masks(product_screen, 1);

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
        return 5;
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
        return (int)(sizeof(diy_detail_titles) /
                     sizeof(diy_detail_titles[0]));
    case DIY_ROUTE_CHOICES:
        return appearance_choice_count(
            (enum crazypod_appearance_field)state->group);
    case DIY_ROUTE_BACKGROUNDS:
        return 2;
    case DIY_ROUTE_BACKGROUND_CHOICES:
        return CRAZYPOD_APPEARANCE_COLOR_COUNT + 2;
    case DIY_ROUTE_WALLPAPER_FILES:
        return diy_wallpaper_file_count;
    case DIY_ROUTE_LAYOUT:
        return (int)(sizeof(diy_layout_titles) /
                     sizeof(diy_layout_titles[0]));
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
        return index >= 0 &&
               index < (int)(sizeof(diy_menu_titles) /
                             sizeof(diy_menu_titles[0]))
            ? diy_menu_titles[index] : "";
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
        return index >= 0 &&
               index < (int)(sizeof(diy_detail_titles) /
                             sizeof(diy_detail_titles[0]))
            ? diy_detail_titles[index] : "";
    case DIY_ROUTE_CHOICES:
        return index >= 0 &&
               index < appearance_choice_count(
                   (enum crazypod_appearance_field)state->group)
            ? appearance_choice_title(
                  (enum crazypod_appearance_field)state->group, index)
            : "";
    case DIY_ROUTE_BACKGROUNDS:
        return index >= 0 && index < 2
            ? diy_background_titles[index] : "";
    case DIY_ROUTE_BACKGROUND_CHOICES:
        if(index == 0)
            return "Default";
        if(index <= CRAZYPOD_APPEARANCE_COLOR_COUNT)
            return crazypod_appearance_color_name(index - 1);
        return index == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
            ? "Choose Picture" : "";
    case DIY_ROUTE_WALLPAPER_FILES:
        return index >= 0 && index < diy_wallpaper_file_count
            ? path_basename(diy_wallpaper_files[index]) : "";
    case DIY_ROUTE_LAYOUT:
        return index >= 0 &&
               index < (int)(sizeof(diy_layout_titles) /
                             sizeof(diy_layout_titles[0]))
            ? diy_layout_titles[index] : "";
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
    case DIY_ROUTE_CHOICES:
        return appearance_field_title(
            (enum crazypod_appearance_field)state->group);
    case DIY_ROUTE_BACKGROUNDS: return "BACKGROUNDS";
    case DIY_ROUTE_BACKGROUND_CHOICES:
        return state->group == CRAZYPOD_APPEARANCE_HOME_BACKGROUND
            ? "HOME" : "MENU";
    case DIY_ROUTE_WALLPAPER_FILES: return "PICTURES";
    case DIY_ROUTE_LAYOUT: return "LAYOUT";
    }
    return "";
}

static bool route_item_is_current(const struct route_state *state, int index)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();

    switch(state->route) {
    case DIY_ROUTE_ICONS:
        return index == appearance->icon_theme;
    case DIY_ROUTE_CHOICES: {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)state->group;
        return appearance_choice_value(field, index) ==
               appearance_field_value(field);
    }
    case DIY_ROUTE_BACKGROUND_CHOICES: {
        const char *path =
            state->group == CRAZYPOD_APPEARANCE_HOME_BACKGROUND
                ? appearance->home_wallpaper
                : appearance->menu_wallpaper;
        if(index == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1)
            return path[0] != '\0';
        return path[0] == '\0' &&
               index == appearance_field_value(
                   (enum crazypod_appearance_field)state->group);
    }
    case DIY_ROUTE_WALLPAPER_FILES: {
        const char *path =
            state->group == CRAZYPOD_APPEARANCE_HOME_BACKGROUND
                ? appearance->home_wallpaper
                : appearance->menu_wallpaper;
        return index >= 0 && index < diy_wallpaper_file_count &&
               strcmp(path, diy_wallpaper_files[index]) == 0;
    }
    default:
        return false;
    }
}

static lv_obj_t *create_artwork_descriptor(
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int display_size,
    const lv_image_dsc_t *descriptor, bool scale_descriptor)
{
    lv_obj_t *card = make_box(parent, x, y,
                              display_size, display_size,
                              display_size > 80 ? 0 : 7,
                              artwork_color(track != NULL ? track->album : "",
                                            0),
                              LV_OPA_COVER);

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
        if(scale_descriptor && descriptor->header.w != display_size)
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

static lv_obj_t *create_artwork_cached(lv_obj_t *parent,
                                       const struct crazypod_track *track,
                                       int x, int y, int display_size,
                                       int cache_size, int slot)
{
    const lv_image_dsc_t *descriptor =
        track != NULL
            ? crazypod_artwork_load(slot, track, cache_size) : NULL;

    return create_artwork_descriptor(
        parent, track, x, y, display_size, descriptor, true);
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
    enum crazypod_appearance_field field;
    int current;
    int index;

    if(state->route == DIY_ROUTE_ICONS)
        return state->selected == value->icon_theme
            ? "Current selection" : "Select to switch now";
    if(state->route == DIY_ROUTE_DETAILS) {
        field = diy_detail_fields[state->selected];
        current = appearance_field_value(field);
        return appearance_choice_title(field, current);
    }
    if(state->route == DIY_ROUTE_LAYOUT) {
        static char radius_text[16];
        field = diy_layout_fields[state->selected];
        snprintf(radius_text, sizeof(radius_text), "%d px",
                 appearance_field_value(field));
        return radius_text;
    }
    if(state->route == DIY_ROUTE_CHOICES) {
        field = (enum crazypod_appearance_field)state->group;
        current = appearance_field_value(field);
        index = appearance_choice_value(field, state->selected);
        return index == current
            ? "Current selection" : "Select to apply";
    }
    if(state->route == DIY_ROUTE_BACKGROUNDS) {
        const char *path = state->selected == 0
            ? value->home_wallpaper : value->menu_wallpaper;
        int color = state->selected == 0
            ? value->home_background : value->menu_background;
        if(path[0] != '\0')
            return path_basename(path);
        return color == 0 ? "Default"
                          : crazypod_appearance_color_name(color - 1);
    }
    if(state->route == DIY_ROUTE_BACKGROUND_CHOICES) {
        const char *path =
            state->group == CRAZYPOD_APPEARANCE_HOME_BACKGROUND
                ? value->home_wallpaper : value->menu_wallpaper;
        int color = appearance_field_value(
            (enum crazypod_appearance_field)state->group);
        if(state->selected == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1)
            return path[0] != '\0'
                ? path_basename(path) : "Open /Pictures";
        return path[0] == '\0' && state->selected == color
            ? "Current selection" : "Select to apply";
    }
    if(state->route == DIY_ROUTE_WALLPAPER_FILES) {
        const char *current_path =
            state->group == CRAZYPOD_APPEARANCE_HOME_BACKGROUND
                ? value->home_wallpaper : value->menu_wallpaper;
        return strcmp(current_path,
                      diy_wallpaper_files[state->selected]) == 0
            ? "Current picture" : "Select to apply";
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
                 state->selected == 2 ? "Size, glow and highlights" :
                 state->selected == 3 ? "Home and menu pictures" :
                                        "Screen corner geometry";
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
    else if(state->route == DIY_ROUTE_CHOICES) {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)state->group;
        int value = appearance_choice_value(field, state->selected);
        if(field == CRAZYPOD_APPEARANCE_PRIMARY ||
           field == CRAZYPOD_APPEARANCE_SECONDARY)
            swatch_color = crazypod_appearance_color(value);
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
    else if(state->route == DIY_ROUTE_BACKGROUND_CHOICES) {
        if(state->selected > 0 &&
           state->selected <= CRAZYPOD_APPEARANCE_COLOR_COUNT)
            swatch_color =
                crazypod_appearance_color(state->selected - 1);
        symbol = state->selected ==
                 CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
            ? LV_SYMBOL_IMAGE : LV_SYMBOL_DIRECTORY;
    }
    else if(state->route == DIY_ROUTE_WALLPAPER_FILES) {
        symbol = LV_SYMBOL_IMAGE;
    }
    else if(state->route == DIY_ROUTE_LAYOUT) {
        symbol = LV_SYMBOL_SHUFFLE;
    }
    else if(state->route == DIY_ROUTE_DETAILS && state->selected == 4) {
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
        if(state->route == DIY_ROUTE_WALLPAPER_FILES)
            render_empty_state("No Pictures",
                               "Add JPG or BMP files to /Pictures.");
        else
            render_empty_state("Nothing Here",
                               "Add local music and rescan.");
        return;
    }

    if(count <= CRAZYPOD_VISIBLE_ROWS) {
        start = 0;
    }
    else {
        start = state->selected - CRAZYPOD_VISIBLE_ROWS / 2;
        if(start < 0)
            start = 0;
        if(start > count - CRAZYPOD_VISIBLE_ROWS)
            start = count - CRAZYPOD_VISIBLE_ROWS;
    }

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
                            route_item_is_current(state, index)
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

    if(count <= CRAZYPOD_VISIBLE_ROWS) {
        start = 0;
    }
    else {
        start = state->selected - CRAZYPOD_VISIBLE_ROWS / 2;
        if(start < 0)
            start = 0;
        if(start > count - CRAZYPOD_VISIBLE_ROWS)
            start = count - CRAZYPOD_VISIBLE_ROWS;
    }

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
            route_item_is_current(state, index)
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
    album_flow_displayed_album = state->selected;
    crazypod_coverflow_enter(state->selected);
}

static void render_now_playing(void)
{
    const struct crazypod_track *track = current_track();
    const lv_image_dsc_t *artwork = NULL;
    const lv_image_dsc_t *lyrics_artwork = NULL;
    const lv_image_dsc_t *presentation_backdrop = NULL;
    enum crazypod_artwork_state artwork_state =
        CRAZYPOD_ARTWORK_EMPTY;
    int artwork_slot = CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT;
    int presentation_bank;
    uint32_t content_color = COLOR_WHITE;
    lv_obj_t *backdrop;
    lv_obj_t *shade;
    lv_obj_t *title;
    lv_obj_t *artist;
    lv_obj_t *mode;

    rendered_track_path[0] = '\0';
    if(track != NULL) {
        snprintf(rendered_track_path, sizeof(rendered_track_path),
                 "%s", track->path);
        if(strcmp(now_prefetch_track_path, track->path) == 0)
            artwork_slot = CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT;
        artwork = crazypod_artwork_load_priority(
            artwork_slot, track,
            CRAZYPOD_NOW_ARTWORK_CACHE_SIZE, 0);
        artwork_state = crazypod_artwork_state(
            artwork_slot, track,
            CRAZYPOD_NOW_ARTWORK_CACHE_SIZE);
        if(artwork_slot == CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT &&
           artwork != NULL) {
            (void)crazypod_artwork_load_priority(
                CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT,
                track, CRAZYPOD_NOW_ARTWORK_CACHE_SIZE, 10);
        }
        presentation_bank = now_presentation_active_bank;
        if(artwork != NULL &&
           (presentation_bank < 0 ||
            strcmp(now_presentation_track_path[presentation_bank],
                   track->path) != 0)) {
            (void)prepare_now_presentation(artwork, track->path);
        }
        else if(artwork_state == CRAZYPOD_ARTWORK_EMPTY &&
                presentation_bank >= 0 &&
                strcmp(now_presentation_track_path[presentation_bank],
                       track->path) != 0) {
            now_presentation_active_bank = -1;
        }
    }

    presentation_bank = now_presentation_active_bank;
    if(presentation_bank >= 0 &&
       now_presentation_valid[presentation_bank]) {
        lyrics_artwork =
            &now_lyrics_cover_descriptor[presentation_bank];
        presentation_backdrop =
            &now_backdrop_descriptor[presentation_bank];
        content_color =
            now_presentation_text_color[presentation_bank];
    }
    if(presentation_backdrop != NULL) {
        backdrop = lv_image_create(product_content);
        lv_image_set_src(backdrop, presentation_backdrop);
        lv_obj_center(backdrop);
    }
    else {
        content_color = now_fallback_contrast_color(track);
        backdrop = make_box(
            product_content, 0, 0, 320, 240, 0,
            artwork_color(track != NULL ? track->album : "", 0),
            LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            backdrop,
            lv_color_hex(
                artwork_color(track != NULL ? track->artist : "", 1)),
            0);
        lv_obj_set_style_bg_grad_dir(
            backdrop, LV_GRAD_DIR_VER, 0);
    }
    shade = make_box(
        product_content, 0, 0, 320, 240, 0,
        CRAZYPOD_NOW_SHADE_COLOR, CRAZYPOD_NOW_SHADE_OPA);
    (void)shade;

    {
        const char *previous = "";
        const char *current = "";
        const char *next = "";
        bool lyrics_available = false;
        lv_obj_t *metadata_shade;

        if(now_lyrics_mode && track != NULL)
            lyrics_available = crazypod_lyrics_load(track->path);
        if(lyrics_available)
            crazypod_lyrics_window(0, &previous, &current, &next);
        create_artwork_descriptor(
            product_content, track, 18, 55,
            CRAZYPOD_NOW_LYRICS_COVER_SIZE,
            lyrics_artwork, false);

        make_box(product_content, 134, 55, 1, 106, 0,
                 content_color, 38);
        if(lyrics_available) {
            metadata_shade = make_box(
                product_content, 18, 116, 108, 47, 0,
                0x000000, 112);
            (void)metadata_shade;
            title = make_label(
                product_content,
                track != NULL ? track->title : "No Track",
                CRAZYPOD_METADATA_FONT,
                COLOR_WHITE, LV_OPA_COVER);
            lv_obj_set_size(title, 96, 17);
            lv_obj_set_style_text_align(
                title, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                title, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(title, 24, 120);
            artist = make_label(
                product_content,
                track != NULL ? track->artist : "Local Music",
                CRAZYPOD_METADATA_FONT,
                COLOR_WHITE, 180);
            lv_obj_set_size(artist, 96, 17);
            lv_obj_set_style_text_align(
                artist, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                artist, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(artist, 24, 140);

            now_lyrics_previous = make_label(
                product_content, previous,
                CRAZYPOD_METADATA_FONT,
                content_color, 120);
            lv_obj_set_pos(now_lyrics_previous, 144, 71);
            lv_obj_set_size(now_lyrics_previous, 158, 18);
            lv_label_set_long_mode(
                now_lyrics_previous, LV_LABEL_LONG_MODE_DOTS);
            now_lyrics_current = make_label(
                product_content, current,
                CRAZYPOD_METADATA_FONT,
                content_color, LV_OPA_COVER);
            lv_obj_set_pos(now_lyrics_current, 144, 100);
            lv_obj_set_size(now_lyrics_current, 158, 18);
            lv_label_set_long_mode(
                now_lyrics_current, LV_LABEL_LONG_MODE_DOTS);
            now_lyrics_next = make_label(
                product_content, next,
                CRAZYPOD_METADATA_FONT,
                content_color, 150);
            lv_obj_set_pos(now_lyrics_next, 144, 129);
            lv_obj_set_size(now_lyrics_next, 158, 18);
            lv_label_set_long_mode(
                now_lyrics_next, LV_LABEL_LONG_MODE_DOTS);
        }
        else {
            lv_obj_t *album;

            title = make_label(
                product_content,
                track != NULL ? track->title : "No Track",
                CRAZYPOD_METADATA_FONT,
                content_color, LV_OPA_COVER);
            lv_obj_set_size(title, 158, 18);
            lv_obj_set_style_text_align(
                title, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                title, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(title, 144, 71);

            artist = make_label(
                product_content,
                track != NULL ? track->artist : "Local Music",
                CRAZYPOD_METADATA_FONT,
                content_color, 220);
            lv_obj_set_size(artist, 158, 18);
            lv_obj_set_style_text_align(
                artist, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                artist, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(artist, 144, 95);

            album = make_label(
                product_content,
                track != NULL && track->album[0] != '\0'
                    ? track->album : "",
                &lv_font_montserrat_10,
                content_color, 190);
            lv_obj_set_size(album, 158, 16);
            lv_obj_set_style_text_align(
                album, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                album, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(album, 144, 119);

            mode = make_label(
                product_content,
                crazypod_queue_repeat() == REPEAT_ONE ? "1" :
                crazypod_queue_repeat() == REPEAT_ALL ? LV_SYMBOL_LOOP :
                crazypod_queue_shuffle() ? LV_SYMBOL_SHUFFLE :
                LV_SYMBOL_PLAY,
                &lv_font_montserrat_12,
                crazypod_queue_repeat() != REPEAT_OFF ||
                crazypod_queue_shuffle() ? COLOR_CYAN : content_color,
                220);
            lv_obj_set_width(mode, 24);
            lv_obj_set_style_text_align(
                mode, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(mode, 211, 143);
        }
    }

    now_wave_surface = lv_obj_create(product_content);
    set_plain_object(now_wave_surface);
    lv_obj_set_pos(now_wave_surface, 16, 183);
    lv_obj_set_size(now_wave_surface, 288, 27);
    lv_obj_set_style_bg_opa(now_wave_surface, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(now_wave_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(now_wave_surface, draw_now_wave_event,
                        LV_EVENT_DRAW_MAIN, NULL);
    now_wave_playing_seen =
        (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
        (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    last_now_wave_tick = current_tick;
    make_box(product_content, 18, 212, 284, 2,
             LV_RADIUS_CIRCLE, COLOR_WHITE, 60);
    now_progress_fill = make_box(
        product_content, 18, 212, 4, 2,
        LV_RADIUS_CIRCLE, COLOR_WHITE, 185);
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

static void clear_now_overlay_objects(void)
{
    now_overlay_root = NULL;
    now_overlay_panel = NULL;
    memset(&now_queue_view, 0, sizeof(now_queue_view));
    memset(&now_actions_view, 0, sizeof(now_actions_view));
    memset(&now_volume_view, 0, sizeof(now_volume_view));
}

static void now_popup_y_anim(void *target, int32_t value)
{
    lv_obj_set_y(target, value);
}

static void now_popup_opa_anim(void *target, int32_t value)
{
    lv_obj_set_style_opa(target, (lv_opa_t)value, 0);
}

static void animate_now_popup(lv_obj_t *panel, int target_y)
{
    lv_anim_t animation;

    lv_obj_set_y(panel, target_y + 9);
    lv_obj_set_style_opa(panel, 120, 0);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, panel);
    lv_anim_set_exec_cb(&animation, now_popup_y_anim);
    lv_anim_set_values(&animation, target_y + 9, target_y);
    lv_anim_set_duration(&animation, 180);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, panel);
    lv_anim_set_exec_cb(&animation, now_popup_opa_anim);
    lv_anim_set_values(&animation, 120, LV_OPA_COVER);
    lv_anim_set_duration(&animation, 160);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static void begin_now_overlay(enum now_playing_overlay overlay)
{
    if(now_overlay_root != NULL)
        lv_obj_delete(now_overlay_root);
    clear_now_overlay_objects();
    now_overlay = overlay;
    now_overlay_root = make_box(product_content, 0, 0,
                                LCD_WIDTH, LCD_HEIGHT, 0,
                                0x000000, 18);
    lv_obj_remove_flag(now_overlay_root, LV_OBJ_FLAG_CLICKABLE);
}

static int now_volume_percent(void)
{
    int minimum = sound_min(SOUND_VOLUME);
    int maximum = sound_max(SOUND_VOLUME);
    int volume = global_status.volume;

    if(volume < minimum)
        volume = minimum;
    if(volume > maximum)
        volume = maximum;
    if(maximum <= minimum)
        return 0;
    return (volume - minimum) * 100 / (maximum - minimum);
}

static const char *now_playback_mode_label(void)
{
    if(crazypod_queue_repeat() == REPEAT_ONE)
        return "REPEAT 1";
    if(crazypod_queue_repeat() == REPEAT_ALL)
        return "REPEAT";
    if(crazypod_queue_shuffle())
        return "SHUFFLE";
    return "ORDERED";
}

static const char *now_playback_mode_icon(void)
{
    if(crazypod_queue_repeat() == REPEAT_ONE)
        return "1";
    if(crazypod_queue_repeat() == REPEAT_ALL)
        return LV_SYMBOL_LOOP;
    if(crazypod_queue_shuffle())
        return LV_SYMBOL_SHUFFLE;
    return LV_SYMBOL_PLAY;
}

static void refresh_now_actions_popup(void)
{
    static const int action_for_cell[3] = {
        NOW_ACTION_PLAYBACK, NOW_ACTION_LYRICS, NOW_ACTION_VOLUME
    };
    char detail[64];
    int i;
    bool queue_selected = now_action_selected == NOW_ACTION_QUEUE;

    if(now_actions_view.queue_row == NULL)
        return;

    lv_obj_set_style_bg_color(
        now_actions_view.queue_row,
        lv_color_hex(queue_selected ? 0xDBD1BD : COLOR_WHITE), 0);
    lv_obj_set_style_bg_opa(
        now_actions_view.queue_row, queue_selected ? 31 : 7, 0);
    lv_obj_set_style_border_width(now_actions_view.queue_row, 1, 0);
    lv_obj_set_style_border_color(
        now_actions_view.queue_row,
        lv_color_hex(queue_selected ? 0xDBD1BD : COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(
        now_actions_view.queue_row, queue_selected ? 148 : 23, 0);
    lv_obj_set_style_bg_color(
        now_actions_view.queue_icon,
        lv_color_hex(queue_selected ? 0xDBD1BD : COLOR_WHITE), 0);
    lv_obj_set_style_bg_opa(
        now_actions_view.queue_icon, queue_selected ? 224 : 20, 0);
    lv_obj_set_style_text_color(
        now_actions_view.queue_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_opa(
        now_actions_view.queue_label, queue_selected ? 250 : 215, 0);

    for(i = 0; i < 3; ++i) {
        bool selected = now_action_selected == action_for_cell[i];
        lv_obj_set_style_bg_color(
            now_actions_view.cells[i],
            lv_color_hex(selected ? 0xDBD1BD : COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(
            now_actions_view.cells[i], selected ? 38 : 10, 0);
        lv_obj_set_style_border_width(now_actions_view.cells[i], 1, 0);
        lv_obj_set_style_border_color(
            now_actions_view.cells[i],
            lv_color_hex(selected ? 0xDBD1BD : COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(
            now_actions_view.cells[i], selected ? 163 : 31, 0);
        lv_obj_set_style_text_opa(
            now_actions_view.cell_icons[i], selected ? 245 : 124, 0);
        lv_obj_set_style_text_opa(
            now_actions_view.cell_labels[i], selected ? 230 : 120, 0);
    }

    lv_label_set_text(now_actions_view.cell_icons[0],
                      now_playback_mode_icon());
    lv_label_set_text(now_actions_view.cell_icons[1],
                      LV_SYMBOL_FILE);
    lv_label_set_text(now_actions_view.cell_icons[2],
                      now_volume_percent() <= 0
                          ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
    if(now_action_selected == NOW_ACTION_QUEUE) {
        snprintf(detail, sizeof(detail),
                 "Scroll browse  Center play  Menu exits");
    }
    else if(now_action_selected == NOW_ACTION_PLAYBACK) {
        snprintf(detail, sizeof(detail), "Center cycles mode  %s",
                 now_playback_mode_label());
    }
    else if(now_action_selected == NOW_ACTION_LYRICS) {
        const struct crazypod_track *track = current_track();
        bool available = track != NULL &&
                         crazypod_lyrics_load(track->path);
        snprintf(detail, sizeof(detail), "%s  %s",
                 now_lyrics_mode ? "Lyrics visible" : "Lyrics hidden",
                 available ? "Local LRC" : "No local LRC");
    }
    else {
        snprintf(detail, sizeof(detail), "Volume  %d%%",
                 now_volume_percent());
    }
    lv_label_set_text(now_actions_view.detail, detail);
}

static void show_now_actions_popup(void)
{
    lv_obj_t *title;
    lv_obj_t *chevron;
    int i;

    if(now_overlay == NOW_OVERLAY_NONE)
        prepare_now_overlay_glass();
    begin_now_overlay(NOW_OVERLAY_ACTIONS);
    if(now_action_selected < 0 ||
       now_action_selected >= NOW_ACTION_COUNT)
        now_action_selected = NOW_ACTION_QUEUE;

    now_overlay_panel = make_now_glass_panel(35, 43, 250, 154);
    title = make_label(now_overlay_panel, "ACTIONS",
                       &lv_font_montserrat_10,
                       COLOR_WHITE, 92);
    lv_obj_set_width(title, 250);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 12);

    now_actions_view.queue_row = make_box(
        now_overlay_panel, 12, 31, 226, 36, 10,
        COLOR_WHITE, LV_OPA_TRANSP);
    now_actions_view.queue_icon = make_box(
        now_actions_view.queue_row, 10, 7, 23, 23,
        LV_RADIUS_CIRCLE, COLOR_WHITE, 20);
    title = make_label(now_actions_view.queue_icon, LV_SYMBOL_LIST,
                       &lv_font_montserrat_10,
                       COLOR_DETAIL, 220);
    lv_obj_center(title);
    now_actions_view.queue_label = make_label(
        now_actions_view.queue_row, "View Playback Queue",
        &lv_font_montserrat_10, COLOR_WHITE, 215);
    lv_obj_set_pos(now_actions_view.queue_label, 42, 11);
    chevron = make_label(now_actions_view.queue_row, LV_SYMBOL_RIGHT,
                         &lv_font_montserrat_8,
                         COLOR_WHITE, 110);
    lv_obj_set_pos(chevron, 207, 14);

    for(i = 0; i < 3; ++i) {
        static const int x_positions[3] = { 42, 99, 156 };
        static const char *const labels[3] = {
            "Playback", "Lyrics", "Volume"
        };
        static const char *const icons[3] = {
            LV_SYMBOL_PLAY, LV_SYMBOL_FILE, LV_SYMBOL_VOLUME_MAX
        };
        int x = x_positions[i];
        now_actions_view.cells[i] = make_box(
            now_overlay_panel, x, 75, 52, 52, 14,
            COLOR_WHITE, 10);
        now_actions_view.cell_icons[i] = make_label(
            now_actions_view.cells[i], icons[i],
            &lv_font_montserrat_12,
            COLOR_WHITE, 124);
        lv_obj_set_width(now_actions_view.cell_icons[i], 52);
        lv_obj_set_style_text_align(
            now_actions_view.cell_icons[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(now_actions_view.cell_icons[i], 0, 8);
        now_actions_view.cell_labels[i] = make_label(
            now_actions_view.cells[i], labels[i],
            &lv_font_montserrat_8,
            COLOR_WHITE, 120);
        lv_obj_set_width(now_actions_view.cell_labels[i], 52);
        lv_obj_set_style_text_align(
            now_actions_view.cell_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(now_actions_view.cell_labels[i], 0, 32);
    }

    now_actions_view.detail = make_label(
        now_overlay_panel, "",
        &lv_font_montserrat_8,
        COLOR_WHITE, 174);
    lv_obj_set_width(now_actions_view.detail, 222);
    lv_obj_set_style_text_align(
        now_actions_view.detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(now_actions_view.detail, 14, 134);
    refresh_now_actions_popup();
    animate_now_popup(now_overlay_panel, 43);
}

static void refresh_now_queue_popup(void)
{
    int count = crazypod_queue_count();
    int current = crazypod_queue_index();
    int start;
    char text[32];
    char mode[32];
    int row;

    if(now_queue_view.count == NULL)
        return;
    if(count <= 0)
        now_queue_selected = 0;
    else {
        if(now_queue_selected < 0)
            now_queue_selected = 0;
        if(now_queue_selected >= count)
            now_queue_selected = count - 1;
    }

    snprintf(mode, sizeof(mode), "%s  %s",
             now_playback_mode_icon(), now_playback_mode_label());
    lv_label_set_text(now_queue_view.mode, mode);
    snprintf(text, sizeof(text), count > 0 ? "%d/%d" : "0/0",
             count > 0 ? now_queue_selected + 1 : 0, count);
    lv_label_set_text(now_queue_view.count, text);
    if(count > 0)
        lv_obj_add_flag(now_queue_view.empty, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(now_queue_view.empty, LV_OBJ_FLAG_HIDDEN);

    start = count <= 3 ? 0 : now_queue_selected - 1;
    if(start < 0)
        start = 0;
    if(start > count - 3)
        start = count - 3;
    for(row = 0; row < 3; ++row) {
        int index = start + row;
        bool selected = index == now_queue_selected;
        bool current_row = index == current;
        const char *path;
        const struct crazypod_track *track;

        if(index < 0 || index >= count) {
            lv_obj_add_flag(now_queue_view.rows[row],
                            LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(now_queue_view.rows[row],
                           LV_OBJ_FLAG_HIDDEN);
        path = crazypod_queue_path(index);
        track = crazypod_music_track(crazypod_music_find_track(path));
        lv_label_set_text(now_queue_view.titles[row],
                          track != NULL ? track->title : "Unavailable");
        lv_label_set_text(now_queue_view.artists[row],
                          track != NULL ? track->artist : "Local Music");
        lv_label_set_text(
            now_queue_view.icons[row],
            current_row
                ? ((audio_status() & AUDIO_STATUS_PAUSE)
                       ? LV_SYMBOL_PAUSE : LV_SYMBOL_VOLUME_MAX)
                : LV_SYMBOL_BULLET);
        lv_obj_set_style_text_color(
            now_queue_view.icons[row],
            lv_color_hex(current_row ? COLOR_CYAN : COLOR_WHITE), 0);
        lv_obj_set_style_text_opa(
            now_queue_view.icons[row], current_row ? 245 : 75, 0);
        lv_obj_set_style_bg_color(
            now_queue_view.rows[row], lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(
            now_queue_view.rows[row], selected ? 41 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_opa(
            now_queue_view.titles[row], selected ? 255 : 220, 0);
        lv_obj_set_style_text_opa(
            now_queue_view.artists[row], selected ? 190 : 140, 0);
    }
    now_queue_generation_seen = crazypod_queue_generation();
}

static void show_now_queue_popup(void)
{
    lv_obj_t *source_icon;
    lv_obj_t *source_title;
    int row;

    if(now_overlay == NOW_OVERLAY_NONE)
        prepare_now_overlay_glass();
    begin_now_overlay(NOW_OVERLAY_QUEUE);
    if(crazypod_queue_count() > 0 &&
       (now_queue_selected < 0 ||
        now_queue_selected >= crazypod_queue_count()))
        now_queue_selected = crazypod_queue_index();
    if(crazypod_queue_count() > 0)
        prefetch_now_queue_artwork(now_queue_selected);

    now_overlay_panel = make_now_glass_panel(
        CRAZYPOD_NOW_POPUP_X, CRAZYPOD_NOW_POPUP_Y,
        CRAZYPOD_NOW_POPUP_WIDTH, CRAZYPOD_NOW_POPUP_HEIGHT);
    now_queue_view.mode = make_label(
        now_overlay_panel, "",
        &lv_font_montserrat_10,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(now_queue_view.mode, 14, 12);
    now_queue_view.count = make_label(
        now_overlay_panel, "0/0",
        &lv_font_montserrat_8,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(now_queue_view.count, 54);
    lv_obj_set_style_text_align(
        now_queue_view.count, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(now_queue_view.count, 182, 14);

    source_icon = make_label(now_overlay_panel, LV_SYMBOL_AUDIO,
                             &lv_font_montserrat_10,
                             COLOR_WHITE, 225);
    lv_obj_set_pos(source_icon, 14, 35);
    source_title = make_label(now_overlay_panel, "Local Queue",
                              &lv_font_montserrat_8,
                              COLOR_WHITE, 220);
    lv_obj_set_pos(source_title, 34, 36);
    now_queue_view.empty = make_label(
        now_overlay_panel, "No Queue",
        CRAZYPOD_METADATA_FONT,
        COLOR_WHITE, 225);
    lv_obj_set_width(now_queue_view.empty, 222);
    lv_obj_set_style_text_align(
        now_queue_view.empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(now_queue_view.empty, 14, 101);

    for(row = 0; row < 3; ++row) {
        int y = 58 + row * 36;
        now_queue_view.rows[row] = make_box(
            now_overlay_panel, 14, y, 222, 32, 6,
            COLOR_WHITE, LV_OPA_TRANSP);
        now_queue_view.icons[row] = make_label(
            now_queue_view.rows[row], LV_SYMBOL_BULLET,
            &lv_font_montserrat_10,
            COLOR_WHITE, 75);
        lv_obj_set_width(now_queue_view.icons[row], 18);
        lv_obj_set_style_text_align(
            now_queue_view.icons[row], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(now_queue_view.icons[row], 7, 11);
        now_queue_view.titles[row] = make_label(
            now_queue_view.rows[row], "",
            CRAZYPOD_METADATA_FONT,
            COLOR_WHITE, 220);
        lv_obj_set_pos(now_queue_view.titles[row], 32, 1);
        lv_obj_set_size(now_queue_view.titles[row], 181, 16);
        lv_label_set_long_mode(
            now_queue_view.titles[row], LV_LABEL_LONG_MODE_DOTS);
        now_queue_view.artists[row] = make_label(
            now_queue_view.rows[row], "",
            CRAZYPOD_METADATA_FONT,
            COLOR_WHITE, 140);
        lv_obj_set_pos(now_queue_view.artists[row], 32, 16);
        lv_obj_set_size(now_queue_view.artists[row], 181, 15);
        lv_label_set_long_mode(
            now_queue_view.artists[row], LV_LABEL_LONG_MODE_DOTS);
    }
    refresh_now_queue_popup();
    animate_now_popup(now_overlay_panel, CRAZYPOD_NOW_POPUP_Y);
}

static void refresh_now_volume_popup(void)
{
    int percent = now_volume_percent();
    char text[16];

    if(now_volume_view.fill == NULL)
        return;
    lv_obj_set_width(now_volume_view.fill,
                     percent > 0 ? 2 + 176 * percent / 100 : 2);
    snprintf(text, sizeof(text), "%d%%", percent);
    lv_label_set_text(now_volume_view.percent, text);
    lv_label_set_text(now_volume_view.icon,
                      percent <= 0 ? LV_SYMBOL_MUTE
                                   : LV_SYMBOL_VOLUME_MAX);
}

static void show_now_volume_popup(void)
{
    lv_obj_t *title;
    lv_obj_t *track;

    if(now_overlay == NOW_OVERLAY_NONE)
        prepare_now_overlay_glass();
    begin_now_overlay(NOW_OVERLAY_VOLUME);
    now_overlay_panel = make_now_glass_panel(35, 65, 250, 110);
    title = make_label(now_overlay_panel, "VOLUME",
                       &lv_font_montserrat_10,
                       COLOR_WHITE, 100);
    lv_obj_set_width(title, 250);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 14);
    now_volume_view.icon = make_label(
        now_overlay_panel, LV_SYMBOL_VOLUME_MAX,
        &lv_font_montserrat_16,
        COLOR_WHITE, 235);
    lv_obj_set_pos(now_volume_view.icon, 23, 47);
    track = make_box(now_overlay_panel, 53, 51, 180, 8,
                     LV_RADIUS_CIRCLE, COLOR_WHITE, 26);
    now_volume_view.fill = make_box(
        track, 0, 0, 2, 8, LV_RADIUS_CIRCLE,
        COLOR_CYAN, LV_OPA_COVER);
    now_volume_view.percent = make_label(
        now_overlay_panel, "0%",
        &lv_font_montserrat_10,
        COLOR_WHITE, 220);
    lv_obj_set_width(now_volume_view.percent, 180);
    lv_obj_set_style_text_align(
        now_volume_view.percent, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(now_volume_view.percent, 53, 72);
    refresh_now_volume_popup();
    animate_now_popup(now_overlay_panel, 65);
}

static void restore_now_overlay(enum now_playing_overlay overlay)
{
    if(overlay == NOW_OVERLAY_ACTIONS)
        show_now_actions_popup();
    else if(overlay == NOW_OVERLAY_QUEUE)
        show_now_queue_popup();
    else if(overlay == NOW_OVERLAY_VOLUME)
        show_now_volume_popup();
}

static void dismiss_now_overlay(bool refresh_now_playing)
{
    if(now_overlay_root != NULL) {
        lv_anim_delete(now_overlay_panel, NULL);
        lv_obj_delete(now_overlay_root);
    }
    now_overlay = NOW_OVERLAY_NONE;
    clear_now_overlay_objects();
    if(refresh_now_playing && product_active && route_depth > 0 &&
       current_route()->route == MUSIC_ROUTE_NOW_PLAYING)
        render_current_route(false);
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
    const lv_image_dsc_t *menu_wallpaper;
    int i;

    if(crazypod_coverflow_active())
        crazypod_coverflow_leave();
    now_overlay = NOW_OVERLAY_NONE;
    clear_now_overlay_objects();
    memset(&menu_view, 0, sizeof(menu_view));
    route_render_pending = false;
    now_progress_fill = NULL;
    now_elapsed = NULL;
    now_remaining = NULL;
    now_wave_surface = NULL;
    now_lyrics_previous = NULL;
    now_lyrics_current = NULL;
    now_lyrics_next = NULL;
    music_loading_title = NULL;
    music_loading_detail = NULL;
    album_flow_title = NULL;
    album_flow_artist = NULL;
    album_flow_position = NULL;
    album_flow_displayed_album = -1;
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
    menu_wallpaper = crazypod_custom_menu_wallpaper();
    if(menu_wallpaper != NULL) {
        lv_obj_t *image = lv_image_create(product_content);
        lv_image_set_src(image, menu_wallpaper);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }

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
    refresh_screen_corner_masks();
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
        music_artwork_cache_failed
            ? "Artwork Cache Failed"
            : music_scan_start_failed
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
                 music_artwork_cache_failed
                     ? "Could not write the CoverFlow cache"
                 : music_scan_start_failed
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
    if(music_scan_pending ||
       crazypod_music_is_scanning() ||
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
        music_library_loaded = false;
        music_scan_screen = true;
        music_artwork_preparing = true;
        music_artwork_cache_failed = false;
        crazypod_artwork_prime_library();
        render_loading();
        lv_refr_now(NULL);
        return;
    }

    music_library_loaded = false;
    music_scan_screen = true;
    music_scan_start_failed = false;
    music_artwork_cache_failed = false;
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
    music_artwork_cache_failed = false;
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

static void activate_now_overlay(void)
{
    if(now_overlay == NOW_OVERLAY_ACTIONS) {
        if(now_action_selected == NOW_ACTION_QUEUE) {
            now_queue_selected = crazypod_queue_count() > 0
                ? crazypod_queue_index() : 0;
            show_now_queue_popup();
        }
        else if(now_action_selected == NOW_ACTION_PLAYBACK) {
            cycle_playback_mode();
            refresh_now_actions_popup();
        }
        else if(now_action_selected == NOW_ACTION_LYRICS) {
            now_lyrics_mode = !now_lyrics_mode;
            render_current_route(false);
            show_now_actions_popup();
        }
        else {
            show_now_volume_popup();
        }
        return;
    }
    if(now_overlay == NOW_OVERLAY_QUEUE) {
        if(now_queue_selected >= 0 &&
           now_queue_selected < crazypod_queue_count()) {
            playlist_start(now_queue_selected, 0, 0);
            crazypod_state_forget_resume();
            crazypod_state_mark_dirty();
            render_current_route(false);
            show_now_queue_popup();
        }
        return;
    }
    if(now_overlay == NOW_OVERLAY_VOLUME)
        dismiss_now_overlay(true);
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

    if(state->route == MUSIC_ROUTE_NOW_PLAYING &&
       now_overlay != NOW_OVERLAY_NONE) {
        activate_now_overlay();
        return;
    }
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
        now_action_selected = NOW_ACTION_QUEUE;
        show_now_actions_popup();
        break;
    case DIY_ROUTE_MENU:
        if(state->selected == 0)
            push_route(DIY_ROUTE_PRESETS, -1);
        else if(state->selected == 1)
            push_route(DIY_ROUTE_ICONS, -1);
        else if(state->selected == 2)
            push_route(DIY_ROUTE_DETAILS, -1);
        else if(state->selected == 3)
            push_route(DIY_ROUTE_BACKGROUNDS, -1);
        else
            push_route(DIY_ROUTE_LAYOUT, -1);
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
            crazypod_wallpaper_reload_custom();
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
        enum crazypod_appearance_field field =
            diy_detail_fields[state->selected];
        push_route_selected(DIY_ROUTE_CHOICES, field,
                            appearance_choice_index(field));
        break;
    }
    case DIY_ROUTE_CHOICES: {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)state->group;
        int value = appearance_choice_value(field, state->selected);
        crazypod_appearance_set_value(field, value);
        refresh_desktop_appearance();
        render_current_route(false);
        break;
    }
    case DIY_ROUTE_BACKGROUNDS: {
        enum crazypod_appearance_field field = state->selected == 0
            ? CRAZYPOD_APPEARANCE_HOME_BACKGROUND
            : CRAZYPOD_APPEARANCE_MENU_BACKGROUND;
        const struct crazypod_appearance *appearance =
            crazypod_appearance_get();
        const char *path = state->selected == 0
            ? appearance->home_wallpaper : appearance->menu_wallpaper;
        int selected = path[0] != '\0'
            ? CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
            : appearance_field_value(field);
        push_route_selected(DIY_ROUTE_BACKGROUND_CHOICES,
                            field, selected);
        break;
    }
    case DIY_ROUTE_BACKGROUND_CHOICES: {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)state->group;
        bool menu = field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND;
        if(state->selected ==
           CRAZYPOD_APPEARANCE_COLOR_COUNT + 1) {
            scan_wallpaper_files();
            push_route(DIY_ROUTE_WALLPAPER_FILES, field);
        }
        else {
            crazypod_wallpaper_clear(menu);
            crazypod_appearance_set_value(field, state->selected);
            refresh_desktop_appearance();
            render_current_route(false);
        }
        break;
    }
    case DIY_ROUTE_WALLPAPER_FILES: {
        bool menu =
            state->group == CRAZYPOD_APPEARANCE_MENU_BACKGROUND;
        if(state->selected >= 0 &&
           state->selected < diy_wallpaper_file_count &&
           crazypod_wallpaper_select(
               menu, diy_wallpaper_files[state->selected])) {
            refresh_desktop_appearance();
            pop_route();
        }
        else
            render_current_route(false);
        break;
    }
    case DIY_ROUTE_LAYOUT: {
        enum crazypod_appearance_field field =
            diy_layout_fields[state->selected];
        push_route_selected(DIY_ROUTE_CHOICES, field,
                            appearance_choice_index(field));
        break;
    }
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
        if(now_overlay == NOW_OVERLAY_ACTIONS) {
            int next = now_action_selected + direction;
            if(next < 0)
                next = 0;
            if(next >= NOW_ACTION_COUNT)
                next = NOW_ACTION_COUNT - 1;
            if(next != now_action_selected) {
                now_action_selected = next;
                refresh_now_actions_popup();
            }
        }
        else if(now_overlay == NOW_OVERLAY_QUEUE) {
            int next = now_queue_selected + direction;
            int queue_count = crazypod_queue_count();
            if(queue_count <= 0)
                return;
            if(next < 0)
                next = 0;
            if(next >= queue_count)
                next = queue_count - 1;
            if(next != now_queue_selected) {
                now_queue_selected = next;
                prefetch_now_queue_artwork(now_queue_selected);
                refresh_now_queue_popup();
            }
        }
        else if(now_overlay == NOW_OVERLAY_VOLUME) {
            int next_volume = global_status.volume + direction * 2;
            if(next_volume < sound_min(SOUND_VOLUME))
                next_volume = sound_min(SOUND_VOLUME);
            if(next_volume > sound_max(SOUND_VOLUME))
                next_volume = sound_max(SOUND_VOLUME);
            sound_set_volume(next_volume);
            global_status.volume = next_volume;
            crazypod_state_mark_dirty();
            refresh_now_volume_popup();
        }
        return;
    }
    if(count <= 0)
        return;
    keep_cpu_boosted(HZ / 3);
    if(state->route == MUSIC_ROUTE_ALBUM_FLOW) {
        int next = crazypod_coverflow_step(direction);
        if(next == state->selected)
            return;
        state->selected = next;
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
    if(now_overlay == NOW_OVERLAY_QUEUE)
        refresh_now_queue_popup();
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
    if(now_overlay == NOW_OVERLAY_ACTIONS)
        refresh_now_actions_popup();
    else if(now_overlay == NOW_OVERLAY_QUEUE)
        refresh_now_queue_popup();
    else if(route_depth > 0 &&
            current_route()->route == MUSIC_ROUTE_NOW_PLAYING)
        render_current_route(false);
}

static void set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    if(label == NULL || text == NULL)
        return;
    if(strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
}

static void refresh_now_playing_lyrics(uint32_t elapsed_ms)
{
    const char *previous;
    const char *current;
    const char *next;

    if(!now_lyrics_mode || now_lyrics_current == NULL ||
       !crazypod_lyrics_available())
        return;
    crazypod_lyrics_window(elapsed_ms, &previous, &current, &next);
    set_label_text_if_changed(now_lyrics_previous, previous);
    set_label_text_if_changed(now_lyrics_current, current);
    set_label_text_if_changed(now_lyrics_next, next);
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
            music_artwork_cache_failed = false;
            crazypod_artwork_prime_library();
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
        if(crazypod_artwork_library_prime_failed()) {
            music_artwork_preparing = false;
            music_artwork_cache_failed = true;
            if(music_scan_screen)
                render_loading();
            return;
        }
        if(!crazypod_artwork_library_priming()) {
            if(!crazypod_coverflow_warm(initial_album_index()))
                return;
            music_artwork_preparing = false;
            music_library_loaded =
                crazypod_music_track_count() > 0;
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
        enum now_playing_overlay overlay = now_overlay;
        render_current_route(false);
        restore_now_overlay(overlay);
        return;
    }
    if(track == NULL && rendered_track_path[0] != '\0') {
        enum now_playing_overlay overlay = now_overlay;
        render_current_route(false);
        restore_now_overlay(overlay);
        return;
    }

    if(now_overlay == NOW_OVERLAY_QUEUE &&
       now_queue_generation_seen != crazypod_queue_generation())
        refresh_now_queue_popup();
    if(id3 != NULL)
        refresh_now_playing_lyrics((uint32_t)id3->elapsed);

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
        lv_obj_set_width(now_progress_fill, width);
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
    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING &&
       now_overlay != NOW_OVERLAY_NONE)
        return;

    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING) {
        unsigned prefetch_generation =
            crazypod_artwork_slot_generation(
                CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT);

        generation = crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);
        if(generation == now_artwork_generation_seen &&
           prefetch_generation ==
               now_prefetch_artwork_generation_seen)
            return;
        now_artwork_generation_seen = generation;
        now_prefetch_artwork_generation_seen =
            prefetch_generation;
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

    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING &&
       now_overlay != NOW_OVERLAY_NONE &&
       base == BUTTON_MENU && !repeated) {
        dismiss_now_overlay(true);
        return;
    }

    if(base == BUTTON_SCROLL_FWD)
        move_selection(wheel_step(
            data,
            current_route()->route == MUSIC_ROUTE_NOW_PLAYING ? 1 :
            current_route()->route == MUSIC_ROUTE_ALBUM_FLOW ? 8 : 12));
    else if(base == BUTTON_SCROLL_BACK)
        move_selection(-wheel_step(
            data,
            current_route()->route == MUSIC_ROUTE_NOW_PLAYING ? 1 :
            current_route()->route == MUSIC_ROUTE_ALBUM_FLOW ? 8 : 12));
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
        if(!repeated)
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
        if(crazypod_coverflow_active()) {
            /*
             * CoverFlow presents the complete screen after its native
             * compositor runs. Holding LVGL's partial update here prevents
             * metadata and covers from reaching the panel as separate frames.
             */
            crazypod_coverflow_invalidate();
        }
        else {
            lcd_update_rect(dirty_x1, dirty_y1,
                            dirty_x2 - dirty_x1 + 1,
                            dirty_y2 - dirty_y1 + 1);
        }
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

static void sync_album_flow_metadata(void)
{
    const struct route_state *state;
    const struct crazypod_album *album;
    char position[32];
    int album_index;
    int count;

    if(!product_active || route_depth <= 0 ||
       !crazypod_coverflow_active() ||
       album_flow_title == NULL || album_flow_artist == NULL ||
       album_flow_position == NULL)
        return;
    state = current_route();
    if(state->route != MUSIC_ROUTE_ALBUM_FLOW)
        return;
    album_index = crazypod_coverflow_center_album();
    if(album_index == album_flow_displayed_album)
        return;
    count = crazypod_music_album_count();
    album = crazypod_music_album(album_index);
    lv_label_set_text(album_flow_title,
                      album != NULL ? album->title : "");
    lv_label_set_text(album_flow_artist,
                      album != NULL ? album->artist : "");
    snprintf(position, sizeof(position), "%d / %d",
             album_index + 1, count);
    lv_label_set_text(album_flow_position, position);
    album_flow_displayed_album = album_index;
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
    if(crazypod_wallpaper_prepare_frosted_capsule()) {
        refresh_desktop_capsule_material();
        lv_refr_now(display);
    }
    preview_artwork_generation_seen =
        crazypod_artwork_slot_generation(CRAZYPOD_PREVIEW_ARTWORK_SLOT);
    now_artwork_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);
    now_prefetch_artwork_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT);
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
        tick_now_playing_wave();
        lv_timer_handler();
        render_desktop_carousel_native();
        crazypod_coverflow_tick();
        sync_album_flow_metadata();
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
