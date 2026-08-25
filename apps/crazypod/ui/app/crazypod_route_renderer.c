#include "config.h"

#ifdef IPOD_6G

#include "lcd.h"
#include "kernel.h"

#include "../../crazypod_appearance.h"
#include "../../crazypod_books.h"
#include "../../crazypod_coverflow.h"
#include "../../crazypod_runtime_font.h"
#include "../../crazypod_wallpaper.h"
#include "../features/books/crazypod_books_feature.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../features/settings/crazypod_settings_feature.h"
#include "../navigation/crazypod_route_registry.h"
#include "../presentation/crazypod_alpha_jump_hud.h"
#include "../presentation/crazypod_glass_slots.h"
#include "../presentation/crazypod_menu_list.h"
#include "../presentation/crazypod_menu_screen.h"
#include "../presentation/crazypod_preview_motion.h"
#include "../presentation/crazypod_screen_corners.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "../shell/crazypod_shell.h"
#include "../shell/crazypod_status_bar.h"
#include "crazypod_choice_coordinator.h"
#include "crazypod_menu_preview.h"
#include "crazypod_route_renderer.h"

#define STATUS_BAR_HEIGHT 32
#define MENU_PANEL_Y STATUS_BAR_HEIGHT
#define MENU_PANEL_HEIGHT (LCD_HEIGHT - MENU_PANEL_Y)
#define MENU_PANEL_WIDTH 160
#define COLOR_DETAIL 0x08080D
#define COLOR_PANEL 0x1B1B22
#define COLOR_WHITE 0xFFFFFF
#define COLOR_MUTED 0x9A9AA4
#define COLOR_CYAN 0x26CFF5

static struct crazypod_route_renderer_host host;

static void reset_feature_surfaces(void)
{
    if(crazypod_coverflow_active())
        crazypod_coverflow_leave();
    crazypod_alpha_jump_hud_reset();
    crazypod_now_playing_overlay_reset();
    crazypod_choice_coordinator_reset();
    crazypod_menu_list_clear();
    crazypod_preview_motion_forget();
    crazypod_menu_preview_reset();
    crazypod_now_playing_feature_reset_screen();
    crazypod_photos_feature_reset_view();
    crazypod_customize_feature_reset_view();
    crazypod_books_feature_reset_view();
    crazypod_music_feature_reset_view();
}

static uint32_t primary_color(void)
{
    return crazypod_appearance_color(
        crazypod_appearance_get()->primary_color);
}

static uint32_t secondary_color(void)
{
    return crazypod_appearance_color(
        crazypod_appearance_get()->secondary_color);
}

static void render_theme_font_error(void)
{
    const char *detail = crazypod_runtime_font_last_error();
    lv_obj_t *content;
    lv_obj_t *panel;
    lv_obj_t *label;

    if(detail == NULL || detail[0] == '\0' ||
       crazypod_now_playing_theme_last_error() >= 0)
        return;
    content = crazypod_shell_product_content();
    panel = crazypod_ui_widget_box(
        content, 8, 174, 304, 58, 8,
        0x26080C, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        panel, "THEME FONT ERROR",
        &lv_font_montserrat_12, 0xFF453A, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 2);
    lv_obj_set_size(label, 288, 23);
    label = crazypod_ui_widget_label(
        panel, detail,
        &lv_font_montserrat_8, 0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 27);
    lv_obj_set_size(label, 288, 27);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

static void create_panel_backgrounds(void)
{
    lv_obj_t *top;
    lv_obj_t *left;
    bool prepared;

    prepared = crazypod_glass_slot_prepare_menu(
        CRAZYPOD_GLASS_SLOT_MENU_TOPBAR,
        0, 0, LCD_WIDTH, STATUS_BAR_HEIGHT,
        CRAZYPOD_GLASS_MENU_TOPBAR);
    top = crazypod_glass_slot_panel(
        CRAZYPOD_GLASS_SLOT_MENU_TOPBAR, prepared,
        crazypod_shell_product_content(), 0, 0,
        LCD_WIDTH, STATUS_BAR_HEIGHT, 0,
        CRAZYPOD_GLASS_MENU_TOPBAR);
    prepared = crazypod_glass_slot_prepare_menu(
        CRAZYPOD_GLASS_SLOT_MENU_PANEL,
        0, MENU_PANEL_Y,
        MENU_PANEL_WIDTH, MENU_PANEL_HEIGHT,
        CRAZYPOD_GLASS_MENU_PANEL);
    left = crazypod_glass_slot_panel(
        CRAZYPOD_GLASS_SLOT_MENU_PANEL, prepared,
        crazypod_shell_product_content(), 0, MENU_PANEL_Y,
        MENU_PANEL_WIDTH, MENU_PANEL_HEIGHT, 0,
        CRAZYPOD_GLASS_MENU_PANEL);
    lv_obj_remove_flag(top, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(top, 1, 0);
    lv_obj_set_style_border_side(
        top, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(
        top, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(top, 22, 0);
    lv_obj_set_style_border_width(left, 1, 0);
    lv_obj_set_style_border_side(
        left, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(
        left, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(left, 22, 0);
}

static lv_obj_t *make_search_panel(
    lv_obj_t *parent, enum crazypod_glass_slot slot,
    int x, int y, int width, int height, int radius)
{
    bool prepared = crazypod_glass_slot_prepare_frame(
        slot, x, y, width, height,
        CRAZYPOD_GLASS_TEXT_PANEL);

    return crazypod_glass_slot_panel(
        slot, prepared, parent, x, y,
        width, height, radius,
        CRAZYPOD_GLASS_TEXT_PANEL);
}

static void render_menu(const struct route_state *state)
{
    int item_count = host.item_count(state);
    const struct crazypod_menu_screen_context context = {
        .parent = crazypod_shell_product_content(),
        .item_count = item_count,
        .primary_color = primary_color(),
        .secondary_color = secondary_color(),
        .panel_color = COLOR_PANEL,
        .gradient_highlight =
            crazypod_appearance_get()->highlight_style != 0,
        .metadata_font = host.metadata_font,
        .item_title = host.item_title,
        .item_is_current = host.item_is_current,
    };

    create_panel_backgrounds();
    if(item_count <= 0)
        crazypod_menu_preview_render(state, false);
    crazypod_menu_screen_render(state, &context);
    if(item_count > 0)
        crazypod_menu_preview_render(state, false);
}

static void render_music(const struct route_state *state)
{
    if(state->route == MUSIC_ROUTE_SEARCH)
        create_panel_backgrounds();
    if(crazypod_music_feature_render_special(
           crazypod_shell_product_content(), state,
           host.metadata_font, host.item_count(state),
           host.item_title, primary_color(), secondary_color(),
           COLOR_PANEL,
           crazypod_appearance_get()->highlight_style != 0,
           make_search_panel)) {
        return;
    }
    render_menu(state);
}

static void render_now_playing(
    const struct route_state *state)
{
    if(state->route == MUSIC_ROUTE_NOW_PLAYING) {
        if(crazypod_now_playing_theme_render(
               crazypod_shell_product_content(), primary_color()))
            return;
        const struct crazypod_now_playing_render_context context = {
            .parent = crazypod_shell_product_content(),
            .metadata_font = host.metadata_font,
            .render_artwork = host.render_artwork,
            .boost = host.boost,
        };

        crazypod_now_playing_feature_render(&context);
        render_theme_font_error();
        return;
    }
    render_menu(state);
}

static void render_feature(
    const struct route_state *state, long now)
{
    const struct crazypod_feature *feature =
        crazypod_route_registry_feature(state->route);

    if(feature == NULL) {
        render_menu(state);
        return;
    }
    switch(feature->id) {
    case CRAZYPOD_FEATURE_MUSIC:
        render_music(state);
        return;
    case CRAZYPOD_FEATURE_NOW_PLAYING:
        render_now_playing(state);
        return;
    case CRAZYPOD_FEATURE_BOOKS:
        if(crazypod_books_feature_render(
               state, crazypod_shell_product_content()))
            return;
        break;
    case CRAZYPOD_FEATURE_NOTES:
        if(crazypod_notes_feature_render(
               state, crazypod_shell_product_content()))
            return;
        break;
    case CRAZYPOD_FEATURE_PHOTOS: {
        const struct crazypod_photos_render_context context = {
            .parent = crazypod_shell_product_content(),
            .metadata_font = host.metadata_font,
            .primary_color = primary_color(),
            .panel_color = COLOR_PANEL,
            .foreground_color = COLOR_WHITE,
            .muted_color = COLOR_MUTED,
            .now = now,
        };

        if(crazypod_photos_feature_render(state, &context))
            return;
        break;
    }
    case CRAZYPOD_FEATURE_ORGANIZER: {
        const struct crazypod_organizer_render_context context = {
            .parent = crazypod_shell_product_content(),
            .now = now,
            .ticks_per_second = HZ,
        };

        if(crazypod_organizer_feature_render(state, &context))
            return;
        break;
    }
    case CRAZYPOD_FEATURE_CUSTOMIZE:
        if(crazypod_customize_feature_render(
               state, crazypod_shell_product_content(),
               host.metadata_font, primary_color(),
               COLOR_PANEL, COLOR_WHITE, COLOR_CYAN))
            return;
        break;
    case CRAZYPOD_FEATURE_SETTINGS:
        if(crazypod_settings_feature_render(
               state, crazypod_shell_product_content(),
               host.metadata_font, primary_color()))
            return;
        break;
    case CRAZYPOD_FEATURE_MINIAPPS:
        if(crazypod_miniapps_feature_render(
               state, crazypod_shell_product_content(),
               primary_color()))
            return;
        break;
    default:
        break;
    }
    render_menu(state);
}

void crazypod_route_renderer_configure(
    const struct crazypod_route_renderer_host *new_host)
{
    if(new_host != NULL)
        host = *new_host;
}

void crazypod_route_renderer_render(
    const struct route_state *state, long now,
    bool transition)
{
    const lv_image_dsc_t *menu_wallpaper;
    bool book_reader = crazypod_route_registry_has_flag(
        state->route, CRAZYPOD_ROUTE_FLAG_BOOK_READER);
    bool fullscreen = crazypod_route_registry_has_flag(
        state->route, CRAZYPOD_ROUTE_FLAG_FULLSCREEN);
    bool solid_black = crazypod_route_registry_has_flag(
        state->route, CRAZYPOD_ROUTE_FLAG_SOLID_BLACK);
    bool hide_status = crazypod_route_registry_has_flag(
        state->route, CRAZYPOD_ROUTE_FLAG_HIDE_STATUS);
    int theme = crazypod_books_theme();
    uint32_t background = book_reader
        ? crazypod_books_feature_page_colors()[theme]
        : solid_black ? 0x000000
        : fullscreen
            ? crazypod_organizer_feature_background(state->route)
            : crazypod_appearance_menu_color();
    uint32_t foreground = book_reader
        ? crazypod_books_feature_ink_colors()[theme]
        : fullscreen &&
          crazypod_route_registry_has_flag(
              state->route,
              CRAZYPOD_ROUTE_FLAG_DARK_STATUS)
            ? 0x0E0E0E : COLOR_WHITE;
    lv_obj_t *content = crazypod_shell_product_content();

    if(crazypod_miniapps_feature_surface_attached(content) &&
       (state->route == MINIAPP_ROUTE_VIEW ||
        (state->route == MUSIC_ROUTE_NOW_PLAYING &&
         crazypod_now_playing_theme_open()))) {
        if(state->route == MUSIC_ROUTE_NOW_PLAYING)
            (void)crazypod_now_playing_theme_render(
                content, primary_color());
        else
            (void)crazypod_miniapps_feature_render(
                state, content, primary_color());
        lv_obj_invalidate(content);
        crazypod_status_bar_set_visible(
            1, !hide_status &&
               (state->route != MUSIC_ROUTE_NOW_PLAYING ||
                !crazypod_now_playing_theme_owns_status_bar()));
        crazypod_status_bar_set_palette(
            1, foreground, background);
        crazypod_status_bar_foreground(1);
        crazypod_screen_corners_refresh();
        return;
    }

    reset_feature_surfaces();
    lv_obj_clean(content);
    if(state->route == MUSIC_ROUTE_NOW_PLAYING &&
       crazypod_now_playing_theme_enabled() &&
       !crazypod_now_playing_theme_open() &&
       !crazypod_miniapps_feature_is_open())
        crazypod_runtime_asset_fonts_reset();
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_style_bg_color(
        content,
        lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(
        content, LV_OPA_COVER, 0);
    crazypod_ui_widget_box(
        content,
        0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
        background, LV_OPA_COVER);
    menu_wallpaper = solid_black || book_reader || fullscreen
        ? NULL : crazypod_custom_menu_wallpaper();
    if(menu_wallpaper != NULL) {
        lv_obj_t *image =
            lv_image_create(content);

        lv_image_set_src(image, menu_wallpaper);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }
    render_feature(state, now);
    lv_obj_invalidate(content);
    if(transition) {
        lv_obj_set_x(content, 0);
        lv_obj_set_style_opa(
            content,
            LV_OPA_COVER, 0);
        lv_obj_invalidate(content);
    }
    crazypod_status_bar_set_visible(
        1, !hide_status &&
           (state->route != MUSIC_ROUTE_NOW_PLAYING ||
            !crazypod_now_playing_theme_owns_status_bar()));
    crazypod_status_bar_set_palette(
        1, foreground, background);
    crazypod_status_bar_foreground(1);
    crazypod_screen_corners_refresh();
}

void crazypod_route_renderer_prepare_loading(void)
{
    reset_feature_surfaces();
    lv_obj_clean(crazypod_shell_product_content());
    crazypod_status_bar_set_palette(
        1, COLOR_WHITE, COLOR_DETAIL);
    lv_obj_set_style_bg_color(
        crazypod_shell_product_content(),
        lv_color_hex(COLOR_DETAIL), 0);
    lv_obj_set_style_bg_opa(
        crazypod_shell_product_content(), LV_OPA_COVER, 0);
}

#endif
