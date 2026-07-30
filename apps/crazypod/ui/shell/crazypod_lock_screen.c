#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "backlight.h"
#include "button.h"
#include "kernel.h"
#include "timefuncs.h"

#include "lvgl.h"

#include "../../crazypod_appearance.h"
#include "../../crazypod_coverflow.h"
#include "../../crazypod_frameclock.h"
#include "../../crazypod_lcd.h"
#include "../../crazypod_wallpaper.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_lock_screen.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_CYAN 0x26CFF5
#define UNLOCK_HOLD_TICKS ((HZ / 2) > 0 ? (HZ / 2) : 1)
#define UNLOCK_OPEN_TICKS \
    ((HZ * 9 / 20) > 0 ? (HZ * 9 / 20) : 1)

struct lock_screen_state {
    lv_obj_t *root;
    lv_obj_t *wallpaper;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
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
    progress = lock_state.progress_percent;
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
    if(progress <= 0)
        return;
    arc.start_angle = 270;
    arc.end_angle = 270 + progress * 360 / 100;
    arc.width = lock_state.opening ? 4 : 3;
    arc.color = lv_color_hex(
        lock_state.opening ? 0xB8FFE2 :
        progress >= 72 ? 0x76F5C3 : COLOR_CYAN);
    arc.opa = lock_state.opening ? 255 : 235;
    lv_draw_arc(layer, &arc);
    if(lock_state.opening) {
        arc.radius = 36;
        arc.width = 2;
        arc.opa = 58;
        lv_draw_arc(layer, &arc);
    }
}

static void refresh_progress(void)
{
    if(lock_state.progress_surface != NULL)
        lv_obj_invalidate(lock_state.progress_surface);
    if(lock_state.hint_label == NULL)
        return;
    if(lock_state.opening)
        CP_LV_LABEL_SET_TEXT(lock_state.hint_label, CP_TR("UNLOCKED"));
    else
        CP_LV_LABEL_SET_TEXT(
            lock_state.hint_label, CP_TR("Hold Center to Unlock"));
    if(!lock_state.opening && lock_state.icon_shackle != NULL) {
        lv_obj_set_pos(lock_state.icon_shackle, 12, 3);
        lv_obj_set_style_transform_rotation(
            lock_state.icon_shackle, 0, 0);
    }
    if(!lock_state.opening && lock_state.icon != NULL)
        lv_obj_set_style_transform_scale(
            lock_state.icon,
            256 - lock_state.progress_percent * 8 / 100, 0);
    if(!lock_state.opening && lock_state.icon_body != NULL) {
        lv_obj_set_style_bg_color(
            lock_state.icon_body,
            lv_color_hex(lock_state.progress_percent >= 72
                ? 0xD5FFED : 0xDDF9FF),
            0);
    }
}

static void reset_unlock(void)
{
    lock_state.opening = false;
    lock_state.unlock_pressed = false;
    lock_state.unlock_press_start = 0;
    lock_state.progress_percent = 0;
    if(lock_state.icon_shackle != NULL) {
        lv_obj_set_pos(lock_state.icon_shackle, 12, 3);
        lv_obj_set_style_transform_rotation(
            lock_state.icon_shackle, 0, 0);
    }
    if(lock_state.icon != NULL)
        lv_obj_set_style_transform_scale(lock_state.icon, 256, 0);
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
    crazypod_coverflow_set_input_suspended(true);
    lock_state.release_guard = false;
    lock_state.wait_for_wake_release = turn_display_off;
    reset_unlock();
    crazypod_lock_screen_refresh_appearance();
    crazypod_lock_screen_refresh_clock();
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
        int settle;
        int angle;
        int scale;

        if(elapsed < 0)
            elapsed = 0;
        raw = clamp_progress(
            (int)(elapsed * 1024 / UNLOCK_OPEN_TICKS));
        lift = ease_out(clamp_progress(raw * 5 / 2));
        turn = ease_out(clamp_progress((raw - 154) * 5 / 3));
        settle = smooth_step(clamp_progress((raw - 717) * 10 / 3));
        angle = -300 * turn / 1024 + 40 * settle / 1024;
        if(lock_state.icon_shackle != NULL) {
            lv_obj_set_pos(
                lock_state.icon_shackle,
                12 + 3 * turn / 1024,
                3 - 8 * lift / 1024 + settle / 1024);
            lv_obj_set_style_transform_rotation(
                lock_state.icon_shackle, angle, 0);
        }
        if(raw < 410)
            scale = 248 + raw * 22 / 410;
        else
            scale = 270 - (raw - 410) * 14 / 614;
        if(lock_state.icon != NULL)
            lv_obj_set_style_transform_scale(
                lock_state.icon, scale, 0);
        if(lock_state.halo != NULL) {
            lv_obj_set_style_transform_scale(
                lock_state.halo, 256 + 24 * turn / 1024, 0);
            lv_obj_set_style_opa(
                lock_state.halo,
                (lv_opa_t)(255 - 95 * settle / 1024), 0);
        }
        if(elapsed >= UNLOCK_OPEN_TICKS)
            finish_unlock();
    }
}

bool crazypod_lock_screen_handle_button(long button, intptr_t data)
{
    long base;
    bool release;

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
    base = button & BUTTON_MAIN;
    if(release) {
        if(base == BUTTON_SELECT && lock_state.unlock_pressed)
            reset_unlock();
        lock_state.wait_for_wake_release = false;
        return true;
    }
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
    if(lock_state.opening || base != BUTTON_SELECT)
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
    lv_obj_t *scrim;
    lv_obj_t *halo;
    lv_obj_t *icon;
    lv_obj_t *keyhole;

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
    scrim = make_box(
        lock_state.root, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 0x03050A, 132);
    lv_obj_remove_flag(scrim, LV_OBJ_FLAG_CLICKABLE);

    lock_state.time_label = make_label(
        lock_state.root, "09:41", &lv_font_montserrat_48,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(lock_state.time_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_state.time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lock_state.time_label, 0, 27);
    lock_state.date_label = make_label(
        lock_state.root, "", &lv_font_montserrat_10, 0xD6E2EA, 184);
    lv_obj_set_width(lock_state.date_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_state.date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lock_state.date_label, 0, 87);

    lock_state.progress_surface = lv_obj_create(lock_state.root);
    crazypod_ui_widget_make_plain(lock_state.progress_surface);
    lv_obj_set_pos(lock_state.progress_surface, 124, 111);
    lv_obj_set_size(lock_state.progress_surface, 72, 72);
    lv_obj_remove_flag(
        lock_state.progress_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        lock_state.progress_surface, draw_progress_event,
        LV_EVENT_DRAW_MAIN, NULL);
    halo = make_box(
        lock_state.progress_surface, 10, 10, 52, 52,
        LV_RADIUS_CIRCLE, COLOR_CYAN, 12);
    lock_state.halo = halo;
    lv_obj_set_style_transform_pivot_x(halo, 26, 0);
    lv_obj_set_style_transform_pivot_y(halo, 26, 0);
    lv_obj_set_style_shadow_width(halo, 12, 0);
    lv_obj_set_style_shadow_color(halo, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_shadow_opa(halo, 38, 0);
    icon = lv_obj_create(lock_state.progress_surface);
    lock_state.icon = icon;
    crazypod_ui_widget_make_plain(icon);
    lv_obj_set_pos(icon, 14, 12);
    lv_obj_set_size(icon, 44, 48);
    lv_obj_set_style_transform_pivot_x(icon, 22, 0);
    lv_obj_set_style_transform_pivot_y(icon, 24, 0);
    lock_state.icon_shackle = make_box(
        icon, 12, 3, 20, 23, 10, COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_transform_pivot_x(
        lock_state.icon_shackle, 17, 0);
    lv_obj_set_style_transform_pivot_y(
        lock_state.icon_shackle, 20, 0);
    lv_obj_set_style_border_width(lock_state.icon_shackle, 3, 0);
    lv_obj_set_style_border_side(
        lock_state.icon_shackle,
        LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(
        lock_state.icon_shackle, lv_color_hex(0xDDF9FF), 0);
    lv_obj_set_style_border_opa(
        lock_state.icon_shackle, LV_OPA_COVER, 0);
    lock_state.icon_body = make_box(
        icon, 6, 19, 32, 23, 7, 0xDDF9FF, LV_OPA_COVER);
    keyhole = make_box(
        lock_state.icon_body, 14, 7, 5, 8,
        LV_RADIUS_CIRCLE, 0x0A1620, 225);
    make_box(keyhole, 2, 4, 1, 6, 0, 0x0A1620, LV_OPA_COVER);
    lock_state.hint_label = make_label(
        lock_state.root, CP_TR("Hold Center to Unlock"),
        &lv_font_montserrat_8, COLOR_WHITE, 135);
    lv_obj_set_width(lock_state.hint_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_state.hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lock_state.hint_label, 0, 201);
    lv_obj_set_style_text_letter_space(lock_state.hint_label, 1, 0);

    crazypod_lock_screen_refresh_appearance();
    crazypod_lock_screen_refresh_clock();
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

    lock_state.backlight_was_on = backlight_is_on;
    lock_state.backlight_off_generation =
        backlight_off_generation();
    lock_state.locked = false;
    lock_state.release_guard = false;
    if(!backlight_is_on)
        crazypod_lock_screen_show(false);
}

#endif
