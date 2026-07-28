#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>

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
#define UNLOCK_WHEEL_STEPS 19
#define UNLOCK_WHEEL_IDLE_TICKS \
    ((HZ * 9 / 10) > 0 ? (HZ * 9 / 10) : 1)
#define UNLOCK_WHEEL_DECAY_TICKS \
    ((HZ * 3 / 5) > 0 ? (HZ * 3 / 5) : 1)
#define UNLOCK_DIRECTION_HINT_TICKS \
    ((HZ * 6 / 5) > 0 ? (HZ * 6 / 5) : 1)
#define UNLOCK_OPEN_TICKS ((HZ / 4) > 0 ? (HZ / 4) : 1)
#define UNLOCK_INPUT_GUARD_TICKS ((HZ / 4) > 0 ? (HZ / 4) : 1)

struct lock_screen_state {
    lv_obj_t *root;
    lv_obj_t *wallpaper;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_obj_t *hint_label;
    lv_obj_t *progress_surface;
    lv_obj_t *icon_shackle;
    lv_obj_t *icon_body;
    struct crazypod_lock_screen_callbacks callbacks;
    bool locked;
    bool backlight_was_on;
    bool wait_for_wake_release;
    bool release_guard;
    bool opening;
    bool wrong_direction;
    long release_guard_until;
    long opening_start;
    long wrong_direction_until;
    long wheel_last_input_tick;
    int wheel_steps;
    int wheel_decay_start_steps;
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

void crazypod_lock_screen_refresh_clock(void)
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

    if(lock_state.time_label == NULL || lock_state.date_label == NULL)
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
    lv_label_set_text(lock_state.time_label, time_text);
    lv_label_set_text(lock_state.date_label, date_text);
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
        lock_state.wrong_direction ? 0xFFB36B :
        progress >= 72 ? 0xFF739E : COLOR_CYAN);
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
        lv_label_set_text(lock_state.hint_label, "UNLOCKED");
    else if(lock_state.wrong_direction)
        lv_label_set_text(lock_state.hint_label, "TURN CLOCKWISE");
    else if(lock_state.progress_percent > 0)
        lv_label_set_text(
            lock_state.hint_label, "KEEP TURNING CLOCKWISE");
    else
        lv_label_set_text(
            lock_state.hint_label, "TURN CLOCKWISE TO UNLOCK");
    if(!lock_state.opening && lock_state.icon_shackle != NULL) {
        int lift = lock_state.progress_percent * 2 / 100;
        lv_obj_set_pos(lock_state.icon_shackle, 12, 3 - lift);
        lv_obj_set_style_transform_rotation(
            lock_state.icon_shackle, 0, 0);
    }
    if(!lock_state.opening && lock_state.icon_body != NULL)
        lv_obj_set_style_bg_color(
            lock_state.icon_body,
            lv_color_hex(lock_state.wrong_direction
                ? 0xFFE1C7
                : lock_state.progress_percent >= 72
                    ? 0xFFE2EC : 0xDDF9FF),
            0);
}

static void reset_wheel(void)
{
    lock_state.opening = false;
    lock_state.wrong_direction = false;
    lock_state.wheel_steps = 0;
    lock_state.wheel_decay_start_steps = 0;
    lock_state.wheel_last_input_tick = 0;
    lock_state.progress_percent = 0;
    if(lock_state.icon_shackle != NULL) {
        lv_obj_set_pos(lock_state.icon_shackle, 12, 3);
        lv_obj_set_style_transform_rotation(
            lock_state.icon_shackle, 0, 0);
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
    lock_state.locked = true;
    crazypod_coverflow_set_input_suspended(true);
    lock_state.release_guard = false;
    lock_state.wait_for_wake_release = turn_display_off;
    reset_wheel();
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
    lock_state.wrong_direction = false;
    lock_state.wheel_steps = 0;
    lock_state.wheel_decay_start_steps = 0;
    lock_state.wheel_last_input_tick = 0;
    lock_state.progress_percent = 0;
    lock_state.release_guard = true;
    lock_state.release_guard_until =
        current_tick + UNLOCK_INPUT_GUARD_TICKS;
    lock_state.wait_for_wake_release = false;
    crazypod_coverflow_set_input_suspended(false);
    lv_obj_add_flag(lock_state.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(lock_state.root);
    if(lock_state.callbacks.unlocked != NULL)
        lock_state.callbacks.unlocked();
}

void crazypod_lock_screen_process(void)
{
    bool backlight_is_on = is_backlight_on(false);

    if(lock_state.backlight_was_on && !backlight_is_on) {
        if(!lock_state.locked)
            crazypod_lock_screen_show(false);
        else
            reset_wheel();
        lock_state.wait_for_wake_release = false;
    }
    else if(!lock_state.backlight_was_on && backlight_is_on &&
            lock_state.locked) {
        lock_state.wait_for_wake_release =
            button_status() != BUTTON_NONE;
        reset_wheel();
        crazypod_lock_screen_refresh_clock();
    }
    lock_state.backlight_was_on = backlight_is_on;
    if(!lock_state.locked)
        return;
    if(lock_state.wrong_direction &&
       !TIME_BEFORE(current_tick, lock_state.wrong_direction_until)) {
        lock_state.wrong_direction = false;
        refresh_progress();
    }
    if(!lock_state.opening && lock_state.wheel_steps > 0 &&
       lock_state.wheel_last_input_tick != 0) {
        long idle = current_tick - lock_state.wheel_last_input_tick;
        if(idle > UNLOCK_WHEEL_IDLE_TICKS) {
            long decay = idle - UNLOCK_WHEEL_IDLE_TICKS;
            int steps = decay >= UNLOCK_WHEEL_DECAY_TICKS
                ? 0
                : (int)(lock_state.wheel_decay_start_steps *
                    (UNLOCK_WHEEL_DECAY_TICKS - decay) /
                    UNLOCK_WHEEL_DECAY_TICKS);
            if(steps != lock_state.wheel_steps) {
                lock_state.wheel_steps = steps;
                lock_state.progress_percent =
                    steps * 100 / UNLOCK_WHEEL_STEPS;
                refresh_progress();
            }
        }
    }
    if(lock_state.opening) {
        long elapsed = current_tick - lock_state.opening_start;
        int shift;
        if(elapsed < 0)
            elapsed = 0;
        shift = (int)(elapsed * 5 / UNLOCK_OPEN_TICKS);
        if(shift > 5)
            shift = 5;
        if(lock_state.icon_shackle != NULL) {
            lv_obj_set_pos(
                lock_state.icon_shackle, 12 + shift, 1 - shift);
            lv_obj_set_style_transform_rotation(
                lock_state.icon_shackle, shift * 30, 0);
        }
        if(elapsed >= UNLOCK_OPEN_TICKS)
            finish_unlock();
    }
}

static int wheel_event_steps(intptr_t data)
{
    int steps = ((unsigned int)data >> 24) & 0x7f;
    if(steps < 1)
        steps = 1;
    if(steps > UNLOCK_WHEEL_STEPS)
        steps = UNLOCK_WHEEL_STEPS;
    return steps;
}

bool crazypod_lock_screen_handle_button(long button, intptr_t data)
{
    long base;
    bool release;
    bool scroll;

    if(lock_state.release_guard) {
        base = button & BUTTON_MAIN;
        if(TIME_BEFORE(current_tick, lock_state.release_guard_until) &&
           (base == BUTTON_SCROLL_FWD || base == BUTTON_SCROLL_BACK))
            return true;
        lock_state.release_guard = false;
    }
    if(!lock_state.locked)
        return false;
    release = (button & BUTTON_REL) != 0;
    base = button & BUTTON_MAIN;
    scroll = base == BUTTON_SCROLL_FWD || base == BUTTON_SCROLL_BACK;
    if(release) {
        lock_state.wait_for_wake_release = false;
        return true;
    }
    backlight_on();
    if(!lock_state.backlight_was_on) {
        lock_state.backlight_was_on = true;
        lock_state.wait_for_wake_release = !scroll;
        reset_wheel();
        return true;
    }
    if(lock_state.wait_for_wake_release) {
        if(scroll)
            lock_state.wait_for_wake_release = false;
        else
            return true;
    }
    if(lock_state.opening || !scroll)
        return true;
    if(lock_state.callbacks.play_wheel_feedback != NULL)
        lock_state.callbacks.play_wheel_feedback(button);
    if(base == BUTTON_SCROLL_FWD) {
        lock_state.wrong_direction = false;
        lock_state.wheel_steps += wheel_event_steps(data);
        if(lock_state.wheel_steps > UNLOCK_WHEEL_STEPS)
            lock_state.wheel_steps = UNLOCK_WHEEL_STEPS;
    }
    else {
        lock_state.wrong_direction = true;
        lock_state.wrong_direction_until =
            current_tick + UNLOCK_DIRECTION_HINT_TICKS;
        lock_state.wheel_steps -= wheel_event_steps(data);
        if(lock_state.wheel_steps < 0)
            lock_state.wheel_steps = 0;
    }
    lock_state.wheel_decay_start_steps = lock_state.wheel_steps;
    lock_state.wheel_last_input_tick = current_tick;
    lock_state.progress_percent =
        lock_state.wheel_steps * 100 / UNLOCK_WHEEL_STEPS;
    if(lock_state.wheel_steps >= UNLOCK_WHEEL_STEPS) {
        lock_state.opening = true;
        lock_state.opening_start = current_tick;
        lock_state.progress_percent = 100;
        if(lock_state.icon_body != NULL)
            lv_obj_set_style_bg_color(
                lock_state.icon_body, lv_color_hex(0xB8FFE2), 0);
    }
    refresh_progress();
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
    lv_obj_set_style_shadow_width(halo, 12, 0);
    lv_obj_set_style_shadow_color(halo, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_shadow_opa(halo, 38, 0);
    icon = lv_obj_create(lock_state.progress_surface);
    crazypod_ui_widget_make_plain(icon);
    lv_obj_set_pos(icon, 14, 12);
    lv_obj_set_size(icon, 44, 48);
    lock_state.icon_shackle = make_box(
        icon, 12, 3, 20, 23, 10, COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(lock_state.icon_shackle, 3, 0);
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
        lock_state.root, "TURN CLOCKWISE TO UNLOCK",
        &lv_font_montserrat_8, COLOR_WHITE, 135);
    lv_obj_set_width(lock_state.hint_label, LCD_WIDTH);
    lv_obj_set_style_text_align(
        lock_state.hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lock_state.hint_label, 0, 201);
    lv_obj_set_style_text_letter_space(lock_state.hint_label, 1, 0);

    crazypod_lock_screen_refresh_appearance();
    crazypod_lock_screen_refresh_clock();
    reset_wheel();
    lv_obj_add_flag(lock_state.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(lock_state.root);
    return lock_state.root;
}

bool crazypod_lock_screen_is_locked(void)
{
    return lock_state.locked;
}

void crazypod_lock_screen_initialize_backlight_state(void)
{
    lock_state.backlight_was_on = is_backlight_on(false);
    lock_state.locked = false;
    lock_state.release_guard = false;
}

#endif
