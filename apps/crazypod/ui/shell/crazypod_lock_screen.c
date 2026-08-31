#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "backlight.h"
#include "button.h"
#include "file.h"
#include "kernel.h"
#include "lcd.h"
#include "system.h"
#include "timefuncs.h"

#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "../../crazypod_appearance.h"
#include "../../crazypod_coverflow.h"
#include "../../crazypod_frameclock.h"
#include "../../crazypod_image.h"
#include "../../crazypod_lcd.h"
#include "../../crazypod_runtime_font.h"
#include "../../crazypod_state.h"
#include "../../crazypod_wallpaper.h"
#include "../presentation/crazypod_marquee.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_lock_screen.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_CYAN 0x26CFF5
#define UNLOCK_HOLD_TICKS ((HZ / 2) > 0 ? (HZ / 2) : 1)
#define UNLOCK_OPEN_TICKS \
    ((HZ * 13 / 50) > 0 ? (HZ * 13 / 50) : 1)
#define MEDIA_PANEL_SIDE_MARGIN 12
#define MEDIA_PANEL_BOTTOM_MARGIN 12
#define MEDIA_PANEL_X MEDIA_PANEL_SIDE_MARGIN
#define MEDIA_PANEL_WIDTH (LCD_WIDTH - MEDIA_PANEL_SIDE_MARGIN * 2)
#define MEDIA_PANEL_HEIGHT 94
#define MEDIA_PANEL_Y \
    (LCD_HEIGHT - MEDIA_PANEL_HEIGHT - MEDIA_PANEL_BOTTOM_MARGIN)
#define MEDIA_PANEL_TINT_COLOR 0x11131A
#define MEDIA_PANEL_TINT_OPA 48
#define MEDIA_ARTWORK_SIZE 74
#define MEDIA_ARTWORK_X 12
#define MEDIA_ARTWORK_Y 10
#define MEDIA_TEXT_X 96
#define MEDIA_TEXT_WIDTH 188
#define MEDIA_PROGRESS_WIDTH 188
#define MEDIA_TITLE_FONT_SIZE 15
#define MEDIA_ARTIST_FONT_SIZE 12
#define MEDIA_ALBUM_FONT_SIZE 10
#define LOCK_MEDIA_DATE_SCALE 307
#define LOCK_MEDIA_TIME_SCALE 336
#define LOCK_MEDIA_GROUP_OFFSET_Y 22
#define LOCK_MEDIA_TIME_Y (4 + LOCK_MEDIA_GROUP_OFFSET_Y)
#define LOCK_MEDIA_DATE_Y (68 + LOCK_MEDIA_GROUP_OFFSET_Y)
#define LOCK_MEDIA_HINT_Y (95 + LOCK_MEDIA_GROUP_OFFSET_Y)
#define LOCK_SURFACE_SIZE 28
#define LOCK_DATE_GAP 6
#define LOCK_DATE_MARGIN 8
#define LOCK_ROW_OPTICAL_Y (-5)
#define LOCK_ICON_X 6
#define LOCK_ICON_Y 6
#define LOCK_ICON_WIDTH 16
#define LOCK_ICON_HEIGHT 16
#define LOCK_SHACKLE_X 4
#define LOCK_SHACKLE_Y 1
#define UNLOCK_FEEDBACK_WIDTH 112
#define UNLOCK_FEEDBACK_HEIGHT 20
#define UNLOCK_FEEDBACK_INSET 2
#define UNLOCK_FEEDBACK_RADIUS 8
#define MEDIA_ARTWORK_BANKS 2

static fb_data media_artwork_pixels[
    MEDIA_ARTWORK_BANKS][MEDIA_ARTWORK_SIZE * MEDIA_ARTWORK_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t media_artwork_descriptors[MEDIA_ARTWORK_BANKS];

struct lock_screen_state {
    lv_obj_t *root;
    lv_obj_t *wallpaper;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_obj_t *media_panel;
    lv_obj_t *media_material_clip[2];
    lv_obj_t *media_material[2];
    lv_obj_t *media_glass[2];
    lv_obj_t *media_border[2];
    lv_obj_t *media_artwork;
    lv_obj_t *media_artwork_image;
    lv_obj_t *media_artwork_symbol;
    lv_obj_t *media_pause_overlay;
    lv_obj_t *media_title;
    lv_obj_t *media_artist;
    lv_obj_t *media_album;
    lv_obj_t *media_progress_fill;
    lv_obj_t *media_elapsed;
    lv_obj_t *media_remaining;
    lv_obj_t *hint_progress;
    lv_obj_t *hint_progress_fill;
    lv_obj_t *hint_label;
    lv_obj_t *progress_surface;
    lv_obj_t *icon;
    lv_obj_t *icon_shackle;
    struct crazypod_lock_screen_callbacks callbacks;
    bool locked;
    bool backlight_was_on;
    unsigned int backlight_off_generation;
    bool wait_for_wake_release;
    bool release_guard;
    bool opening;
    bool unlock_pressed;
    bool unlock_button_down;
    long opening_start;
    long unlock_press_start;
    int progress_percent;
    int date_y;
    int media_artwork_bank;
    bool media_active;
    char media_track_path[MAX_PATH];
    const lv_image_dsc_t *media_artwork_descriptor;
    unsigned media_artwork_generation;
    bool media_has_artwork;
};

static struct lock_screen_state lock_state;

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

static int clamp_progress(int value)
{
    if(value < 0)
        return 0;
    if(value > 1024)
        return 1024;
    return value;
}

static int ease_out(int progress)
{
    int inverse = 1024 - clamp_progress(progress);

    return 1024 - inverse * inverse / 1024;
}

static int smooth_step(int progress)
{
    int clamped = clamp_progress(progress);
    int squared = clamped * clamped / 1024;

    return squared * (3072 - 2 * clamped) / 1024;
}

static void set_hidden(lv_obj_t *object, bool hidden)
{
    if(object == NULL)
        return;
    if(hidden)
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static void set_label_text(lv_obj_t *label, const char *text)
{
    const char *safe_text = text != NULL ? text : "";
    const char *resolved_text = crazypod_l10n_text(safe_text);

    if(label != NULL &&
       strcmp(lv_label_get_text(label), resolved_text) != 0)
        lv_label_set_text(label, resolved_text);
}

static int media_corner_radius(bool top)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    int radius = top
        ? appearance->screen_top_radius
        : appearance->screen_bottom_radius;
    int maximum = MEDIA_PANEL_HEIGHT / 2;

    if(radius < 0)
        return 0;
    if(radius > maximum)
        return maximum;
    return radius;
}

static void refresh_media_corners(void)
{
    int split = MEDIA_PANEL_HEIGHT / 2;
    int index;

    if(lock_state.media_panel == NULL ||
       lock_state.media_material[0] == NULL ||
       lock_state.media_material[1] == NULL ||
       lock_state.media_border[0] == NULL ||
       lock_state.media_border[1] == NULL)
        return;
    for(index = 0; index < 2; ++index) {
        bool top = index == 0;
        int radius = media_corner_radius(top);
        int segment_y = top ? 0 : split;
        int segment_height = top
            ? split : MEDIA_PANEL_HEIGHT - split;
        int layer_y = top ? 0 : -radius;
        int layer_height = segment_height + radius;

        lv_obj_set_pos(
            lock_state.media_material_clip[index], 0, segment_y);
        lv_obj_set_size(
            lock_state.media_material_clip[index],
            MEDIA_PANEL_WIDTH, segment_height);
        lv_obj_set_pos(
            lock_state.media_material[index], 0, layer_y);
        lv_obj_set_size(
            lock_state.media_material[index],
            MEDIA_PANEL_WIDTH, layer_height);
        lv_obj_set_style_radius(
            lock_state.media_material[index], radius, 0);
        lv_obj_set_style_clip_corner(
            lock_state.media_material[index], radius > 0, 0);
        if(lock_state.media_glass[index] != NULL)
            lv_obj_set_pos(
                lock_state.media_glass[index], 0,
                top ? 0 : radius - split);

        lv_obj_set_pos(lock_state.media_border[index], 0, layer_y);
        lv_obj_set_size(
            lock_state.media_border[index],
            MEDIA_PANEL_WIDTH, layer_height);
        lv_obj_set_style_radius(
            lock_state.media_border[index], radius, 0);
    }
    lv_obj_invalidate(lock_state.media_panel);
}

static void set_media_label_text(
    lv_obj_t *label, const char *text, unsigned font_size)
{
    const char *safe_text = text != NULL ? text : "";
    const char *resolved_text = crazypod_l10n_text(safe_text);

    if(label == NULL ||
       strcmp(lv_label_get_text(label), resolved_text) == 0)
        return;
    /* Use the runtime Noto face explicitly: metadata is empty at creation,
     * so a later LVGL-only text update cannot infer that CJK glyphs are
     * required. The same face keeps Latin and CJK tracks metrically stable. */
    lv_obj_set_style_text_font(
        label, crazypod_runtime_font_at_size(font_size), 0);
    lv_label_set_text(label, resolved_text);
}

static void refresh_media_material(void)
{
    const lv_image_dsc_t *glass = NULL;

    int index;

    if(lock_state.media_glass[0] == NULL ||
       lock_state.media_glass[1] == NULL)
        return;
    if(crazypod_wallpaper_prepare_frosted_lock_media(
           MEDIA_PANEL_TINT_COLOR, MEDIA_PANEL_TINT_OPA))
        glass = crazypod_frosted_lock_media();
    if(glass == NULL) {
        set_hidden(lock_state.media_glass[0], true);
        set_hidden(lock_state.media_glass[1], true);
        return;
    }
    for(index = 0; index < 2; ++index) {
        lv_image_set_src(lock_state.media_glass[index], glass);
        lv_obj_set_style_image_opa(
            lock_state.media_glass[index], LV_OPA_COVER, 0);
        set_hidden(lock_state.media_glass[index], false);
    }
    refresh_media_corners();
}

static void add_text_outline(lv_obj_t *label, lv_opa_t opacity)
{
    lv_obj_set_style_text_outline_stroke_color(
        label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_outline_stroke_width(label, 1, 0);
    lv_obj_set_style_text_outline_stroke_opa(label, opacity, 0);
}

static void format_media_time(
    char *buffer, size_t size, uint32_t milliseconds)
{
    uint32_t seconds = milliseconds / 1000;

    snprintf(buffer, size, "%lu:%02lu",
             (unsigned long)(seconds / 60),
             (unsigned long)(seconds % 60));
}

static void layout_lock_row(void)
{
    const lv_font_t *font;
    const char *date_text;
    lv_point_t date_size = { 0, 0 };
    int available_width;
    int date_width;
    int date_scale;
    int scaled_date_width;
    int row_width;
    int row_x;
    int row_y;

    if(lock_state.date_label == NULL ||
       lock_state.progress_surface == NULL)
        return;
    font = lv_obj_get_style_text_font(
        lock_state.date_label, LV_PART_MAIN);
    date_text = lv_label_get_text(lock_state.date_label);
    lv_text_get_size(
        &date_size, date_text != NULL ? date_text : "", font,
        0, 0, LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
    date_scale = lv_obj_get_style_transform_scale_x(
        lock_state.date_label, LV_PART_MAIN);
    if(date_scale <= 0)
        date_scale = 256;
    available_width = LCD_WIDTH - LOCK_DATE_MARGIN * 2 -
        LOCK_SURFACE_SIZE - LOCK_DATE_GAP;
    date_width = date_size.x;
    if(date_width * date_scale / 256 > available_width)
        date_width = available_width * 256 / date_scale;
    scaled_date_width =
        (date_width * date_scale + 255) / 256;
    row_width = LOCK_SURFACE_SIZE + LOCK_DATE_GAP +
        scaled_date_width;
    row_x = (LCD_WIDTH - row_width) / 2;
    row_y = lock_state.date_y +
        (lv_font_get_line_height(font) - LOCK_SURFACE_SIZE) / 2 +
        LOCK_ROW_OPTICAL_Y;
    lv_obj_set_pos(lock_state.progress_surface, row_x, row_y);
    lv_obj_set_pos(
        lock_state.date_label,
        row_x + LOCK_SURFACE_SIZE + LOCK_DATE_GAP,
        lock_state.date_y);
    lv_obj_set_width(lock_state.date_label, date_width);
    lv_obj_set_style_text_align(
        lock_state.date_label, LV_TEXT_ALIGN_LEFT, 0);
}

static void apply_media_layout(bool media_active)
{
    if(lock_state.time_label == NULL)
        return;
    set_hidden(lock_state.media_panel, !media_active);
    if(media_active) {
        lv_obj_set_style_text_font(
            lock_state.time_label, &lv_font_montserrat_48, 0);
        lv_obj_set_width(lock_state.time_label, LCD_WIDTH);
        lv_obj_set_pos(
            lock_state.time_label, 0, LOCK_MEDIA_TIME_Y);
        lv_obj_set_style_transform_scale(
            lock_state.time_label, LOCK_MEDIA_TIME_SCALE, 0);
        lv_obj_set_style_text_letter_space(
            lock_state.time_label, 0, 0);
        lv_obj_set_style_text_outline_stroke_width(
            lock_state.time_label, 1, 0);
        lv_obj_set_style_transform_scale(
            lock_state.date_label, LOCK_MEDIA_DATE_SCALE, 0);
        lv_obj_set_width(lock_state.date_label, LCD_WIDTH);
        lock_state.date_y = LOCK_MEDIA_DATE_Y;
        lv_obj_set_pos(lock_state.date_label, 0, lock_state.date_y);
        lv_obj_set_pos(
            lock_state.hint_progress,
            (LCD_WIDTH - UNLOCK_FEEDBACK_WIDTH) / 2,
            LOCK_MEDIA_HINT_Y -
                lv_obj_get_y(lock_state.hint_label));
    }
    else {
        lv_obj_set_style_text_font(
            lock_state.time_label, &lv_font_montserrat_48, 0);
        lv_obj_set_width(lock_state.time_label, LCD_WIDTH);
        lv_obj_set_pos(lock_state.time_label, 0, 62);
        lv_obj_set_style_transform_scale(
            lock_state.time_label, 292, 0);
        lv_obj_set_style_text_letter_space(
            lock_state.time_label, 2, 0);
        lv_obj_set_style_text_outline_stroke_width(
            lock_state.time_label, 2, 0);
        lv_obj_set_style_transform_scale(
            lock_state.date_label, 256, 0);
        lv_obj_set_width(lock_state.date_label, LCD_WIDTH);
        lock_state.date_y = 124;
        lv_obj_set_pos(lock_state.date_label, 0, lock_state.date_y);
        lv_obj_set_pos(
            lock_state.hint_progress,
            (LCD_WIDTH - UNLOCK_FEEDBACK_WIDTH) / 2,
            153 - lv_obj_get_y(lock_state.hint_label));
    }
    layout_lock_row();
}

static const lv_image_dsc_t *prepare_media_artwork(
    const lv_image_dsc_t *source)
{
    const fb_data *source_pixels;
    int source_stride;
    int crop_size;
    int crop_x;
    int crop_y;
    int bank;

    if(source == NULL || source->data == NULL ||
       source->header.magic != LV_IMAGE_HEADER_MAGIC ||
       source->header.cf != LV_COLOR_FORMAT_RGB565 ||
       source->header.w <= 0 || source->header.h <= 0 ||
       (size_t)source->header.stride <
           (size_t)source->header.w * sizeof(fb_data) ||
       (size_t)source->data_size <
           (size_t)source->header.stride * source->header.h)
        return NULL;
    source_stride = source->header.stride / sizeof(fb_data);
    crop_size = source->header.w < source->header.h
        ? source->header.w : source->header.h;
    crop_x = (source->header.w - crop_size) / 2;
    crop_y = (source->header.h - crop_size) / 2;
    source_pixels = (const fb_data *)source->data +
        crop_y * source_stride + crop_x;
    bank = lock_state.media_artwork_bank == 0 ? 1 : 0;
    if(media_artwork_descriptors[bank].header.magic ==
           LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&media_artwork_descriptors[bank]);
    if(!crazypod_image_scale_rgb565(
           source_pixels, crop_size, crop_size, source_stride,
           media_artwork_pixels[bank],
           MEDIA_ARTWORK_SIZE, MEDIA_ARTWORK_SIZE) ||
       !crazypod_image_configure_rgb565(
           &media_artwork_descriptors[bank],
           media_artwork_pixels[bank],
           MEDIA_ARTWORK_SIZE, MEDIA_ARTWORK_SIZE))
        return NULL;
    lock_state.media_artwork_bank = bank;
    return &media_artwork_descriptors[bank];
}

static void update_media_artwork(
    const struct crazypod_lock_media_snapshot *snapshot)
{
    const lv_image_dsc_t *prepared;
    bool has_artwork;

    /* A NULL source means the confirmed track's artwork is still pending.
     * Keep the copied image until the replacement can be committed. */
    if(snapshot->artwork == NULL) {
        set_hidden(lock_state.media_artwork_image,
                   !lock_state.media_has_artwork);
        return;
    }
    if(snapshot->artwork == lock_state.media_artwork_descriptor &&
       snapshot->artwork_generation ==
           lock_state.media_artwork_generation &&
       strcmp(lock_state.media_track_path,
              snapshot->track_path != NULL
                  ? snapshot->track_path : "") == 0)
        return;
    snprintf(lock_state.media_track_path,
             sizeof(lock_state.media_track_path), "%s",
             snapshot->track_path != NULL
                 ? snapshot->track_path : "");
    lock_state.media_artwork_descriptor = snapshot->artwork;
    lock_state.media_artwork_generation =
        snapshot->artwork_generation;
    prepared = prepare_media_artwork(snapshot->artwork);
    has_artwork = prepared != NULL;
    lock_state.media_has_artwork = has_artwork;
    if(has_artwork) {
        lv_image_set_src(
            lock_state.media_artwork_image, prepared);
        lv_image_set_scale(lock_state.media_artwork_image, LV_SCALE_NONE);
        lv_obj_center(lock_state.media_artwork_image);
    }
    set_hidden(lock_state.media_artwork_image, !has_artwork);
}

void crazypod_lock_screen_update_media(
    const struct crazypod_lock_media_snapshot *snapshot)
{
    char elapsed[16];
    char remaining[16];
    uint32_t duration;
    uint32_t position;
    int progress_width;
    bool active = snapshot != NULL && snapshot->active;

    if(lock_state.root == NULL)
        return;
    if(!active) {
        lock_state.media_active = false;
        lock_state.media_track_path[0] = '\0';
        lock_state.media_artwork_descriptor = NULL;
        lock_state.media_artwork_generation = 0;
        lock_state.media_has_artwork = false;
        set_hidden(lock_state.media_artwork_image, true);
        apply_media_layout(false);
        return;
    }

    if(!lock_state.media_active)
        refresh_media_material();
    lock_state.media_active = true;
    apply_media_layout(true);
    if(snapshot->metadata_ready) {
        set_media_label_text(
            lock_state.media_title,
            snapshot->title != NULL && snapshot->title[0] != '\0'
                ? snapshot->title : CP_TR("Untitled"),
            MEDIA_TITLE_FONT_SIZE);
        set_media_label_text(
            lock_state.media_artist,
            snapshot->artist != NULL && snapshot->artist[0] != '\0'
                ? snapshot->artist : CP_TR("Unknown Artist"),
            MEDIA_ARTIST_FONT_SIZE);
        set_media_label_text(
            lock_state.media_album, snapshot->album,
            MEDIA_ALBUM_FONT_SIZE);
        set_hidden(lock_state.media_album,
                   snapshot->album == NULL || snapshot->album[0] == '\0');
    }
    crazypod_marquee_configure_centered(
        lock_state.media_title, true);
    crazypod_marquee_configure_centered(
        lock_state.media_artist, true);
    crazypod_marquee_configure_centered(
        lock_state.media_album, true);

    duration = snapshot->length_ms;
    position = snapshot->elapsed_ms;
    if(duration > 0 && position > duration)
        position = duration;
    progress_width = duration > 0
        ? (int)((uint64_t)position * MEDIA_PROGRESS_WIDTH / duration)
        : 0;
    lv_obj_set_width(lock_state.media_progress_fill, progress_width);
    format_media_time(elapsed, sizeof(elapsed), position);
    format_media_time(remaining, sizeof(remaining),
                      duration > position ? duration - position : 0);
    set_label_text(lock_state.media_elapsed, elapsed);
    if(duration > 0) {
        char remaining_text[18];

        snprintf(remaining_text, sizeof(remaining_text), "-%s", remaining);
        set_label_text(lock_state.media_remaining, remaining_text);
    }
    else {
        set_label_text(lock_state.media_remaining, "--:--");
    }
    update_media_artwork(snapshot);
    set_hidden(
        lock_state.media_artwork_symbol,
        lock_state.media_has_artwork || !snapshot->playing);
    set_hidden(lock_state.media_pause_overlay, snapshot->playing);
    if(lock_state.locked)
        lv_obj_invalidate(lock_state.root);
}

void crazypod_lock_screen_refresh_clock(void)
{
    static const char *const days[] = {
        CP_TR("SUNDAY"), CP_TR("MONDAY"), CP_TR("TUESDAY"), CP_TR("WEDNESDAY"),
        CP_TR("THURSDAY"), CP_TR("FRIDAY"), CP_TR("SATURDAY")
    };
    static const char *const months[] = {
        CP_TR("JAN"), CP_TR("FEB"), CP_TR("MAR"), CP_TR("APR"), CP_TR("MAY"), CP_TR("JUN"),
        CP_TR("JUL"), CP_TR("AUG"), CP_TR("SEP"), CP_TR("OCT"), CP_TR("NOV"), CP_TR("DEC")
    };
    struct tm *now;
    char time_text[8];
    char date_text[32];
    int weekday;
    int month;

    if(lock_state.time_label == NULL || lock_state.date_label == NULL ||
       !lock_state.locked)
        return;
    now = get_time();
    weekday = now->tm_wday >= 0 && now->tm_wday < 7
        ? now->tm_wday : 0;
    month = now->tm_mon >= 0 && now->tm_mon < 12
        ? now->tm_mon : 0;
    snprintf(time_text, sizeof(time_text), CP_FMT("%02d:%02d"),
             now->tm_hour, now->tm_min);
    snprintf(date_text, sizeof(date_text), "%s  \xE2\x80\xA2  %s %d",
             crazypod_l10n_text(days[weekday]),
             crazypod_l10n_text(months[month]), now->tm_mday);
    if(strcmp(lv_label_get_text(lock_state.time_label), time_text) != 0)
        CP_LV_LABEL_SET_TEXT(lock_state.time_label, time_text);
    if(strcmp(lv_label_get_text(lock_state.date_label), date_text) != 0)
        CP_LV_LABEL_SET_TEXT(lock_state.date_label, date_text);
    layout_lock_row();
}

void crazypod_lock_screen_refresh_appearance(void)
{
    const struct crazypod_appearance *appearance;
    const lv_image_dsc_t *wallpaper = NULL;
    uint32_t color;

    if(lock_state.root == NULL)
        return;
    appearance = crazypod_appearance_get();
    if(appearance->lock_wallpaper[0] != '\0') {
        wallpaper = crazypod_custom_lock_wallpaper();
        color = crazypod_appearance_lock_color();
    }
    else if(appearance->lock_background == 0) {
        wallpaper = crazypod_default_wallpaper();
        color = crazypod_appearance_lock_color();
    }
    else {
        color = crazypod_appearance_lock_color();
    }
    lv_obj_set_style_bg_color(
        lock_state.root, lv_color_hex(color), 0);
    if(wallpaper != NULL) {
        lv_image_set_src(lock_state.wallpaper, wallpaper);
        lv_obj_remove_flag(lock_state.wallpaper, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(lock_state.wallpaper, LV_OBJ_FLAG_HIDDEN);
    }
    refresh_media_corners();
    if(lock_state.media_active)
        refresh_media_material();
    lv_obj_invalidate(lock_state.root);
}

static void refresh_progress(void)
{
    bool active = lock_state.opening ||
        lock_state.progress_percent > 0;
    int max_fill_width =
        UNLOCK_FEEDBACK_WIDTH - 2 * UNLOCK_FEEDBACK_INSET - 2;
    int fill_width = 1 +
        (max_fill_width - 1) *
        lock_state.progress_percent / 100;

    lv_obj_set_style_bg_opa(
        lock_state.hint_progress,
        active ? 226 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(
        lock_state.hint_progress,
        active ? 45 : LV_OPA_TRANSP, 0);
    set_hidden(lock_state.hint_progress_fill, !active);
    lv_obj_set_width(lock_state.hint_progress_fill, fill_width);
    if(lock_state.progress_surface != NULL) {
        set_hidden(lock_state.progress_surface, false);
        if(active)
            lv_obj_move_foreground(lock_state.progress_surface);
        lv_obj_invalidate(lock_state.progress_surface);
    }
    if(lock_state.hint_label == NULL)
        return;
    set_hidden(lock_state.hint_label, false);
    if(!lock_state.opening && lock_state.icon_shackle != NULL) {
        lv_obj_set_pos(
            lock_state.icon_shackle,
            LOCK_SHACKLE_X, LOCK_SHACKLE_Y);
        lv_obj_set_style_transform_rotation(
            lock_state.icon_shackle, 0, 0);
    }
    if(!lock_state.opening && lock_state.icon != NULL)
        lv_obj_set_style_transform_scale(
            lock_state.icon,
            256 - lock_state.progress_percent * 12 / 100, 0);
}

#ifdef SIMULATOR
void crazypod_lock_screen_simulator_set_progress(int progress)
{
    if(progress < 0)
        progress = 0;
    if(progress > 100)
        progress = 100;
    lock_state.progress_percent = progress;
    refresh_progress();
}
#endif

static void reset_unlock(void)
{
    lock_state.opening = false;
    lock_state.unlock_pressed = false;
    lock_state.unlock_button_down = false;
    lock_state.unlock_press_start = 0;
    lock_state.progress_percent = 0;
    if(lock_state.icon_shackle != NULL) {
        lv_obj_set_pos(
            lock_state.icon_shackle,
            LOCK_SHACKLE_X, LOCK_SHACKLE_Y);
        lv_obj_set_style_transform_rotation(
            lock_state.icon_shackle, 0, 0);
    }
    if(lock_state.icon != NULL)
        lv_obj_set_style_transform_scale(lock_state.icon, 256, 0);
    if(lock_state.progress_surface != NULL)
        lv_obj_set_style_opa(
            lock_state.progress_surface, LV_OPA_COVER, 0);
    refresh_progress();
}

void crazypod_lock_screen_show(bool turn_display_off)
{
    if(lock_state.root == NULL)
        return;
    if(lock_state.callbacks.lock_inhibited != NULL &&
       lock_state.callbacks.lock_inhibited())
        return;
    lock_state.locked = true;
#ifdef HAVE_WHEEL_POSITION
    wheel_set_backlight_on_touch(false);
    wheel_send_events(true);
#endif
    crazypod_coverflow_set_input_suspended(true);
    crazypod_coverflow_set_compositing_suspended(true);
    lock_state.release_guard = false;
    lock_state.wait_for_wake_release = turn_display_off;
    reset_unlock();
    crazypod_lock_screen_refresh_appearance();
    crazypod_lock_screen_refresh_clock();
    if(lock_state.callbacks.refresh_media != NULL)
        lock_state.callbacks.refresh_media();
    lv_obj_remove_flag(lock_state.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(lock_state.root);
    lv_obj_invalidate(lock_state.root);
    lv_refr_now(NULL);
    crazypod_present_now();
    if(turn_display_off)
        backlight_off();
}

static void finish_unlock(bool animate_transition)
{
    bool transition_started = animate_transition &&
        lock_state.callbacks.begin_unlock_transition != NULL &&
        lock_state.callbacks.begin_unlock_transition();

    lock_state.locked = false;
#ifdef HAVE_WHEEL_POSITION
    wheel_set_backlight_on_touch(true);
#endif
    lock_state.opening = false;
    lock_state.unlock_pressed = false;
    lock_state.unlock_press_start = 0;
    lock_state.progress_percent = 0;
    lock_state.release_guard = lock_state.unlock_button_down;
    lock_state.wait_for_wake_release = false;
    crazypod_coverflow_set_input_suspended(false);
    crazypod_coverflow_set_compositing_suspended(false);
    lv_obj_add_flag(lock_state.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(lock_state.root);
    if(lock_state.callbacks.unlocked != NULL)
        lock_state.callbacks.unlocked(transition_started);
}

void crazypod_lock_screen_process(void)
{
    bool backlight_is_on = is_backlight_on(true);
    unsigned int off_generation = backlight_off_generation();
    bool went_off =
        off_generation != lock_state.backlight_off_generation ||
        (lock_state.backlight_was_on && !backlight_is_on);

    lock_state.backlight_off_generation = off_generation;
    if(went_off) {
        if(!lock_state.locked &&
           lock_state.callbacks.lock_inhibited != NULL &&
           lock_state.callbacks.lock_inhibited()) {
            backlight_on();
            lock_state.backlight_was_on = true;
            lock_state.backlight_off_generation =
                backlight_off_generation();
            return;
        }
        if(!lock_state.locked)
            crazypod_lock_screen_show(false);
        else
            reset_unlock();
        /*
         * If the display already woke before this UI turn, consume the
         * triggering press on the lock screen instead of letting Home act
         * on an input that occurred after an off transition.
         */
        lock_state.wait_for_wake_release = backlight_is_on;
    }
    else if(!lock_state.backlight_was_on && backlight_is_on &&
            lock_state.locked) {
        lock_state.wait_for_wake_release =
            button_status() != BUTTON_NONE;
        reset_unlock();
        crazypod_lock_screen_refresh_clock();
    }
    lock_state.backlight_was_on = backlight_is_on;
    if(!lock_state.locked)
        return;

    if(lock_state.unlock_pressed && !lock_state.opening) {
        long elapsed = current_tick - lock_state.unlock_press_start;
        int progress;

        if(!lock_state.unlock_button_down) {
            reset_unlock();
            return;
        }
        if(elapsed < 0)
            elapsed = 0;
        if(elapsed >= UNLOCK_HOLD_TICKS)
            progress = 100;
        else
            progress = 1 +
                (int)(elapsed * 99 / UNLOCK_HOLD_TICKS);
        if(progress != lock_state.progress_percent) {
            lock_state.progress_percent = progress;
            refresh_progress();
        }
        if(elapsed >= UNLOCK_HOLD_TICKS) {
            lock_state.unlock_pressed = false;
            lock_state.opening = true;
            lock_state.opening_start = current_tick;
            lock_state.progress_percent = 100;
            refresh_progress();
        }
    }
    if(lock_state.opening) {
        long elapsed = current_tick - lock_state.opening_start;
        int raw;
        int lift;
        int turn;
        int angle;
        int fade;
        int scale;

        if(elapsed < 0)
            elapsed = 0;
        if(crazypod_state_reduce_motion()) {
            finish_unlock(false);
            return;
        }
        raw = clamp_progress(
            (int)(elapsed * 1024 / UNLOCK_OPEN_TICKS));
        lift = ease_out(clamp_progress(raw * 5 / 2));
        turn = ease_out(clamp_progress((raw - 154) * 5 / 3));
        fade = smooth_step(clamp_progress((raw - 410) * 5 / 3));
        angle = -260 * turn / 1024;
        if(lock_state.icon_shackle != NULL) {
            lv_obj_set_pos(
                lock_state.icon_shackle,
                LOCK_SHACKLE_X + 2 * turn / 1024,
                LOCK_SHACKLE_Y - 4 * lift / 1024);
            lv_obj_set_style_transform_rotation(
                lock_state.icon_shackle, angle, 0);
        }
        scale = 244 - raw * 244 / 1024;
        if(lock_state.icon != NULL)
            lv_obj_set_style_transform_scale(
                lock_state.icon, scale, 0);
        if(lock_state.progress_surface != NULL)
            lv_obj_set_style_opa(
                lock_state.progress_surface,
                (lv_opa_t)(255 - 255 * fade / 1024), 0);
        if(elapsed >= UNLOCK_OPEN_TICKS)
            finish_unlock(true);
    }
}

bool crazypod_lock_screen_handle_button(long button, intptr_t data)
{
    long base;
    bool release;
    bool repeated;

    (void)data;

    if(lock_state.release_guard) {
        base = button & BUTTON_MAIN;
        if(base == BUTTON_SELECT) {
            if((button & BUTTON_REL) != 0) {
                lock_state.release_guard = false;
                lock_state.unlock_button_down = false;
            }
            return true;
        }
    }
    if(!lock_state.locked)
        return false;
    release = (button & BUTTON_REL) != 0;
    repeated = (button & BUTTON_REPEAT) != 0;
    base = button & BUTTON_MAIN;
    if(release) {
        if(base == BUTTON_SELECT) {
            lock_state.unlock_button_down = false;
            if(lock_state.unlock_pressed)
                reset_unlock();
        }
        lock_state.wait_for_wake_release = false;
        return true;
    }
    if(base == BUTTON_SCROLL_FWD || base == BUTTON_SCROLL_BACK)
        return true;
    backlight_on();
    if(!lock_state.backlight_was_on) {
        lock_state.backlight_was_on = true;
        lock_state.wait_for_wake_release = true;
        reset_unlock();
        crazypod_lock_screen_refresh_clock();
        return true;
    }
    if(lock_state.wait_for_wake_release) {
        return true;
    }
    if(lock_state.opening)
        return true;
    if(lock_state.media_active && !repeated) {
        if(base == BUTTON_LEFT &&
           lock_state.callbacks.previous_track != NULL) {
            lock_state.callbacks.previous_track();
            return true;
        }
        if(base == BUTTON_PLAY &&
           lock_state.callbacks.toggle_playback != NULL) {
            lock_state.callbacks.toggle_playback();
            return true;
        }
        if(base == BUTTON_RIGHT &&
           lock_state.callbacks.next_track != NULL) {
            lock_state.callbacks.next_track();
            return true;
        }
    }
    if(base != BUTTON_SELECT)
        return true;
    lock_state.unlock_button_down = true;
    if(!lock_state.unlock_pressed) {
        lock_state.unlock_pressed = true;
        lock_state.unlock_press_start = current_tick;
        lock_state.progress_percent = 1;
        refresh_progress();
    }
    return true;
}

bool crazypod_lock_screen_media_controls_ready(void)
{
    return lock_state.locked && lock_state.media_active &&
        lock_state.backlight_was_on &&
        !lock_state.wait_for_wake_release &&
        !lock_state.opening;
}

lv_obj_t *crazypod_lock_screen_create(
    lv_obj_t *parent,
    const struct crazypod_lock_screen_callbacks *callbacks)
{
    lv_obj_t *body;
    lv_obj_t *icon;
    lv_obj_t *progress_track;
    int index;

    lock_state = (struct lock_screen_state){0};
    if(callbacks != NULL)
        lock_state.callbacks = *callbacks;
    lock_state.root = lv_obj_create(parent);
    crazypod_ui_widget_make_plain(lock_state.root);
    lv_obj_set_pos(lock_state.root, 0, 0);
    lv_obj_set_size(lock_state.root, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(lock_state.root, LV_OPA_COVER, 0);
    lock_state.wallpaper = lv_image_create(lock_state.root);
    lv_obj_set_pos(lock_state.wallpaper, 0, 0);
    lv_obj_remove_flag(lock_state.wallpaper, LV_OBJ_FLAG_CLICKABLE);

    lock_state.time_label = make_label(
        lock_state.root, "09:41", &lv_font_montserrat_48,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(lock_state.time_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_state.time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_transform_pivot_x(
        lock_state.time_label, LCD_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(
        lock_state.time_label, 26, 0);
    lv_obj_set_pos(lock_state.time_label, 0, 27);
    add_text_outline(lock_state.time_label, 178);
    lock_state.date_label = make_label(
        lock_state.root, "",
        crazypod_runtime_font_at_size_weight(10, 700),
        COLOR_WHITE, 225);
    lv_obj_set_width(lock_state.date_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_state.date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_transform_pivot_x(
        lock_state.date_label, 0, 0);
    lv_obj_set_style_transform_pivot_y(
        lock_state.date_label, 5, 0);
    lv_obj_set_pos(lock_state.date_label, 0, 87);
    add_text_outline(lock_state.date_label, 190);

    lock_state.media_panel = make_box(
        lock_state.root, MEDIA_PANEL_X, MEDIA_PANEL_Y,
        MEDIA_PANEL_WIDTH, MEDIA_PANEL_HEIGHT, 0,
        MEDIA_PANEL_TINT_COLOR, LV_OPA_TRANSP);
    lv_obj_remove_flag(
        lock_state.media_panel, LV_OBJ_FLAG_CLICKABLE);
    for(index = 0; index < 2; ++index) {
        lock_state.media_material_clip[index] = make_box(
            lock_state.media_panel, 0, 0,
            MEDIA_PANEL_WIDTH, MEDIA_PANEL_HEIGHT / 2, 0,
            MEDIA_PANEL_TINT_COLOR, LV_OPA_TRANSP);
        lv_obj_remove_flag(
            lock_state.media_material_clip[index],
            LV_OBJ_FLAG_CLICKABLE);
        lock_state.media_material[index] = make_box(
            lock_state.media_material_clip[index], 0, 0,
            MEDIA_PANEL_WIDTH, MEDIA_PANEL_HEIGHT, 0,
            MEDIA_PANEL_TINT_COLOR, LV_OPA_TRANSP);
        lv_obj_remove_flag(
            lock_state.media_material[index], LV_OBJ_FLAG_CLICKABLE);
        lock_state.media_glass[index] =
            lv_image_create(lock_state.media_material[index]);
        lv_obj_set_pos(lock_state.media_glass[index], 0, 0);
        lv_obj_remove_flag(
            lock_state.media_glass[index], LV_OBJ_FLAG_CLICKABLE);
        set_hidden(lock_state.media_glass[index], true);
        lock_state.media_border[index] = make_box(
            lock_state.media_material_clip[index], 0, 0,
            MEDIA_PANEL_WIDTH, MEDIA_PANEL_HEIGHT, 0,
            COLOR_WHITE, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(
            lock_state.media_border[index], 1, 0);
        lv_obj_set_style_border_side(
            lock_state.media_border[index], LV_BORDER_SIDE_FULL, 0);
        lv_obj_set_style_border_color(
            lock_state.media_border[index],
            lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(
            lock_state.media_border[index], 38, 0);
        lv_obj_remove_flag(
            lock_state.media_border[index], LV_OBJ_FLAG_CLICKABLE);
    }

    lock_state.media_artwork = make_box(
        lock_state.media_panel, MEDIA_ARTWORK_X, MEDIA_ARTWORK_Y,
        MEDIA_ARTWORK_SIZE, MEDIA_ARTWORK_SIZE, 10,
        0x25485A, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        lock_state.media_artwork, lv_color_hex(0x0D1623), 0);
    lv_obj_set_style_bg_grad_dir(
        lock_state.media_artwork, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_clip_corner(
        lock_state.media_artwork, true, 0);
    lv_obj_set_style_shadow_width(
        lock_state.media_artwork, 6, 0);
    lv_obj_set_style_shadow_offset_y(
        lock_state.media_artwork, 3, 0);
    lv_obj_set_style_shadow_color(
        lock_state.media_artwork, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(
        lock_state.media_artwork, 88, 0);
    lock_state.media_artwork_image =
        lv_image_create(lock_state.media_artwork);
    lv_obj_remove_flag(
        lock_state.media_artwork_image,
        LV_OBJ_FLAG_CLICKABLE);
    lock_state.media_artwork_symbol = make_label(
        lock_state.media_artwork, LV_SYMBOL_AUDIO,
        &lv_font_montserrat_24, COLOR_WHITE, 220);
    lv_obj_center(lock_state.media_artwork_symbol);
    lock_state.media_pause_overlay = make_label(
        lock_state.media_artwork, LV_SYMBOL_PAUSE,
        &lv_font_montserrat_24, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_center(lock_state.media_pause_overlay);
    add_text_outline(lock_state.media_pause_overlay, 210);
    set_hidden(lock_state.media_pause_overlay, true);

    lock_state.media_title = make_label(
        lock_state.media_panel, "",
        crazypod_runtime_font_at_size(MEDIA_TITLE_FONT_SIZE),
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(lock_state.media_title, MEDIA_TEXT_X, 12);
    lv_obj_set_size(lock_state.media_title, MEDIA_TEXT_WIDTH, 20);
    crazypod_marquee_configure_centered(
        lock_state.media_title, true);
    lock_state.media_artist = make_label(
        lock_state.media_panel, "",
        crazypod_runtime_font_at_size(MEDIA_ARTIST_FONT_SIZE),
        0xE4EEF4, 235);
    lv_obj_set_pos(lock_state.media_artist, MEDIA_TEXT_X, 33);
    lv_obj_set_size(lock_state.media_artist, MEDIA_TEXT_WIDTH, 17);
    crazypod_marquee_configure_centered(
        lock_state.media_artist, true);
    lock_state.media_album = make_label(
        lock_state.media_panel, "",
        crazypod_runtime_font_at_size(MEDIA_ALBUM_FONT_SIZE),
        0xD4E2EA, 205);
    lv_obj_set_pos(lock_state.media_album, MEDIA_TEXT_X, 51);
    lv_obj_set_size(lock_state.media_album, MEDIA_TEXT_WIDTH, 14);
    crazypod_marquee_configure_centered(
        lock_state.media_album, true);

    progress_track = make_box(
        lock_state.media_panel, MEDIA_TEXT_X, 69,
        MEDIA_PROGRESS_WIDTH, 3, LV_RADIUS_CIRCLE,
        COLOR_WHITE, 70);
    lock_state.media_progress_fill = make_box(
        progress_track, 0, 0, 0, 3,
        LV_RADIUS_CIRCLE, COLOR_CYAN, LV_OPA_COVER);
    lock_state.media_elapsed = make_label(
        lock_state.media_panel, "0:00", &lv_font_montserrat_8,
        COLOR_WHITE, 210);
    lv_obj_set_pos(lock_state.media_elapsed, MEDIA_TEXT_X, 75);
    lv_obj_set_width(lock_state.media_elapsed, 58);
    lock_state.media_remaining = make_label(
        lock_state.media_panel, "--:--", &lv_font_montserrat_8,
        COLOR_WHITE, 210);
    lv_obj_set_pos(
        lock_state.media_remaining,
        MEDIA_TEXT_X + MEDIA_PROGRESS_WIDTH - 50, 75);
    lv_obj_set_width(lock_state.media_remaining, 50);
    lv_obj_set_style_text_align(
        lock_state.media_remaining, LV_TEXT_ALIGN_RIGHT, 0);

    lock_state.progress_surface = make_box(
        lock_state.root,
        0, 0,
        LOCK_SURFACE_SIZE, LOCK_SURFACE_SIZE, LV_RADIUS_CIRCLE,
        0x101A22, LV_OPA_TRANSP);
    lv_obj_set_pos(
        lock_state.progress_surface,
        0, 0);
    lv_obj_set_size(
        lock_state.progress_surface,
        LOCK_SURFACE_SIZE, LOCK_SURFACE_SIZE);
    lv_obj_remove_flag(
        lock_state.progress_surface, LV_OBJ_FLAG_CLICKABLE);
    icon = lv_obj_create(lock_state.progress_surface);
    lock_state.icon = icon;
    crazypod_ui_widget_make_plain(icon);
    lv_obj_set_pos(icon, LOCK_ICON_X, LOCK_ICON_Y);
    lv_obj_set_size(icon, LOCK_ICON_WIDTH, LOCK_ICON_HEIGHT);
    lv_obj_set_style_transform_pivot_x(
        icon, LOCK_ICON_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(
        icon, LOCK_ICON_HEIGHT / 2, 0);
    lock_state.icon_shackle = make_box(
        icon, LOCK_SHACKLE_X, LOCK_SHACKLE_Y,
        8, 9, 4, COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_transform_pivot_x(
        lock_state.icon_shackle, 7, 0);
    lv_obj_set_style_transform_pivot_y(
        lock_state.icon_shackle, 8, 0);
    lv_obj_set_style_border_width(lock_state.icon_shackle, 2, 0);
    lv_obj_set_style_border_side(
        lock_state.icon_shackle,
        LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(
        lock_state.icon_shackle, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(
        lock_state.icon_shackle, LV_OPA_COVER, 0);
    body = make_box(
        icon, 1, 8, 14, 8, 2,
        COLOR_WHITE, LV_OPA_COVER);
    make_box(
        body, 6, 2, 2, 4,
        LV_RADIUS_CIRCLE, 0x0A1620, LV_OPA_COVER);
    lock_state.hint_progress = make_box(
        lock_state.root, 0, 0,
        UNLOCK_FEEDBACK_WIDTH, UNLOCK_FEEDBACK_HEIGHT,
        UNLOCK_FEEDBACK_RADIUS, 0x111118, 226);
    lv_obj_remove_flag(
        lock_state.hint_progress, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(
        lock_state.hint_progress, 1, 0);
    lv_obj_set_style_border_color(
        lock_state.hint_progress, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(
        lock_state.hint_progress, 45, 0);
    lock_state.hint_progress_fill = make_box(
        lock_state.hint_progress,
        UNLOCK_FEEDBACK_INSET, UNLOCK_FEEDBACK_INSET,
        1,
        UNLOCK_FEEDBACK_HEIGHT - 2 * UNLOCK_FEEDBACK_INSET - 2,
        UNLOCK_FEEDBACK_RADIUS - UNLOCK_FEEDBACK_INSET,
        COLOR_WHITE, 78);
    lv_obj_remove_flag(
        lock_state.hint_progress_fill, LV_OBJ_FLAG_CLICKABLE);
    lock_state.hint_label = make_label(
        lock_state.hint_progress, "",
        crazypod_runtime_font_at_size_weight(8, 700),
        COLOR_WHITE, 220);
    lv_label_set_text(
        lock_state.hint_label, CP_TR("Hold Center to Unlock"));
    lv_obj_set_size(
        lock_state.hint_label,
        UNLOCK_FEEDBACK_WIDTH - 2 * UNLOCK_FEEDBACK_INSET,
        lv_font_get_line_height(
            crazypod_runtime_font_at_size_weight(8, 700)));
    lv_obj_set_style_text_align(
        lock_state.hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(
        lock_state.hint_label,
        UNLOCK_FEEDBACK_INSET,
        (UNLOCK_FEEDBACK_HEIGHT -
            lv_font_get_line_height(
                crazypod_runtime_font_at_size_weight(8, 700))) / 2);
    lv_obj_set_style_text_letter_space(lock_state.hint_label, 1, 0);
    add_text_outline(lock_state.hint_label, 190);

    crazypod_lock_screen_refresh_appearance();
    crazypod_lock_screen_refresh_clock();
    lock_state.media_artwork_bank = -1;
    apply_media_layout(false);
    reset_unlock();
    lv_obj_add_flag(lock_state.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(lock_state.root);
    return lock_state.root;
}

bool crazypod_lock_screen_is_locked(void)
{
    return lock_state.locked;
}

bool crazypod_lock_screen_motion_active(void)
{
    return lock_state.opening || lock_state.unlock_pressed;
}

void crazypod_lock_screen_initialize_backlight_state(void)
{
    bool backlight_is_on = is_backlight_on(true);

#ifdef HAVE_WHEEL_POSITION
    wheel_set_backlight_on_touch(true);
#endif
    lock_state.backlight_was_on = backlight_is_on;
    lock_state.backlight_off_generation =
        backlight_off_generation();
    lock_state.locked = false;
    lock_state.release_guard = false;
    crazypod_lock_screen_show(false);
}

#endif
