#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "button.h"
#include "kernel.h"

#include "../../crazypod_appearance.h"
#include "../../crazypod_apps.h"
#include "../../crazypod_frameclock.h"
#include "../../crazypod_icons.h"
#include "../../crazypod_wallpaper.h"
#include "../presentation/crazypod_hold_feedback.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_app_catalog.h"
#include "crazypod_desktop.h"
#include "crazypod_desktop_native.h"
#include "crazypod_now_capsule.h"
#include "crazypod_status_bar.h"

#define COLOR_WHITE 0xFFFFFF
#define HOME_POSITION_ONE (1L << 16)
#define HOME_WHEEL_POSITIONS 96
#define HOME_WHEEL_CLICKS_PER_DETENT 8
#define HOME_WHEEL_RELEASE_TICKS \
    (((HZ * 6) / 100) > 0 ? ((HZ * 6) / 100) : 1)
#define HOME_SPRING_STIFFNESS 503
#define HOME_SPRING_DAMPING 37
#define HOME_SPRING_POSITION_EPSILON (HOME_POSITION_ONE / 1024)
#define HOME_SPRING_VELOCITY_EPSILON (HOME_POSITION_ONE / 64)
#define HOME_INDICATOR_WIDTH 5
#define HOME_SELECTED_INDICATOR_WIDTH 14
#define HOME_INDICATOR_GAP 4

static struct crazypod_desktop_host desktop_host;
static lv_obj_t *screen;
static lv_obj_t *wallpaper;
static lv_obj_t *title;
static lv_obj_t *indicators[CRAZYPOD_APP_COUNT];
static int selected_app;
static int32_t position_q16;
static int32_t spring_velocity_q16;
static long motion_last_tick;
static long wheel_last_seen;
static int wheel_position;
static int wheel_detent_accumulator;
static int wheel_feedback_direction;
static bool desktop_active;
static bool wheel_tracking;
static bool springing;
static lv_obj_t *hold_feedback_root;
static bool hold_feedback_teardown_pending;
static struct crazypod_hold_feedback hold_feedback;

static void hold_feedback_teardown_ready(lv_event_t *event)
{
    lv_display_t *display = lv_event_get_target(event);

    lv_display_remove_event_cb_with_user_data(
        display, hold_feedback_teardown_ready, NULL);
    if(!hold_feedback_teardown_pending)
        return;
    hold_feedback_teardown_pending = false;
    crazypod_desktop_native_invalidate(true);
}

static const struct crazypod_app_descriptor *visible_app(int index)
{
    return crazypod_app_catalog_find(crazypod_apps_visible_id(index));
}

static void update_selection_chrome(void)
{
    int visible_count = crazypod_apps_visible_count();
    int indicator_strip_width = visible_count > 0
        ? HOME_SELECTED_INDICATOR_WIDTH +
            (visible_count - 1) *
                (HOME_INDICATOR_WIDTH + HOME_INDICATOR_GAP)
        : 0;
    int indicator_x = (LCD_WIDTH - indicator_strip_width) / 2;
    const struct crazypod_app_descriptor *selected =
        visible_app(selected_app);
    int i;

    if(title != NULL && selected != NULL)
        CP_LV_LABEL_SET_TEXT(title, selected->name);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        int width = i == selected_app
            ? HOME_SELECTED_INDICATOR_WIDTH
            : HOME_INDICATOR_WIDTH;

        if(indicators[i] == NULL)
            continue;
        if(i >= visible_count) {
            lv_obj_add_flag(indicators[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(indicators[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(indicators[i], indicator_x, 169);
        lv_obj_set_size(indicators[i], width, 4);
        lv_obj_set_style_bg_opa(
            indicators[i],
            i == selected_app ? LV_OPA_COVER : 89, 0);
        indicator_x += width + HOME_INDICATOR_GAP;
    }
}

static void layout(void)
{
    update_selection_chrome();
    crazypod_desktop_native_invalidate(false);
}

static int clamp_selected(int selected)
{
    int count = crazypod_apps_visible_count();

    if(selected < 0)
        return 0;
    if(selected >= count)
        return count > 0 ? count - 1 : 0;
    return selected;
}

static int32_t selected_position_q16(void)
{
    return selected_app * HOME_POSITION_ONE;
}

static void start_spring(void)
{
    springing = position_q16 != selected_position_q16() ||
        spring_velocity_q16 != 0;
    if(springing)
        crazypod_desktop_native_invalidate(false);
}

#ifdef HAVE_WHEEL_POSITION
static bool set_selected_target(int selected)
{
    selected = clamp_selected(selected);

    if(selected == selected_app)
        return false;
    selected_app = selected;
    update_selection_chrome();
    start_spring();
    return true;
}
#endif

static void advance_spring_tick(void)
{
    int32_t target = selected_position_q16();
    int64_t acceleration;
    int32_t maximum =
        (crazypod_apps_visible_count() > 0
            ? crazypod_apps_visible_count() - 1 : 0) *
        HOME_POSITION_ONE;

    if(!springing)
        return;
    acceleration =
        (int64_t)(target - position_q16) * HOME_SPRING_STIFFNESS -
        (int64_t)spring_velocity_q16 * HOME_SPRING_DAMPING;
    spring_velocity_q16 += (int32_t)(acceleration / HZ);
    position_q16 += spring_velocity_q16 / HZ;
    if(position_q16 < 0) {
        position_q16 = 0;
        if(spring_velocity_q16 < 0)
            spring_velocity_q16 = 0;
    }
    else if(position_q16 > maximum) {
        position_q16 = maximum;
        if(spring_velocity_q16 > 0)
            spring_velocity_q16 = 0;
    }
    if(position_q16 >= target - HOME_SPRING_POSITION_EPSILON &&
       position_q16 <= target + HOME_SPRING_POSITION_EPSILON &&
       spring_velocity_q16 >= -HOME_SPRING_VELOCITY_EPSILON &&
       spring_velocity_q16 <= HOME_SPRING_VELOCITY_EPSILON) {
        position_q16 = target;
        spring_velocity_q16 = 0;
        springing = false;
    }
}

#ifdef HAVE_WHEEL_POSITION
static void apply_wheel_clicks(int delta)
{
    int direction;

    wheel_detent_accumulator += delta;
    while(wheel_detent_accumulator >= HOME_WHEEL_CLICKS_PER_DETENT ||
          wheel_detent_accumulator <= -HOME_WHEEL_CLICKS_PER_DETENT) {
        direction = wheel_detent_accumulator < 0 ? -1 : 1;
        if(!set_selected_target(selected_app + direction)) {
            wheel_detent_accumulator = 0;
            if((direction < 0 && spring_velocity_q16 < 0) ||
               (direction > 0 && spring_velocity_q16 > 0))
                spring_velocity_q16 = 0;
            return;
        }
        wheel_detent_accumulator -=
            direction * HOME_WHEEL_CLICKS_PER_DETENT;
        wheel_feedback_direction = direction;
    }
}

static void sample_wheel(long now)
{
    int current = wheel_status();

    if(current >= 0) {
        current %= HOME_WHEEL_POSITIONS;
        if(!wheel_tracking) {
            wheel_tracking = true;
            wheel_position = current;
            wheel_detent_accumulator = 0;
            motion_last_tick = now;
        }
        else {
            int delta = current - wheel_position;

            if(delta < -HOME_WHEEL_POSITIONS / 2)
                delta += HOME_WHEEL_POSITIONS;
            else if(delta > HOME_WHEEL_POSITIONS / 2)
                delta -= HOME_WHEEL_POSITIONS;
            wheel_position = current;
            if(delta != 0)
                apply_wheel_clicks(delta);
        }
        wheel_last_seen = now;
        return;
    }
    if(wheel_tracking &&
       !TIME_BEFORE(now,
                    wheel_last_seen + HOME_WHEEL_RELEASE_TICKS)) {
        wheel_tracking = false;
        wheel_position = -1;
        wheel_detent_accumulator = 0;
        motion_last_tick = now;
    }
}
#endif

static void advance_motion(long now)
{
    long elapsed = now - motion_last_tick;
    long steps;

    if(elapsed <= 0)
        return;
    motion_last_tick = now;
    if(elapsed > HZ / 2) {
        position_q16 = selected_position_q16();
        spring_velocity_q16 = 0;
        springing = false;
        return;
    }
    for(steps = 0; steps < elapsed; ++steps)
        advance_spring_tick();
    if(springing)
        crazypod_desktop_native_invalidate(false);
}

lv_obj_t *crazypod_desktop_create(
    long now, const lv_font_t *metadata_font,
    const struct crazypod_desktop_host *host)
{
    const lv_image_dsc_t *image =
        crazypod_custom_home_wallpaper();
    int i;

    memset(&desktop_host, 0, sizeof(desktop_host));
    if(host != NULL)
        desktop_host = *host;
    (void)now;
    screen = lv_obj_create(NULL);
    crazypod_ui_widget_make_plain(screen);
    lv_obj_set_style_bg_color(
        screen, lv_color_hex(crazypod_appearance_home_color()), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    if(image == NULL &&
       crazypod_appearance_get()->home_wallpaper[0] == '\0' &&
       crazypod_appearance_get()->home_background == 0)
        image = crazypod_default_wallpaper();
    wallpaper = lv_image_create(screen);
    if(image != NULL)
        lv_image_set_src(wallpaper, image);
    else
        lv_obj_add_flag(wallpaper, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(wallpaper, 0, 0);
    lv_obj_remove_flag(wallpaper, LV_OBJ_FLAG_CLICKABLE);
    crazypod_status_bar_create(0, screen);

    title = crazypod_ui_widget_label(
        screen, visible_app(0)->name,
        &lv_font_montserrat_16, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(title, LCD_WIDTH);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 150);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i)
        indicators[i] = crazypod_ui_widget_box(
            screen, 0, 169, 5, 4,
            LV_RADIUS_CIRCLE, COLOR_WHITE, 89);

    crazypod_now_capsule_create(screen, metadata_font);
    selected_app = 0;
    position_q16 = 0;
    spring_velocity_q16 = 0;
    motion_last_tick = now;
    wheel_last_seen = now;
    wheel_position = -1;
    wheel_detent_accumulator = 0;
    wheel_feedback_direction = 0;
    desktop_active = true;
    wheel_tracking = false;
    springing = false;
#ifdef HAVE_WHEEL_POSITION
    wheel_send_events(false);
#endif
    crazypod_desktop_native_reset();
    layout();
    if(desktop_host.create_corner_masks != NULL)
        desktop_host.create_corner_masks(screen, 0);
    return screen;
}

lv_obj_t *crazypod_desktop_screen(void)
{
    return screen;
}

int crazypod_desktop_selected(void)
{
    return selected_app;
}

void crazypod_desktop_set_selected(int selected, bool animated)
{
    selected = clamp_selected(selected);
    wheel_tracking = false;
    wheel_detent_accumulator = 0;
    motion_last_tick = current_tick;
    if(animated) {
        if(selected != selected_app) {
            selected_app = selected;
            update_selection_chrome();
        }
        start_spring();
    }
    else {
        selected_app = selected;
        position_q16 = selected * HOME_POSITION_ONE;
        spring_velocity_q16 = 0;
        springing = false;
        layout();
    }
}

void crazypod_desktop_move_selection(int direction)
{
    int count = crazypod_apps_visible_count();
    int selected = selected_app + direction;

    if(count <= 1 || direction == 0 ||
       selected < 0 || selected >= count ||
       selected == selected_app)
        return;
    crazypod_desktop_set_selected(selected, true);
}

void crazypod_desktop_set_active(bool active, long now)
{
    desktop_active = active;
#ifdef HAVE_WHEEL_POSITION
    wheel_send_events(!active);
#endif
    if(active)
        return;
    wheel_tracking = false;
    wheel_position = -1;
    wheel_detent_accumulator = 0;
    wheel_feedback_direction = 0;
    advance_motion(now);
    position_q16 = selected_app * HOME_POSITION_ONE;
    spring_velocity_q16 = 0;
    springing = false;
}

void crazypod_desktop_tick(long now)
{
    if(!desktop_active)
        return;
#ifdef HAVE_WHEEL_POSITION
    sample_wheel(now);
#else
    (void)now;
#endif
    advance_motion(now);
}

bool crazypod_desktop_motion_active(void)
{
    return desktop_active &&
        (wheel_tracking || springing);
}

bool crazypod_desktop_wheel_touch_active(void)
{
#ifdef HAVE_WHEEL_POSITION
    return desktop_active && wheel_touch_status();
#else
    return false;
#endif
}

int crazypod_desktop_take_wheel_feedback(void)
{
    int direction = wheel_feedback_direction;

    wheel_feedback_direction = 0;
    return direction;
}

bool crazypod_desktop_hold_feedback_visible(void)
{
    return hold_feedback_root != NULL ||
        hold_feedback_teardown_pending;
}

void crazypod_desktop_hold_feedback_dismiss(
    bool preserve_underlay)
{
    lv_obj_t *root = hold_feedback_root;
    lv_display_t *display;

    if(root == NULL)
        return;
    if(preserve_underlay)
        crazypod_desktop_native_preserve_modal_underlay();
    crazypod_hold_feedback_dismiss(&hold_feedback);
    hold_feedback_root = NULL;
    hold_feedback_teardown_pending = true;
    display = lv_obj_get_display(root);
    lv_display_remove_event_cb_with_user_data(
        display, hold_feedback_teardown_ready, NULL);
    lv_display_add_event_cb(
        display, hold_feedback_teardown_ready,
        LV_EVENT_REFR_READY, NULL);
    lv_obj_delete(root);
}

void crazypod_desktop_hold_feedback_begin(
    const char *symbol, int duration_ms)
{
    if(!desktop_active ||
       screen == NULL || !lv_obj_is_valid(screen) ||
       crazypod_desktop_hold_feedback_visible())
        return;
    hold_feedback_root = crazypod_ui_widget_box(
        screen, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, LV_OPA_TRANSP);
    lv_obj_remove_flag(
        hold_feedback_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(hold_feedback_root);
    (void)crazypod_desktop_native_create_modal_underlay(
        hold_feedback_root);
    crazypod_hold_feedback_begin(
        &hold_feedback, hold_feedback_root,
        symbol, duration_ms);
    if(hold_feedback.root == NULL)
        crazypod_desktop_hold_feedback_dismiss(false);
}

void crazypod_desktop_refresh_appearance(void)
{
    const lv_image_dsc_t *custom =
        crazypod_custom_home_wallpaper();

    if(screen == NULL)
        return;
    if(custom != NULL) {
        lv_image_set_src(wallpaper, custom);
        lv_obj_remove_flag(wallpaper, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(
            screen, lv_color_hex(0x141419), 0);
    }
    else if(crazypod_appearance_get()->home_wallpaper[0] == '\0' &&
            crazypod_appearance_get()->home_background == 0 &&
            crazypod_default_wallpaper() != NULL) {
        lv_image_set_src(wallpaper, crazypod_default_wallpaper());
        lv_obj_remove_flag(wallpaper, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(
            screen, lv_color_hex(0x141419), 0);
    }
    else {
        lv_obj_add_flag(wallpaper, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(
            screen, lv_color_hex(crazypod_appearance_home_color()), 0);
    }
    crazypod_icons_load_theme(crazypod_appearance_get()->icon_theme);
    crazypod_desktop_native_invalidate_icons();
    crazypod_now_capsule_refresh_material();
    crazypod_now_capsule_refresh_appearance();
    crazypod_desktop_native_invalidate(true);
    layout();
    if(desktop_host.refresh_corner_masks != NULL)
        desktop_host.refresh_corner_masks();
    if(desktop_host.refresh_lock_appearance != NULL)
        desktop_host.refresh_lock_appearance();
    lv_obj_invalidate(screen);
}

void crazypod_desktop_render_icon(
    int tile_size, bool blocked)
{
    bool rendered;
    uint32_t render_started_us;
    int count = crazypod_apps_visible_count();
    int app_indices[CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE];
    int centers_x[CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE];
    int whole = position_q16 >> 16;
    int spacing = tile_size + 6;
    int first = whole - 2;
    int visible = 0;
    int index;

    if(blocked)
        return;
    for(index = first;
        index < first + CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE;
        ++index) {
        int center_x;

        if(index < 0 || index >= count)
            continue;
        center_x = 160 + (int)(((int64_t)
            (index * HOME_POSITION_ONE - position_q16) * spacing) >> 16);
        if(center_x + tile_size / 2 <= 0 ||
           center_x - tile_size / 2 >= LCD_WIDTH)
            continue;
        app_indices[visible] = crazypod_app_catalog_index(
            crazypod_apps_visible_id(index));
        centers_x[visible] = center_x;
        ++visible;
    }
    render_started_us = crazypod_monotonic_usec();
    rendered = crazypod_desktop_native_render(
        app_indices, centers_x, visible, tile_size, false);
    if(!rendered)
        return;
    crazypod_present_note_render(
        CRAZYPOD_RENDER_HOME,
        crazypod_monotonic_usec() - render_started_us);
}

#endif
