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
#define MEDIA_PANEL_X 12
#define MEDIA_PANEL_Y 112
#define MEDIA_PANEL_WIDTH 296
#define MEDIA_PANEL_HEIGHT 116
#define MEDIA_ARTWORK_SIZE 84
#define MEDIA_TEXT_X 104
#define MEDIA_TEXT_WIDTH 178
#define MEDIA_PROGRESS_WIDTH 178
#define MEDIA_TITLE_FONT_SIZE 15
#define MEDIA_ARTIST_FONT_SIZE 12
#define MEDIA_ALBUM_FONT_SIZE 10
#define LOCK_SURFACE_SIZE 84
#define LOCK_SURFACE_X ((LCD_WIDTH - LOCK_SURFACE_SIZE) / 2)
#define LOCK_SURFACE_Y ((LCD_HEIGHT - LOCK_SURFACE_SIZE) / 2)
#define LOCK_PROGRESS_RADIUS 38
#define LOCK_ICON_X 16
#define LOCK_ICON_Y 13
#define LOCK_ICON_WIDTH 52
#define LOCK_ICON_HEIGHT 58
#define LOCK_SHACKLE_X 12
#define LOCK_SHACKLE_Y 2
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
    lv_obj_t *hint_label;
    lv_obj_t *progress_surface;
    lv_obj_t *halo;
    lv_obj_t *icon;
    lv_obj_t *icon_shackle;
    lv_obj_t *icon_body;
    struct crazypod_lock_screen_callbacks callbacks;
    bool locked;
    bool backlight_was_on;
    unsigned int backlight_off_generation;
    bool wait_for_wake_release;
    bool release_guard;
    bool opening;
    bool unlock_pressed;
    long opening_start;
    long unlock_press_start;
    int progress_percent;
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

static void apply_media_layout(bool media_active)
{
    if(lock_state.time_label == NULL)
        return;
    set_hidden(lock_state.media_panel, !media_active);
    if(media_active) {
        lv_obj_set_style_text_font(
            lock_state.time_label, &lv_font_montserrat_48, 0);
        lv_obj_set_width(lock_state.time_label, LCD_WIDTH);
        lv_obj_set_pos(lock_state.time_label, 0, 10);
        lv_obj_set_style_transform_scale(
            lock_state.time_label, 256, 0);
        lv_obj_set_style_text_letter_space(
            lock_state.time_label, 0, 0);
        lv_obj_set_style_text_outline_stroke_width(
            lock_state.time_label, 1, 0);
        lv_obj_set_width(lock_state.date_label, LCD_WIDTH);
        lv_obj_set_pos(lock_state.date_label, 0, 68);
        lv_obj_set_pos(
            lock_state.progress_surface,
            LOCK_SURFACE_X, LOCK_SURFACE_Y);
        lv_obj_set_width(lock_state.hint_label, LCD_WIDTH);
        lv_obj_set_pos(lock_state.hint_label, 0, 95);
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
        lv_obj_set_width(lock_state.date_label, LCD_WIDTH);
        lv_obj_set_pos(lock_state.date_label, 0, 124);
        lv_obj_set_pos(
            lock_state.progress_surface,
            LOCK_SURFACE_X, LOCK_SURFACE_Y);
        lv_obj_set_width(lock_state.hint_label, LCD_WIDTH);
        lv_obj_set_pos(lock_state.hint_label, 0, 153);
    }
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
        apply_media_layout(false);
        return;
    }

    lock_state.media_active = true;
    apply_media_layout(true);
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
    lv_obj_invalidate(lock_state.root);
}

static void draw_progress_event(lv_event_t *event)
{
    lv_obj_t *surface;
    lv_layer_t *layer;
    lv_area_t area;
    lv_area_t end_cap_area;
    lv_draw_arc_dsc_t arc;
    lv_draw_rect_dsc_t end_cap;
    int center_x;
    int center_y;
    int active_width;
    int end_angle;
    int end_radius;
    int end_x;
    int end_y;
    int progress;

    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN)
        return;
    surface = lv_event_get_target(event);
    layer = lv_event_get_layer(event);
    lv_obj_get_coords(surface, &area);
    progress = lock_state.progress_percent;
    if(progress < 0)
        progress = 0;
    if(progress > 100)
        progress = 100;
    center_x = area.x1 + lv_area_get_width(&area) / 2;
    center_y = area.y1 + lv_area_get_height(&area) / 2;

    lv_draw_arc_dsc_init(&arc);
    arc.base.layer = layer;
    arc.center.x = center_x;
    arc.center.y = center_y;
    arc.radius = LOCK_PROGRESS_RADIUS;
    arc.width = 3;
    arc.rounded = 0;
    arc.color = lv_color_hex(COLOR_WHITE);
    arc.opa = 66;
    arc.start_angle = 0;
    arc.end_angle = 360;
    lv_draw_arc(layer, &arc);
    if(progress <= 0)
        return;

    active_width = lock_state.opening ? 7 : 6;
    end_angle = 270 + progress * 360 / 100;
    arc.width = active_width;
    arc.opa = LV_OPA_COVER;
    arc.start_angle = 270;
    arc.end_angle = end_angle;
    lv_draw_arc(layer, &arc);

    /* Keep the fixed start flat: LVGL's rounded arc draws a permanent cap at
     * 12 o'clock, which rasterizes as the stray horizontal line. Only the
     * moving end gets a round cap. */
    end_radius = LOCK_PROGRESS_RADIUS - active_width / 2;
    end_x = center_x +
        ((end_radius * lv_trigo_cos(end_angle)) >> LV_TRIGO_SHIFT);
    end_y = center_y +
        ((end_radius * lv_trigo_sin(end_angle)) >> LV_TRIGO_SHIFT);
    end_cap_area.x1 = end_x - active_width / 2;
    end_cap_area.y1 = end_y - active_width / 2;
    end_cap_area.x2 = end_cap_area.x1 + active_width - 1;
    end_cap_area.y2 = end_cap_area.y1 + active_width - 1;
    lv_draw_rect_dsc_init(&end_cap);
    end_cap.base.layer = layer;
    end_cap.radius = LV_RADIUS_CIRCLE;
    end_cap.bg_color = lv_color_hex(COLOR_WHITE);
    end_cap.bg_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &end_cap, &end_cap_area);
}

static void refresh_progress(void)
{
    bool visible = lock_state.unlock_pressed || lock_state.opening;

    if(lock_state.progress_surface != NULL) {
        set_hidden(lock_state.progress_surface, !visible);
        if(visible)
            lv_obj_move_foreground(lock_state.progress_surface);
        lv_obj_invalidate(lock_state.progress_surface);
    }
    if(lock_state.hint_label == NULL)
        return;
    set_hidden(lock_state.hint_label, visible);
    if(lock_state.opening)
        CP_LV_LABEL_SET_TEXT(lock_state.hint_label, CP_TR("UNLOCKED"));
    else
        CP_LV_LABEL_SET_TEXT(
            lock_state.hint_label, CP_TR("Hold Center to Unlock"));
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
            256 - lock_state.progress_percent * 44 / 100, 0);
    if(!lock_state.opening && lock_state.icon_body != NULL) {
        lv_obj_set_style_bg_color(
            lock_state.icon_body,
            lv_color_hex(lock_state.progress_percent >= 72
                ? 0xD5FFED : 0xDDF9FF),
            0);
    }
}

#ifdef SIMULATOR
void crazypod_lock_screen_simulator_set_progress(int progress)
{
    if(progress < 0)
        progress = 0;
    if(progress > 100)
        progress = 100;
    lock_state.progress_percent = progress;
    set_hidden(lock_state.hint_label, progress > 0);
    set_hidden(lock_state.progress_surface, progress <= 0);
    if(progress > 0)
        lv_obj_move_foreground(lock_state.progress_surface);
    if(lock_state.icon != NULL)
        lv_obj_set_style_transform_scale(
            lock_state.icon, 256 - progress * 44 / 100, 0);
    if(lock_state.icon_body != NULL)
        lv_obj_set_style_bg_color(
            lock_state.icon_body,
            lv_color_hex(progress >= 72 ? 0xD5FFED : 0xDDF9FF), 0);
    if(lock_state.progress_surface != NULL)
        lv_obj_invalidate(lock_state.progress_surface);
}
#endif

static void reset_unlock(void)
{
    lock_state.opening = false;
    lock_state.unlock_pressed = false;
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
    if(lock_state.halo != NULL) {
        lv_obj_set_style_transform_scale(lock_state.halo, 256, 0);
        lv_obj_set_style_opa(lock_state.halo, LV_OPA_COVER, 0);
    }
    if(lock_state.icon_body != NULL)
        lv_obj_set_style_bg_color(
            lock_state.icon_body, lv_color_hex(0xDDF9FF), 0);
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

static void finish_unlock(void)
{
    lock_state.locked = false;
#ifdef HAVE_WHEEL_POSITION
    wheel_set_backlight_on_touch(true);
#endif
    lock_state.opening = false;
    lock_state.unlock_pressed = false;
    lock_state.unlock_press_start = 0;
    lock_state.progress_percent = 0;
    lock_state.release_guard =
        (button_status() & BUTTON_SELECT) != 0;
    lock_state.wait_for_wake_release = false;
    crazypod_coverflow_set_input_suspended(false);
    lv_obj_add_flag(lock_state.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(lock_state.root);
    if(lock_state.callbacks.unlocked != NULL)
        lock_state.callbacks.unlocked();
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

        if((button_status() & BUTTON_SELECT) == 0) {
            reset_unlock();
            return;
        }
        if(elapsed < 0)
            elapsed = 0;
        progress = elapsed >= UNLOCK_HOLD_TICKS
            ? 100 : (int)(elapsed * 100 / UNLOCK_HOLD_TICKS);
        if(progress != lock_state.progress_percent) {
            lock_state.progress_percent = progress;
            refresh_progress();
        }
        if(elapsed >= UNLOCK_HOLD_TICKS) {
            lock_state.unlock_pressed = false;
            lock_state.opening = true;
            lock_state.opening_start = current_tick;
            lock_state.progress_percent = 100;
            if(lock_state.icon_body != NULL)
                lv_obj_set_style_bg_color(
                    lock_state.icon_body, lv_color_hex(0xB8FFE2), 0);
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
            finish_unlock();
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
                LOCK_SHACKLE_X + 5 * turn / 1024,
                LOCK_SHACKLE_Y - 10 * lift / 1024);
            lv_obj_set_style_transform_rotation(
                lock_state.icon_shackle, angle, 0);
        }
        scale = 212 - raw * 212 / 1024;
        if(lock_state.icon != NULL)
            lv_obj_set_style_transform_scale(
                lock_state.icon, scale, 0);
        if(lock_state.progress_surface != NULL)
            lv_obj_set_style_opa(
                lock_state.progress_surface,
                (lv_opa_t)(255 - 255 * fade / 1024), 0);
        if(lock_state.halo != NULL) {
            lv_obj_set_style_transform_scale(
                lock_state.halo,
                256 + 30 * turn / 1024, 0);
            lv_obj_set_style_opa(
                lock_state.halo,
                (lv_opa_t)(255 - 255 * smooth_step(raw) / 1024), 0);
        }
        if(elapsed >= UNLOCK_OPEN_TICKS)
            finish_unlock();
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
            if((button & BUTTON_REL) != 0)
                lock_state.release_guard = false;
            return true;
        }
    }
    if(!lock_state.locked)
        return false;
    release = (button & BUTTON_REL) != 0;
    repeated = (button & BUTTON_REPEAT) != 0;
    base = button & BUTTON_MAIN;
    if(release) {
        if(base == BUTTON_SELECT && lock_state.unlock_pressed)
            reset_unlock();
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
        if(lock_state.callbacks.refresh_media != NULL)
            lock_state.callbacks.refresh_media();
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
            if(lock_state.callbacks.refresh_media != NULL)
                lock_state.callbacks.refresh_media();
            return true;
        }
        if(base == BUTTON_PLAY &&
           lock_state.callbacks.toggle_playback != NULL) {
            lock_state.callbacks.toggle_playback();
            if(lock_state.callbacks.refresh_media != NULL)
                lock_state.callbacks.refresh_media();
            return true;
        }
        if(base == BUTTON_RIGHT &&
           lock_state.callbacks.next_track != NULL) {
            lock_state.callbacks.next_track();
            if(lock_state.callbacks.refresh_media != NULL)
                lock_state.callbacks.refresh_media();
            return true;
        }
    }
    if(base != BUTTON_SELECT)
        return true;
    if(!lock_state.unlock_pressed) {
        lock_state.unlock_pressed = true;
        lock_state.unlock_press_start = current_tick;
        lock_state.progress_percent = 0;
        refresh_progress();
    }
    return true;
}

lv_obj_t *crazypod_lock_screen_create(
    lv_obj_t *parent,
    const struct crazypod_lock_screen_callbacks *callbacks)
{
    lv_obj_t *halo;
    lv_obj_t *icon;
    lv_obj_t *keyhole;
    lv_obj_t *progress_track;

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
        lock_state.root, "", &lv_font_montserrat_10, COLOR_WHITE, 225);
    lv_obj_set_width(lock_state.date_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_state.date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lock_state.date_label, 0, 87);
    add_text_outline(lock_state.date_label, 190);

    lock_state.media_panel = make_box(
        lock_state.root, MEDIA_PANEL_X, MEDIA_PANEL_Y,
        MEDIA_PANEL_WIDTH, MEDIA_PANEL_HEIGHT, 14,
        0x05080B, 148);
    lv_obj_set_style_border_width(lock_state.media_panel, 1, 0);
    lv_obj_set_style_border_color(
        lock_state.media_panel, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(lock_state.media_panel, 38, 0);
    lv_obj_remove_flag(
        lock_state.media_panel, LV_OBJ_FLAG_CLICKABLE);

    lock_state.media_artwork = make_box(
        lock_state.media_panel, 8, 16,
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
    lv_obj_set_pos(lock_state.media_title, MEDIA_TEXT_X, 20);
    lv_obj_set_size(lock_state.media_title, MEDIA_TEXT_WIDTH, 20);
    crazypod_marquee_configure_centered(
        lock_state.media_title, true);
    lock_state.media_artist = make_label(
        lock_state.media_panel, "",
        crazypod_runtime_font_at_size(MEDIA_ARTIST_FONT_SIZE),
        0xE4EEF4, 235);
    lv_obj_set_pos(lock_state.media_artist, MEDIA_TEXT_X, 41);
    lv_obj_set_size(lock_state.media_artist, MEDIA_TEXT_WIDTH, 17);
    crazypod_marquee_configure_centered(
        lock_state.media_artist, true);
    lock_state.media_album = make_label(
        lock_state.media_panel, "",
        crazypod_runtime_font_at_size(MEDIA_ALBUM_FONT_SIZE),
        0xD4E2EA, 205);
    lv_obj_set_pos(lock_state.media_album, MEDIA_TEXT_X, 59);
    lv_obj_set_size(lock_state.media_album, MEDIA_TEXT_WIDTH, 14);
    crazypod_marquee_configure_centered(
        lock_state.media_album, true);

    progress_track = make_box(
        lock_state.media_panel, MEDIA_TEXT_X, 77,
        MEDIA_PROGRESS_WIDTH, 3, LV_RADIUS_CIRCLE,
        COLOR_WHITE, 70);
    lock_state.media_progress_fill = make_box(
        progress_track, 0, 0, 0, 3,
        LV_RADIUS_CIRCLE, COLOR_CYAN, LV_OPA_COVER);
    lock_state.media_elapsed = make_label(
        lock_state.media_panel, "0:00", &lv_font_montserrat_8,
        COLOR_WHITE, 210);
    lv_obj_set_pos(lock_state.media_elapsed, MEDIA_TEXT_X, 83);
    lv_obj_set_width(lock_state.media_elapsed, 58);
    lock_state.media_remaining = make_label(
        lock_state.media_panel, "--:--", &lv_font_montserrat_8,
        COLOR_WHITE, 210);
    lv_obj_set_pos(
        lock_state.media_remaining,
        MEDIA_TEXT_X + MEDIA_PROGRESS_WIDTH - 50, 83);
    lv_obj_set_width(lock_state.media_remaining, 50);
    lv_obj_set_style_text_align(
        lock_state.media_remaining, LV_TEXT_ALIGN_RIGHT, 0);

    lock_state.progress_surface = make_box(
        lock_state.root,
        LOCK_SURFACE_X, LOCK_SURFACE_Y,
        LOCK_SURFACE_SIZE, LOCK_SURFACE_SIZE, LV_RADIUS_CIRCLE,
        0x101A22, 76);
    lv_obj_set_pos(
        lock_state.progress_surface,
        LOCK_SURFACE_X, LOCK_SURFACE_Y);
    lv_obj_set_size(
        lock_state.progress_surface,
        LOCK_SURFACE_SIZE, LOCK_SURFACE_SIZE);
    lv_obj_set_style_blur_backdrop(
        lock_state.progress_surface, true, 0);
    lv_obj_set_style_blur_radius(
        lock_state.progress_surface, 10, 0);
    lv_obj_set_style_blur_quality(
        lock_state.progress_surface, LV_BLUR_QUALITY_SPEED, 0);
    lv_obj_set_style_border_width(
        lock_state.progress_surface, 1, 0);
    lv_obj_set_style_border_color(
        lock_state.progress_surface, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(
        lock_state.progress_surface, 82, 0);
    lv_obj_set_style_shadow_width(
        lock_state.progress_surface, 10, 0);
    lv_obj_set_style_shadow_offset_y(
        lock_state.progress_surface, 4, 0);
    lv_obj_set_style_shadow_color(
        lock_state.progress_surface, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(
        lock_state.progress_surface, 76, 0);
    lv_obj_remove_flag(
        lock_state.progress_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        lock_state.progress_surface, draw_progress_event,
        LV_EVENT_DRAW_MAIN, NULL);
    halo = make_box(
        lock_state.progress_surface, 11, 11, 62, 62,
        LV_RADIUS_CIRCLE, COLOR_WHITE, 12);
    lock_state.halo = halo;
    lv_obj_set_style_transform_pivot_x(halo, 31, 0);
    lv_obj_set_style_transform_pivot_y(halo, 31, 0);
    lv_obj_set_style_shadow_width(halo, 12, 0);
    lv_obj_set_style_shadow_color(halo, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_shadow_opa(halo, 30, 0);
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
        28, 32, 14, COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_transform_pivot_x(
        lock_state.icon_shackle, 23, 0);
    lv_obj_set_style_transform_pivot_y(
        lock_state.icon_shackle, 27, 0);
    lv_obj_set_style_border_width(lock_state.icon_shackle, 4, 0);
    lv_obj_set_style_border_side(
        lock_state.icon_shackle,
        LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(
        lock_state.icon_shackle, lv_color_hex(0xDDF9FF), 0);
    lv_obj_set_style_border_opa(
        lock_state.icon_shackle, LV_OPA_COVER, 0);
    lock_state.icon_body = make_box(
        icon, 5, 27, 42, 27, 8, 0xDDF9FF, LV_OPA_COVER);
    keyhole = make_box(
        lock_state.icon_body, 18, 7, 7, 10,
        LV_RADIUS_CIRCLE, 0x0A1620, 225);
    make_box(keyhole, 2, 5, 3, 7, 1, 0x0A1620, LV_OPA_COVER);
    lock_state.hint_label = make_label(
        lock_state.root, CP_TR("Hold Center to Unlock"),
        &lv_font_montserrat_8, COLOR_WHITE, 220);
    lv_obj_set_width(lock_state.hint_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_state.hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lock_state.hint_label, 0, 179);
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
