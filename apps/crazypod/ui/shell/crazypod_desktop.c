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
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_app_catalog.h"
#include "crazypod_desktop.h"
#include "crazypod_desktop_native.h"
#include "crazypod_home_input.h"
#include "crazypod_now_capsule.h"
#include "crazypod_status_bar.h"

#define COLOR_WHITE 0xFFFFFF
#define HOME_POSITION_ONE (1L << 16)
#define HOME_WHEEL_POSITIONS 96
#define HOME_WHEEL_CLICKS_PER_ICON 12
#define HOME_WHEEL_FEEDBACK_CLICKS 4
#define HOME_WHEEL_RELEASE_TICKS \
    (((HZ * 6) / 100) > 0 ? ((HZ * 6) / 100) : 1)
#define HOME_SNAP_TICKS \
    (((HZ * 18) / 100) > 0 ? ((HZ * 18) / 100) : 1)

static struct crazypod_desktop_host desktop_host;
static lv_obj_t *screen;
static lv_obj_t *wallpaper;
static lv_obj_t *title;
static lv_obj_t *indicators[CRAZYPOD_APP_COUNT];
static int selected_app;
static int32_t position_q16;
static int32_t snap_start_q16;
static int32_t snap_target_q16;
static long snap_started;
static long wheel_last_seen;
static struct crazypod_frameclock render_clock;
static struct crazypod_home_wheel_filter wheel_filter;
static int wheel_position;
static int wheel_feedback_accumulator;
static int wheel_feedback_direction;
static bool desktop_active;
static bool wheel_tracking;
static bool snapping;

static const struct crazypod_app_descriptor *visible_app(int index)
{
    return crazypod_app_catalog_find(crazypod_apps_visible_id(index));
}

static void update_selection_chrome(void)
{
    int indicator_x = 95;
    int visible_count = crazypod_apps_visible_count();
    const struct crazypod_app_descriptor *selected =
        visible_app(selected_app);
    int i;

    if(title != NULL && selected != NULL)
        CP_LV_LABEL_SET_TEXT(title, selected->name);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        int width = i == selected_app ? 14 : 5;

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
        indicator_x += width + 4;
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

#ifdef HAVE_WHEEL_POSITION
static int nearest_selected(void)
{
    return clamp_selected(
        (position_q16 + HOME_POSITION_ONE / 2) >> 16);
}

static void update_nearest_selection(void)
{
    int nearest = nearest_selected();

    if(nearest == selected_app)
        return;
    selected_app = nearest;
    update_selection_chrome();
}

static void begin_snap(long now)
{
    int target = nearest_selected();

    selected_app = target;
    snap_start_q16 = position_q16;
    snap_target_q16 = target * HOME_POSITION_ONE;
    snap_started = now;
    snapping = snap_start_q16 != snap_target_q16;
    update_selection_chrome();
    crazypod_desktop_native_invalidate(false);
}
#endif

static void advance_snap(long now)
{
    long elapsed = now - snap_started;
    int32_t remaining_q16;
    int32_t eased_q16;

    if(!snapping)
        return;
    if(elapsed >= HOME_SNAP_TICKS) {
        position_q16 = snap_target_q16;
        snapping = false;
    }
    else {
        int32_t progress_q16 =
            (int32_t)((int64_t)elapsed * HOME_POSITION_ONE /
                      HOME_SNAP_TICKS);
        int32_t remaining_squared_q16;

        if(progress_q16 < 0)
            progress_q16 = 0;
        remaining_q16 = HOME_POSITION_ONE - progress_q16;
        remaining_squared_q16 = (int32_t)(
            (int64_t)remaining_q16 * remaining_q16 >> 16);
        eased_q16 = HOME_POSITION_ONE - (int32_t)(
            (int64_t)remaining_squared_q16 * remaining_q16 >> 16);
        position_q16 = snap_start_q16 +
            (int32_t)(((int64_t)
                (snap_target_q16 - snap_start_q16) * eased_q16) >> 16);
    }
    crazypod_desktop_native_invalidate(false);
}

#ifdef HAVE_WHEEL_POSITION
static void sample_wheel(long now)
{
    int current = wheel_status();
    int count = crazypod_apps_visible_count();

    if(current >= 0) {
        current %= HOME_WHEEL_POSITIONS;
        if(!wheel_tracking) {
            wheel_tracking = true;
            wheel_position = current;
            wheel_feedback_accumulator = 0;
            crazypod_home_wheel_filter_reset(&wheel_filter);
            snapping = false;
        }
        else {
            int delta = current - wheel_position;

            if(delta < -HOME_WHEEL_POSITIONS / 2)
                delta += HOME_WHEEL_POSITIONS;
            else if(delta > HOME_WHEEL_POSITIONS / 2)
                delta -= HOME_WHEEL_POSITIONS;
            wheel_position = current;
            delta = crazypod_home_wheel_filter_apply(
                &wheel_filter, delta);
            if(delta != 0) {
                int32_t previous = position_q16;
                int32_t maximum =
                    (count > 0 ? count - 1 : 0) * HOME_POSITION_ONE;
                int64_t next = (int64_t)position_q16 +
                    (int64_t)delta * HOME_POSITION_ONE /
                        HOME_WHEEL_CLICKS_PER_ICON;

                if(next < 0)
                    position_q16 = 0;
                else if(next > maximum)
                    position_q16 = maximum;
                else
                    position_q16 = (int32_t)next;
                if(position_q16 != previous) {
                    if(wheel_feedback_accumulator != 0 &&
                       (wheel_feedback_accumulator < 0) != (delta < 0))
                        wheel_feedback_accumulator = 0;
                    wheel_feedback_accumulator += delta;
                    if(wheel_feedback_accumulator >=
                       HOME_WHEEL_FEEDBACK_CLICKS ||
                       wheel_feedback_accumulator <=
                       -HOME_WHEEL_FEEDBACK_CLICKS) {
                        wheel_feedback_direction =
                            wheel_feedback_accumulator < 0 ? -1 : 1;
                        wheel_feedback_accumulator %=
                            HOME_WHEEL_FEEDBACK_CLICKS;
                    }
                    update_nearest_selection();
                    crazypod_desktop_native_invalidate(false);
                }
            }
        }
        wheel_last_seen = now;
        return;
    }
    if(wheel_tracking &&
       !TIME_BEFORE(now,
                    wheel_last_seen + HOME_WHEEL_RELEASE_TICKS)) {
        wheel_tracking = false;
        wheel_position = -1;
        wheel_feedback_accumulator = 0;
        crazypod_home_wheel_filter_reset(&wheel_filter);
        begin_snap(now);
    }
}
#endif

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
    snap_start_q16 = 0;
    snap_target_q16 = 0;
    snap_started = now;
    wheel_last_seen = now;
    wheel_position = -1;
    wheel_feedback_accumulator = 0;
    wheel_feedback_direction = 0;
    crazypod_home_wheel_filter_reset(&wheel_filter);
    desktop_active = true;
    wheel_tracking = false;
    snapping = false;
    crazypod_frameclock_reset(&render_clock, now);
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
    selected_app = selected;
    wheel_tracking = false;
    if(animated) {
        snap_start_q16 = position_q16;
        snap_target_q16 = selected * HOME_POSITION_ONE;
        snap_started = current_tick;
        snapping = snap_start_q16 != snap_target_q16;
        update_selection_chrome();
        crazypod_desktop_native_invalidate(false);
    }
    else {
        position_q16 = selected * HOME_POSITION_ONE;
        snap_start_q16 = position_q16;
        snap_target_q16 = position_q16;
        snapping = false;
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
    wheel_feedback_accumulator = 0;
    wheel_feedback_direction = 0;
    crazypod_home_wheel_filter_reset(&wheel_filter);
    if(snapping)
        advance_snap(now);
    position_q16 = selected_app * HOME_POSITION_ONE;
    snap_start_q16 = position_q16;
    snap_target_q16 = position_q16;
    snapping = false;
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
    if(!wheel_tracking)
        advance_snap(now);
}

bool crazypod_desktop_motion_active(void)
{
    return desktop_active && (wheel_tracking || snapping);
}

int crazypod_desktop_take_wheel_feedback(void)
{
    int direction = wheel_feedback_direction;

    wheel_feedback_direction = 0;
    return direction;
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
    if(crazypod_desktop_motion_active()) {
        if(!crazypod_frameclock_due(&render_clock, current_tick))
            return;
        crazypod_frameclock_schedule_next(
            &render_clock, current_tick);
    }
    else
        crazypod_frameclock_reset(&render_clock, current_tick);
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
    crazypod_desktop_native_render(
        app_indices, centers_x, visible, tile_size, false);
}

#endif
