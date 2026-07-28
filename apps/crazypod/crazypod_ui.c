#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "audio.h"
#include "backlight.h"
#include "button.h"
#include "dir.h"
#include "events.h"
#include "file.h"
#include "kernel.h"
#include "lcd.h"
#include "misc.h"
#if defined(HAVE_HARDWARE_CLICK) && !defined(SIMULATOR)
#include "piezo.h"
#endif
#include "powermgmt.h"
#include "playlist.h"
#include "settings.h"
#include "sound.h"
#include "system.h"
#include "timefuncs.h"
#include "usb.h"
#ifdef SIMULATOR
#include "screendump.h"
#endif

#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "crazypod_audio_shims.h"
#include "crazypod_apps.h"
#include "crazypod_artwork.h"
#include "crazypod_appearance.h"
#include "crazypod_book_cover.h"
#include "crazypod_books.h"
#include "crazypod_coverflow.h"
#include "crazypod_frameclock.h"
#include "crazypod_image.h"
#include "crazypod_icons.h"
#include "crazypod_lyrics.h"
#include "crazypod_lcd.h"
#include "crazypod_music.h"
#include "crazypod_miniapps.h"
#include "crazypod_miniapp_input.h"
#include "crazypod_miniapp_font.h"
#include "crazypod_notes.h"
#include "crazypod_organizer.h"
#include "crazypod_playlist.h"
#include "crazypod_photos.h"
#include "crazypod_presets.h"
#include "crazypod_soundwave.h"
#include "crazypod_state.h"
#include "crazypod_ui.h"
#include "crazypod_videos.h"
#include "crazypod_wallpaper.h"
#include "crazypod_workouts.h"

#define CRAZYPOD_STATUS_BAR_COUNT 2
#define CRAZYPOD_CORNER_SCREEN_COUNT 3
#define CRAZYPOD_DRAW_ROWS 40
#define CRAZYPOD_ROUTE_DEPTH 8
#define CRAZYPOD_VISIBLE_ROWS 7
#define CRAZYPOD_SEARCH_PREVIEW_ROWS 4
#define CRAZYPOD_STATUS_BAR_HEIGHT 32
#define CRAZYPOD_MENU_TOPBAR_OPA 118
#define CRAZYPOD_MENU_PANEL_Y CRAZYPOD_STATUS_BAR_HEIGHT
#define CRAZYPOD_MENU_PANEL_HEIGHT (LCD_HEIGHT - CRAZYPOD_MENU_PANEL_Y)
#define CRAZYPOD_MENU_PANEL_WIDTH 160
#define CRAZYPOD_MENU_HEADER_X 16
#define CRAZYPOD_MENU_HEADER_Y 42
#define CRAZYPOD_MENU_HEADER_WIDTH 128
#define CRAZYPOD_MENU_HEADER_HEIGHT 20
#define CRAZYPOD_MENU_ROW_X 8
#define CRAZYPOD_MENU_ROW_Y 64
#define CRAZYPOD_MENU_ROW_WIDTH 140
#define CRAZYPOD_MENU_ROW_HEIGHT 24
#define CRAZYPOD_MENU_ROW_STEP 24
#define CRAZYPOD_MENU_SCROLL_X 153
#define CRAZYPOD_MENU_SCROLL_Y 66
#define CRAZYPOD_MENU_SCROLL_HEIGHT 164
#define CRAZYPOD_PREVIEW_SETTLE_TICKS \
    ((HZ * 120 / 1000) > 0 ? (HZ * 120 / 1000) : 1)
#define CRAZYPOD_MUSIC_PREVIEW_SETTLE_TICKS \
    CRAZYPOD_PREVIEW_SETTLE_TICKS
#define CRAZYPOD_PREVIEW_PART_COUNT 20
#define CRAZYPOD_PREVIEW_ENTER_DURATION_MS 380
#define CRAZYPOD_PREVIEW_EXIT_DURATION_MS 180
#define CRAZYPOD_PREVIEW_REDUCED_DURATION_MS 80
#define CRAZYPOD_PREVIEW_PART_TIME_NUMERATOR 3
#define CRAZYPOD_PREVIEW_PART_TIME_DENOMINATOR 2
#define CRAZYPOD_EDITOR_CHAR_COUNT 36
#define CRAZYPOD_SEARCH_QUERY_SIZE 33
#define CRAZYPOD_ALBUM_FLOW_CARD_COUNT 5
#define CRAZYPOD_ALBUM_FLOW_COVER_SIZE 120
#define CRAZYPOD_NOW_ARTWORK_CACHE_SIZE \
    CRAZYPOD_COVERFLOW_ARTWORK_SIZE
#define CRAZYPOD_MENU_ARTWORK_CACHE_SIZE 72
#define CRAZYPOD_MENU_NOW_ARTWORK_SIZE 68
#define CRAZYPOD_MENU_FLOW_ARTWORK_SIZE 58
#define CRAZYPOD_MENU_FLOW_ARTWORK_SLOT_BASE \
    (CRAZYPOD_COVERFLOW_ARTWORK_SLOTS - 3)
#define CRAZYPOD_MENU_ALBUM_ARTWORK_SIZE 67
#define CRAZYPOD_MENU_ARTWORK_PRIORITY 20
#define CRAZYPOD_NOW_LYRICS_COVER_SIZE 108
#define CRAZYPOD_NOW_POPUP_X 35
#define CRAZYPOD_NOW_POPUP_Y 32
#define CRAZYPOD_NOW_POPUP_WIDTH 250
#define CRAZYPOD_NOW_POPUP_HEIGHT 176
#define CRAZYPOD_NOW_POPUP_RADIUS 18
#define CRAZYPOD_CHOICE_OVERLAY_ROWS 5
#define CRAZYPOD_NOW_GLASS_WIDTH \
    ((CRAZYPOD_NOW_POPUP_WIDTH + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) / \
     CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE)
#define CRAZYPOD_NOW_GLASS_HEIGHT \
    ((CRAZYPOD_NOW_POPUP_HEIGHT + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) / \
     CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE)
#define CRAZYPOD_GLASS_SAMPLE_WIDTH 80
#define CRAZYPOD_GLASS_SAMPLE_HEIGHT 60
#define CRAZYPOD_NOW_BACKDROP_WIDTH 40
#define CRAZYPOD_NOW_BACKDROP_HEIGHT 30
#define CRAZYPOD_NOW_PRESENTATION_BANKS 2
#define CRAZYPOD_NOW_SHADE_COLOR 0x050508
#define CRAZYPOD_NOW_SHADE_OPA 118
#define CRAZYPOD_SCREEN_RADIUS_MAX 32
#define CRAZYPOD_METADATA_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_GESTURE_SETTLE_TICKS \
    ((HZ * 80 / 1000) > 0 ? (HZ * 80 / 1000) : 1)
#define CRAZYPOD_DESKTOP_MOTION_SIM_FPS 60
#define CRAZYPOD_NOW_WAVE_FRAME_TICKS \
    ((HZ / 10) > 0 ? (HZ / 10) : 1)
#define CRAZYPOD_DESKTOP_SPECTRUM_FRAME_TICKS \
    ((HZ / 10) > 0 ? (HZ / 10) : 1)
#define CRAZYPOD_PHOTO_PAN_STEP 24
#define CRAZYPOD_PHOTO_FAVORITE_HOLD_TICKS \
    ((HZ * 8 / 10) > 0 ? (HZ * 8 / 10) : 1)
#define CRAZYPOD_PHOTO_FAVORITE_PROGRESS_DELAY_TICKS \
    ((HZ / 2) > 0 ? (HZ / 2) : 1)
#define CRAZYPOD_PHOTO_FAVORITE_FEEDBACK_TICKS \
    ((HZ * 6 / 5) > 0 ? (HZ * 6 / 5) : 1)
#define CRAZYPOD_PHOTO_FAVORITE_PROGRESS_WIDTH 126
#define CRAZYPOD_PHOTO_TOUCH_MOVE_THRESHOLD 4
#define CRAZYPOD_WALLPAPER_CROP_HOLD_TICKS \
    ((HZ / 2) > 0 ? (HZ / 2) : 1)
#define CRAZYPOD_WALLPAPER_CROP_SUCCESS_TICKS \
    ((HZ * 2 / 5) > 0 ? (HZ * 2 / 5) : 1)
#define CRAZYPOD_UNLOCK_WHEEL_STEPS 19
#define CRAZYPOD_UNLOCK_WHEEL_IDLE_TICKS \
    ((HZ * 9 / 10) > 0 ? (HZ * 9 / 10) : 1)
#define CRAZYPOD_UNLOCK_WHEEL_DECAY_TICKS \
    ((HZ * 3 / 5) > 0 ? (HZ * 3 / 5) : 1)
#define CRAZYPOD_UNLOCK_DIRECTION_HINT_TICKS \
    ((HZ * 6 / 5) > 0 ? (HZ * 6 / 5) : 1)
#define CRAZYPOD_UNLOCK_OPEN_TICKS \
    ((HZ / 4) > 0 ? (HZ / 4) : 1)
#define CRAZYPOD_UNLOCK_INPUT_GUARD_TICKS \
    ((HZ / 4) > 0 ? (HZ / 4) : 1)
#define CRAZYPOD_POWER_HOLD_TICKS (3 * HZ)
#define CRAZYPOD_PREVIOUS_RESTART_THRESHOLD_MS 3000
#define CRAZYPOD_BOOT_FADE_DURATION_MS 220
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
#define CRAZYPOD_USB_PROMPT_REQUEST \
    MAKE_SYS_EVENT(SYS_EVENT_CLS_PRIVATE, 0x31)
#define CRAZYPOD_USB_PROMPT_DONE \
    MAKE_SYS_EVENT(SYS_EVENT_CLS_PRIVATE, 0x32)
#define CRAZYPOD_USB_PROMPT_TIMEOUT (5 * HZ)
#endif
#define CRAZYPOD_DESKTOP_NATIVE_TOP 40
#define CRAZYPOD_DESKTOP_NATIVE_BOTTOM 143
#define CRAZYPOD_DESKTOP_NATIVE_HEIGHT \
    (CRAZYPOD_DESKTOP_NATIVE_BOTTOM - CRAZYPOD_DESKTOP_NATIVE_TOP)
#define CRAZYPOD_DESKTOP_CAPSULE_FALLBACK_OPA 34
#define CRAZYPOD_DESKTOP_CAPSULE_TINT_OPA 48
#define CRAZYPOD_GLASS_TINT_COLOR 0x11131A
#define CRAZYPOD_GLASS_BAKE_TINT_OPA 104
#define CRAZYPOD_GLASS_PANEL_TINT_OPA 92
#define CRAZYPOD_GLASS_BORDER_OPA 38
#define CRAZYPOD_GLASS_SHADOW_OPA 92
#define CRAZYPOD_MENU_TOPBAR_PIXELS \
    (LCD_WIDTH * CRAZYPOD_STATUS_BAR_HEIGHT)
#define CRAZYPOD_MENU_PANEL_PIXELS \
    (CRAZYPOD_MENU_PANEL_WIDTH * CRAZYPOD_MENU_PANEL_HEIGHT)
#define CRAZYPOD_PREVIEW_TEXT_PANEL_WIDTH 140
#define CRAZYPOD_PREVIEW_TEXT_PANEL_MAX_HEIGHT 70
#define CRAZYPOD_SEARCH_QUERY_PIXELS (136 * 38)
#define CRAZYPOD_SEARCH_RESULTS_PIXELS (136 * 104)
#define CRAZYPOD_INFO_TOAST_PIXELS (230 * 50)
#define CRAZYPOD_INFO_BAR_PIXELS (LCD_WIDTH * 34)
#define CRAZYPOD_INFO_BAR_ALT_PIXELS (LCD_WIDTH * 34)

#define COLOR_DETAIL  0x08080D
#define COLOR_PANEL   0x1B1B22
#define COLOR_WHITE   0xFFFFFF
#define COLOR_MUTED   0x9A9AA4
#define COLOR_ROSE    0xC94A78
#define COLOR_VIOLET  0x744BC8
#define COLOR_CYAN    0x26CFF5
#define COLOR_GREEN   0x30D158
#define COLOR_AMBER   0xFFD166
#define CRAZYPOD_EQ_GAIN_MIN (-240)
#define CRAZYPOD_EQ_GAIN_MAX 240
#define CRAZYPOD_EQ_GAIN_STEP 1
#define CRAZYPOD_EQ_GAIN_FAST_STEP 10
#define CRAZYPOD_EQ_Q_MIN 1
#define CRAZYPOD_EQ_Q_MAX 64
#define CRAZYPOD_EQ_Q_STEP 1
#define CRAZYPOD_EQ_Q_FAST_STEP 10
#define CRAZYPOD_EQ_CUTOFF_MIN 20
#define CRAZYPOD_EQ_CUTOFF_MAX 22040
#define CRAZYPOD_EQ_CUTOFF_STEP 10
#define CRAZYPOD_EQ_CUTOFF_FAST_STEP 100
#define CRAZYPOD_EQ_PRECUT_MIN 0
#define CRAZYPOD_EQ_PRECUT_MAX 240
#define CRAZYPOD_EQ_PRECUT_STEP 1
#define CRAZYPOD_EQ_PRECUT_FAST_STEP 10

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
    PHOTOS_ROUTE_MENU,
    PHOTOS_ROUTE_LIBRARY,
    PHOTOS_ROUTE_VIDEOS,
    PHOTOS_ROUTE_FAVORITES,
    PHOTOS_ROUTE_DETAIL,
    SETTINGS_ROUTE_MENU,
    SETTINGS_ROUTE_SOUND,
    SETTINGS_ROUTE_EQ_STUDIO,
    SETTINGS_ROUTE_DISPLAY,
    SETTINGS_ROUTE_PLAYBACK,
    SETTINGS_ROUTE_POWER,
    SETTINGS_ROUTE_CONTROLS,
    SETTINGS_ROUTE_MAIN_MENU,
    SETTINGS_ROUTE_MAIN_MENU_ACTIONS,
    EXTRAS_ROUTE_MENU,
    NOTES_ROUTE_MENU,
    NOTES_ROUTE_COMPOSER,
    NOTES_ROUTE_EXIT_ACTIONS,
    NOTES_ROUTE_DISCARD_CONFIRM,
    NOTES_ROUTE_SEARCH,
    NOTES_ROUTE_SEARCH_RESULTS,
    NOTES_ROUTE_READER,
    NOTES_ROUTE_ACTIONS,
    NOTES_ROUTE_DELETED,
    NOTES_ROUTE_DELETED_ACTIONS,
    NOTES_ROUTE_DELETE_CONFIRM,
    NOTES_ROUTE_PERMANENT_CONFIRM,
    NOTES_ROUTE_EMPTY_TRASH_CONFIRM,
    BOOKS_ROUTE_MENU,
    BOOKS_ROUTE_RECENTS,
    BOOKS_ROUTE_LIBRARY,
    BOOKS_ROUTE_FAVORITES,
    BOOKS_ROUTE_READER,
    BOOKS_ROUTE_ACTIONS,
    BOOKS_ROUTE_CHAPTERS,
    BOOKS_ROUTE_BOOKMARKS,
    BOOKS_ROUTE_DELETE_CONFIRM,
    BOOKS_ROUTE_STATS,
    BOOKS_ROUTE_READING_SETTINGS,
    BOOKS_ROUTE_INFO,
    PODCASTS_ROUTE_MENU,
    UTILITIES_ROUTE_MENU,
    MINIAPP_ROUTE_VIEW,
    CLOCK_ROUTE_MENU,
    CLOCK_ROUTE_SLEEP_TIMER,
    CLOCK_ROUTE_VIEW,
    STOPWATCH_ROUTE_VIEW,
    WORKOUT_ROUTE_MENU,
    WORKOUT_ROUTE_TYPES,
    WORKOUT_ROUTE_READY,
    WORKOUT_ROUTE_ACTIVE,
    WORKOUT_ROUTE_FINISH_CONFIRM,
    WORKOUT_ROUTE_HISTORY,
    WORKOUT_ROUTE_SUMMARY,
    WORKOUT_ROUTE_DETAIL,
    WORKOUT_ROUTE_DELETE_CONFIRM,
    CALENDAR_ROUTE_MENU,
    CALENDAR_ROUTE_TODAY,
    CALENDAR_ROUTE_UPCOMING,
    CALENDAR_ROUTE_MONTH,
    CALENDAR_ROUTE_DAY_EVENTS,
    CALENDAR_ROUTE_EDITOR,
    CALENDAR_ROUTE_TITLE_EDITOR,
    CALENDAR_ROUTE_DETAIL,
    CALENDAR_ROUTE_ACTIONS,
    CALENDAR_ROUTE_DELETE_CONFIRM,
    CONTACTS_ROUTE_LIST,
    CONTACTS_ROUTE_DETAIL,
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
    DIY_ROUTE_WALLPAPER_CROP,
    DIY_ROUTE_LAYOUT,
};

enum crazypod_preview_icon {
    CRAZYPOD_PREVIEW_ICON_NOW_PLAYING,
    CRAZYPOD_PREVIEW_ICON_ALBUM_FLOW,
    CRAZYPOD_PREVIEW_ICON_ALL_MUSIC,
    CRAZYPOD_PREVIEW_ICON_PLAYLISTS,
    CRAZYPOD_PREVIEW_ICON_ARTIST,
    CRAZYPOD_PREVIEW_ICON_ALBUMS,
    CRAZYPOD_PREVIEW_ICON_SONGS,
    CRAZYPOD_PREVIEW_ICON_SEARCH
};

struct route_state {
    enum crazypod_route route;
    int selected;
    int group;
};

struct crazypod_app {
    enum crazypod_app_id id;
    const char *name;
    const char *symbol;
    uint32_t color;
    lv_obj_t *cell;
};

struct status_bar {
    lv_obj_t *time;
    lv_obj_t *battery;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_cap;
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

enum choice_overlay_kind {
    CHOICE_OVERLAY_NONE,
    CHOICE_OVERLAY_ICON_THEME,
    CHOICE_OVERLAY_APPEARANCE,
    CHOICE_OVERLAY_BACKGROUND,
    CHOICE_OVERLAY_SETTING,
    CHOICE_OVERLAY_BOOK_FONT_SIZE,
    CHOICE_OVERLAY_BOOK_THEME,
};

enum crazypod_glass_material {
    CRAZYPOD_GLASS_POPUP = 0,
    CRAZYPOD_GLASS_MENU_PANEL,
    CRAZYPOD_GLASS_MENU_TOPBAR,
    CRAZYPOD_GLASS_TEXT_PANEL,
    CRAZYPOD_GLASS_HOME_CAPSULE,
    CRAZYPOD_GLASS_INFO_TOAST,
};

enum eq_studio_mode {
    EQ_STUDIO_GAIN,
    EQ_STUDIO_CUTOFF,
    EQ_STUDIO_Q,
    EQ_STUDIO_PRECUT,
    EQ_STUDIO_MODE_COUNT,
};

enum settings_item {
    SETTINGS_ITEM_EQ_ENABLED,
    SETTINGS_ITEM_BASS,
    SETTINGS_ITEM_TREBLE,
    SETTINGS_ITEM_BALANCE,
    SETTINGS_ITEM_BRIGHTNESS,
    SETTINGS_ITEM_BACKLIGHT_TIMEOUT,
    SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED,
    SETTINGS_ITEM_LCD_SLEEP,
    SETTINGS_ITEM_REDUCE_MOTION,
    SETTINGS_ITEM_SHUFFLE,
    SETTINGS_ITEM_REPEAT,
    SETTINGS_ITEM_SLEEP_TIMER_DURATION,
    SETTINGS_ITEM_SLEEP_TIMER_STARTUP,
    SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS,
#ifdef HAVE_USB_CHARGING_ENABLE
    SETTINGS_ITEM_USB_CHARGING,
#endif
    SETTINGS_ITEM_BEEP,
    SETTINGS_ITEM_KEYCLICK,
#ifdef HAVE_HARDWARE_CLICK
    SETTINGS_ITEM_SPEAKER_CLICK,
#endif
    SETTINGS_ITEM_KEYCLICK_REPEATS,
    SETTINGS_ITEM_COUNT,
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

struct choice_overlay_view {
    enum choice_overlay_kind kind;
    int id;
    int selected;
    int count;
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *value;
    lv_obj_t *counter;
    lv_obj_t *rows[CRAZYPOD_CHOICE_OVERLAY_ROWS];
    lv_obj_t *swatches[CRAZYPOD_CHOICE_OVERLAY_ROWS];
    lv_obj_t *labels[CRAZYPOD_CHOICE_OVERLAY_ROWS];
    lv_obj_t *markers[CRAZYPOD_CHOICE_OVERLAY_ROWS];
    lv_obj_t *scroll_thumb;
};

#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
struct usb_prompt_view {
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *rows[2];
    lv_obj_t *markers[2];
    lv_obj_t *hints[2];
    int selected;
    unsigned request;
};
#endif

struct power_prompt_view {
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *rows[2];
    lv_obj_t *markers[2];
    lv_obj_t *hints[2];
    int selected;
};

static struct crazypod_app apps[CRAZYPOD_APP_COUNT] = {
    { CRAZYPOD_APP_MUSIC, "Music", LV_SYMBOL_AUDIO, 0xFF2E54, NULL },
    { CRAZYPOD_APP_PODCASTS, "Podcasts", LV_SYMBOL_VOLUME_MAX,
      0xA95BDE, NULL },
    { CRAZYPOD_APP_MINI_APPS, "Mini Apps", LV_SYMBOL_LIST,
      0xFF9F0A, NULL },
    { CRAZYPOD_APP_SHUFFLE, "Shuffle", LV_SYMBOL_SHUFFLE,
      0xFF375F, NULL },
    { CRAZYPOD_APP_LOCK, "Lock", LV_SYMBOL_EYE_CLOSE, 0x59606B, NULL },
    { CRAZYPOD_APP_PHOTOS, "Media", LV_SYMBOL_IMAGE, 0x3478F6, NULL },
    { CRAZYPOD_APP_CUSTOMIZE, "Customize", LV_SYMBOL_EDIT,
      0xBF5AF2, NULL },
    { CRAZYPOD_APP_WORKOUTS, "Workouts", LV_SYMBOL_PLAY,
      0xA8F12D, NULL },
    { CRAZYPOD_APP_BOOKS, "Books", LV_SYMBOL_FILE, 0xFF9F0A, NULL },
    { CRAZYPOD_APP_NOTES, "Notes", LV_SYMBOL_EDIT, 0xFFD60A, NULL },
    { CRAZYPOD_APP_CLOCK, "Clock", LV_SYMBOL_HOME, 0xF26D5B, NULL },
    { CRAZYPOD_APP_CONTACTS, "Contacts", LV_SYMBOL_HOME,
      0x4F9BFF, NULL },
    { CRAZYPOD_APP_CALENDAR, "Calendar", LV_SYMBOL_LIST,
      0xFF453A, NULL },
    { CRAZYPOD_APP_STOPWATCH, "Stopwatch", LV_SYMBOL_REFRESH,
      0xFFB340, NULL },
    { CRAZYPOD_APP_EXTRAS, "More Features", LV_SYMBOL_DIRECTORY,
      0x64D2FF, NULL },
    { CRAZYPOD_APP_SETTINGS, "Settings", LV_SYMBOL_SETTINGS,
      0x8E8E93, NULL },
};

static int app_catalog_index(enum crazypod_app_id id)
{
    int i;

    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        if(apps[i].id == id)
            return i;
    }
    return -1;
}

static struct crazypod_app *app_for_id(enum crazypod_app_id id)
{
    int index = app_catalog_index(id);
    return index >= 0 ? &apps[index] : NULL;
}

static struct crazypod_app *visible_app(int index)
{
    return app_for_id(crazypod_apps_visible_id(index));
}

static struct crazypod_app *ordered_app(int index)
{
    return app_for_id(crazypod_apps_ordered_id(index));
}

static struct crazypod_app *hidden_app(int index)
{
    return app_for_id(crazypod_apps_hidden_id(index));
}

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
    "Icon Size", "Wave Style", "Glow", "Highlight",
    "Primary", "Secondary"
};

static const enum crazypod_appearance_field diy_detail_fields[] = {
    CRAZYPOD_APPEARANCE_ICON_SCALE,
    CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE,
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
    "Home", "Menu", "Lock Screen"
};

static const char *const settings_menu_titles[] = {
    "Sound", "Display", "Playback", "Power", "Controls", "Main Menu"
};

static const char *const settings_menu_symbols[] = {
    LV_SYMBOL_AUDIO, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_PLAY,
    LV_SYMBOL_POWER, LV_SYMBOL_SETTINGS, LV_SYMBOL_LIST
};

static const char *const main_menu_action_titles[] = {
    "Hide", "Up", "Down"
};

static const char *const workout_menu_titles[] = {
    "Start Workout", "History", "Summary"
};

static const int settings_sound_items[] = {
    SETTINGS_ITEM_EQ_ENABLED,
    SETTINGS_ITEM_BASS,
    SETTINGS_ITEM_TREBLE,
    SETTINGS_ITEM_BALANCE,
};

static const int settings_display_items[] = {
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    SETTINGS_ITEM_BRIGHTNESS,
#endif
    SETTINGS_ITEM_BACKLIGHT_TIMEOUT,
#if CONFIG_CHARGING
    SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED,
#endif
    SETTINGS_ITEM_LCD_SLEEP,
    SETTINGS_ITEM_REDUCE_MOTION,
};

static const int settings_playback_items[] = {
    SETTINGS_ITEM_SHUFFLE,
    SETTINGS_ITEM_REPEAT,
};

static const int settings_power_items[] = {
#ifdef HAVE_USB_CHARGING_ENABLE
    SETTINGS_ITEM_USB_CHARGING,
#endif
    SETTINGS_ITEM_SLEEP_TIMER_DURATION,
    SETTINGS_ITEM_SLEEP_TIMER_STARTUP,
    SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS,
};

static const int settings_controls_items[] = {
    SETTINGS_ITEM_BEEP,
    SETTINGS_ITEM_KEYCLICK,
#ifdef HAVE_HARDWARE_CLICK
    SETTINGS_ITEM_SPEAKER_CLICK,
#endif
    SETTINGS_ITEM_KEYCLICK_REPEATS,
};

static const int setting_timeout_values[] = {
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    15, 20, 25, 30, 45, 60, 90, 120, 180, 240, 300
};

static const int setting_lcd_sleep_values[] = {
    -1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    15, 20, 25, 30, 45, 60, 90, 120, 180, 240, 300
};

static const int setting_sleep_timer_values[] = {
    0, 5, 10, 15, 30, 45, 60, 90, 120, 180, 240, 300
};

static const int setting_repeat_values[] = {
    REPEAT_OFF, REPEAT_ALL, REPEAT_ONE
};

static const char *const photos_menu_titles[] = {
    "Photos", "Videos", "Favorites"
};

static const char *const photos_menu_symbols[] = {
    LV_SYMBOL_IMAGE, LV_SYMBOL_PLAY, LV_SYMBOL_OK
};

static const uint32_t book_page_colors[] = {
    0xE8D5A4, 0xF8F8F4, 0xDDEFE3, 0x17181D
};

static const uint32_t book_ink_colors[] = {
    0x302A22, 0x252525, 0x24382D, 0xECECF1
};

static struct status_bar status_bars[CRAZYPOD_STATUS_BAR_COUNT];
static struct route_state route_stack[CRAZYPOD_ROUTE_DEPTH];
static lv_obj_t *desktop_screen;
static lv_obj_t *desktop_wallpaper;
static lv_obj_t *desktop_carousel;
static lv_obj_t *desktop_title;
static lv_obj_t *desktop_indicators[CRAZYPOD_APP_COUNT];
static lv_obj_t *product_screen;
static lv_obj_t *product_content;
static lv_obj_t *lock_screen;
static lv_obj_t *lock_wallpaper;
static lv_obj_t *lock_time_label;
static lv_obj_t *lock_date_label;
static lv_obj_t *lock_hint_label;
static lv_obj_t *lock_progress_surface;
static lv_obj_t *lock_icon_shackle;
static lv_obj_t *lock_icon_body;
static lv_obj_t *desktop_capsule;
static lv_obj_t *desktop_capsule_glass;
static lv_obj_t *desktop_capsule_track;
static lv_obj_t *desktop_capsule_artist;
static lv_obj_t *desktop_capsule_progress;
static lv_obj_t *desktop_capsule_spectrum;
static lv_obj_t *desktop_capsule_wave_ball;
static lv_obj_t *desktop_capsule_wave_glow;
static lv_obj_t *desktop_capsule_artwork;
static lv_obj_t *desktop_capsule_artwork_image;
static lv_obj_t *desktop_capsule_artwork_symbol;
static struct album_flow_card
    album_flow_cards[CRAZYPOD_ALBUM_FLOW_CARD_COUNT];
static lv_obj_t *album_flow_title;
static lv_obj_t *album_flow_artist;
static lv_obj_t *album_flow_position;
static int album_flow_displayed_album = -1;
static lv_obj_t *now_progress_marker;
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
static struct crazypod_note_draft note_editor;
static struct crazypod_note_draft note_editor_baseline;
static bool note_draft_available;
static bool note_editor_body_active;
static size_t note_editor_title_cursor;
static size_t note_editor_body_cursor;
static bool note_draft_save_pending;
static long note_draft_save_due;
static char note_reader_body[CRAZYPOD_NOTE_BODY_SIZE];
static char note_search_query[CRAZYPOD_SEARCH_QUERY_SIZE];
static int selected_book_index = -1;
static uint32_t book_page_offset;
static uint32_t book_next_offset;
static uint32_t book_page_history[64];
static int book_page_history_count;
static char book_page_text[2048];
static bool books_metadata_ready;
static lv_obj_t *book_loading_progress_fill;
static lv_obj_t *book_loading_progress_label;
static lv_obj_t *book_loading_percent_label;
static bool stopwatch_running;
static long stopwatch_started_at;
static long stopwatch_accumulated_ticks;
static long stopwatch_last_render_tick;
static long stopwatch_laps[32];
static int stopwatch_lap_count;
static int stopwatch_style_index;
static long stopwatch_reset_armed_until;
static bool workout_running;
static long workout_started_at;
static long workout_accumulated_ticks;
static long workout_last_render_tick;
static int workout_activity;
static long clock_last_render_tick;
static int calendar_focus_year;
static int calendar_focus_month;
static int calendar_focus_day;
static uint32_t calendar_editor_id;
static int calendar_editor_date;
static int calendar_editor_minutes;
static char calendar_editor_summary[96];
static size_t calendar_editor_cursor;
static int calendar_editor_error;
static struct cp_scene miniapp_scene;
static fb_data miniapp_bitmap_pixels[160 * 160];
static lv_image_dsc_t miniapp_bitmap_descriptor;
static char miniapp_bitmap_id[CP_MINIAPP_RESOURCE_ID_SIZE];
static int miniapp_bitmap_app_index = -1;
static uint32_t miniapp_bitmap_crc;
static uint16_t miniapp_bitmap_width;
static uint16_t miniapp_bitmap_height;
static long miniapp_last_service_tick;
static char miniapp_alert_id[CRAZYPOD_MINIAPP_ID_SIZE];
static uint32_t miniapp_alert_deadline;
static uint32_t miniapp_alert_token;
static int miniapp_alert_remaining;
static long miniapp_alert_next_tick;
static bool miniapp_alert_delivery_pending;
static bool miniapp_render_pending;
static int miniapp_last_error;
static struct crazypod_miniapp_input_queue miniapp_input_queue;
static int route_depth;
static bool product_active;
static bool screen_locked;
static bool lock_backlight_was_on;
static bool lock_wait_for_wake_release;
static bool lock_release_guard;
static bool lock_opening;
static bool lock_wrong_direction;
static long lock_release_guard_until;
static long lock_opening_start;
static long lock_wrong_direction_until;
static long lock_wheel_last_input_tick;
static int lock_wheel_steps;
static int lock_wheel_decay_start_steps;
static int lock_progress_percent;
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
static unsigned menu_preview_artwork_generation_seen;
static unsigned now_prefetch_artwork_generation_seen;
static unsigned now_artwork_generation_seen;
static unsigned capsule_artwork_generation_seen;
static unsigned photo_generation_seen;
static unsigned photo_view_generation_seen;
static unsigned video_generation_seen;
static long route_render_due;
static long menu_preview_due;
static long boost_until;
static long music_scan_not_before;
static struct crazypod_frameclock desktop_motion_clock;
static struct crazypod_frameclock lvgl_clock;
static int desktop_motion_step_accumulator;
static int desktop_position_q8;
static int desktop_velocity_q8;
static struct menu_view_state menu_view;

struct menu_preview_motion_part {
    lv_obj_t *object;
    int final_x;
    int final_y;
    int final_scale;
    int final_rotation;
    int final_opacity;
    int enter_dx;
    int enter_dy;
    int enter_scale;
    int enter_rotation;
    int enter_opacity;
    int exit_dx;
    int exit_dy;
    int exit_scale;
    int exit_rotation;
    int enter_delay;
    int enter_duration;
};

struct menu_preview_scene {
    lv_obj_t *content;
    struct menu_preview_motion_part parts[CRAZYPOD_PREVIEW_PART_COUNT];
    int part_count;
};

enum menu_preview_motion_phase {
    MENU_PREVIEW_MOTION_IDLE = 0,
    MENU_PREVIEW_MOTION_ENTERING,
    MENU_PREVIEW_MOTION_EXITING,
};

enum menu_preview_motion_profile {
    MENU_PREVIEW_PROFILE_DEFAULT = 0,
    MENU_PREVIEW_PROFILE_MUSIC,
    MENU_PREVIEW_PROFILE_PHOTOS,
    MENU_PREVIEW_PROFILE_NOTES,
    MENU_PREVIEW_PROFILE_BOOKS,
};

static lv_obj_t *menu_preview_root;
static lv_obj_t *menu_preview_content;
static struct menu_preview_scene menu_preview_scene;
static bool menu_preview_pending;
static bool menu_preview_motion_ready;
static bool menu_preview_build_defer_media;
static bool menu_preview_media_deferred;
static bool menu_preview_media_refresh_pending;
static bool menu_preview_motion_reduced;
static long menu_preview_media_due;
static enum menu_preview_motion_phase menu_preview_motion_phase;
static enum menu_preview_motion_profile menu_preview_motion_profile;
static int menu_preview_navigation_direction = 1;
static struct now_queue_popup_view now_queue_view;
static struct now_actions_popup_view now_actions_view;
static struct now_volume_popup_view now_volume_view;
static enum now_playing_overlay now_overlay;
static struct choice_overlay_view choice_overlay;
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
static struct usb_prompt_view usb_prompt_view;
static struct semaphore usb_prompt_response;
static volatile bool usb_prompt_registered;
static volatile bool usb_prompt_ui_ready;
static volatile bool usb_prompt_waiting;
static volatile unsigned usb_prompt_request_id;
static volatile int usb_prompt_result;
#endif
static struct power_prompt_view power_prompt_view;
static bool power_play_holding;
static long power_play_hold_start;
static int now_action_selected;
static int now_queue_selected;
static int eq_studio_band = 5;
static enum eq_studio_mode eq_studio_mode = EQ_STUDIO_GAIN;
static bool eq_studio_editing;
static unsigned now_queue_generation_seen;
static lv_obj_t *now_overlay_root;
static lv_obj_t *now_overlay_panel;
static char search_query[CRAZYPOD_SEARCH_QUERY_SIZE];
static char preset_name_editor[CRAZYPOD_PRESET_NAME_SIZE];
static char rendered_track_path[MAX_PATH];
static char now_prefetch_track_path[MAX_PATH];
static char pending_now_playing_track_path[MAX_PATH];
static char now_presentation_track_path[
    CRAZYPOD_NOW_PRESENTATION_BANKS][MAX_PATH];
static char desktop_capsule_artwork_path[MAX_PATH];
static bool now_playing_open_pending;
static int pending_now_playing_route_depth;
static enum crazypod_route pending_now_playing_route;
static int wallpaper_crop_photo_index;
static enum crazypod_appearance_field wallpaper_crop_target;
static int wallpaper_crop_zoom_percent;
static int wallpaper_crop_center_x;
static int wallpaper_crop_center_y;
static bool wallpaper_crop_render_pending;
enum wallpaper_crop_phase {
    WALLPAPER_CROP_EDITING = 0,
    WALLPAPER_CROP_APPLYING,
    WALLPAPER_CROP_APPLIED,
    WALLPAPER_CROP_ERROR
};
static enum wallpaper_crop_phase wallpaper_crop_phase;
static bool wallpaper_crop_error_loading;
static long wallpaper_crop_feedback_until;
static bool wallpaper_crop_menu_holding;
static bool wallpaper_crop_menu_armed;
static long wallpaper_crop_menu_hold_start;
static bool wallpaper_crop_play_holding;
static bool wallpaper_crop_play_armed;
static long wallpaper_crop_play_hold_start;
static bool wallpaper_crop_select_armed;
static lv_obj_t *wallpaper_crop_progress_fill;
static lv_obj_t *wallpaper_crop_progress_label;
static int wallpaper_crop_load_progress_seen;
static int wallpaper_crop_apply_progress;
static enum crazypod_wallpaper_apply_result
    wallpaper_crop_apply_result;
static int photo_pan_x;
static int photo_pan_y;
static int photo_zoom_percent;
static bool photo_select_long_handled;
static bool photo_select_holding;
static long photo_select_hold_start;
static int photo_select_hold_percent;
static long photo_favorite_feedback_until;
static bool photo_favorite_feedback_added;
static bool photo_favorite_feedback_error;
static lv_obj_t *photo_favorite_progress_fill;
static bool photo_wheel_touch_active;
static int photo_wheel_touch_start;
static int photo_wheel_touch_max_delta;
static long photo_direction_input_tick;
static bool photo_pan_render_pending;
static lv_obj_t *screen_corner_masks[
    CRAZYPOD_CORNER_SCREEN_COUNT][4];
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
static fb_data glass_sample_pixels[
    CRAZYPOD_GLASS_SAMPLE_WIDTH * CRAZYPOD_GLASS_SAMPLE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data glass_sample_scratch[
    CRAZYPOD_GLASS_SAMPLE_WIDTH * CRAZYPOD_GLASS_SAMPLE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data menu_topbar_glass_pixels[CRAZYPOD_MENU_TOPBAR_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data menu_panel_glass_pixels[CRAZYPOD_MENU_PANEL_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data search_query_glass_pixels[CRAZYPOD_SEARCH_QUERY_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data search_results_glass_pixels[CRAZYPOD_SEARCH_RESULTS_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data info_toast_glass_pixels[CRAZYPOD_INFO_TOAST_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data info_bar_glass_pixels[CRAZYPOD_INFO_BAR_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data info_bar_alt_glass_pixels[CRAZYPOD_INFO_BAR_ALT_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t menu_topbar_glass_descriptor;
static lv_image_dsc_t menu_panel_glass_descriptor;
static lv_image_dsc_t search_query_glass_descriptor;
static lv_image_dsc_t search_results_glass_descriptor;
static lv_image_dsc_t info_toast_glass_descriptor;
static lv_image_dsc_t info_bar_glass_descriptor;
static lv_image_dsc_t info_bar_alt_glass_descriptor;
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
static int desktop_capsule_spectrum_phase;
static long last_desktop_capsule_spectrum_tick;
static bool desktop_capsule_spectrum_playing_seen;
static bool now_lyrics_mode;

extern struct frame_buffer_t lcd_framebuffer_default;

static int appearance_tile_size(void);
static void layout_desktop_carousel(bool animated);
static void refresh_menu_rows(const struct route_state *state);
static struct route_state *current_route(void);
static void render_current_route(bool transition);
static void render_menu_preview(const struct route_state *state,
                                bool animated);
static void cycle_playback_mode(void);
static void show_now_actions_popup(void);
static void show_now_queue_popup(void);
static void show_now_volume_popup(void);
static void dismiss_now_overlay(bool refresh_now_playing);
static void dismiss_choice_overlay(bool refresh_route);
static void apply_book_font_size(int value);
static void rescan_books_with_progress(void);
static void render_book_loading_screen(
    const struct crazypod_book *book, const char *title,
    const char *detail);
static void book_prepare_progress_callback(
    int percent, const char *stage, void *context);
static void load_book_metadata_with_progress(
    int start_percent, int end_percent);
static void request_now_playing_route(void);
static void play_wheel_feedback(long button);
static void push_route_selected(enum crazypod_route route, int group,
                                int selected);
static void pop_route(void);
static void prepare_glass_descriptor(int source_x, int source_y,
                                     int width, int height,
                                     enum crazypod_glass_material material,
                                     fb_data *render_pixels,
                                     lv_image_dsc_t *descriptor);

static bool usb_prompt_visible(void)
{
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    return usb_prompt_view.root != NULL;
#else
    return false;
#endif
}

static bool power_prompt_visible(void)
{
    return power_prompt_view.root != NULL;
}

static bool modal_prompt_visible(void)
{
    return usb_prompt_visible() || power_prompt_visible();
}

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

static lv_obj_t *make_preview_icon_part(lv_obj_t *parent,
                                        int x, int y,
                                        int width, int height,
                                        int radius, lv_opa_t opacity)
{
    return make_box(parent, x, y, width, height, radius,
                    COLOR_CYAN, opacity);
}

static lv_obj_t *make_music_preview_icon(
    lv_obj_t *parent, enum crazypod_preview_icon icon, int x, int y)
{
    lv_obj_t *stage = make_box(parent, x, y, 96, 96, 24,
                               0x102A38, 188);
    lv_obj_t *part;

    lv_obj_set_style_bg_grad_color(
        stage, lv_color_hex(0x07131B), 0);
    lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(stage, 1, 0);
    lv_obj_set_style_border_color(stage, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_border_opa(stage, 58, 0);

    switch(icon) {
    case CRAZYPOD_PREVIEW_ICON_NOW_PLAYING:
        make_preview_icon_part(stage, 26, 18, 38, 7, 3, 235);
        make_preview_icon_part(stage, 26, 22, 7, 40, 3, 235);
        make_preview_icon_part(stage, 57, 22, 7, 34, 3, 235);
        make_preview_icon_part(stage, 16, 53, 22, 17,
                               LV_RADIUS_CIRCLE, 255);
        make_preview_icon_part(stage, 47, 47, 22, 17,
                               LV_RADIUS_CIRCLE, 255);
        break;
    case CRAZYPOD_PREVIEW_ICON_ALBUM_FLOW:
    case CRAZYPOD_PREVIEW_ICON_ALBUMS:
        part = make_preview_icon_part(stage, 12, 20, 54, 50, 10, 75);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(part, lv_color_hex(COLOR_CYAN), 0);
        lv_obj_set_style_border_opa(part, 95, 0);
        part = make_preview_icon_part(stage, 17, 15, 54, 50, 10, 120);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(part, lv_color_hex(COLOR_CYAN), 0);
        lv_obj_set_style_border_opa(part, 125, 0);
        part = make_preview_icon_part(stage, 22, 10, 54, 50, 10, 215);
        lv_obj_set_style_bg_grad_color(
            part, lv_color_hex(0x176F8A), 0);
        lv_obj_set_style_bg_grad_dir(part, LV_GRAD_DIR_HOR, 0);
        make_preview_icon_part(part, 13, 11, 30, 30,
                               LV_RADIUS_CIRCLE, 220);
        make_box(part, 24, 22, 8, 8, LV_RADIUS_CIRCLE,
                 0x102A38, LV_OPA_COVER);
        break;
    case CRAZYPOD_PREVIEW_ICON_ARTIST:
        make_preview_icon_part(stage, 29, 13, 30, 30,
                               LV_RADIUS_CIRCLE, 245);
        make_preview_icon_part(stage, 17, 48, 54, 27,
                               LV_RADIUS_CIRCLE, 225);
        make_box(stage, 27, 62, 34, 14, 7, 0x102A38, 188);
        break;
    case CRAZYPOD_PREVIEW_ICON_PLAYLISTS:
        make_preview_icon_part(stage, 17, 20, 10, 10,
                               LV_RADIUS_CIRCLE, 235);
        make_preview_icon_part(stage, 17, 40, 10, 10,
                               LV_RADIUS_CIRCLE, 235);
        make_preview_icon_part(stage, 17, 60, 10, 10,
                               LV_RADIUS_CIRCLE, 235);
        make_preview_icon_part(stage, 34, 22, 38, 6, 3, 220);
        make_preview_icon_part(stage, 34, 42, 38, 6, 3, 220);
        make_preview_icon_part(stage, 34, 62, 38, 6, 3, 220);
        break;
    case CRAZYPOD_PREVIEW_ICON_ALL_MUSIC:
        make_preview_icon_part(stage, 14, 21, 39, 6, 3, 180);
        make_preview_icon_part(stage, 14, 39, 31, 6, 3, 180);
        make_preview_icon_part(stage, 14, 57, 24, 6, 3, 180);
        make_preview_icon_part(stage, 58, 16, 7, 38, 3, 235);
        make_preview_icon_part(stage, 46, 47, 24, 18,
                               LV_RADIUS_CIRCLE, 255);
        break;
    case CRAZYPOD_PREVIEW_ICON_SONGS:
        make_preview_icon_part(stage, 48, 18, 7, 41, 3, 235);
        make_preview_icon_part(stage, 48, 18, 22, 7, 3, 235);
        make_preview_icon_part(stage, 33, 51, 24, 19,
                               LV_RADIUS_CIRCLE, 255);
        break;
    case CRAZYPOD_PREVIEW_ICON_SEARCH:
        part = make_box(stage, 14, 12, 48, 48,
                        LV_RADIUS_CIRCLE, COLOR_CYAN, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(part, 8, 0);
        lv_obj_set_style_border_color(part, lv_color_hex(COLOR_CYAN), 0);
        lv_obj_set_style_border_opa(part, 235, 0);
        make_preview_icon_part(stage, 57, 54, 8, 8, 3, 235);
        make_preview_icon_part(stage, 62, 59, 8, 8, 3, 235);
        make_preview_icon_part(stage, 67, 64, 8, 8, 3, 235);
        break;
    }
    return stage;
}

static void make_pixel_heart(lv_obj_t *parent, int x, int y, int unit,
                             uint32_t color, lv_opa_t opacity)
{
    if(unit < 1)
        unit = 1;
    make_box(parent, x + unit, y, 2 * unit, unit, 0,
             color, opacity);
    make_box(parent, x + 5 * unit, y, 2 * unit, unit, 0,
             color, opacity);
    make_box(parent, x, y + unit, 8 * unit, 2 * unit, 0,
             color, opacity);
    make_box(parent, x + unit, y + 3 * unit, 6 * unit, unit, 0,
             color, opacity);
    make_box(parent, x + 2 * unit, y + 4 * unit, 4 * unit, unit, 0,
             color, opacity);
    make_box(parent, x + 3 * unit, y + 5 * unit, 2 * unit, unit, 0,
             color, opacity);
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
    case CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE:
        return value->sound_wave_style;
    case CRAZYPOD_APPEARANCE_GLOW: return value->glow;
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE:
        return value->highlight_style;
    case CRAZYPOD_APPEARANCE_PRIMARY: return value->primary_color;
    case CRAZYPOD_APPEARANCE_SECONDARY: return value->secondary_color;
    case CRAZYPOD_APPEARANCE_HOME_BACKGROUND:
        return value->home_background;
    case CRAZYPOD_APPEARANCE_MENU_BACKGROUND:
        return value->menu_background;
    case CRAZYPOD_APPEARANCE_LOCK_BACKGROUND:
        return value->lock_background;
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
    case CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE:
        return CRAZYPOD_SOUND_WAVE_STYLE_COUNT;
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
    case CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE:
        return crazypod_sound_wave_style_name(index);
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
    case CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE: return "WAVE STYLE";
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

static enum crazypod_appearance_field background_field_for_index(int index)
{
    if(index == 1)
        return CRAZYPOD_APPEARANCE_MENU_BACKGROUND;
    if(index == 2)
        return CRAZYPOD_APPEARANCE_LOCK_BACKGROUND;
    return CRAZYPOD_APPEARANCE_HOME_BACKGROUND;
}

static const char *background_field_title(
    enum crazypod_appearance_field field)
{
    if(field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
        return "MENU";
    if(field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
        return "LOCK SCREEN";
    return "HOME";
}

static const char *background_wallpaper(
    const struct crazypod_appearance *appearance,
    enum crazypod_appearance_field field)
{
    if(field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
        return appearance->menu_wallpaper;
    if(field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
        return appearance->lock_wallpaper;
    return appearance->home_wallpaper;
}

static enum crazypod_wallpaper_target wallpaper_target_for_field(
    enum crazypod_appearance_field field)
{
    if(field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
        return CRAZYPOD_WALLPAPER_MENU;
    if(field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
        return CRAZYPOD_WALLPAPER_LOCK;
    return CRAZYPOD_WALLPAPER_HOME;
}

static uint32_t background_default_color(
    enum crazypod_appearance_field field)
{
    if(field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
        return 0x08080D;
    if(field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
        return 0x07090D;
    return 0x141419;
}

static void refresh_lock_clock(void)
{
    static const char *const days[] = {
        "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
        "THURSDAY", "FRIDAY", "SATURDAY"
    };
    static const char *const months[] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };
    struct tm *now;
    char time_text[8];
    char date_text[32];
    int weekday;
    int month;

    if(lock_time_label == NULL || lock_date_label == NULL)
        return;
    now = get_time();
    weekday = now->tm_wday >= 0 && now->tm_wday < 7
        ? now->tm_wday : 0;
    month = now->tm_mon >= 0 && now->tm_mon < 12
        ? now->tm_mon : 0;
    snprintf(time_text, sizeof(time_text), "%02d:%02d",
             now->tm_hour, now->tm_min);
    snprintf(date_text, sizeof(date_text), "%s  \xE2\x80\xA2  %s %d",
             days[weekday], months[month], now->tm_mday);
    lv_label_set_text(lock_time_label, time_text);
    lv_label_set_text(lock_date_label, date_text);
}

static void refresh_lock_appearance(void)
{
    const struct crazypod_appearance *appearance;
    const lv_image_dsc_t *wallpaper = NULL;
    uint32_t color;

    if(lock_screen == NULL)
        return;
    appearance = crazypod_appearance_get();
    if(appearance->lock_wallpaper[0] != '\0') {
        wallpaper = crazypod_custom_lock_wallpaper();
        color = crazypod_appearance_lock_color();
    }
    else if(appearance->lock_background == 0) {
        wallpaper = crazypod_custom_home_wallpaper();
        if(wallpaper == NULL &&
           appearance->home_wallpaper[0] == '\0' &&
           appearance->home_background == 0)
            wallpaper = crazypod_default_wallpaper();
        color = crazypod_appearance_home_color();
    }
    else {
        color = crazypod_appearance_lock_color();
    }
    lv_obj_set_style_bg_color(
        lock_screen, lv_color_hex(color), 0);
    if(wallpaper != NULL) {
        lv_image_set_src(lock_wallpaper, wallpaper);
        lv_obj_remove_flag(lock_wallpaper, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(lock_wallpaper, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_invalidate(lock_screen);
}

static void draw_lock_progress_event(lv_event_t *event)
{
    lv_obj_t *surface;
    lv_layer_t *layer;
    lv_area_t area;
    lv_draw_arc_dsc_t arc;
    int center_x;
    int center_y;
    int progress;

    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN)
        return;
    surface = lv_event_get_target(event);
    layer = lv_event_get_layer(event);
    lv_obj_get_coords(surface, &area);
    center_x = area.x1 + lv_area_get_width(&area) / 2;
    center_y = area.y1 + lv_area_get_height(&area) / 2;
    progress = lock_progress_percent;
    if(progress < 0)
        progress = 0;
    if(progress > 100)
        progress = 100;

    lv_draw_arc_dsc_init(&arc);
    arc.base.layer = layer;
    arc.center.x = center_x;
    arc.center.y = center_y;
    arc.radius = 33;
    arc.start_angle = 0;
    arc.end_angle = 359;
    arc.width = 2;
    arc.rounded = 1;
    arc.color = lv_color_hex(COLOR_WHITE);
    arc.opa = 64;
    lv_draw_arc(layer, &arc);

    if(progress > 0) {
        arc.start_angle = 270;
        arc.end_angle = 270 + progress * 360 / 100;
        arc.width = lock_opening ? 4 : 3;
        arc.color = lv_color_hex(
            lock_opening ? 0xB8FFE2 :
            lock_wrong_direction ? 0xFFB36B :
            progress >= 72 ? 0xFF739E : COLOR_CYAN);
        arc.opa = lock_opening ? 255 : 235;
        lv_draw_arc(layer, &arc);
        if(lock_opening) {
            arc.radius = 36;
            arc.width = 2;
            arc.opa = 58;
            lv_draw_arc(layer, &arc);
        }
    }
}

static void refresh_lock_progress(void)
{
    if(lock_progress_surface != NULL)
        lv_obj_invalidate(lock_progress_surface);
    if(lock_hint_label == NULL)
        return;
    if(lock_opening)
        lv_label_set_text(lock_hint_label, "UNLOCKED");
    else if(lock_wrong_direction)
        lv_label_set_text(lock_hint_label, "TURN CLOCKWISE");
    else if(lock_progress_percent > 0)
        lv_label_set_text(lock_hint_label, "KEEP TURNING CLOCKWISE");
    else
        lv_label_set_text(lock_hint_label, "TURN CLOCKWISE TO UNLOCK");
    if(!lock_opening && lock_icon_shackle != NULL) {
        int lift = lock_progress_percent * 2 / 100;
        lv_obj_set_pos(lock_icon_shackle, 12, 3 - lift);
        lv_obj_set_style_transform_rotation(
            lock_icon_shackle, 0, 0);
    }
    if(!lock_opening && lock_icon_body != NULL)
        lv_obj_set_style_bg_color(
            lock_icon_body,
            lv_color_hex(lock_wrong_direction
                ? 0xFFE1C7
                : lock_progress_percent >= 72
                    ? 0xFFE2EC : 0xDDF9FF),
            0);
}

static void reset_lock_wheel(void)
{
    lock_opening = false;
    lock_wrong_direction = false;
    lock_wheel_steps = 0;
    lock_wheel_decay_start_steps = 0;
    lock_wheel_last_input_tick = 0;
    lock_progress_percent = 0;
    if(lock_icon_shackle != NULL) {
        lv_obj_set_pos(lock_icon_shackle, 12, 3);
        lv_obj_set_style_transform_rotation(
            lock_icon_shackle, 0, 0);
    }
    if(lock_icon_body != NULL)
        lv_obj_set_style_bg_color(
            lock_icon_body, lv_color_hex(0xDDF9FF), 0);
    refresh_lock_progress();
}

static void show_lock_screen(bool turn_display_off)
{
    if(lock_screen == NULL)
        return;
    screen_locked = true;
    crazypod_coverflow_set_input_suspended(true);
    lock_release_guard = false;
    lock_wait_for_wake_release = turn_display_off;
    reset_lock_wheel();
    refresh_lock_appearance();
    refresh_lock_clock();
    lv_obj_remove_flag(lock_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(lock_screen);
    lv_obj_invalidate(lock_screen);
    lv_refr_now(NULL);
    crazypod_present_now();
    if(turn_display_off)
        backlight_off();
}

static void finish_unlock(void)
{
    screen_locked = false;
    lock_opening = false;
    lock_wrong_direction = false;
    lock_wheel_steps = 0;
    lock_wheel_decay_start_steps = 0;
    lock_wheel_last_input_tick = 0;
    lock_progress_percent = 0;
    lock_release_guard = true;
    lock_release_guard_until =
        current_tick + CRAZYPOD_UNLOCK_INPUT_GUARD_TICKS;
    lock_wait_for_wake_release = false;
    crazypod_coverflow_set_input_suspended(false);
    lv_obj_add_flag(lock_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(lock_screen);
    if(product_active)
        lv_obj_invalidate(product_screen);
    else
        lv_obj_invalidate(desktop_screen);
}

static void process_lock_state(void)
{
    bool backlight_is_on = is_backlight_on(false);

    if(lock_backlight_was_on && !backlight_is_on) {
        if(!screen_locked)
            show_lock_screen(false);
        else
            reset_lock_wheel();
        lock_wait_for_wake_release = false;
    }
    else if(!lock_backlight_was_on && backlight_is_on &&
            screen_locked) {
        lock_wait_for_wake_release = button_status() != BUTTON_NONE;
        reset_lock_wheel();
        refresh_lock_clock();
    }
    lock_backlight_was_on = backlight_is_on;

    if(!screen_locked)
        return;
    if(lock_wrong_direction &&
       !TIME_BEFORE(current_tick, lock_wrong_direction_until)) {
        lock_wrong_direction = false;
        refresh_lock_progress();
    }
    if(!lock_opening && lock_wheel_steps > 0 &&
       lock_wheel_last_input_tick != 0) {
        long idle = current_tick - lock_wheel_last_input_tick;

        if(idle > CRAZYPOD_UNLOCK_WHEEL_IDLE_TICKS) {
            long decay =
                idle - CRAZYPOD_UNLOCK_WHEEL_IDLE_TICKS;
            int steps;

            if(decay >= CRAZYPOD_UNLOCK_WHEEL_DECAY_TICKS)
                steps = 0;
            else
                steps = (int)(
                    lock_wheel_decay_start_steps *
                    (CRAZYPOD_UNLOCK_WHEEL_DECAY_TICKS - decay) /
                    CRAZYPOD_UNLOCK_WHEEL_DECAY_TICKS);
            if(steps != lock_wheel_steps) {
                lock_wheel_steps = steps;
                lock_progress_percent =
                    steps * 100 / CRAZYPOD_UNLOCK_WHEEL_STEPS;
                refresh_lock_progress();
            }
        }
    }
    if(lock_opening) {
        long elapsed = current_tick - lock_opening_start;
        int shift;

        if(elapsed < 0)
            elapsed = 0;
        shift = (int)(elapsed * 5 / CRAZYPOD_UNLOCK_OPEN_TICKS);
        if(shift > 5)
            shift = 5;
        if(lock_icon_shackle != NULL) {
            lv_obj_set_pos(lock_icon_shackle,
                           12 + shift, 1 - shift);
            lv_obj_set_style_transform_rotation(
                lock_icon_shackle, shift * 30, 0);
        }
        if(elapsed >= CRAZYPOD_UNLOCK_OPEN_TICKS)
            finish_unlock();
    }
}

static int lock_wheel_event_steps(intptr_t data)
{
    int steps = ((unsigned int)data >> 24) & 0x7f;

    if(steps < 1)
        steps = 1;
    if(steps > CRAZYPOD_UNLOCK_WHEEL_STEPS)
        steps = CRAZYPOD_UNLOCK_WHEEL_STEPS;
    return steps;
}

static long main_button_base(long button)
{
    return button & BUTTON_MAIN;
}

static bool handle_lock_button(long button, intptr_t data)
{
    long base;
    bool release;
    bool scroll;

    if(lock_release_guard) {
        base = main_button_base(button);
        if(TIME_BEFORE(current_tick, lock_release_guard_until) &&
           (base == BUTTON_SCROLL_FWD ||
            base == BUTTON_SCROLL_BACK))
            return true;
        lock_release_guard = false;
    }
    if(!screen_locked)
        return false;
    release = (button & BUTTON_REL) != 0;
    base = main_button_base(button);
    scroll = base == BUTTON_SCROLL_FWD ||
             base == BUTTON_SCROLL_BACK;

    if(release) {
        lock_wait_for_wake_release = false;
        return true;
    }
    backlight_on();
    if(!lock_backlight_was_on) {
        lock_backlight_was_on = true;
        /*
         * Click-wheel motion is posted directly to the button queue and
         * never produces BUTTON_REL. Consume only the wake event; do not
         * leave subsequent motion waiting for a release that cannot arrive.
         */
        lock_wait_for_wake_release = !scroll;
        reset_lock_wheel();
        return true;
    }
    if(lock_wait_for_wake_release) {
        if(scroll)
            lock_wait_for_wake_release = false;
        else
            return true;
    }
    if(lock_opening)
        return true;
    if(!scroll)
        return true;

    play_wheel_feedback(button);
    if(base == BUTTON_SCROLL_FWD) {
        lock_wrong_direction = false;
        lock_wheel_steps += lock_wheel_event_steps(data);
        if(lock_wheel_steps > CRAZYPOD_UNLOCK_WHEEL_STEPS)
            lock_wheel_steps = CRAZYPOD_UNLOCK_WHEEL_STEPS;
    }
    else {
        lock_wrong_direction = true;
        lock_wrong_direction_until =
            current_tick + CRAZYPOD_UNLOCK_DIRECTION_HINT_TICKS;
        lock_wheel_steps -= lock_wheel_event_steps(data);
        if(lock_wheel_steps < 0)
            lock_wheel_steps = 0;
    }
    lock_wheel_decay_start_steps = lock_wheel_steps;
    lock_wheel_last_input_tick = current_tick;
    lock_progress_percent =
        lock_wheel_steps * 100 / CRAZYPOD_UNLOCK_WHEEL_STEPS;
    if(lock_wheel_steps >= CRAZYPOD_UNLOCK_WHEEL_STEPS) {
        lock_opening = true;
        lock_opening_start = current_tick;
        lock_progress_percent = 100;
        if(lock_icon_body != NULL)
            lv_obj_set_style_bg_color(
                lock_icon_body, lv_color_hex(0xB8FFE2), 0);
    }
    refresh_lock_progress();
    return true;
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
    for(screen = 0; screen < CRAZYPOD_CORNER_SCREEN_COUNT; ++screen) {
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
    const lv_image_dsc_t *glass = NULL;

    if(desktop_capsule == NULL)
        return;
    if(crazypod_wallpaper_prepare_frosted_capsule(
           CRAZYPOD_GLASS_TINT_COLOR,
           CRAZYPOD_DESKTOP_CAPSULE_TINT_OPA))
        glass = crazypod_frosted_wallpaper_capsule();
    if(glass != NULL) {
        if(desktop_capsule_glass == NULL) {
            desktop_capsule_glass = lv_image_create(desktop_capsule);
            lv_obj_set_pos(desktop_capsule_glass, 0, 0);
            lv_obj_remove_flag(
                desktop_capsule_glass, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_move_to_index(desktop_capsule_glass, 0);
        }
        lv_image_set_src(desktop_capsule_glass, glass);
        lv_obj_set_style_image_opa(
            desktop_capsule_glass, LV_OPA_COVER, 0);
        lv_obj_remove_flag(desktop_capsule_glass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(desktop_capsule, LV_OPA_TRANSP, 0);
    }
    else {
        if(desktop_capsule_glass != NULL)
            lv_obj_add_flag(
                desktop_capsule_glass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(
            desktop_capsule, lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(
            desktop_capsule,
            CRAZYPOD_DESKTOP_CAPSULE_FALLBACK_OPA, 0);
    }
}

static void refresh_desktop_wave_ball_appearance(void)
{
    bool playing =
        (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
        (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    uint32_t primary = highlight_primary();
    uint32_t secondary = highlight_secondary();

    if(desktop_capsule_wave_ball != NULL) {
        lv_obj_set_style_shadow_width(
            desktop_capsule_wave_ball, playing ? 10 : 4, 0);
        lv_obj_set_style_shadow_color(
            desktop_capsule_wave_ball, lv_color_hex(primary), 0);
        lv_obj_set_style_shadow_opa(
            desktop_capsule_wave_ball, playing ? 112 : 34, 0);
    }
    if(desktop_capsule_wave_glow != NULL) {
        lv_obj_set_style_bg_color(
            desktop_capsule_wave_glow, lv_color_hex(primary), 0);
        lv_obj_set_style_bg_grad_color(
            desktop_capsule_wave_glow, lv_color_hex(secondary), 0);
        lv_obj_set_style_bg_grad_dir(
            desktop_capsule_wave_glow, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_bg_opa(
            desktop_capsule_wave_glow, playing ? 82 : 18, 0);
    }
    if(desktop_capsule_spectrum != NULL)
        lv_obj_invalidate(desktop_capsule_spectrum);
}

static void refresh_desktop_appearance(void)
{
    const lv_image_dsc_t *custom =
        crazypod_custom_home_wallpaper();

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
    refresh_desktop_capsule_material();
    refresh_desktop_wave_ball_appearance();
    desktop_native_backdrop_ready = false;
    layout_desktop_carousel(false);
    refresh_screen_corner_masks();
    refresh_lock_appearance();
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

static int now_playing_artwork_slot_for_track(
    const struct crazypod_track *track)
{
    return track != NULL &&
           strcmp(now_prefetch_track_path, track->path) == 0
        ? CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT
        : CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT;
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

    for(i = 0; i < CRAZYPOD_STATUS_BAR_COUNT; ++i) {
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
    if(screen_locked)
        refresh_lock_clock();
}

static void create_status_bar(lv_obj_t *screen, struct status_bar *bar)
{
    bar->time = make_label(screen, "00:00", &lv_font_montserrat_12,
                           COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(bar->time, 34, 10);

    bar->playing = make_label(screen, LV_SYMBOL_PLAY,
                              &lv_font_montserrat_10,
                              COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(bar->playing, 241, 11);
    lv_obj_add_flag(bar->playing, LV_OBJ_FLAG_HIDDEN);

    bar->battery = make_box(
        screen, 258, 11, 27, 12, 3, COLOR_WHITE, 64);
    bar->battery_fill = make_box(bar->battery, 1, 1, 24, 10, 2,
                                 COLOR_WHITE, LV_OPA_COVER);
    bar->charge = make_label(bar->battery, LV_SYMBOL_CHARGE,
                             &lv_font_montserrat_8,
                             COLOR_DETAIL, LV_OPA_COVER);
    lv_obj_center(bar->charge);

    bar->battery_cap = make_box(
        screen, 287, 15, 2, 5, 1, COLOR_WHITE, 128);
}

static void set_status_bar_palette(struct status_bar *bar,
                                   uint32_t foreground,
                                   uint32_t background)
{
    lv_obj_set_style_text_color(
        bar->time, lv_color_hex(foreground), 0);
    lv_obj_set_style_text_color(
        bar->playing, lv_color_hex(foreground), 0);
    lv_obj_set_style_bg_color(
        bar->battery, lv_color_hex(foreground), 0);
    lv_obj_set_style_bg_color(
        bar->battery_fill, lv_color_hex(foreground), 0);
    lv_obj_set_style_bg_color(
        bar->battery_cap, lv_color_hex(foreground), 0);
    lv_obj_set_style_text_color(
        bar->charge, lv_color_hex(background), 0);
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
    int visible_count = crazypod_apps_visible_count();
    struct crazypod_app *selected = visible_app(selected_app);
    int i;

    if(desktop_title != NULL && selected != NULL)
        lv_label_set_text(desktop_title, selected->name);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        int width = i == selected_app ? 14 : 5;
        if(desktop_indicators[i] == NULL)
            continue;
        if(i >= visible_count) {
            lv_obj_add_flag(desktop_indicators[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(desktop_indicators[i], LV_OBJ_FLAG_HIDDEN);
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
    crazypod_image_scale_rgb565(
        source_pixels, width, height, width,
        now_lyrics_cover_pixels[bank],
        CRAZYPOD_NOW_LYRICS_COVER_SIZE,
        CRAZYPOD_NOW_LYRICS_COVER_SIZE);
    crazypod_image_configure_rgb565(
        &now_lyrics_cover_descriptor[bank],
        now_lyrics_cover_pixels[bank],
        CRAZYPOD_NOW_LYRICS_COVER_SIZE,
        CRAZYPOD_NOW_LYRICS_COVER_SIZE);
    return true;
}

static void prepare_now_overlay_glass(bool refresh)
{
    const fb_data *framebuffer =
        (const fb_data *)lcd_framebuffer_default.data;

    keep_cpu_boosted(HZ / 2);
    /*
     * Capture the already-rendered Now Playing surface at quarter size.
     * The popup reuses this frozen material; no blur work runs while the
     * wheel is moving and the full 250x176 source never needs a second copy.
     */
    if(refresh)
        lv_refr_now(NULL);
    if(!crazypod_image_render_glass_rgb565(
           framebuffer, LCD_WIDTH, LCD_HEIGHT, LCD_WIDTH,
           CRAZYPOD_NOW_POPUP_X, CRAZYPOD_NOW_POPUP_Y,
           CRAZYPOD_NOW_POPUP_WIDTH, CRAZYPOD_NOW_POPUP_HEIGHT,
           CRAZYPOD_GLASS_TINT_COLOR,
           CRAZYPOD_GLASS_BAKE_TINT_OPA,
           now_glass_pixels, now_glass_scratch,
           sizeof(now_glass_pixels) / sizeof(now_glass_pixels[0]),
           now_glass_render_pixels,
           CRAZYPOD_NOW_POPUP_WIDTH, CRAZYPOD_NOW_POPUP_HEIGHT)) {
        now_glass_valid = false;
        return;
    }
    if(now_glass_descriptor.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&now_glass_descriptor);
    now_glass_valid = crazypod_image_configure_rgb565(
        &now_glass_descriptor, now_glass_render_pixels,
        CRAZYPOD_NOW_POPUP_WIDTH, CRAZYPOD_NOW_POPUP_HEIGHT);
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

    crazypod_image_scale_rgb565(
        now_backdrop_pixels,
        CRAZYPOD_NOW_BACKDROP_WIDTH, CRAZYPOD_NOW_BACKDROP_HEIGHT,
        CRAZYPOD_NOW_BACKDROP_WIDTH,
        now_backdrop_render_pixels[bank], LCD_WIDTH, LCD_HEIGHT);
    if(now_backdrop_descriptor[bank].header.magic ==
       LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&now_backdrop_descriptor[bank]);
    crazypod_image_configure_rgb565(
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
    lv_obj_t *surface = lv_event_get_target(event);
    lv_layer_t *layer;
    lv_area_t area;
    bool playing;

    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN)
        return;
    layer = lv_event_get_layer(event);
    lv_obj_get_coords(surface, &area);
    playing = (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
              (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    crazypod_sound_wave_draw_bar(
        layer, &area,
        (enum crazypod_sound_wave_style)
            crazypod_appearance_get()->sound_wave_style,
        now_wave_phase, playing,
        highlight_primary(), highlight_secondary());
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
    if(!playing) {
        if(now_wave_playing_seen) {
            now_wave_playing_seen = false;
            lv_obj_invalidate(now_wave_surface);
        }
        return;
    }
    if(TIME_BEFORE(current_tick,
                   last_now_wave_tick + CRAZYPOD_NOW_WAVE_FRAME_TICKS))
        return;
    last_now_wave_tick = current_tick;
    now_wave_playing_seen = true;
    now_wave_phase = (now_wave_phase + 1) & 0x7fff;
    lv_obj_invalidate(now_wave_surface);
}

static void draw_desktop_capsule_spectrum_event(lv_event_t *event)
{
    lv_obj_t *surface = lv_event_get_target(event);
    lv_layer_t *layer;
    lv_area_t area;
    bool playing;

    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN)
        return;

    layer = lv_event_get_layer(event);
    lv_obj_get_coords(surface, &area);
    playing = (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
              (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    crazypod_sound_wave_draw_ball(
        layer, &area,
        (enum crazypod_sound_wave_style)
            crazypod_appearance_get()->sound_wave_style,
        desktop_capsule_spectrum_phase, playing,
        highlight_primary(), highlight_secondary());
}

static void tick_desktop_capsule_spectrum(void)
{
    bool playing;

    if(product_active || desktop_capsule_spectrum == NULL)
        return;

    playing = (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
              (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    if(!playing) {
        if(desktop_capsule_spectrum_playing_seen) {
            desktop_capsule_spectrum_playing_seen = false;
            refresh_desktop_wave_ball_appearance();
            lv_obj_invalidate(desktop_capsule_spectrum);
        }
        return;
    }
    if(TIME_BEFORE(
           current_tick,
           last_desktop_capsule_spectrum_tick +
               CRAZYPOD_DESKTOP_SPECTRUM_FRAME_TICKS))
        return;

    last_desktop_capsule_spectrum_tick = current_tick;
    if(!desktop_capsule_spectrum_playing_seen) {
        desktop_capsule_spectrum_playing_seen = true;
        refresh_desktop_wave_ball_appearance();
    }
    desktop_capsule_spectrum_phase =
        (desktop_capsule_spectrum_phase + 1) & 0x7fff;
    lv_obj_invalidate(desktop_capsule_spectrum);
}

static uint32_t glass_material_tint(enum crazypod_glass_material material)
{
    (void)material;
    return CRAZYPOD_GLASS_TINT_COLOR;
}

static lv_opa_t glass_material_tint_opa(
    enum crazypod_glass_material material)
{
    (void)material;
    return CRAZYPOD_GLASS_PANEL_TINT_OPA;
}

static lv_opa_t glass_material_border_opa(
    enum crazypod_glass_material material)
{
    if(material == CRAZYPOD_GLASS_MENU_PANEL ||
       material == CRAZYPOD_GLASS_MENU_TOPBAR)
        return LV_OPA_TRANSP;
    return CRAZYPOD_GLASS_BORDER_OPA;
}

static lv_opa_t glass_material_shadow_opa(
    enum crazypod_glass_material material)
{
    if(material == CRAZYPOD_GLASS_MENU_PANEL ||
       material == CRAZYPOD_GLASS_MENU_TOPBAR)
        return LV_OPA_TRANSP;
    return CRAZYPOD_GLASS_SHADOW_OPA;
}

static bool render_glass_descriptor(
    const fb_data *source, int source_width, int source_height,
    int source_stride, int source_x, int source_y,
    int width, int height, enum crazypod_glass_material material,
    fb_data *render_pixels, lv_image_dsc_t *descriptor)
{
    if(width <= 0 || height <= 0 ||
       render_pixels == NULL || descriptor == NULL)
        return false;

    keep_cpu_boosted(HZ / 4);
    if(!crazypod_image_render_glass_rgb565(
           source, source_width, source_height, source_stride,
           source_x, source_y, width, height,
           glass_material_tint(material),
           CRAZYPOD_GLASS_BAKE_TINT_OPA,
           glass_sample_pixels, glass_sample_scratch,
           sizeof(glass_sample_pixels) /
               sizeof(glass_sample_pixels[0]),
           render_pixels, width, height))
        return false;
    if(descriptor->header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(descriptor);
    return crazypod_image_configure_rgb565(
        descriptor, render_pixels, width, height);
}

static void prepare_glass_descriptor(int source_x, int source_y,
                                     int width, int height,
                                     enum crazypod_glass_material material,
                                     fb_data *render_pixels,
                                     lv_image_dsc_t *descriptor)
{
    const fb_data *framebuffer =
        (const fb_data *)lcd_framebuffer_default.data;

    lv_refr_now(NULL);
    render_glass_descriptor(
        framebuffer, LCD_WIDTH, LCD_HEIGHT, LCD_WIDTH,
        source_x, source_y, width, height, material,
        render_pixels, descriptor);
}

static bool prepare_menu_glass_descriptor(
    int source_x, int source_y, int width, int height,
    enum crazypod_glass_material material,
    fb_data *render_pixels, lv_image_dsc_t *descriptor)
{
    const lv_image_dsc_t *wallpaper =
        crazypod_custom_menu_wallpaper();
    fb_data solid_pixel;
    const fb_data *source;
    int source_width;
    int source_height;
    int source_stride;

    if(wallpaper != NULL &&
       wallpaper->header.cf == LV_COLOR_FORMAT_RGB565 &&
       wallpaper->data != NULL &&
       wallpaper->header.stride % sizeof(fb_data) == 0) {
        source = (const fb_data *)wallpaper->data;
        source_width = wallpaper->header.w;
        source_height = wallpaper->header.h;
        source_stride =
            wallpaper->header.stride / sizeof(fb_data);
    }
    else {
        uint32_t color = crazypod_appearance_menu_color();

        solid_pixel = LCD_RGBPACK(
            (color >> 16) & 0xff,
            (color >> 8) & 0xff,
            color & 0xff);
        source = &solid_pixel;
        source_width = 1;
        source_height = 1;
        source_stride = 1;
        source_x = 0;
        source_y = 0;
    }

    return render_glass_descriptor(
        source, source_width, source_height, source_stride,
        source_x, source_y, width, height, material,
        render_pixels, descriptor);
}

static lv_obj_t *make_glass_material_panel(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, enum crazypod_glass_material material,
    const lv_image_dsc_t *descriptor)
{
    lv_obj_t *panel = make_box(
        parent, x, y, width, height,
        radius, COLOR_PANEL, LV_OPA_COVER);
    lv_obj_t *tint;
    lv_obj_t *border;
    lv_opa_t shadow_opa = glass_material_shadow_opa(material);

    lv_obj_set_style_clip_corner(panel, true, 0);
    if(shadow_opa > 0) {
        lv_obj_set_style_shadow_width(panel, 12, 0);
        lv_obj_set_style_shadow_offset_y(panel, 6, 0);
        lv_obj_set_style_shadow_color(panel, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(panel, shadow_opa, 0);
    }

    if(descriptor != NULL &&
       descriptor->header.magic == LV_IMAGE_HEADER_MAGIC) {
        lv_obj_t *image = lv_image_create(panel);
        lv_image_set_src(image, descriptor);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }

    tint = make_box(panel, 0, 0, width, height, radius,
                    glass_material_tint(material),
                    glass_material_tint_opa(material));
    lv_obj_remove_flag(tint, LV_OBJ_FLAG_CLICKABLE);
    border = make_box(panel, 0, 0, width, height, radius,
                      COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(border, 1, 0);
    lv_obj_set_style_border_color(
        border, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(
        border, glass_material_border_opa(material), 0);
    lv_obj_remove_flag(border, LV_OBJ_FLAG_CLICKABLE);
    return panel;
}

static lv_obj_t *make_glass_panel(lv_obj_t *parent, int x, int y,
                                  int width, int height)
{
    return make_glass_material_panel(
        parent, x, y, width, height, CRAZYPOD_NOW_POPUP_RADIUS,
        CRAZYPOD_GLASS_POPUP,
        now_glass_valid ? &now_glass_descriptor : NULL);
}

static lv_obj_t *make_now_glass_panel(int x, int y, int width, int height)
{
    return make_glass_panel(now_overlay_root, x, y, width, height);
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
    const struct crazypod_icon *image = crazypod_icon_get(app_index);
    const uint8_t *source = image != NULL ? image->pixels : NULL;
    fb_data *pixels = (fb_data *)lcd_framebuffer_default.data;
    int source_width;
    int source_height;
    int source_stride;
    int left = center_x - size / 2;
    int top = center_y - size / 2;
    int source_y_q16;
    int source_y_step;
    int y;

    if(image == NULL || source == NULL) {
        draw_desktop_placeholder(app_index, center_x, center_y,
                                 size, opacity);
        return;
    }
    source_width = image->width;
    source_height = image->height;
    source_stride = image->stride;
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

    if(product_active || modal_prompt_visible() || !desktop_native_dirty)
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
        for(i = 0; i < crazypod_apps_visible_count(); ++i) {
            int catalog_index =
                app_catalog_index(crazypod_apps_visible_id(i));
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
            if(catalog_index >= 0)
                draw_desktop_icon(catalog_index, center_x, center_y,
                                  icon_size, 255);
        }
    }

    crazypod_present_queue_rect(0, CRAZYPOD_DESKTOP_NATIVE_TOP, LCD_WIDTH,
                                CRAZYPOD_DESKTOP_NATIVE_HEIGHT);
    desktop_native_dirty = false;
}

static void layout_desktop_carousel(bool animated)
{
    update_desktop_selection_chrome();
    if(animated) {
        desktop_motion_active = true;
        desktop_motion_step_accumulator = 0;
        crazypod_frameclock_reset(&desktop_motion_clock, current_tick);
        keep_cpu_boosted(HZ / 4);
    }
    else {
        desktop_position_q8 = selected_app * 256;
        desktop_velocity_q8 = 0;
        desktop_motion_active = false;
        desktop_motion_step_accumulator = 0;
    }
    desktop_native_dirty = true;
}

static void schedule_next_desktop_motion_frame(long now)
{
    crazypod_frameclock_schedule_next(&desktop_motion_clock, now);
}

static bool advance_desktop_carousel_motion_step(void)
{
    int target;
    int delta;

    target = selected_app * 256;
    delta = target - desktop_position_q8;
    if(delta == 0 && desktop_velocity_q8 == 0) {
        desktop_motion_active = false;
        return false;
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
    return desktop_motion_active;
}

static void tick_desktop_carousel(void)
{
    if(!desktop_motion_active ||
       !crazypod_frameclock_due(&desktop_motion_clock, current_tick))
        return;

    desktop_motion_step_accumulator += CRAZYPOD_DESKTOP_MOTION_SIM_FPS;
    do {
        if(!advance_desktop_carousel_motion_step())
            break;
        desktop_motion_step_accumulator -= CRAZYPOD_TARGET_FPS;
    } while(desktop_motion_step_accumulator >= CRAZYPOD_TARGET_FPS);

    if(desktop_motion_active)
        schedule_next_desktop_motion_frame(current_tick);
    else
        desktop_motion_step_accumulator = 0;
}

static void app_focus_event(lv_event_t *event)
{
    struct crazypod_app *app = lv_event_get_user_data(event);
    int visible_index;

    if(lv_event_get_code(event) != LV_EVENT_FOCUSED)
        return;
    visible_index = crazypod_apps_visible_index(app->id);
    if(visible_index < 0)
        return;
    selected_app = visible_index;
    layout_desktop_carousel(true);
}

static void move_desktop_selection(int direction)
{
    int next = selected_app + direction;
    int count = crazypod_apps_visible_count();

    if(next < 0)
        next = 0;
    if(next >= count)
        next = count - 1;
    if(next == selected_app)
        return;
    selected_app = next;
    layout_desktop_carousel(true);
}

static void create_launcher_app(int index)
{
    struct crazypod_app *app = &apps[index];

    app->cell = lv_obj_create(desktop_carousel);
    set_plain_object(app->cell);
    lv_obj_set_size(app->cell, 120, 110);
    lv_obj_set_style_bg_opa(app->cell, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(app->cell, LV_OBJ_FLAG_CLICKABLE);

    lv_group_add_obj(desktop_group, app->cell);
    lv_obj_add_event_cb(app->cell, app_focus_event, LV_EVENT_FOCUSED, app);
}

static void create_now_playing_capsule(void)
{
    lv_obj_t *capsule;
    lv_obj_t *glass_border;
    lv_obj_t *progress_track;
    lv_obj_t *wave_ball;

    desktop_capsule = make_box(desktop_screen, 8, 174, 304, 58, 29,
                               COLOR_WHITE,
                               CRAZYPOD_DESKTOP_CAPSULE_FALLBACK_OPA);
    capsule = desktop_capsule;

    desktop_capsule_artwork = make_box(
        capsule, 17, 8, 42, 42, 9, 0x941FFC, LV_OPA_COVER);
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
        &lv_font_montserrat_16, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_center(desktop_capsule_artwork_symbol);
    desktop_capsule_artwork_path[0] = '\0';

    desktop_capsule_track = make_label(
        capsule, "No Track", CRAZYPOD_METADATA_FONT,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(desktop_capsule_track, 60, 7);
    lv_obj_set_width(desktop_capsule_track, 171);
    lv_obj_set_height(desktop_capsule_track, 17);
    lv_obj_set_style_text_align(
        desktop_capsule_track, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(desktop_capsule_track, LV_LABEL_LONG_MODE_DOTS);

    desktop_capsule_artist = make_label(
        capsule, "Local Music", CRAZYPOD_METADATA_FONT,
        COLOR_WHITE, 190);
    lv_obj_set_pos(desktop_capsule_artist, 60, 25);
    lv_obj_set_width(desktop_capsule_artist, 171);
    lv_obj_set_height(desktop_capsule_artist, 17);
    lv_obj_set_style_text_align(
        desktop_capsule_artist, LV_TEXT_ALIGN_CENTER, 0);
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

    wave_ball = make_box(capsule, 253, 8, 42, 42,
                         LV_RADIUS_CIRCLE, 0x080A14, LV_OPA_COVER);
    desktop_capsule_wave_ball = wave_ball;
    lv_obj_set_style_bg_grad_color(
        wave_ball, lv_color_hex(0x1A1F38), 0);
    lv_obj_set_style_bg_grad_dir(wave_ball, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_clip_corner(wave_ball, true, 0);
    lv_obj_set_style_border_width(wave_ball, 1, 0);
    lv_obj_set_style_border_color(
        wave_ball, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(wave_ball, 66, 0);
    desktop_capsule_wave_glow = make_box(
        wave_ball, 0, 0, 32, 32, LV_RADIUS_CIRCLE,
        highlight_primary(), 82);
    lv_obj_center(desktop_capsule_wave_glow);
    desktop_capsule_spectrum = lv_obj_create(wave_ball);
    set_plain_object(desktop_capsule_spectrum);
    lv_obj_set_size(desktop_capsule_spectrum, 42, 42);
    lv_obj_center(desktop_capsule_spectrum);
    lv_obj_remove_flag(desktop_capsule_spectrum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(desktop_capsule_spectrum,
                        draw_desktop_capsule_spectrum_event,
                        LV_EVENT_DRAW_MAIN, NULL);
    refresh_desktop_wave_ball_appearance();

    glass_border = make_box(capsule, 0, 0, 304, 58, 29,
                            COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(glass_border, 1, 0);
    lv_obj_set_style_border_color(
        glass_border, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(
        glass_border,
        glass_material_border_opa(CRAZYPOD_GLASS_HOME_CAPSULE), 0);
    lv_obj_remove_flag(glass_border, LV_OBJ_FLAG_CLICKABLE);
}

static void update_desktop_capsule_artwork(
    const struct crazypod_track *track)
{
    const lv_image_dsc_t *descriptor = NULL;
    enum crazypod_artwork_state state = CRAZYPOD_ARTWORK_EMPTY;

    if(desktop_capsule_artwork == NULL)
        return;

    if(track != NULL) {
        descriptor = crazypod_artwork_load(
            CRAZYPOD_CAPSULE_ARTWORK_SLOT, track,
            CRAZYPOD_CAPSULE_ARTWORK_SIZE);
        state = crazypod_artwork_state(
            CRAZYPOD_CAPSULE_ARTWORK_SLOT, track,
            CRAZYPOD_CAPSULE_ARTWORK_SIZE);
        (void)crazypod_artwork_load_priority(
            CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT, track,
            CRAZYPOD_NOW_ARTWORK_CACHE_SIZE, 8);
        if(state == CRAZYPOD_ARTWORK_PENDING)
            return;
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

    desktop_title = make_label(desktop_screen, visible_app(0)->name,
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
    lv_group_focus_obj(visible_app(0)->cell);
    create_screen_corner_masks(desktop_screen, 0);
}

static lv_obj_t *create_boot_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *logo;

    set_plain_object(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    logo = lv_image_create(screen);
    lv_image_set_src(logo, crazypod_lcd_boot_logo_image());
    lv_obj_center(logo);
    lv_obj_remove_flag(logo, LV_OBJ_FLAG_CLICKABLE);
    return screen;
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

static void create_lock_screen(void)
{
    lv_obj_t *scrim;
    lv_obj_t *halo;
    lv_obj_t *icon;
    lv_obj_t *keyhole;

    lock_screen = lv_obj_create(desktop_screen);
    set_plain_object(lock_screen);
    lv_obj_set_pos(lock_screen, 0, 0);
    lv_obj_set_size(lock_screen, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(lock_screen, LV_OPA_COVER, 0);

    lock_wallpaper = lv_image_create(lock_screen);
    lv_obj_set_pos(lock_wallpaper, 0, 0);
    lv_obj_remove_flag(lock_wallpaper, LV_OBJ_FLAG_CLICKABLE);
    scrim = make_box(lock_screen, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
                     0x03050A, 132);
    lv_obj_remove_flag(scrim, LV_OBJ_FLAG_CLICKABLE);

    lock_time_label = make_label(
        lock_screen, "09:41", &lv_font_montserrat_48,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(lock_time_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lock_time_label, 0, 27);

    lock_date_label = make_label(
        lock_screen, "", &lv_font_montserrat_10,
        0xD6E2EA, 184);
    lv_obj_set_width(lock_date_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lock_date_label, 0, 87);

    lock_progress_surface = lv_obj_create(lock_screen);
    set_plain_object(lock_progress_surface);
    lv_obj_set_pos(lock_progress_surface, 124, 111);
    lv_obj_set_size(lock_progress_surface, 72, 72);
    lv_obj_remove_flag(
        lock_progress_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        lock_progress_surface, draw_lock_progress_event,
        LV_EVENT_DRAW_MAIN, NULL);
    halo = make_box(lock_progress_surface, 10, 10, 52, 52,
                    LV_RADIUS_CIRCLE, COLOR_CYAN, 12);
    lv_obj_set_style_shadow_width(halo, 12, 0);
    lv_obj_set_style_shadow_color(
        halo, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_shadow_opa(halo, 38, 0);
    icon = lv_obj_create(lock_progress_surface);
    set_plain_object(icon);
    lv_obj_set_pos(icon, 14, 12);
    lv_obj_set_size(icon, 44, 48);
    lock_icon_shackle = make_box(
        icon, 12, 3, 20, 23, 10, COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(lock_icon_shackle, 3, 0);
    lv_obj_set_style_border_color(
        lock_icon_shackle, lv_color_hex(0xDDF9FF), 0);
    lv_obj_set_style_border_opa(
        lock_icon_shackle, LV_OPA_COVER, 0);
    lock_icon_body = make_box(
        icon, 6, 19, 32, 23, 7, 0xDDF9FF, LV_OPA_COVER);
    keyhole = make_box(lock_icon_body, 14, 7, 5, 8,
                       LV_RADIUS_CIRCLE, 0x0A1620, 225);
    make_box(keyhole, 2, 4, 1, 6, 0,
             0x0A1620, LV_OPA_COVER);

    lock_hint_label = make_label(
        lock_screen, "TURN CLOCKWISE TO UNLOCK",
        &lv_font_montserrat_8, COLOR_WHITE, 135);
    lv_obj_set_width(lock_hint_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lock_hint_label, 0, 201);
    lv_obj_set_style_text_letter_space(lock_hint_label, 1, 0);

    refresh_lock_appearance();
    refresh_lock_clock();
    reset_lock_wheel();
    create_screen_corner_masks(lock_screen, 2);
    lv_obj_add_flag(lock_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(lock_screen);
}

static struct route_state *current_route(void)
{
    return &route_stack[route_depth - 1];
}

static bool is_settings_route(enum crazypod_route route)
{
    return route == SETTINGS_ROUTE_MENU ||
           route == SETTINGS_ROUTE_SOUND ||
           route == SETTINGS_ROUTE_EQ_STUDIO ||
           route == SETTINGS_ROUTE_DISPLAY ||
           route == SETTINGS_ROUTE_PLAYBACK ||
           route == SETTINGS_ROUTE_POWER ||
           route == SETTINGS_ROUTE_CONTROLS ||
           route == SETTINGS_ROUTE_MAIN_MENU ||
           route == SETTINGS_ROUTE_MAIN_MENU_ACTIONS;
}

static int array_count_int(const int *items, int bytes)
{
    (void)items;
    return bytes / (int)sizeof(int);
}

static int settings_route_item_count(enum crazypod_route route)
{
    switch(route) {
    case SETTINGS_ROUTE_MENU:
        return (int)(sizeof(settings_menu_titles) /
                     sizeof(settings_menu_titles[0]));
    case SETTINGS_ROUTE_SOUND:
        return array_count_int(settings_sound_items,
                               sizeof(settings_sound_items));
    case SETTINGS_ROUTE_EQ_STUDIO:
        return EQ_NUM_BANDS;
    case SETTINGS_ROUTE_DISPLAY:
        return array_count_int(settings_display_items,
                               sizeof(settings_display_items));
    case SETTINGS_ROUTE_PLAYBACK:
        return array_count_int(settings_playback_items,
                               sizeof(settings_playback_items));
    case SETTINGS_ROUTE_POWER:
        return array_count_int(settings_power_items,
                               sizeof(settings_power_items));
    case SETTINGS_ROUTE_CONTROLS:
        return array_count_int(settings_controls_items,
                               sizeof(settings_controls_items));
    case SETTINGS_ROUTE_MAIN_MENU:
        return crazypod_apps_count();
    default:
        return 0;
    }
}

static int settings_route_item(enum crazypod_route route, int index)
{
    const int *items = NULL;
    int count = 0;

    switch(route) {
    case SETTINGS_ROUTE_SOUND:
        items = settings_sound_items;
        count = array_count_int(settings_sound_items,
                                sizeof(settings_sound_items));
        break;
    case SETTINGS_ROUTE_DISPLAY:
        items = settings_display_items;
        count = array_count_int(settings_display_items,
                                sizeof(settings_display_items));
        break;
    case SETTINGS_ROUTE_PLAYBACK:
        items = settings_playback_items;
        count = array_count_int(settings_playback_items,
                                sizeof(settings_playback_items));
        break;
    case SETTINGS_ROUTE_POWER:
        items = settings_power_items;
        count = array_count_int(settings_power_items,
                                sizeof(settings_power_items));
        break;
    case SETTINGS_ROUTE_CONTROLS:
        items = settings_controls_items;
        count = array_count_int(settings_controls_items,
                                sizeof(settings_controls_items));
        break;
    default:
        break;
    }
    return items != NULL && index >= 0 && index < count
        ? items[index] : -1;
}

static const struct crazypod_miniapp_metadata *
miniapp_metadata(int index)
{
    return crazypod_miniapps_metadata(index);
}

static const char *miniapp_symbol(int index)
{
    const struct crazypod_miniapp_metadata *metadata =
        miniapp_metadata(index);

    return metadata != NULL && metadata->symbol[0] != '\0'
        ? metadata->symbol : LV_SYMBOL_FILE;
}

static int notes_home_note_start(void)
{
    return note_draft_available ? 2 : 1;
}

static int notes_home_deleted_index(void)
{
    return notes_home_note_start() + crazypod_notes_count(false) + 1;
}

static int notes_home_search_index(void)
{
    return notes_home_deleted_index() - 1;
}

static const struct crazypod_note *notes_home_note(int index)
{
    int note_index = index - notes_home_note_start();
    return note_index >= 0 ? crazypod_note_get(false, note_index) : NULL;
}

static int utf8_character_size(const char *text)
{
    unsigned char value = (unsigned char)text[0];

    if((value & 0x80) == 0)
        return 1;
    if((value & 0xe0) == 0xc0 && text[1] != '\0')
        return 2;
    if((value & 0xf0) == 0xe0 && text[1] != '\0' &&
       text[2] != '\0')
        return 3;
    if((value & 0xf8) == 0xf0 && text[1] != '\0' &&
       text[2] != '\0' && text[3] != '\0')
        return 4;
    return 1;
}

static int note_body_line_count(const char *body)
{
    const char *cursor = body;
    int column = 0;
    int lines = 1;

    while(cursor != NULL && *cursor != '\0') {
        int bytes;
        if(*cursor == '\n') {
            ++lines;
            column = 0;
            ++cursor;
            continue;
        }
        bytes = utf8_character_size(cursor);
        cursor += bytes;
        ++column;
        if(column >= 34) {
            ++lines;
            column = 0;
        }
    }
    return lines;
}

static void format_note_body_window(const char *body, int first_line,
                                    char *output, size_t size)
{
    const char *cursor = body;
    size_t used = 0;
    int line = 0;
    int column = 0;

    if(size == 0)
        return;
    while(cursor != NULL && *cursor != '\0' && line < first_line + 9) {
        int bytes;
        bool newline = *cursor == '\n';

        if(newline) {
            bytes = 1;
        }
        else {
            bytes = utf8_character_size(cursor);
        }
        if(line >= first_line) {
            int i;
            for(i = 0; i < bytes && used + 1 < size; ++i)
                output[used++] = cursor[i];
        }
        cursor += bytes;
        if(newline || ++column >= 34) {
            if(!newline && line >= first_line && used + 1 < size)
                output[used++] = '\n';
            ++line;
            column = 0;
        }
    }
    output[used] = '\0';
}

static const char *note_text_with_cursor(const char *text, size_t cursor,
                                         char *output, size_t size)
{
    size_t length;

    if(output == NULL || size < 2)
        return text;
    length = strlen(text);
    if(cursor > length)
        cursor = length;
    if(length + 2 > size)
        length = size - 2;
    if(cursor > length)
        cursor = length;
    memcpy(output, text, cursor);
    output[cursor] = '|';
    memcpy(output + cursor + 1, text + cursor, length - cursor);
    output[length + 1] = '\0';
    return output;
}

static bool books_has_continue(void)
{
    int index = crazypod_books_recent_index();
    const struct crazypod_book *book = crazypod_book_get(index);
    return book != NULL && book->progress > 0;
}

static int books_route_book_index(const struct route_state *state,
                                  int position)
{
    if(state->route == BOOKS_ROUTE_LIBRARY)
        return position;
    if(state->route == BOOKS_ROUTE_RECENTS)
        return crazypod_books_recent_at(position);
    if(state->route == BOOKS_ROUTE_FAVORITES)
        return crazypod_books_favorite_at(position);
    return state->group;
}

static bool is_podcast_path(const char *path)
{
    return path != NULL &&
           (strstr(path, "/Podcasts/") != NULL ||
            strstr(path, "/podcasts/") != NULL);
}

static int podcast_count(void)
{
    int count = 0;
    int i;
    for(i = 0; i < crazypod_music_track_count(); ++i) {
        const struct crazypod_track *track = crazypod_music_track(i);
        if(track != NULL && is_podcast_path(track->path))
            ++count;
    }
    return count;
}

static int podcast_track_index(int position)
{
    int visible = 0;
    int i;
    for(i = 0; i < crazypod_music_track_count(); ++i) {
        const struct crazypod_track *track = crazypod_music_track(i);
        if(track != NULL && is_podcast_path(track->path) &&
           visible++ == position)
            return i;
    }
    return -1;
}

static int days_in_month(int year, int month);
static bool note_editor_dirty(void);

static int calendar_focus_date(void)
{
    return calendar_focus_year * 10000 +
           (calendar_focus_month + 1) * 100 +
           calendar_focus_day;
}

static int calendar_event_index_on_focus(int position)
{
    int visible = 0;
    int i;
    int date = calendar_focus_date();

    for(i = 0; i < crazypod_calendar_event_count(); ++i) {
        const struct crazypod_calendar_event *event =
            crazypod_calendar_event_get(i);
        if(event != NULL && event->date == date &&
           visible++ == position)
            return i;
    }
    return -1;
}

static int calendar_today_date(void)
{
    struct tm *now = get_time();

    return (now->tm_year + 1900) * 10000 +
           (now->tm_mon + 1) * 100 + now->tm_mday;
}

static int calendar_upcoming_event_index(int position)
{
    int visible = 0;
    int today = calendar_today_date();
    int i;

    for(i = 0; i < crazypod_calendar_event_count(); ++i) {
        const struct crazypod_calendar_event *event =
            crazypod_calendar_event_get(i);
        if(event != NULL && event->date >= today &&
           visible++ == position)
            return i;
    }
    return -1;
}

static int calendar_route_event_index(
    const struct route_state *state, int position)
{
    if(state->route == CALENDAR_ROUTE_UPCOMING)
        return calendar_upcoming_event_index(position);
    if(state->route == CALENDAR_ROUTE_TODAY) {
        int saved_year = calendar_focus_year;
        int saved_month = calendar_focus_month;
        int saved_day = calendar_focus_day;
        int today = calendar_today_date();
        int result;

        calendar_focus_year = today / 10000;
        calendar_focus_month = today / 100 % 100 - 1;
        calendar_focus_day = today % 100;
        result = calendar_event_index_on_focus(position);
        calendar_focus_year = saved_year;
        calendar_focus_month = saved_month;
        calendar_focus_day = saved_day;
        return result;
    }
    if(state->route == CALENDAR_ROUTE_DAY_EVENTS)
        return calendar_event_index_on_focus(position);
    return -1;
}

static int calendar_route_event_count(const struct route_state *state)
{
    int count = 0;

    while(calendar_route_event_index(state, count) >= 0)
        ++count;
    return count;
}

static int calendar_shifted_date(int date, int direction)
{
    int year = date / 10000;
    int month = date / 100 % 100 - 1;
    int day = date % 100;

    if(direction > 0) {
        ++day;
        if(day > days_in_month(year, month)) {
            day = 1;
            if(++month > 11) {
                month = 0;
                ++year;
            }
        }
    }
    else if(direction < 0) {
        if(--day < 1) {
            if(--month < 0) {
                month = 11;
                --year;
            }
            day = days_in_month(year, month);
        }
    }
    return year * 10000 + (month + 1) * 100 + day;
}

static int calendar_parse_minutes(const char *time)
{
    if(time == NULL || strlen(time) < 5 || time[2] != ':')
        return -1;
    return ((time[0] - '0') * 10 + time[1] - '0') * 60 +
           (time[3] - '0') * 10 + time[4] - '0';
}

static void calendar_format_time(char *buffer, size_t size, int minutes)
{
    if(minutes < 0)
        buffer[0] = '\0';
    else
        snprintf(buffer, size, "%02d:%02d",
                 minutes / 60, minutes % 60);
}

static const char *settings_item_title(int item)
{
    switch(item) {
    case SETTINGS_ITEM_EQ_ENABLED: return "Equalizer";
    case SETTINGS_ITEM_BASS: return "Bass";
    case SETTINGS_ITEM_TREBLE: return "Treble";
    case SETTINGS_ITEM_BALANCE: return "Balance";
    case SETTINGS_ITEM_BRIGHTNESS: return "Brightness";
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT: return "Backlight";
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED: return "Charging Light";
    case SETTINGS_ITEM_LCD_SLEEP: return "LCD Sleep";
    case SETTINGS_ITEM_REDUCE_MOTION: return "Reduce Motion";
    case SETTINGS_ITEM_SHUFFLE: return "Shuffle";
    case SETTINGS_ITEM_REPEAT: return "Repeat";
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION: return "Sleep Timer";
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP: return "Timer on Boot";
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS: return "Key Reset Timer";
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING: return "USB Charging";
#endif
    case SETTINGS_ITEM_BEEP: return "System Beep";
    case SETTINGS_ITEM_KEYCLICK: return "Keyclick";
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK: return "Speaker Click";
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS: return "Repeat Clicks";
    default: return "";
    }
}

static const char *settings_item_symbol(int item)
{
    switch(item) {
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_BASS:
    case SETTINGS_ITEM_TREBLE:
    case SETTINGS_ITEM_BALANCE:
        return LV_SYMBOL_AUDIO;
    case SETTINGS_ITEM_BRIGHTNESS:
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
    case SETTINGS_ITEM_LCD_SLEEP:
        return LV_SYMBOL_EYE_OPEN;
    case SETTINGS_ITEM_REDUCE_MOTION:
        return LV_SYMBOL_EYE_CLOSE;
    case SETTINGS_ITEM_SHUFFLE:
        return LV_SYMBOL_SHUFFLE;
    case SETTINGS_ITEM_REPEAT:
        return LV_SYMBOL_LOOP;
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
#endif
        return LV_SYMBOL_POWER;
    default:
        return LV_SYMBOL_SETTINGS;
    }
}

static const char *settings_group_detail(int index)
{
    switch(index) {
    case 0: return "EQ, tone and balance";
    case 1: return "Backlight, sleep and brightness";
    case 2: return "Shuffle and repeat";
    case 3: return "Charging and sleep timer";
    case 4: return "Beeps and wheel feedback";
    case 5: return "Reorder or hide entries";
    default: return "";
    }
}

static const char *format_bool_value(bool value)
{
    return value ? "On" : "Off";
}

static const char *format_timeout_value(int value)
{
    static char text[20];

    if(value < 0)
        return "Never";
    if(value == 0)
        return "Always";
    if(value >= 60 && value % 60 == 0)
        snprintf(text, sizeof(text), "%d min", value / 60);
    else
        snprintf(text, sizeof(text), "%d sec", value);
    return text;
}

static const char *format_sleep_timer_value(int value)
{
    static char text[20];

    if(value <= 0)
        return "Off";
    if(value >= 60 && value % 60 == 0)
        snprintf(text, sizeof(text), "%d hr", value / 60);
    else
        snprintf(text, sizeof(text), "%d min", value);
    return text;
}

static int range_choice_count(int minimum, int maximum, int step)
{
    if(step <= 0)
        step = 1;
    if(maximum < minimum)
        return 0;
    return (maximum - minimum) / step + 1;
}

static int range_choice_index(int value, int minimum, int maximum, int step)
{
    int index;
    int count = range_choice_count(minimum, maximum, step);

    if(count <= 0)
        return 0;
    if(value < minimum)
        value = minimum;
    if(value > maximum)
        value = maximum;
    if(step <= 0)
        step = 1;
    index = (value - minimum + step / 2) / step;
    if(index < 0)
        index = 0;
    if(index >= count)
        index = count - 1;
    return index;
}

static int range_choice_value(int index, int minimum, int maximum, int step)
{
    int value;

    if(step <= 0)
        step = 1;
    value = minimum + index * step;
    if(value > maximum)
        value = maximum;
    return value;
}

static int setting_sound_step(int setting)
{
    int step = sound_steps(setting);

    return step > 0 ? step : 1;
}

static int settings_item_current_value(int item)
{
    switch(item) {
    case SETTINGS_ITEM_EQ_ENABLED:
        return global_settings.eq_enabled ? 1 : 0;
    case SETTINGS_ITEM_BASS:
        return global_settings.bass;
    case SETTINGS_ITEM_TREBLE:
        return global_settings.treble;
    case SETTINGS_ITEM_BALANCE:
        return global_settings.balance;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        return global_settings.brightness;
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
        return global_settings.backlight_timeout;
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
        return global_settings.backlight_timeout_plugged;
#endif
    case SETTINGS_ITEM_LCD_SLEEP:
        return global_settings.lcd_sleep_after_backlight_off;
    case SETTINGS_ITEM_REDUCE_MOTION:
        return crazypod_state_reduce_motion() ? 1 : 0;
    case SETTINGS_ITEM_SHUFFLE:
        return global_settings.playlist_shuffle ? 1 : 0;
    case SETTINGS_ITEM_REPEAT:
        return crazypod_queue_repeat();
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return global_settings.sleeptimer_duration;
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
        return global_settings.sleeptimer_on_startup ? 1 : 0;
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
        return global_settings.keypress_restarts_sleeptimer ? 1 : 0;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        return global_settings.usb_charging;
#endif
    case SETTINGS_ITEM_BEEP:
        return global_settings.beep;
    case SETTINGS_ITEM_KEYCLICK:
        return global_settings.keyclick;
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
        return global_settings.keyclick_hardware ? 1 : 0;
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return global_settings.keyclick_repeats ? 1 : 0;
    default:
        return 0;
    }
}

static int settings_choice_count(int item)
{
    switch(item) {
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_REDUCE_MOTION:
    case SETTINGS_ITEM_SHUFFLE:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return 2;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        return 3;
#endif
    case SETTINGS_ITEM_BASS:
        return range_choice_count(sound_min(SOUND_BASS),
                                  sound_max(SOUND_BASS),
                                  setting_sound_step(SOUND_BASS));
    case SETTINGS_ITEM_TREBLE:
        return range_choice_count(sound_min(SOUND_TREBLE),
                                  sound_max(SOUND_TREBLE),
                                  setting_sound_step(SOUND_TREBLE));
    case SETTINGS_ITEM_BALANCE:
        return range_choice_count(sound_min(SOUND_BALANCE),
                                  sound_max(SOUND_BALANCE),
                                  setting_sound_step(SOUND_BALANCE));
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        return range_choice_count(MIN_BRIGHTNESS_SETTING,
                                  MAX_BRIGHTNESS_SETTING, 1);
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
#endif
        return (int)(sizeof(setting_timeout_values) /
                     sizeof(setting_timeout_values[0]));
    case SETTINGS_ITEM_LCD_SLEEP:
        return (int)(sizeof(setting_lcd_sleep_values) /
                     sizeof(setting_lcd_sleep_values[0]));
    case SETTINGS_ITEM_REPEAT:
        return (int)(sizeof(setting_repeat_values) /
                     sizeof(setting_repeat_values[0]));
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return (int)(sizeof(setting_sleep_timer_values) /
                     sizeof(setting_sleep_timer_values[0]));
    case SETTINGS_ITEM_BEEP:
    case SETTINGS_ITEM_KEYCLICK:
        return 4;
    default:
        return 0;
    }
}

static int settings_choice_value(int item, int index)
{
    switch(item) {
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_REDUCE_MOTION:
    case SETTINGS_ITEM_SHUFFLE:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return index > 0 ? 1 : 0;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        if(index < USB_CHARGING_DISABLE)
            return USB_CHARGING_DISABLE;
        if(index > USB_CHARGING_FORCE)
            return USB_CHARGING_FORCE;
        return index;
#endif
    case SETTINGS_ITEM_BASS:
        return range_choice_value(index, sound_min(SOUND_BASS),
                                  sound_max(SOUND_BASS),
                                  setting_sound_step(SOUND_BASS));
    case SETTINGS_ITEM_TREBLE:
        return range_choice_value(index, sound_min(SOUND_TREBLE),
                                  sound_max(SOUND_TREBLE),
                                  setting_sound_step(SOUND_TREBLE));
    case SETTINGS_ITEM_BALANCE:
        return range_choice_value(index, sound_min(SOUND_BALANCE),
                                  sound_max(SOUND_BALANCE),
                                  setting_sound_step(SOUND_BALANCE));
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        return range_choice_value(index, MIN_BRIGHTNESS_SETTING,
                                  MAX_BRIGHTNESS_SETTING, 1);
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
#endif
        return setting_timeout_values[index];
    case SETTINGS_ITEM_LCD_SLEEP:
        return setting_lcd_sleep_values[index];
    case SETTINGS_ITEM_REPEAT:
        return setting_repeat_values[index];
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return setting_sleep_timer_values[index];
    case SETTINGS_ITEM_BEEP:
    case SETTINGS_ITEM_KEYCLICK:
        return index;
    default:
        return 0;
    }
}

static int find_value_index(const int *values, int count, int value)
{
    int index;

    for(index = 0; index < count; ++index) {
        if(values[index] == value)
            return index;
    }
    return 0;
}

static int settings_choice_index(int item)
{
    int current = settings_item_current_value(item);

    switch(item) {
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_REDUCE_MOTION:
    case SETTINGS_ITEM_SHUFFLE:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return current ? 1 : 0;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        if(current < USB_CHARGING_DISABLE)
            return USB_CHARGING_DISABLE;
        if(current > USB_CHARGING_FORCE)
            return USB_CHARGING_FORCE;
        return current;
#endif
    case SETTINGS_ITEM_BASS:
        return range_choice_index(current, sound_min(SOUND_BASS),
                                  sound_max(SOUND_BASS),
                                  setting_sound_step(SOUND_BASS));
    case SETTINGS_ITEM_TREBLE:
        return range_choice_index(current, sound_min(SOUND_TREBLE),
                                  sound_max(SOUND_TREBLE),
                                  setting_sound_step(SOUND_TREBLE));
    case SETTINGS_ITEM_BALANCE:
        return range_choice_index(current, sound_min(SOUND_BALANCE),
                                  sound_max(SOUND_BALANCE),
                                  setting_sound_step(SOUND_BALANCE));
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        return range_choice_index(current, MIN_BRIGHTNESS_SETTING,
                                  MAX_BRIGHTNESS_SETTING, 1);
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
#endif
        return find_value_index(
            setting_timeout_values,
            (int)(sizeof(setting_timeout_values) /
                  sizeof(setting_timeout_values[0])),
            current);
    case SETTINGS_ITEM_LCD_SLEEP:
        return find_value_index(
            setting_lcd_sleep_values,
            (int)(sizeof(setting_lcd_sleep_values) /
                  sizeof(setting_lcd_sleep_values[0])),
            current);
    case SETTINGS_ITEM_REPEAT:
        return find_value_index(
            setting_repeat_values,
            (int)(sizeof(setting_repeat_values) /
                  sizeof(setting_repeat_values[0])),
            current);
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return find_value_index(
            setting_sleep_timer_values,
            (int)(sizeof(setting_sleep_timer_values) /
                  sizeof(setting_sleep_timer_values[0])),
            current);
    case SETTINGS_ITEM_BEEP:
    case SETTINGS_ITEM_KEYCLICK:
        if(current < 0)
            return 0;
        return current > 3 ? 3 : current;
    default:
        return 0;
    }
}

static const char *settings_repeat_title(int value)
{
    switch(value) {
    case REPEAT_ALL: return "All";
    case REPEAT_ONE: return "One";
    default: return "Off";
    }
}

static const char *settings_level_title(int value)
{
    switch(value) {
    case 1: return "Weak";
    case 2: return "Moderate";
    case 3: return "Strong";
    default: return "Off";
    }
}

#ifdef HAVE_USB_CHARGING_ENABLE
static const char *settings_usb_charging_title(int value)
{
    switch(value) {
    case USB_CHARGING_ENABLE:
        return "On";
    case USB_CHARGING_FORCE:
        return "Force";
    default:
        return "Off";
    }
}
#endif

static const char *settings_choice_title(int item, int index)
{
    static char text[24];
    int value = settings_choice_value(item, index);

    switch(item) {
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_REDUCE_MOTION:
    case SETTINGS_ITEM_SHUFFLE:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return format_bool_value(value != 0);
    case SETTINGS_ITEM_BASS:
    case SETTINGS_ITEM_TREBLE:
        snprintf(text, sizeof(text), "%+d dB", value);
        return text;
    case SETTINGS_ITEM_BALANCE:
        if(value < 0)
            snprintf(text, sizeof(text), "Left %d%%", -value);
        else if(value > 0)
            snprintf(text, sizeof(text), "Right %d%%", value);
        else
            snprintf(text, sizeof(text), "Center");
        return text;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        snprintf(text, sizeof(text), "%d", value);
        return text;
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
#endif
    case SETTINGS_ITEM_LCD_SLEEP:
        return format_timeout_value(value);
    case SETTINGS_ITEM_REPEAT:
        return settings_repeat_title(value);
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return format_sleep_timer_value(value);
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        return settings_usb_charging_title(value);
#endif
    case SETTINGS_ITEM_BEEP:
    case SETTINGS_ITEM_KEYCLICK:
        return settings_level_title(value);
    default:
        return "";
    }
}

static const char *settings_item_value_label(int item)
{
    return settings_choice_title(item, settings_choice_index(item));
}

static void settings_apply_choice(int item, int index)
{
    int value = settings_choice_value(item, index);

    switch(item) {
    case SETTINGS_ITEM_EQ_ENABLED:
        global_settings.eq_enabled = value != 0;
        crazypod_eq_settings_apply();
        break;
    case SETTINGS_ITEM_BASS:
        global_settings.bass = value;
        sound_set(SOUND_BASS, global_settings.bass);
        break;
    case SETTINGS_ITEM_TREBLE:
        global_settings.treble = value;
        sound_set(SOUND_TREBLE, global_settings.treble);
        break;
    case SETTINGS_ITEM_BALANCE:
        global_settings.balance = value;
        sound_set(SOUND_BALANCE, global_settings.balance);
        break;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        global_settings.brightness = value;
        backlight_set_brightness(global_settings.brightness);
        break;
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
        global_settings.backlight_timeout = value;
        backlight_set_timeout(global_settings.backlight_timeout);
        break;
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
        global_settings.backlight_timeout_plugged = value;
        backlight_set_timeout_plugged(
            global_settings.backlight_timeout_plugged);
        break;
#endif
    case SETTINGS_ITEM_LCD_SLEEP:
        global_settings.lcd_sleep_after_backlight_off = value;
        lcd_set_sleep_after_backlight_off(
            global_settings.lcd_sleep_after_backlight_off);
        break;
    case SETTINGS_ITEM_REDUCE_MOTION:
        crazypod_state_set_reduce_motion(value != 0);
        break;
    case SETTINGS_ITEM_SHUFFLE:
        crazypod_queue_set_shuffle(value != 0);
        crazypod_state_mark_dirty();
        break;
    case SETTINGS_ITEM_REPEAT:
        crazypod_queue_set_repeat(value);
        crazypod_state_mark_dirty();
        break;
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        global_settings.sleeptimer_duration = value;
        set_sleeptimer_duration(global_settings.sleeptimer_duration);
        break;
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
        global_settings.sleeptimer_on_startup = value != 0;
        break;
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
        global_settings.keypress_restarts_sleeptimer = value != 0;
        set_keypress_restarts_sleep_timer(
            global_settings.keypress_restarts_sleeptimer);
        break;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        global_settings.usb_charging = value;
        usb_charging_enable(global_settings.usb_charging);
        break;
#endif
    case SETTINGS_ITEM_BEEP:
        global_settings.beep = value;
        break;
    case SETTINGS_ITEM_KEYCLICK:
        global_settings.keyclick = value;
        break;
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
        global_settings.keyclick_hardware = value != 0;
        break;
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        global_settings.keyclick_repeats = value != 0;
        break;
    default:
        break;
    }
    crazypod_state_mark_dirty();
    crazypod_state_save(false);
}

static int clamp_value(int value, int minimum, int maximum)
{
    if(value < minimum)
        return minimum;
    if(value > maximum)
        return maximum;
    return value;
}

static const char *eq_mode_title(enum eq_studio_mode mode)
{
    switch(mode) {
    case EQ_STUDIO_CUTOFF: return "Freq";
    case EQ_STUDIO_Q: return "Q";
    case EQ_STUDIO_PRECUT: return "Precut";
    default: return "Gain";
    }
}

static const char *eq_band_role(int band)
{
    if(band <= 1)
        return "Sub Bass";
    if(band <= 3)
        return "Low Mid";
    if(band <= 5)
        return "Presence";
    if(band <= 7)
        return "Air Detail";
    return "Top End";
}

static void format_eq_db(char *buffer, size_t size, int value)
{
    int abs_value = value < 0 ? -value : value;

    snprintf(buffer, size, "%c%d.%d dB",
             value < 0 ? '-' : '+',
             abs_value / 10, abs_value % 10);
}

static void format_eq_precut(char *buffer, size_t size, int value)
{
    snprintf(buffer, size, value == 0 ? "0.0 dB" : "-%d.%d dB",
             value / 10, value % 10);
}

static void format_eq_frequency(char *buffer, size_t size, int value)
{
    if(value >= 1000 && value % 1000 == 0)
        snprintf(buffer, size, "%dkHz", value / 1000);
    else if(value >= 1000)
        snprintf(buffer, size, "%d.%dkHz", value / 1000,
                 (value % 1000) / 100);
    else
        snprintf(buffer, size, "%dHz", value);
}

static void format_eq_q(char *buffer, size_t size, int value)
{
    snprintf(buffer, size, "%d.%d Q", value / 10, value % 10);
}

static void eq_studio_apply_band(int band)
{
    if(band < 0 || band >= EQ_NUM_BANDS)
        return;
    dsp_set_eq_coefs(band, &global_settings.eq_band_settings[band]);
    crazypod_state_mark_dirty();
}

static void eq_studio_apply_precut(void)
{
    dsp_set_eq_precut(global_settings.eq_precut);
    crazypod_state_mark_dirty();
}

static void eq_studio_toggle_enabled(void)
{
    global_settings.eq_enabled = !global_settings.eq_enabled;
    crazypod_eq_settings_apply();
    crazypod_state_mark_dirty();
    render_current_route(false);
}

static void eq_studio_cycle_mode(void)
{
    eq_studio_mode =
        (enum eq_studio_mode)((eq_studio_mode + 1) %
                              EQ_STUDIO_MODE_COUNT);
    render_current_route(false);
}

static void eq_studio_adjust(int direction)
{
    struct eq_band_setting *band;
    int next;

    if(direction == 0)
        return;
    if(!eq_studio_editing) {
        eq_studio_band = clamp_value(eq_studio_band + direction,
                                     0, EQ_NUM_BANDS - 1);
        render_current_route(false);
        return;
    }

    band = &global_settings.eq_band_settings[eq_studio_band];
    switch(eq_studio_mode) {
    case EQ_STUDIO_CUTOFF:
        next = band->cutoff + direction * CRAZYPOD_EQ_CUTOFF_FAST_STEP;
        band->cutoff = clamp_value(next, CRAZYPOD_EQ_CUTOFF_MIN,
                                   CRAZYPOD_EQ_CUTOFF_MAX);
        eq_studio_apply_band(eq_studio_band);
        break;
    case EQ_STUDIO_Q:
        next = band->q + direction * CRAZYPOD_EQ_Q_STEP;
        band->q = clamp_value(next, CRAZYPOD_EQ_Q_MIN,
                              CRAZYPOD_EQ_Q_MAX);
        eq_studio_apply_band(eq_studio_band);
        break;
    case EQ_STUDIO_PRECUT:
        next = global_settings.eq_precut +
               direction * CRAZYPOD_EQ_PRECUT_FAST_STEP;
        global_settings.eq_precut =
            clamp_value(next, CRAZYPOD_EQ_PRECUT_MIN,
                        CRAZYPOD_EQ_PRECUT_MAX);
        eq_studio_apply_precut();
        break;
    default:
        next = band->gain + direction * CRAZYPOD_EQ_GAIN_FAST_STEP;
        band->gain = clamp_value(next, CRAZYPOD_EQ_GAIN_MIN,
                                 CRAZYPOD_EQ_GAIN_MAX);
        eq_studio_apply_band(eq_studio_band);
        break;
    }
    render_current_route(false);
}

static void eq_studio_select_band(int direction)
{
    if(direction == 0)
        return;
    eq_studio_band = clamp_value(eq_studio_band + direction,
                                 0, EQ_NUM_BANDS - 1);
    render_current_route(false);
}

static void render_eq_chip(lv_obj_t *parent, int x, const char *title,
                           bool active)
{
    lv_obj_t *chip = make_box(parent, x, 194, 58, 18, 9,
                              active ? highlight_primary() : COLOR_WHITE,
                              active ? 210 : 20);
    lv_obj_t *label = make_label(chip, title, &lv_font_montserrat_8,
                                 COLOR_WHITE, active ? 255 : 140);
    lv_obj_set_width(label, 58);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 0, 5);
}

static int eq_bar_y_for_gain(int gain)
{
    return 124 - gain * 38 / CRAZYPOD_EQ_GAIN_MAX;
}

static void render_eq_studio(void)
{
    static const char *const fixed_labels[EQ_NUM_BANDS] = {
        "32", "64", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
    };
    lv_obj_t *label;
    lv_obj_t *bar;
    char text[96];
    char gain_text[24];
    char freq_text[24];
    char q_text[24];
    char precut_text[24];
    int i;
    int max_gain = 0;
    const struct eq_band_setting *current =
        &global_settings.eq_band_settings[eq_studio_band];
    bool clipping_risk;

    make_box(product_content, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
             0x050508, LV_OPA_COVER);
    make_box(product_content, 0, 29, LCD_WIDTH, 35, 0,
             0x101017, 235);

    label = make_label(product_content, "EQ Studio",
                       CRAZYPOD_METADATA_FONT, COLOR_WHITE, 245);
    lv_obj_set_pos(label, 14, 39);
    lv_obj_set_width(label, 120);
    label = make_label(product_content,
                       global_settings.eq_enabled ? "On" : "Bypass",
                       &lv_font_montserrat_10,
                       global_settings.eq_enabled ? COLOR_GREEN : COLOR_MUTED,
                       240);
    lv_obj_set_width(label, 60);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 190, 41);
    label = make_label(product_content,
                       eq_studio_editing ? "EDIT" : "BROWSE",
                       &lv_font_montserrat_8, COLOR_WHITE, 125);
    lv_obj_set_width(label, 54);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 252, 42);

    format_eq_db(gain_text, sizeof(gain_text), current->gain);
    format_eq_frequency(freq_text, sizeof(freq_text), current->cutoff);
    format_eq_q(q_text, sizeof(q_text), current->q);
    format_eq_precut(precut_text, sizeof(precut_text),
                     global_settings.eq_precut);

    snprintf(text, sizeof(text), "%s  %s  %s",
             freq_text, gain_text, q_text);
    label = make_label(product_content, text, &lv_font_montserrat_10,
                       COLOR_WHITE, 180);
    lv_obj_set_pos(label, 14, 66);
    lv_obj_set_width(label, 198);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    for(i = 0; i < EQ_NUM_BANDS; ++i) {
        int gain = global_settings.eq_band_settings[i].gain;
        if(gain > max_gain)
            max_gain = gain;
    }
    clipping_risk = max_gain > 0 &&
                    max_gain > (int)global_settings.eq_precut;
    snprintf(text, sizeof(text), "Precut %s", precut_text);
    label = make_label(product_content, text, &lv_font_montserrat_8,
                       clipping_risk ? COLOR_AMBER : COLOR_WHITE,
                       clipping_risk ? 235 : 125);
    lv_obj_set_width(label, 90);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 216, 68);

    make_box(product_content, 14, 124, 292, 1, 0, COLOR_WHITE, 70);
    make_box(product_content, 14, 84, 292, 1, 0, COLOR_WHITE, 18);
    make_box(product_content, 14, 163, 292, 1, 0, COLOR_WHITE, 18);
    label = make_label(product_content, "0 dB", &lv_font_montserrat_8,
                       COLOR_WHITE, 95);
    lv_obj_set_pos(label, 16, 113);

    for(i = 0; i < EQ_NUM_BANDS; ++i) {
        int gain = global_settings.eq_band_settings[i].gain;
        int abs_gain = gain < 0 ? -gain : gain;
        int height = abs_gain * 38 / CRAZYPOD_EQ_GAIN_MAX;
        int x = 29 + i * 27;
        int y = gain >= 0 ? 124 - height : 125;
        int width = i == eq_studio_band ? 16 : 10;
        uint32_t color = gain >= 0 ? COLOR_GREEN : COLOR_ROSE;
        lv_opa_t opa = global_settings.eq_enabled ? 230 : 80;

        if(height < 2)
            height = 2;
        if(i == eq_studio_band)
            color = highlight_primary();
        bar = make_box(product_content, x - width / 2, y,
                       width, height, 4, color, opa);
        if(i == eq_studio_band) {
            lv_obj_set_style_border_width(bar, 1, 0);
            lv_obj_set_style_border_color(bar, lv_color_hex(COLOR_WHITE), 0);
            lv_obj_set_style_border_opa(bar, 95, 0);
        }
        make_box(product_content, x - 2,
                 eq_bar_y_for_gain(gain) - 2, 4, 4,
                 LV_RADIUS_CIRCLE, color, opa);
        label = make_label(product_content, fixed_labels[i],
                           &lv_font_montserrat_8,
                           i == eq_studio_band ? COLOR_WHITE : COLOR_MUTED,
                           i == eq_studio_band ? 235 : 110);
        lv_obj_set_width(label, 28);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, x - 14, 168);
    }

    make_box(product_content, 0, 184, LCD_WIDTH, 34, 0,
             0x111119, 235);
    label = make_label(product_content, fixed_labels[eq_studio_band],
                       &lv_font_montserrat_16, COLOR_WHITE, 245);
    lv_obj_set_pos(label, 14, 188);
    lv_obj_set_width(label, 44);
    label = make_label(product_content, eq_band_role(eq_studio_band),
                       &lv_font_montserrat_8, COLOR_WHITE, 120);
    lv_obj_set_pos(label, 62, 190);
    lv_obj_set_width(label, 75);
    label = make_label(product_content, eq_mode_title(eq_studio_mode),
                       &lv_font_montserrat_10, COLOR_CYAN, 225);
    lv_obj_set_pos(label, 142, 189);
    lv_obj_set_width(label, 60);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    label = make_label(product_content,
                       eq_studio_editing ? "Wheel adjusts" : "Wheel selects",
                       &lv_font_montserrat_8, COLOR_WHITE, 115);
    lv_obj_set_pos(label, 205, 190);
    lv_obj_set_width(label, 98);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);

    render_eq_chip(product_content, 14, "Gain",
                   eq_studio_mode == EQ_STUDIO_GAIN);
    render_eq_chip(product_content, 80, "Freq",
                   eq_studio_mode == EQ_STUDIO_CUTOFF);
    render_eq_chip(product_content, 146, "Q",
                   eq_studio_mode == EQ_STUDIO_Q);
    render_eq_chip(product_content, 212, "Precut",
                   eq_studio_mode == EQ_STUDIO_PRECUT);

    make_box(product_content, 0, 218, LCD_WIDTH, 22, 0,
             0x050508, 245);
    label = make_label(product_content, "Menu Done",
                       &lv_font_montserrat_8, COLOR_WHITE, 125);
    lv_obj_set_pos(label, 14, 225);
    label = make_label(product_content, "Select Edit",
                       &lv_font_montserrat_8, COLOR_WHITE, 165);
    lv_obj_set_pos(label, 113, 225);
    label = make_label(product_content,
                       eq_studio_editing ? "Play Mode" : "Play A/B",
                       &lv_font_montserrat_8, COLOR_WHITE, 125);
    lv_obj_set_width(label, 82);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 224, 225);
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
        return CRAZYPOD_EDITOR_CHAR_COUNT + 3;
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
    case PHOTOS_ROUTE_MENU:
        return 3;
    case PHOTOS_ROUTE_LIBRARY:
        return crazypod_photo_count();
    case PHOTOS_ROUTE_VIDEOS:
        return crazypod_video_count();
    case PHOTOS_ROUTE_FAVORITES:
        return crazypod_photo_favorite_count();
    case PHOTOS_ROUTE_DETAIL:
        return 2;
    case EXTRAS_ROUTE_MENU:
        return crazypod_apps_hidden_count();
    case NOTES_ROUTE_MENU:
        return notes_home_deleted_index() + 1;
    case NOTES_ROUTE_COMPOSER:
        return CRAZYPOD_EDITOR_CHAR_COUNT + 3;
    case NOTES_ROUTE_EXIT_ACTIONS:
        return 3;
    case NOTES_ROUTE_DISCARD_CONFIRM:
        return 1;
    case NOTES_ROUTE_SEARCH:
        return CRAZYPOD_EDITOR_CHAR_COUNT + 3;
    case NOTES_ROUTE_SEARCH_RESULTS:
        return crazypod_notes_search_count(note_search_query);
    case NOTES_ROUTE_READER:
        return note_body_line_count(note_reader_body) > 9
            ? note_body_line_count(note_reader_body) - 8 : 1;
    case NOTES_ROUTE_ACTIONS:
        return 4;
    case NOTES_ROUTE_DELETED:
        return crazypod_notes_count(true) > 0
            ? crazypod_notes_count(true) + 1 : 0;
    case NOTES_ROUTE_DELETED_ACTIONS:
        return 2;
    case NOTES_ROUTE_DELETE_CONFIRM:
    case NOTES_ROUTE_PERMANENT_CONFIRM:
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        return 1;
    case BOOKS_ROUTE_MENU:
        return books_has_continue() ? 6 : 5;
    case BOOKS_ROUTE_RECENTS:
        return crazypod_books_recent_count();
    case BOOKS_ROUTE_LIBRARY:
        return crazypod_books_count();
    case BOOKS_ROUTE_FAVORITES:
        return crazypod_books_favorite_count();
    case BOOKS_ROUTE_READER:
    case BOOKS_ROUTE_STATS:
    case BOOKS_ROUTE_INFO:
        return 1;
    case BOOKS_ROUTE_ACTIONS: {
        return 6;
    }
    case BOOKS_ROUTE_CHAPTERS:
        return crazypod_book_chapter_count(state->group);
    case BOOKS_ROUTE_BOOKMARKS: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);
        return book != NULL && book->bookmark > 0 ? 1 : 0;
    }
    case BOOKS_ROUTE_DELETE_CONFIRM:
        return 1;
    case BOOKS_ROUTE_READING_SETTINGS:
        return 3;
    case PODCASTS_ROUTE_MENU:
        return podcast_count();
    case UTILITIES_ROUTE_MENU:
        return crazypod_miniapps_count();
    case MINIAPP_ROUTE_VIEW:
        return 1;
    case CLOCK_ROUTE_MENU:
        return 3;
    case CLOCK_ROUTE_SLEEP_TIMER:
        return get_sleep_timer_active() ? 2 : 4;
    case CLOCK_ROUTE_VIEW:
    case STOPWATCH_ROUTE_VIEW:
    case WORKOUT_ROUTE_READY:
    case WORKOUT_ROUTE_ACTIVE:
    case WORKOUT_ROUTE_SUMMARY:
    case WORKOUT_ROUTE_DETAIL:
    case CALENDAR_ROUTE_DETAIL:
    case CONTACTS_ROUTE_DETAIL:
        return 1;
    case WORKOUT_ROUTE_MENU:
        return 3;
    case WORKOUT_ROUTE_TYPES:
        return CRAZYPOD_WORKOUT_ACTIVITY_COUNT;
    case WORKOUT_ROUTE_FINISH_CONFIRM:
    case WORKOUT_ROUTE_DELETE_CONFIRM:
        return 1;
    case WORKOUT_ROUTE_HISTORY:
        return crazypod_workouts_count();
    case CALENDAR_ROUTE_MENU:
        return 4;
    case CALENDAR_ROUTE_TODAY:
    case CALENDAR_ROUTE_UPCOMING:
    case CALENDAR_ROUTE_DAY_EVENTS:
        return calendar_route_event_count(state) + 1;
    case CALENDAR_ROUTE_MONTH:
        return 1;
    case CALENDAR_ROUTE_EDITOR:
        return 4;
    case CALENDAR_ROUTE_TITLE_EDITOR:
        return CRAZYPOD_EDITOR_CHAR_COUNT + 3;
    case CALENDAR_ROUTE_ACTIONS:
        return 2;
    case CALENDAR_ROUTE_DELETE_CONFIRM:
        return 1;
    case CONTACTS_ROUTE_LIST:
        return crazypod_contacts_count();
    case SETTINGS_ROUTE_MENU:
    case SETTINGS_ROUTE_SOUND:
    case SETTINGS_ROUTE_EQ_STUDIO:
    case SETTINGS_ROUTE_DISPLAY:
    case SETTINGS_ROUTE_PLAYBACK:
    case SETTINGS_ROUTE_POWER:
    case SETTINGS_ROUTE_CONTROLS:
    case SETTINGS_ROUTE_MAIN_MENU:
        return settings_route_item_count(state->route);
    case SETTINGS_ROUTE_MAIN_MENU_ACTIONS:
        return crazypod_apps_is_fixed(
                   (enum crazypod_app_id)state->group)
            ? 2 : 3;
    case DIY_ROUTE_MENU:
        return (int)(sizeof(diy_menu_titles) /
                     sizeof(diy_menu_titles[0]));
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
        return (int)(sizeof(diy_background_titles) /
                     sizeof(diy_background_titles[0]));
    case DIY_ROUTE_BACKGROUND_CHOICES:
        return CRAZYPOD_APPEARANCE_COLOR_COUNT + 2;
    case DIY_ROUTE_WALLPAPER_FILES:
        return crazypod_photo_count();
    case DIY_ROUTE_WALLPAPER_CROP:
        return 0;
    case DIY_ROUTE_LAYOUT:
        return (int)(sizeof(diy_layout_titles) /
                     sizeof(diy_layout_titles[0]));
    }
    return 0;
}

static struct crazypod_app *route_app(const struct route_state *state,
                                     int index)
{
    if(state->route == SETTINGS_ROUTE_MAIN_MENU)
        return ordered_app(index);
    if(state->route == EXTRAS_ROUTE_MENU)
        return hidden_app(index);
    if(state->route == SETTINGS_ROUTE_MAIN_MENU_ACTIONS)
        return app_for_id((enum crazypod_app_id)state->group);
    return NULL;
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
    case PODCASTS_ROUTE_MENU:
        return crazypod_music_track(podcast_track_index(index));
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
    case PHOTOS_ROUTE_MENU:
        return index >= 0 && index < 3 ? photos_menu_titles[index] : "";
    case PHOTOS_ROUTE_LIBRARY:
        return crazypod_photo_name(index);
    case PHOTOS_ROUTE_VIDEOS:
        return crazypod_video_name(index);
    case PHOTOS_ROUTE_FAVORITES:
        return crazypod_photo_name(crazypod_photo_favorite_index(index));
    case PHOTOS_ROUTE_DETAIL:
        return index == 0 ? "Fit" : "2x";
    case SETTINGS_ROUTE_MENU:
        return index >= 0 &&
               index < (int)(sizeof(settings_menu_titles) /
                             sizeof(settings_menu_titles[0]))
            ? settings_menu_titles[index] : "";
    case SETTINGS_ROUTE_MAIN_MENU: {
        struct crazypod_app *app = ordered_app(index);
        return app != NULL ? app->name : "";
    }
    case SETTINGS_ROUTE_MAIN_MENU_ACTIONS:
        if(index == 0 &&
           !crazypod_apps_is_fixed(
               (enum crazypod_app_id)state->group))
            return crazypod_apps_is_enabled(
                       (enum crazypod_app_id)state->group)
                ? "Hide" : "Show";
        index += crazypod_apps_is_fixed(
                     (enum crazypod_app_id)state->group)
            ? 1 : 0;
        return index >= 1 && index < 3
            ? main_menu_action_titles[index] : "";
    case EXTRAS_ROUTE_MENU: {
        struct crazypod_app *app = hidden_app(index);
        return app != NULL ? app->name : "";
    }
    case NOTES_ROUTE_MENU: {
        const struct crazypod_note *note;
        if(index == 0)
            return "New Note";
        if(note_draft_available && index == 1)
            return "Continue Draft";
        if(index == notes_home_search_index())
            return "Search";
        if(index == notes_home_deleted_index())
            return "Deleted";
        note = notes_home_note(index);
        return note != NULL ? note->title : "";
    }
    case NOTES_ROUTE_COMPOSER:
        if(index >= 0 && index < CRAZYPOD_EDITOR_CHAR_COUNT)
            return editor_characters[index];
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT)
            return "Space";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            return "Backspace";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 2)
            return "Save Note";
        return "";
    case NOTES_ROUTE_EXIT_ACTIONS:
        return index == 0 ? "Save" :
               index == 1 ? "Keep" :
               index == 2 ? "Discard" : "";
    case NOTES_ROUTE_DISCARD_CONFIRM:
        return "Hold Center to Discard";
    case NOTES_ROUTE_SEARCH:
        if(index >= 0 && index < CRAZYPOD_EDITOR_CHAR_COUNT)
            return editor_characters[index];
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT)
            return "Space";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            return "Backspace";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 2)
            return "View Results";
        return "";
    case NOTES_ROUTE_SEARCH_RESULTS: {
        const struct crazypod_note *note =
            crazypod_notes_search_get(note_search_query, index);
        return note != NULL ? note->title : "";
    }
    case NOTES_ROUTE_ACTIONS: {
        const struct crazypod_note *note =
            crazypod_note_find((uint32_t)state->group);
        if(index == 0)
            return note != NULL && note->pinned ? "Unpin" : "Pin";
        return index == 1 ? "Edit" :
               index == 2 ? "Duplicate" :
               index == 3 ? "Delete" : "";
    }
    case NOTES_ROUTE_DELETED:
        if(index == crazypod_notes_count(true))
            return "Empty Deleted";
        else {
            const struct crazypod_note *note =
                crazypod_note_get(true, index);
            return note != NULL ? note->title : "";
        }
    case NOTES_ROUTE_DELETED_ACTIONS:
        return index == 0 ? "Restore" :
               index == 1 ? "Erase Forever" : "";
    case NOTES_ROUTE_DELETE_CONFIRM:
        return "Hold Center to Delete";
    case NOTES_ROUTE_PERMANENT_CONFIRM:
        return "Hold Center to Erase";
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        return "Hold Center to Empty";
    case BOOKS_ROUTE_MENU: {
        static const char *const titles[] = {
            "Recents", "Books", "Favorites", "Stats", "Reading"
        };
        int logical;
        if(books_has_continue() && index == 0)
            return "Continue";
        logical = index - (books_has_continue() ? 1 : 0);
        return logical >= 0 && logical < 5 ? titles[logical] : "";
    }
    case BOOKS_ROUTE_RECENTS:
    case BOOKS_ROUTE_LIBRARY:
    case BOOKS_ROUTE_FAVORITES: {
        const struct crazypod_book *book =
            crazypod_book_get(books_route_book_index(state, index));
        return book != NULL ? book->title : "";
    }
    case BOOKS_ROUTE_ACTIONS: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);
        if(index == 0)
            return "Read";
        if(index == 1)
            return "Bookmarks";
        if(index == 2)
            return "Chapters";
        if(index == 3)
            return book != NULL && book->favorite
                ? "Remove Favorite" : "Favorite";
        if(index == 4)
            return "Info";
        return index == 5 ? "Delete" : "";
    }
    case BOOKS_ROUTE_CHAPTERS: {
        static char chapter_title[96];
        uint32_t offset;
        if(crazypod_book_chapter_get(
               state->group, index, chapter_title,
               sizeof(chapter_title), &offset))
            return chapter_title;
        return "";
    }
    case BOOKS_ROUTE_BOOKMARKS:
        return "Saved Page";
    case BOOKS_ROUTE_DELETE_CONFIRM:
        return "Hold Center to Delete";
    case BOOKS_ROUTE_READING_SETTINGS:
        if(index == 0) {
            static const char *const sizes[] = {
                "Text Size: Small", "Text Size: Medium",
                "Text Size: Large"
            };
            return sizes[crazypod_books_font_size()];
        }
        if(index == 1) {
            static const char *const themes[] = {
                "Page: Parchment", "Page: Light",
                "Page: Mint", "Page: Dark"
            };
            return themes[crazypod_books_theme()];
        }
        return index == 2 ? "Import / Rescan" : "";
    case BOOKS_ROUTE_STATS:
        return "Library Summary";
    case BOOKS_ROUTE_INFO:
        return "Book Details";
    case PODCASTS_ROUTE_MENU: {
        int track_index = podcast_track_index(index);
        const struct crazypod_track *track =
            crazypod_music_track(track_index);
        return track != NULL ? track->title : "";
    }
    case UTILITIES_ROUTE_MENU:
    {
        const struct crazypod_miniapp_metadata *metadata =
            miniapp_metadata(index);
        return metadata != NULL ? metadata->name : "";
    }
    case MINIAPP_ROUTE_VIEW:
    {
        const struct crazypod_miniapp_metadata *metadata =
            miniapp_metadata(state->group);
        return metadata != NULL ? metadata->name : "Mini App";
    }
    case CLOCK_ROUTE_MENU:
        return index == 0 ? "Local Time" :
               index == 1 ? "Sleep Timer" :
               index == 2 ? "Stopwatch" : "";
    case CLOCK_ROUTE_SLEEP_TIMER:
        if(get_sleep_timer_active())
            return index == 0 ? "Cancel Timer" :
                   index == 1 ? "End Now" : "";
        else {
            static const char *const durations[] = {
                "15 Minutes", "30 Minutes", "45 Minutes", "60 Minutes"
            };
            return index >= 0 && index < 4 ? durations[index] : "";
        }
    case CLOCK_ROUTE_VIEW:
        return "Current Time";
    case STOPWATCH_ROUTE_VIEW:
        return stopwatch_running ? "Pause" : "Start";
    case WORKOUT_ROUTE_MENU:
        return index >= 0 && index < 3
            ? workout_menu_titles[index] : "";
    case WORKOUT_ROUTE_TYPES:
        return crazypod_workout_activity_title(index);
    case WORKOUT_ROUTE_READY:
        return "Start";
    case WORKOUT_ROUTE_ACTIVE:
        return workout_running ? "Pause" : "Resume";
    case WORKOUT_ROUTE_FINISH_CONFIRM:
        return "Hold Center to Save";
    case WORKOUT_ROUTE_HISTORY: {
        const struct crazypod_workout *workout =
            crazypod_workout_get(index);
        return workout != NULL
            ? crazypod_workout_activity_title(workout->activity) : "";
    }
    case WORKOUT_ROUTE_SUMMARY:
        return "Workout Summary";
    case WORKOUT_ROUTE_DETAIL:
        return "Workout Details";
    case WORKOUT_ROUTE_DELETE_CONFIRM:
        return "Hold Center to Delete";
    case CALENDAR_ROUTE_MENU:
        return index == 0 ? "Today" :
               index == 1 ? "Upcoming" :
               index == 2 ? "Month" :
               index == 3 ? "Add Event" : "";
    case CALENDAR_ROUTE_TODAY:
    case CALENDAR_ROUTE_UPCOMING:
    case CALENDAR_ROUTE_DAY_EVENTS: {
        int event_count = calendar_route_event_count(state);
        const struct crazypod_calendar_event *event;
        if(index == event_count)
            return "Add Event";
        event = crazypod_calendar_event_get(
            calendar_route_event_index(state, index));
        return event != NULL ? event->summary : "";
    }
    case CALENDAR_ROUTE_MONTH:
        return "Month";
    case CALENDAR_ROUTE_EDITOR: {
        static char text[128];
        char time[16];
        if(index == 0) {
            snprintf(text, sizeof(text), "Title: %s",
                     calendar_editor_summary[0] != '\0'
                         ? calendar_editor_summary : "Untitled");
            return text;
        }
        if(index == 1) {
            snprintf(text, sizeof(text), "Date: %04d-%02d-%02d",
                     calendar_editor_date / 10000,
                     calendar_editor_date / 100 % 100,
                     calendar_editor_date % 100);
            return text;
        }
        if(index == 2) {
            calendar_format_time(time, sizeof(time),
                                 calendar_editor_minutes);
            snprintf(text, sizeof(text), "Time: %s",
                     time[0] != '\0' ? time : "All Day");
            return text;
        }
        if(index == 3)
            return calendar_editor_error == 1
                ? "Title Required"
                : calendar_editor_error == 2
                    ? "Storage Error" : "Save Event";
        return "";
    }
    case CALENDAR_ROUTE_TITLE_EDITOR:
        if(index >= 0 && index < CRAZYPOD_EDITOR_CHAR_COUNT)
            return editor_characters[index];
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT)
            return "Space";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            return "Backspace";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 2)
            return "Done";
        return "";
    case CALENDAR_ROUTE_DETAIL:
        return "Event Details";
    case CALENDAR_ROUTE_ACTIONS:
        return index == 0 ? "Edit" :
               index == 1 ? "Delete" : "";
    case CALENDAR_ROUTE_DELETE_CONFIRM:
        return "Hold Center to Delete";
    case CONTACTS_ROUTE_LIST: {
        const struct crazypod_contact *contact =
            crazypod_contact_get(index);
        return contact != NULL ? contact->name : "";
    }
    case CONTACTS_ROUTE_DETAIL:
        return "Contact Details";
    case SETTINGS_ROUTE_EQ_STUDIO: {
        static const char *const labels[EQ_NUM_BANDS] = {
            "32Hz", "64Hz", "125Hz", "250Hz", "500Hz",
            "1kHz", "2kHz", "4kHz", "8kHz", "16kHz"
        };
        return index >= 0 && index < EQ_NUM_BANDS ? labels[index] : "";
    }
    case SETTINGS_ROUTE_SOUND:
    case SETTINGS_ROUTE_DISPLAY:
    case SETTINGS_ROUTE_PLAYBACK:
    case SETTINGS_ROUTE_POWER:
    case SETTINGS_ROUTE_CONTROLS:
        return settings_item_title(
            settings_route_item(state->route, index));
    case MUSIC_ROUTE_SEARCH:
        if(index >= 0 && index < CRAZYPOD_EDITOR_CHAR_COUNT)
            return editor_characters[index];
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT)
            return "Space";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            return "Delete";
        if(index == CRAZYPOD_EDITOR_CHAR_COUNT + 2)
            return "View Results";
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
        return index >= 0 &&
               index < (int)(sizeof(diy_background_titles) /
                             sizeof(diy_background_titles[0]))
            ? diy_background_titles[index] : "";
    case DIY_ROUTE_BACKGROUND_CHOICES:
        if(index == 0)
            return state->group ==
                   CRAZYPOD_APPEARANCE_LOCK_BACKGROUND
                ? "Follow Home" : "Default";
        if(index <= CRAZYPOD_APPEARANCE_COLOR_COUNT)
            return crazypod_appearance_color_name(index - 1);
        return index == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
            ? "Choose Picture" : "";
    case DIY_ROUTE_WALLPAPER_FILES:
        return crazypod_photo_name(index);
    case DIY_ROUTE_WALLPAPER_CROP:
        return "";
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
    case PHOTOS_ROUTE_MENU: return "MEDIA";
    case PHOTOS_ROUTE_LIBRARY: return "PHOTOS";
    case PHOTOS_ROUTE_VIDEOS: return "VIDEOS";
    case PHOTOS_ROUTE_FAVORITES: return "FAVORITES";
    case PHOTOS_ROUTE_DETAIL: return "PHOTO";
    case SETTINGS_ROUTE_MENU: return "SETTINGS";
    case SETTINGS_ROUTE_SOUND: return "SOUND";
    case SETTINGS_ROUTE_EQ_STUDIO: return "EQ STUDIO";
    case SETTINGS_ROUTE_DISPLAY: return "DISPLAY";
    case SETTINGS_ROUTE_PLAYBACK: return "PLAYBACK";
    case SETTINGS_ROUTE_POWER: return "POWER";
    case SETTINGS_ROUTE_CONTROLS: return "CONTROLS";
    case SETTINGS_ROUTE_MAIN_MENU: return "MAIN MENU";
    case SETTINGS_ROUTE_MAIN_MENU_ACTIONS: {
        struct crazypod_app *app =
            app_for_id((enum crazypod_app_id)state->group);
        return app != NULL ? app->name : "MAIN MENU";
    }
    case EXTRAS_ROUTE_MENU: return "MORE FEATURES";
    case NOTES_ROUTE_MENU: return "NOTES";
    case NOTES_ROUTE_COMPOSER:
        return note_editor_body_active ? "EDIT BODY" : "EDIT TITLE";
    case NOTES_ROUTE_EXIT_ACTIONS:
        return "UNSAVED NOTE";
    case NOTES_ROUTE_DISCARD_CONFIRM:
        return "DISCARD DRAFT";
    case NOTES_ROUTE_SEARCH: return "SEARCH NOTES";
    case NOTES_ROUTE_SEARCH_RESULTS: return "RESULTS";
    case NOTES_ROUTE_READER: {
        const struct crazypod_note *note =
            crazypod_note_find((uint32_t)state->group);
        return note != NULL ? note->title : "NOTE";
    }
    case NOTES_ROUTE_ACTIONS: return "NOTE ACTIONS";
    case NOTES_ROUTE_DELETED: return "DELETED";
    case NOTES_ROUTE_DELETED_ACTIONS: return "DELETED NOTE";
    case NOTES_ROUTE_DELETE_CONFIRM: return "DELETE NOTE";
    case NOTES_ROUTE_PERMANENT_CONFIRM: return "ERASE NOTE";
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM: return "EMPTY DELETED";
    case BOOKS_ROUTE_MENU: return "BOOKS";
    case BOOKS_ROUTE_RECENTS: return "RECENTS";
    case BOOKS_ROUTE_LIBRARY: return "BOOKS";
    case BOOKS_ROUTE_FAVORITES: return "FAVORITES";
    case BOOKS_ROUTE_READER: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);
        return book != NULL ? book->title : "BOOK";
    }
    case BOOKS_ROUTE_ACTIONS: return "BOOK ACTIONS";
    case BOOKS_ROUTE_CHAPTERS: return "CHAPTERS";
    case BOOKS_ROUTE_BOOKMARKS: return "BOOKMARKS";
    case BOOKS_ROUTE_DELETE_CONFIRM: return "DELETE BOOK";
    case BOOKS_ROUTE_STATS: return "READING STATS";
    case BOOKS_ROUTE_READING_SETTINGS: return "READING";
    case BOOKS_ROUTE_INFO: return "BOOK INFO";
    case PODCASTS_ROUTE_MENU: return "PODCASTS";
    case UTILITIES_ROUTE_MENU: return "MINI APPS";
    case MINIAPP_ROUTE_VIEW: {
        const struct crazypod_miniapp_metadata *metadata =
            miniapp_metadata(state->group);
        return metadata != NULL ? metadata->name : "MINI APP";
    }
    case CLOCK_ROUTE_MENU: return "CLOCK";
    case CLOCK_ROUTE_SLEEP_TIMER: return "SLEEP TIMER";
    case CLOCK_ROUTE_VIEW: return "CLOCK";
    case STOPWATCH_ROUTE_VIEW: return "STOPWATCH";
    case WORKOUT_ROUTE_MENU: return "WORKOUTS";
    case WORKOUT_ROUTE_TYPES: return "CHOOSE WORKOUT";
    case WORKOUT_ROUTE_READY: return "WORKOUT";
    case WORKOUT_ROUTE_ACTIVE: return "WORKOUT";
    case WORKOUT_ROUTE_FINISH_CONFIRM: return "END WORKOUT";
    case WORKOUT_ROUTE_HISTORY: return "HISTORY";
    case WORKOUT_ROUTE_SUMMARY: return "SUMMARY";
    case WORKOUT_ROUTE_DETAIL: return "WORKOUT";
    case WORKOUT_ROUTE_DELETE_CONFIRM: return "DELETE WORKOUT";
    case CALENDAR_ROUTE_MENU: return "CALENDAR";
    case CALENDAR_ROUTE_TODAY: return "TODAY";
    case CALENDAR_ROUTE_UPCOMING: return "UPCOMING";
    case CALENDAR_ROUTE_MONTH: return "MONTH";
    case CALENDAR_ROUTE_DAY_EVENTS: return "EVENTS";
    case CALENDAR_ROUTE_EDITOR:
        return calendar_editor_id != 0 ? "EDIT EVENT" : "ADD EVENT";
    case CALENDAR_ROUTE_TITLE_EDITOR: return "EVENT TITLE";
    case CALENDAR_ROUTE_DETAIL: return "EVENT";
    case CALENDAR_ROUTE_ACTIONS: return "EVENT ACTIONS";
    case CALENDAR_ROUTE_DELETE_CONFIRM: return "DELETE EVENT";
    case CONTACTS_ROUTE_LIST: return "CONTACTS";
    case CONTACTS_ROUTE_DETAIL: return "CONTACT";
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
        return background_field_title(
            (enum crazypod_appearance_field)state->group);
    case DIY_ROUTE_WALLPAPER_FILES:
        if(state->group == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
            return "MENU PICTURE";
        if(state->group == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
            return "LOCK PICTURE";
        return "HOME PICTURE";
    case DIY_ROUTE_WALLPAPER_CROP:
        if(wallpaper_crop_target ==
           CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
            return "CROP MENU";
        if(wallpaper_crop_target ==
           CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
            return "CROP LOCK";
        return "CROP HOME";
    case DIY_ROUTE_LAYOUT: return "LAYOUT";
    }
    return "";
}

static bool route_item_is_current(const struct route_state *state, int index)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();

    switch(state->route) {
    case SETTINGS_ROUTE_MAIN_MENU:
        return crazypod_apps_is_enabled(
            crazypod_apps_ordered_id(index));
    case DIY_ROUTE_ICONS:
        return index == appearance->icon_theme;
    case DIY_ROUTE_CHOICES: {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)state->group;
        return appearance_choice_value(field, index) ==
               appearance_field_value(field);
    }
    case DIY_ROUTE_BACKGROUND_CHOICES: {
        const char *path = background_wallpaper(
            appearance,
            (enum crazypod_appearance_field)state->group);
        if(index == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1)
            return path[0] != '\0';
        return path[0] == '\0' &&
               index == appearance_field_value(
                   (enum crazypod_appearance_field)state->group);
    }
    case DIY_ROUTE_WALLPAPER_FILES: {
        const char *path = background_wallpaper(
            appearance,
            (enum crazypod_appearance_field)state->group);
        return index >= 0 && index < crazypod_photo_count() &&
               strcmp(path, crazypod_photo_path(index)) == 0;
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
        parent, track, x, y, display_size, descriptor, false);
}

static lv_obj_t *create_artwork(lv_obj_t *parent,
                                const struct crazypod_track *track,
                                int x, int y, int size, int slot)
{
    return create_artwork_cached(parent, track, x, y, size, size, slot);
}

static void create_panel_backgrounds(void)
{
    lv_obj_t *top;
    lv_obj_t *left;
    bool top_glass_valid;
    bool left_glass_valid;

    top_glass_valid = prepare_menu_glass_descriptor(
        0, 0, LCD_WIDTH, CRAZYPOD_STATUS_BAR_HEIGHT,
        CRAZYPOD_GLASS_MENU_TOPBAR,
        menu_topbar_glass_pixels, &menu_topbar_glass_descriptor);
    top = make_glass_material_panel(
        product_content, 0, 0,
        LCD_WIDTH, CRAZYPOD_STATUS_BAR_HEIGHT, 0,
        CRAZYPOD_GLASS_MENU_TOPBAR,
        top_glass_valid ? &menu_topbar_glass_descriptor : NULL);
    left_glass_valid = prepare_menu_glass_descriptor(
        0, CRAZYPOD_MENU_PANEL_Y,
        CRAZYPOD_MENU_PANEL_WIDTH, CRAZYPOD_MENU_PANEL_HEIGHT,
        CRAZYPOD_GLASS_MENU_PANEL,
        menu_panel_glass_pixels, &menu_panel_glass_descriptor);
    left = make_glass_material_panel(
        product_content, 0, CRAZYPOD_MENU_PANEL_Y,
        CRAZYPOD_MENU_PANEL_WIDTH, CRAZYPOD_MENU_PANEL_HEIGHT, 0,
        CRAZYPOD_GLASS_MENU_PANEL,
        left_glass_valid ? &menu_panel_glass_descriptor : NULL);
    lv_obj_remove_flag(top, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(top, 1, 0);
    lv_obj_set_style_border_side(top, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(top, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(top, 22, 0);
    lv_obj_set_style_border_width(left, 1, 0);
    lv_obj_set_style_border_side(left, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(left, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(left, 22, 0);
}

static bool is_music_preview_route(enum crazypod_route route)
{
    switch(route) {
    case MUSIC_ROUTE_MENU:
    case MUSIC_ROUTE_ALL:
    case MUSIC_ROUTE_PLAYLISTS:
    case MUSIC_ROUTE_PLAYLIST_SONGS:
    case MUSIC_ROUTE_ARTISTS:
    case MUSIC_ROUTE_ARTIST_SONGS:
    case MUSIC_ROUTE_ALBUMS:
    case MUSIC_ROUTE_ALBUM_SONGS:
    case MUSIC_ROUTE_SONGS:
    case MUSIC_ROUTE_SEARCH_RESULTS:
    case MUSIC_ROUTE_QUEUE:
        return true;
    default:
        return false;
    }
}

static bool is_skeuomorphic_preview_route(enum crazypod_route route)
{
    return route == MUSIC_ROUTE_MENU ||
           route == PHOTOS_ROUTE_MENU ||
           route == NOTES_ROUTE_MENU ||
           route == BOOKS_ROUTE_MENU;
}

static void menu_preview_register_motion(
    lv_obj_t *object,
    int enter_dx, int enter_dy, int enter_scale,
    int enter_rotation, int enter_opacity,
    int enter_delay, int enter_duration,
    int exit_dx, int exit_dy, int exit_scale, int exit_rotation)
{
    struct menu_preview_motion_part *part;

    if(object == NULL ||
       menu_preview_scene.part_count >= CRAZYPOD_PREVIEW_PART_COUNT)
        return;
    part = &menu_preview_scene.parts[menu_preview_scene.part_count++];
    part->object = object;
    /*
     * Preview objects are registered immediately after creation, before
     * LVGL has run layout. lv_obj_get_x/y() still report zero at that
     * point, which made every animated part settle outside the clipped
     * preview pane. The authored positions already live in the local
     * style, so read those directly.
     */
    part->final_x = lv_obj_get_style_x(object, 0);
    part->final_y = lv_obj_get_style_y(object, 0);
    part->final_scale =
        lv_obj_get_style_transform_scale_x(object, 0);
    part->final_rotation =
        lv_obj_get_style_transform_rotation(object, 0);
    part->final_opacity = lv_obj_get_style_opa(object, 0);
    part->enter_dx = enter_dx;
    part->enter_dy = enter_dy;
    part->enter_scale =
        enter_scale > 0 ? enter_scale : part->final_scale;
    part->enter_rotation = enter_rotation;
    part->enter_opacity = enter_opacity;
    part->exit_dx = exit_dx;
    part->exit_dy = exit_dy;
    part->exit_scale =
        exit_scale > 0 ? exit_scale : part->final_scale;
    part->exit_rotation = exit_rotation;
    part->enter_delay = enter_delay;
    part->enter_duration = enter_duration;
}

static void settle_menu_preview_scene(struct menu_preview_scene *scene)
{
    int index;

    if(scene->content != NULL && lv_obj_is_valid(scene->content)) {
        lv_anim_delete(scene->content, NULL);
        lv_obj_set_pos(
            scene->content,
            -CRAZYPOD_MENU_PANEL_WIDTH,
            -CRAZYPOD_STATUS_BAR_HEIGHT);
        lv_obj_set_style_opa(scene->content, LV_OPA_COVER, 0);
    }
    for(index = 0; index < scene->part_count; ++index) {
        struct menu_preview_motion_part *part = &scene->parts[index];
        if(part->object == NULL || !lv_obj_is_valid(part->object))
            continue;
        lv_anim_delete(part->object, NULL);
        lv_obj_set_pos(part->object, part->final_x, part->final_y);
        lv_obj_set_style_transform_scale(
            part->object, part->final_scale, 0);
        lv_obj_set_style_transform_rotation(
            part->object, part->final_rotation, 0);
        lv_obj_set_style_opa(
            part->object, (lv_opa_t)part->final_opacity, 0);
    }
}

static int menu_preview_clamp_progress(int value)
{
    if(value < 0)
        return 0;
    if(value > 1024)
        return 1024;
    return value;
}

static int menu_preview_ease_out(int progress)
{
    int inverse = 1024 - menu_preview_clamp_progress(progress);

    return 1024 - inverse * inverse / 1024;
}

static int menu_preview_ease_in(int progress)
{
    int clamped = menu_preview_clamp_progress(progress);

    return clamped * clamped / 1024;
}

static int menu_preview_back_out(int progress)
{
    int shifted = menu_preview_clamp_progress(progress) - 1024;
    int squared = shifted * shifted / 1024;
    int cubed = squared * shifted / 1024;

    return 1024 + (2766 * cubed + 1742 * squared) / 1024;
}

static int menu_preview_smooth_step(int progress)
{
    int clamped = menu_preview_clamp_progress(progress);
    int squared = clamped * clamped / 1024;

    return squared * (3072 - 2 * clamped) / 1024;
}

static int menu_preview_arc(int progress)
{
    int clamped = menu_preview_clamp_progress(progress);

    return clamped * (1024 - clamped) / 256;
}

static int menu_preview_lerp(int from, int to, int progress)
{
    return from + (to - from) * progress / 1024;
}

static void menu_preview_profile_arc_offset(
    int index, int arc, bool exiting, int *x, int *y)
{
    int visual_count = menu_preview_scene.part_count - 1;
    bool is_caption = index == menu_preview_scene.part_count - 1;
    int spread = visual_count > 1
        ? index * 2 - (visual_count - 1) : 0;
    int direction = menu_preview_navigation_direction;

    *x = 0;
    *y = 0;
    if(is_caption) {
        *y = -arc * (exiting ? 1 : 2) / 1024;
        return;
    }

    switch(menu_preview_motion_profile) {
    case MENU_PREVIEW_PROFILE_MUSIC:
        *x = arc * direction *
            ((index & 1) != 0 ? -3 : 3) / 1024;
        *y = -arc * (3 + index % 3) / 1024;
        break;
    case MENU_PREVIEW_PROFILE_PHOTOS:
        *x = arc * spread * (exiting ? -2 : 2) / 1024;
        *y = -arc * (exiting ? 3 : 6) / 1024;
        break;
    case MENU_PREVIEW_PROFILE_NOTES:
        *x = arc * direction *
            ((index & 1) != 0 ? -4 : 4) / 1024;
        *y = -arc * (exiting ? 2 : 5) / 1024;
        break;
    case MENU_PREVIEW_PROFILE_BOOKS:
        *x = arc * spread / 1024;
        *y = -arc * (exiting ? 3 : 7) / 1024;
        break;
    default:
        *y = -arc * 2 / 1024;
        break;
    }
}

static int menu_preview_scaled_part_time(int duration)
{
    return duration * CRAZYPOD_PREVIEW_PART_TIME_NUMERATOR /
        CRAZYPOD_PREVIEW_PART_TIME_DENOMINATOR;
}

static int menu_preview_part_raw_progress(
    const struct menu_preview_motion_part *part, int elapsed)
{
    int duration = part->enter_duration > 0
        ? part->enter_duration : CRAZYPOD_PREVIEW_ENTER_DURATION_MS;
    int delay = menu_preview_scaled_part_time(part->enter_delay);

    duration = menu_preview_scaled_part_time(duration);
    return menu_preview_clamp_progress(
        (elapsed - delay) * 1024 / duration);
}

static void menu_preview_timeline_anim(void *target, int32_t elapsed)
{
    lv_obj_t *content = target;
    int index;

    if(content == NULL || content != menu_preview_content ||
       !lv_obj_is_valid(content))
        return;

    if(menu_preview_motion_phase == MENU_PREVIEW_MOTION_ENTERING) {
        int content_raw_progress = menu_preview_clamp_progress(
            elapsed * 1024 /
            (menu_preview_motion_reduced
                ? CRAZYPOD_PREVIEW_REDUCED_DURATION_MS
                : CRAZYPOD_PREVIEW_ENTER_DURATION_MS));
        int content_position_progress =
            menu_preview_motion_reduced
                ? menu_preview_ease_out(content_raw_progress)
                : menu_preview_back_out(content_raw_progress);
        int content_opacity_progress =
            menu_preview_smooth_step(content_raw_progress);

        lv_obj_set_pos(
            content,
            menu_preview_lerp(
                -CRAZYPOD_MENU_PANEL_WIDTH + 8,
                -CRAZYPOD_MENU_PANEL_WIDTH,
                content_position_progress),
            menu_preview_lerp(
                -CRAZYPOD_STATUS_BAR_HEIGHT +
                    menu_preview_navigation_direction * 5,
                -CRAZYPOD_STATUS_BAR_HEIGHT,
                menu_preview_ease_out(content_raw_progress)));
        lv_obj_set_style_opa(
            content,
            (lv_opa_t)menu_preview_lerp(
                0, LV_OPA_COVER, content_opacity_progress),
            0);
        if(menu_preview_motion_reduced)
            return;

        for(index = 0; index < menu_preview_scene.part_count; ++index) {
            struct menu_preview_motion_part *part =
                &menu_preview_scene.parts[index];
            lv_obj_t *object = part->object;
            int raw_progress;
            int position_progress;
            int opacity_progress;
            int arc_x;
            int arc_y;

            if(object == NULL || !lv_obj_is_valid(object))
                continue;
            raw_progress =
                menu_preview_part_raw_progress(part, elapsed);
            position_progress =
                menu_preview_back_out(raw_progress);
            opacity_progress =
                menu_preview_smooth_step(
                    menu_preview_clamp_progress(
                        (raw_progress - 64) * 1024 / 960));
            menu_preview_profile_arc_offset(
                index, menu_preview_arc(raw_progress),
                false, &arc_x, &arc_y);
            lv_obj_set_pos(
                object,
                menu_preview_lerp(
                    part->final_x + part->enter_dx,
                    part->final_x, position_progress) + arc_x,
                menu_preview_lerp(
                    part->final_y + part->enter_dy,
                    part->final_y, position_progress) + arc_y);
            lv_obj_set_style_opa(
                object,
                (lv_opa_t)menu_preview_lerp(
                    part->enter_opacity,
                    part->final_opacity, opacity_progress),
                0);
        }
    }
    else if(menu_preview_motion_phase == MENU_PREVIEW_MOTION_EXITING) {
        int duration = menu_preview_motion_reduced
            ? CRAZYPOD_PREVIEW_REDUCED_DURATION_MS
            : CRAZYPOD_PREVIEW_EXIT_DURATION_MS;
        int progress = menu_preview_ease_in(
            menu_preview_clamp_progress(elapsed * 1024 / duration));
        int raw_progress = menu_preview_clamp_progress(
            elapsed * 1024 / duration);

        lv_obj_set_pos(
            content,
            menu_preview_lerp(
                -CRAZYPOD_MENU_PANEL_WIDTH,
                -CRAZYPOD_MENU_PANEL_WIDTH - 6,
                progress),
            menu_preview_lerp(
                -CRAZYPOD_STATUS_BAR_HEIGHT,
                -CRAZYPOD_STATUS_BAR_HEIGHT -
                    menu_preview_navigation_direction * 4,
                progress));
        lv_obj_set_style_opa(
            content,
            (lv_opa_t)menu_preview_lerp(
                LV_OPA_COVER, 0, progress),
            0);
        if(menu_preview_motion_reduced)
            return;

        for(index = 0; index < menu_preview_scene.part_count; ++index) {
            struct menu_preview_motion_part *part =
                &menu_preview_scene.parts[index];
            lv_obj_t *object = part->object;
            int arc_x;
            int arc_y;

            if(object == NULL || !lv_obj_is_valid(object))
                continue;
            menu_preview_profile_arc_offset(
                index, menu_preview_arc(raw_progress),
                true, &arc_x, &arc_y);
            lv_obj_set_pos(
                object,
                menu_preview_lerp(
                    part->final_x,
                    part->final_x + part->exit_dx,
                    progress) + arc_x,
                menu_preview_lerp(
                    part->final_y,
                    part->final_y + part->exit_dy,
                    progress) + arc_y);
        }
    }
}

static void settle_menu_preview_motion(void)
{
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
    menu_preview_media_deferred = false;
    menu_preview_media_refresh_pending = false;
    settle_menu_preview_scene(&menu_preview_scene);
}

static bool menu_preview_motion_active(void)
{
    return menu_preview_motion_phase != MENU_PREVIEW_MOTION_IDLE;
}

static void menu_preview_timeline_completed(lv_anim_t *animation)
{
    lv_obj_t *content = lv_anim_get_user_data(animation);
    enum menu_preview_motion_phase completed_phase =
        menu_preview_motion_phase;

    if(content == NULL || content != menu_preview_content)
        return;
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
    if(completed_phase == MENU_PREVIEW_MOTION_ENTERING) {
        settle_menu_preview_scene(&menu_preview_scene);
        if(menu_preview_media_deferred) {
            menu_preview_media_deferred = false;
            menu_preview_media_refresh_pending = true;
            menu_preview_media_due = current_tick + 1;
        }
        return;
    }
    if(completed_phase != MENU_PREVIEW_MOTION_EXITING)
        return;

    if(lv_obj_is_valid(content))
        lv_obj_delete(content);
    menu_preview_content = NULL;
    memset(&menu_preview_scene, 0, sizeof(menu_preview_scene));
    if(product_active && route_depth > 0 &&
       menu_view.valid &&
       menu_view.route == current_route()->route)
        render_menu_preview(current_route(), true);
}

static bool start_menu_preview_timeline(int duration)
{
    lv_anim_t animation;

    if(menu_preview_content == NULL)
        return false;
    lv_anim_delete(menu_preview_content, menu_preview_timeline_anim);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, menu_preview_content);
    lv_anim_set_exec_cb(&animation, menu_preview_timeline_anim);
    lv_anim_set_values(&animation, 0, duration);
    lv_anim_set_duration(&animation, duration);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_completed_cb(
        &animation, menu_preview_timeline_completed);
    lv_anim_set_user_data(&animation, menu_preview_content);
    lv_anim_set_early_apply(&animation, true);
    return lv_anim_start(&animation) != NULL;
}

static void reset_menu_preview_root(void)
{
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
    if(menu_preview_root == NULL) {
        menu_preview_root = lv_obj_create(product_content);
        set_plain_object(menu_preview_root);
        lv_obj_set_pos(menu_preview_root,
                       CRAZYPOD_MENU_PANEL_WIDTH,
                       CRAZYPOD_STATUS_BAR_HEIGHT);
        lv_obj_set_size(
            menu_preview_root,
            LCD_WIDTH - CRAZYPOD_MENU_PANEL_WIDTH,
            LCD_HEIGHT - CRAZYPOD_STATUS_BAR_HEIGHT);
        lv_obj_set_style_bg_opa(
            menu_preview_root, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(
            menu_preview_root, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(
            menu_preview_root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    }
    else {
        settle_menu_preview_scene(&menu_preview_scene);
        menu_preview_content = NULL;
        memset(&menu_preview_scene, 0, sizeof(menu_preview_scene));
        lv_obj_clean(menu_preview_root);
    }

    menu_preview_content = lv_obj_create(menu_preview_root);
    set_plain_object(menu_preview_content);
    lv_obj_set_pos(menu_preview_content,
                   -CRAZYPOD_MENU_PANEL_WIDTH,
                   -CRAZYPOD_STATUS_BAR_HEIGHT);
    lv_obj_set_size(menu_preview_content, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(
        menu_preview_content, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(
        menu_preview_content, LV_OBJ_FLAG_CLICKABLE);
    memset(&menu_preview_scene, 0, sizeof(menu_preview_scene));
    menu_preview_scene.content = menu_preview_content;
}

static void start_menu_preview_scene_entrance(void)
{
    bool reduced = crazypod_state_reduce_motion();
    int duration = reduced
        ? CRAZYPOD_PREVIEW_REDUCED_DURATION_MS : 0;
    int index;

    if(menu_preview_content == NULL)
        return;
    if(!reduced) {
        duration = CRAZYPOD_PREVIEW_ENTER_DURATION_MS;
        for(index = 0; index < menu_preview_scene.part_count; ++index) {
            struct menu_preview_motion_part *part =
                &menu_preview_scene.parts[index];
            int part_end = menu_preview_scaled_part_time(
                part->enter_delay +
                (part->enter_duration > 0
                    ? part->enter_duration
                    : CRAZYPOD_PREVIEW_ENTER_DURATION_MS));

            if(part_end > duration)
                duration = part_end;
        }
    }
    keep_cpu_boosted((duration * HZ + 999) / 1000 + HZ / 10);
    menu_preview_motion_reduced = reduced;
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_ENTERING;
    if(!start_menu_preview_timeline(duration)) {
        menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
        settle_menu_preview_scene(&menu_preview_scene);
    }
}

static void start_menu_preview_scene_exit(void)
{
    bool reduced = crazypod_state_reduce_motion();
    int duration = reduced
        ? CRAZYPOD_PREVIEW_REDUCED_DURATION_MS
        : CRAZYPOD_PREVIEW_EXIT_DURATION_MS;
    if(menu_preview_content == NULL)
        return;
    keep_cpu_boosted((duration * HZ + 999) / 1000 + HZ / 10);
    menu_preview_motion_reduced = reduced;
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_EXITING;
    if(!start_menu_preview_timeline(duration)) {
        menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
        render_menu_preview(current_route(), true);
    }
}

static lv_obj_t *preview_parent(void)
{
    return menu_preview_content != NULL
        ? menu_preview_content : product_content;
}

static void preview_add_bevel(
    lv_obj_t *object, int width, int height,
    uint32_t light, uint32_t dark)
{
    if(object == NULL || width < 8 || height < 8)
        return;
    make_box(object, 3, 2, width - 6, 1, 0, light, 72);
    make_box(object, 3, height - 3, width - 6, 1, 0, dark, 112);
}

static void preview_add_fastener(
    lv_obj_t *parent, int x, int y, uint32_t metal)
{
    lv_obj_t *fastener = make_box(
        parent, x, y, 5, 5, LV_RADIUS_CIRCLE,
        metal, LV_OPA_COVER);

    lv_obj_set_style_border_width(fastener, 1, 0);
    lv_obj_set_style_border_color(
        fastener, lv_color_hex(0x1C2022), 0);
    lv_obj_set_style_border_opa(fastener, 125, 0);
    make_box(fastener, 1, 2, 3, 1, 0, 0x303538, 185);
}

static lv_obj_t *make_preview_plinth(
    lv_obj_t *parent, int x, int y, int width,
    uint32_t top, uint32_t base)
{
    lv_obj_t *plinth = make_box(
        parent, x, y, width, 9, 3,
        base, LV_OPA_COVER);

    lv_obj_set_style_border_width(plinth, 1, 0);
    lv_obj_set_style_border_color(
        plinth, lv_color_hex(0x0B0D0E), 0);
    lv_obj_set_style_border_opa(plinth, 185, 0);
    make_box(plinth, 4, 1, width - 8, 2, 1,
             top, 205);
    make_box(plinth, 8, 6, width - 16, 1, 0,
             0x000000, 145);
    return plinth;
}

static void preview_add_paper_rules(
    lv_obj_t *paper, int width, int top, int count,
    int spacing, uint32_t ink)
{
    int index;

    make_box(paper, 9, top - 3, 1,
             count * spacing - 1, 0,
             0xC96F64, 62);
    for(index = 0; index < count; ++index)
        make_box(paper, 13, top + index * spacing,
                 width - 21 - (index == count - 1 ? 12 : 0),
                 1, 0, ink, index == 0 ? 105 : 68);
}

static lv_obj_t *make_preview_text_panel(int y, int height)
{
    lv_obj_t *parent = preview_parent();
    lv_obj_t *panel;

    if(height > CRAZYPOD_PREVIEW_TEXT_PANEL_MAX_HEIGHT)
        height = CRAZYPOD_PREVIEW_TEXT_PANEL_MAX_HEIGHT;
    panel = make_box(
        parent, 170, y, CRAZYPOD_PREVIEW_TEXT_PANEL_WIDTH, height,
        9, 0x11171A, 242);
    lv_obj_set_style_bg_grad_color(
        panel, lv_color_hex(0x060809), 0);
    lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(
        panel, lv_color_hex(0x89959A), 0);
    lv_obj_set_style_border_opa(panel, 72, 0);
    preview_add_bevel(
        panel, CRAZYPOD_PREVIEW_TEXT_PANEL_WIDTH,
        height, 0xEEF5F7, 0x000000);
    preview_add_fastener(panel, 5, 5, 0xAEB7BB);
    preview_add_fastener(
        panel, CRAZYPOD_PREVIEW_TEXT_PANEL_WIDTH - 10,
        5, 0xAEB7BB);
    return panel;
}

static void render_empty_state(const char *symbol_text,
                               const char *title, const char *message)
{
    lv_obj_t *symbol;
    lv_obj_t *label;

    if(symbol_text != NULL) {
        symbol = make_label(product_content, symbol_text,
                            &lv_font_montserrat_24, COLOR_WHITE, 80);
        lv_obj_set_pos(symbol, 148, 96);
    }
    else {
        lv_obj_t *outline = make_box(
            product_content, 145, 91, 30, 30,
            LV_RADIUS_CIRCLE, COLOR_WHITE, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(outline, 2, 0);
        lv_obj_set_style_border_color(
            outline, lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(outline, 80, 0);
        make_box(outline, 10, 6, 10, 10,
                 LV_RADIUS_CIRCLE, COLOR_WHITE, 80);
        make_box(outline, 6, 17, 18, 8,
                 LV_RADIUS_CIRCLE, COLOR_WHITE, 80);
    }
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
    lv_obj_t *sleeve = make_box(
        parent, x, y, size, size, size > 40 ? 5 : 3,
        primary, LV_OPA_COVER);
    lv_obj_t *label;
    const lv_image_dsc_t *descriptor = NULL;

    if(track != NULL) {
        if(menu_preview_build_defer_media) {
            descriptor = crazypod_artwork_load_priority(
                artwork_slot, track, size,
                CRAZYPOD_MENU_ARTWORK_PRIORITY);
            if(descriptor == NULL)
                menu_preview_media_deferred = true;
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
    preview_add_bevel(
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

static void prefetch_music_preview_artwork(
    const struct route_state *state)
{
    const struct crazypod_track *track = NULL;
    int i;
    int count;

    if(state == NULL)
        return;

    if(state->route == MUSIC_ROUTE_MENU) {
        if(state->selected == 0) {
            track = current_track();
            if(track != NULL)
                (void)crazypod_artwork_load_priority(
                    CRAZYPOD_PREVIEW_ARTWORK_SLOT, track,
                    CRAZYPOD_MENU_NOW_ARTWORK_SIZE,
                    CRAZYPOD_MENU_ARTWORK_PRIORITY);
        }
        else if(state->selected == 1) {
            count = crazypod_music_album_count();
            for(i = 0; i < 3 && i < count; ++i) {
                track = crazypod_music_album_track(i, 0);
                if(track != NULL)
                    (void)crazypod_artwork_load_priority(
                        CRAZYPOD_MENU_FLOW_ARTWORK_SLOT_BASE + i,
                        track, CRAZYPOD_MENU_FLOW_ARTWORK_SIZE,
                        CRAZYPOD_MENU_ARTWORK_PRIORITY);
            }
        }
        else if(state->selected == 5 &&
                crazypod_music_album_count() > 0) {
            track = crazypod_music_album_track(0, 0);
            if(track != NULL)
                (void)crazypod_artwork_load_priority(
                    CRAZYPOD_PREVIEW_ARTWORK_SLOT, track,
                    CRAZYPOD_MENU_ALBUM_ARTWORK_SIZE,
                    CRAZYPOD_MENU_ARTWORK_PRIORITY);
        }
        return;
    }

    if(state->route == MUSIC_ROUTE_ALBUMS)
        track = crazypod_music_album_track(state->selected, 0);
    else if(state->route == MUSIC_ROUTE_ARTISTS ||
            state->route == MUSIC_ROUTE_PLAYLISTS)
        return;
    else if(is_music_preview_route(state->route))
        track = route_track(state, state->selected);

    if(track != NULL)
        (void)crazypod_artwork_load_priority(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT, track,
            CRAZYPOD_MENU_ARTWORK_CACHE_SIZE,
            CRAZYPOD_MENU_ARTWORK_PRIORITY);
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
    preview_add_bevel(
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
        badge, initial, CRAZYPOD_METADATA_FONT,
        COLOR_WHITE, track != NULL ? 235 : 100);
    lv_obj_center(label);
    make_box(
        cover, size / 4, size - 6, size / 2, 2, 1,
        0xF4F7F8, track != NULL ? 68 : 30);
    return cover;
}

static void render_root_preview(int selected)
{
    lv_obj_t *parent = preview_parent();
    lv_obj_t *text_panel;
    lv_obj_t *title;
    lv_obj_t *detail;
    lv_obj_t *stage;
    lv_obj_t *part;
    char count_text[96];
    int count = 0;
    int i;

    switch(selected) {
    case 0: {
        const struct crazypod_track *track = current_track();
        lv_obj_t *disc = make_box(
            parent, 242, 69, 59, 59,
            LV_RADIUS_CIRCLE, 0xC7D1D8, 235);
        lv_obj_set_style_border_width(disc, 2, 0);
        lv_obj_set_style_border_color(
            disc, lv_color_hex(0xF8FFFF), 0);
        lv_obj_set_style_border_opa(disc, 105, 0);
        part = make_box(
            disc, 4, 4, 51, 51, LV_RADIUS_CIRCLE,
            0xD9E4E8, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(0xF7FCFF), 0);
        lv_obj_set_style_border_opa(part, 96, 0);
        make_box(disc, 9, 9, 41, 41, LV_RADIUS_CIRCLE,
                 0x627582, 215);
        part = make_box(
            disc, 14, 14, 31, 31, LV_RADIUS_CIRCLE,
            0x738693, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(0xD7E5EA), 0);
        lv_obj_set_style_border_opa(part, 65, 0);
        make_box(disc, 23, 23, 13, 13, LV_RADIUS_CIRCLE,
                 0x101820, LV_OPA_COVER);
        make_box(disc, 27, 27, 5, 5, LV_RADIUS_CIRCLE,
                 0xE9F2F5, 195);
        make_preview_plinth(
            parent, 184, 143, 118, 0xB9C3C7, 0x283035);
        stage = make_box(parent, 190, 50, 82, 91, 5,
                         0xD9E4E8, 58);
        lv_obj_set_style_border_width(stage, 2, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xEAFBFF), 0);
        lv_obj_set_style_border_opa(stage, 145, 0);
        preview_add_bevel(
            stage, 82, 91, 0xF4FBFD, 0x182025);
        preview_add_fastener(stage, 3, 3, 0xB7C1C5);
        preview_add_fastener(stage, 74, 3, 0xB7C1C5);
        make_procedural_record_sleeve(
            stage, track, 7, 7, CRAZYPOD_MENU_NOW_ARTWORK_SIZE, 0,
            CRAZYPOD_PREVIEW_ARTWORK_SLOT, false);
        part = make_box(stage, 8, 80, 66, 4,
                        LV_RADIUS_CIRCLE, 0x20313A, 95);
        make_box(part, 0, 0, track != NULL ? 24 : 3, 4,
                 LV_RADIUS_CIRCLE, COLOR_CYAN, 225);
        for(i = 0; i < 4; ++i)
            make_box(parent, 199 + i * 18, 146, 10, 3,
                     LV_RADIUS_CIRCLE, COLOR_CYAN,
                     i < 2 ? 210 : 55);
        menu_preview_register_motion(
            disc, 24, 0, 214, 110, 0, 20, 260,
            -24, 0, 214, -80);
        menu_preview_register_motion(
            stage, 10, -8, 230, -25, 0, 0, 240,
            -9, 5, 230, 20);
        count = crazypod_music_track_count();
        break;
    }
    case 1: {
        count = crazypod_music_album_count();
        static const int positions[] = { 180, 211, 242 };
        static const int rotations[] = { -90, 0, 90 };
        make_preview_plinth(
            parent, 178, 143, 122, 0xC5CED2, 0x242A2D);
        make_box(parent, 183, 139, 112, 3,
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
            menu_preview_register_motion(
                part, (i - 1) * 24, 12, 178,
                rotations[i] + (i - 1) * 80, 0,
                i * 40, 280,
                (i - 1) * -20, 8, 184,
                rotations[i] + (i - 1) * -60);
        }
        part = make_box(parent, 257, 108, 34, 34,
                        LV_RADIUS_CIRCLE, 0xD7E2E8, 205);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(0xF4FBFD), 0);
        lv_obj_set_style_border_opa(part, 115, 0);
        make_box(part, 5, 5, 24, 24,
                 LV_RADIUS_CIRCLE, 0xAAB7BD, 85);
        make_box(part, 12, 12, 10, 10,
                 LV_RADIUS_CIRCLE, 0x25313A, 245);
        make_box(part, 15, 15, 4, 4,
                 LV_RADIUS_CIRCLE, 0xE8EFF1, 210);
        menu_preview_register_motion(
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
        make_preview_plinth(
            parent, 175, 153, 130, 0xBFC9CD, 0x22282B);
        stage = make_box(parent, 176, 47, 128, 106, 8,
                         0x394247, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            stage, lv_color_hex(0x171C1F), 0);
        lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(stage, 2, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xAAB5BA), 0);
        lv_obj_set_style_border_opa(stage, 135, 0);
        preview_add_bevel(
            stage, 128, 106, 0xECF3F5, 0x080A0B);
        preview_add_fastener(stage, 5, 5, 0xB8C2C6);
        preview_add_fastener(stage, 118, 5, 0xB8C2C6);
        make_box(stage, 16, 7, 96, 3, 1,
                 0xD5DEE1, 72);
        title = make_label(
            stage, "A  /  Z", &lv_font_montserrat_8,
            0xD9E3E6, 125);
        lv_obj_set_width(title, 64);
        lv_obj_set_style_text_align(
            title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(title, 32, 10);
        cavity = make_box(
            stage, 5, 20, 118, 77, 5,
            0x07090A, 238);
        lv_obj_set_style_border_width(cavity, 1, 0);
        lv_obj_set_style_border_color(
            cavity, lv_color_hex(0x7E8A8F), 0);
        lv_obj_set_style_border_opa(cavity, 82, 0);
        make_box(cavity, 4, 4, 110, 2, 1,
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
            menu_preview_register_motion(
                part, (i - 1) * 5, 34 + i * 3,
                210, rotations[i], 0,
                i * 35, 260,
                (i - 1) * 7, 28,
                210, rotations[i]);
        }
        rail = make_box(stage, 5, 78, 118, 20, 4,
                        0x2C3438, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            rail, lv_color_hex(0x111619), 0);
        lv_obj_set_style_bg_grad_dir(rail, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(rail, 1, 0);
        lv_obj_set_style_border_color(
            rail, lv_color_hex(0x9DA8AD), 0);
        lv_obj_set_style_border_opa(rail, 112, 0);
        preview_add_bevel(
            rail, 118, 20, 0xE1E8EA, 0x050708);
        preview_add_fastener(rail, 6, 7, 0xAAB4B8);
        preview_add_fastener(rail, 107, 7, 0xAAB4B8);
        make_box(rail, 28, 8, 62, 4, 2,
                 0x060809, 210);
        make_box(rail, 39, 9, 40, 2, 1,
                 COLOR_CYAN, 105);
        menu_preview_register_motion(
            stage, 0, 13, 232, 0, 0, 0, 260,
            -8, 8, 232, 0);
        break;
    }
    case 3: {
        count = crazypod_music_playlist_count();
        stage = make_box(parent, 184, 60, 112, 84, 9,
                         0xB7A986, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            stage, lv_color_hex(0x544936), 0);
        lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(stage, 2, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xD8C99F), 0);
        lv_obj_set_style_border_opa(stage, 125, 0);
        preview_add_bevel(
            stage, 112, 84, 0xE8D9B4, 0x20170F);
        preview_add_fastener(stage, 5, 5, 0xB9A982);
        preview_add_fastener(stage, 102, 5, 0xB9A982);
        make_box(stage, 13, 11, 86, 15, 2, 0xE8E0CA, 215);
        title = make_label(
            stage, "PLAYLIST / A",
            &lv_font_montserrat_8, 0x443A2E, 155);
        lv_obj_set_pos(title, 25, 14);
        part = make_box(stage, 15, 32, 82, 36, 4,
                        0x24211D, 235);
        lv_obj_set_style_border_width(part, 1, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(0xB7A986), 0);
        lv_obj_set_style_border_opa(part, 72, 0);
        for(i = 0; i < 2; ++i) {
            lv_obj_t *reel = make_box(
                part, 12 + i * 43, 6, 24, 24,
                LV_RADIUS_CIRCLE, 0xD8D1BC, 235);
            make_box(reel, 7, 7, 10, 10,
                     LV_RADIUS_CIRCLE, 0x34312C, 240);
            make_box(reel, 10, 10, 4, 4,
                     LV_RADIUS_CIRCLE, 0xE6DDC8, 185);
            menu_preview_register_motion(
                reel, 0, -5, 184, (i ? 120 : -120), 0,
                60 + i * 30, 240,
                0, 4, 184, i ? 140 : -140);
        }
        make_box(part, 30, 16, 22, 2,
                 LV_RADIUS_CIRCLE, 0x8F7651, 115);
        make_box(stage, 47, 70, 18, 8, 2,
                 0xBFC6C8, 220);
        make_box(stage, 54, 70, 3, 8, 1,
                 0x34383A, 190);
        make_box(stage, 27, 76, 58, 2,
                 LV_RADIUS_CIRCLE, 0xF4D35E, 145);
        menu_preview_register_motion(
            stage, 0, -13, 228, -18, 0, 0, 260,
            0, -9, 228, 15);
        break;
    }
    case 4: {
        count = crazypod_music_artist_count();
        make_box(parent, 184, 55, 112, 92, 46,
                 0xD8B96B, 22);
        stage = make_box(
            parent, 211, 47, 58, 70, 25,
            0xC7D0D4, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(stage, 2, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0x818B90), 0);
        lv_obj_set_style_border_opa(stage, 165, 0);
        part = make_box(parent, 218, 53, 44, 58, 21,
                        0xB8C1C6, 245);
        lv_obj_set_style_bg_grad_color(
            part, lv_color_hex(0x3A4146), 0);
        lv_obj_set_style_bg_grad_dir(part, LV_GRAD_DIR_HOR, 0);
        for(i = 0; i < 5; ++i)
            make_box(part, 8, 10 + i * 8, 28, 2,
                     LV_RADIUS_CIRCLE, 0x171A1D, 145);
        for(i = 0; i < 3; ++i)
            make_box(part, 11 + i * 10, 8, 1, 42,
                     0, 0xF0F4F5, 45);
        make_box(parent, 228, 109, 24, 8, 3,
                 0x7A858A, 235);
        stage = make_box(parent, 237, 107, 6, 36, 3,
                         0xAEB7BC, 245);
        make_preview_plinth(
            parent, 213, 139, 54, 0xD6DEE1, 0x697277);
        make_box(parent, 264, 142, 23, 2,
                 LV_RADIUS_CIRCLE, 0x6A7174, 120);
        menu_preview_register_motion(
            part, 0, 22, 204, 0, 0, 20, 280,
            0, 20, 204, 0);
        menu_preview_register_motion(
            stage, 0, 15, 224, 0, 0, 0, 240,
            0, 12, 224, 0);
        break;
    }
    case 5: {
        const struct crazypod_track *track =
            crazypod_music_album_count() > 0
                ? crazypod_music_album_track(0, 0) : NULL;
        count = crazypod_music_album_count();
        stage = make_box(parent, 178, 62, 124, 83, 8,
                         0x50463B, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            stage, lv_color_hex(0x201C18), 0);
        lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0x8D785F), 0);
        lv_obj_set_style_border_opa(stage, 135, 0);
        preview_add_bevel(
            stage, 124, 83, 0xAE9471, 0x120D09);
        preview_add_fastener(stage, 5, 5, 0x8F826F);
        preview_add_fastener(stage, 114, 72, 0x8F826F);
        part = make_box(stage, 55, 12, 62, 62,
                        LV_RADIUS_CIRCLE, 0x111111, LV_OPA_COVER);
        make_box(part, 10, 10, 42, 42,
                 LV_RADIUS_CIRCLE, 0x252525, 235);
        make_box(part, 16, 16, 30, 30,
                 LV_RADIUS_CIRCLE, 0x171717, 235);
        make_box(part, 25, 25, 12, 12,
                 LV_RADIUS_CIRCLE, 0xD8A94D, 245);
        make_box(part, 29, 29, 4, 4,
                 LV_RADIUS_CIRCLE, 0xF5E8CA, 225);
        menu_preview_register_motion(
            part, 0, 0, 192, -180, 0, 20, 300,
            0, 0, 192, 180);
        part = make_procedural_record_sleeve(
            parent, track, 183, 54,
            CRAZYPOD_MENU_ALBUM_ARTWORK_SIZE, 23,
            CRAZYPOD_PREVIEW_ARTWORK_SLOT, false);
        menu_preview_register_motion(
            part, -28, 0, 224, -35, 0, 0, 280,
            -25, 0, 224, -45);
        part = make_box(stage, 105, 8, 4, 54, 2,
                        0xD8D4C8, 240);
        lv_obj_set_style_transform_rotation(part, 160, 0);
        make_box(stage, 99, 5, 15, 15,
                 LV_RADIUS_CIRCLE, 0x969A9A, 245);
        make_box(stage, 104, 10, 5, 5,
                 LV_RADIUS_CIRCLE, 0x303234, 230);
        make_box(stage, 98, 57, 18, 7, 3, 0xD8D4C8, 235);
        menu_preview_register_motion(
            part, 8, -7, 220, 310, 0, 80, 240,
            8, -7, 220, 310);
        break;
    }
    case 6: {
        count = crazypod_music_track_count();
        stage = make_box(parent, 181, 61, 118, 84, 7,
                         0x78634D, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            stage, lv_color_hex(0x3B2D23), 0);
        lv_obj_set_style_bg_grad_dir(stage, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xB99A74), 0);
        lv_obj_set_style_border_opa(stage, 120, 0);
        preview_add_bevel(
            stage, 118, 84, 0xC7AB87, 0x1B120D);
        preview_add_fastener(stage, 4, 4, 0x9A836D);
        preview_add_fastener(stage, 109, 4, 0x9A836D);
        make_box(stage, 8, 9, 102, 12, 3,
                 0x251F1A, 210);
        make_box(stage, 13, 13, 34, 3, 1,
                 0xD8C7A7, 115);
        for(i = 3; i >= 0; --i) {
            part = make_box(stage, 16 + i * 4, 28 + i * 12,
                            82, 29, 3, 0xF1E4C9, LV_OPA_COVER);
            lv_obj_set_style_border_width(part, 1, 0);
            lv_obj_set_style_border_color(
                part, lv_color_hex(0xC8B997), 0);
            lv_obj_set_style_border_opa(part, 105, 0);
            make_box(part, 58, 2, 16, 4, 1,
                     0xD6C39F, 165);
            make_box(part, 8, 8, 47 - i * 3, 2, 1,
                     0x6A5C4D, 105);
            make_box(part, 8, 15, 60, 2, 1,
                     0x6A5C4D, 70);
            menu_preview_register_motion(
                part, 0, 18 + i * 5, 230, i * 12, 0,
                (3 - i) * 30, 240,
                0, 19 + i * 4, 230, i * -12);
        }
        menu_preview_register_motion(
            stage, 0, 8, 238, 0, 0, 0, 220,
            0, -6, 238, 0);
        break;
    }
    default: {
        count = crazypod_music_track_count();
        stage = make_box(parent, 187, 58, 99, 91, 5,
                         0xF0E6D2, LV_OPA_COVER);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xC7B99C), 0);
        lv_obj_set_style_border_opa(stage, 135, 0);
        preview_add_bevel(
            stage, 99, 91, 0xFFFFFF, 0x8D806D);
        preview_add_paper_rules(
            stage, 99, 18, 5, 13, 0x6E7780);
        make_box(stage, 13, 9, 42, 3, 1,
                 0x384A58, 120);
        make_box(stage, 84, 3, 10, 10, 1,
                 0xD4C7AC, 210);
        part = make_box(parent, 218, 68, 45, 45,
                        LV_RADIUS_CIRCLE, 0x8DD9EA, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(part, 7, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(COLOR_CYAN), 0);
        lv_obj_set_style_border_opa(part, 235, 0);
        title = make_box(
            part, 6, 6, 33, 33, LV_RADIUS_CIRCLE,
            0xD7F5FA, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(title, 1, 0);
        lv_obj_set_style_border_color(
            title, lv_color_hex(0xE9FCFF), 0);
        lv_obj_set_style_border_opa(title, 115, 0);
        title = make_box(parent, 258, 107, 8, 34, 4,
                         0x76858C, 245);
        preview_add_bevel(
            title, 8, 34, 0xE3EAEC, 0x262E31);
        make_box(title, 2, 22, 4, 8, 2,
                 0x23282A, 220);
        menu_preview_register_motion(
            stage, 0, 10, 236, 0, 0, 0, 240,
            -8, 4, 236, 0);
        menu_preview_register_motion(
            part, 38, -4, 190, 120, 0, 40, 300,
            34, 5, 190, 160);
        break;
    }
    }

    text_panel = make_preview_text_panel(158, 58);
    title = make_label(
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
    detail = make_label(text_panel, count_text,
                        &lv_font_montserrat_8,
                        COLOR_WHITE, 125);
    lv_obj_set_width(detail, 132);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(detail, 4, 32);
    menu_preview_register_motion(
        text_panel, 0, 10, 246, 0, 0, 70, 220,
        0, 6, 246, 0);
}

static void render_item_preview(const struct route_state *state)
{
    const struct crazypod_track *track =
        route_track(state, state->selected);
    lv_obj_t *parent = preview_parent();
    lv_obj_t *text_panel = make_preview_text_panel(153, 70);
    lv_obj_t *title;
    lv_obj_t *detail;
    char text[96];

    if(state->route == MUSIC_ROUTE_ARTISTS) {
        const char *artist = crazypod_music_artist(state->selected);
        int count = crazypod_music_artist_track_count(state->selected);
        make_music_preview_icon(
            parent, CRAZYPOD_PREVIEW_ICON_ARTIST, 192, 52);
        title = make_label(text_panel, artist != NULL ? artist : "",
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, LV_OPA_COVER);
        snprintf(text, sizeof(text), "%d songs", count);
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
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, LV_OPA_COVER);
        detail = make_label(text_panel,
                            album != NULL ? album->artist : "",
                            CRAZYPOD_METADATA_FONT, COLOR_WHITE, 135);
    }
    else if(state->route == MUSIC_ROUTE_PLAYLISTS) {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(state->selected);
        make_music_preview_icon(
            parent, CRAZYPOD_PREVIEW_ICON_PLAYLISTS, 192, 52);
        title = make_label(text_panel,
                           playlist != NULL ? playlist->name : "",
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, LV_OPA_COVER);
        snprintf(text, sizeof(text), "%d songs",
                 playlist != NULL ? playlist->track_count : 0);
        detail = make_label(text_panel, text, &lv_font_montserrat_8,
                            COLOR_WHITE, 130);
    }
    else {
        char duration[16];
        create_artwork(parent, track, 204, 72, 72,
                       CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        title = make_label(text_panel,
                           track != NULL ? track->title : "No Track",
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE, LV_OPA_COVER);
        detail = make_label(text_panel,
                            track != NULL ? track->artist : "",
                            CRAZYPOD_METADATA_FONT, COLOR_WHITE, 155);
        if(track != NULL) {
            format_time_ms(track->duration_ms, duration, sizeof(duration));
            snprintf(text, sizeof(text), "%s  " LV_SYMBOL_BULLET "  %s",
                     track->album, duration);
            {
                lv_obj_t *album = make_label(text_panel, text,
                                              CRAZYPOD_METADATA_FONT,
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
        if(field != CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS &&
           field != CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS)
            return appearance_choice_title(field,
                                           appearance_choice_index(field));
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
        enum crazypod_appearance_field background_field =
            background_field_for_index(state->selected);
        const char *path =
            background_wallpaper(value, background_field);
        int color = appearance_field_value(background_field);
        if(path[0] != '\0')
            return path_basename(path);
        return color == 0
            ? background_field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND
                ? "Follow Home" : "Default"
                          : crazypod_appearance_color_name(color - 1);
    }
    if(state->route == DIY_ROUTE_BACKGROUND_CHOICES) {
        const char *path = background_wallpaper(
            value, (enum crazypod_appearance_field)state->group);
        int color = appearance_field_value(
            (enum crazypod_appearance_field)state->group);
        if(state->selected == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1)
            return path[0] != '\0'
                ? path_basename(path) : "Open /Pictures";
        return path[0] == '\0' && state->selected == color
            ? "Current selection" : "Select to apply";
    }
    if(state->route == DIY_ROUTE_WALLPAPER_FILES) {
        const char *current_path = background_wallpaper(
            value, (enum crazypod_appearance_field)state->group);
        return strcmp(current_path,
                      crazypod_photo_path(state->selected)) == 0
            ? "Current picture" : "Select to crop";
    }
    return "";
}

static void render_editor_preview(const char *value, const char *empty_text,
                                  const char *detail)
{
    lv_obj_t *parent = preview_parent();
    lv_obj_t *card;
    lv_obj_t *symbol;
    lv_obj_t *label;

    card = make_box(parent, 181, 78, 118, 64, 14,
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

    card = make_preview_text_panel(154, 46);
    label = make_label(card, detail, &lv_font_montserrat_8,
                       COLOR_WHITE, 125);
    lv_obj_set_pos(label, 11, 8);
    lv_obj_set_width(label, 118);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

static void render_search_screen(const struct route_state *state)
{
    int count = route_item_count(state);
    int start;
    int row;
    int result_count = search_query[0] != '\0'
        ? crazypod_music_search_count(search_query) : 0;
    lv_obj_t *label;
    lv_obj_t *query_box;
    char text[96];

    menu_view.valid = true;
    menu_view.route = state->route;
    create_panel_backgrounds();

    label = make_label(product_content, "SEARCH",
                       CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, 85);
    lv_obj_set_pos(label, CRAZYPOD_MENU_HEADER_X,
                   CRAZYPOD_MENU_HEADER_Y);
    lv_obj_set_width(label, CRAZYPOD_MENU_HEADER_WIDTH);
    lv_obj_set_height(label, CRAZYPOD_MENU_HEADER_HEIGHT);

    prepare_glass_descriptor(
        170, 43, 136, 38, CRAZYPOD_GLASS_TEXT_PANEL,
        search_query_glass_pixels, &search_query_glass_descriptor);
    query_box = make_glass_material_panel(
        product_content, 170, 43, 136, 38, 12,
        CRAZYPOD_GLASS_TEXT_PANEL, &search_query_glass_descriptor);
    if(search_query[0] != '\0') {
        lv_obj_t *active = make_box(
            query_box, 0, 0, 136, 38, 12,
            highlight_primary(), 82);

        if(crazypod_appearance_get()->highlight_style != 0) {
            lv_obj_set_style_bg_grad_color(
                active, lv_color_hex(highlight_secondary()), 0);
            lv_obj_set_style_bg_grad_dir(active, LV_GRAD_DIR_HOR, 0);
        }
        lv_obj_remove_flag(active, LV_OBJ_FLAG_CLICKABLE);
    }
    label = make_label(query_box, LV_SYMBOL_KEYBOARD,
                       &lv_font_montserrat_12,
                       COLOR_WHITE, search_query[0] != '\0' ? 235 : 90);
    lv_obj_set_pos(label, 10, 12);
    label = make_label(query_box,
                       search_query[0] != '\0'
                           ? search_query : "Start typing",
                       CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE,
                       search_query[0] != '\0' ? 255 : 120);
    lv_obj_set_pos(label, 31, 10);
    lv_obj_set_width(label, 92);
    lv_obj_set_height(label, 18);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

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
        int y = CRAZYPOD_MENU_ROW_Y + row * CRAZYPOD_MENU_ROW_STEP;
        bool selected = index == state->selected;
        lv_obj_t *row_box;
        lv_obj_t *marker;
        const char *title;

        if(index >= count)
            break;

        row_box = make_box(product_content,
                           CRAZYPOD_MENU_ROW_X, y,
                           CRAZYPOD_MENU_ROW_WIDTH,
                           CRAZYPOD_MENU_ROW_HEIGHT, 8,
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

        title = route_item_title(state, index);
        label = make_label(row_box, title != NULL ? title : "",
                           CRAZYPOD_METADATA_FONT,
                           COLOR_WHITE,
                           selected ? 255 : 150);
        lv_obj_set_pos(label, 14, 5);
        lv_obj_set_width(label, 104);
        lv_obj_set_height(label, 16);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

        marker = make_label(row_box,
                            selected ? LV_SYMBOL_PLAY : "",
                            &lv_font_montserrat_8,
                            COLOR_WHITE, selected ? 205 : 75);
        lv_obj_set_pos(marker, 128, 8);
        menu_view.labels[row] = label;
        menu_view.markers[row] = marker;
    }

    prepare_glass_descriptor(
        170, 91, 136, 104, CRAZYPOD_GLASS_TEXT_PANEL,
        search_results_glass_pixels, &search_results_glass_descriptor);
    make_glass_material_panel(
        product_content, 170, 91, 136, 104, 12,
        CRAZYPOD_GLASS_TEXT_PANEL, &search_results_glass_descriptor);
    snprintf(text, sizeof(text), "%d match%s",
             result_count, result_count == 1 ? "" : "es");
    label = make_label(product_content,
                       search_query[0] != '\0'
                           ? text : "Live results",
                       &lv_font_montserrat_10,
                       COLOR_WHITE,
                       search_query[0] != '\0' ? 205 : 105);
    lv_obj_set_pos(label, 182, 101);
    lv_obj_set_width(label, 112);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    if(search_query[0] == '\0') {
        label = make_label(product_content,
                           "Choose a letter, then press Select.",
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 100);
        lv_obj_set_pos(label, 182, 125);
        lv_obj_set_width(label, 112);
        lv_obj_set_height(label, 40);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    }
    else if(result_count <= 0) {
        label = make_label(product_content,
                           "No title, artist or album matched.",
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 105);
        lv_obj_set_pos(label, 182, 125);
        lv_obj_set_width(label, 112);
        lv_obj_set_height(label, 45);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    }
    else {
        int shown = result_count < CRAZYPOD_SEARCH_PREVIEW_ROWS
            ? result_count : CRAZYPOD_SEARCH_PREVIEW_ROWS;
        int i;
        for(i = 0; i < shown; ++i) {
            const struct crazypod_track *track =
                crazypod_music_search_track(search_query, i);
            int y = 124 + i * 17;
            if(track == NULL)
                continue;
            label = make_label(product_content, track->title,
                               &lv_font_montserrat_8,
                               COLOR_WHITE, i == 0 ? 210 : 145);
            lv_obj_set_pos(label, 182, y);
            lv_obj_set_width(label, 112);
            lv_obj_set_height(label, 10);
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
            label = make_label(product_content, track->artist,
                               &lv_font_montserrat_8,
                               COLOR_WHITE, 75);
            lv_obj_set_pos(label, 182, y + 9);
            lv_obj_set_width(label, 112);
            lv_obj_set_height(label, 10);
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        }
    }

    label = make_label(product_content,
                       "Wheel Choose  Select Action",
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 125);
    lv_obj_set_pos(label, 174, 202);
    lv_obj_set_width(label, 128);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    label = make_label(product_content,
                       "Choose View Results to listen",
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 95);
    lv_obj_set_pos(label, 174, 216);
    lv_obj_set_width(label, 128);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

static void render_diy_preview(const struct route_state *state)
{
    const char *title = route_item_title(state, state->selected);
    const char *detail = diy_current_value(state);
    const char *symbol = LV_SYMBOL_SETTINGS;
    uint32_t swatch_color = highlight_primary();
    lv_obj_t *parent = preview_parent();
    lv_obj_t *text_panel;
    lv_obj_t *glyph;
    lv_obj_t *label;
    lv_obj_t *swatch;

    if(state->route == DIY_ROUTE_MENU) {
        symbol = diy_menu_symbols[state->selected];
        detail = state->selected == 0 ? "Save and reuse appearances" :
                 state->selected == 1 ? "16 complete icon themes" :
                 state->selected == 2 ? "Wave, size, glow and colors" :
                 state->selected == 3 ? "Home, menu and lock pictures" :
                                       "Screen corner radius";
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
        if(field == CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE)
            symbol = LV_SYMBOL_AUDIO;
        if(field == CRAZYPOD_APPEARANCE_PRIMARY ||
           field == CRAZYPOD_APPEARANCE_SECONDARY)
            swatch_color = crazypod_appearance_color(value);
    }
    else if(state->route == DIY_ROUTE_BACKGROUNDS) {
        enum crazypod_appearance_field background_field =
            background_field_for_index(state->selected);
        int surface = appearance_field_value(background_field);
        symbol = LV_SYMBOL_DIRECTORY;
        swatch_color = surface == 0
            ? background_default_color(background_field)
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
    else if(state->route == DIY_ROUTE_DETAILS) {
        if(state->selected == 1)
            symbol = LV_SYMBOL_AUDIO;
        else if(state->selected == 5)
            swatch_color = highlight_secondary();
    }

    swatch = make_box(parent, 204, 76, 72, 72, 16,
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

    text_panel = make_preview_text_panel(158, 50);
    label = make_label(text_panel, title != NULL ? title : "",
                       &lv_font_montserrat_12,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 6);
    label = make_label(text_panel, detail, &lv_font_montserrat_8,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 30);
}

static void render_settings_preview(const struct route_state *state)
{
    const char *title = route_item_title(state, state->selected);
    const char *detail = "";
    const char *symbol = LV_SYMBOL_SETTINGS;
    uint32_t swatch_color = highlight_primary();
    lv_obj_t *parent = preview_parent();
    lv_obj_t *text_panel;
    lv_obj_t *swatch;
    lv_obj_t *glyph;
    lv_obj_t *label;
    char detail_text[48];
    int item;

    if(state->route == SETTINGS_ROUTE_MENU) {
        symbol = settings_menu_symbols[state->selected];
        detail = settings_group_detail(state->selected);
    }
    else if(state->route == SETTINGS_ROUTE_MAIN_MENU) {
        struct crazypod_app *app = ordered_app(state->selected);
        enum crazypod_app_id id = app != NULL
            ? app->id : CRAZYPOD_APP_INVALID;
        symbol = app != NULL ? app->symbol : LV_SYMBOL_LIST;
        swatch_color = app != NULL ? app->color : highlight_primary();
        snprintf(detail_text, sizeof(detail_text), "%s · Position %d",
                 crazypod_apps_is_enabled(id) ? "Visible" : "In More",
                 state->selected + 1);
        detail = detail_text;
    }
    else if(state->route == SETTINGS_ROUTE_MAIN_MENU_ACTIONS) {
        struct crazypod_app *app =
            app_for_id((enum crazypod_app_id)state->group);
        int action = state->selected +
            (crazypod_apps_is_fixed(
                 (enum crazypod_app_id)state->group) ? 1 : 0);
        symbol = app != NULL ? app->symbol : LV_SYMBOL_LIST;
        swatch_color = app != NULL ? app->color : highlight_primary();
        detail = action == 0
            ? "Changes More Features"
            : "Changes launcher position";
    }
    else {
        item = settings_route_item(state->route, state->selected);
        symbol = settings_item_symbol(item);
        detail = settings_item_value_label(item);
        if(item == SETTINGS_ITEM_EQ_ENABLED &&
           global_settings.eq_enabled)
            swatch_color = 0x26CFF5;
        else if(item == SETTINGS_ITEM_SHUFFLE &&
                global_settings.playlist_shuffle)
            swatch_color = 0xFF375F;
        else if(item == SETTINGS_ITEM_REPEAT &&
                crazypod_queue_repeat() != REPEAT_OFF)
            swatch_color = 0x30D158;
    }

    swatch = make_box(parent, 204, 76, 72, 72, 16,
                      swatch_color, LV_OPA_COVER);
    if(crazypod_appearance_get()->highlight_style != 0) {
        lv_obj_set_style_bg_grad_color(
            swatch, lv_color_hex(highlight_secondary()), 0);
        lv_obj_set_style_bg_grad_dir(swatch, LV_GRAD_DIR_HOR, 0);
    }
    glyph = make_label(swatch, symbol, &lv_font_montserrat_24,
                       COLOR_WHITE, 225);
    lv_obj_center(glyph);

    text_panel = make_preview_text_panel(158, 50);
    label = make_label(text_panel, title != NULL ? title : "",
                       &lv_font_montserrat_12,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 6);
    label = make_label(text_panel, detail,
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 30);
}

static int photo_route_index(const struct route_state *state, int position)
{
    if(state->route == PHOTOS_ROUTE_FAVORITES)
        return crazypod_photo_favorite_index(position);
    if(state->route == PHOTOS_ROUTE_LIBRARY ||
       state->route == DIY_ROUTE_WALLPAPER_FILES)
        return position;
    return -1;
}

static int photo_index_for_path(const char *path)
{
    int index;

    if(path == NULL || path[0] == '\0')
        return 0;
    for(index = 0; index < crazypod_photo_count(); ++index) {
        if(strcmp(path, crazypod_photo_path(index)) == 0)
            return index;
    }
    return 0;
}

static lv_obj_t *render_photo_image(
    lv_obj_t *parent, const lv_image_dsc_t *descriptor,
    int x, int y, int width, int height)
{
    lv_obj_t *image;
    uint32_t scale_x;
    uint32_t scale_y;
    uint32_t scale;
    int display_width;
    int display_height;

    if(descriptor == NULL || descriptor->header.w <= 0 ||
       descriptor->header.h <= 0)
        return NULL;
    scale_x = (uint32_t)width * LV_SCALE_NONE /
        descriptor->header.w;
    scale_y = (uint32_t)height * LV_SCALE_NONE /
        descriptor->header.h;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if(scale > LV_SCALE_NONE)
        scale = LV_SCALE_NONE;
    if(scale == 0)
        scale = 1;
    display_width = descriptor->header.w * scale / LV_SCALE_NONE;
    display_height = descriptor->header.h * scale / LV_SCALE_NONE;
    image = lv_image_create(parent);
    lv_image_set_src(image, descriptor);
    lv_image_set_scale(image, scale);
    lv_obj_set_pos(image,
                   x + (width - display_width) / 2,
                   y + (height - display_height) / 2);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    return image;
}

static void render_procedural_photo(
    lv_obj_t *parent, int x, int y,
    int width, int height, int seed)
{
    static const uint32_t sky_colors[] = {
        0x8EC5D8, 0xD7A7B8, 0x8AB89B, 0xC8A66A
    };
    static const uint32_t ground_colors[] = {
        0x385869, 0x744A61, 0x46654F, 0x725537
    };
    lv_obj_t *scene = make_box(
        parent, x, y, width, height, 2,
        sky_colors[seed % 4], LV_OPA_COVER);
    int horizon = height * 2 / 3;

    lv_obj_set_style_bg_grad_color(
        scene,
        lv_color_hex(
            sky_colors[(seed + 1) % 4]), 0);
    lv_obj_set_style_bg_grad_dir(
        scene, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(scene, 1, 0);
    lv_obj_set_style_border_color(
        scene, lv_color_hex(0xF8FAF8), 0);
    lv_obj_set_style_border_opa(scene, 62, 0);
    make_box(
        scene, width * 2 / 3, height / 7,
        width / 7, width / 7,
        LV_RADIUS_CIRCLE, 0xF7DE8B, 210);
    make_box(
        scene, 0, horizon, width, height - horizon,
        0, ground_colors[seed % 4], LV_OPA_COVER);
    make_box(
        scene, width / 8, horizon - height / 5,
        width * 3 / 5, height / 4,
        height / 8, ground_colors[(seed + 1) % 4], 210);
    make_box(
        scene, width * 3 / 4, horizon - height / 9,
        width / 9, height / 3,
        1, 0x253B35, 145);
}

static void format_media_duration(char *buffer, size_t size,
                                  uint32_t seconds)
{
    uint32_t hours = seconds / 3600u;
    uint32_t minutes = seconds / 60u % 60u;
    uint32_t remainder = seconds % 60u;

    if(seconds == 0)
        snprintf(buffer, size, "--:--");
    else if(hours > 0)
        snprintf(buffer, size, "%lu:%02lu:%02lu",
                 (unsigned long)hours, (unsigned long)minutes,
                 (unsigned long)remainder);
    else
        snprintf(buffer, size, "%lu:%02lu",
                 (unsigned long)minutes, (unsigned long)remainder);
}

static lv_obj_t *render_video_card(lv_obj_t *parent, int video_index,
                                   int x, int y, int width, int height)
{
    const lv_image_dsc_t *poster = NULL;
    lv_obj_t *card = make_box(parent, x, y, width, height, 6,
                              0x0B0D12, LV_OPA_COVER);
    lv_obj_t *play;

    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xAEB7C7), 0);
    lv_obj_set_style_border_opa(card, 120, 0);
    if(video_index >= 0) {
        if(menu_preview_build_defer_media)
            menu_preview_media_deferred = true;
        else
            poster = crazypod_video_poster(video_index);
    }
    if(poster != NULL)
        (void)render_photo_image(card, poster, 3, 3,
                                 width - 6, height - 6);
    else {
        lv_obj_t *empty = make_label(
            card, LV_SYMBOL_IMAGE, &lv_font_montserrat_24,
            COLOR_WHITE, 55);
        lv_obj_center(empty);
    }
    play = make_box(card, width / 2 - 15, height / 2 - 15,
                    30, 30, LV_RADIUS_CIRCLE,
                    0x05070A, 190);
    {
        lv_obj_t *symbol = make_label(
            play, LV_SYMBOL_PLAY, &lv_font_montserrat_12,
            COLOR_WHITE, LV_OPA_COVER);
        lv_obj_center(symbol);
    }
    return card;
}

static void render_videos_preview(const struct route_state *state)
{
    int count = crazypod_video_count();
    int index = count > 0 ? state->selected : -1;
    const char *name = index >= 0 ? crazypod_video_name(index) : "No Videos";
    uint32_t resume = index >= 0
        ? crazypod_video_resume_seconds(index) : 0;
    uint32_t duration = index >= 0
        ? crazypod_video_duration_seconds(index) : 0;
    lv_obj_t *parent = preview_parent();
    lv_obj_t *text_panel;
    lv_obj_t *label;
    char time[24];
    char detail[64];

    make_preview_plinth(parent, 173, 154, 134, 0x8892A2, 0x161A21);
    render_video_card(parent, index, 173, 48, 134, 102);
    format_media_duration(time, sizeof(time), duration);
    if(resume > 0)
        snprintf(detail, sizeof(detail), "Resume %lu:%02lu  ·  %s",
                 (unsigned long)(resume / 60u),
                 (unsigned long)(resume % 60u), time);
    else
        snprintf(detail, sizeof(detail), "%s  ·  MPEG", time);
    text_panel = make_preview_text_panel(160, 52);
    label = make_label(text_panel, name, &lv_font_montserrat_10,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 128);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 6, 6);
    label = make_label(text_panel, detail, &lv_font_montserrat_8,
                       COLOR_WHITE, 120);
    lv_obj_set_width(label, 128);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 6, 28);
}

static void render_photos_preview(const struct route_state *state)
{
    int count = state->selected == 0
        ? crazypod_photo_count()
        : state->selected == 1
            ? crazypod_video_count()
            : crazypod_photo_favorite_count();
    lv_obj_t *parent = preview_parent();
    lv_obj_t *preview = NULL;
    lv_obj_t *label;
    lv_obj_t *text_panel;
    char detail[48];
    int i;

    make_preview_plinth(
        parent, 180, 149, 120, 0xAEB6B9, 0x252A2C);
    if(state->selected == 0) {
        static const int x[] = { 181, 213, 244 };
        static const int y[] = { 70, 57, 72 };
        static const int angle[] = { -75, 15, 80 };
        for(i = 0; i < 3; ++i) {
            const lv_image_dsc_t *descriptor = NULL;

            if(i < count) {
                if(menu_preview_build_defer_media)
                    menu_preview_media_deferred = true;
                else
                    descriptor = crazypod_photo_thumbnail(
                        CRAZYPOD_PHOTO_THUMB_SLOTS - 1 - i, i);
            }
            preview = make_box(parent, x[i], y[i], 57, 76, 3,
                               0xF0E9DB, LV_OPA_COVER);
            lv_obj_set_style_border_width(preview, 1, 0);
            lv_obj_set_style_border_color(
                preview, lv_color_hex(0xB8AE9F), 0);
            lv_obj_set_style_border_opa(preview, 155, 0);
            preview_add_bevel(
                preview, 57, 76, 0xFFFFFF, 0x867C6C);
            render_procedural_photo(
                preview, 4, 4, 49, 54, i);
            if(descriptor != NULL) {
                (void)render_photo_image(
                    preview, descriptor, 4, 4, 49, 54);
            }
            make_box(preview, 12, 65, 33, 2, 1,
                     0x6C6258, 82);
            make_box(preview, 17, 70, 23, 1, 0,
                     0x6C6258, 48);
            if(i == 1)
                make_box(preview, 22, 0, 14, 4, 1,
                         0xD2B879, 185);
            lv_obj_set_style_transform_rotation(
                preview, angle[i], 0);
            menu_preview_register_motion(
                preview,
                (i - 1) * 28, 25 + i * 4, 194,
                angle[i] + (i - 1) * 110, 0,
                i * 45, 280,
                (i - 1) * 23, -16, 194,
                angle[i] + (i - 1) * 80);
        }
    }
    else if(state->selected == 1) {
        int video_index = count > 0 ? 0 : -1;

        preview = render_video_card(parent, video_index,
                                    173, 48, 134, 102);
        make_box(parent, 209, 146, 60, 6, 3,
                 0x252A31, 210);
        menu_preview_register_motion(
            preview, 18, 0, 205, 55, 0,
            0, 280, 18, -4, 205, 80);
    }
    else {
        int photo_index = crazypod_photo_favorite_index(0);
        const lv_image_dsc_t *descriptor = NULL;

        if(photo_index >= 0) {
            if(menu_preview_build_defer_media)
                menu_preview_media_deferred = true;
            else
                descriptor = crazypod_photo_thumbnail(
                    CRAZYPOD_PHOTO_THUMB_SLOTS - 1, photo_index);
        }
        preview = make_box(parent, 188, 56, 104, 96, 7,
                           0x6B4429, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            preview, lv_color_hex(0x2D1B12), 0);
        lv_obj_set_style_bg_grad_dir(preview, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_border_width(preview, 3, 0);
        lv_obj_set_style_border_color(
            preview, lv_color_hex(0xB7834D), 0);
        lv_obj_set_style_border_opa(preview, 190, 0);
        preview_add_bevel(
            preview, 104, 96, 0xD1A36A, 0x160B06);
        make_box(preview, 5, 5, 1, 86, 0,
                 0xD8A76B, 70);
        make_box(preview, 98, 5, 1, 86, 0,
                 0x160B06, 120);
        {
            lv_obj_t *mat = make_box(
                preview, 9, 9, 86, 77, 3,
                0xE7DDC9, LV_OPA_COVER);
            lv_obj_set_style_border_width(mat, 1, 0);
            lv_obj_set_style_border_color(
                mat, lv_color_hex(0xBEB39F), 0);
            lv_obj_set_style_border_opa(mat, 135, 0);
            if(count > 0) {
                render_procedural_photo(
                    mat, 5, 5, 76, 67, 3);
                (void)render_photo_image(
                    mat, descriptor, 5, 5, 76, 67);
            }
            else {
                make_box(mat, 5, 5, 76, 67, 2,
                         0xA8B0B2, LV_OPA_COVER);
                label = make_label(
                    mat, LV_SYMBOL_IMAGE,
                    &lv_font_montserrat_24,
                    COLOR_WHITE, 82);
                lv_obj_center(label);
            }
        }
        make_box(parent, 228, 151, 24, 5, 2,
                 0x6E452A, 225);
        {
            lv_obj_t *pin = make_box(
                parent, 257, 48, 29, 29,
                LV_RADIUS_CIRCLE, 0x8D243A, LV_OPA_COVER);
            lv_obj_set_style_border_width(pin, 1, 0);
            lv_obj_set_style_border_color(
                pin, lv_color_hex(0xE1B46E), 0);
            lv_obj_set_style_border_opa(pin, 130, 0);
            preview_add_bevel(
                pin, 29, 29, 0xF0CE91, 0x310914);
            make_pixel_heart(
                pin, 6, 8, 2, 0xFFE2A8, LV_OPA_COVER);
            menu_preview_register_motion(
                pin, 13, -14, 170, 120, 0,
                80, 240, 11, -11, 170, 180);
        }
        menu_preview_register_motion(
            preview, 17, 0, 214, 65, 0,
            0, 280, 18, -5, 214, 95);
    }

    snprintf(detail, sizeof(detail), "%d %s%s",
             count,
             state->selected == 1 ? "video" : "photo",
             count == 1 ? "" : "s");
    text_panel = make_preview_text_panel(166, 52);
    label = make_label(text_panel, detail,
                       &lv_font_montserrat_10,
                       COLOR_WHITE, 190);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 7, 6);
    label = make_label(
        text_panel,
        state->selected == 0 ? "All pictures in /Pictures"
        : state->selected == 1 ? "Converted MPEG files in /Videos"
                               : "Saved favorites",
        &lv_font_montserrat_8, COLOR_WHITE, 100);
    lv_obj_set_width(label, 132);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 4, 25);
    menu_preview_register_motion(
        text_panel, 0, 9, 246, 0, 0, 70, 220,
        0, 6, 246, 0);
}

static void render_extras_preview(const struct route_state *state)
{
    static lv_image_dsc_t icon_descriptor;
    struct crazypod_app *app = hidden_app(state->selected);
    int app_index = app != NULL ? app_catalog_index(app->id) : -1;
    const struct crazypod_icon *icon =
        app_index >= 0 ? crazypod_icon_get(app_index) : NULL;
    lv_obj_t *parent = preview_parent();
    lv_obj_t *text_panel;
    lv_obj_t *label;

    make_box(parent, 188, 153, 104, 10, LV_RADIUS_CIRCLE,
             0x000000, 72);
    if(icon != NULL && icon->pixels != NULL) {
        memset(&icon_descriptor, 0, sizeof(icon_descriptor));
        icon_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
        icon_descriptor.header.cf =
            LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
        icon_descriptor.header.w = icon->width;
        icon_descriptor.header.h = icon->height;
        icon_descriptor.header.stride = icon->stride;
        icon_descriptor.data_size = icon->stride * icon->height;
        icon_descriptor.data = icon->pixels;
        render_photo_image(parent, &icon_descriptor,
                           174, 37, 132, 132);
    }
    else {
        lv_obj_t *fallback = make_box(
            parent, 196, 57, 88, 88, 20,
            app != NULL ? app->color : 0x59606B,
            LV_OPA_COVER);
        label = make_label(fallback,
                           app != NULL ? app->symbol : LV_SYMBOL_LIST,
                           &lv_font_montserrat_24,
                           COLOR_WHITE, 230);
        lv_obj_center(label);
    }

    text_panel = make_preview_text_panel(168, 52);
    label = make_label(text_panel,
                       app != NULL ? app->name : "More Features",
                       CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 5);
    label = make_label(text_panel,
                       app != NULL
                           ? "Hidden from Main Menu"
                           : "No hidden applications",
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 7, 30);
}

static void render_notes_preview(const struct route_state *state)
{
    const char *title = route_item_title(state, state->selected);
    const char *detail = "";
    const struct crazypod_note *note = NULL;
    lv_obj_t *parent = preview_parent();
    lv_obj_t *binder;
    lv_obj_t *paper;
    lv_obj_t *label;
    lv_obj_t *text_panel;
    lv_obj_t *part;
    char search_detail[48];
    bool is_home = state->route == NOTES_ROUTE_MENU;
    bool is_new = is_home && state->selected == 0;
    bool is_draft = is_home && note_draft_available &&
                    state->selected == 1;
    bool is_search = is_home &&
                     state->selected == notes_home_search_index();
    bool is_deleted = is_home &&
                      state->selected == notes_home_deleted_index();
    int i;

    if(state->route == NOTES_ROUTE_MENU)
        note = notes_home_note(state->selected);
    else if(state->route == NOTES_ROUTE_SEARCH_RESULTS)
        note = crazypod_notes_search_get(
            note_search_query, state->selected);
    else if(state->group > 0)
        note = crazypod_note_find((uint32_t)state->group);
    if(state->route == NOTES_ROUTE_SEARCH) {
        title = note_search_query[0] != '\0'
            ? note_search_query : "Search Notes";
        snprintf(search_detail, sizeof(search_detail),
                 "%d matching note%s",
                 crazypod_notes_search_count(note_search_query),
                 crazypod_notes_search_count(note_search_query) == 1
                    ? "" : "s");
        detail = search_detail;
    }
    else if(note != NULL)
        detail = note->pinned ? "Pinned note" :
                 note->deleted ? "Recently deleted" : "Saved note";
    else if(state->route == NOTES_ROUTE_MENU && state->selected == 0)
        detail = "Write with the click wheel";
    else if(state->route == NOTES_ROUTE_MENU &&
            note_draft_available && state->selected == 1)
        detail = "Resume unsaved changes";
    else if(is_search)
        detail = "Search title and body";
    else if(is_deleted)
        detail = "Restore or erase notes";
    else if(state->route == NOTES_ROUTE_MENU)
        detail = "Saved note";

    make_preview_plinth(
        parent, 187, 156, 108, 0xAEB7BA, 0x252A2C);
    if(is_new) {
        binder = make_box(parent, 190, 52, 92, 104, 6,
                          0x6B4B32, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            binder, lv_color_hex(0x392517), 0);
        lv_obj_set_style_bg_grad_dir(
            binder, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(binder, 1, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xA68159), 0);
        lv_obj_set_style_border_opa(binder, 145, 0);
        preview_add_bevel(
            binder, 92, 104, 0xB4936C, 0x1D120B);
        paper = make_box(binder, 7, 10, 78, 87, 3,
                         0xF4E9CF, LV_OPA_COVER);
        lv_obj_set_style_border_width(paper, 1, 0);
        lv_obj_set_style_border_color(
            paper, lv_color_hex(0xD5C49F), 0);
        lv_obj_set_style_border_opa(paper, 165, 0);
        preview_add_bevel(
            paper, 78, 87, 0xFFFFFF, 0xA8987C);
        preview_add_paper_rules(
            paper, 78, 25, 5, 12, 0x8FA7B5);
        make_box(paper, 14, 12, 34, 3, 1,
                 0x6F5540, 125);
        part = make_box(binder, 29, 2, 34, 13, 5,
                        0xB9C0C2, LV_OPA_COVER);
        preview_add_bevel(
            part, 34, 13, 0xF2F5F6, 0x454B4D);
        make_box(part, 8, 4, 18, 4, 2,
                 0x53595B, 185);
        part = make_box(parent, 270, 70, 7, 77, 3,
                        0xE5B64A, LV_OPA_COVER);
        make_box(part, 0, 0, 7, 11, 2, 0xE26D5A, 245);
        make_box(part, 0, 11, 7, 4, 0,
                 0xB9BEC0, 240);
        make_box(part, 1, 69, 5, 8, 2, 0x252525, 245);
        lv_obj_set_style_transform_rotation(part, 235, 0);
        menu_preview_register_motion(
            binder, 0, 18, 228, -15, 0, 0, 260,
            -8, 12, 228, -25);
        menu_preview_register_motion(
            part, 22, -18, 196, 410, 0, 60, 300,
            18, -14, 196, 430);
    }
    else if(is_draft) {
        binder = make_box(parent, 194, 58, 92, 98, 8,
                          0x75492D, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            binder, lv_color_hex(0x3B2519), 0);
        lv_obj_set_style_bg_grad_dir(binder, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(binder, 1, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xB0835D), 0);
        lv_obj_set_style_border_opa(binder, 145, 0);
        preview_add_bevel(
            binder, 92, 98, 0xC29B73, 0x21150E);
        paper = make_box(binder, 12, 12, 68, 78, 3,
                         0xF4E9CF, LV_OPA_COVER);
        preview_add_bevel(
            paper, 68, 78, 0xFFFFFF, 0x9C8B70);
        preview_add_paper_rules(
            paper, 68, 23, 4, 12, 0x8FA7B5);
        make_box(paper, 13, 11, 28, 3, 1,
                 0x6F5540, 115);
        part = make_box(binder, 30, 3, 33, 12, 5,
                        0xB8B9B7, LV_OPA_COVER);
        preview_add_bevel(
            part, 33, 12, 0xEDF0F1, 0x3A3D3E);
        make_box(part, 7, 3, 19, 4, 2, 0x4A4C4D, 165);
        menu_preview_register_motion(
            binder, 15, 0, 218, 45, 0, 0, 270,
            14, 5, 218, 70);
        menu_preview_register_motion(
            part, 0, -14, 200, 0, 0, 70, 230,
            0, -10, 200, 0);
    }
    else if(is_search) {
        paper = make_box(parent, 192, 61, 83, 91, 4,
                         0xF4E9CF, LV_OPA_COVER);
        lv_obj_set_style_border_width(paper, 1, 0);
        lv_obj_set_style_border_color(
            paper, lv_color_hex(0xC9B99A), 0);
        lv_obj_set_style_border_opa(paper, 135, 0);
        preview_add_bevel(
            paper, 83, 91, 0xFFFFFF, 0x998970);
        preview_add_paper_rules(
            paper, 83, 18, 5, 14, 0x6E7780);
        make_box(paper, 14, 8, 33, 3, 1,
                 0x6F5540, 105);
        part = make_box(parent, 228, 65, 44, 44,
                        LV_RADIUS_CIRCLE, COLOR_CYAN, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(part, 7, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(COLOR_CYAN), 0);
        lv_obj_set_style_border_opa(part, 230, 0);
        binder = make_box(
            part, 6, 6, 32, 32, LV_RADIUS_CIRCLE,
            0xDAF6FA, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(binder, 1, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xF1FCFE), 0);
        lv_obj_set_style_border_opa(binder, 105, 0);
        binder = make_box(parent, 266, 103, 8, 37, 4,
                          0x76858C, 245);
        preview_add_bevel(
            binder, 8, 37, 0xE3EAEC, 0x262E31);
        make_box(binder, 2, 25, 4, 8, 2,
                 0x23282A, 220);
        menu_preview_register_motion(
            paper, -8, 7, 232, 0, 0, 0, 240,
            -10, 5, 232, 0);
        menu_preview_register_motion(
            part, 42, 9, 185, 120, 0, 50, 300,
            38, 9, 185, 160);
    }
    else if(is_deleted) {
        binder = make_box(parent, 205, 94, 72, 61, 7,
                          0x6D7377, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            binder, lv_color_hex(0x303639), 0);
        lv_obj_set_style_bg_grad_dir(
            binder, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(binder, 2, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xB9C0C4), 0);
        lv_obj_set_style_border_opa(binder, 150, 0);
        preview_add_bevel(
            binder, 72, 61, 0xD6DCDE, 0x16191A);
        make_box(binder, 3, 4, 66, 4, 2,
                 0xBCC5C8, 225);
        for(i = 0; i < 5; ++i)
            make_box(binder, 9 + i * 12, 8, 2, 43, 1,
                     0x252A2D, 115);
        for(i = 0; i < 3; ++i)
            make_box(binder, 7, 19 + i * 12, 58, 1, 0,
                     0xD8DEE0, 45);
        make_box(binder, 9, 54, 10, 5, 2,
                 0x24282A, 210);
        make_box(binder, 53, 54, 10, 5, 2,
                 0x24282A, 210);
        part = make_box(parent, 225, 59, 37, 37,
                        LV_RADIUS_CIRCLE, 0xE9DDC4, LV_OPA_COVER);
        preview_add_bevel(
            part, 37, 37, 0xFFFFFF, 0xA99D88);
        make_box(part, 8, 7, 20, 3, 1, 0x756B5F, 90);
        make_box(part, 13, 16, 16, 3, 1, 0x756B5F, 75);
        make_box(part, 4, 25, 18, 2, 1, 0x756B5F, 52);
        lv_obj_set_style_transform_rotation(part, 120, 0);
        menu_preview_register_motion(
            binder, 0, 18, 225, 0, 0, 0, 240,
            0, 13, 225, 0);
        menu_preview_register_motion(
            part, -18, -37, 150, -220, 0, 30, 300,
            13, 31, 160, 270);
    }
    else {
        binder = make_box(parent, 195, 60, 92, 96, 8,
                          0x5C2D23, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            binder, lv_color_hex(0x2B1514), 0);
        lv_obj_set_style_bg_grad_dir(binder, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(binder, 2, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xB06A4A), 0);
        lv_obj_set_style_border_opa(binder, 145, 0);
        preview_add_bevel(
            binder, 92, 96, 0xC17B5C, 0x1B0D0C);
        make_box(binder, 6, 7, 5, 82, 2,
                 0x311515, 225);
        make_box(binder, 12, 7, 1, 82, 0,
                 0xC17B5C, 65);
        paper = make_box(binder, 17, 8, 67, 80, 3,
                         0xF4E9CF, LV_OPA_COVER);
        preview_add_bevel(
            paper, 67, 80, 0xFFFFFF, 0x9B8B72);
        preview_add_paper_rules(
            paper, 67, 20, 4, 13, 0x8FA7B5);
        make_box(paper, 13, 9, 34, 3, 1,
                 0x6F5540, 115);
        for(i = 0; i < 4; ++i) {
            make_box(binder, 8, 17 + i * 19, 13, 5,
                     LV_RADIUS_CIRCLE, 0xD6D7D9, 230);
            make_box(binder, 9, 18 + i * 19, 9, 2,
                     LV_RADIUS_CIRCLE, 0x52545A, 180);
        }
        part = make_box(binder, 76, 37, 12, 22, 3,
                        note != NULL && note->pinned
                            ? 0xF0B43C : 0xBD7B42, 220);
        menu_preview_register_motion(
            binder, 16, 0, 215, 55, 0, 0, 280,
            14, 5, 215, 75);
        menu_preview_register_motion(
            paper, 17, 0, 226, 0, 0, 60, 240,
            15, 0, 226, 0);
        menu_preview_register_motion(
            part, 8, -8, 170, 120, 0, 90, 220,
            8, -7, 170, 160);
    }

    text_panel = make_preview_text_panel(168, 52);
    label = make_label(text_panel, title != NULL ? title : "Notes",
                       CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 7, 5);
    label = make_label(text_panel, detail, &lv_font_montserrat_8,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 7, 30);
    menu_preview_register_motion(
        text_panel, 0, 9, 246, 0, 0, 70, 220,
        0, 6, 246, 0);
}

static lv_obj_t *make_book_preview_cover(
    lv_obj_t *parent, const struct crazypod_book *book,
    int x, int y, int width, int height)
{
    int book_index = crazypod_book_index(book);
    const lv_image_dsc_t *image =
        book_index >= 0 && width >= 50
            ? crazypod_book_cover_get(
                  book_index, width, height) : NULL;
    uint32_t color = book != NULL
        ? artwork_color(book->path, 0) : 0x70462A;
    lv_obj_t *cover = make_box(
        parent, x, y, width, height, 4,
        image != NULL ? 0x090806 : color, LV_OPA_COVER);
    lv_obj_t *label;
    int spine_width = width > 50 ? 7 : 4;

    if(image != NULL) {
        render_photo_image(cover, image, 0, 0, width, height);
        lv_obj_set_style_border_width(cover, 1, 0);
        lv_obj_set_style_border_color(
            cover, lv_color_hex(0xD8D0C2), 0);
        lv_obj_set_style_border_opa(cover, 105, 0);
        preview_add_bevel(
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
    preview_add_bevel(
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

static void render_books_menu_stage(
    lv_obj_t *parent, const struct route_state *state,
    const struct crazypod_book **selected_book,
    const char **detail)
{
    bool has_continue = books_has_continue();
    int logical = state->selected - (has_continue ? 1 : 0);
    int count = crazypod_books_count();
    lv_obj_t *stage;
    lv_obj_t *part;
    lv_obj_t *label;
    int i;

    if(has_continue && state->selected == 0) {
        int index = crazypod_books_recent_index();
        const struct crazypod_book *book = crazypod_book_get(index);
        lv_obj_t *left_page;
        lv_obj_t *right_page;
        *selected_book = book;
        *detail = "Resume saved position";
        make_preview_plinth(
            parent, 184, 155, 112, 0xAA8B61, 0x2A1B12);
        stage = make_box(parent, 185, 71, 110, 77, 5,
                         0x5B3A25, LV_OPA_COVER);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0x9A744C), 0);
        lv_obj_set_style_border_opa(stage, 140, 0);
        preview_add_bevel(
            stage, 110, 77, 0xB58B5F, 0x21140C);
        left_page = make_box(stage, 5, 5, 49, 66, 3,
                             0xEFE2C5, LV_OPA_COVER);
        right_page = make_box(stage, 56, 5, 49, 66, 3,
                              0xF7EBD2, LV_OPA_COVER);
        preview_add_bevel(
            left_page, 49, 66, 0xFFFFFF, 0x9E8A68);
        preview_add_bevel(
            right_page, 49, 66, 0xFFFFFF, 0x9E8A68);
        preview_add_paper_rules(
            left_page, 49, 15, 4, 10, 0x6E5946);
        preview_add_paper_rules(
            right_page, 49, 15, 4, 10, 0x6E5946);
        make_box(stage, 52, 5, 5, 66, 2,
                 0xB69565, 190);
        make_box(stage, 54, 7, 1, 62, 0,
                 0x5A3821, 145);
        part = make_box(right_page, 37, 0, 7, 31, 1,
                        0xB43B45, 225);
        make_box(part, 1, 0, 2, 31, 0,
                 0xE77780, 95);
        menu_preview_register_motion(
            stage, 0, 11, 225, 0, 0, 0, 250,
            0, 8, 225, 0);
        menu_preview_register_motion(
            left_page, 22, 0, 182, 120, 0, 30, 280,
            20, 0, 182, 120);
        menu_preview_register_motion(
            right_page, -22, 0, 182, -120, 0, 30, 280,
            -20, 0, 182, -120);
        menu_preview_register_motion(
            part, 0, -14, 210, 0, 0, 90, 220,
            0, -9, 210, 0);
        return;
    }

    if(logical == 0) {
        int recent_count = crazypod_books_recent_count();
        *detail = recent_count > 0
            ? "Latest reading activity" : "No recent reading";
        make_preview_plinth(
            parent, 181, 152, 118, 0xB38A5E, 0x2C1C12);
        for(i = 2; i >= 0; --i) {
            const struct crazypod_book *book =
                i < recent_count
                    ? crazypod_book_get(
                          crazypod_books_recent_at(i))
                    : NULL;
            stage = make_book_preview_cover(
                parent, book,
                186 + i * 13, 68 - i * 5,
                70, 86);
            menu_preview_register_motion(
                stage, (i - 1) * 20, 14 + i * 3, 195,
                (i - 1) * 45, 0, (2 - i) * 35, 270,
                (i - 1) * 18, 9, 195,
                (i - 1) * 55);
        }
        return;
    }

    if(logical == 1) {
        *detail = count > 0
            ? "Browse the local library" : "Import EPUB, TXT or MD";
        make_box(parent, 183, 70, 114, 83, 4,
                 0x261A12, 82);
        stage = make_preview_plinth(
            parent, 178, 151, 124, 0xB47A3C, 0x3A2416);
        preview_add_fastener(stage, 5, 2, 0x9A7655);
        preview_add_fastener(stage, 114, 2, 0x9A7655);
        menu_preview_register_motion(
            stage, 0, 16, 230, 0, 0, 0, 220,
            0, 12, 230, 0);
        for(i = 0; i < 4; ++i) {
            const struct crazypod_book *book =
                i < count ? crazypod_book_get(i) : NULL;
            part = make_book_preview_cover(
                parent, book, 188 + i * 27,
                75 + (i % 2) * 7,
                21, 76 - (i % 2) * 7);
            menu_preview_register_motion(
                part, 0, 44 + (i % 2) * 7, 218,
                (i - 2) * 12, 0, i * 35, 260,
                0, 35, 218, (i - 2) * -12);
        }
        return;
    }

    if(logical == 2) {
        int favorite_count = crazypod_books_favorite_count();
        *detail = favorite_count > 0
            ? "Your favorite books" : "No favorites yet";
        if(favorite_count > 0) {
            const struct crazypod_book *book =
                crazypod_book_get(
                    crazypod_books_favorite_at(0));
            *selected_book = book;
            make_preview_plinth(
                parent, 184, 155, 112, 0xB58D63, 0x301D13);
            part = make_book_preview_cover(
                parent, book, 204, 55, 72, 101);
            stage = make_box(parent, 257, 52, 26, 26,
                             LV_RADIUS_CIRCLE, 0x6D1526, 240);
            lv_obj_set_style_border_width(stage, 1, 0);
            lv_obj_set_style_border_color(
                stage, lv_color_hex(0xD7B06A), 0);
            lv_obj_set_style_border_opa(stage, 155, 0);
            preview_add_bevel(
                stage, 26, 26, 0xF0CE86, 0x2D0710);
            make_pixel_heart(stage, 5, 7, 2,
                             0xF7D788, LV_OPA_COVER);
            menu_preview_register_motion(
                part, 0, 21, 210, -25, 0, 0, 270,
                0, 16, 210, 25);
            menu_preview_register_motion(
                stage, 12, -13, 165, 120, 0, 70, 240,
                10, -10, 165, 170);
        }
        else {
            stage = make_box(parent, 184, 70, 112, 84, 9,
                             0x5B0F19, LV_OPA_COVER);
            lv_obj_set_style_bg_grad_color(
                stage, lv_color_hex(0x250408), 0);
            lv_obj_set_style_bg_grad_dir(
                stage, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_border_width(stage, 1, 0);
            lv_obj_set_style_border_color(
                stage, lv_color_hex(0xE8C875), 0);
            lv_obj_set_style_border_opa(stage, 48, 0);
            preview_add_bevel(
                stage, 112, 84, 0xA84854, 0x120103);
            preview_add_fastener(
                stage, 7, 7, 0xA77D4C);
            preview_add_fastener(
                stage, 100, 7, 0xA77D4C);
            make_pixel_heart(stage, 40, 27, 4,
                             0xE8C875, 88);
            make_box(stage, 30, 67, 52, 4, 2,
                     0xD1AE63, 75);
            menu_preview_register_motion(
                stage, 0, 20, 208, 0, 0, 0, 260,
                0, 14, 208, 0);
        }
        return;
    }

    if(logical == 3) {
        char value[16];
        *detail = "Library and progress totals";
        stage = make_box(parent, 184, 61, 112, 94, 10,
                         0x15110C, 232);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xD4B46A), 0);
        lv_obj_set_style_border_opa(stage, 82, 0);
        preview_add_bevel(
            stage, 112, 94, 0xA58D5B, 0x000000);
        preview_add_fastener(stage, 5, 5, 0xA89265);
        preview_add_fastener(stage, 102, 5, 0xA89265);
        make_box(stage, 55, 13, 1, 48, 0,
                 0xD4B46A, 55);
        snprintf(value, sizeof(value), "%d", count);
        label = make_label(stage, value,
                           &lv_font_montserrat_24,
                           0xF6D58C, LV_OPA_COVER);
        lv_obj_set_width(label, 52);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 4, 14);
        label = make_label(stage, "BOOKS",
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 110);
        lv_obj_set_width(label, 52);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 4, 44);
        snprintf(value, sizeof(value), "%d",
                 crazypod_books_favorite_count());
        label = make_label(stage, value,
                           &lv_font_montserrat_24,
                           0xF6D58C, LV_OPA_COVER);
        lv_obj_set_width(label, 52);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 56, 14);
        label = make_label(stage, "FAVORITES",
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 110);
        lv_obj_set_width(label, 52);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 56, 44);
        make_box(stage, 12, 67, 88, 5,
                 LV_RADIUS_CIRCLE, COLOR_WHITE, 30);
        make_box(stage, 12, 67,
                 count > 0 ? 56 : 4, 5,
                 LV_RADIUS_CIRCLE, 0xD4B46A, 215);
        make_box(stage, 12, 77, 88, 1, 0,
                 0xD4B46A, 52);
        make_box(stage, 12, 83,
                 crazypod_books_favorite_count() > 0 ? 34 : 4,
                 3, 1, 0x8E7447, 165);
        menu_preview_register_motion(
            stage, 13, 0, 220, 30, 0, 0, 270,
            12, 0, 220, 45);
        return;
    }

    *detail = "Font, size and page theme";
    stage = make_box(parent, 193, 57, 94, 101, 6,
                     0xF4E9CF, LV_OPA_COVER);
    lv_obj_set_style_border_width(stage, 1, 0);
    lv_obj_set_style_border_color(
        stage, lv_color_hex(0xD5BB84), 0);
    lv_obj_set_style_border_opa(stage, 150, 0);
    preview_add_bevel(
        stage, 94, 101, 0xFFFFFF, 0x95815F);
    make_box(stage, 7, 7, 80, 4, 2,
             0xB89B68, 95);
    label = make_label(stage, "Aa",
                       &lv_font_montserrat_24,
                       0x4A3524, LV_OPA_COVER);
    lv_obj_set_pos(label, 31, 17);
    preview_add_paper_rules(
        stage, 94, 55, 3, 10, 0x665344);
    {
        static const uint32_t colors[] = {
            0xE8D7B7, 0xF7F7F4, 0xCFE6D8, 0x242424
        };
        for(i = 0; i < 4; ++i) {
            lv_obj_t *swatch = make_box(
                stage, 18 + i * 16, 84,
                11, 11, LV_RADIUS_CIRCLE,
                colors[i], LV_OPA_COVER);
            if(i == crazypod_books_theme()) {
                lv_obj_set_style_border_width(swatch, 2, 0);
                lv_obj_set_style_border_color(
                    swatch, lv_color_hex(0x9A6A2C), 0);
                lv_obj_set_style_border_opa(
                    swatch, LV_OPA_COVER, 0);
            }
        }
    }
    menu_preview_register_motion(
        stage, 0, 18, 220, 0, 0, 0, 270,
        -8, 12, 220, -20);
}

static void render_books_settings_stage(
    lv_obj_t *parent, const struct route_state *state,
    const char **detail)
{
    lv_obj_t *page;
    lv_obj_t *label;
    int selected = state->selected;
    int i;

    if(selected == 2) {
        page = make_box(parent, 198, 67, 84, 84, 18,
                        0xA56D2E, LV_OPA_COVER);
        label = make_label(page, LV_SYMBOL_REFRESH,
                           &lv_font_montserrat_24,
                           COLOR_WHITE, 235);
        lv_obj_center(label);
        *detail = "Scan Books folders again";
        return;
    }

    {
        static const uint32_t page_colors[] = {
            0xE8D7B7, 0xF7F7F4, 0xCFE6D8, 0x242424
        };
        int theme = crazypod_books_theme();
        uint32_t ink = theme == 3 ? 0xF2F2EE : 0x4A3524;
        page = make_box(parent, 194, 57, 92, 101, 6,
                        page_colors[theme], LV_OPA_COVER);
        lv_obj_set_style_border_width(page, 1, 0);
        lv_obj_set_style_border_color(
            page, lv_color_hex(0xD5BB84), 0);
        lv_obj_set_style_border_opa(page, 135, 0);
        label = make_label(
            page, "Aa",
            crazypod_books_font_size() == 0
                ? &lv_font_montserrat_16
                : &lv_font_montserrat_24,
            ink, LV_OPA_COVER);
        lv_obj_set_width(label, 92);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 0,
                       crazypod_books_font_size() == 0 ? 17 : 11);
        for(i = 0; i < 3; ++i)
            make_box(page, 17, 54 + i * 10,
                     58 - i * 7, 2, 1,
                     ink, 82);
        if(selected == 1) {
            static const uint32_t swatch_colors[] = {
                0xE8D7B7, 0xF7F7F4, 0xCFE6D8, 0x242424
            };
            for(i = 0; i < 4; ++i)
                make_box(page, 15 + i * 17, 84,
                         12, 12, LV_RADIUS_CIRCLE,
                         swatch_colors[i],
                         i == theme ? 255 : 115);
        }
    }
    *detail = selected == 0
        ? "Choose size in a focused popup"
        : "Choose a page theme in a popup";
}

static void render_books_preview(const struct route_state *state)
{
    int index = books_route_book_index(state, state->selected);
    const struct crazypod_book *book;
    const char *title;
    const char *detail = "";
    lv_obj_t *parent = preview_parent();
    lv_obj_t *label;
    lv_obj_t *text_panel;
    char detail_text[64];

    if(index >= 0)
        crazypod_book_probe(index);
    book = crazypod_book_get(index);
    title = route_item_title(state, state->selected);

    if(state->route == BOOKS_ROUTE_MENU) {
        book = NULL;
        render_books_menu_stage(
            parent, state, &book, &detail);
    }
    else if(state->route == BOOKS_ROUTE_READING_SETTINGS) {
        book = NULL;
        render_books_settings_stage(parent, state, &detail);
    }
    else {
        make_box(parent, 182, 157, 116, 8,
                 LV_RADIUS_CIRCLE, 0x000000, 66);
        make_book_preview_cover(parent, book, 204, 55, 72, 101);
        if(book != NULL &&
           (book->content_size > 0 || book->size > 0)) {
            uint32_t total = book->content_size > 0
                ? book->content_size : book->size;
            snprintf(detail_text, sizeof(detail_text),
                     "%lu%% read%s",
                     (unsigned long)(
                         book->progress * 100u / total),
                     book->favorite ? " · Favorite" : "");
            detail = detail_text;
        }
        else {
            snprintf(detail_text, sizeof(detail_text),
                     "%d book%s", crazypod_books_count(),
                     crazypod_books_count() == 1 ? "" : "s");
            detail = detail_text;
        }

        if(state->route == BOOKS_ROUTE_ACTIONS) {
            static const char *const symbols[] = {
                LV_SYMBOL_PLAY, LV_SYMBOL_FILE, LV_SYMBOL_LIST,
                LV_SYMBOL_OK, LV_SYMBOL_SETTINGS, LV_SYMBOL_TRASH
            };
            static const uint32_t colors[] = {
                0x34C759, 0x4F9BFF, 0xFFB340,
                0xD4B46A, 0x8E8E93, 0xFF453A
            };
            lv_obj_t *badge = make_box(
                parent, 255, 51, 29, 29,
                LV_RADIUS_CIRCLE,
                colors[state->selected],
                LV_OPA_COVER);
            label = make_label(
                badge, symbols[state->selected],
                &lv_font_montserrat_10,
                COLOR_WHITE, LV_OPA_COVER);
            lv_obj_center(label);
            detail = state->selected == 0
                ? "Open at saved position"
                : state->selected == 1
                    ? "Open the saved bookmark"
                    : state->selected == 2
                        ? "Browse EPUB chapters"
                        : state->selected == 3
                            ? "Change favorite status"
                            : state->selected == 4
                                ? "View file information"
                                : "Delete this local file";
        }
        else if(state->route == BOOKS_ROUTE_CHAPTERS) {
            snprintf(detail_text, sizeof(detail_text),
                     "Chapter %d of %d",
                     state->selected + 1,
                     crazypod_book_chapter_count(state->group));
            detail = detail_text;
        }
        else if(state->route == BOOKS_ROUTE_BOOKMARKS)
            detail = "Jump to the saved page";
        else if(state->route == BOOKS_ROUTE_DELETE_CONFIRM)
            detail = "Hold center to delete permanently";
    }

    if(state->route != BOOKS_ROUTE_MENU && book != NULL &&
       state->route != BOOKS_ROUTE_ACTIONS &&
       state->route != BOOKS_ROUTE_CHAPTERS &&
       state->route != BOOKS_ROUTE_BOOKMARKS &&
       state->route != BOOKS_ROUTE_DELETE_CONFIRM)
        title = book->title;
    else if(state->route == BOOKS_ROUTE_MENU &&
            book != NULL && books_has_continue() &&
            state->selected == 0)
        title = book->title;

    text_panel = make_preview_text_panel(172, 50);
    label = make_label(text_panel,
                       title != NULL ? title : "Books",
                       CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 5);
    label = make_label(text_panel, detail,
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 30);
    if(state->route == BOOKS_ROUTE_MENU)
        menu_preview_register_motion(
            text_panel, 0, 9, 246, 0, 0, 70, 220,
            0, 6, 246, 0);
}

static void render_utility_preview(const struct route_state *state)
{
    const char *title = route_item_title(state, state->selected);
    const char *detail = "";
    const char *symbol = LV_SYMBOL_HOME;
    uint32_t color = 0x4F9BFF;
    lv_obj_t *parent = preview_parent();
    lv_obj_t *swatch;
    lv_obj_t *label;
    lv_obj_t *text_panel;
    char detail_text[96];

    if(state->route == UTILITIES_ROUTE_MENU) {
        const struct crazypod_miniapp_metadata *metadata =
            miniapp_metadata(state->selected);

        if(metadata != NULL) {
            symbol = miniapp_symbol(state->selected);
            color = metadata->accent_rgb;
            detail = metadata->summary;
        }
        if(miniapp_last_error < 0) {
            snprintf(detail_text, sizeof(detail_text),
                     "Package error %d", miniapp_last_error);
            detail = detail_text;
        }
    }
    else if(state->route == WORKOUT_ROUTE_MENU) {
        symbol = state->selected == 0 ? LV_SYMBOL_PLAY :
                 state->selected == 1 ? LV_SYMBOL_REFRESH :
                 LV_SYMBOL_LIST;
        color = 0xA8F12D;
        detail = state->selected == 0
            ? "Choose a time-only workout"
            : state->selected == 1
                ? "Saved workout history"
                : "Elapsed-time totals";
    }
    else if(state->route == WORKOUT_ROUTE_TYPES) {
        symbol = LV_SYMBOL_PLAY;
        color = 0xA8F12D;
        detail = "Manual elapsed-time tracking";
    }
    else if(state->route == WORKOUT_ROUTE_HISTORY) {
        const struct crazypod_workout *workout =
            crazypod_workout_get(state->selected);
        symbol = LV_SYMBOL_REFRESH;
        color = 0xA8F12D;
        if(workout != NULL) {
            snprintf(detail_text, sizeof(detail_text),
                     "%04d-%02d-%02d · %lu min",
                     (int)(workout->date / 10000),
                     (int)(workout->date / 100 % 100),
                     (int)(workout->date % 100),
                     (unsigned long)(workout->duration_seconds / 60u));
            detail = detail_text;
        }
        else
            detail = "No saved workouts";
    }
    else if(state->route == WORKOUT_ROUTE_FINISH_CONFIRM ||
            state->route == WORKOUT_ROUTE_DELETE_CONFIRM) {
        symbol = LV_SYMBOL_TRASH;
        color = 0xFF453A;
        detail = state->route == WORKOUT_ROUTE_FINISH_CONFIRM
            ? "Hold center to save elapsed time"
            : "Hold center to delete permanently";
    }
    else if(state->route == CLOCK_ROUTE_MENU) {
        static const char *const symbols[] = {
            LV_SYMBOL_HOME, LV_SYMBOL_POWER, LV_SYMBOL_REFRESH
        };
        symbol = symbols[state->selected];
        color = state->selected == 0 ? 0xF26D5B :
                state->selected == 1 ? 0x7B61FF : 0xFFB340;
        detail = state->selected == 0 ? "Device local time" :
                 state->selected == 1
                    ? (get_sleep_timer_active()
                        ? "Timer is running" : "Power off after playback")
                    : "Precise elapsed time";
    }
    else if(state->route == CLOCK_ROUTE_SLEEP_TIMER) {
        symbol = LV_SYMBOL_POWER;
        color = 0x7B61FF;
        if(get_sleep_timer_active()) {
            int remaining = get_sleep_timer();
            snprintf(detail_text, sizeof(detail_text),
                     "%d:%02d remaining",
                     remaining / 60, remaining % 60);
            detail = detail_text;
        }
        else
            detail = "Select a shutdown delay";
    }
    else if(state->route == CALENDAR_ROUTE_MENU) {
        static const char *const symbols[] = {
            LV_SYMBOL_HOME, LV_SYMBOL_REFRESH,
            LV_SYMBOL_LIST, LV_SYMBOL_EDIT
        };
        static const char *const details[] = {
            "Events on the current date",
            "Future calendar events",
            "Browse the month grid",
            "Create a local event"
        };
        symbol = symbols[state->selected];
        color = 0xFF453A;
        detail = details[state->selected];
    }
    else if(state->route == CALENDAR_ROUTE_TODAY ||
            state->route == CALENDAR_ROUTE_UPCOMING ||
            state->route == CALENDAR_ROUTE_DAY_EVENTS) {
        int event_count = calendar_route_event_count(state);
        const struct crazypod_calendar_event *event =
            state->selected < event_count
                ? crazypod_calendar_event_get(
                      calendar_route_event_index(
                          state, state->selected))
                : NULL;
        color = 0xFF453A;
        if(event != NULL) {
            snprintf(detail_text, sizeof(detail_text),
                     "%d · %s", event->date,
                     event->time[0] != '\0' ? event->time : "All day");
            detail = detail_text;
        }
        else
            detail = "Create an event on this iPod";

        {
            struct tm *now = get_time();
            static const char *const weekday[] = {
                "S", "M", "T", "W", "T", "F", "S"
            };
            int first_day =
                (now->tm_wday - ((now->tm_mday - 1) % 7) + 7) % 7;
            int count = days_in_month(now->tm_year + 1900, now->tm_mon);
            int cell_width = 13;
            int day;
            char month[20];

            swatch = make_box(parent, 190, 58, 100, 92, 10,
                              0xF4F0E8, LV_OPA_COVER);
            make_box(swatch, 0, 0, 100, 20, 10,
                     color, LV_OPA_COVER);
            snprintf(month, sizeof(month), "%d / %d",
                     now->tm_mon + 1, now->tm_year + 1900);
            label = make_label(swatch, month, &lv_font_montserrat_8,
                               COLOR_WHITE, LV_OPA_COVER);
            lv_obj_set_width(label, 100);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(label, 0, 6);
            for(day = 0; day < 7; ++day) {
                label = make_label(swatch, weekday[day],
                                   &lv_font_montserrat_8,
                                   0x565656, 190);
                lv_obj_set_width(label, cell_width);
                lv_obj_set_style_text_align(
                    label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_pos(label, 4 + day * cell_width, 24);
            }
            for(day = 1; day <= count; ++day) {
                int slot = first_day + day - 1;
                int x = 4 + (slot % 7) * cell_width;
                int y = 35 + (slot / 7) * 10;
                char day_text[4];
                if(day == now->tm_mday)
                    make_box(swatch, x + 1, y - 1, 11, 10,
                             LV_RADIUS_CIRCLE, color, LV_OPA_COVER);
                snprintf(day_text, sizeof(day_text), "%d", day);
                label = make_label(
                    swatch, day_text, &lv_font_montserrat_8,
                    day == now->tm_mday ? COLOR_WHITE : 0x252525,
                    LV_OPA_COVER);
                lv_obj_set_width(label, cell_width);
                lv_obj_set_style_text_align(
                    label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_pos(label, x, y);
            }
        }
    }
    else if(state->route == CALENDAR_ROUTE_EDITOR) {
        symbol = state->selected == 3
            ? LV_SYMBOL_SAVE : LV_SYMBOL_EDIT;
        color = 0xFF453A;
        detail = state->selected == 0 ? "Edit the event title" :
                 state->selected == 1
                    ? "Center next day · Left previous"
                    : state->selected == 2
                        ? "Center later · Left earlier"
                        : calendar_editor_error == 1
                            ? "Enter a title before saving"
                            : calendar_editor_error == 2
                                ? "Could not write calendar.bin"
                                : "Write event to local storage";
    }
    else if(state->route == CALENDAR_ROUTE_ACTIONS ||
            state->route == CALENDAR_ROUTE_DELETE_CONFIRM) {
        symbol = state->route == CALENDAR_ROUTE_DELETE_CONFIRM ||
                 state->selected == 1
            ? LV_SYMBOL_TRASH : LV_SYMBOL_EDIT;
        color = 0xFF453A;
        detail = state->route == CALENDAR_ROUTE_DELETE_CONFIRM
            ? "Hold center to delete permanently"
            : state->selected == 0
                ? "Change this local event"
                : "Open delete confirmation";
    }
    else if(state->route == CONTACTS_ROUTE_LIST) {
        const struct crazypod_contact *contact =
            crazypod_contact_get(state->selected);
        symbol = LV_SYMBOL_HOME;
        color = 0x4F9BFF;
        detail = contact != NULL && contact->phone[0] != '\0'
            ? contact->phone : "Imported contact";
    }

    if(state->route != CALENDAR_ROUTE_TODAY &&
       state->route != CALENDAR_ROUTE_UPCOMING &&
       state->route != CALENDAR_ROUTE_DAY_EVENTS) {
        swatch = make_box(parent, 204, 76, 72, 72, 16,
                          color, LV_OPA_COVER);
        label = make_label(swatch, symbol, &lv_font_montserrat_24,
                           COLOR_WHITE, 230);
        lv_obj_center(label);
    }
    text_panel = make_preview_text_panel(158, 50);
    label = make_label(text_panel, title != NULL ? title : "",
                       CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 5);
    label = make_label(text_panel, detail, CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 29);
}

static void render_menu_preview(const struct route_state *state,
                                bool animated)
{
    bool should_animate =
        animated && menu_preview_motion_ready &&
        is_skeuomorphic_preview_route(state->route);

    if(state->route == MUSIC_ROUTE_MENU)
        menu_preview_motion_profile = MENU_PREVIEW_PROFILE_MUSIC;
    else if(state->route == PHOTOS_ROUTE_MENU)
        menu_preview_motion_profile = MENU_PREVIEW_PROFILE_PHOTOS;
    else if(state->route >= NOTES_ROUTE_MENU &&
            state->route <= NOTES_ROUTE_EMPTY_TRASH_CONFIRM)
        menu_preview_motion_profile = MENU_PREVIEW_PROFILE_NOTES;
    else if(state->route >= BOOKS_ROUTE_MENU &&
            state->route <= BOOKS_ROUTE_INFO)
        menu_preview_motion_profile = MENU_PREVIEW_PROFILE_BOOKS;
    else
        menu_preview_motion_profile = MENU_PREVIEW_PROFILE_DEFAULT;

    menu_preview_build_defer_media = should_animate;
    menu_preview_media_deferred = false;
    reset_menu_preview_root();
    if(state->route == MUSIC_ROUTE_SEARCH)
        render_editor_preview(search_query, "Any track",
                              "Searches title, artist and album.");
    else if(state->route == CALENDAR_ROUTE_TITLE_EDITOR) {
        static char calendar_cursor_text[
            sizeof(calendar_editor_summary) + 2];
        render_editor_preview(
            note_text_with_cursor(
                calendar_editor_summary,
                calendar_editor_cursor,
                calendar_cursor_text,
                sizeof(calendar_cursor_text)),
            "Event title",
            "Center inserts · Left/Right moves cursor.");
    }
    else if(state->route == MUSIC_ROUTE_MENU)
        render_root_preview(state->selected);
    else if(state->route == PHOTOS_ROUTE_MENU)
        render_photos_preview(state);
    else if(state->route == PHOTOS_ROUTE_VIDEOS)
        render_videos_preview(state);
    else if(state->route == EXTRAS_ROUTE_MENU)
        render_extras_preview(state);
    else if(state->route >= NOTES_ROUTE_MENU &&
            state->route <= NOTES_ROUTE_EMPTY_TRASH_CONFIRM)
        render_notes_preview(state);
    else if(state->route >= BOOKS_ROUTE_MENU &&
            state->route <= BOOKS_ROUTE_INFO)
        render_books_preview(state);
    else if(state->route == UTILITIES_ROUTE_MENU ||
            state->route == CLOCK_ROUTE_MENU ||
            state->route == CLOCK_ROUTE_SLEEP_TIMER ||
            state->route == WORKOUT_ROUTE_MENU ||
            state->route == WORKOUT_ROUTE_TYPES ||
            state->route == WORKOUT_ROUTE_HISTORY ||
            state->route == WORKOUT_ROUTE_FINISH_CONFIRM ||
            state->route == WORKOUT_ROUTE_DELETE_CONFIRM ||
            state->route == CALENDAR_ROUTE_MENU ||
            state->route == CALENDAR_ROUTE_TODAY ||
            state->route == CALENDAR_ROUTE_UPCOMING ||
            state->route == CALENDAR_ROUTE_DAY_EVENTS ||
            state->route == CALENDAR_ROUTE_EDITOR ||
            state->route == CALENDAR_ROUTE_ACTIONS ||
            state->route == CALENDAR_ROUTE_DELETE_CONFIRM ||
            state->route == CONTACTS_ROUTE_LIST)
        render_utility_preview(state);
    else if(is_settings_route(state->route))
        render_settings_preview(state);
    else if(state->route >= DIY_ROUTE_MENU)
        render_diy_preview(state);
    else
        render_item_preview(state);
    menu_preview_build_defer_media = false;
    menu_preview_pending = false;
    menu_preview_motion_ready = true;
    if(should_animate)
        start_menu_preview_scene_entrance();
}

static void render_photo_favorite_status(int photo_index)
{
    const bool show_progress =
        photo_select_holding && !photo_select_long_handled &&
        photo_select_hold_percent >= 0;
    const bool show_feedback =
        photo_favorite_feedback_until != 0 &&
        TIME_BEFORE(current_tick, photo_favorite_feedback_until);
    lv_obj_t *panel;
    lv_obj_t *label;

    if(!show_progress && !show_feedback)
        return;
    prepare_glass_descriptor(
        64, 172, 192, 34, CRAZYPOD_GLASS_INFO_TOAST,
        info_toast_glass_pixels, &info_toast_glass_descriptor);
    panel = make_glass_material_panel(
        product_content, 64, 172, 192, 34, 12,
        CRAZYPOD_GLASS_INFO_TOAST, &info_toast_glass_descriptor);
    make_pixel_heart(panel, 11, 10, 2,
                     photo_favorite_feedback_error
                         ? COLOR_MUTED : 0xFF375F,
                     LV_OPA_COVER);
    if(show_progress) {
        int width = CRAZYPOD_PHOTO_FAVORITE_PROGRESS_WIDTH *
            photo_select_hold_percent / 100;
        lv_obj_t *track;

        label = make_label(
            panel,
            crazypod_photo_is_favorite(photo_index)
                ? "Hold to Remove Favorite"
                : "Hold to Add Favorite",
            &lv_font_montserrat_8, COLOR_WHITE, 225);
        lv_obj_set_pos(label, 35, 5);
        track = make_box(
            panel, 35, 22, CRAZYPOD_PHOTO_FAVORITE_PROGRESS_WIDTH,
            3, LV_RADIUS_CIRCLE, COLOR_WHITE, 40);
        if(width < 1)
            width = 1;
        photo_favorite_progress_fill = make_box(
            track, 0, 0, width, 3, LV_RADIUS_CIRCLE,
            0xFF375F, LV_OPA_COVER);
    }
    else {
        const char *message = photo_favorite_feedback_error
            ? "Favorite Save Failed"
            : photo_favorite_feedback_added
                ? "Saved to Favorites"
                : "Removed from Favorites";

        label = make_label(panel, message, &lv_font_montserrat_10,
                           COLOR_WHITE, LV_OPA_COVER);
        lv_obj_set_pos(label, 35, 10);
    }
}

static void render_photo_grid(const struct route_state *state)
{
    const int columns = 4;
    const int visible_rows = 2;
    const int cell_size = 64;
    const int column_gap = 8;
    const int row_gap = 12;
    int count = route_item_count(state);
    int selected_row = state->selected / columns;
    int start_row = selected_row > 0 ? selected_row - 1 : 0;
    int total_rows = (count + columns - 1) / columns;
    int visible;
    lv_obj_t *label;
    char position[96];

    if(start_row > total_rows - visible_rows)
        start_row = total_rows - visible_rows;
    if(start_row < 0)
        start_row = 0;
    label = make_label(product_content, route_title(state),
                       CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, 150);
    lv_obj_set_pos(label, 14, 40);
    if(count <= 0) {
        render_empty_state(
            LV_SYMBOL_IMAGE,
            state->route == PHOTOS_ROUTE_FAVORITES
                ? "No Favorites" : "No Pictures",
            state->route == PHOTOS_ROUTE_FAVORITES
                ? "Hold Select on a photo to save it here."
                : "Add JPG, JPEG or BMP files to /Pictures.");
        if(state->route != DIY_ROUTE_WALLPAPER_FILES)
            render_photo_favorite_status(-1);
        return;
    }
    for(visible = 0; visible < columns * visible_rows; ++visible) {
        int row = visible / columns;
        int column = visible % columns;
        int position_index =
            (start_row + row) * columns + column;
        int photo_index;
        int x;
        int y;
        bool selected;
        lv_obj_t *cell;
        const lv_image_dsc_t *descriptor;

        if(position_index >= count)
            continue;
        photo_index = photo_route_index(state, position_index);
        if(photo_index < 0)
            continue;
        x = 20 + column * (cell_size + column_gap);
        y = 59 + row * (cell_size + row_gap);
        selected = position_index == state->selected;
        cell = make_box(product_content, x, y,
                        cell_size, cell_size, 7,
                        selected ? highlight_primary() : 0x050507,
                        LV_OPA_COVER);
        lv_obj_set_style_clip_corner(cell, true, 0);
        descriptor = crazypod_photo_thumbnail(visible, photo_index);
        if(descriptor != NULL)
            render_photo_image(cell, descriptor, 0, 0,
                               cell_size, cell_size);
        else {
            lv_obj_t *symbol = make_label(
                cell, LV_SYMBOL_REFRESH,
                &lv_font_montserrat_16,
                COLOR_WHITE, 65);
            lv_obj_center(symbol);
        }
        if(crazypod_photo_is_favorite(photo_index)) {
            make_pixel_heart(cell, 52, 5, 1,
                             0xFF375F, LV_OPA_COVER);
        }
        {
            lv_obj_t *ring = make_box(
                cell, 0, 0, cell_size, cell_size, 7,
                COLOR_WHITE, LV_OPA_TRANSP);

            lv_obj_set_style_border_width(
                ring, selected ? 3 : 1, 0);
            lv_obj_set_style_border_color(
                ring,
                lv_color_hex(
                    selected ? COLOR_WHITE : COLOR_PANEL), 0);
            lv_obj_set_style_border_opa(
                ring, selected ? 235 : 90, 0);
            lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    snprintf(position, sizeof(position), "%d / %d  %s",
             state->selected + 1, count,
             crazypod_photo_name(
                 photo_route_index(state, state->selected)));
    label = make_label(product_content, position,
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 145);
    lv_obj_set_width(label, 292);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 14, 211);
    if(state->route != DIY_ROUTE_WALLPAPER_FILES)
        render_photo_favorite_status(
            photo_route_index(state, state->selected));
}

static void render_photo_detail(const struct route_state *state)
{
    const int info_x = 10;
    const int info_y = 196;
    const int info_width = LCD_WIDTH - 20;
    const int info_height = 34;
    const int info_radius = 12;
    const lv_image_dsc_t *descriptor =
        crazypod_photo_render_viewport(
            state->group, photo_zoom_percent,
            &photo_pan_x, &photo_pan_y);
    lv_obj_t *viewport = make_box(
        product_content, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, LV_OPA_COVER);
    lv_obj_t *label;
    char zoom_label[24];

    lv_obj_set_style_clip_corner(viewport, true, 0);
    if(descriptor != NULL) {
        lv_obj_t *image = lv_image_create(viewport);

        lv_image_set_src(image, descriptor);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }
    else {
        label = make_label(viewport, LV_SYMBOL_REFRESH,
                           &lv_font_montserrat_24,
                           COLOR_WHITE, 130);
        lv_obj_set_pos(label, 148, 89);
        label = make_label(viewport, "Loading photo",
                           &lv_font_montserrat_10,
                           COLOR_WHITE, 160);
        lv_obj_set_width(label, 200);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 60, 124);
    }
    prepare_glass_descriptor(
        info_x, info_y, info_width, info_height,
        CRAZYPOD_GLASS_TEXT_PANEL,
        info_bar_glass_pixels, &info_bar_glass_descriptor);
    make_glass_material_panel(
        viewport, info_x, info_y, info_width, info_height,
        info_radius,
        CRAZYPOD_GLASS_TEXT_PANEL, &info_bar_glass_descriptor);
    label = make_label(
        viewport, crazypod_photo_name(state->group),
        &lv_font_montserrat_8, COLOR_WHITE, 225);
    lv_obj_set_width(label, 204);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, info_x + 12, info_y + 11);
    if(photo_zoom_percent > 100) {
        int tenths = (photo_zoom_percent + 5) / 10;

        snprintf(zoom_label, sizeof(zoom_label), "%d.%dx",
                 tenths / 10, tenths % 10);
    }
    else {
        snprintf(zoom_label, sizeof(zoom_label), "FIT");
    }
    if(crazypod_photo_is_favorite(state->group))
        make_pixel_heart(viewport, info_x + 216, info_y + 13, 1,
                         0xFF375F, LV_OPA_COVER);
    label = make_label(
        viewport, zoom_label,
        &lv_font_montserrat_8, COLOR_WHITE, 210);
    lv_obj_set_width(label, 54);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, info_x + 234, info_y + 11);
    render_photo_favorite_status(state->group);
}

static bool wallpaper_crop_rect(
    const lv_image_dsc_t *source,
    int *crop_x, int *crop_y, int *crop_width, int *crop_height)
{
    int source_width;
    int source_height;
    int maximum_width;
    int maximum_height;
    int maximum_zoom;
    int width;
    int height;

    if(source == NULL || crop_x == NULL || crop_y == NULL ||
       crop_width == NULL || crop_height == NULL ||
       source->header.w <= 0 || source->header.h <= 0)
        return false;
    source_width = source->header.w;
    source_height = source->header.h;
    if(source_width * 3 > source_height * 4) {
        maximum_height = source_height;
        maximum_width = maximum_height * 4 / 3;
    }
    else {
        maximum_width = source_width;
        maximum_height = maximum_width * 3 / 4;
    }
    if(wallpaper_crop_zoom_percent < 100)
        wallpaper_crop_zoom_percent = 100;
    maximum_zoom =
        crazypod_wallpaper_crop_max_zoom(source);
    if(wallpaper_crop_zoom_percent > maximum_zoom)
        wallpaper_crop_zoom_percent = maximum_zoom;
    width = maximum_width * 100 / wallpaper_crop_zoom_percent;
    height = maximum_height * 100 / wallpaper_crop_zoom_percent;
    if(width < 4)
        width = 4;
    if(height < 3)
        height = 3;
    width -= width % 4;
    height = width * 3 / 4;
    if(height > source_height) {
        height = source_height - source_height % 3;
        width = height * 4 / 3;
    }
    if(wallpaper_crop_center_x < 0)
        wallpaper_crop_center_x = source_width / 2;
    if(wallpaper_crop_center_y < 0)
        wallpaper_crop_center_y = source_height / 2;
    if(wallpaper_crop_center_x < width / 2)
        wallpaper_crop_center_x = width / 2;
    if(wallpaper_crop_center_x > source_width - (width + 1) / 2)
        wallpaper_crop_center_x =
            source_width - (width + 1) / 2;
    if(wallpaper_crop_center_y < height / 2)
        wallpaper_crop_center_y = height / 2;
    if(wallpaper_crop_center_y >
       source_height - (height + 1) / 2)
        wallpaper_crop_center_y =
            source_height - (height + 1) / 2;
    *crop_x = wallpaper_crop_center_x - width / 2;
    *crop_y = wallpaper_crop_center_y - height / 2;
    *crop_width = width;
    *crop_height = height;
    return true;
}

static void render_wallpaper_crop(void)
{
    const lv_image_dsc_t *source =
        crazypod_photo_view(wallpaper_crop_photo_index);
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    bool crop_valid = wallpaper_crop_rect(
        source, &crop_x, &crop_y, &crop_width, &crop_height);
    const lv_image_dsc_t *preview = crop_valid
        ? crazypod_photo_render_crop_preview(
              wallpaper_crop_photo_index,
              wallpaper_crop_center_y)
        : NULL;
    lv_obj_t *viewport = make_box(
        product_content, 0, 40, LCD_WIDTH, LCD_HEIGHT - 40,
        0, 0x000000, LV_OPA_COVER);
    lv_obj_t *label;
    const char *instruction;

    if(source != NULL && preview != NULL) {
        const int canvas_height = 168;
        int display_height;
        int display_y;
        int frame_x;
        int frame_y;
        int frame_width;
        int frame_height;
        int visible_frame_y;
        int visible_frame_bottom;
        int visible_frame_height;
        lv_obj_t *image;
        lv_obj_t *ring;

        display_height =
            source->header.h * LCD_WIDTH /
            source->header.w;
        if(display_height < 1)
            display_height = 1;
        if(display_height <= canvas_height)
            display_y =
                (canvas_height - display_height) / 2;
        else {
            display_y = canvas_height / 2 -
                wallpaper_crop_center_y * display_height /
                source->header.h;
            if(display_y > 0)
                display_y = 0;
            if(display_y < canvas_height - display_height)
                display_y = canvas_height - display_height;
        }
        image = lv_image_create(viewport);
        lv_image_set_src(image, preview);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
        if(crop_valid) {
            frame_x = crop_x * LCD_WIDTH /
                source->header.w;
            frame_y = display_y +
                crop_y * display_height /
                source->header.h;
            frame_width = crop_width * LCD_WIDTH /
                source->header.w;
            frame_height = crop_height * display_height /
                source->header.h;
            visible_frame_y = frame_y > 0 ? frame_y : 0;
            visible_frame_bottom =
                frame_y + frame_height < canvas_height
                ? frame_y + frame_height : canvas_height;
            visible_frame_height =
                visible_frame_bottom - visible_frame_y;
            if(frame_y > 0)
                make_box(viewport, 0, 0, LCD_WIDTH, frame_y,
                         0, 0x000000, 150);
            if(frame_y + frame_height < canvas_height)
                make_box(
                    viewport, 0, frame_y + frame_height,
                    LCD_WIDTH,
                    canvas_height - frame_y - frame_height,
                    0, 0x000000, 150);
            if(frame_x > 0 && visible_frame_height > 0)
                make_box(
                    viewport, 0, visible_frame_y, frame_x,
                    visible_frame_height, 0, 0x000000, 150);
            if(frame_x + frame_width < LCD_WIDTH &&
               visible_frame_height > 0)
                make_box(
                    viewport, frame_x + frame_width,
                    visible_frame_y,
                    LCD_WIDTH - frame_x - frame_width,
                    visible_frame_height, 0, 0x000000, 150);
            ring = make_box(
                viewport, frame_x, frame_y,
                frame_width, frame_height, 6,
                COLOR_WHITE, LV_OPA_TRANSP);
            lv_obj_set_style_border_width(ring, 2, 0);
            lv_obj_set_style_border_color(
                ring, lv_color_hex(COLOR_WHITE), 0);
            lv_obj_set_style_border_opa(ring, 235, 0);
        }
    }
    else {
        int progress =
            crazypod_photo_view_progress(
                wallpaper_crop_photo_index);
        char loading_text[40];
        lv_obj_t *track;
        int fill_width;

        wallpaper_crop_load_progress_seen = progress;
        label = make_label(viewport, LV_SYMBOL_REFRESH,
                           &lv_font_montserrat_24,
                           COLOR_WHITE, 130);
        lv_obj_set_pos(label, 148, 55);
        if(progress < 0)
            snprintf(loading_text, sizeof(loading_text),
                     "Could not load picture");
        else
            snprintf(loading_text, sizeof(loading_text),
                     "Loading picture  %d%%",
                     progress > 100 ? 100 : progress);
        wallpaper_crop_progress_label = make_label(
            viewport, loading_text,
            &lv_font_montserrat_10, COLOR_WHITE, 185);
        lv_obj_set_width(
            wallpaper_crop_progress_label, 220);
        lv_obj_set_style_text_align(
            wallpaper_crop_progress_label,
            LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(
            wallpaper_crop_progress_label, 50, 91);
        track = make_box(
            viewport, 60, 112, 200, 6,
            LV_RADIUS_CIRCLE, COLOR_WHITE, 35);
        if(progress < 0)
            fill_width = 200;
        else {
            fill_width = progress * 200 / 100;
            if(fill_width < 2)
                fill_width = 2;
            if(fill_width > 200)
                fill_width = 200;
        }
        wallpaper_crop_progress_fill = make_box(
            track, 0, 0, fill_width, 6,
            LV_RADIUS_CIRCLE,
            progress < 0 ? 0xFF453A : COLOR_CYAN,
            LV_OPA_COVER);
    }
    {
        lv_obj_t *hint;

        prepare_glass_descriptor(
            6, 45, LCD_WIDTH - 12, 34,
            CRAZYPOD_GLASS_INFO_TOAST,
            info_bar_alt_glass_pixels,
            &info_bar_alt_glass_descriptor);
        hint = make_glass_material_panel(
            viewport, 6, 5, LCD_WIDTH - 12, 34, 10,
            CRAZYPOD_GLASS_INFO_TOAST,
            &info_bar_alt_glass_descriptor);

        label = make_label(
            hint,
            "Click Arrows: Move  \xE2\x80\xA2  Rotate: Zoom  "
            "\xE2\x80\xA2  SELECT: Apply",
            &lv_font_montserrat_8, COLOR_WHITE, 230);
        lv_obj_set_width(label, LCD_WIDTH - 24);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 6, 4);
        label = make_label(
            hint,
            "Hold MENU 0.5s: Cancel  \xE2\x80\xA2  "
            "Hold PLAY 0.5s: Reset",
            &lv_font_montserrat_8, 0xFFD60A, 235);
        lv_obj_set_width(label, LCD_WIDTH - 24);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 6, 18);
    }
    if(wallpaper_crop_phase == WALLPAPER_CROP_APPLYING) {
        lv_obj_t *panel;
        lv_obj_t *track;
        char progress_text[40];
        int fill_width =
            wallpaper_crop_apply_progress * 200 / 100;

        prepare_glass_descriptor(
            45, 107, 230, 50, CRAZYPOD_GLASS_INFO_TOAST,
            info_toast_glass_pixels, &info_toast_glass_descriptor);
        panel = make_glass_material_panel(
            viewport, 45, 67, 230, 50, 12,
            CRAZYPOD_GLASS_INFO_TOAST, &info_toast_glass_descriptor);
        if(fill_width < 2)
            fill_width = 2;
        if(fill_width > 200)
            fill_width = 200;
        snprintf(progress_text, sizeof(progress_text),
                 "Applying wallpaper  %d%%",
                 wallpaper_crop_apply_progress);
        wallpaper_crop_progress_label = make_label(
            panel, progress_text,
            &lv_font_montserrat_10, COLOR_WHITE, 230);
        lv_obj_set_width(
            wallpaper_crop_progress_label, 210);
        lv_obj_set_style_text_align(
            wallpaper_crop_progress_label,
            LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(
            wallpaper_crop_progress_label, 10, 9);
        track = make_box(
            panel, 15, 31, 200, 7,
            LV_RADIUS_CIRCLE, COLOR_WHITE, 38);
        wallpaper_crop_progress_fill = make_box(
            track, 0, 0, fill_width, 7,
            LV_RADIUS_CIRCLE, COLOR_CYAN,
            LV_OPA_COVER);
    }
    prepare_glass_descriptor(
        0, 208, LCD_WIDTH, 32, CRAZYPOD_GLASS_TEXT_PANEL,
        info_bar_glass_pixels, &info_bar_glass_descriptor);
    make_glass_material_panel(
        viewport, 0, 168, LCD_WIDTH, 32, 0,
        CRAZYPOD_GLASS_TEXT_PANEL, &info_bar_glass_descriptor);
    label = make_label(
        viewport,
        crazypod_photo_name(wallpaper_crop_photo_index),
        &lv_font_montserrat_8, COLOR_WHITE, 225);
    lv_obj_set_pos(label, 9, 173);
    lv_obj_set_width(label, 156);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    {
        char zoom[20];

        snprintf(zoom, sizeof(zoom), "%d.%dx",
                 wallpaper_crop_zoom_percent / 100,
                 (wallpaper_crop_zoom_percent % 100) / 10);
        label = make_label(
            viewport, zoom, &lv_font_montserrat_8,
            COLOR_WHITE, 220);
        lv_obj_set_width(label, 42);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(label, 269, 173);
    }
    if(wallpaper_crop_phase == WALLPAPER_CROP_APPLYING)
        instruction = "Applying wallpaper...";
    else if(wallpaper_crop_phase == WALLPAPER_CROP_APPLIED)
        instruction = wallpaper_crop_target ==
                          CRAZYPOD_APPEARANCE_HOME_BACKGROUND
            ? "Applied to Home"
            : wallpaper_crop_target ==
                  CRAZYPOD_APPEARANCE_MENU_BACKGROUND
                ? "Applied to Menu"
                : "Applied to Lock Screen";
    else if(wallpaper_crop_phase == WALLPAPER_CROP_ERROR) {
        if(wallpaper_crop_error_loading)
            instruction = "Picture is still loading";
        else if(wallpaper_crop_apply_result ==
                CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE)
            instruction = "Invalid crop area";
        else if(wallpaper_crop_apply_result ==
                CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED)
            instruction = "Not enough image workspace";
        else if(wallpaper_crop_apply_result ==
                CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED)
            instruction = "Could not decode full picture";
        else if(wallpaper_crop_apply_result ==
                CRAZYPOD_WALLPAPER_APPLY_CACHE_OPEN_FAILED)
            instruction = "Could not open wallpaper cache";
        else if(wallpaper_crop_apply_result ==
                CRAZYPOD_WALLPAPER_APPLY_CACHE_WRITE_FAILED)
            instruction = "Could not write wallpaper cache";
        else if(wallpaper_crop_apply_result ==
                CRAZYPOD_WALLPAPER_APPLY_CACHE_PUBLISH_FAILED)
            instruction = "Could not publish wallpaper cache";
        else if(wallpaper_crop_apply_result ==
                CRAZYPOD_WALLPAPER_APPLY_SETTINGS_FAILED)
            instruction = "Could not save wallpaper setting";
        else if(wallpaper_crop_apply_result ==
                CRAZYPOD_WALLPAPER_APPLY_ACTIVATE_FAILED)
            instruction = "Could not activate wallpaper";
        else
            instruction = "Could not apply wallpaper";
    }
    else if(wallpaper_crop_menu_armed)
        instruction = "Release MENU to Cancel";
    else if(wallpaper_crop_play_armed)
        instruction = "Release PLAY to Reset";
    else
        instruction =
            "SELECT Apply  \xE2\x80\xA2  Wheel Zoom  "
            "\xE2\x80\xA2  Hold MENU Cancel";
    label = make_label(
        viewport, instruction,
        &lv_font_montserrat_8, COLOR_WHITE,
        wallpaper_crop_phase == WALLPAPER_CROP_EDITING
            ? 145 : 220);
    lv_obj_set_width(label, 302);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 9, 187);
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
    lv_obj_set_pos(header, CRAZYPOD_MENU_HEADER_X,
                   CRAZYPOD_MENU_HEADER_Y);
    lv_obj_set_width(header, CRAZYPOD_MENU_HEADER_WIDTH);
    lv_obj_set_height(header, CRAZYPOD_MENU_HEADER_HEIGHT);
    lv_label_set_long_mode(header, LV_LABEL_LONG_MODE_DOTS);

    if(count <= 0) {
        if(state->route == DIY_ROUTE_WALLPAPER_FILES)
            render_empty_state(LV_SYMBOL_IMAGE, "No Pictures",
                               "Add JPG or BMP files to /Pictures.");
        else if(state->route == PHOTOS_ROUTE_LIBRARY)
            render_empty_state(
                LV_SYMBOL_IMAGE,
                "No Pictures",
                "Add JPG, JPEG or BMP files to /Pictures.");
        else if(state->route == PHOTOS_ROUTE_VIDEOS)
            render_empty_state(
                LV_SYMBOL_PLAY,
                "No Videos",
                "Convert MPG or MPEG files into /Videos.");
        else if(state->route == PHOTOS_ROUTE_FAVORITES)
            render_empty_state(
                LV_SYMBOL_IMAGE,
                "No Favorites",
                               "Hold Select on a photo to save it here.");
        else if(state->route == EXTRAS_ROUTE_MENU)
            render_empty_state(
                LV_SYMBOL_DIRECTORY,
                "Nothing Hidden",
                "Hide apps in Settings > Main Menu.");
        else if(state->route == UTILITIES_ROUTE_MENU)
            render_empty_state(
                LV_SYMBOL_FILE,
                "No Mini Apps",
                "Copy a signed CPK to /MiniApps/Install.");
        else if(state->route == NOTES_ROUTE_DELETED)
            render_empty_state(
                LV_SYMBOL_EDIT,
                "Deleted Is Empty",
                "Deleted notes can be restored from here.");
        else if(state->route == BOOKS_ROUTE_LIBRARY)
            render_empty_state(
                LV_SYMBOL_FILE,
                "No Books",
                "Add EPUB, TXT or Markdown files to /Books.");
        else if(state->route == BOOKS_ROUTE_RECENTS)
            render_empty_state(
                LV_SYMBOL_FILE,
                "No Recent Books",
                "Open a book to add it here.");
        else if(state->route == BOOKS_ROUTE_FAVORITES)
            render_empty_state(
                LV_SYMBOL_FILE,
                "No Favorites",
                "Favorite a book from Book Actions.");
        else if(state->route == BOOKS_ROUTE_BOOKMARKS)
            render_empty_state(
                LV_SYMBOL_FILE,
                "No Bookmark",
                "Press PLAY while reading to save this page.");
        else if(state->route == PODCASTS_ROUTE_MENU)
            render_empty_state(
                LV_SYMBOL_AUDIO,
                "No Podcasts",
                "Add audio files under /Podcasts and rescan.");
        else if(state->route == CONTACTS_ROUTE_LIST)
            render_empty_state(
                NULL,
                "No Contacts",
                "Add VCF files to /Contacts.");
        else if(state->route == WORKOUT_ROUTE_HISTORY)
            render_empty_state(
                LV_SYMBOL_PLAY,
                "No Workouts",
                "Start a time-only workout first.");
        else
            render_empty_state(LV_SYMBOL_AUDIO, "Nothing Here",
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
        int y = CRAZYPOD_MENU_ROW_Y + row * CRAZYPOD_MENU_ROW_STEP;
        bool selected = index == state->selected;
        lv_obj_t *row_box;
        lv_obj_t *label;
        lv_obj_t *marker;
        const char *title;
        int text_x = 12;
        int text_width = 120;

        if(index >= count)
            break;
        row_box = make_box(product_content,
                           CRAZYPOD_MENU_ROW_X, y,
                           CRAZYPOD_MENU_ROW_WIDTH,
                           CRAZYPOD_MENU_ROW_HEIGHT, 8,
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
           state->route == PHOTOS_ROUTE_MENU ||
           state->route == SETTINGS_ROUTE_MENU ||
           state->route == SETTINGS_ROUTE_MAIN_MENU ||
           state->route == EXTRAS_ROUTE_MENU ||
           state->route == UTILITIES_ROUTE_MENU ||
           state->route == DIY_ROUTE_MENU) {
            struct crazypod_app *route_app_item =
                route_app(state, index);
            const char *icon_text = route_app_item != NULL
                ? route_app_item->symbol :
                state->route == MUSIC_ROUTE_MENU
                    ? music_menu_symbols[index]
                : state->route == PHOTOS_ROUTE_MENU
                    ? photos_menu_symbols[index]
                : state->route == SETTINGS_ROUTE_MENU
                    ? settings_menu_symbols[index]
                : state->route == UTILITIES_ROUTE_MENU
                    ? miniapp_symbol(index)
                    : diy_menu_symbols[index];
            lv_obj_t *circle = make_box(row_box, 6, 2, 21, 21,
                                        LV_RADIUS_CIRCLE, COLOR_WHITE,
                                        selected ? 45 : 18);
            lv_obj_t *icon;

            if(state->route == PHOTOS_ROUTE_MENU && index == 2) {
                icon = make_box(circle, 0, 0, 8, 6, 0,
                                COLOR_WHITE, LV_OPA_TRANSP);
                make_pixel_heart(icon, 0, 0, 1,
                                 0xFF375F, LV_OPA_COVER);
                lv_obj_center(icon);
                lv_obj_set_style_opa(
                    icon, selected ? 255 : 170, 0);
            }
            else {
                icon = make_label(circle, icon_text,
                                  &lv_font_montserrat_10,
                                  COLOR_WHITE,
                                  selected ? 255 : 170);
                lv_obj_center(icon);
            }
            menu_view.circles[row] = circle;
            menu_view.icons[row] = icon;
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
        int track_height = CRAZYPOD_MENU_SCROLL_HEIGHT;
        int thumb_height = track_height * CRAZYPOD_VISIBLE_ROWS / count;
        int thumb_y;
        lv_obj_t *bar;
        if(thumb_height < 12)
            thumb_height = 12;
        thumb_y = CRAZYPOD_MENU_SCROLL_Y +
                  (track_height - thumb_height) * state->selected /
                      (count - 1);
        bar = make_box(product_content, CRAZYPOD_MENU_SCROLL_X,
                       CRAZYPOD_MENU_SCROLL_Y, 2, track_height, 1,
                       COLOR_WHITE, 25);
        (void)bar;
        bar = make_box(product_content, CRAZYPOD_MENU_SCROLL_X,
                       thumb_y, 2, thumb_height, 1, COLOR_WHITE, 155);
        menu_view.scroll_thumb = bar;
    }

    render_menu_preview(state, false);
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
        lv_obj_set_style_text_opa(
            label,
            selected ? 255 :
            state->route == MUSIC_ROUTE_SEARCH ? 150 : 195, 0);
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
                route_app(state, index) != NULL
                    ? route_app(state, index)->symbol
                : state->route == MUSIC_ROUTE_MENU
                    ? music_menu_symbols[index]
                : state->route == PHOTOS_ROUTE_MENU
                    ? photos_menu_symbols[index]
                : state->route == SETTINGS_ROUTE_MENU
                    ? settings_menu_symbols[index]
                : state->route == UTILITIES_ROUTE_MENU
                    ? miniapp_symbol(index)
                    : diy_menu_symbols[index];
            lv_obj_set_style_bg_opa(menu_view.circles[row],
                                    selected ? 45 : 18, 0);
            if(state->route == PHOTOS_ROUTE_MENU && index == 2)
                lv_obj_set_style_opa(
                    menu_view.icons[row],
                    selected ? 255 : 170, 0);
            else {
                lv_label_set_text(menu_view.icons[row], icon_text);
                lv_obj_set_style_text_opa(
                    menu_view.icons[row],
                    selected ? 255 : 170, 0);
            }
        }
        lv_label_set_text(
            marker,
            route_item_is_current(state, index)
                ? LV_SYMBOL_OK :
            selected ? LV_SYMBOL_PLAY :
            state->route == MUSIC_ROUTE_SEARCH ? "" : LV_SYMBOL_BULLET);
        lv_obj_set_style_text_opa(marker, selected ? 205 : 90, 0);
    }

    if(menu_view.scroll_thumb != NULL && count > 1) {
        int track_height = CRAZYPOD_MENU_SCROLL_HEIGHT;
        int thumb_height = track_height * CRAZYPOD_VISIBLE_ROWS / count;
        int thumb_y;
        if(thumb_height < 12)
            thumb_height = 12;
        thumb_y = CRAZYPOD_MENU_SCROLL_Y +
                  (track_height - thumb_height) *
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

    lv_obj_set_style_bg_color(product_content, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(product_content, LV_OPA_COVER, 0);
    if(count <= 0) {
        render_empty_state(LV_SYMBOL_AUDIO, "No Albums",
                           "Add local music and rescan.");
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
        artwork_slot = now_playing_artwork_slot_for_track(track);
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
    if(track != NULL &&
       presentation_bank >= 0 &&
       now_presentation_valid[presentation_bank] &&
       strcmp(now_presentation_track_path[presentation_bank],
              track->path) == 0) {
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
    lv_obj_set_pos(now_wave_surface, 16, 181);
    lv_obj_set_size(now_wave_surface, 288, 34);
    lv_obj_set_style_bg_opa(now_wave_surface, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(now_wave_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(now_wave_surface, draw_now_wave_event,
                        LV_EVENT_DRAW_MAIN, NULL);
    now_wave_playing_seen =
        (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
        (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    last_now_wave_tick = current_tick;
    now_progress_marker = make_box(
        now_wave_surface, 0, 14, 7, 7,
        LV_RADIUS_CIRCLE, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_style_shadow_width(now_progress_marker, 6, 0);
    lv_obj_set_style_shadow_color(
        now_progress_marker, lv_color_hex(highlight_primary()), 0);
    lv_obj_set_style_shadow_opa(now_progress_marker, 190, 0);
    lv_obj_remove_flag(now_progress_marker, LV_OBJ_FLAG_CLICKABLE);
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
        prepare_now_overlay_glass(true);
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
        prepare_now_overlay_glass(true);
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
        prepare_now_overlay_glass(true);
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

static void clear_choice_overlay_objects(void)
{
    memset(&choice_overlay, 0, sizeof(choice_overlay));
}

static int choice_overlay_count_for(enum choice_overlay_kind kind, int id)
{
    switch(kind) {
    case CHOICE_OVERLAY_ICON_THEME:
        return CRAZYPOD_ICON_THEME_COUNT;
    case CHOICE_OVERLAY_APPEARANCE:
        return appearance_choice_count(
            (enum crazypod_appearance_field)id);
    case CHOICE_OVERLAY_BACKGROUND:
        return CRAZYPOD_APPEARANCE_COLOR_COUNT + 2;
    case CHOICE_OVERLAY_SETTING:
        return settings_choice_count(id);
    case CHOICE_OVERLAY_BOOK_FONT_SIZE:
        return 3;
    case CHOICE_OVERLAY_BOOK_THEME:
        return 4;
    default:
        return 0;
    }
}

static int choice_overlay_current_index(enum choice_overlay_kind kind, int id)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();

    switch(kind) {
    case CHOICE_OVERLAY_ICON_THEME:
        return appearance->icon_theme;
    case CHOICE_OVERLAY_APPEARANCE:
        return appearance_choice_index(
            (enum crazypod_appearance_field)id);
    case CHOICE_OVERLAY_BACKGROUND: {
        const char *path = background_wallpaper(
            appearance, (enum crazypod_appearance_field)id);
        if(path[0] != '\0')
            return CRAZYPOD_APPEARANCE_COLOR_COUNT + 1;
        return appearance_field_value(
            (enum crazypod_appearance_field)id);
    }
    case CHOICE_OVERLAY_SETTING:
        return settings_choice_index(id);
    case CHOICE_OVERLAY_BOOK_FONT_SIZE:
        return crazypod_books_font_size();
    case CHOICE_OVERLAY_BOOK_THEME:
        return crazypod_books_theme();
    default:
        return 0;
    }
}

static const char *choice_overlay_title(void)
{
    switch(choice_overlay.kind) {
    case CHOICE_OVERLAY_ICON_THEME:
        return "ICON THEME";
    case CHOICE_OVERLAY_APPEARANCE:
        return appearance_field_title(
            (enum crazypod_appearance_field)choice_overlay.id);
    case CHOICE_OVERLAY_BACKGROUND:
        return background_field_title(
            (enum crazypod_appearance_field)choice_overlay.id);
    case CHOICE_OVERLAY_SETTING:
        return settings_item_title(choice_overlay.id);
    case CHOICE_OVERLAY_BOOK_FONT_SIZE:
        return "TEXT SIZE";
    case CHOICE_OVERLAY_BOOK_THEME:
        return "PAGE THEME";
    default:
        return "";
    }
}

static const char *choice_overlay_item_title(int index)
{
    switch(choice_overlay.kind) {
    case CHOICE_OVERLAY_ICON_THEME:
        return crazypod_icon_theme_name(index);
    case CHOICE_OVERLAY_APPEARANCE:
        return appearance_choice_title(
            (enum crazypod_appearance_field)choice_overlay.id, index);
    case CHOICE_OVERLAY_BACKGROUND:
        if(index == 0)
            return choice_overlay.id ==
                   CRAZYPOD_APPEARANCE_LOCK_BACKGROUND
                ? "Follow Home" : "Default";
        if(index <= CRAZYPOD_APPEARANCE_COLOR_COUNT)
            return crazypod_appearance_color_name(index - 1);
        return "Choose Picture";
    case CHOICE_OVERLAY_SETTING:
        return settings_choice_title(choice_overlay.id, index);
    case CHOICE_OVERLAY_BOOK_FONT_SIZE: {
        static const char *const sizes[] = {
            "Small  ·  12 pt",
            "Medium  ·  14 pt",
            "Large  ·  16 pt"
        };
        return index >= 0 && index < 3 ? sizes[index] : "";
    }
    case CHOICE_OVERLAY_BOOK_THEME: {
        static const char *const themes[] = {
            "Parchment", "Light", "Mint", "Dark"
        };
        return index >= 0 && index < 4 ? themes[index] : "";
    }
    default:
        return "";
    }
}

static bool choice_overlay_item_is_current(int index)
{
    return index ==
           choice_overlay_current_index(choice_overlay.kind,
                                        choice_overlay.id);
}

static bool choice_overlay_item_color(int index, uint32_t *color)
{
    if(color == NULL)
        return false;
    if(choice_overlay.kind == CHOICE_OVERLAY_APPEARANCE) {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)choice_overlay.id;
        if(field == CRAZYPOD_APPEARANCE_PRIMARY ||
           field == CRAZYPOD_APPEARANCE_SECONDARY) {
            *color = crazypod_appearance_color(
                appearance_choice_value(field, index));
            return true;
        }
    }
    if(choice_overlay.kind == CHOICE_OVERLAY_BACKGROUND) {
        if(index > 0 && index <= CRAZYPOD_APPEARANCE_COLOR_COUNT)
            *color = crazypod_appearance_color(index - 1);
        else
            *color = background_default_color(
                (enum crazypod_appearance_field)choice_overlay.id);
        return true;
    }
    if(choice_overlay.kind == CHOICE_OVERLAY_BOOK_THEME &&
       index >= 0 && index < 4) {
        *color = book_page_colors[index];
        return true;
    }
    return false;
}

static void refresh_choice_overlay(void)
{
    int count = choice_overlay_count_for(choice_overlay.kind,
                                         choice_overlay.id);
    int start;
    int row;
    char counter[24];

    if(choice_overlay.root == NULL || choice_overlay.panel == NULL)
        return;
    choice_overlay.count = count;
    if(count <= 0) {
        choice_overlay.selected = 0;
        return;
    }
    if(choice_overlay.selected < 0)
        choice_overlay.selected = 0;
    if(choice_overlay.selected >= count)
        choice_overlay.selected = count - 1;

    lv_label_set_text(choice_overlay.title, choice_overlay_title());
    lv_label_set_text(
        choice_overlay.value,
        choice_overlay_item_title(choice_overlay.selected));
    snprintf(counter, sizeof(counter), "%d/%d",
             choice_overlay.selected + 1, count);
    lv_label_set_text(choice_overlay.counter, counter);

    if(count <= CRAZYPOD_CHOICE_OVERLAY_ROWS) {
        start = 0;
    }
    else {
        start = choice_overlay.selected -
                CRAZYPOD_CHOICE_OVERLAY_ROWS / 2;
        if(start < 0)
            start = 0;
        if(start > count - CRAZYPOD_CHOICE_OVERLAY_ROWS)
            start = count - CRAZYPOD_CHOICE_OVERLAY_ROWS;
    }

    for(row = 0; row < CRAZYPOD_CHOICE_OVERLAY_ROWS; ++row) {
        int index = start + row;
        bool selected = index == choice_overlay.selected;
        bool current = index < count &&
                       choice_overlay_item_is_current(index);
        uint32_t swatch_color = COLOR_WHITE;
        bool has_color;

        if(index >= count) {
            lv_obj_add_flag(choice_overlay.rows[row],
                            LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(choice_overlay.rows[row],
                           LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(choice_overlay.labels[row],
                          choice_overlay_item_title(index));
        lv_label_set_text(choice_overlay.markers[row],
                          current ? LV_SYMBOL_OK :
                          selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET);
        lv_obj_set_style_bg_color(
            choice_overlay.rows[row],
            lv_color_hex(selected ? COLOR_WHITE : COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(
            choice_overlay.rows[row], selected ? 32 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(
            choice_overlay.rows[row], selected ? 1 : 0, 0);
        lv_obj_set_style_border_color(
            choice_overlay.rows[row], lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(
            choice_overlay.rows[row], selected ? 72 : 0, 0);
        lv_obj_set_style_text_opa(
            choice_overlay.labels[row], selected ? 255 : 180, 0);
        lv_obj_set_style_text_opa(
            choice_overlay.markers[row], current ? 235 :
            selected ? 190 : 80, 0);

        has_color = choice_overlay_item_color(index, &swatch_color);
        lv_obj_set_style_bg_color(
            choice_overlay.swatches[row],
            lv_color_hex(has_color ? swatch_color : COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(
            choice_overlay.swatches[row],
            has_color ? LV_OPA_COVER : selected ? 72 : 28, 0);
    }

    if(choice_overlay.scroll_thumb != NULL) {
        if(count > CRAZYPOD_CHOICE_OVERLAY_ROWS) {
            const int track_y = 57;
            const int track_height = 92;
            int thumb_height =
                track_height * CRAZYPOD_CHOICE_OVERLAY_ROWS / count;
            int thumb_y;

            if(thumb_height < 12)
                thumb_height = 12;
            thumb_y = track_y + (track_height - thumb_height) *
                      choice_overlay.selected / (count - 1);
            lv_obj_set_y(choice_overlay.scroll_thumb, thumb_y);
            lv_obj_set_height(choice_overlay.scroll_thumb,
                              thumb_height);
            lv_obj_remove_flag(choice_overlay.scroll_thumb,
                               LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(choice_overlay.scroll_thumb,
                            LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void begin_choice_overlay(enum choice_overlay_kind kind, int id,
                                 int selected)
{
    if(choice_overlay.root != NULL)
        lv_obj_delete(choice_overlay.root);
    clear_choice_overlay_objects();
    choice_overlay.kind = kind;
    choice_overlay.id = id;
    choice_overlay.selected = selected;
    choice_overlay.count = choice_overlay_count_for(kind, id);
    choice_overlay.root = make_box(product_content, 0, 0,
                                   LCD_WIDTH, LCD_HEIGHT, 0,
                                   0x000000, 30);
    lv_obj_remove_flag(choice_overlay.root, LV_OBJ_FLAG_CLICKABLE);
}

static void show_choice_overlay(enum choice_overlay_kind kind, int id,
                                int selected)
{
    lv_obj_t *track;
    int row;

    if(now_overlay != NOW_OVERLAY_NONE)
        dismiss_now_overlay(false);
    if(selected < 0)
        selected = choice_overlay_current_index(kind, id);
    prepare_now_overlay_glass(true);
    begin_choice_overlay(kind, id, selected);
    choice_overlay.panel = make_glass_panel(
        choice_overlay.root, CRAZYPOD_NOW_POPUP_X, CRAZYPOD_NOW_POPUP_Y,
        CRAZYPOD_NOW_POPUP_WIDTH, CRAZYPOD_NOW_POPUP_HEIGHT);

    choice_overlay.title = make_label(
        choice_overlay.panel, "",
        &lv_font_montserrat_10, COLOR_WHITE, 100);
    lv_obj_set_width(choice_overlay.title, 170);
    lv_label_set_long_mode(choice_overlay.title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(choice_overlay.title, 16, 13);

    choice_overlay.counter = make_label(
        choice_overlay.panel, "0/0",
        &lv_font_montserrat_8, COLOR_WHITE, 120);
    lv_obj_set_width(choice_overlay.counter, 48);
    lv_obj_set_style_text_align(
        choice_overlay.counter, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(choice_overlay.counter, 186, 15);

    choice_overlay.value = make_label(
        choice_overlay.panel, "",
        &lv_font_montserrat_8, COLOR_WHITE, 170);
    lv_obj_set_width(choice_overlay.value, 218);
    lv_label_set_long_mode(choice_overlay.value, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(choice_overlay.value, 16, 31);

    for(row = 0; row < CRAZYPOD_CHOICE_OVERLAY_ROWS; ++row) {
        int y = 52 + row * 23;
        choice_overlay.rows[row] = make_box(
            choice_overlay.panel, 12, y, 218, 21, 8,
            COLOR_WHITE, LV_OPA_TRANSP);
        choice_overlay.swatches[row] = make_box(
            choice_overlay.rows[row], 10, 6, 9, 9,
            LV_RADIUS_CIRCLE, COLOR_WHITE, 35);
        choice_overlay.labels[row] = make_label(
            choice_overlay.rows[row], "",
            CRAZYPOD_METADATA_FONT, COLOR_WHITE, 180);
        lv_obj_set_pos(choice_overlay.labels[row], 30, 3);
        lv_obj_set_width(choice_overlay.labels[row], 146);
        lv_obj_set_height(choice_overlay.labels[row], 15);
        lv_label_set_long_mode(
            choice_overlay.labels[row], LV_LABEL_LONG_MODE_DOTS);
        choice_overlay.markers[row] = make_label(
            choice_overlay.rows[row], LV_SYMBOL_BULLET,
            &lv_font_montserrat_8, COLOR_WHITE, 80);
        lv_obj_set_width(choice_overlay.markers[row], 24);
        lv_obj_set_style_text_align(
            choice_overlay.markers[row], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(choice_overlay.markers[row], 184, 6);
    }

    track = make_box(choice_overlay.panel, 236, 57, 2, 92, 1,
                     COLOR_WHITE, 24);
    (void)track;
    choice_overlay.scroll_thumb = make_box(
        choice_overlay.panel, 236, 57, 2, 20, 1,
        COLOR_WHITE, 150);

    refresh_choice_overlay();
    animate_now_popup(choice_overlay.panel, CRAZYPOD_NOW_POPUP_Y);
}

static void dismiss_choice_overlay(bool refresh_route)
{
    if(choice_overlay.root != NULL) {
        lv_anim_delete(choice_overlay.panel, NULL);
        lv_obj_delete(choice_overlay.root);
    }
    clear_choice_overlay_objects();
    if(refresh_route && product_active && route_depth > 0)
        render_current_route(false);
}

static void move_choice_overlay(int direction)
{
    int next;

    if(choice_overlay.kind == CHOICE_OVERLAY_NONE ||
       choice_overlay.count <= 0)
        return;
    next = choice_overlay.selected + direction;
    if(next < 0)
        next = 0;
    if(next >= choice_overlay.count)
        next = choice_overlay.count - 1;
    if(next == choice_overlay.selected)
        return;
    choice_overlay.selected = next;
    refresh_choice_overlay();
}

static void activate_choice_overlay(void)
{
    enum choice_overlay_kind kind = choice_overlay.kind;
    int id = choice_overlay.id;
    int selected = choice_overlay.selected;

    if(kind == CHOICE_OVERLAY_NONE)
        return;
    if(kind == CHOICE_OVERLAY_ICON_THEME) {
        crazypod_appearance_set_icon_theme(selected);
        refresh_desktop_appearance();
        dismiss_choice_overlay(true);
    }
    else if(kind == CHOICE_OVERLAY_APPEARANCE) {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)id;
        int value = appearance_choice_value(field, selected);

        crazypod_appearance_set_value(field, value);
        refresh_desktop_appearance();
        dismiss_choice_overlay(true);
    }
    else if(kind == CHOICE_OVERLAY_BACKGROUND) {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)id;
        enum crazypod_wallpaper_target target =
            wallpaper_target_for_field(field);

        if(selected == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1) {
            const struct crazypod_appearance *appearance =
                crazypod_appearance_get();
            const char *path =
                background_wallpaper(appearance, field);

            dismiss_choice_overlay(false);
            push_route_selected(DIY_ROUTE_WALLPAPER_FILES, field,
                                photo_index_for_path(path));
        }
        else {
            crazypod_wallpaper_clear(target);
            crazypod_appearance_set_value(field, selected);
            refresh_desktop_appearance();
            dismiss_choice_overlay(true);
        }
    }
    else if(kind == CHOICE_OVERLAY_SETTING) {
        settings_apply_choice(id, selected);
        dismiss_choice_overlay(true);
    }
    else if(kind == CHOICE_OVERLAY_BOOK_FONT_SIZE) {
        dismiss_choice_overlay(false);
        apply_book_font_size(selected);
    }
    else if(kind == CHOICE_OVERLAY_BOOK_THEME) {
        crazypod_books_set_theme(selected);
        dismiss_choice_overlay(true);
    }
}

static void animate_content_entrance(void)
{
    lv_obj_set_x(product_content, 0);
    lv_obj_set_style_opa(product_content, LV_OPA_COVER, 0);
    lv_obj_invalidate(product_content);
}

static void render_note_composer(const struct route_state *state)
{
    static char title_display[CRAZYPOD_NOTE_TITLE_SIZE + 2];
    static char body_display[CRAZYPOD_NOTE_BODY_SIZE + 2];
    lv_obj_t *paper;
    lv_obj_t *label;
    lv_obj_t *key;
    const char *selection = route_item_title(state, state->selected);
    int line;

    paper = make_box(product_content, 10, 38, 300, 190, 12,
                     0xFAEFCB, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        paper, lv_color_hex(0xE8D5A4), 0);
    lv_obj_set_style_bg_grad_dir(paper, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(paper, 1, 0);
    lv_obj_set_style_border_color(paper, lv_color_hex(0xB79F70), 0);
    lv_obj_set_style_border_opa(paper, 100, 0);
    make_box(paper, 36, 12, 1, 166, 0, 0xB82E26, 140);
    for(line = 0; line < 6; ++line)
        make_box(paper, 48, 82 + line * 17, 232, 1, 0,
                 0x7F9EB7, 55);
    for(line = 0; line < 3; ++line) {
        lv_obj_t *hole = make_box(
            paper, 15, 25 + line * 55, 8, 8,
            LV_RADIUS_CIRCLE, 0x5A4A34, 45);
        lv_obj_set_style_border_width(hole, 1, 0);
        lv_obj_set_style_border_color(
            hole, lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(hole, 110, 0);
    }

    label = make_label(
        paper,
        note_editor.source_id == 0 ? "NEW NOTE" : "EDIT NOTE",
        &lv_font_montserrat_8, 0x7F2D23, LV_OPA_COVER);
    lv_obj_set_pos(label, 48, 13);
    label = make_label(
        paper, note_editor_dirty() ? "Unsaved" : "Saved",
        &lv_font_montserrat_8, 0x6E5B42, 220);
    lv_obj_set_width(label, 88);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 192, 13);

    label = make_label(paper, "TITLE", &lv_font_montserrat_8,
                       note_editor_body_active ? 0x8C7958 : 0x94291F,
                       230);
    lv_obj_set_pos(label, 48, 31);
    label = make_label(
        paper,
        note_editor_body_active
            ? (note_editor.title[0] != '\0'
                ? note_editor.title : "Untitled")
            : note_text_with_cursor(
                note_editor.title, note_editor_title_cursor,
                title_display, sizeof(title_display)),
        CRAZYPOD_METADATA_FONT, 0x30291F,
        note_editor.title[0] != '\0' ? 255 : 125);
    lv_obj_set_pos(label, 48, 45);
    lv_obj_set_width(label, 232);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    make_box(paper, 48, 66, 232, 1, 0,
             note_editor_body_active ? 0x8C7958 : 0xBC4034,
             note_editor_body_active ? 55 : 145);

    label = make_label(paper, "BODY", &lv_font_montserrat_8,
                       note_editor_body_active ? 0x94291F : 0x8C7958,
                       230);
    lv_obj_set_pos(label, 48, 72);
    label = make_label(
        paper,
        !note_editor_body_active
            ? (note_editor.body[0] != '\0'
                ? note_editor.body : "Empty body")
            : note_text_with_cursor(
                note_editor.body, note_editor_body_cursor,
                body_display, sizeof(body_display)),
        CRAZYPOD_METADATA_FONT, 0x2A261F,
        note_editor.body[0] != '\0' ? 235 : 115);
    lv_obj_set_pos(label, 48, 86);
    lv_obj_set_width(label, 232);
    lv_obj_set_height(label, 75);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);

    key = make_box(paper, 48, 158, 232, 24, 7,
                   0x74422F, 225);
    lv_obj_set_style_border_width(key, 1, 0);
    lv_obj_set_style_border_color(key, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(key, 75, 0);
    label = make_label(
        key, selection != NULL ? selection : "",
        CRAZYPOD_METADATA_FONT, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 60);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 4, 4);
    label = make_label(
        key,
        "Wheel choose  ·  Center type  ·  PLAY field",
        &lv_font_montserrat_8, COLOR_WHITE, 155);
    lv_obj_set_width(label, 164);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 64, 7);
}

static void render_note_reader(const struct route_state *state)
{
    static char window[CRAZYPOD_NOTE_BODY_SIZE];
    const struct crazypod_note *note =
        crazypod_note_find((uint32_t)state->group);
    lv_obj_t *paper;
    lv_obj_t *label;
    char progress[64];
    int lines = note_body_line_count(note_reader_body);
    int maximum = lines > 9 ? lines - 9 : 0;
    int first = state->selected;

    if(first > maximum)
        first = maximum;
    label = make_label(product_content,
                       note != NULL ? note->title : "Missing Note",
                       CRAZYPOD_METADATA_FONT, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 42);
    lv_obj_set_width(label, 250);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    if(note != NULL && note->pinned) {
        label = make_label(product_content, LV_SYMBOL_OK,
                           &lv_font_montserrat_10, COLOR_AMBER, 230);
        lv_obj_set_pos(label, 288, 44);
    }

    paper = make_box(product_content, 10, 64, 300, 145, 6,
                     0xF5EEDC, LV_OPA_COVER);
    lv_obj_set_style_border_width(paper, 1, 0);
    lv_obj_set_style_border_color(paper, lv_color_hex(0xB7A98E), 0);
    lv_obj_set_style_border_opa(paper, 180, 0);
    format_note_body_window(note_reader_body, first,
                            window, sizeof(window));
    label = make_label(paper,
                       window[0] != '\0' ? window : "This note is empty.",
                       CRAZYPOD_METADATA_FONT, 0x302A22,
                       window[0] != '\0' ? 255 : 125);
    lv_obj_set_pos(label, 9, 7);
    lv_obj_set_width(label, 282);
    lv_obj_set_height(label, 126);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);

    snprintf(progress, sizeof(progress), "%d / %d  ·  Center: Actions",
             first + 1, lines);
    label = make_label(product_content, progress, &lv_font_montserrat_8,
                       COLOR_WHITE, 125);
    lv_obj_set_width(label, 292);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 14, 216);
}

static bool load_book_page(int index, uint32_t offset)
{
    book_page_text[0] = '\0';
    book_next_offset = offset;
    if(!crazypod_book_read_page(index, offset, book_page_text,
                                sizeof(book_page_text),
                                &book_next_offset))
        return false;
    selected_book_index = index;
    book_page_offset = offset;
    return true;
}

static void turn_book_page(int direction)
{
    const struct crazypod_book *book =
        crazypod_book_get(selected_book_index);
    uint32_t target;

    if(book == NULL)
        return;
    if(direction > 0) {
        if(book_next_offset <= book_page_offset ||
           (book->content_size > 0 &&
            book_next_offset >= book->content_size))
            return;
        if(book_page_history_count <
           (int)(sizeof(book_page_history) /
                 sizeof(book_page_history[0])))
            book_page_history[book_page_history_count++] =
                book_page_offset;
        target = book_next_offset;
    }
    else {
        if(book_page_history_count <= 0)
            target = 0;
        else
            target = book_page_history[--book_page_history_count];
        if(target == book_page_offset)
            return;
    }
    if(load_book_page(selected_book_index, target)) {
        crazypod_book_set_progress(selected_book_index, target);
        render_current_route(false);
    }
}

static void render_book_reader(const struct route_state *state)
{
    const struct crazypod_book *book =
        crazypod_book_get(state->group);
    int theme = crazypod_books_theme();
    int font_size = crazypod_books_font_size();
    const lv_font_t *reader_font = font_size == 2
        ? &lv_font_source_han_sans_sc_16_cjk
        : CRAZYPOD_METADATA_FONT;
    lv_obj_t *page;
    lv_obj_t *toolbar;
    lv_obj_t *label;
    char progress[24];
    uint32_t total = book != NULL && book->content_size > 0
        ? book->content_size : book != NULL ? book->size : 0;
    unsigned percent = total > 0
        ? book_page_offset * 100u / total : 0;

    page = make_box(product_content, 0, 0, 320, 240, 0,
                    book_page_colors[theme], LV_OPA_COVER);
    label = make_label(
        page,
        book_page_text[0] != '\0'
            ? book_page_text : "This book could not be decoded.",
        reader_font, book_ink_colors[theme],
        LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 42);
    lv_obj_set_width(label, 292);
    lv_obj_set_height(label, 158);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    {
        static const int scales[] = { 224, 256, 256 };

        lv_obj_set_style_transform_scale_x(
            label, scales[font_size], 0);
        lv_obj_set_style_transform_scale_y(
            label, scales[font_size], 0);
        lv_obj_set_style_transform_pivot_x(label, 0, 0);
        lv_obj_set_style_transform_pivot_y(label, 0, 0);
        if(font_size == 0) {
            lv_obj_set_width(label, 332);
            lv_obj_set_height(label, 174);
        }
        else if(font_size == 2) {
            lv_obj_set_width(label, 258);
            lv_obj_set_height(label, 136);
        }
    }

    toolbar = make_box(
        page, 0, 206, 320, 34, 0,
        theme == 3 ? 0xFFFFFF : 0x000000,
        theme == 3 ? 24 : 15);
    label = make_label(toolbar, LV_SYMBOL_LEFT,
                       &lv_font_montserrat_16,
                       book_ink_colors[theme], 155);
    lv_obj_set_pos(label, 47, 6);
    label = make_label(
        toolbar,
        book != NULL && book->bookmark == book_page_offset &&
                book_page_offset > 0
            ? LV_SYMBOL_OK : LV_SYMBOL_SAVE,
        &lv_font_montserrat_12,
        book_ink_colors[theme], 155);
    lv_obj_set_pos(label, 151, 7);
    label = make_label(toolbar, LV_SYMBOL_RIGHT,
                       &lv_font_montserrat_16,
                       book_ink_colors[theme], 155);
    lv_obj_set_pos(label, 255, 6);
    snprintf(progress, sizeof(progress), "%u%%", percent);
    label = make_label(toolbar, progress, &lv_font_montserrat_8,
                       book_ink_colors[theme], 115);
    lv_obj_set_width(label, 42);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 139, 23);
}

static void render_books_stats(const struct route_state *state)
{
    lv_obj_t *label;
    lv_obj_t *panel;
    char text[160];

    label = make_label(product_content, "READING STATS",
                       &lv_font_montserrat_16,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 43);
    panel = make_box(product_content, 14, 74, 292, 126, 10,
                     COLOR_PANEL, 220);
    snprintf(text, sizeof(text),
             "%d books\n%d recently opened\n%d favorites\n\n"
             "Progress is stored on this iPod.",
             crazypod_books_count(),
             crazypod_books_recent_count(),
             crazypod_books_favorite_count());
    label = make_label(panel, text, CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, 230);
    lv_obj_set_pos(label, 14, 13);
    lv_obj_set_width(label, 264);
    (void)state;
}

static void render_book_info(const struct route_state *state)
{
    const struct crazypod_book *book;
    lv_obj_t *label;
    lv_obj_t *panel;
    char text[256];
    const char *format;

    crazypod_book_probe(state->group);
    book = crazypod_book_get(state->group);
    format = book == NULL ? "" :
        book->format == CRAZYPOD_BOOK_TXT ? "TXT" :
        book->format == CRAZYPOD_BOOK_MARKDOWN ? "Markdown" : "EPUB";

    label = make_label(product_content, "BOOK INFO",
                       &lv_font_montserrat_16,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 43);
    panel = make_box(product_content, 14, 74, 292, 132, 10,
                     COLOR_PANEL, 220);
    snprintf(text, sizeof(text),
             "%.60s\n%s%.60s%s\n%.8s · %lu KB\n\n%.80s",
             book != NULL ? book->title : "Missing Book",
             book != NULL && book->author[0] != '\0' ? "by " : "",
             book != NULL ? book->author : "",
             book != NULL && book->author[0] != '\0' ? "" : "Unknown author",
             format,
             (unsigned long)(book != NULL ? book->size / 1024u : 0),
             book != NULL ? book->path : "");
    label = make_label(panel, text, CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, 225);
    lv_obj_set_pos(label, 12, 10);
    lv_obj_set_width(label, 268);
    lv_obj_set_height(label, 112);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

static lv_obj_t *make_clock_hand(lv_obj_t *dial, int center,
                                 int length, int width,
                                 int angle_tenths, uint32_t color)
{
    lv_obj_t *hand = make_box(
        dial, center - width / 2, center - length,
        width, length, width, color, LV_OPA_COVER);

    lv_obj_set_style_transform_pivot_x(hand, width / 2, 0);
    lv_obj_set_style_transform_pivot_y(hand, length, 0);
    lv_obj_set_style_transform_rotation(hand, angle_tenths, 0);
    return hand;
}

static lv_obj_t *make_analog_clock(lv_obj_t *parent, int x, int y,
                                   int size, int hour, int minute,
                                   int second_tenths,
                                   uint32_t dial_color,
                                   uint32_t ink_color)
{
    lv_obj_t *dial = make_box(
        parent, x, y, size, size, LV_RADIUS_CIRCLE,
        dial_color, LV_OPA_COVER);
    int center = size / 2;
    int tick;

    lv_obj_set_style_border_width(dial, 2, 0);
    lv_obj_set_style_border_color(dial, lv_color_hex(ink_color), 0);
    lv_obj_set_style_border_opa(dial, 220, 0);
    for(tick = 0; tick < 12; ++tick) {
        int width = tick % 3 == 0 ? 2 : 1;
        int height = tick % 3 == 0 ? 10 : 6;
        lv_obj_t *mark = make_box(
            dial, center - width / 2, 7,
            width, height, width, tick % 3 == 0
                ? ink_color : 0x949494,
            tick % 3 == 0 ? 235 : 180);
        lv_obj_set_style_transform_pivot_x(mark, width / 2, 0);
        lv_obj_set_style_transform_pivot_y(mark, center - 7, 0);
        lv_obj_set_style_transform_rotation(mark, tick * 300, 0);
    }
    make_clock_hand(dial, center, size * 25 / 100, 4,
                    ((hour % 12) * 30 + minute / 2) * 10,
                    ink_color);
    make_clock_hand(dial, center, size * 36 / 100, 3,
                    minute * 60 + second_tenths / 10,
                    ink_color);
    make_clock_hand(dial, center, size * 42 / 100, 1,
                    second_tenths * 6, ink_color);
    make_box(dial, center - 4, center - 4, 8, 8,
             LV_RADIUS_CIRCLE, ink_color, LV_OPA_COVER);
    return dial;
}

static void render_clock_view(void)
{
    static const char *const weekdays[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    static const char *const months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    struct tm *now = get_time();
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[64];

    make_box(product_content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
             0xF9F9F7, LV_OPA_COVER);
    panel = make_box(product_content, 10, 40, 300, 188, 12,
                     0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(panel, 34, 0);
    make_analog_clock(panel, 14, 23, 140,
                      now->tm_hour, now->tm_min,
                      now->tm_sec * 10 +
                          (current_tick % HZ) * 10 / HZ,
                      0xFFFFFF, 0x0E0E0E);

    label = make_label(panel, "LOCAL TIME",
                       &lv_font_montserrat_8,
                       0x5C5C5C, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 170, 34);
    snprintf(text, sizeof(text), "%02d:%02d:%02d",
             now->tm_hour, now->tm_min, now->tm_sec);
    label = make_label(panel, text, &lv_font_montserrat_24,
                       0x0E0E0E, LV_OPA_COVER);
    lv_obj_set_pos(label, 170, 53);
    make_box(panel, 170, 86, 112, 1, 0,
             0x0E0E0E, 210);
    snprintf(text, sizeof(text), "%s\n%s %d",
             weekdays[now->tm_wday], months[now->tm_mon], now->tm_mday);
    label = make_label(panel, text, &lv_font_montserrat_10,
                       0x5C5C5C, LV_OPA_COVER);
    lv_obj_set_pos(label, 170, 97);
    label = make_label(panel, "DEVICE TIME",
                       &lv_font_montserrat_8, 0x949494, 230);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_set_pos(label, 170, 132);
}

static long stopwatch_elapsed_ticks(void)
{
    return stopwatch_accumulated_ticks +
           (stopwatch_running
                ? current_tick - stopwatch_started_at : 0);
}

static void render_stopwatch_view(void)
{
    static const char *const style_names[] = {
        "CLASSIC SILVER", "OBSIDIAN GOLD", "CHAMPAGNE GOLD"
    };
    static const uint32_t canvas_colors[] = {
        0xF9F9F7, 0xF2F2F2, 0xFFFFFF
    };
    static const uint32_t dial_colors[] = {
        0xFFFFFF, 0xFFFFFF, 0xEDEDED
    };
    static const uint32_t ink_colors[] = {
        0x0E0E0E, 0x2C2416, 0x3B2A10
    };
    long ticks = stopwatch_elapsed_ticks();
    unsigned total_hundredths =
        (unsigned)(ticks * 100 / HZ);
    unsigned minutes = total_hundredths / 6000;
    unsigned seconds = total_hundredths / 100 % 60;
    unsigned hundredths = total_hundredths % 100;
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[32];
    int first_lap;
    int lap;
    int style = stopwatch_style_index % 3;

    make_box(product_content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
             canvas_colors[style], LV_OPA_COVER);
    panel = make_box(product_content, 10, 40, 300, 188, 12,
                     0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(panel, 34, 0);
    make_analog_clock(
        panel, 8, 23, 140,
        (int)(minutes / 60),
        (int)minutes % 60,
        (int)seconds * 10 + (int)hundredths / 10,
        dial_colors[style], ink_colors[style]);
    label = make_label(panel, style_names[style],
                       &lv_font_montserrat_8,
                       ink_colors[style], 135);
    lv_obj_set_width(label, 140);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 8, 168);

    label = make_label(panel, "CHRONOGRAPH",
                       &lv_font_montserrat_8,
                       ink_colors[style], 110);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 166, 22);
    snprintf(text, sizeof(text), "%02u:%02u.%02u",
             minutes, seconds, hundredths);
    label = make_label(panel, text, &lv_font_montserrat_24,
                       ink_colors[style], LV_OPA_COVER);
    lv_obj_set_pos(label, 166, 39);
    label = make_label(panel,
                       stopwatch_running ? "RUNNING" : "PAUSED",
                       &lv_font_montserrat_8,
                       ink_colors[style], 225);
    lv_obj_set_pos(label, 166, 69);
    if(stopwatch_lap_count > 0) {
        snprintf(text, sizeof(text), "%d LAPS",
                 stopwatch_lap_count);
        label = make_label(panel, text,
                           &lv_font_montserrat_8,
                           ink_colors[style], 140);
        lv_obj_set_pos(label, 224, 69);
    }
    make_box(panel, 166, 84, 122, 1, 0,
             ink_colors[style], 52);
    first_lap = stopwatch_lap_count > 4
        ? stopwatch_lap_count - 4 : 0;
    if(stopwatch_lap_count > 0) {
        label = make_label(panel, "LAP       TOTAL",
                           &lv_font_montserrat_8,
                           ink_colors[style], 105);
        lv_obj_set_pos(label, 168, 90);
    }
    for(lap = first_lap; lap < stopwatch_lap_count; ++lap) {
        unsigned lap_hundredths =
            (unsigned)(stopwatch_laps[lap] * 100 / HZ);
        snprintf(text, sizeof(text), "%02d     %02u:%02u.%02u",
                 lap + 1, lap_hundredths / 6000,
                 lap_hundredths / 100 % 60,
                 lap_hundredths % 100);
        label = make_label(panel, text, &lv_font_montserrat_8,
                           ink_colors[style], 225);
        lv_obj_set_pos(label, 168,
                       104 + (lap - first_lap) * 15);
    }
    if(stopwatch_lap_count == 0) {
        label = make_label(panel,
                           "CENTER  START / PAUSE\nRIGHT   RECORD LAP\nLEFT    RESET",
                           &lv_font_montserrat_8,
                           ink_colors[style], 150);
        lv_obj_set_pos(label, 166, 101);
    }
    label = make_label(panel,
                       TIME_BEFORE(current_tick,
                                   stopwatch_reset_armed_until)
                           ? "Press LEFT again to reset"
                           : "Wheel changes style before first lap",
                       &lv_font_montserrat_8, 0x949494, 210);
    lv_obj_set_width(label, 136);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(label, 166, 168);
}

static int days_in_month(int year, int month)
{
    static const int days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if(month == 1 &&
       ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month];
}

static int weekday_for_date(int year, int month, int day)
{
    static const int offsets[] = {
        0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
    };

    if(month < 2)
        --year;
    return (year + year / 4 - year / 100 + year / 400 +
            offsets[month] + day) % 7;
}

static void calendar_move_focus(int direction)
{
    if(direction > 0) {
        ++calendar_focus_day;
        if(calendar_focus_day >
           days_in_month(calendar_focus_year,
                         calendar_focus_month)) {
            calendar_focus_day = 1;
            ++calendar_focus_month;
            if(calendar_focus_month > 11) {
                calendar_focus_month = 0;
                ++calendar_focus_year;
            }
        }
    }
    else if(direction < 0) {
        --calendar_focus_day;
        if(calendar_focus_day < 1) {
            --calendar_focus_month;
            if(calendar_focus_month < 0) {
                calendar_focus_month = 11;
                --calendar_focus_year;
            }
            calendar_focus_day =
                days_in_month(calendar_focus_year,
                              calendar_focus_month);
        }
    }
}

static void render_calendar_grid(void)
{
    static const char *const months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    static const char *const weekdays[] = {
        "S", "M", "T", "W", "T", "F", "S"
    };
    static const char *const compact_weekdays[] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
    };
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[48];
    int first = weekday_for_date(
        calendar_focus_year, calendar_focus_month, 1);
    int count = days_in_month(
        calendar_focus_year, calendar_focus_month);
    int previous_month = calendar_focus_month - 1;
    int previous_year = calendar_focus_year;
    int previous_count;
    int today = calendar_today_date();
    int slot;

    if(previous_month < 0) {
        previous_month = 11;
        --previous_year;
    }
    previous_count = days_in_month(previous_year, previous_month);
    make_box(product_content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
             0xF9F9F7, LV_OPA_COVER);
    panel = make_box(product_content, 10, 38, 300, 194, 12,
                     0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(panel, 34, 0);
    label = make_label(panel, "CALENDAR",
                       &lv_font_montserrat_8,
                       0x949494, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 14, 9);
    snprintf(text, sizeof(text), "%s %d",
             months[calendar_focus_month], calendar_focus_year);
    label = make_label(panel, text,
                       &lv_font_montserrat_16,
                       0x0E0E0E, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 23);
    snprintf(text, sizeof(text), "%s %d",
             compact_weekdays[
                 weekday_for_date(calendar_focus_year,
                                  calendar_focus_month,
                                  calendar_focus_day)],
             calendar_focus_day);
    label = make_label(panel, text, &lv_font_montserrat_10,
                       0x5C5C5C, LV_OPA_COVER);
    lv_obj_set_width(label, 72);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 210, 24);
    make_box(panel, 12, 50, 276, 1, 0,
             0x0E0E0E, 205);

    for(slot = 0; slot < 7; ++slot) {
        label = make_label(panel, weekdays[slot],
                           &lv_font_montserrat_8,
                           0x949494, 230);
        lv_obj_set_width(label, 40);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 10 + slot * 40, 58);
    }
    for(slot = 0; slot < 42; ++slot) {
        int column = slot % 7;
        int row = slot / 7;
        int x = 10 + column * 40;
        int y = 75 + row * 18;
        int relative_day = slot - first + 1;
        int day;
        int year = calendar_focus_year;
        int month = calendar_focus_month;
        bool in_month = true;
        int date;
        bool selected;
        bool is_today;
        bool has_event = false;
        int event_index;
        char day_text[4];

        if(relative_day < 1) {
            day = previous_count + relative_day;
            year = previous_year;
            month = previous_month;
            in_month = false;
        }
        else if(relative_day > count) {
            day = relative_day - count;
            if(++month > 11) {
                month = 0;
                ++year;
            }
            in_month = false;
        }
        else
            day = relative_day;
        date = year * 10000 + (month + 1) * 100 + day;
        selected = in_month && day == calendar_focus_day;
        is_today = date == today;
        for(event_index = 0;
            event_index < crazypod_calendar_event_count();
            ++event_index) {
            const struct crazypod_calendar_event *event =
                crazypod_calendar_event_get(event_index);
            if(event != NULL && event->date == date) {
                has_event = true;
                break;
            }
        }
        if(selected)
            make_box(panel, x + 2, y - 1, 36, 17,
                     7, 0x0E0E0E, LV_OPA_COVER);
        else if(is_today) {
            lv_obj_t *today_box = make_box(
                panel, x + 2, y - 1, 36, 17,
                7, 0xFFFFFF, LV_OPA_TRANSP);
            lv_obj_set_style_border_width(today_box, 1, 0);
            lv_obj_set_style_border_color(
                today_box, lv_color_hex(0x0E0E0E), 0);
            lv_obj_set_style_border_opa(today_box, 210, 0);
        }
        snprintf(day_text, sizeof(day_text), "%d", day);
        label = make_label(
            panel, day_text, &lv_font_montserrat_10,
            selected ? COLOR_WHITE :
                in_month ? 0x0E0E0E : 0xB8B8B8,
            in_month ? LV_OPA_COVER : 155);
        lv_obj_set_width(label, 40);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, x, y);
        if(has_event)
            make_box(panel, x + 19, y + 13, 3, 3,
                     LV_RADIUS_CIRCLE,
                     selected ? COLOR_WHITE : 0x0E0E0E,
                     in_month ? 205 : 70);
    }
}

static void render_calendar_day_overlay(
    const struct route_state *state)
{
    lv_obj_t *overlay;
    lv_obj_t *label;
    char text[64];
    int count = calendar_route_event_count(state);
    int start = state->selected > 3
        ? state->selected - 3 : 0;
    int row;

    render_calendar_grid();
    overlay = make_box(product_content, 25, 49, 270, 172, 12,
                       0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(overlay, 1, 0);
    lv_obj_set_style_border_color(overlay,
                                  lv_color_hex(0x0E0E0E), 0);
    lv_obj_set_style_border_opa(overlay, 210, 0);
    label = make_label(overlay, "SCHEDULE",
                       &lv_font_montserrat_8,
                       0x949494, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 14, 9);
    snprintf(text, sizeof(text), "%04d-%02d-%02d",
             calendar_focus_year, calendar_focus_month + 1,
             calendar_focus_day);
    label = make_label(overlay, text,
                       &lv_font_montserrat_16,
                       0x0E0E0E, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 23);
    snprintf(text, sizeof(text), "%d item%s",
             count, count == 1 ? "" : "s");
    label = make_label(overlay, text, &lv_font_montserrat_8,
                       0x5C5C5C, 220);
    lv_obj_set_width(label, 72);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 184, 29);
    make_box(overlay, 12, 49, 246, 1, 0,
             0x0E0E0E, 205);

    for(row = 0; row < 4; ++row) {
        int position = start + row;
        const struct crazypod_calendar_event *event;
        int y = 57 + row * 27;
        bool selected;

        if(position > count)
            break;
        selected = position == state->selected;
        if(selected)
            make_box(overlay, 10, y - 2, 250, 25, 7,
                     0xF3F3F0, LV_OPA_COVER);
        if(position == count) {
            label = make_label(overlay, LV_SYMBOL_EDIT,
                               &lv_font_montserrat_10,
                               0x0E0E0E, 220);
            lv_obj_set_pos(label, 17, y + 4);
            label = make_label(overlay, "Add Event",
                               &lv_font_montserrat_10,
                               0x0E0E0E, LV_OPA_COVER);
            lv_obj_set_pos(label, 43, y + 3);
            continue;
        }
        event = crazypod_calendar_event_get(
            calendar_route_event_index(state, position));
        if(event == NULL)
            continue;
        label = make_label(
            overlay,
            event->time[0] != '\0' ? event->time : "All day",
            &lv_font_montserrat_8, 0x5C5C5C, 230);
        lv_obj_set_width(label, 44);
        lv_obj_set_pos(label, 17, y + 4);
        make_box(overlay, 64, y + 2, 2, 17, 1,
                 0x0E0E0E, 210);
        label = make_label(overlay, event->summary,
                           &lv_font_montserrat_10,
                           0x0E0E0E, LV_OPA_COVER);
        lv_obj_set_width(label, 180);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_pos(label, 75, y + 3);
    }
}

static void render_calendar_detail(const struct route_state *state)
{
    const struct crazypod_calendar_event *event =
        crazypod_calendar_event_get(state->group);
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[180];

    panel = make_box(product_content, 18, 54, 284, 145, 12,
                     COLOR_PANEL, 230);
    snprintf(text, sizeof(text), "%s\n\n%04d-%02d-%02d\n%s\n\n%s",
             event != NULL ? event->summary : "No Event",
             event != NULL ? event->date / 10000 : 0,
             event != NULL ? event->date / 100 % 100 : 0,
             event != NULL ? event->date % 100 : 0,
             event != NULL && event->time[0] != '\0'
                ? event->time : "All day",
             event != NULL && event->editable
                ? "Center: Event Actions"
                : "Imported from .ics");
    label = make_label(panel, text, CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, 235);
    lv_obj_set_pos(label, 14, 14);
    lv_obj_set_width(label, 256);
}

static long workout_elapsed_ticks(void)
{
    long elapsed = workout_accumulated_ticks;

    if(workout_running)
        elapsed += current_tick - workout_started_at;
    return elapsed > 0 ? elapsed : 0;
}

static void format_workout_duration(char *text, size_t size,
                                    uint32_t seconds)
{
    snprintf(text, size, "%02lu:%02lu:%02lu",
             (unsigned long)(seconds / 3600u),
             (unsigned long)(seconds / 60u % 60u),
             (unsigned long)(seconds % 60u));
}

static void render_workout_ready(void)
{
    lv_obj_t *panel;
    lv_obj_t *label;

    make_box(product_content, 0, 32, 320, 208, 0,
             0x050505, LV_OPA_COVER);
    panel = make_box(product_content, 18, 48, 284, 166, 18,
                     0x111512, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xA8F12D), 0);
    lv_obj_set_style_border_opa(panel, 100, 0);
    label = make_label(
        panel, crazypod_workout_activity_title(workout_activity),
        &lv_font_montserrat_16, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 248);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 18, 20);
    label = make_label(panel, "READY", &lv_font_montserrat_24,
                       0xA8F12D, LV_OPA_COVER);
    lv_obj_set_width(label, 248);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 18, 57);
    label = make_label(
        panel, "TIME ONLY\nNo motion, distance, or calorie estimates",
        &lv_font_montserrat_10, COLOR_WHITE, 145);
    lv_obj_set_width(label, 248);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 18, 96);
    label = make_label(panel, "CENTER  START",
                       &lv_font_montserrat_10,
                       0xA8F12D, 230);
    lv_obj_set_width(label, 248);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 18, 140);
}

static void render_workout_active(void)
{
    lv_obj_t *ring;
    lv_obj_t *label;
    char elapsed[24];
    uint32_t seconds = (uint32_t)(workout_elapsed_ticks() / HZ);

    make_box(product_content, 0, 32, 320, 208, 0,
             0x050505, LV_OPA_COVER);
    ring = make_box(product_content, 26, 49, 126, 126,
                    LV_RADIUS_CIRCLE, 0x0A0A0A, LV_OPA_COVER);
    lv_obj_set_style_border_width(ring, 5, 0);
    lv_obj_set_style_border_color(
        ring, lv_color_hex(workout_running ? 0xA8F12D : 0xFFB340), 0);
    lv_obj_set_style_border_opa(ring, 235, 0);
    label = make_label(ring, workout_running ? LV_SYMBOL_PLAY : "II",
                       &lv_font_montserrat_24,
                       workout_running ? 0xA8F12D : 0xFFB340,
                       LV_OPA_COVER);
    lv_obj_center(label);
    label = make_label(
        product_content,
        crazypod_workout_activity_title(workout_activity),
        &lv_font_montserrat_10, COLOR_WHITE, 165);
    lv_obj_set_pos(label, 174, 56);
    format_workout_duration(elapsed, sizeof(elapsed), seconds);
    label = make_label(product_content, elapsed,
                       &lv_font_montserrat_24,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 174, 76);
    make_box(product_content, 174, 108, 126, 1, 0,
             COLOR_WHITE, 90);
    label = make_label(product_content,
                       workout_running ? "RUNNING" : "PAUSED",
                       &lv_font_montserrat_10,
                       workout_running ? 0xA8F12D : 0xFFB340, 235);
    lv_obj_set_pos(label, 174, 119);
    label = make_label(
        product_content,
        "CENTER  PAUSE / RESUME\nPLAY  FINISH\nTIME-ONLY LOG",
        &lv_font_montserrat_8, COLOR_WHITE, 125);
    lv_obj_set_pos(label, 174, 145);
}

static void render_workout_summary(void)
{
    lv_obj_t *panel;
    lv_obj_t *label;
    uint32_t total_seconds = 0;
    char text[160];
    int i;

    for(i = 0; i < crazypod_workouts_count(); ++i) {
        const struct crazypod_workout *workout =
            crazypod_workout_get(i);
        if(workout != NULL)
            total_seconds += workout->duration_seconds;
    }
    panel = make_box(product_content, 18, 52, 284, 150, 14,
                     0x111512, 238);
    snprintf(text, sizeof(text),
             "WORKOUT SUMMARY\n\n%d saved workouts\n%lu total minutes\n\n"
             "Metrics: elapsed time only\nNo sensor data is fabricated.",
             crazypod_workouts_count(),
             (unsigned long)(total_seconds / 60u));
    label = make_label(panel, text, CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, 230);
    lv_obj_set_pos(label, 16, 14);
    lv_obj_set_width(label, 252);
}

static void render_workout_detail(const struct route_state *state)
{
    const struct crazypod_workout *workout =
        crazypod_workout_get(state->group);
    lv_obj_t *panel;
    lv_obj_t *label;
    char duration[24];
    char text[192];

    format_workout_duration(
        duration, sizeof(duration),
        workout != NULL ? workout->duration_seconds : 0);
    panel = make_box(product_content, 18, 52, 284, 150, 14,
                     0x111512, 238);
    snprintf(text, sizeof(text), "%s\n\n%04d-%02d-%02d\n%s\n\n"
             "Time-only workout\nCenter: Delete",
             workout != NULL
                 ? crazypod_workout_activity_title(workout->activity)
                 : "Missing Workout",
             workout != NULL ? (int)(workout->date / 10000) : 0,
             workout != NULL ? (int)(workout->date / 100 % 100) : 0,
             workout != NULL ? (int)(workout->date % 100) : 0,
             duration);
    label = make_label(panel, text, CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, 230);
    lv_obj_set_pos(label, 16, 14);
    lv_obj_set_width(label, 252);
}

static void render_contact_detail(const struct route_state *state)
{
    const struct crazypod_contact *contact =
        crazypod_contact_get(state->group);
    lv_obj_t *card;
    lv_obj_t *avatar;
    lv_obj_t *label;
    lv_obj_t *row;
    char initials[8];
    int initial_bytes = 0;

    initials[0] = '?';
    initials[1] = '\0';
    if(contact != NULL && contact->name[0] != '\0') {
        initial_bytes = utf8_character_size(contact->name);
        if(initial_bytes > 0 &&
           initial_bytes < (int)sizeof(initials)) {
            memcpy(initials, contact->name,
                   (size_t)initial_bytes);
            initials[initial_bytes] = '\0';
        }
    }

    make_box(product_content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
             0x000000, 105);
    card = make_box(product_content, 70, 42, 180, 184, 18,
                    0x242A31, 238);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(card, 42, 0);
    lv_obj_set_style_shadow_width(card, 18, 0);
    lv_obj_set_style_shadow_offset_y(card, 10, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, 95, 0);

    avatar = make_box(card, 63, 14, 54, 54,
                      LV_RADIUS_CIRCLE, 0x59B89E, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        avatar, lv_color_hex(0x2E4857), 0);
    lv_obj_set_style_bg_grad_dir(avatar, LV_GRAD_DIR_VER, 0);
    label = make_label(avatar, initials,
                       CRAZYPOD_METADATA_FONT,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_center(label);
    label = make_label(
        card,
        contact != NULL ? contact->name : "Missing Contact",
        CRAZYPOD_METADATA_FONT, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 156);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 12, 76);

    row = make_box(card, 12, 103, 156, 30, 8,
                   COLOR_WHITE, 28);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(row, 32, 0);
    label = make_label(row, LV_SYMBOL_CALL,
                       &lv_font_montserrat_10,
                       COLOR_WHITE, 175);
    lv_obj_set_pos(label, 10, 9);
    label = make_label(
        row,
        contact != NULL && contact->phone[0] != '\0'
            ? contact->phone : "No phone number",
        &lv_font_montserrat_10, COLOR_WHITE,
        contact != NULL && contact->phone[0] != '\0'
            ? 225 : 105);
    lv_obj_set_width(label, 118);
    lv_obj_set_height(label, 16);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 31, 8);

    row = make_box(card, 12, 140, 156, 30, 8,
                   COLOR_WHITE, 16);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(row, 22, 0);
    label = make_label(row, LV_SYMBOL_ENVELOPE,
                       &lv_font_montserrat_10,
                       COLOR_WHITE, 135);
    lv_obj_set_pos(label, 10, 9);
    label = make_label(
        row,
        contact != NULL && contact->email[0] != '\0'
            ? contact->email : "No email address",
        &lv_font_montserrat_10, COLOR_WHITE,
        contact != NULL && contact->email[0] != '\0'
            ? 190 : 90);
    lv_obj_set_width(label, 118);
    lv_obj_set_height(label, 16);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 31, 8);
}

static uint32_t miniapp_accent_foreground(uint32_t accent)
{
    uint32_t red = (accent >> 16) & 0xffu;
    uint32_t green = (accent >> 8) & 0xffu;
    uint32_t blue = accent & 0xffu;
    uint32_t luminance =
        2126u * red * red +
        7152u * green * green +
        722u * blue * blue;

    /*
     * A quadratic sRGB approximation keeps this integer-only. The threshold
     * selects the higher-contrast pure black or white foreground for every
     * supported appearance accent, including Rose, Emerald, and Blue.
     */
    return luminance >= 130050000u ? 0x000000 : 0xFFFFFF;
}

static uint32_t miniapp_color(enum cp_color_token token)
{
    uint32_t accent = highlight_primary();

    switch(token) {
    case CP_COLOR_BACKGROUND:
        return COLOR_DETAIL;
    case CP_COLOR_SURFACE:
        return COLOR_PANEL;
    case CP_COLOR_SURFACE_RAISED:
        return 0x292932;
    case CP_COLOR_WHITE:
        return COLOR_WHITE;
    case CP_COLOR_MUTED:
        return COLOR_MUTED;
    case CP_COLOR_ACCENT:
        return accent;
    case CP_COLOR_ACCENT_FOREGROUND:
        return miniapp_accent_foreground(accent);
    case CP_COLOR_ROSE:
        return 0xFF4568;
    case CP_COLOR_GREEN:
        return COLOR_GREEN;
    case CP_COLOR_CYAN:
        return COLOR_CYAN;
    case CP_COLOR_AMBER:
        return COLOR_AMBER;
    case CP_COLOR_ERROR:
        return 0xFF453A;
    case CP_COLOR_COUNT:
    default:
        return COLOR_WHITE;
    }
}

static const lv_font_t *miniapp_font(enum cp_font_token token)
{
    switch(token) {
    case CP_FONT_CAPTION:
        return &lv_font_montserrat_8;
    case CP_FONT_LABEL:
        return &crazypod_miniapp_symbol_font;
    case CP_FONT_BODY:
        return &lv_font_montserrat_12;
    case CP_FONT_CJK:
        return &lv_font_source_han_sans_sc_14_cjk;
    case CP_FONT_TITLE:
        return &lv_font_source_han_sans_sc_16_cjk;
    case CP_FONT_NUMBER:
        return &lv_font_montserrat_24;
    case CP_FONT_DISPLAY:
        return &lv_font_montserrat_48;
    case CP_FONT_COUNT:
    default:
        return &lv_font_montserrat_10;
    }
}

static void render_miniapp_rectangle(
    const struct cp_draw_command *command)
{
    bool focused = (command->flags & CP_DRAW_FOCUSED) != 0;
    int border_width = focused && command->border_width < 2
        ? 2 : command->border_width;
    int border_opacity = focused ? 255 : command->border_opacity;
    uint32_t border_color = focused
        ? 0xFFFFFF
        : miniapp_color((enum cp_color_token)command->border);
    int radius = (command->flags & CP_DRAW_CIRCLE) != 0
        ? LV_RADIUS_CIRCLE : command->radius;
    lv_obj_t *box = make_box(
        product_content, command->x, command->y,
        command->width, command->height, radius,
        miniapp_color((enum cp_color_token)command->background),
        command->opacity);

    if(border_width > 0 && border_opacity > 0) {
        lv_obj_set_style_border_width(box, border_width, 0);
        lv_obj_set_style_border_color(
            box, lv_color_hex(border_color), 0);
        lv_obj_set_style_border_opa(box, border_opacity, 0);
    }
    lv_obj_set_style_clip_corner(box, true, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
}

static void render_miniapp_text(const struct cp_draw_command *command)
{
    const lv_font_t *font =
        miniapp_font((enum cp_font_token)command->font);
    int text_y = command->y;
    int text_height = command->height;
    lv_obj_t *label;

    if(text_height > font->line_height) {
        text_y += (text_height - font->line_height) / 2;
        text_height = font->line_height;
    }
    label = make_label(
        product_content, command->text, font,
        miniapp_color((enum cp_color_token)command->foreground),
        command->opacity);
    lv_obj_set_pos(label, command->x, text_y);
    lv_obj_set_size(label, command->width, text_height);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(
        label,
        command->align == CP_ALIGN_RIGHT
            ? LV_TEXT_ALIGN_RIGHT
            : command->align == CP_ALIGN_CENTER
                ? LV_TEXT_ALIGN_CENTER
                : LV_TEXT_ALIGN_LEFT,
        0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
}

static void render_miniapp_ring(const struct cp_draw_command *command)
{
    int maximum = command->maximum > 0 ? command->maximum : 1;
    int value = command->value;
    lv_obj_t *ring;

    if(value < 0)
        value = 0;
    if(value > maximum)
        value = maximum;
    ring = lv_arc_create(product_content);
    lv_obj_remove_style_all(ring);
    lv_obj_set_pos(ring, command->x, command->y);
    lv_obj_set_size(ring, command->width, command->height);
    lv_arc_set_range(ring, 0, maximum);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_rotation(ring, 270);
    lv_arc_set_value(ring, value);
    lv_obj_set_style_arc_color(
        ring,
        lv_color_hex(miniapp_color(
            (enum cp_color_token)command->track_color)),
        LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ring, command->opacity, LV_PART_MAIN);
    lv_obj_set_style_arc_width(
        ring, command->track_width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(
        ring,
        lv_color_hex(miniapp_color(
            (enum cp_color_token)command->progress_color)),
        LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(
        ring, command->opacity, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(
        ring, command->progress_width, LV_PART_INDICATOR);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
}

static void render_miniapp_divider(
    const struct cp_draw_command *command)
{
    lv_obj_t *divider = make_box(
        product_content, command->x, command->y,
        command->width, command->height, command->radius,
        miniapp_color((enum cp_color_token)command->background),
        command->opacity);

    lv_obj_remove_flag(divider, LV_OBJ_FLAG_CLICKABLE);
}

static void render_miniapp_progress(
    const struct cp_draw_command *command)
{
    int maximum = command->maximum > 0 ? command->maximum : 1;
    int value = command->value;
    int fill_width;
    lv_obj_t *track;
    lv_obj_t *fill;

    if(value < 0)
        value = 0;
    if(value > maximum)
        value = maximum;
    fill_width =
        (int)((int64_t)value * command->width / maximum);
    track = make_box(
        product_content, command->x, command->y,
        command->width, command->height, command->radius,
        miniapp_color((enum cp_color_token)command->track_color),
        command->opacity);
    if(fill_width > 0) {
        fill = make_box(
            track, 0, 0, fill_width, command->height,
            command->radius,
            miniapp_color(
                (enum cp_color_token)command->progress_color),
            command->opacity);
        lv_obj_remove_flag(fill, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE);
}

static void render_miniapp_bitmap(
    const struct cp_draw_command *command)
{
    struct cp_resource_info info;
    lv_obj_t *image;
    int scale = LV_SCALE_NONE;
    int app_index = crazypod_miniapps_current();

    memset(&info, 0, sizeof(info));
    info.struct_size = sizeof(info);
    if(crazypod_miniapps_resource_stat(command->text, &info) !=
           CRAZYPOD_MINIAPP_OK ||
       info.type != CP_RESOURCE_BITMAP_RGB565 ||
       info.width == 0 || info.height == 0 ||
       info.width > 160 || info.height > 160 ||
       info.size > sizeof(miniapp_bitmap_pixels))
        return;
    if(app_index != miniapp_bitmap_app_index ||
       info.crc32 != miniapp_bitmap_crc ||
       info.width != miniapp_bitmap_width ||
       info.height != miniapp_bitmap_height ||
       strcmp(command->text, miniapp_bitmap_id) != 0) {
        if(crazypod_miniapps_resource_read(
               command->text, 0, miniapp_bitmap_pixels, info.size) !=
               (int)info.size ||
           !crazypod_image_configure_rgb565(
               &miniapp_bitmap_descriptor, miniapp_bitmap_pixels,
               info.width, info.height))
            return;
        snprintf(miniapp_bitmap_id, sizeof(miniapp_bitmap_id),
                 "%s", command->text);
        miniapp_bitmap_id[sizeof(miniapp_bitmap_id) - 1] = '\0';
        miniapp_bitmap_app_index = app_index;
        miniapp_bitmap_crc = info.crc32;
        miniapp_bitmap_width = info.width;
        miniapp_bitmap_height = info.height;
    }
    image = lv_image_create(product_content);
    lv_image_set_src(image, &miniapp_bitmap_descriptor);
    lv_obj_set_pos(image, command->x, command->y);
    if(command->width > 0 && command->height > 0) {
        int scale_x = command->width * LV_SCALE_NONE / info.width;
        int scale_y = command->height * LV_SCALE_NONE / info.height;
        scale = scale_x < scale_y ? scale_x : scale_y;
        if(scale < 1)
            scale = 1;
        lv_image_set_scale(image, scale);
    }
    lv_obj_set_style_opa(image, command->opacity, 0);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

static void render_miniapp_toast(void)
{
    char text[CP_MINIAPP_TOAST_TEXT_SIZE];
    lv_obj_t *panel;
    lv_obj_t *label;

    if(!crazypod_miniapps_toast(text, sizeof(text)))
        return;
    panel = make_box(
        product_content, 30, 198, 260, 30, 12,
        0x292932, 245);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(
        panel, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(panel, 45, 0);
    label = make_label(
        panel, text, &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 10, 7);
    lv_obj_set_size(label, 240, 16);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);
}

static void render_miniapp_view(void)
{
    int index;

    if(!crazypod_miniapps_render(&miniapp_scene)) {
        lv_obj_set_style_bg_color(
            product_content, lv_color_hex(COLOR_DETAIL), 0);
        make_box(product_content, 10, 40, 300, 188, 12,
                 COLOR_PANEL, LV_OPA_COVER);
        {
            lv_obj_t *label = make_label(
                product_content, "APP RENDER ERROR",
                &lv_font_montserrat_12, 0xFF453A, LV_OPA_COVER);
            lv_obj_set_pos(label, 30, 126);
            lv_obj_set_width(label, 260);
            lv_obj_set_style_text_align(
                label, LV_TEXT_ALIGN_CENTER, 0);
        }
        return;
    }
    lv_obj_set_style_bg_color(
        product_content,
        lv_color_hex(miniapp_color(
            (enum cp_color_token)miniapp_scene.background)),
        0);
    make_box(
        product_content, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
        miniapp_color((enum cp_color_token)miniapp_scene.background),
        LV_OPA_COVER);
    for(index = 0; index < miniapp_scene.command_count; ++index) {
        const struct cp_draw_command *command =
            &miniapp_scene.commands[index];

        if(command->type == CP_DRAW_RECT)
            render_miniapp_rectangle(command);
        else if(command->type == CP_DRAW_TEXT)
            render_miniapp_text(command);
        else if(command->type == CP_DRAW_RING)
            render_miniapp_ring(command);
        else if(command->type == CP_DRAW_DIVIDER)
            render_miniapp_divider(command);
        else if(command->type == CP_DRAW_PROGRESS)
            render_miniapp_progress(command);
        else if(command->type == CP_DRAW_BITMAP)
            render_miniapp_bitmap(command);
    }
    render_miniapp_toast();
}

static bool route_owns_fullscreen_surface(enum crazypod_route route)
{
    switch(route) {
    case CLOCK_ROUTE_VIEW:
    case STOPWATCH_ROUTE_VIEW:
    case WORKOUT_ROUTE_READY:
    case WORKOUT_ROUTE_ACTIVE:
    case WORKOUT_ROUTE_SUMMARY:
    case WORKOUT_ROUTE_DETAIL:
    case CALENDAR_ROUTE_MONTH:
    case CALENDAR_ROUTE_DAY_EVENTS:
    case CALENDAR_ROUTE_DETAIL:
    case CONTACTS_ROUTE_DETAIL:
        return true;
    default:
        return false;
    }
}

static uint32_t fullscreen_route_background(enum crazypod_route route)
{
    if(route == STOPWATCH_ROUTE_VIEW) {
        static const uint32_t backgrounds[] = {
            0xF9F9F7, 0xF2F2F2, 0xFFFFFF
        };
        return backgrounds[stopwatch_style_index % 3];
    }
    if(route == CLOCK_ROUTE_VIEW ||
       route == CALENDAR_ROUTE_MONTH ||
       route == CALENDAR_ROUTE_DAY_EVENTS)
        return 0xF9F9F7;
    if(route == WORKOUT_ROUTE_READY ||
       route == WORKOUT_ROUTE_ACTIVE ||
       route == WORKOUT_ROUTE_SUMMARY ||
       route == WORKOUT_ROUTE_DETAIL)
        return 0x050505;
    return COLOR_DETAIL;
}

static bool fullscreen_route_uses_dark_status_bar(
    enum crazypod_route route)
{
    return route == CLOCK_ROUTE_VIEW ||
           route == STOPWATCH_ROUTE_VIEW ||
           route == CALENDAR_ROUTE_MONTH ||
           route == CALENDAR_ROUTE_DAY_EVENTS;
}

static void render_current_route(bool transition)
{
    struct route_state *state = current_route();
    const lv_image_dsc_t *menu_wallpaper;
    bool book_reader = state->route == BOOKS_ROUTE_READER;
    bool fullscreen_surface =
        route_owns_fullscreen_surface(state->route);
    bool solid_black =
        state->route == DIY_ROUTE_WALLPAPER_CROP ||
        state->route == MUSIC_ROUTE_ALBUM_FLOW ||
        state->route == MINIAPP_ROUTE_VIEW;
    int book_theme = crazypod_books_theme();
    uint32_t route_background = book_reader
        ? book_page_colors[book_theme]
        : solid_black ? 0x000000
        : fullscreen_surface
            ? fullscreen_route_background(state->route)
            : crazypod_appearance_menu_color();
    uint32_t status_foreground = book_reader
        ? book_ink_colors[book_theme]
        : fullscreen_surface &&
          fullscreen_route_uses_dark_status_bar(state->route)
            ? 0x0E0E0E : COLOR_WHITE;
    int i;

    if(state->route == PHOTOS_ROUTE_DETAIL)
        photo_pan_render_pending = false;
    if(crazypod_coverflow_active())
        crazypod_coverflow_leave();
    now_overlay = NOW_OVERLAY_NONE;
    clear_now_overlay_objects();
    clear_choice_overlay_objects();
    memset(&menu_view, 0, sizeof(menu_view));
    menu_preview_root = NULL;
    menu_preview_content = NULL;
    memset(&menu_preview_scene, 0, sizeof(menu_preview_scene));
    menu_preview_pending = false;
    menu_preview_motion_ready = false;
    menu_preview_build_defer_media = false;
    menu_preview_media_deferred = false;
    menu_preview_media_refresh_pending = false;
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
    route_render_pending = false;
    now_progress_marker = NULL;
    now_elapsed = NULL;
    now_remaining = NULL;
    now_wave_surface = NULL;
    now_lyrics_previous = NULL;
    now_lyrics_current = NULL;
    now_lyrics_next = NULL;
    photo_favorite_progress_fill = NULL;
    wallpaper_crop_progress_fill = NULL;
    wallpaper_crop_progress_label = NULL;
    music_loading_title = NULL;
    music_loading_detail = NULL;
    book_loading_progress_fill = NULL;
    book_loading_progress_label = NULL;
    book_loading_percent_label = NULL;
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
        lv_color_hex(route_background), 0);
    lv_obj_set_style_bg_opa(product_content, LV_OPA_COVER, 0);
    make_box(product_content, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
             route_background, LV_OPA_COVER);
    menu_wallpaper = solid_black || book_reader || fullscreen_surface
        ? NULL : crazypod_custom_menu_wallpaper();
    if(menu_wallpaper != NULL) {
        lv_obj_t *image = lv_image_create(product_content);
        lv_image_set_src(image, menu_wallpaper);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }

    if(state->route == MINIAPP_ROUTE_VIEW)
        render_miniapp_view();
    else if(state->route == MUSIC_ROUTE_NOW_PLAYING)
        render_now_playing();
    else if(state->route == SETTINGS_ROUTE_EQ_STUDIO)
        render_eq_studio();
    else if(state->route == MUSIC_ROUTE_ALBUM_FLOW)
        render_album_flow(state);
    else if(state->route == MUSIC_ROUTE_SEARCH)
        render_search_screen(state);
    else if(state->route == PHOTOS_ROUTE_LIBRARY ||
            state->route == PHOTOS_ROUTE_FAVORITES ||
            state->route == DIY_ROUTE_WALLPAPER_FILES)
        render_photo_grid(state);
    else if(state->route == PHOTOS_ROUTE_DETAIL)
        render_photo_detail(state);
    else if(state->route == NOTES_ROUTE_COMPOSER)
        render_note_composer(state);
    else if(state->route == NOTES_ROUTE_READER)
        render_note_reader(state);
    else if(state->route == BOOKS_ROUTE_READER)
        render_book_reader(state);
    else if(state->route == BOOKS_ROUTE_STATS)
        render_books_stats(state);
    else if(state->route == BOOKS_ROUTE_INFO)
        render_book_info(state);
    else if(state->route == CLOCK_ROUTE_VIEW)
        render_clock_view();
    else if(state->route == STOPWATCH_ROUTE_VIEW)
        render_stopwatch_view();
    else if(state->route == WORKOUT_ROUTE_READY)
        render_workout_ready();
    else if(state->route == WORKOUT_ROUTE_ACTIVE)
        render_workout_active();
    else if(state->route == WORKOUT_ROUTE_SUMMARY)
        render_workout_summary();
    else if(state->route == WORKOUT_ROUTE_DETAIL)
        render_workout_detail(state);
    else if(state->route == CALENDAR_ROUTE_MONTH)
        render_calendar_grid();
    else if(state->route == CALENDAR_ROUTE_DAY_EVENTS)
        render_calendar_day_overlay(state);
    else if(state->route == CALENDAR_ROUTE_DETAIL)
        render_calendar_detail(state);
    else if(state->route == CONTACTS_ROUTE_DETAIL)
        render_contact_detail(state);
    else if(state->route == DIY_ROUTE_WALLPAPER_CROP)
        render_wallpaper_crop();
    else
        render_menu_screen(state);

    lv_obj_invalidate(product_content);
    if(transition)
        animate_content_entrance();
    set_status_bar_palette(
        &status_bars[1],
        status_foreground, route_background);
    lv_obj_move_foreground(status_bars[1].time);
    lv_obj_move_foreground(status_bars[1].playing);
    refresh_screen_corner_masks();
}

static void render_loading(void)
{
    lv_obj_t *symbol;
    char detail[64];

    lv_obj_clean(product_content);
    /*
     * The loading screen owns the whole product surface. Artwork generation
     * changes while the cover cache is being primed, so no stale menu preview
     * or deferred menu render may survive and draw over it.
     */
    memset(&menu_view, 0, sizeof(menu_view));
    menu_preview_root = NULL;
    menu_preview_content = NULL;
    memset(&menu_preview_scene, 0, sizeof(menu_preview_scene));
    menu_preview_pending = false;
    menu_preview_motion_ready = false;
    menu_preview_build_defer_media = false;
    menu_preview_media_deferred = false;
    menu_preview_media_refresh_pending = false;
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
    route_render_pending = false;
    set_status_bar_palette(
        &status_bars[1], COLOR_WHITE, COLOR_DETAIL);
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

static void open_photos(void)
{
    product_active = true;
    lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(product_screen);
    set_cpu_boost(true);
    photo_pan_x = 0;
    photo_pan_y = 0;
    photo_zoom_percent = 100;
    photo_select_long_handled = true;
    photo_select_holding = false;
    photo_select_hold_percent = -1;
    photo_favorite_feedback_until = 0;
    photo_favorite_feedback_error = false;
    photo_favorite_progress_fill = NULL;
    photo_wheel_touch_active = false;
    photo_wheel_touch_start = -1;
    photo_wheel_touch_max_delta = 0;
    photo_direction_input_tick = 0;
    photo_pan_render_pending = false;
    route_depth = 1;
    route_stack[0].route = PHOTOS_ROUTE_MENU;
    route_stack[0].selected = 0;
    route_stack[0].group = -1;
    render_current_route(true);
}

static void open_settings(void)
{
    product_active = true;
    lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(product_screen);
    set_cpu_boost(true);

    route_depth = 1;
    route_stack[0].route = SETTINGS_ROUTE_MENU;
    route_stack[0].selected = 0;
    route_stack[0].group = -1;
    render_current_route(true);
}

static void open_extras(void)
{
    product_active = true;
    lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(product_screen);
    set_cpu_boost(true);

    route_depth = 1;
    route_stack[0].route = EXTRAS_ROUTE_MENU;
    route_stack[0].selected = 0;
    route_stack[0].group = -1;
    render_current_route(true);
}

static void open_notes(void)
{
    product_active = true;
    lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(product_screen);
    set_cpu_boost(true);

    note_draft_available = crazypod_note_draft_load(&note_editor);
    route_depth = 1;
    route_stack[0].route = NOTES_ROUTE_MENU;
    route_stack[0].selected = 0;
    route_stack[0].group = -1;
    render_current_route(true);
}

static void open_books(void)
{
    product_active = true;
    lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(product_screen);
    set_cpu_boost(true);

    if(!books_metadata_ready) {
        render_book_loading_screen(
            NULL, "Loading Library",
            "Reading final book titles and covers");
        book_prepare_progress_callback(
            6, "Scanning Books folder", NULL);
        crazypod_books_scan();
        load_book_metadata_with_progress(16, 96);
        books_metadata_ready = true;
        book_prepare_progress_callback(
            100, "Library ready", NULL);
    }
    route_depth = 1;
    route_stack[0].route = BOOKS_ROUTE_MENU;
    route_stack[0].selected = 0;
    route_stack[0].group = -1;
    render_current_route(true);
}

static void begin_note_composer(uint32_t id, bool resume_draft)
{
    const struct crazypod_note *note;

    memset(&note_editor, 0, sizeof(note_editor));
    if(resume_draft) {
        if(!crazypod_note_draft_load(&note_editor))
            memset(&note_editor, 0, sizeof(note_editor));
    }
    else if(id != 0) {
        note = crazypod_note_find(id);
        if(note != NULL) {
            note_editor.source_id = id;
            snprintf(note_editor.title, sizeof(note_editor.title),
                     "%s", note->title);
            crazypod_note_read_body(id, note_editor.body,
                                    sizeof(note_editor.body));
        }
    }
    note_editor_baseline = note_editor;
    note_editor_title_cursor = strlen(note_editor.title);
    note_editor_body_cursor = strlen(note_editor.body);
    note_editor_body_active = false;
    push_route_selected(NOTES_ROUTE_COMPOSER, -1, 0);
}

static bool note_editor_dirty(void)
{
    return note_editor.source_id != note_editor_baseline.source_id ||
           strcmp(note_editor.title, note_editor_baseline.title) != 0 ||
           strcmp(note_editor.body, note_editor_baseline.body) != 0;
}

static void open_note_reader(uint32_t id)
{
    note_reader_body[0] = '\0';
    crazypod_note_read_body(id, note_reader_body,
                            sizeof(note_reader_body));
    push_route_selected(NOTES_ROUTE_READER, (int)id, 0);
}

static void render_book_loading_screen(
    const struct crazypod_book *book, const char *title,
    const char *detail)
{
    int theme = crazypod_books_theme();
    uint32_t page_color = book_page_colors[theme];
    uint32_t ink_color = book_ink_colors[theme];
    lv_obj_t *label;
    lv_obj_t *track;

    lv_obj_clean(product_content);
    lv_obj_set_style_bg_color(
        product_content, lv_color_hex(page_color), 0);
    lv_obj_set_style_bg_opa(product_content, LV_OPA_COVER, 0);
    make_box(product_content, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
             page_color, LV_OPA_COVER);
    set_status_bar_palette(&status_bars[1], ink_color, page_color);

    label = make_label(
        product_content,
        title != NULL ? title : "Preparing Book",
        &lv_font_montserrat_16, ink_color, LV_OPA_COVER);
    lv_obj_set_width(label, 280);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 20, 57);

    label = make_label(
        product_content,
        book != NULL && book->title[0] != '\0'
            ? book->title : "Reading local book data",
        CRAZYPOD_METADATA_FONT, ink_color, 180);
    lv_obj_set_width(label, 260);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 30, 88);

    track = make_box(
        product_content, 40, 126, 240, 7,
        LV_RADIUS_CIRCLE, ink_color, 32);
    book_loading_progress_fill = make_box(
        track, 0, 0, 2, 7,
        LV_RADIUS_CIRCLE, ink_color, 220);

    book_loading_progress_label = make_label(
        product_content, "Starting",
        &lv_font_montserrat_10, ink_color, 190);
    lv_obj_set_width(book_loading_progress_label, 220);
    lv_label_set_long_mode(
        book_loading_progress_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(book_loading_progress_label, 40, 145);

    book_loading_percent_label = make_label(
        product_content, "0%",
        &lv_font_montserrat_10, ink_color, 190);
    lv_obj_set_width(book_loading_percent_label, 42);
    lv_obj_set_style_text_align(
        book_loading_percent_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(book_loading_percent_label, 238, 145);

    label = make_label(
        product_content,
        detail != NULL ? detail : "",
        &lv_font_montserrat_8, ink_color, 105);
    lv_obj_set_width(label, 280);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 20, 181);

    lv_obj_move_foreground(status_bars[1].time);
    lv_obj_move_foreground(status_bars[1].playing);
    lv_refr_now(NULL);
    crazypod_present_now();
}

static void book_prepare_progress_callback(
    int percent, const char *stage, void *context)
{
    char value[16];
    int width;

    (void)context;
    if(book_loading_progress_fill == NULL ||
       book_loading_progress_label == NULL ||
       book_loading_percent_label == NULL)
        return;
    if(percent < 0)
        percent = 0;
    if(percent > 100)
        percent = 100;
    width = percent * 240 / 100;
    if(width < 2)
        width = 2;
    lv_obj_set_width(book_loading_progress_fill, width);
    lv_label_set_text(
        book_loading_progress_label,
        stage != NULL ? stage : "Preparing");
    snprintf(value, sizeof(value), "%d%%", percent);
    lv_label_set_text(book_loading_percent_label, value);
    lv_refr_now(NULL);
    crazypod_present_now();
}

static void load_book_metadata_with_progress(
    int start_percent, int end_percent)
{
    int count = crazypod_books_count();
    int span = end_percent - start_percent;
    int i;

    if(span < 0)
        span = 0;
    if(count <= 0) {
        book_prepare_progress_callback(
            end_percent, "Library is empty", NULL);
        return;
    }
    for(i = 0; i < count; ++i) {
        book_prepare_progress_callback(
            start_percent + span * i / count,
            "Reading book titles and covers", NULL);
        crazypod_book_probe(i);
    }
    book_prepare_progress_callback(
        end_percent, "Finalizing library", NULL);
}

static void apply_book_font_size(int value)
{
    const struct crazypod_book *book =
        crazypod_book_get(selected_book_index);
    bool needs_reflow =
        book != NULL && book_page_text[0] != '\0';

    if(value == crazypod_books_font_size()) {
        render_current_route(false);
        return;
    }
    if(needs_reflow) {
        render_book_loading_screen(
            book, "Reflowing Text",
            "Keeping your current reading position");
        book_prepare_progress_callback(
            20, "Applying text size", NULL);
    }
    crazypod_books_set_font_size(value);
    if(needs_reflow) {
        bool loaded;

        book_prepare_progress_callback(
            62, "Rebuilding current page", NULL);
        book_page_history_count = 0;
        loaded = load_book_page(
            selected_book_index, book_page_offset);
        book_prepare_progress_callback(
            100, loaded ? "Text ready" : "Could not reload page", NULL);
    }
    render_current_route(false);
}

static void rescan_books_with_progress(void)
{
    render_book_loading_screen(
        NULL, "Scanning Books",
        "Refreshing imported books and cover data");
    books_metadata_ready = false;
    book_prepare_progress_callback(
        12, "Resetting cover cache", NULL);
    crazypod_book_cover_reset();
    book_prepare_progress_callback(
        20, "Reading Books folders", NULL);
    crazypod_books_scan();
    load_book_metadata_with_progress(30, 94);
    books_metadata_ready = true;
    selected_book_index = -1;
    book_page_text[0] = '\0';
    book_page_history_count = 0;
    book_prepare_progress_callback(
        100, "Library ready", NULL);
    render_current_route(false);
}

static void begin_book_reader(int index, uint32_t offset)
{
    const struct crazypod_book *book;
    bool show_progress;
    bool ready;
    bool loaded = false;

    book = crazypod_book_get(index);
    if(book == NULL)
        return;
    show_progress =
        book->format == CRAZYPOD_BOOK_EPUB &&
        book->content_size == 0;
    if(show_progress)
        render_book_loading_screen(
            book, "Preparing Book",
            "First open creates a local reading cache");
    ready = show_progress
        ? crazypod_book_prepare_with_progress(
              index, book_prepare_progress_callback, NULL)
        : crazypod_book_prepare(index);
    book = crazypod_book_get(index);
    if(book == NULL)
        return;
    if(book->content_size > 0 && offset >= book->content_size)
        offset = 0;
    selected_book_index = index;
    book_page_history_count = 0;
    if(show_progress)
        book_prepare_progress_callback(
            94,
            ready ? "Loading first page"
                  : "Preparing error details",
            NULL);
    if(ready)
        loaded = load_book_page(index, offset);
    if(!loaded) {
        snprintf(
            book_page_text, sizeof(book_page_text),
            book->format == CRAZYPOD_BOOK_EPUB
                ? "This EPUB is damaged, encrypted, or unsupported."
                : "Unable to open this book.");
    }
    else {
        if(show_progress)
            book_prepare_progress_callback(
                98, "Saving reading position", NULL);
        crazypod_book_set_progress(index, offset);
    }
    if(show_progress)
        book_prepare_progress_callback(
            100,
            loaded ? "Opening reader"
                   : "Showing book error",
            NULL);
    push_route_selected(BOOKS_ROUTE_READER, index, 0);
}

static void save_note_editor_draft(void)
{
    if(note_editor.title[0] == '\0' &&
       note_editor.body[0] == '\0') {
        crazypod_note_draft_clear();
        note_draft_available = false;
        note_draft_save_pending = false;
        return;
    }
    note_draft_available = crazypod_note_draft_save(&note_editor);
    note_draft_save_pending = false;
}

static void schedule_note_editor_draft(void)
{
    note_draft_save_pending = true;
    note_draft_save_due = current_tick + HZ * 2;
}

static void service_note_editor_draft(void)
{
    if(note_draft_save_pending &&
       !TIME_BEFORE(current_tick, note_draft_save_due))
        save_note_editor_draft();
}

static void commit_note_editor(void)
{
    uint32_t id = crazypod_note_save(
        note_editor.source_id, note_editor.title, note_editor.body);

    if(id == 0) {
        save_note_editor_draft();
        render_current_route(false);
        return;
    }
    crazypod_note_draft_clear();
    note_draft_available = false;
    note_draft_save_pending = false;
    memset(&note_editor, 0, sizeof(note_editor));
    route_depth = 1;
    route_stack[0].route = NOTES_ROUTE_MENU;
    route_stack[0].selected = 0;
    route_stack[0].group = -1;
    open_note_reader(id);
}

static void open_root_route(enum crazypod_route route)
{
    product_active = true;
    lv_obj_remove_flag(product_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(product_screen);
    set_cpu_boost(true);
    route_depth = 1;
    route_stack[0].route = route;
    route_stack[0].selected = 0;
    route_stack[0].group = -1;
    render_current_route(true);
}

static void open_podcasts(void)
{
    open_root_route(PODCASTS_ROUTE_MENU);
    if(!music_library_loaded)
        begin_music_scan();
}

static void open_utilities(void)
{
    open_root_route(UTILITIES_ROUTE_MENU);
}

static void open_calendar(void)
{
    struct tm *now;

    crazypod_organizer_scan();
    now = get_time();
    calendar_focus_year = now->tm_year + 1900;
    calendar_focus_month = now->tm_mon;
    calendar_focus_day = now->tm_mday;
    open_root_route(CALENDAR_ROUTE_MENU);
}

static void open_contacts(void)
{
    crazypod_organizer_scan();
    open_root_route(CONTACTS_ROUTE_LIST);
}

static void open_app(enum crazypod_app_id id)
{
    switch(id) {
    case CRAZYPOD_APP_MUSIC:
        open_music();
        break;
    case CRAZYPOD_APP_SHUFFLE:
        open_music();
        if(crazypod_music_track_count() > 0) {
            crazypod_queue_set_shuffle(true);
            crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0);
            crazypod_state_forget_resume();
            crazypod_state_mark_dirty();
            request_now_playing_route();
        }
        break;
    case CRAZYPOD_APP_LOCK:
        show_lock_screen(true);
        break;
    case CRAZYPOD_APP_PHOTOS:
        open_photos();
        break;
    case CRAZYPOD_APP_CUSTOMIZE:
        open_diy();
        break;
    case CRAZYPOD_APP_WORKOUTS:
        open_root_route(WORKOUT_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_EXTRAS:
        open_extras();
        break;
    case CRAZYPOD_APP_NOTES:
        open_notes();
        break;
    case CRAZYPOD_APP_BOOKS:
        open_books();
        break;
    case CRAZYPOD_APP_PODCASTS:
        open_podcasts();
        break;
    case CRAZYPOD_APP_MINI_APPS:
        open_utilities();
        break;
    case CRAZYPOD_APP_CLOCK:
        open_root_route(CLOCK_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_CONTACTS:
        open_contacts();
        break;
    case CRAZYPOD_APP_CALENDAR:
        open_calendar();
        break;
    case CRAZYPOD_APP_STOPWATCH:
        open_root_route(STOPWATCH_ROUTE_VIEW);
        break;
    case CRAZYPOD_APP_SETTINGS:
        open_settings();
        break;
    default:
        break;
    }
}

static void persist_main_menu_change(enum crazypod_app_id preferred)
{
    int next = crazypod_apps_visible_index(preferred);

    if(next < 0)
        next = selected_app;
    if(next >= crazypod_apps_visible_count())
        next = crazypod_apps_visible_count() - 1;
    if(next < 0)
        next = 0;
    selected_app = next;
    crazypod_state_mark_dirty();
    crazypod_state_save(true);
    layout_desktop_carousel(false);
}

static void close_product(void)
{
    if(!product_active)
        return;
    if(crazypod_miniapps_is_open()) {
        crazypod_miniapp_input_reset(&miniapp_input_queue);
        crazypod_miniapps_close();
    }
    dismiss_choice_overlay(false);
    dismiss_now_overlay(false);
    now_playing_open_pending = false;
    pending_now_playing_track_path[0] = '\0';
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
    if(is_music_preview_route(route))
        prefetch_music_preview_artwork(current_route());
    render_current_route(true);
}

static void push_route(enum crazypod_route route, int group)
{
    push_route_selected(route, group, 0);
}

static void request_now_playing_route(void)
{
    const struct crazypod_track *track = current_track();
    enum crazypod_artwork_state state;
    int artwork_slot;

    now_playing_open_pending = false;
    pending_now_playing_track_path[0] = '\0';
    if(track == NULL) {
        push_route(MUSIC_ROUTE_NOW_PLAYING, -1);
        return;
    }

    artwork_slot = now_playing_artwork_slot_for_track(track);
    (void)crazypod_artwork_load_priority(
        artwork_slot, track,
        CRAZYPOD_NOW_ARTWORK_CACHE_SIZE, 0);
    state = crazypod_artwork_state(
        artwork_slot, track,
        CRAZYPOD_NOW_ARTWORK_CACHE_SIZE);
    if(state != CRAZYPOD_ARTWORK_PENDING) {
        now_artwork_generation_seen =
            crazypod_artwork_slot_generation(
                CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);
        now_prefetch_artwork_generation_seen =
            crazypod_artwork_slot_generation(
                CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT);
        push_route(MUSIC_ROUTE_NOW_PLAYING, -1);
        return;
    }

    now_playing_open_pending = true;
    pending_now_playing_route_depth = route_depth;
    pending_now_playing_route = current_route()->route;
    snprintf(pending_now_playing_track_path,
             sizeof(pending_now_playing_track_path),
             "%s", track->path);
    keep_cpu_boosted(HZ / 2);
}

static void process_pending_now_playing_open(void)
{
    const struct crazypod_track *track;
    enum crazypod_artwork_state state;
    int artwork_slot;

    if(!now_playing_open_pending)
        return;
    if(!product_active || route_depth <= 0 ||
       route_depth != pending_now_playing_route_depth ||
       current_route()->route != pending_now_playing_route) {
        now_playing_open_pending = false;
        pending_now_playing_track_path[0] = '\0';
        return;
    }

    track = current_track();
    if(track == NULL) {
        now_playing_open_pending = false;
        pending_now_playing_track_path[0] = '\0';
        push_route(MUSIC_ROUTE_NOW_PLAYING, -1);
        return;
    }
    if(strcmp(pending_now_playing_track_path, track->path) != 0) {
        snprintf(pending_now_playing_track_path,
                 sizeof(pending_now_playing_track_path),
                 "%s", track->path);
    }
    artwork_slot = now_playing_artwork_slot_for_track(track);
    (void)crazypod_artwork_load_priority(
        artwork_slot, track,
        CRAZYPOD_NOW_ARTWORK_CACHE_SIZE, 0);
    state = crazypod_artwork_state(
        artwork_slot, track,
        CRAZYPOD_NOW_ARTWORK_CACHE_SIZE);
    if(state == CRAZYPOD_ARTWORK_PENDING)
        return;

    now_playing_open_pending = false;
    pending_now_playing_track_path[0] = '\0';
    now_artwork_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);
    now_prefetch_artwork_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT);
    push_route(MUSIC_ROUTE_NOW_PLAYING, -1);
}

static void pop_route(void)
{
    if(route_depth > 0 &&
       current_route()->route == MINIAPP_ROUTE_VIEW &&
       crazypod_miniapps_is_open()) {
        crazypod_miniapp_input_reset(&miniapp_input_queue);
        crazypod_miniapps_close();
    }
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
        request_now_playing_route();
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

    if(length == 0)
        return;
    --length;
    while(length > 0 &&
          ((unsigned char)buffer[length] & 0xc0) == 0x80)
        --length;
    buffer[length] = '\0';
}

static void editor_insert_at(char *buffer, size_t size,
                             size_t *cursor, const char *text)
{
    size_t length = strlen(buffer);
    size_t insert_length = text != NULL ? strlen(text) : 0;

    if(cursor == NULL || insert_length == 0 || length >= size - 1)
        return;
    if(*cursor > length)
        *cursor = length;
    if(insert_length > size - length - 1)
        insert_length = size - length - 1;
    memmove(buffer + *cursor + insert_length,
            buffer + *cursor, length - *cursor + 1);
    memcpy(buffer + *cursor, text, insert_length);
    *cursor += insert_length;
}

static void editor_backspace_at(char *buffer, size_t *cursor)
{
    size_t start;
    size_t length;

    if(cursor == NULL || *cursor == 0)
        return;
    length = strlen(buffer);
    if(*cursor > length)
        *cursor = length;
    start = *cursor - 1;
    while(start > 0 &&
          ((unsigned char)buffer[start] & 0xc0) == 0x80)
        --start;
    memmove(buffer + start, buffer + *cursor,
            length - *cursor + 1);
    *cursor = start;
}

static void editor_move_cursor(const char *buffer, size_t *cursor,
                               int direction)
{
    size_t length;

    if(buffer == NULL || cursor == NULL || direction == 0)
        return;
    length = strlen(buffer);
    if(*cursor > length)
        *cursor = length;
    if(direction < 0) {
        if(*cursor == 0)
            return;
        --*cursor;
        while(*cursor > 0 &&
              ((unsigned char)buffer[*cursor] & 0xc0) == 0x80)
            --*cursor;
    }
    else {
        if(*cursor >= length)
            return;
        ++*cursor;
        while(*cursor < length &&
              ((unsigned char)buffer[*cursor] & 0xc0) == 0x80)
            ++*cursor;
    }
}

static void begin_calendar_editor(uint32_t id, int date)
{
    const struct crazypod_calendar_event *event =
        crazypod_calendar_event_find(id);

    calendar_editor_id = event != NULL ? event->id : 0;
    calendar_editor_date = event != NULL
        ? event->date : (date > 0 ? date : calendar_today_date());
    calendar_editor_minutes = event != NULL
        ? calendar_parse_minutes(event->time) : -1;
    snprintf(calendar_editor_summary,
             sizeof(calendar_editor_summary), "%s",
             event != NULL ? event->summary : "");
    calendar_editor_cursor = strlen(calendar_editor_summary);
    calendar_editor_error = 0;
    push_route(CALENDAR_ROUTE_EDITOR, -1);
}

static void show_calendar_day(int date)
{
    int calendar_root = -1;
    int i;

    calendar_focus_year = date / 10000;
    calendar_focus_month = date / 100 % 100 - 1;
    calendar_focus_day = date % 100;
    for(i = route_depth - 1; i >= 0; --i) {
        if(route_stack[i].route == CALENDAR_ROUTE_MENU) {
            calendar_root = i;
            break;
        }
    }
    if(calendar_root < 0) {
        calendar_root = 0;
        route_stack[0].route = CALENDAR_ROUTE_MENU;
        route_stack[0].selected = 0;
        route_stack[0].group = -1;
    }
    route_depth = calendar_root + 2;
    route_stack[calendar_root + 1].route =
        CALENDAR_ROUTE_DAY_EVENTS;
    route_stack[calendar_root + 1].selected = 0;
    route_stack[calendar_root + 1].group = -1;
}

static bool commit_calendar_editor(void)
{
    char time[16];
    uint32_t id;

    if(calendar_editor_summary[0] == '\0') {
        calendar_editor_error = 1;
        render_current_route(false);
        return false;
    }
    calendar_format_time(time, sizeof(time),
                         calendar_editor_minutes);
    if(calendar_editor_id != 0) {
        if(!crazypod_calendar_event_update(
               calendar_editor_id, calendar_editor_date,
               time, calendar_editor_summary)) {
            calendar_editor_error = 2;
            render_current_route(false);
            return false;
        }
        id = calendar_editor_id;
    }
    else {
        id = crazypod_calendar_event_add(
            calendar_editor_date, time,
            calendar_editor_summary);
        if(id == 0) {
            calendar_editor_error = 2;
            render_current_route(false);
            return false;
        }
    }
    calendar_editor_id = id;
    show_calendar_day(calendar_editor_date);
    render_current_route(true);
    return true;
}

static void activate_selected(void)
{
    struct route_state *state = current_route();

    if(state->route == MUSIC_ROUTE_NOW_PLAYING &&
       now_overlay != NOW_OVERLAY_NONE) {
        activate_now_overlay();
        return;
    }
    if(choice_overlay.kind != CHOICE_OVERLAY_NONE) {
        activate_choice_overlay();
        return;
    }
    switch(state->route) {
    case MUSIC_ROUTE_MENU:
        switch(state->selected) {
        case 0: request_now_playing_route(); break;
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
        else if(search_query[0] != '\0' &&
                crazypod_music_search_count(search_query) > 0) {
            push_route(MUSIC_ROUTE_SEARCH_RESULTS, -1);
            break;
        }
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
    case PHOTOS_ROUTE_MENU:
        if(state->selected == 0)
            push_route(PHOTOS_ROUTE_LIBRARY, -1);
        else if(state->selected == 1)
            push_route(PHOTOS_ROUTE_VIDEOS, -1);
        else
            push_route(PHOTOS_ROUTE_FAVORITES, -1);
        break;
    case PHOTOS_ROUTE_VIDEOS:
        if(state->selected >= 0 &&
           state->selected < crazypod_video_count()) {
            (void)crazypod_video_play(state->selected);
            video_generation_seen = crazypod_video_generation();
            render_current_route(false);
        }
        break;
    case PHOTOS_ROUTE_LIBRARY:
    case PHOTOS_ROUTE_FAVORITES: {
        int photo_index =
            photo_route_index(state, state->selected);

        if(photo_index >= 0) {
            photo_pan_x = 0;
            photo_pan_y = 0;
            photo_zoom_percent = 100;
            push_route_selected(PHOTOS_ROUTE_DETAIL,
                                photo_index, 0);
        }
        break;
    }
    case PHOTOS_ROUTE_DETAIL:
        state->selected = state->selected == 0 ? 1 : 0;
        photo_zoom_percent = state->selected > 0 ? 220 : 100;
        photo_pan_x = 0;
        photo_pan_y = 0;
        render_current_route(false);
        break;
    case EXTRAS_ROUTE_MENU: {
        struct crazypod_app *app = hidden_app(state->selected);
        if(app != NULL)
            open_app(app->id);
        break;
    }
    case NOTES_ROUTE_MENU:
        if(state->selected == 0)
            begin_note_composer(0, false);
        else if(note_draft_available && state->selected == 1)
            begin_note_composer(0, true);
        else if(state->selected == notes_home_search_index()) {
            note_search_query[0] = '\0';
            push_route(NOTES_ROUTE_SEARCH, -1);
        }
        else if(state->selected == notes_home_deleted_index())
            push_route(NOTES_ROUTE_DELETED, -1);
        else {
            const struct crazypod_note *note =
                notes_home_note(state->selected);
            if(note != NULL)
                open_note_reader(note->id);
        }
        break;
    case NOTES_ROUTE_COMPOSER: {
        char *target = note_editor_body_active
            ? note_editor.body : note_editor.title;
        size_t target_size = note_editor_body_active
            ? sizeof(note_editor.body) : sizeof(note_editor.title);
        size_t *cursor = note_editor_body_active
            ? &note_editor_body_cursor : &note_editor_title_cursor;

        if(state->selected < CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_insert_at(target, target_size, cursor,
                             editor_characters[state->selected]);
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_insert_at(target, target_size, cursor, " ");
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            editor_backspace_at(target, cursor);
        else {
            commit_note_editor();
            break;
        }
        schedule_note_editor_draft();
        render_current_route(false);
        break;
    }
    case NOTES_ROUTE_EXIT_ACTIONS:
        if(state->selected == 0) {
            commit_note_editor();
        }
        else if(state->selected == 1) {
            save_note_editor_draft();
            pop_route();
            if(current_route()->route == NOTES_ROUTE_COMPOSER)
                pop_route();
        }
        else
            push_route(NOTES_ROUTE_DISCARD_CONFIRM, -1);
        break;
    case NOTES_ROUTE_DISCARD_CONFIRM:
        break;
    case NOTES_ROUTE_SEARCH:
        if(state->selected < CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_append(note_search_query, sizeof(note_search_query),
                          editor_characters[state->selected]);
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_append(note_search_query, sizeof(note_search_query), " ");
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            editor_backspace(note_search_query);
        else if(note_search_query[0] != '\0')
            push_route(NOTES_ROUTE_SEARCH_RESULTS, -1);
        if(current_route()->route == NOTES_ROUTE_SEARCH)
            render_current_route(false);
        break;
    case NOTES_ROUTE_SEARCH_RESULTS: {
        const struct crazypod_note *note =
            crazypod_notes_search_get(
                note_search_query, state->selected);
        if(note != NULL)
            open_note_reader(note->id);
        break;
    }
    case NOTES_ROUTE_READER:
        push_route(NOTES_ROUTE_ACTIONS, state->group);
        break;
    case NOTES_ROUTE_ACTIONS: {
        uint32_t id = (uint32_t)state->group;
        const struct crazypod_note *note = crazypod_note_find(id);
        if(note == NULL) {
            pop_route();
            break;
        }
        if(state->selected == 0) {
            crazypod_note_set_pinned(id, !note->pinned);
            render_current_route(false);
        }
        else if(state->selected == 1)
            begin_note_composer(id, false);
        else if(state->selected == 2) {
            uint32_t duplicate = crazypod_note_duplicate(id);
            if(duplicate != 0) {
                route_depth = 1;
                route_stack[0].route = NOTES_ROUTE_MENU;
                route_stack[0].selected = 0;
                route_stack[0].group = -1;
                open_note_reader(duplicate);
            }
        }
        else
            push_route(NOTES_ROUTE_DELETE_CONFIRM, (int)id);
        break;
    }
    case NOTES_ROUTE_DELETED:
        if(state->selected == crazypod_notes_count(true))
            push_route(NOTES_ROUTE_EMPTY_TRASH_CONFIRM, -1);
        else {
            const struct crazypod_note *note =
                crazypod_note_get(true, state->selected);
            if(note != NULL)
                push_route(NOTES_ROUTE_DELETED_ACTIONS, (int)note->id);
        }
        break;
    case NOTES_ROUTE_DELETED_ACTIONS:
        if(state->selected == 0) {
            crazypod_note_restore((uint32_t)state->group);
            pop_route();
        }
        else
            push_route(NOTES_ROUTE_PERMANENT_CONFIRM, state->group);
        break;
    case NOTES_ROUTE_DELETE_CONFIRM:
    case NOTES_ROUTE_PERMANENT_CONFIRM:
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        break;
    case BOOKS_ROUTE_MENU: {
        int logical = state->selected;
        if(books_has_continue() && state->selected == 0) {
            int index = crazypod_books_recent_index();
            const struct crazypod_book *book =
                crazypod_book_get(index);
            if(book != NULL)
                begin_book_reader(index, book->progress);
            break;
        }
        logical -= books_has_continue() ? 1 : 0;
        if(logical == 0)
            push_route(BOOKS_ROUTE_RECENTS, -1);
        else if(logical == 1)
            push_route(BOOKS_ROUTE_LIBRARY, -1);
        else if(logical == 2)
            push_route(BOOKS_ROUTE_FAVORITES, -1);
        else if(logical == 3)
            push_route(BOOKS_ROUTE_STATS, -1);
        else if(logical == 4)
            push_route(BOOKS_ROUTE_READING_SETTINGS, -1);
        break;
    }
    case BOOKS_ROUTE_RECENTS:
    case BOOKS_ROUTE_LIBRARY:
    case BOOKS_ROUTE_FAVORITES: {
        int index = books_route_book_index(state, state->selected);
        if(index >= 0)
            push_route(BOOKS_ROUTE_ACTIONS, index);
        break;
    }
    case BOOKS_ROUTE_READER:
        push_route(BOOKS_ROUTE_ACTIONS, state->group);
        break;
    case BOOKS_ROUTE_ACTIONS: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);
        if(book == NULL) {
            pop_route();
            break;
        }
        if(state->selected == 0)
            begin_book_reader(state->group, book->progress);
        else if(state->selected == 1)
            push_route(BOOKS_ROUTE_BOOKMARKS, state->group);
        else if(state->selected == 2)
            push_route(BOOKS_ROUTE_CHAPTERS, state->group);
        else if(state->selected == 3) {
            crazypod_book_toggle_favorite(state->group);
            render_current_route(false);
        }
        else if(state->selected == 4)
            push_route(BOOKS_ROUTE_INFO, state->group);
        else
            push_route(BOOKS_ROUTE_DELETE_CONFIRM, state->group);
        break;
    }
    case BOOKS_ROUTE_CHAPTERS: {
        uint32_t offset;
        if(crazypod_book_chapter_get(
               state->group, state->selected, NULL, 0, &offset))
            begin_book_reader(state->group, offset);
        break;
    }
    case BOOKS_ROUTE_BOOKMARKS: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);
        if(book != NULL && book->bookmark > 0)
            begin_book_reader(state->group, book->bookmark);
        break;
    }
    case BOOKS_ROUTE_READING_SETTINGS:
        if(state->selected == 0)
            show_choice_overlay(
                CHOICE_OVERLAY_BOOK_FONT_SIZE, 0,
                crazypod_books_font_size());
        else if(state->selected == 1)
            show_choice_overlay(
                CHOICE_OVERLAY_BOOK_THEME, 0,
                crazypod_books_theme());
        else if(state->selected == 2) {
            rescan_books_with_progress();
        }
        break;
    case BOOKS_ROUTE_STATS:
    case BOOKS_ROUTE_INFO:
    case BOOKS_ROUTE_DELETE_CONFIRM:
        break;
    case PODCASTS_ROUTE_MENU: {
        int track_index = podcast_track_index(state->selected);
        if(track_index >= 0 &&
           crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, track_index)) {
            crazypod_state_forget_resume();
            crazypod_state_mark_dirty();
            request_now_playing_route();
        }
        break;
    }
    case UTILITIES_ROUTE_MENU:
        miniapp_last_error =
            crazypod_miniapps_open(state->selected);
        if(miniapp_last_error == CRAZYPOD_MINIAPP_OK) {
            int selected = state->selected;

            miniapp_last_service_tick = 0;
            crazypod_miniapp_input_reset(&miniapp_input_queue);
            push_route(MINIAPP_ROUTE_VIEW, selected);
        }
        else
            render_current_route(false);
        break;
    case MINIAPP_ROUTE_VIEW:
        break;
    case CLOCK_ROUTE_MENU:
        if(state->selected == 0)
            push_route(CLOCK_ROUTE_VIEW, -1);
        else if(state->selected == 1)
            push_route(CLOCK_ROUTE_SLEEP_TIMER, -1);
        else
            push_route(STOPWATCH_ROUTE_VIEW, -1);
        break;
    case CLOCK_ROUTE_SLEEP_TIMER:
        if(get_sleep_timer_active()) {
            if(state->selected == 1 &&
               (audio_status() & AUDIO_STATUS_PLAY))
                audio_pause();
            set_sleeptimer_duration(0);
        }
        else {
            static const int durations[] = { 15, 30, 45, 60 };
            if(state->selected >= 0 && state->selected < 4) {
                global_settings.sleeptimer_duration =
                    durations[state->selected];
                set_sleeptimer_duration(durations[state->selected]);
                crazypod_state_mark_dirty();
            }
        }
        state->selected = 0;
        render_current_route(false);
        break;
    case CLOCK_ROUTE_VIEW:
        break;
    case STOPWATCH_ROUTE_VIEW:
        stopwatch_reset_armed_until = 0;
        if(stopwatch_running) {
            stopwatch_accumulated_ticks +=
                current_tick - stopwatch_started_at;
            stopwatch_running = false;
        }
        else {
            stopwatch_started_at = current_tick;
            stopwatch_running = true;
        }
        stopwatch_last_render_tick = current_tick;
        render_current_route(false);
        break;
    case WORKOUT_ROUTE_MENU:
        if(state->selected == 0)
            push_route(WORKOUT_ROUTE_TYPES, -1);
        else if(state->selected == 1)
            push_route(WORKOUT_ROUTE_HISTORY, -1);
        else
            push_route(WORKOUT_ROUTE_SUMMARY, -1);
        break;
    case WORKOUT_ROUTE_TYPES:
        workout_activity = state->selected;
        workout_running = false;
        workout_accumulated_ticks = 0;
        push_route(WORKOUT_ROUTE_READY, -1);
        break;
    case WORKOUT_ROUTE_READY:
        workout_accumulated_ticks = 0;
        workout_started_at = current_tick;
        workout_last_render_tick = current_tick;
        workout_running = true;
        push_route(WORKOUT_ROUTE_ACTIVE, -1);
        break;
    case WORKOUT_ROUTE_ACTIVE:
        if(workout_running) {
            workout_accumulated_ticks +=
                current_tick - workout_started_at;
            workout_running = false;
        }
        else {
            workout_started_at = current_tick;
            workout_running = true;
        }
        render_current_route(false);
        break;
    case WORKOUT_ROUTE_HISTORY:
        if(crazypod_workout_get(state->selected) != NULL)
            push_route(WORKOUT_ROUTE_DETAIL, state->selected);
        break;
    case WORKOUT_ROUTE_DETAIL: {
        const struct crazypod_workout *workout =
            crazypod_workout_get(state->group);
        if(workout != NULL)
            push_route(WORKOUT_ROUTE_DELETE_CONFIRM,
                       (int)workout->id);
        break;
    }
    case WORKOUT_ROUTE_FINISH_CONFIRM:
    case WORKOUT_ROUTE_SUMMARY:
    case WORKOUT_ROUTE_DELETE_CONFIRM:
        break;
    case CALENDAR_ROUTE_MENU:
        if(state->selected == 0) {
            int today = calendar_today_date();
            calendar_focus_year = today / 10000;
            calendar_focus_month = today / 100 % 100 - 1;
            calendar_focus_day = today % 100;
            push_route(CALENDAR_ROUTE_TODAY, -1);
        }
        else if(state->selected == 1)
            push_route(CALENDAR_ROUTE_UPCOMING, -1);
        else if(state->selected == 2)
            push_route(CALENDAR_ROUTE_MONTH, -1);
        else
            begin_calendar_editor(0, calendar_today_date());
        break;
    case CALENDAR_ROUTE_TODAY:
    case CALENDAR_ROUTE_UPCOMING:
    case CALENDAR_ROUTE_DAY_EVENTS: {
        int event_count = calendar_route_event_count(state);
        int event_index;
        if(state->selected == event_count) {
            int date = state->route == CALENDAR_ROUTE_DAY_EVENTS
                ? calendar_focus_date() : calendar_today_date();
            begin_calendar_editor(0, date);
            break;
        }
        event_index = calendar_route_event_index(
            state, state->selected);
        if(event_index >= 0)
            push_route(CALENDAR_ROUTE_DETAIL, event_index);
        break;
    }
    case CALENDAR_ROUTE_MONTH:
        push_route(CALENDAR_ROUTE_DAY_EVENTS, -1);
        break;
    case CALENDAR_ROUTE_EDITOR:
        if(state->selected == 0)
            push_route(CALENDAR_ROUTE_TITLE_EDITOR, -1);
        else if(state->selected == 1) {
            calendar_editor_error = 0;
            calendar_editor_date =
                calendar_shifted_date(calendar_editor_date, 1);
            render_current_route(false);
        }
        else if(state->selected == 2) {
            calendar_editor_error = 0;
            calendar_editor_minutes =
                calendar_editor_minutes < 0
                    ? 9 * 60
                    : (calendar_editor_minutes + 30) % (24 * 60);
            render_current_route(false);
        }
        else
            commit_calendar_editor();
        break;
    case CALENDAR_ROUTE_TITLE_EDITOR:
        calendar_editor_error = 0;
        if(state->selected < CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_insert_at(
                calendar_editor_summary,
                sizeof(calendar_editor_summary),
                &calendar_editor_cursor,
                editor_characters[state->selected]);
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT)
            editor_insert_at(
                calendar_editor_summary,
                sizeof(calendar_editor_summary),
                &calendar_editor_cursor, " ");
        else if(state->selected == CRAZYPOD_EDITOR_CHAR_COUNT + 1)
            editor_backspace_at(
                calendar_editor_summary,
                &calendar_editor_cursor);
        else {
            pop_route();
            break;
        }
        render_current_route(false);
        break;
    case CALENDAR_ROUTE_DETAIL: {
        const struct crazypod_calendar_event *event =
            crazypod_calendar_event_get(state->group);
        if(event != NULL && event->editable)
            push_route(CALENDAR_ROUTE_ACTIONS, (int)event->id);
        break;
    }
    case CALENDAR_ROUTE_ACTIONS:
        if(state->selected == 0)
            begin_calendar_editor((uint32_t)state->group, 0);
        else
            push_route(CALENDAR_ROUTE_DELETE_CONFIRM, state->group);
        break;
    case CALENDAR_ROUTE_DELETE_CONFIRM:
        break;
    case CONTACTS_ROUTE_LIST:
        if(crazypod_contact_get(state->selected) != NULL)
            push_route(CONTACTS_ROUTE_DETAIL, state->selected);
        break;
    case CONTACTS_ROUTE_DETAIL:
        break;
    case SETTINGS_ROUTE_MENU:
        switch(state->selected) {
        case 0: push_route(SETTINGS_ROUTE_SOUND, -1); break;
        case 1: push_route(SETTINGS_ROUTE_DISPLAY, -1); break;
        case 2: push_route(SETTINGS_ROUTE_PLAYBACK, -1); break;
        case 3: push_route(SETTINGS_ROUTE_POWER, -1); break;
        case 4: push_route(SETTINGS_ROUTE_CONTROLS, -1); break;
        case 5: push_route(SETTINGS_ROUTE_MAIN_MENU, -1); break;
        }
        break;
    case SETTINGS_ROUTE_MAIN_MENU: {
        struct crazypod_app *app = ordered_app(state->selected);
        if(app != NULL)
            push_route(SETTINGS_ROUTE_MAIN_MENU_ACTIONS, app->id);
        break;
    }
    case SETTINGS_ROUTE_MAIN_MENU_ACTIONS: {
        enum crazypod_app_id id =
            (enum crazypod_app_id)state->group;
        enum crazypod_app_id preferred =
            crazypod_apps_visible_id(selected_app);
        int action = state->selected +
            (crazypod_apps_is_fixed(id) ? 1 : 0);
        bool changed = false;

        if(action == 0) {
            changed = crazypod_apps_set_enabled(
                id, !crazypod_apps_is_enabled(id));
        }
        else if(action == 1) {
            changed = crazypod_apps_move(id, -1);
        }
        else if(action == 2) {
            changed = crazypod_apps_move(id, 1);
        }
        if(changed) {
            struct route_state *parent = route_depth > 1
                ? &route_stack[route_depth - 2] : NULL;
            if(parent != NULL &&
               parent->route == SETTINGS_ROUTE_MAIN_MENU)
                parent->selected = crazypod_apps_order_index(id);
            persist_main_menu_change(preferred);
        }
        render_current_route(false);
        break;
    }
    case SETTINGS_ROUTE_SOUND:
    case SETTINGS_ROUTE_DISPLAY:
    case SETTINGS_ROUTE_PLAYBACK:
    case SETTINGS_ROUTE_POWER:
    case SETTINGS_ROUTE_CONTROLS: {
        int item = settings_route_item(state->route, state->selected);
        if(item == SETTINGS_ITEM_EQ_ENABLED) {
            eq_studio_band = clamp_value(eq_studio_band, 0,
                                         EQ_NUM_BANDS - 1);
            eq_studio_mode = EQ_STUDIO_GAIN;
            eq_studio_editing = false;
            push_route(SETTINGS_ROUTE_EQ_STUDIO, -1);
            break;
        }
        show_choice_overlay(CHOICE_OVERLAY_SETTING, item,
                            settings_choice_index(item));
        break;
    }
    case DIY_ROUTE_MENU:
        if(state->selected == 0)
            push_route(DIY_ROUTE_PRESETS, -1);
        else if(state->selected == 1)
            show_choice_overlay(CHOICE_OVERLAY_ICON_THEME,
                                CRAZYPOD_APPEARANCE_ICON_THEME,
                                crazypod_appearance_get()->icon_theme);
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
        show_choice_overlay(CHOICE_OVERLAY_APPEARANCE, field,
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
        enum crazypod_appearance_field field =
            background_field_for_index(state->selected);
        const struct crazypod_appearance *appearance =
            crazypod_appearance_get();
        const char *path = background_wallpaper(appearance, field);
        int selected = path[0] != '\0'
            ? CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
            : appearance_field_value(field);
        show_choice_overlay(CHOICE_OVERLAY_BACKGROUND, field, selected);
        break;
    }
    case DIY_ROUTE_BACKGROUND_CHOICES: {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)state->group;
        enum crazypod_wallpaper_target target =
            wallpaper_target_for_field(field);
        if(state->selected ==
           CRAZYPOD_APPEARANCE_COLOR_COUNT + 1) {
            const struct crazypod_appearance *appearance =
                crazypod_appearance_get();
            const char *path =
                background_wallpaper(appearance, field);

            push_route_selected(
                DIY_ROUTE_WALLPAPER_FILES, field,
                photo_index_for_path(path));
        }
        else {
            crazypod_wallpaper_clear(target);
            crazypod_appearance_set_value(field, state->selected);
            refresh_desktop_appearance();
            render_current_route(false);
        }
        break;
    }
    case DIY_ROUTE_WALLPAPER_FILES: {
        if(state->selected >= 0 &&
           state->selected < crazypod_photo_count()) {
            wallpaper_crop_photo_index = state->selected;
            wallpaper_crop_target =
                (enum crazypod_appearance_field)state->group;
            wallpaper_crop_zoom_percent = 140;
            wallpaper_crop_center_x = -1;
            wallpaper_crop_center_y = -1;
            wallpaper_crop_render_pending = false;
            wallpaper_crop_phase = WALLPAPER_CROP_EDITING;
            wallpaper_crop_error_loading = false;
            wallpaper_crop_feedback_until = 0;
            wallpaper_crop_menu_holding = false;
            wallpaper_crop_menu_armed = false;
            wallpaper_crop_play_holding = false;
            wallpaper_crop_play_armed = false;
            wallpaper_crop_select_armed = false;
            wallpaper_crop_load_progress_seen = -2;
            wallpaper_crop_apply_progress = 0;
            wallpaper_crop_apply_result =
                CRAZYPOD_WALLPAPER_APPLY_OK;
            crazypod_photo_view(wallpaper_crop_photo_index);
            push_route(
                DIY_ROUTE_WALLPAPER_CROP,
                wallpaper_crop_photo_index);
        }
        else
            render_current_route(false);
        break;
    }
    case DIY_ROUTE_LAYOUT: {
        enum crazypod_appearance_field field =
            diy_layout_fields[state->selected];
        show_choice_overlay(CHOICE_OVERLAY_APPEARANCE, field,
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

    if(choice_overlay.kind != CHOICE_OVERLAY_NONE) {
        move_choice_overlay(direction);
        return;
    }
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
    if(state->route == PHOTOS_ROUTE_DETAIL) {
        if(route_depth > 1) {
            struct route_state *parent =
                &route_stack[route_depth - 2];
            int parent_count = route_item_count(parent);

            if(parent_count > 0) {
                int next = parent->selected + direction;

                if(next < 0)
                    next = 0;
                if(next >= parent_count)
                    next = parent_count - 1;
                if(next != parent->selected) {
                    parent->selected = next;
                    state->group = photo_route_index(
                        parent, parent->selected);
                    state->selected = 0;
                    photo_pan_x = 0;
                    photo_pan_y = 0;
                    photo_zoom_percent = 100;
                }
            }
        }
        render_current_route(false);
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
    int next = state->selected + direction;
    if(next < 0)
        next = 0;
    if(next >= count)
        next = count - 1;
    if(next == state->selected)
        return;
    state->selected = next;
    if(is_music_preview_route(state->route))
        prefetch_music_preview_artwork(state);
    if(is_skeuomorphic_preview_route(state->route))
        menu_preview_navigation_direction = direction < 0 ? -1 : 1;
    if(state->route == MUSIC_ROUTE_SEARCH) {
        refresh_menu_rows(state);
        return;
    }
    if(menu_view.valid && menu_view.route == state->route) {
        refresh_menu_rows(state);
        menu_preview_due = current_tick +
            (is_music_preview_route(state->route)
                ? CRAZYPOD_MUSIC_PREVIEW_SETTLE_TICKS
                : CRAZYPOD_PREVIEW_SETTLE_TICKS);
        menu_preview_pending = true;
    }
    else {
        route_render_due = current_tick;
        route_render_pending = true;
    }
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

static void previous_or_restart_track(void)
{
    const struct mp3entry *id3;

    if(crazypod_queue_count() <= 0)
        return;

    id3 = audio_current_track();
    if(id3 != NULL &&
       id3->elapsed >= CRAZYPOD_PREVIOUS_RESTART_THRESHOLD_MS) {
        audio_pre_ff_rewind();
        audio_ff_rewind(0);
    }
    else {
        audio_prev();
    }
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
        int artwork_slot =
            now_playing_artwork_slot_for_track(track);
        enum now_playing_overlay overlay = now_overlay;

        (void)crazypod_artwork_load_priority(
            artwork_slot, track,
            CRAZYPOD_NOW_ARTWORK_CACHE_SIZE, 0);
        if(crazypod_artwork_state(
               artwork_slot, track,
               CRAZYPOD_NOW_ARTWORK_CACHE_SIZE) ==
           CRAZYPOD_ARTWORK_PENDING)
            return;
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

    if(id3 != NULL && id3->length > 0 &&
       now_progress_marker != NULL) {
        int x = 281 * id3->elapsed / id3->length;
        char elapsed[16];
        char remaining[16];
        unsigned long left = id3->length > id3->elapsed
            ? id3->length - id3->elapsed : 0;
        if(x < 0)
            x = 0;
        if(x > 281)
            x = 281;
        lv_obj_set_x(now_progress_marker, x);
        format_time_ms(id3->elapsed, elapsed, sizeof(elapsed));
        format_time_ms(left, remaining + 1, sizeof(remaining) - 1);
        remaining[0] = '-';
        lv_label_set_text(now_elapsed, elapsed);
        lv_label_set_text(now_remaining, remaining);
    }
}

static unsigned menu_preview_artwork_signature(void)
{
    unsigned signature = 2166136261u;
    bool album_flow_preview =
        product_active && route_depth > 0 &&
        current_route()->route == MUSIC_ROUTE_MENU &&
        current_route()->selected == 1;
    int first_slot = album_flow_preview
        ? CRAZYPOD_MENU_FLOW_ARTWORK_SLOT_BASE : 0;
    int slot_count = album_flow_preview ? 3 : 7;
    int slot;

    for(slot = first_slot;
        slot < first_slot + slot_count;
        ++slot) {
        signature ^= crazypod_artwork_slot_generation(slot);
        signature *= 16777619u;
    }
    signature ^=
        crazypod_artwork_slot_generation(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT);
    signature *= 16777619u;
    return signature;
}

static void process_artwork_updates(void)
{
    unsigned generation = crazypod_artwork_slot_generation(
        CRAZYPOD_CAPSULE_ARTWORK_SLOT);

    if(generation != capsule_artwork_generation_seen) {
        capsule_artwork_generation_seen = generation;
        update_desktop_capsule_artwork(current_track());
    }
    if(!product_active || route_depth <= 0 || music_scan_screen ||
       route_render_pending ||
       menu_preview_pending || menu_preview_media_refresh_pending ||
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
    else if(current_route()->route == MUSIC_ROUTE_MENU) {
        unsigned signature =
            menu_preview_artwork_signature();
        if(signature == menu_preview_artwork_generation_seen)
            return;
        if(menu_preview_motion_active())
            return;
        menu_preview_artwork_generation_seen = signature;
        render_menu_preview(current_route(), false);
        return;
    }
    else {
        generation = crazypod_artwork_slot_generation(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        if(generation == preview_artwork_generation_seen)
            return;
        if(menu_view.valid &&
           menu_view.route == current_route()->route &&
           is_music_preview_route(current_route()->route) &&
           menu_preview_motion_active())
            return;
        preview_artwork_generation_seen = generation;
    }
    if(menu_view.valid &&
       menu_view.route == current_route()->route &&
       is_music_preview_route(current_route()->route))
        render_menu_preview(current_route(), false);
    else
        render_current_route(false);
}

static void process_photo_updates(void)
{
    enum crazypod_route route;
    unsigned generation;

    if(!product_active || route_depth <= 0 ||
       route_render_pending || menu_preview_pending ||
       menu_preview_media_refresh_pending)
        return;
    route = current_route()->route;
    if(route == PHOTOS_ROUTE_DETAIL ||
       route == DIY_ROUTE_WALLPAPER_CROP) {
        generation = crazypod_photo_view_generation();
        if(generation == photo_view_generation_seen)
            return;
        photo_view_generation_seen = generation;
        render_current_route(false);
        return;
    }
    generation = crazypod_photo_generation();
    if(generation == photo_generation_seen)
        return;
    if(route == PHOTOS_ROUTE_MENU) {
        if(menu_preview_motion_active())
            return;
        photo_generation_seen = generation;
        render_menu_preview(current_route(), false);
        return;
    }
    photo_generation_seen = generation;
    if(route == PHOTOS_ROUTE_LIBRARY ||
       route == PHOTOS_ROUTE_FAVORITES ||
       route == DIY_ROUTE_WALLPAPER_FILES)
        render_current_route(false);
}

static void process_video_updates(void)
{
    enum crazypod_route route;
    unsigned generation;

    if(!product_active || route_depth <= 0 ||
       route_render_pending || menu_preview_pending ||
       menu_preview_media_refresh_pending)
        return;
    route = current_route()->route;
    generation = crazypod_video_generation();
    if(generation == video_generation_seen)
        return;
    if((route == PHOTOS_ROUTE_MENU ||
        route == PHOTOS_ROUTE_VIDEOS) &&
       menu_preview_motion_active())
        return;
    video_generation_seen = generation;
    if(route == PHOTOS_ROUTE_MENU ||
       route == PHOTOS_ROUTE_VIDEOS)
        render_menu_preview(current_route(), false);
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

static void clear_power_prompt_objects(void)
{
    memset(&power_prompt_view, 0, sizeof(power_prompt_view));
}

static void refresh_power_prompt(void)
{
    int index;

    if(!power_prompt_visible())
        return;

    for(index = 0; index < 2; ++index) {
        bool selected = index == power_prompt_view.selected;

        lv_obj_set_style_bg_color(
            power_prompt_view.rows[index],
            lv_color_hex(selected ? COLOR_WHITE : COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(
            power_prompt_view.rows[index],
            selected ? 34 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(
            power_prompt_view.rows[index], selected ? 1 : 0, 0);
        lv_obj_set_style_border_color(
            power_prompt_view.rows[index], lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(
            power_prompt_view.rows[index], selected ? 72 : 0, 0);
        lv_label_set_text(
            power_prompt_view.markers[index],
            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET);
        lv_obj_set_style_text_opa(
            power_prompt_view.markers[index], selected ? 210 : 90, 0);
        lv_obj_set_style_text_opa(
            power_prompt_view.hints[index], selected ? 190 : 120, 0);
    }
}

static void dismiss_power_prompt(void)
{
    if(power_prompt_visible()) {
        lv_anim_delete(power_prompt_view.panel, NULL);
        lv_obj_delete(power_prompt_view.root);
    }
    clear_power_prompt_objects();
    desktop_native_backdrop_ready = false;
    desktop_native_dirty = true;
}

static void show_power_prompt(void)
{
    static const char *const titles[] = { "Shut Down", "Restart" };
    static const char *const hints[] = {
        "Turn CrazyPod off",
        "Restart CrazyPod"
    };
    static const char *const symbols[] = {
        LV_SYMBOL_POWER,
        LV_SYMBOL_REFRESH
    };
    lv_obj_t *title;
    lv_obj_t *detail;
    int index;

    if(desktop_screen == NULL || power_prompt_visible())
        return;

    if(choice_overlay.kind != CHOICE_OVERLAY_NONE)
        dismiss_choice_overlay(false);
    if(now_overlay != NOW_OVERLAY_NONE)
        dismiss_now_overlay(false);

    menu_preview_pending = false;
    menu_preview_media_refresh_pending = false;
    if(menu_preview_motion_active())
        settle_menu_preview_motion();

    backlight_on();
    prepare_now_overlay_glass(false);
    power_prompt_view.selected = 0;
    power_prompt_view.root = make_box(
        desktop_screen, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
        0x000000, 86);
    lv_obj_remove_flag(
        power_prompt_view.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(power_prompt_view.root);
    desktop_native_backdrop_ready = false;
    power_prompt_view.panel = make_glass_panel(
        power_prompt_view.root, 35, 55, 250, 132);

    title = make_label(power_prompt_view.panel, "POWER",
                       &lv_font_montserrat_10, COLOR_WHITE, 110);
    lv_obj_set_width(title, 250);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 14);

    detail = make_label(power_prompt_view.panel, "Choose Action",
                        &lv_font_montserrat_12, COLOR_WHITE, 235);
    lv_obj_set_width(detail, 250);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(detail, 0, 29);

    for(index = 0; index < 2; ++index) {
        lv_obj_t *symbol;
        lv_obj_t *label;
        int y = 53 + index * 35;

        power_prompt_view.rows[index] = make_box(
            power_prompt_view.panel, 16, y, 218, 30, 9,
            COLOR_WHITE, LV_OPA_TRANSP);
        symbol = make_label(
            power_prompt_view.rows[index], symbols[index],
            &lv_font_montserrat_12, COLOR_WHITE, 220);
        lv_obj_set_pos(symbol, 13, 7);
        label = make_label(
            power_prompt_view.rows[index], titles[index],
            &lv_font_montserrat_10, COLOR_WHITE, 235);
        lv_obj_set_pos(label, 39, 4);
        lv_obj_set_size(label, 86, 14);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        power_prompt_view.hints[index] = make_label(
            power_prompt_view.rows[index], hints[index],
            &lv_font_montserrat_8, COLOR_WHITE, 130);
        lv_obj_set_pos(power_prompt_view.hints[index], 39, 17);
        lv_obj_set_size(power_prompt_view.hints[index], 142, 11);
        lv_label_set_long_mode(
            power_prompt_view.hints[index],
            LV_LABEL_LONG_MODE_DOTS);
        power_prompt_view.markers[index] = make_label(
            power_prompt_view.rows[index], LV_SYMBOL_BULLET,
            &lv_font_montserrat_8, COLOR_WHITE, 90);
        lv_obj_set_width(power_prompt_view.markers[index], 24);
        lv_obj_set_style_text_align(
            power_prompt_view.markers[index],
            LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(power_prompt_view.markers[index], 184, 11);
    }

    refresh_power_prompt();
    animate_now_popup(power_prompt_view.panel, 55);
}

static void execute_power_action(enum shutdown_type type)
{
    if(crazypod_miniapps_is_open()) {
        crazypod_miniapp_input_reset(&miniapp_input_queue);
        crazypod_miniapps_close();
    }
    crazypod_state_save(true);
    shutdown_hw(type);
}

static void finish_power_prompt(void)
{
    enum shutdown_type type;

    if(!power_prompt_visible())
        return;
    type = power_prompt_view.selected == 0
        ? SHUTDOWN_POWER_OFF : SHUTDOWN_REBOOT;
    dismiss_power_prompt();
    execute_power_action(type);
}

static void move_power_prompt(int direction)
{
    int next;

    if(!power_prompt_visible())
        return;
    next = power_prompt_view.selected + (direction > 0 ? 1 : -1);
    if(next < 0)
        next = 0;
    if(next > 1)
        next = 1;
    if(next == power_prompt_view.selected)
        return;
    power_prompt_view.selected = next;
    refresh_power_prompt();
}

static bool handle_power_prompt_button(long base, bool repeated,
                                       intptr_t data)
{
    (void)data;

    if(!power_prompt_visible())
        return false;
    if(base == BUTTON_SCROLL_FWD)
        move_power_prompt(1);
    else if(base == BUTTON_SCROLL_BACK)
        move_power_prompt(-1);
    else if(base == BUTTON_RIGHT)
        move_power_prompt(1);
    else if(base == BUTTON_LEFT)
        move_power_prompt(-1);
    else if(base == BUTTON_SELECT && !repeated)
        finish_power_prompt();
    else if(base == BUTTON_MENU && !repeated)
        dismiss_power_prompt();
    return true;
}

#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
static void clear_usb_prompt_objects(void)
{
    memset(&usb_prompt_view, 0, sizeof(usb_prompt_view));
}

static void refresh_usb_prompt(void)
{
    int index;

    if(usb_prompt_view.root == NULL)
        return;

    for(index = 0; index < 2; ++index) {
        bool selected = index == usb_prompt_view.selected;

        lv_obj_set_style_bg_color(
            usb_prompt_view.rows[index],
            lv_color_hex(selected ? COLOR_WHITE : COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(
            usb_prompt_view.rows[index], selected ? 34 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(
            usb_prompt_view.rows[index], selected ? 1 : 0, 0);
        lv_obj_set_style_border_color(
            usb_prompt_view.rows[index], lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(
            usb_prompt_view.rows[index], selected ? 72 : 0, 0);
        lv_label_set_text(
            usb_prompt_view.markers[index],
            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET);
        lv_obj_set_style_text_opa(
            usb_prompt_view.markers[index], selected ? 210 : 90, 0);
        lv_obj_set_style_text_opa(
            usb_prompt_view.hints[index], selected ? 190 : 120, 0);
    }
}

static void dismiss_usb_prompt(void)
{
    if(usb_prompt_view.root != NULL) {
        lv_anim_delete(usb_prompt_view.panel, NULL);
        lv_obj_delete(usb_prompt_view.root);
    }
    clear_usb_prompt_objects();
    desktop_native_backdrop_ready = false;
    desktop_native_dirty = true;
}

static void apply_usb_prompt_charge_mode(void)
{
#ifdef HAVE_USB_CHARGING_ENABLE
    if(global_settings.usb_charging != USB_CHARGING_FORCE) {
        global_settings.usb_charging = USB_CHARGING_FORCE;
        usb_charging_enable(global_settings.usb_charging);
        crazypod_state_mark_dirty();
    }
#endif
}

static void show_usb_prompt(unsigned request)
{
    static const char *const titles[] = { "Charge", "Data" };
    static const char *const hints[] = {
        "Keep CrazyPod running",
        "Mount disk on computer"
    };
    static const char *const symbols[] = {
        LV_SYMBOL_CHARGE,
        LV_SYMBOL_DOWNLOAD
    };
    lv_obj_t *title;
    lv_obj_t *detail;
    int index;

    if(desktop_screen == NULL) {
        usb_prompt_result = USB_MODE_CHARGE;
        if(usb_prompt_waiting)
            semaphore_release(&usb_prompt_response);
        return;
    }

    dismiss_usb_prompt();
    dismiss_power_prompt();
    if(choice_overlay.kind != CHOICE_OVERLAY_NONE)
        dismiss_choice_overlay(false);
    if(now_overlay != NOW_OVERLAY_NONE)
        dismiss_now_overlay(false);

    /*
     * The USB prompt owns the UI until the USB thread receives a response.
     * Do not leave a hidden menu-preview transition running behind it.
     */
    menu_preview_pending = false;
    menu_preview_media_refresh_pending = false;
    if(menu_preview_motion_active())
        settle_menu_preview_motion();

    prepare_now_overlay_glass(true);
    usb_prompt_view.request = request;
    usb_prompt_view.selected = 0;
    usb_prompt_view.root = make_box(desktop_screen, 0, 0,
                                    LCD_WIDTH, LCD_HEIGHT, 0,
                                    0x000000, 86);
    lv_obj_remove_flag(usb_prompt_view.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(usb_prompt_view.root);
    desktop_native_backdrop_ready = false;
    usb_prompt_view.panel = make_glass_panel(
        usb_prompt_view.root, 35, 55, 250, 132);

    title = make_label(usb_prompt_view.panel, "USB CONNECTED",
                       &lv_font_montserrat_10, COLOR_WHITE, 110);
    lv_obj_set_width(title, 250);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 14);

    detail = make_label(usb_prompt_view.panel, "Choose Mode",
                        &lv_font_montserrat_12, COLOR_WHITE, 235);
    lv_obj_set_width(detail, 250);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(detail, 0, 29);

    for(index = 0; index < 2; ++index) {
        lv_obj_t *symbol;
        lv_obj_t *label;
        int y = 53 + index * 35;

        usb_prompt_view.rows[index] = make_box(
            usb_prompt_view.panel, 16, y, 218, 30, 9,
            COLOR_WHITE, LV_OPA_TRANSP);
        symbol = make_label(usb_prompt_view.rows[index], symbols[index],
                            &lv_font_montserrat_12,
                            COLOR_WHITE, 220);
        lv_obj_set_pos(symbol, 13, 7);
        label = make_label(usb_prompt_view.rows[index], titles[index],
                           &lv_font_montserrat_10,
                           COLOR_WHITE, 235);
        lv_obj_set_pos(label, 39, 4);
        lv_obj_set_size(label, 86, 14);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        usb_prompt_view.hints[index] = make_label(
            usb_prompt_view.rows[index], hints[index],
            &lv_font_montserrat_8, COLOR_WHITE, 130);
        lv_obj_set_pos(usb_prompt_view.hints[index], 39, 17);
        lv_obj_set_size(usb_prompt_view.hints[index], 142, 11);
        lv_label_set_long_mode(
            usb_prompt_view.hints[index], LV_LABEL_LONG_MODE_DOTS);
        usb_prompt_view.markers[index] = make_label(
            usb_prompt_view.rows[index], LV_SYMBOL_BULLET,
            &lv_font_montserrat_8, COLOR_WHITE, 90);
        lv_obj_set_width(usb_prompt_view.markers[index], 24);
        lv_obj_set_style_text_align(
            usb_prompt_view.markers[index], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(usb_prompt_view.markers[index], 184, 11);
    }

    refresh_usb_prompt();
    animate_now_popup(usb_prompt_view.panel, 55);
}

static void finish_usb_prompt(int mode)
{
    unsigned request = usb_prompt_view.request;

    if(usb_prompt_view.root == NULL)
        return;

    usb_prompt_result = mode;
    if(mode == USB_MODE_CHARGE)
        apply_usb_prompt_charge_mode();
    dismiss_usb_prompt();
    if(usb_prompt_waiting && request == usb_prompt_request_id)
        semaphore_release(&usb_prompt_response);
}

static void move_usb_prompt(int direction)
{
    int next;

    if(usb_prompt_view.root == NULL)
        return;
    next = usb_prompt_view.selected + (direction > 0 ? 1 : -1);
    if(next < 0)
        next = 0;
    if(next > 1)
        next = 1;
    if(next == usb_prompt_view.selected)
        return;
    usb_prompt_view.selected = next;
    refresh_usb_prompt();
}

static bool handle_usb_prompt_button(long base, bool repeated,
                                     intptr_t data)
{
    (void)data;

    if(usb_prompt_view.root == NULL)
        return false;
    if(base == BUTTON_SCROLL_FWD)
        move_usb_prompt(1);
    else if(base == BUTTON_SCROLL_BACK)
        move_usb_prompt(-1);
    else if(base == BUTTON_RIGHT)
        move_usb_prompt(1);
    else if(base == BUTTON_LEFT)
        move_usb_prompt(-1);
    else if(base == BUTTON_SELECT && !repeated)
        finish_usb_prompt(usb_prompt_view.selected == 0
                          ? USB_MODE_CHARGE
                          : USB_MODE_MASS_STORAGE);
    else if(base == BUTTON_MENU && !repeated)
        finish_usb_prompt(USB_MODE_CHARGE);
    else if(base == BUTTON_PLAY && !repeated)
        finish_usb_prompt(USB_MODE_MASS_STORAGE);
    return true;
}

static void usb_prompt_inserted_event(unsigned short id, void *data)
{
    int *mode = (int *)data;

    (void)id;
    if(mode == NULL)
        return;

    if(!usb_prompt_registered || !usb_prompt_ui_ready) {
        apply_usb_prompt_charge_mode();
        *mode = USB_MODE_CHARGE;
        return;
    }

    while(semaphore_wait(&usb_prompt_response, TIMEOUT_NOBLOCK) ==
          OBJ_WAIT_SUCCEEDED) {
        /* Drain a stale response from a timed-out prompt. */
    }

    usb_prompt_result = USB_MODE_CHARGE;
    usb_prompt_waiting = true;
    ++usb_prompt_request_id;
    button_queue_post(CRAZYPOD_USB_PROMPT_REQUEST,
                      usb_prompt_request_id);

    if(semaphore_wait(&usb_prompt_response,
                      CRAZYPOD_USB_PROMPT_TIMEOUT) !=
       OBJ_WAIT_SUCCEEDED)
        usb_prompt_result = USB_MODE_CHARGE;

    if(usb_prompt_result == USB_MODE_CHARGE)
        apply_usb_prompt_charge_mode();
    *mode = usb_prompt_result;
    usb_prompt_waiting = false;
    button_queue_post(CRAZYPOD_USB_PROMPT_DONE, usb_prompt_request_id);
}
#endif

void crazypod_ui_usb_prompt_init(void)
{
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    if(usb_prompt_registered)
        return;
    semaphore_init(&usb_prompt_response, 1, 0);
    usb_prompt_result = USB_MODE_CHARGE;
    usb_prompt_registered =
        add_event(SYS_EVENT_USB_INSERTED, usb_prompt_inserted_event);
#endif
}

static void adjust_photo_zoom(int direction, int steps)
{
    struct route_state *state = current_route();

    while(steps-- > 0) {
        if(direction > 0)
            photo_zoom_percent =
                (photo_zoom_percent * 104 + 50) / 100;
        else
            photo_zoom_percent =
                (photo_zoom_percent * 96 + 50) / 100;
    }
    if(photo_zoom_percent < 100)
        photo_zoom_percent = 100;
    if(photo_zoom_percent > 500)
        photo_zoom_percent = 500;
    state->selected = photo_zoom_percent > 100 ? 1 : 0;
    if(state->selected == 0) {
        photo_pan_x = 0;
        photo_pan_y = 0;
    }
    render_current_route(false);
}

static void adjust_wallpaper_crop_zoom(int direction, int steps)
{
    const lv_image_dsc_t *source =
        crazypod_photo_view(wallpaper_crop_photo_index);
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;

    while(steps-- > 0) {
        if(direction > 0)
            wallpaper_crop_zoom_percent =
                (wallpaper_crop_zoom_percent * 104 + 50) / 100;
        else
            wallpaper_crop_zoom_percent =
                (wallpaper_crop_zoom_percent * 96 + 50) / 100;
    }
    if(wallpaper_crop_zoom_percent < 100)
        wallpaper_crop_zoom_percent = 100;
    if(wallpaper_crop_zoom_percent > 500)
        wallpaper_crop_zoom_percent = 500;
    if(source != NULL)
        wallpaper_crop_rect(
            source, &crop_x, &crop_y, &crop_width, &crop_height);
    wallpaper_crop_phase = WALLPAPER_CROP_EDITING;
    wallpaper_crop_error_loading = false;
    wallpaper_crop_render_pending = true;
}

static void move_wallpaper_crop(int direction_x, int direction_y)
{
    const lv_image_dsc_t *source =
        crazypod_photo_view(wallpaper_crop_photo_index);
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    int step_x;
    int step_y;

    if(!wallpaper_crop_rect(
           source, &crop_x, &crop_y, &crop_width, &crop_height))
        return;
    step_x = crop_width / 12;
    step_y = crop_height / 12;
    if(step_x < 1)
        step_x = 1;
    if(step_y < 1)
        step_y = 1;
    wallpaper_crop_center_x += direction_x * step_x;
    wallpaper_crop_center_y += direction_y * step_y;
    wallpaper_crop_rect(
        source, &crop_x, &crop_y, &crop_width, &crop_height);
    wallpaper_crop_phase = WALLPAPER_CROP_EDITING;
    wallpaper_crop_error_loading = false;
    wallpaper_crop_render_pending = true;
}

static void reset_wallpaper_crop(void)
{
    wallpaper_crop_zoom_percent = 140;
    wallpaper_crop_center_x = -1;
    wallpaper_crop_center_y = -1;
    wallpaper_crop_phase = WALLPAPER_CROP_EDITING;
    wallpaper_crop_error_loading = false;
    wallpaper_crop_render_pending = true;
}

static void wallpaper_crop_apply_progress_update(
    int progress, void *user_data)
{
    char text[40];
    int fill_width;

    (void)user_data;
    if(progress < 0)
        progress = 0;
    if(progress > 100)
        progress = 100;
    wallpaper_crop_apply_progress = progress;
    if(wallpaper_crop_progress_fill == NULL ||
       wallpaper_crop_progress_label == NULL)
        return;
    fill_width = progress * 200 / 100;
    if(fill_width < 2)
        fill_width = 2;
    snprintf(text, sizeof(text),
             "Applying wallpaper  %d%%", progress);
    lv_obj_set_width(
        wallpaper_crop_progress_fill, fill_width);
    lv_label_set_text(
        wallpaper_crop_progress_label, text);
    /* The crop renderer blocks the UI loop.  Commit each real milestone
     * immediately instead of leaving it queued for the next UI tick. */
    lv_refr_now(NULL);
    crazypod_present_now();
}

static void apply_wallpaper_crop(void)
{
    const lv_image_dsc_t *source =
        crazypod_photo_view(wallpaper_crop_photo_index);
    const char *path =
        crazypod_photo_path(wallpaper_crop_photo_index);
    enum crazypod_wallpaper_target target =
        wallpaper_target_for_field(wallpaper_crop_target);
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    enum crazypod_wallpaper_apply_result result;

    if(!wallpaper_crop_rect(
           source, &crop_x, &crop_y, &crop_width, &crop_height)) {
        wallpaper_crop_phase = WALLPAPER_CROP_ERROR;
        wallpaper_crop_error_loading = true;
        wallpaper_crop_apply_result =
            CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE;
        wallpaper_crop_render_pending = true;
        return;
    }
    wallpaper_crop_phase = WALLPAPER_CROP_APPLYING;
    wallpaper_crop_error_loading = false;
    wallpaper_crop_apply_progress = 5;
    render_current_route(false);
    lv_refr_now(NULL);
    /* The original-image decode starts immediately below and can take a
     * noticeable time.  Present this first frame before entering it. */
    crazypod_present_now();
    result = crazypod_wallpaper_apply_crop(
        target, path, source, crop_x, crop_y,
        crop_width, crop_height,
        wallpaper_crop_apply_progress_update, NULL);
    wallpaper_crop_apply_result = result;
    if(result != CRAZYPOD_WALLPAPER_APPLY_OK) {
        wallpaper_crop_phase = WALLPAPER_CROP_ERROR;
        wallpaper_crop_error_loading = false;
        render_current_route(false);
        return;
    }
    refresh_desktop_appearance();
    wallpaper_crop_phase = WALLPAPER_CROP_APPLIED;
    wallpaper_crop_feedback_until =
        current_tick + CRAZYPOD_WALLPAPER_CROP_SUCCESS_TICKS;
    render_current_route(false);
}

static int selected_photo_index(const struct route_state *state)
{
    if(state->route == PHOTOS_ROUTE_DETAIL)
        return state->group;
    if(state->route == PHOTOS_ROUTE_LIBRARY ||
       state->route == PHOTOS_ROUTE_FAVORITES)
        return photo_route_index(state, state->selected);
    return -1;
}

static void process_photo_favorite_hold(void)
{
    struct route_state *state;

    if(!product_active || route_depth <= 0)
        return;
    state = current_route();
    if(photo_select_holding && !photo_select_long_handled) {
        long elapsed = current_tick - photo_select_hold_start;
        int percent;

        if(elapsed < 0)
            elapsed = 0;
        if(elapsed <
           CRAZYPOD_PHOTO_FAVORITE_PROGRESS_DELAY_TICKS) {
            percent = -1;
        }
        else {
            percent = (int)(
                (elapsed -
                 CRAZYPOD_PHOTO_FAVORITE_PROGRESS_DELAY_TICKS) *
                100 /
                (CRAZYPOD_PHOTO_FAVORITE_HOLD_TICKS -
                 CRAZYPOD_PHOTO_FAVORITE_PROGRESS_DELAY_TICKS));
        }
        if(percent > 100)
            percent = 100;
        if(percent != photo_select_hold_percent) {
            bool needs_overlay =
                percent >= 0 &&
                photo_favorite_progress_fill == NULL;

            photo_select_hold_percent = percent;
            if(needs_overlay)
                render_current_route(false);
            else if(photo_favorite_progress_fill != NULL) {
                int width = CRAZYPOD_PHOTO_FAVORITE_PROGRESS_WIDTH *
                    percent / 100;

                if(width < 1)
                    width = 1;
                lv_obj_set_width(photo_favorite_progress_fill, width);
            }
        }
        if(percent >= 100) {
            int photo_index = selected_photo_index(state);
            bool was_favorite =
                crazypod_photo_is_favorite(photo_index);
            bool saved =
                crazypod_photo_toggle_favorite(photo_index);

            photo_select_holding = false;
            photo_select_long_handled = true;
            photo_favorite_feedback_error = !saved;
            photo_favorite_feedback_added =
                saved ? crazypod_photo_is_favorite(photo_index)
                      : was_favorite;
            photo_favorite_feedback_until =
                current_tick +
                CRAZYPOD_PHOTO_FAVORITE_FEEDBACK_TICKS;
            if(saved &&
               state->route == PHOTOS_ROUTE_FAVORITES &&
               !crazypod_photo_is_favorite(photo_index)) {
                int count = crazypod_photo_favorite_count();

                if(state->selected >= count)
                    state->selected = count > 0 ? count - 1 : 0;
            }
            render_current_route(false);
        }
    }
    if(photo_favorite_feedback_until != 0 &&
       !TIME_BEFORE(current_tick, photo_favorite_feedback_until)) {
        photo_favorite_feedback_until = 0;
        photo_favorite_feedback_error = false;
        if(state->route == PHOTOS_ROUTE_LIBRARY ||
           state->route == PHOTOS_ROUTE_FAVORITES ||
           state->route == PHOTOS_ROUTE_DETAIL)
            render_current_route(false);
    }
}

static void queue_photo_pan(int delta_x, int delta_y)
{
    photo_pan_x += delta_x;
    photo_pan_y += delta_y;
    photo_pan_render_pending = true;
}

static void process_photo_pan_render(void)
{
    if(!photo_pan_render_pending)
        return;
    photo_pan_render_pending = false;
    if(product_active && route_depth > 0 &&
       current_route()->route == PHOTOS_ROUTE_DETAIL &&
       photo_zoom_percent > 100)
        render_current_route(false);
}

static void process_wallpaper_crop_render(void)
{
    if(!wallpaper_crop_render_pending)
        return;
    wallpaper_crop_render_pending = false;
    if(product_active && route_depth > 0 &&
       current_route()->route == DIY_ROUTE_WALLPAPER_CROP)
        render_current_route(false);
}

static void process_wallpaper_crop_loading_progress(void)
{
    int progress;
    int fill_width;
    char text[40];

    if(!product_active || route_depth <= 0 ||
       current_route()->route != DIY_ROUTE_WALLPAPER_CROP ||
       wallpaper_crop_phase == WALLPAPER_CROP_APPLYING ||
       wallpaper_crop_phase == WALLPAPER_CROP_APPLIED ||
       crazypod_photo_view(wallpaper_crop_photo_index) != NULL)
        return;
    progress = crazypod_photo_view_progress(
        wallpaper_crop_photo_index);
    if(progress == wallpaper_crop_load_progress_seen)
        return;
    wallpaper_crop_load_progress_seen = progress;
    if(wallpaper_crop_progress_fill == NULL ||
       wallpaper_crop_progress_label == NULL)
        return;
    if(progress < 0) {
        snprintf(text, sizeof(text),
                 "Could not load picture");
        fill_width = 200;
        lv_obj_set_style_bg_color(
            wallpaper_crop_progress_fill,
            lv_color_hex(0xFF453A), 0);
    }
    else {
        if(progress > 100)
            progress = 100;
        snprintf(text, sizeof(text),
                 "Loading picture  %d%%", progress);
        fill_width = progress * 200 / 100;
        if(fill_width < 2)
            fill_width = 2;
    }
    lv_obj_set_width(
        wallpaper_crop_progress_fill, fill_width);
    lv_label_set_text(
        wallpaper_crop_progress_label, text);
}

static void process_wallpaper_crop_state(void)
{
    if(!product_active || route_depth <= 0 ||
       current_route()->route != DIY_ROUTE_WALLPAPER_CROP) {
        wallpaper_crop_menu_holding = false;
        wallpaper_crop_menu_armed = false;
        wallpaper_crop_play_holding = false;
        wallpaper_crop_play_armed = false;
        wallpaper_crop_select_armed = false;
        return;
    }
    if(wallpaper_crop_phase == WALLPAPER_CROP_APPLIED &&
       wallpaper_crop_feedback_until != 0 &&
       !TIME_BEFORE(
           current_tick, wallpaper_crop_feedback_until)) {
        wallpaper_crop_feedback_until = 0;
        wallpaper_crop_phase = WALLPAPER_CROP_EDITING;
        if(route_depth >= 3)
            route_depth -= 2;
        render_current_route(true);
        return;
    }
    if(wallpaper_crop_phase == WALLPAPER_CROP_APPLYING ||
       wallpaper_crop_phase == WALLPAPER_CROP_APPLIED)
        return;
    if(wallpaper_crop_menu_holding &&
       !wallpaper_crop_menu_armed &&
       !TIME_BEFORE(
           current_tick,
           wallpaper_crop_menu_hold_start +
               CRAZYPOD_WALLPAPER_CROP_HOLD_TICKS)) {
        wallpaper_crop_menu_armed = true;
        wallpaper_crop_render_pending = true;
    }
    if(wallpaper_crop_play_holding &&
       !wallpaper_crop_play_armed &&
       !TIME_BEFORE(
           current_tick,
           wallpaper_crop_play_hold_start +
               CRAZYPOD_WALLPAPER_CROP_HOLD_TICKS)) {
        wallpaper_crop_play_armed = true;
        wallpaper_crop_render_pending = true;
    }
}

static void process_photo_wheel_touch(void)
{
    struct route_state *state;
    int position;

    if(!product_active || route_depth <= 0) {
        photo_wheel_touch_active = false;
        return;
    }
    state = current_route();
    if(state->route != PHOTOS_ROUTE_DETAIL ||
       photo_zoom_percent <= 100) {
        photo_wheel_touch_active = false;
        return;
    }
#ifdef HAVE_WHEEL_POSITION
    position = wheel_status();
#else
    position = -1;
#endif
    if(position >= 0) {
        int delta;

        position %= 96;
        if(!photo_wheel_touch_active) {
            photo_wheel_touch_active = true;
            photo_wheel_touch_start = position;
            photo_wheel_touch_max_delta = 0;
            return;
        }
        delta = position - photo_wheel_touch_start;
        if(delta < -48)
            delta += 96;
        else if(delta > 48)
            delta -= 96;
        if(delta < 0)
            delta = -delta;
        if(delta > photo_wheel_touch_max_delta)
            photo_wheel_touch_max_delta = delta;
        return;
    }
    if(photo_wheel_touch_active) {
        bool recent_direction_input =
            photo_direction_input_tick != 0 &&
            TIME_BEFORE(
                current_tick,
                photo_direction_input_tick + HZ / 3);

        photo_wheel_touch_active = false;
        if(photo_wheel_touch_start >= 0 &&
           photo_wheel_touch_max_delta <
               CRAZYPOD_PHOTO_TOUCH_MOVE_THRESHOLD &&
           !recent_direction_input) {
            int quadrant =
                ((photo_wheel_touch_start + 12) / 24) & 3;

            if(quadrant == 0)
                queue_photo_pan(0, CRAZYPOD_PHOTO_PAN_STEP);
            else if(quadrant == 1)
                queue_photo_pan(CRAZYPOD_PHOTO_PAN_STEP, 0);
            else if(quadrant == 2)
                queue_photo_pan(0, -CRAZYPOD_PHOTO_PAN_STEP);
            else
                queue_photo_pan(-CRAZYPOD_PHOTO_PAN_STEP, 0);
        }
        photo_wheel_touch_start = -1;
        photo_wheel_touch_max_delta = 0;
    }
}

static void play_wheel_feedback(long button)
{
    long base;

    if(button == BUTTON_NONE || (button & (SYS_EVENT | BUTTON_REL)) != 0)
        return;

    base = main_button_base(button);
    if(base != BUTTON_SCROLL_FWD && base != BUTTON_SCROLL_BACK)
        return;
    if((button & BUTTON_REPEAT) != 0 &&
       !global_settings.keyclick_repeats)
        return;

#if defined(HAVE_HARDWARE_CLICK) && !defined(SIMULATOR)
    if(global_settings.keyclick_hardware)
        piezo_button_beep(false, false);
#endif
    if(global_settings.keyclick)
        system_sound_play(SOUND_KEYCLICK);
}

static bool handle_miniapp_button(long button, intptr_t data)
{
    struct cp_input_event event;
    long base;
    bool repeated;
    bool handled;
    bool ui_refresh;

    if(!product_active || route_depth <= 0 ||
       current_route()->route != MINIAPP_ROUTE_VIEW)
        return false;
    if(button & BUTTON_REL)
        return true;

    repeated = (button & BUTTON_REPEAT) != 0;
    base = main_button_base(button);
    memset(&event, 0, sizeof(event));
    event.struct_size = sizeof(event);
    event.steps = 1;
    event.repeated = repeated ? 1 : 0;

    if(base == BUTTON_SCROLL_FWD) {
        event.type = CP_INPUT_WHEEL_CLOCKWISE;
        event.steps = (uint8_t)wheel_step(data, 4);
    }
    else if(base == BUTTON_SCROLL_BACK) {
        event.type = CP_INPUT_WHEEL_COUNTERCLOCKWISE;
        event.steps = (uint8_t)wheel_step(data, 4);
    }
    else if(base == BUTTON_LEFT)
        event.type = CP_INPUT_LEFT;
    else if(base == BUTTON_RIGHT)
        event.type = CP_INPUT_RIGHT;
    else if(base == BUTTON_SELECT)
        event.type = CP_INPUT_SELECT;
    else if(base == BUTTON_PLAY)
        event.type = CP_INPUT_PLAY;
    else if(base == BUTTON_MENU)
        event.type = CP_INPUT_MENU;
    else
        return true;

    backlight_on();
    if((base == BUTTON_MENU || base == BUTTON_PLAY ||
        base == BUTTON_SELECT) && repeated)
        return true;
    if(event.type == CP_INPUT_WHEEL_CLOCKWISE ||
       event.type == CP_INPUT_WHEEL_COUNTERCLOCKWISE) {
        (void)crazypod_miniapp_input_push_wheel(
            &miniapp_input_queue, &event);
        keep_cpu_boosted(HZ / 10);
        return true;
    }

    /*
     * A button action must apply to the focus that is already visible.
     * Discard wheel intent that has not reached a presented frame yet.
     */
    crazypod_miniapp_input_reset(&miniapp_input_queue);
    handled = crazypod_miniapps_event(&event);
    if(crazypod_miniapps_take_close_request()) {
        pop_route();
        return true;
    }
    if(base == BUTTON_MENU && !handled) {
        pop_route();
        return true;
    }
    ui_refresh = crazypod_miniapps_take_ui_refresh();
    if(handled || ui_refresh) {
        if(ui_refresh)
            crazypod_miniapp_input_reset(&miniapp_input_queue);
        miniapp_render_pending = true;
        keep_cpu_boosted(HZ / 10);
    }
    return true;
}

static bool handle_power_play_hold(long button)
{
    long base;
    bool repeated;

    if((button & SYS_EVENT) != 0)
        return false;

    base = main_button_base(button);
    if(base != BUTTON_PLAY)
        return false;

    if((button & BUTTON_REL) != 0) {
        power_play_holding = false;
        return false;
    }

    repeated = (button & BUTTON_REPEAT) != 0;
    if(!repeated) {
        power_play_holding = true;
        power_play_hold_start = current_tick;
        return false;
    }

    /* Repeats belong to the hold gesture, never to the short-press action. */
    if(!power_play_holding) {
        power_play_holding = true;
        power_play_hold_start = current_tick;
        return true;
    }
    if(TIME_BEFORE(current_tick,
                   power_play_hold_start + CRAZYPOD_POWER_HOLD_TICKS))
        return true;

    power_play_holding = false;
    wallpaper_crop_play_holding = false;
    wallpaper_crop_play_armed = false;
    show_power_prompt();
    return true;
}

static void handle_button(long button, intptr_t data)
{
    long base;
    bool repeated;

    if(button == BUTTON_NONE)
        return;
    if(button & SYS_EVENT) {
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
        if(button == CRAZYPOD_USB_PROMPT_REQUEST) {
            show_usb_prompt((unsigned)data);
        }
        else if(button == CRAZYPOD_USB_PROMPT_DONE) {
            if(usb_prompt_view.root != NULL &&
               usb_prompt_view.request == (unsigned)data)
                dismiss_usb_prompt();
        }
        else
#endif
        if(button == SYS_USB_CONNECTED) {
            if(crazypod_miniapps_is_open())
                close_product();
            usb_storage_active = true;
            music_scan_pending = true;
            music_scan_not_before = current_tick + HZ / 2;
            music_library_loaded = false;
            crazypod_artwork_suspend();
            crazypod_photos_suspend();
            crazypod_videos_suspend();
            crazypod_music_cancel_scan();
            crazypod_state_save(true);
            usb_acknowledge(SYS_USB_CONNECTED_ACK, data);
        }
        else if(button == SYS_USB_DISCONNECTED) {
            usb_storage_active = false;
            crazypod_artwork_resume();
            crazypod_photos_resume();
            crazypod_videos_refresh();
            miniapp_last_error = crazypod_miniapps_rescan();
            music_scan_pending = true;
            music_scan_not_before = current_tick + HZ / 2;
            music_library_loaded = false;
        }
        else if(button == SYS_POWEROFF) {
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
            if(usb_prompt_view.root != NULL)
                finish_usb_prompt(USB_MODE_CHARGE);
#endif
            dismiss_power_prompt();
            execute_power_action(SHUTDOWN_POWER_OFF);
        }
        else if(button == SYS_REBOOT) {
            dismiss_power_prompt();
            execute_power_action(SHUTDOWN_REBOOT);
        }
        return;
    }

    if(power_prompt_visible()) {
        play_wheel_feedback(button);
        if(button & BUTTON_REL)
            return;
        repeated = (button & BUTTON_REPEAT) != 0;
        base = main_button_base(button);
        backlight_on();
        handle_power_prompt_button(base, repeated, data);
        return;
    }

#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    if(usb_prompt_view.root != NULL) {
        play_wheel_feedback(button);
        if(button & BUTTON_REL)
            return;
        repeated = (button & BUTTON_REPEAT) != 0;
        base = main_button_base(button);
        backlight_on();
        handle_usb_prompt_button(base, repeated, data);
        return;
    }
#endif

    if(handle_power_play_hold(button))
        return;

    if(handle_lock_button(button, data))
        return;

    play_wheel_feedback(button);

    if(product_active && route_depth > 0 &&
       current_route()->route == DIY_ROUTE_WALLPAPER_CROP) {
        long crop_base =
            main_button_base(button);
        bool crop_release = (button & BUTTON_REL) != 0;
        bool crop_repeat = (button & BUTTON_REPEAT) != 0;

        if(wallpaper_crop_phase == WALLPAPER_CROP_APPLYING ||
           wallpaper_crop_phase == WALLPAPER_CROP_APPLIED)
            return;
        if(crop_base == BUTTON_SCROLL_FWD ||
           crop_base == BUTTON_SCROLL_BACK) {
            photo_direction_input_tick = current_tick;
            if(!crop_release)
                adjust_wallpaper_crop_zoom(
                    crop_base == BUTTON_SCROLL_FWD ? 1 : -1,
                    wheel_step(data, 12));
            return;
        }
        if(crop_base == BUTTON_LEFT ||
           crop_base == BUTTON_RIGHT) {
            photo_direction_input_tick = current_tick;
            if(!crop_release)
                move_wallpaper_crop(
                    crop_base == BUTTON_RIGHT ? 1 : -1, 0);
            return;
        }
        if(crop_base == BUTTON_SELECT) {
            photo_direction_input_tick = current_tick;
            if(crop_release) {
                bool apply = wallpaper_crop_select_armed;

                wallpaper_crop_select_armed = false;
                if(apply)
                    apply_wallpaper_crop();
            }
            else if(!crop_repeat) {
                wallpaper_crop_select_armed = true;
            }
            return;
        }
        if(crop_base == BUTTON_MENU) {
            photo_direction_input_tick = current_tick;
            if(crop_release) {
                bool cancel = wallpaper_crop_menu_armed;

                wallpaper_crop_menu_holding = false;
                wallpaper_crop_menu_armed = false;
                if(cancel)
                    pop_route();
                else
                    move_wallpaper_crop(0, -1);
            }
            else if(!crop_repeat) {
                wallpaper_crop_menu_holding = true;
                wallpaper_crop_menu_armed = false;
                wallpaper_crop_menu_hold_start = current_tick;
            }
            return;
        }
        if(crop_base == BUTTON_PLAY) {
            photo_direction_input_tick = current_tick;
            if(crop_release) {
                bool reset = wallpaper_crop_play_armed;

                wallpaper_crop_play_holding = false;
                wallpaper_crop_play_armed = false;
                if(reset)
                    reset_wallpaper_crop();
                else
                    move_wallpaper_crop(0, 1);
            }
            else if(!crop_repeat) {
                wallpaper_crop_play_holding = true;
                wallpaper_crop_play_armed = false;
                wallpaper_crop_play_hold_start = current_tick;
            }
            return;
        }
        return;
    }

    if(product_active && route_depth > 0 &&
       (current_route()->route == PHOTOS_ROUTE_MENU ||
        current_route()->route == PHOTOS_ROUTE_LIBRARY ||
        current_route()->route == PHOTOS_ROUTE_FAVORITES ||
        current_route()->route == PHOTOS_ROUTE_DETAIL)) {
        struct route_state *photo_state = current_route();
        long photo_base = main_button_base(button);
        bool photo_detail =
            photo_state->route == PHOTOS_ROUTE_DETAIL;
        bool photo_release = (button & BUTTON_REL) != 0;
        bool photo_repeat = (button & BUTTON_REPEAT) != 0;

        if(photo_detail &&
           (photo_base == BUTTON_SCROLL_FWD ||
            photo_base == BUTTON_SCROLL_BACK)) {
            photo_direction_input_tick = current_tick;
            if(photo_zoom_percent > 100) {
                adjust_photo_zoom(
                    photo_base == BUTTON_SCROLL_FWD ? 1 : -1,
                    wheel_step(data, 12));
            }
            return;
        }
        if(photo_detail &&
           (photo_base == BUTTON_LEFT ||
            photo_base == BUTTON_RIGHT)) {
            int direction =
                photo_base == BUTTON_RIGHT ? 1 : -1;

            photo_direction_input_tick = current_tick;
            if(photo_zoom_percent > 100) {
                if(!photo_release) {
                    queue_photo_pan(
                        -direction * CRAZYPOD_PHOTO_PAN_STEP, 0);
                }
            }
            else if(photo_release && !photo_repeat) {
                move_selection(direction);
            }
            return;
        }
        if(photo_base == BUTTON_SELECT) {
            if(photo_release) {
                bool handled = photo_select_long_handled;
                bool remove_progress =
                    photo_select_holding &&
                    photo_select_hold_percent >= 0;

                photo_select_holding = false;
                photo_select_hold_percent = -1;
                photo_favorite_progress_fill = NULL;
                if(!handled)
                    activate_selected();
                else if(remove_progress)
                    render_current_route(false);
                photo_select_long_handled = false;
            }
            else if(!photo_repeat) {
                int photo_index = selected_photo_index(photo_state);

                photo_select_long_handled = false;
                photo_select_holding = photo_index >= 0;
                photo_select_hold_start = current_tick;
                photo_select_hold_percent = -1;
            }
            return;
        }
        if(photo_detail && photo_base == BUTTON_MENU) {
            photo_direction_input_tick = current_tick;
            if(photo_zoom_percent > 100) {
                if(!photo_release) {
                    queue_photo_pan(
                        0, CRAZYPOD_PHOTO_PAN_STEP);
                }
            }
            else if(photo_release && !photo_repeat) {
                pop_route();
            }
            return;
        }
        if(photo_detail && photo_base == BUTTON_PLAY) {
            photo_direction_input_tick = current_tick;
            if(photo_zoom_percent > 100) {
                if(!photo_release) {
                    queue_photo_pan(
                        0, -CRAZYPOD_PHOTO_PAN_STEP);
                }
            }
            else if(photo_release && !photo_repeat &&
                    crazypod_wallpaper_select(
                        CRAZYPOD_WALLPAPER_HOME,
                        crazypod_photo_path(photo_state->group))) {
                refresh_desktop_appearance();
            }
            return;
        }
    }

    if(handle_miniapp_button(button, data))
        return;

    if(button & BUTTON_REL)
        return;
    repeated = (button & BUTTON_REPEAT) != 0;
    base = main_button_base(button);
    backlight_on();

    if(product_active && route_depth > 0 && music_scan_screen) {
        if(base == BUTTON_MENU && !repeated)
            close_product();
        return;
    }

    if(product_active && route_depth > 0 &&
       base == BUTTON_SELECT && repeated) {
        struct route_state *state = current_route();

        if(state->route == NOTES_ROUTE_DELETE_CONFIRM) {
            crazypod_note_move_to_trash((uint32_t)state->group);
            route_depth = 1;
            route_stack[0].route = NOTES_ROUTE_MENU;
            route_stack[0].selected = 0;
            route_stack[0].group = -1;
            render_current_route(true);
            return;
        }
        if(state->route == NOTES_ROUTE_PERMANENT_CONFIRM) {
            crazypod_note_delete_forever((uint32_t)state->group);
            route_depth = 2;
            route_stack[1].route = NOTES_ROUTE_DELETED;
            route_stack[1].selected = 0;
            route_stack[1].group = -1;
            render_current_route(true);
            return;
        }
        if(state->route == NOTES_ROUTE_EMPTY_TRASH_CONFIRM) {
            crazypod_notes_empty_trash();
            route_depth = 2;
            route_stack[1].route = NOTES_ROUTE_DELETED;
            route_stack[1].selected = 0;
            route_stack[1].group = -1;
            render_current_route(true);
            return;
        }
        if(state->route == NOTES_ROUTE_DISCARD_CONFIRM) {
            crazypod_note_draft_clear();
            note_draft_available = false;
            note_draft_save_pending = false;
            memset(&note_editor, 0, sizeof(note_editor));
            route_depth -= 3;
            if(route_depth < 1)
                route_depth = 1;
            render_current_route(true);
            return;
        }
        if(state->route == BOOKS_ROUTE_DELETE_CONFIRM) {
            if(crazypod_book_delete(state->group)) {
                books_metadata_ready = false;
                selected_book_index = -1;
                book_page_text[0] = '\0';
                open_books();
            }
            else
                render_current_route(false);
            return;
        }
        if(state->route == CALENDAR_ROUTE_DELETE_CONFIRM) {
            const struct crazypod_calendar_event *event =
                crazypod_calendar_event_find(
                    (uint32_t)state->group);
            int date = event != NULL
                ? event->date : calendar_today_date();
            if(crazypod_calendar_event_delete(
                   (uint32_t)state->group)) {
                show_calendar_day(date);
                render_current_route(true);
            }
            else
                render_current_route(false);
            return;
        }
        if(state->route == WORKOUT_ROUTE_FINISH_CONFIRM) {
            struct tm *now = get_time();
            uint32_t seconds;
            int date;

            if(workout_running) {
                workout_accumulated_ticks +=
                    current_tick - workout_started_at;
                workout_running = false;
            }
            seconds = (uint32_t)(
                workout_accumulated_ticks / HZ);
            if(seconds == 0)
                seconds = 1;
            date = (now->tm_year + 1900) * 10000 +
                   (now->tm_mon + 1) * 100 + now->tm_mday;
            if(crazypod_workout_add(
                   workout_activity, date, seconds) != 0) {
                workout_accumulated_ticks = 0;
                route_depth = 1;
                route_stack[0].route = WORKOUT_ROUTE_MENU;
                route_stack[0].selected = 1;
                route_stack[0].group = -1;
                render_current_route(true);
            }
            else
                render_current_route(false);
            return;
        }
        if(state->route == WORKOUT_ROUTE_DELETE_CONFIRM) {
            if(crazypod_workout_delete(
                   (uint32_t)state->group)) {
                route_depth = 2;
                route_stack[0].route = WORKOUT_ROUTE_MENU;
                route_stack[0].selected = 1;
                route_stack[0].group = -1;
                route_stack[1].route = WORKOUT_ROUTE_HISTORY;
                route_stack[1].selected = 0;
                route_stack[1].group = -1;
                render_current_route(true);
            }
            else
                render_current_route(false);
            return;
        }
    }

    if(!product_active) {
        if(base == BUTTON_SCROLL_FWD) {
            move_desktop_selection(1);
        }
        else if(base == BUTTON_SCROLL_BACK) {
            move_desktop_selection(-1);
        }
        else if(base == BUTTON_RIGHT && !repeated) {
            if(crazypod_queue_count() > 0)
                audio_next();
        }
        else if(base == BUTTON_LEFT && !repeated)
            previous_or_restart_track();
        else if(base == BUTTON_SELECT) {
            struct crazypod_app *app = visible_app(selected_app);
            if(app != NULL)
                open_app(app->id);
        }
        else if(base == BUTTON_PLAY && !repeated)
            toggle_playback();
        return;
    }

    if(route_depth <= 0) {
        if(base == BUTTON_MENU)
            close_product();
        return;
    }

    if(current_route()->route == SETTINGS_ROUTE_EQ_STUDIO) {
        if(base == BUTTON_SCROLL_FWD)
            eq_studio_adjust(wheel_step(data, 8));
        else if(base == BUTTON_SCROLL_BACK)
            eq_studio_adjust(-wheel_step(data, 8));
        else if(base == BUTTON_RIGHT)
            eq_studio_select_band(1);
        else if(base == BUTTON_LEFT)
            eq_studio_select_band(-1);
        else if(base == BUTTON_SELECT && !repeated) {
            eq_studio_editing = !eq_studio_editing;
            render_current_route(false);
        }
        else if(base == BUTTON_PLAY && !repeated) {
            if(eq_studio_editing)
                eq_studio_cycle_mode();
            else
                eq_studio_toggle_enabled();
        }
        else if(base == BUTTON_MENU && !repeated) {
            eq_studio_editing = false;
            crazypod_state_save(false);
            pop_route();
        }
        return;
    }

    if(current_route()->route == BOOKS_ROUTE_READER) {
        if(base == BUTTON_SCROLL_FWD) {
            int count = wheel_step(data, 12);
            while(count-- > 0)
                turn_book_page(1);
        }
        else if(base == BUTTON_SCROLL_BACK) {
            int count = wheel_step(data, 12);
            while(count-- > 0)
                turn_book_page(-1);
        }
        else if(base == BUTTON_RIGHT)
            turn_book_page(1);
        else if(base == BUTTON_LEFT)
            turn_book_page(-1);
        else if(base == BUTTON_SELECT && !repeated)
            activate_selected();
        else if(base == BUTTON_PLAY && !repeated) {
            crazypod_book_toggle_bookmark(
                selected_book_index, book_page_offset);
            render_current_route(false);
        }
        else if(base == BUTTON_MENU && !repeated)
            pop_route();
        return;
    }

    if(current_route()->route == WORKOUT_ROUTE_ACTIVE) {
        if(base == BUTTON_SELECT && !repeated)
            activate_selected();
        else if((base == BUTTON_PLAY || base == BUTTON_MENU) &&
                !repeated) {
            if(workout_running) {
                workout_accumulated_ticks +=
                    current_tick - workout_started_at;
                workout_running = false;
            }
            push_route(WORKOUT_ROUTE_FINISH_CONFIRM, -1);
        }
        return;
    }

    if(current_route()->route == STOPWATCH_ROUTE_VIEW) {
        if((base == BUTTON_SCROLL_FWD ||
            base == BUTTON_SCROLL_BACK) &&
           stopwatch_lap_count == 0) {
            int direction = base == BUTTON_SCROLL_FWD
                ? wheel_step(data, 12)
                : -wheel_step(data, 12);
            stopwatch_style_index =
                (stopwatch_style_index + direction) % 3;
            if(stopwatch_style_index < 0)
                stopwatch_style_index += 3;
            render_current_route(false);
        }
        else if(base == BUTTON_SELECT && !repeated)
            activate_selected();
        else if(base == BUTTON_RIGHT && !repeated &&
                stopwatch_running &&
                stopwatch_lap_count <
                    (int)(sizeof(stopwatch_laps) /
                          sizeof(stopwatch_laps[0]))) {
            stopwatch_laps[stopwatch_lap_count++] =
                stopwatch_elapsed_ticks();
            render_current_route(false);
        }
        else if((base == BUTTON_LEFT || base == BUTTON_PLAY) &&
                !repeated && !stopwatch_running &&
                (stopwatch_accumulated_ticks > 0 ||
                 stopwatch_lap_count > 0)) {
            if(TIME_BEFORE(current_tick,
                           stopwatch_reset_armed_until)) {
                memset(stopwatch_laps, 0, sizeof(stopwatch_laps));
                stopwatch_lap_count = 0;
                stopwatch_running = false;
                stopwatch_started_at = 0;
                stopwatch_accumulated_ticks = 0;
                stopwatch_reset_armed_until = 0;
            }
            else
                stopwatch_reset_armed_until =
                    current_tick + HZ * 3 / 2;
            stopwatch_last_render_tick = current_tick;
            render_current_route(false);
        }
        else if(base == BUTTON_PLAY && !repeated &&
                stopwatch_running) {
            if(stopwatch_lap_count <
               (int)(sizeof(stopwatch_laps) /
                     sizeof(stopwatch_laps[0])))
                stopwatch_laps[stopwatch_lap_count++] =
                    stopwatch_elapsed_ticks();
            render_current_route(false);
        }
        else if(base == BUTTON_MENU && !repeated) {
            stopwatch_reset_armed_until = 0;
            pop_route();
        }
        return;
    }

    if(current_route()->route == CALENDAR_ROUTE_MONTH) {
        if(base == BUTTON_SCROLL_FWD) {
            int count = wheel_step(data, 12);
            while(count-- > 0)
                calendar_move_focus(1);
            render_current_route(false);
        }
        else if(base == BUTTON_SCROLL_BACK) {
            int count = wheel_step(data, 12);
            while(count-- > 0)
                calendar_move_focus(-1);
            render_current_route(false);
        }
        else if(base == BUTTON_RIGHT && !repeated) {
            calendar_move_focus(1);
            render_current_route(false);
        }
        else if(base == BUTTON_LEFT && !repeated) {
            calendar_move_focus(-1);
            render_current_route(false);
        }
        else if(base == BUTTON_SELECT && !repeated)
            activate_selected();
        else if(base == BUTTON_PLAY && !repeated) {
            struct tm *now = get_time();
            calendar_focus_year = now->tm_year + 1900;
            calendar_focus_month = now->tm_mon;
            calendar_focus_day = now->tm_mday;
            render_current_route(false);
        }
        else if(base == BUTTON_MENU && !repeated)
            pop_route();
        return;
    }

    if(current_route()->route == CALENDAR_ROUTE_EDITOR) {
        struct route_state *state = current_route();
        int direction = 0;

        if(base == BUTTON_SCROLL_FWD)
            move_selection(wheel_step(data, 12));
        else if(base == BUTTON_SCROLL_BACK)
            move_selection(-wheel_step(data, 12));
        else if(base == BUTTON_RIGHT && !repeated)
            direction = 1;
        else if(base == BUTTON_LEFT && !repeated)
            direction = -1;
        else if(base == BUTTON_SELECT && !repeated)
            activate_selected();
        else if(base == BUTTON_MENU && !repeated)
            pop_route();

        if(direction != 0 && state->selected == 1) {
            calendar_editor_error = 0;
            calendar_editor_date = calendar_shifted_date(
                calendar_editor_date, direction);
            render_current_route(false);
        }
        else if(direction != 0 && state->selected == 2) {
            calendar_editor_error = 0;
            if(direction > 0)
                calendar_editor_minutes =
                    calendar_editor_minutes < 0
                        ? 9 * 60
                        : (calendar_editor_minutes + 30) %
                            (24 * 60);
            else if(calendar_editor_minutes <= 0)
                calendar_editor_minutes = -1;
            else
                calendar_editor_minutes -= 30;
            render_current_route(false);
        }
        return;
    }

    if(current_route()->route == CALENDAR_ROUTE_TITLE_EDITOR) {
        if(base == BUTTON_SCROLL_FWD)
            move_selection(wheel_step(data, 12));
        else if(base == BUTTON_SCROLL_BACK)
            move_selection(-wheel_step(data, 12));
        else if(base == BUTTON_RIGHT && !repeated) {
            editor_move_cursor(
                calendar_editor_summary,
                &calendar_editor_cursor, 1);
            render_current_route(false);
        }
        else if(base == BUTTON_LEFT && !repeated) {
            editor_move_cursor(
                calendar_editor_summary,
                &calendar_editor_cursor, -1);
            render_current_route(false);
        }
        else if(base == BUTTON_SELECT && !repeated)
            activate_selected();
        else if(base == BUTTON_MENU && !repeated)
            pop_route();
        return;
    }

    if(current_route()->route == NOTES_ROUTE_COMPOSER) {
        char *target = note_editor_body_active
            ? note_editor.body : note_editor.title;
        size_t *cursor = note_editor_body_active
            ? &note_editor_body_cursor : &note_editor_title_cursor;

        if(base == BUTTON_SCROLL_FWD)
            move_selection(wheel_step(data, 12));
        else if(base == BUTTON_SCROLL_BACK)
            move_selection(-wheel_step(data, 12));
        else if(base == BUTTON_RIGHT && !repeated) {
            editor_move_cursor(target, cursor, 1);
            render_current_route(false);
        }
        else if(base == BUTTON_LEFT && !repeated) {
            editor_move_cursor(target, cursor, -1);
            render_current_route(false);
        }
        else if(base == BUTTON_SELECT && !repeated)
            activate_selected();
        else if(base == BUTTON_PLAY && !repeated) {
            note_editor_body_active = !note_editor_body_active;
            render_current_route(false);
        }
        else if(base == BUTTON_MENU && !repeated) {
            if(note_editor_dirty())
                push_route(NOTES_ROUTE_EXIT_ACTIONS, -1);
            else
                pop_route();
        }
        return;
    }

    if(current_route()->route == MUSIC_ROUTE_SEARCH) {
        if(base == BUTTON_SCROLL_FWD)
            move_selection(wheel_step(data, 12));
        else if(base == BUTTON_SCROLL_BACK)
            move_selection(-wheel_step(data, 12));
        else if(base == BUTTON_RIGHT)
            move_selection(1);
        else if(base == BUTTON_LEFT)
            move_selection(-1);
        else if(base == BUTTON_SELECT && !repeated)
            activate_selected();
        else if(base == BUTTON_MENU && !repeated) {
            if(search_query[0] != '\0') {
                editor_backspace(search_query);
                render_current_route(false);
            }
            else {
                pop_route();
            }
        }
        else if(base == BUTTON_PLAY && !repeated &&
                search_query[0] != '\0' &&
                crazypod_music_search_count(search_query) > 0) {
            push_route(MUSIC_ROUTE_SEARCH_RESULTS, -1);
        }
        return;
    }

    if(current_route()->route == NOTES_ROUTE_SEARCH) {
        if(base == BUTTON_SCROLL_FWD)
            move_selection(wheel_step(data, 12));
        else if(base == BUTTON_SCROLL_BACK)
            move_selection(-wheel_step(data, 12));
        else if(base == BUTTON_RIGHT)
            move_selection(1);
        else if(base == BUTTON_LEFT)
            move_selection(-1);
        else if(base == BUTTON_SELECT && !repeated)
            activate_selected();
        else if(base == BUTTON_MENU && !repeated) {
            if(note_search_query[0] != '\0') {
                editor_backspace(note_search_query);
                render_current_route(false);
            }
            else
                pop_route();
        }
        else if(base == BUTTON_PLAY && !repeated &&
                note_search_query[0] != '\0')
            push_route(NOTES_ROUTE_SEARCH_RESULTS, -1);
        return;
    }

    if(choice_overlay.kind != CHOICE_OVERLAY_NONE) {
        if(base == BUTTON_SCROLL_FWD)
            move_choice_overlay(wheel_step(data, 12));
        else if(base == BUTTON_SCROLL_BACK)
            move_choice_overlay(-wheel_step(data, 12));
        else if(base == BUTTON_RIGHT)
            move_choice_overlay(1);
        else if(base == BUTTON_LEFT)
            move_choice_overlay(-1);
        else if(base == BUTTON_SELECT) {
            if(!repeated)
                activate_choice_overlay();
        }
        else if(base == BUTTON_MENU && !repeated)
            dismiss_choice_overlay(true);
        return;
    }

    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING &&
       now_overlay != NOW_OVERLAY_NONE &&
       base == BUTTON_MENU && !repeated) {
        dismiss_now_overlay(true);
        return;
    }

#ifdef HAVE_WHEEL_POSITION
    if(current_route()->route == MUSIC_ROUTE_ALBUM_FLOW &&
       (base == BUTTON_SCROLL_FWD ||
        base == BUTTON_SCROLL_BACK))
        return;
#endif
    if(base == BUTTON_SCROLL_FWD)
        move_selection(is_skeuomorphic_preview_route(
                           current_route()->route)
                       ? 1
                       : wheel_step(
                             data,
                             current_route()->route ==
                                     MUSIC_ROUTE_NOW_PLAYING
                                 ? 1
                                 : current_route()->route ==
                                           MUSIC_ROUTE_ALBUM_FLOW
                                       ? 15
                                       : 12));
    else if(base == BUTTON_SCROLL_BACK)
        move_selection(is_skeuomorphic_preview_route(
                           current_route()->route)
                       ? -1
                       : -wheel_step(
                             data,
                             current_route()->route ==
                                     MUSIC_ROUTE_NOW_PLAYING
                                 ? 1
                                 : current_route()->route ==
                                           MUSIC_ROUTE_ALBUM_FLOW
                                       ? 15
                                       : 12));
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
    else if(base == BUTTON_PLAY) {
        if(current_route()->route == NOTES_ROUTE_COMPOSER &&
           !repeated) {
            note_editor_body_active = !note_editor_body_active;
            render_current_route(false);
        }
        else
            toggle_playback();
    }
}

static void service_stopwatch(void)
{
    if(!product_active || route_depth <= 0 ||
       current_route()->route != STOPWATCH_ROUTE_VIEW)
        return;
    if(stopwatch_reset_armed_until != 0 &&
       !TIME_BEFORE(current_tick, stopwatch_reset_armed_until)) {
        stopwatch_reset_armed_until = 0;
        render_current_route(false);
    }
    if(!stopwatch_running ||
       TIME_BEFORE(current_tick, stopwatch_last_render_tick + HZ / 10))
        return;
    stopwatch_last_render_tick = current_tick;
    render_current_route(false);
}

static void service_workout(void)
{
    if(!product_active || route_depth <= 0 ||
       current_route()->route != WORKOUT_ROUTE_ACTIVE ||
       !workout_running ||
       TIME_BEFORE(current_tick,
                   workout_last_render_tick + HZ / 10))
        return;
    workout_last_render_tick = current_tick;
    render_current_route(false);
}

static void service_clock_routes(void)
{
    enum crazypod_route route;
    long interval;

    if(!product_active || route_depth <= 0)
        return;
    route = current_route()->route;
    if(route != CLOCK_ROUTE_VIEW &&
       route != CLOCK_ROUTE_SLEEP_TIMER)
        return;
    interval = route == CLOCK_ROUTE_VIEW
        ? (HZ / 4 > 0 ? HZ / 4 : 1) : HZ;
    if(TIME_BEFORE(current_tick,
                   clock_last_render_tick + interval))
        return;
    clock_last_render_tick = current_tick;
    render_current_route(false);
}

static void service_miniapps(void)
{
    struct cp_input_event input_event;
    bool service_due =
        miniapp_last_service_tick == 0 ||
        !TIME_BEFORE(
            current_tick, miniapp_last_service_tick + HZ);

    if(screen_locked || !product_active || route_depth <= 0 ||
       current_route()->route != MINIAPP_ROUTE_VIEW ||
       !crazypod_miniapps_is_open()) {
        crazypod_miniapp_input_reset(&miniapp_input_queue);
    }
    else if(crazypod_miniapp_input_next(
                &miniapp_input_queue,
                crazypod_frameclock_due(&lvgl_clock, current_tick),
                &input_event)) {
        if(crazypod_miniapps_event(&input_event))
            miniapp_render_pending = true;
    }

    if(service_due) {
        struct crazypod_miniapp_alarm alarm;

        miniapp_last_service_tick = current_tick;
        if(crazypod_miniapps_alarm_service(&alarm)) {
            snprintf(miniapp_alert_id, sizeof(miniapp_alert_id),
                     "%s", alarm.id);
            miniapp_alert_deadline = alarm.deadline_epoch;
            miniapp_alert_token = alarm.token;
            miniapp_alert_remaining = 3;
            miniapp_alert_next_tick = current_tick;
            miniapp_alert_delivery_pending = true;
        }
        if(crazypod_miniapps_tick())
            miniapp_render_pending = true;
    }

    if(crazypod_miniapps_take_close_request()) {
        crazypod_miniapp_input_reset(&miniapp_input_queue);
        pop_route();
        return;
    }
    if(crazypod_miniapps_take_ui_refresh()) {
        crazypod_miniapp_input_reset(&miniapp_input_queue);
        miniapp_render_pending = true;
    }

    if(miniapp_alert_remaining > 0 &&
       !TIME_BEFORE(current_tick, miniapp_alert_next_tick)) {
#if defined(HAVE_HARDWARE_CLICK) && !defined(SIMULATOR)
        piezo_button_beep(false, false);
#endif
        beep_play(1568, 90, 6000);
        if(miniapp_alert_delivery_pending &&
           crazypod_miniapps_alarm_delivery_acknowledge(
               miniapp_alert_id, miniapp_alert_deadline,
               miniapp_alert_token) == CRAZYPOD_MINIAPP_OK)
            miniapp_alert_delivery_pending = false;
        --miniapp_alert_remaining;
        miniapp_alert_next_tick = current_tick + HZ / 3;
    }

    if(miniapp_render_pending && !screen_locked &&
       product_active && route_depth > 0 &&
       current_route()->route == MINIAPP_ROUTE_VIEW) {
        miniapp_render_pending = false;
        render_current_route(false);
    }
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
    if(!screen_locked &&
       !product_active &&
       !modal_prompt_visible() &&
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
        if(!screen_locked && crazypod_coverflow_active()) {
            /*
             * CoverFlow presents the complete screen after its native
             * compositor runs. Holding LVGL's partial update here prevents
             * metadata and covers from reaching the panel as separate frames.
             */
            crazypod_coverflow_invalidate();
        }
        else {
            crazypod_present_queue_rect(dirty_x1, dirty_y1,
                                        dirty_x2 - dirty_x1 + 1,
                                        dirty_y2 - dirty_y1 + 1);
        }
        dirty_valid = false;
    }

    lv_display_flush_ready(display);
}

static void process_deferred_route_render(void)
{
    if(route_render_pending &&
       !TIME_BEFORE(current_tick, route_render_due)) {
        if(product_active && route_depth > 0)
            render_current_route(false);
        else
            route_render_pending = false;
    }
    if(menu_preview_pending &&
       !TIME_BEFORE(current_tick, menu_preview_due)) {
        if(product_active && route_depth > 0 &&
           menu_view.valid &&
           menu_view.route == current_route()->route) {
            menu_preview_pending = false;
            if(menu_preview_motion_ready &&
               is_skeuomorphic_preview_route(
                   current_route()->route) &&
               menu_preview_content != NULL)
                start_menu_preview_scene_exit();
            else
                render_menu_preview(current_route(), false);
        }
        else
            menu_preview_pending = false;
    }
    if(menu_preview_media_refresh_pending &&
       !TIME_BEFORE(current_tick, menu_preview_media_due)) {
        if(product_active && route_depth > 0 &&
           menu_view.valid &&
           menu_view.route == current_route()->route &&
           !menu_preview_pending &&
           !menu_preview_motion_active()) {
            menu_preview_media_refresh_pending = false;
            render_menu_preview(current_route(), false);
        }
        else if(!product_active || route_depth <= 0 ||
                !menu_view.valid ||
                menu_view.route != current_route()->route)
            menu_preview_media_refresh_pending = false;
    }
}

static void sync_album_flow_metadata(void)
{
    struct route_state *state;
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
    state->selected = album_index;
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

static void service_album_flow_warm(void)
{
    static long last_warm;
    struct route_state *state;

    if(!product_active || screen_locked || route_depth <= 0 ||
       crazypod_coverflow_active() ||
       menu_preview_pending || menu_preview_motion_active())
        return;
    state = current_route();
    if(state->route != MUSIC_ROUTE_MENU || state->selected != 1)
        return;
    if(last_warm != 0 &&
       TIME_BEFORE(current_tick, last_warm + HZ / 10))
        return;
    last_warm = current_tick;
    (void)crazypod_coverflow_warm(initial_album_index());
}

#ifdef SIMULATOR
static bool simulator_prepare_snapshot(void)
{
    const char *screen = getenv("CRAZYPOD_SIM_SCREEN");
    int preview_index;

    if(getenv("CRAZYPOD_SIM_DUMP") == NULL)
        return false;
    if(screen == NULL || strcmp(screen, "home") == 0)
        return true;
    if(strcmp(screen, "power") == 0)
        show_power_prompt();
    else if(sscanf(screen, "music-%d", &preview_index) == 1) {
        open_music();
        if(route_depth > 0 &&
           current_route()->route == MUSIC_ROUTE_MENU) {
            int count = route_item_count(current_route());
            if(preview_index < 0)
                preview_index = 0;
            if(preview_index >= count)
                preview_index = count - 1;
            current_route()->selected = preview_index;
            render_current_route(false);
        }
    }
    else if(sscanf(screen, "play-video-%d", &preview_index) == 1) {
        int count;

        open_photos();
        push_route(PHOTOS_ROUTE_VIDEOS, -1);
        count = route_item_count(current_route());
        if(preview_index < 0)
            preview_index = 0;
        if(preview_index >= count)
            preview_index = count > 0 ? count - 1 : 0;
        current_route()->selected = preview_index;
        render_current_route(false);
        lv_refr_now(NULL);
        crazypod_present_now();
        if(count > 0) {
            enum crazypod_video_result video_result =
                crazypod_video_play(preview_index);

            fprintf(stderr, "CrazyPod video smoke: %d (%s)\n",
                    video_result,
                    crazypod_video_result_message(video_result));
        }
        render_current_route(false);
        return false;
    }
    else if(sscanf(screen, "videos-%d", &preview_index) == 1) {
        int count;

        open_photos();
        push_route(PHOTOS_ROUTE_VIDEOS, -1);
        count = route_item_count(current_route());
        if(preview_index < 0)
            preview_index = 0;
        if(preview_index >= count)
            preview_index = count > 0 ? count - 1 : 0;
        current_route()->selected = preview_index;
        render_current_route(false);
    }
    else if(sscanf(screen, "media-%d", &preview_index) == 1 ||
            sscanf(screen, "photos-%d", &preview_index) == 1) {
        open_photos();
        if(preview_index < 0)
            preview_index = 0;
        if(preview_index > 2)
            preview_index = 2;
        current_route()->selected = preview_index;
        render_current_route(false);
    }
    else if(sscanf(screen, "books-%d", &preview_index) == 1) {
        open_books();
        {
            int count = route_item_count(current_route());
            if(preview_index < 0)
                preview_index = 0;
            if(preview_index >= count)
                preview_index = count - 1;
            current_route()->selected = preview_index;
            render_current_route(false);
        }
    }
    else if(strcmp(screen, "notes-new") == 0) {
        open_notes();
        current_route()->selected = 0;
        render_current_route(false);
    }
    else if(strcmp(screen, "notes-draft") == 0) {
        open_notes();
        current_route()->selected = note_draft_available ? 1 : 0;
        render_current_route(false);
    }
    else if(strcmp(screen, "notes-item") == 0) {
        open_notes();
        current_route()->selected = crazypod_notes_count(false) > 0
            ? notes_home_note_start() : 0;
        render_current_route(false);
    }
    else if(strcmp(screen, "notes-search") == 0) {
        open_notes();
        current_route()->selected = notes_home_search_index();
        render_current_route(false);
    }
    else if(strcmp(screen, "notes-deleted") == 0) {
        open_notes();
        current_route()->selected = notes_home_deleted_index();
        render_current_route(false);
    }
    else if(strcmp(screen, "more") == 0)
        open_extras();
    else if(strcmp(screen, "more-second") == 0) {
        open_extras();
        if(crazypod_apps_hidden_count() > 1) {
            current_route()->selected = 1;
            render_current_route(false);
        }
    }
    else if(strcmp(screen, "settings-main-menu") == 0)
        open_root_route(SETTINGS_ROUTE_MAIN_MENU);
    else if(strcmp(screen, "settings-reduce-motion") == 0) {
        open_root_route(SETTINGS_ROUTE_DISPLAY);
        current_route()->selected =
            route_item_count(current_route()) - 1;
        render_current_route(false);
    }
    else if(strcmp(screen, "notes") == 0)
        open_notes();
    else if(strcmp(screen, "note-compose") == 0) {
        open_notes();
        begin_note_composer(0, false);
    }
    else if(strcmp(screen, "books") == 0)
        open_books();
    else if(strcmp(screen, "books-reading") == 0) {
        open_books();
        current_route()->selected =
            (books_has_continue() ? 1 : 0) + 4;
        render_current_route(false);
    }
    else if(strcmp(screen, "book-reader") == 0) {
        open_books();
        if(crazypod_books_count() > 0)
            begin_book_reader(0, 0);
    }
    else if(strcmp(screen, "book-reader-next") == 0) {
        open_books();
        if(crazypod_books_count() > 0) {
            begin_book_reader(0, 0);
            turn_book_page(1);
        }
    }
    else if(strcmp(screen, "clock") == 0)
        open_root_route(CLOCK_ROUTE_VIEW);
    else if(strcmp(screen, "stopwatch") == 0)
        open_root_route(STOPWATCH_ROUTE_VIEW);
    else if(strcmp(screen, "workouts") == 0)
        open_root_route(WORKOUT_ROUTE_MENU);
    else if(strcmp(screen, "workout-ready") == 0) {
        workout_activity = 0;
        open_root_route(WORKOUT_ROUTE_READY);
    }
    else if(strcmp(screen, "workout-active") == 0) {
        workout_activity = 0;
        workout_accumulated_ticks = 62 * HZ;
        workout_started_at = current_tick;
        workout_running = true;
        open_root_route(WORKOUT_ROUTE_ACTIVE);
    }
    else if(strcmp(screen, "workout-detail") == 0) {
        open_root_route(WORKOUT_ROUTE_HISTORY);
        if(crazypod_workouts_count() > 0)
            push_route(WORKOUT_ROUTE_DETAIL, 0);
    }
    else if(strcmp(screen, "calendar") == 0) {
        open_calendar();
        push_route(CALENDAR_ROUTE_MONTH, -1);
    }
    else if(strcmp(screen, "calendar-day") == 0) {
        open_calendar();
        show_calendar_day(calendar_focus_date());
        render_current_route(true);
    }
    else if(strcmp(screen, "contacts") == 0)
        open_contacts();
    else if(strcmp(screen, "contact-detail") == 0) {
        open_contacts();
        if(crazypod_contacts_count() > 0)
            push_route(CONTACTS_ROUTE_DETAIL, 0);
    }
    else if(strcmp(screen, "calculator") == 0 ||
            strcmp(screen, "pomodoro") == 0) {
        int app_index;

        open_utilities();
        app_index = crazypod_miniapps_find(screen);
        if(app_index < 0)
            return false;
        current_route()->selected = app_index;
        activate_selected();
        if(current_route()->route != MINIAPP_ROUTE_VIEW)
            return false;
    }
    else
        return false;
    return true;
}
#endif

void crazypod_ui_run(void)
{
    lv_display_t *display;
    lv_obj_t *boot_screen;
#ifdef SIMULATOR
    bool simulator_snapshot_pending;
    long simulator_snapshot_due = 0;
    int simulator_snapshot_stage = 0;
#endif

    lcd_set_viewport(NULL);
#ifdef HAVE_SW_POWEROFF
    /*
     * SYS_POWEROFF is a committed shutdown broadcast. CrazyPod owns the
     * Play-button hold gesture so opening its confirmation UI cannot wake and
     * reinitialize the iPod LCD through the Rockbox backlight thread.
     */
    button_set_sw_poweroff_state(false);
#endif
    lv_init();
    lv_tick_set_cb(rockbox_tick_ms);
    crazypod_present_init(current_tick);
    crazypod_frameclock_reset(&lvgl_clock, current_tick);
    crazypod_frameclock_reset(&desktop_motion_clock, current_tick);
    last_desktop_capsule_spectrum_tick = current_tick;
    desktop_capsule_spectrum_phase = 0;
    desktop_capsule_spectrum_playing_seen = false;
    crazypod_image_init();
    crazypod_artwork_init();
    crazypod_icons_init();
    crazypod_photos_init();
    crazypod_videos_init();
    crazypod_wallpaper_init();
    miniapp_last_error = crazypod_miniapps_init();

    display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, display_flush);
    lv_display_set_antialiasing(display, true);

    boot_screen = create_boot_screen();
    create_desktop();
    create_product_screen();
    create_lock_screen();
    update_status_bars(NULL);
    lv_timer_create(update_status_bars, 1000, NULL);
    lv_timer_create(update_playback_ui, 250, NULL);
    lv_timer_create(update_persistent_state, 1000, NULL);

    lv_screen_load(boot_screen);
    lv_refr_now(display);
    crazypod_present_tick();
    refresh_desktop_capsule_material();
    lv_screen_load_anim(desktop_screen, LV_SCREEN_LOAD_ANIM_FADE_IN,
                        CRAZYPOD_BOOT_FADE_DURATION_MS, 0, true);
    lv_refr_now(display);
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    usb_prompt_ui_ready = true;
#endif
    set_cpu_boost(true);
    boost_until = current_tick + HZ / 2;
    preview_artwork_generation_seen =
        crazypod_artwork_slot_generation(CRAZYPOD_PREVIEW_ARTWORK_SLOT);
    menu_preview_artwork_generation_seen =
        menu_preview_artwork_signature();
    now_artwork_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);
    now_prefetch_artwork_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT);
    capsule_artwork_generation_seen =
        crazypod_artwork_slot_generation(CRAZYPOD_CAPSULE_ARTWORK_SLOT);
    photo_generation_seen = crazypod_photo_generation();
    photo_view_generation_seen =
        crazypod_photo_view_generation();
    video_generation_seen = crazypod_video_generation();

#ifdef SIMULATOR
    simulator_snapshot_pending =
        getenv("CRAZYPOD_SIM_DUMP") != NULL;
    if(simulator_snapshot_pending)
        simulator_snapshot_due = current_tick + HZ / 2;
#endif

    music_scan_generation_seen = crazypod_music_scan_generation();
    music_library_loaded = false;
    music_scan_pending = true;
    usb_storage_active = false;
    music_scan_not_before = current_tick + HZ;
    lock_backlight_was_on = is_backlight_on(false);
    screen_locked = false;
    lock_release_guard = false;

    while(true) {
        long button;
        int drained = 0;

        process_lock_state();
        button = button_get_w_tmo(1);
        while(button != BUTTON_NONE && drained < 16) {
            intptr_t data = button_get_data();
            handle_button(button, data);
            ++drained;
            button = button_get_w_tmo(0);
        }
        process_lock_state();
        if(!screen_locked) {
            process_photo_favorite_hold();
            process_photo_wheel_touch();
            process_photo_pan_render();
            process_wallpaper_crop_state();
            process_wallpaper_crop_loading_progress();
            process_wallpaper_crop_render();
        }
        service_music_scan();
        service_note_editor_draft();
        service_stopwatch();
        service_workout();
        service_clock_routes();
        service_miniapps();
        process_deferred_route_render();
        service_album_flow_warm();
        process_pending_now_playing_open();
        process_artwork_updates();
        process_photo_updates();
        process_video_updates();
        if(!screen_locked) {
            tick_desktop_carousel();
            tick_desktop_capsule_spectrum();
            tick_now_playing_wave();
        }
        if(crazypod_frameclock_due(&lvgl_clock, current_tick)) {
            lv_timer_handler();
            crazypod_frameclock_schedule_next(&lvgl_clock, current_tick);
        }
        if(!screen_locked) {
            int coverflow_feedback;

            render_desktop_carousel_native();
            crazypod_coverflow_tick();
            coverflow_feedback =
                crazypod_coverflow_take_wheel_feedback();
            if(coverflow_feedback != 0)
                play_wheel_feedback(
                    coverflow_feedback < 0
                        ? BUTTON_SCROLL_BACK
                        : BUTTON_SCROLL_FWD);
            sync_album_flow_metadata();
        }
        crazypod_present_tick();
#ifdef SIMULATOR
        if(simulator_snapshot_pending &&
           !TIME_BEFORE(current_tick, simulator_snapshot_due)) {
            if(simulator_snapshot_stage == 0) {
                simulator_snapshot_pending =
                    simulator_prepare_snapshot();
                simulator_snapshot_stage = 1;
                simulator_snapshot_due = current_tick + HZ / 2;
            }
            else {
                lv_refr_now(display);
                crazypod_present_tick();
                screen_dump();
                simulator_snapshot_pending = false;
            }
        }
#endif
        if(crazypod_artwork_busy() || crazypod_photos_busy() ||
           crazypod_videos_busy())
            keep_cpu_boosted(HZ / 10);
        if(!lv_anim_count_running() &&
           !desktop_motion_active &&
           !crazypod_music_is_scanning() &&
           !crazypod_coverflow_active() &&
           !crazypod_artwork_busy() &&
           !crazypod_photos_busy() &&
           !crazypod_videos_busy() &&
           !TIME_BEFORE(current_tick, boost_until))
            set_cpu_boost(false);
    }
}

#endif
