#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "kernel.h"

#include "../../crazypod_appearance.h"
#include "../../crazypod_apps.h"
#include "../../crazypod_icons.h"
#include "../../crazypod_wallpaper.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_app_catalog.h"
#include "crazypod_desktop.h"
#include "crazypod_desktop_motion.h"
#include "crazypod_desktop_native.h"
#include "crazypod_now_capsule.h"
#include "crazypod_status_bar.h"

#define COLOR_WHITE 0xFFFFFF

static struct crazypod_desktop_host desktop_host;
static lv_obj_t *screen;
static lv_obj_t *wallpaper;
static lv_obj_t *carousel;
static lv_obj_t *title;
static lv_obj_t *indicators[CRAZYPOD_APP_COUNT];
static lv_obj_t *launcher_cells[CRAZYPOD_APP_COUNT];
static lv_group_t *group;
static int selected_app;
static long desktop_now;

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
        lv_label_set_text(title, selected->name);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        int width = i == selected_app ? 14 : 5;

        if(indicators[i] == NULL)
            continue;
        if(i >= visible_count) {
            lv_obj_add_flag(indicators[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(indicators[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(indicators[i], indicator_x, 166);
        lv_obj_set_size(indicators[i], width, 4);
        lv_obj_set_style_bg_opa(
            indicators[i],
            i == selected_app ? LV_OPA_COVER : 89, 0);
        indicator_x += width + 4;
    }
}

static void layout(bool animated)
{
    update_selection_chrome();
    crazypod_desktop_motion_select(
        desktop_now, selected_app, animated);
    if(animated && desktop_host.boost != NULL)
        desktop_host.boost(HZ / 4);
    crazypod_desktop_native_invalidate(false);
}

static void app_focus_event(lv_event_t *event)
{
    const struct crazypod_app_descriptor *app =
        lv_event_get_user_data(event);
    int visible_index;

    if(lv_event_get_code(event) != LV_EVENT_FOCUSED)
        return;
    visible_index = crazypod_apps_visible_index(app->id);
    if(visible_index < 0)
        return;
    selected_app = visible_index;
    layout(true);
}

static void create_launcher_app(int index)
{
    const struct crazypod_app_descriptor *app =
        crazypod_app_catalog_at(index);
    lv_obj_t *cell;

    if(app == NULL)
        return;
    cell = lv_obj_create(carousel);
    launcher_cells[index] = cell;
    crazypod_ui_widget_make_plain(cell);
    lv_obj_set_size(cell, 120, 110);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    lv_group_add_obj(group, cell);
    lv_obj_add_event_cb(
        cell, app_focus_event, LV_EVENT_FOCUSED, (void *)app);
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
    desktop_now = now;
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

    group = lv_group_create();
    lv_group_set_wrap(group, false);
    carousel = lv_obj_create(screen);
    crazypod_ui_widget_make_plain(carousel);
    lv_obj_set_pos(carousel, 0, 42);
    lv_obj_set_size(carousel, LCD_WIDTH, 116);
    lv_obj_set_style_bg_opa(carousel, LV_OPA_TRANSP, 0);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i)
        create_launcher_app(i);
    lv_obj_add_flag(carousel, LV_OBJ_FLAG_HIDDEN);

    title = crazypod_ui_widget_label(
        screen, visible_app(0)->name,
        &lv_font_montserrat_16, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(title, LCD_WIDTH);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 143);
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i)
        indicators[i] = crazypod_ui_widget_box(
            screen, 0, 166, 5, 4,
            LV_RADIUS_CIRCLE, COLOR_WHITE, 89);

    crazypod_now_capsule_create(screen, metadata_font);
    selected_app = 0;
    crazypod_desktop_native_reset();
    crazypod_desktop_motion_initialize(now, selected_app);
    layout(false);
    lv_group_focus_obj(launcher_cells[
        crazypod_app_catalog_index(crazypod_apps_visible_id(0))]);
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
    int count = crazypod_apps_visible_count();

    if(selected < 0)
        selected = 0;
    if(selected >= count)
        selected = count - 1;
    selected_app = selected;
    layout(animated);
}

void crazypod_desktop_move_selection(int direction)
{
    crazypod_desktop_set_selected(
        selected_app + direction, true);
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
    crazypod_now_capsule_refresh_material();
    crazypod_now_capsule_refresh_appearance();
    crazypod_desktop_native_invalidate(true);
    layout(false);
    if(desktop_host.refresh_corner_masks != NULL)
        desktop_host.refresh_corner_masks();
    if(desktop_host.refresh_lock_appearance != NULL)
        desktop_host.refresh_lock_appearance();
    lv_obj_invalidate(screen);
}

void crazypod_desktop_tick(long now)
{
    desktop_now = now;
    if(crazypod_desktop_motion_tick(now))
        crazypod_desktop_native_invalidate(false);
}

void crazypod_desktop_render_carousel(
    int tile_size, bool blocked)
{
    crazypod_desktop_native_render(
        crazypod_desktop_motion_position_q8(),
        tile_size, blocked);
}

#endif
