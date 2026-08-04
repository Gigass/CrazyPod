#include "config.h"

#if defined(IPOD_6G) && defined(SIMULATOR)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "button.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "lvgl.h"

#include "../../crazypod_books.h"
#include "../../crazypod_frameclock.h"
#include "../../crazypod_l10n.h"
#include "../../crazypod_lcd.h"
#include "../../crazypod_miniapps.h"
#include "../../miniapps/runtime/crazypod_miniapp_native_runtime.h"
#include "../../crazypod_music.h"
#include "../../crazypod_notes.h"
#include "../../crazypod_organizer.h"
#include "../../crazypod_runtime_font.h"
#include "../../crazypod_videos.h"
#include "../../crazypod_workouts.h"
#include "../features/books/crazypod_books_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../navigation/crazypod_route_query.h"
#include "../shell/crazypod_shell.h"
#include "crazypod_app_input.h"
#include "crazypod_simulator_snapshot.h"

long crazypod_simulator_snapshot_settle_ticks(void)
{
    const char *value =
        getenv("CRAZYPOD_SIM_DUMP_SETTLE_MS");
    char *end = NULL;
    long milliseconds = value != NULL
        ? strtol(value, &end, 10) : 500;

    if(end == value || (end != NULL && *end != '\0') ||
       milliseconds < 50 || milliseconds > 10000)
        milliseconds = 500;
    return MAX(1, milliseconds * HZ / 1000);
}

static struct route_state *current_route(void)
{
    return crazypod_ui_routes_current();
}

static int item_count(void)
{
    return crazypod_route_query_item_count(
        current_route(), crazypod_music_search_query());
}

static void add_runtime_font_probe_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    int x, int y, int width, int height, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
}

static bool open_runtime_font_catalog(void)
{
    static const struct {
        const char *name;
        enum crazypod_font_family family;
        unsigned weight;
    } probes[] = {
        { "SANS 100", CRAZYPOD_FONT_FAMILY_SYSTEM, 100 },
        { "SANS 200", CRAZYPOD_FONT_FAMILY_SYSTEM, 200 },
        { "SANS 300", CRAZYPOD_FONT_FAMILY_SYSTEM, 300 },
        { "SANS 400", CRAZYPOD_FONT_FAMILY_SYSTEM, 400 },
        { "SANS 500", CRAZYPOD_FONT_FAMILY_SYSTEM, 500 },
        { "SANS 600", CRAZYPOD_FONT_FAMILY_SYSTEM, 600 },
        { "SANS 700", CRAZYPOD_FONT_FAMILY_SYSTEM, 700 },
        { "SANS 800", CRAZYPOD_FONT_FAMILY_SYSTEM, 800 },
        { "SANS 900", CRAZYPOD_FONT_FAMILY_SYSTEM, 900 },
        { "SERIF 100", CRAZYPOD_FONT_FAMILY_SERIF, 100 },
        { "SERIF 200", CRAZYPOD_FONT_FAMILY_SERIF, 200 },
        { "SERIF 300", CRAZYPOD_FONT_FAMILY_SERIF, 300 },
        { "SERIF 400", CRAZYPOD_FONT_FAMILY_SERIF, 400 },
        { "SERIF 500", CRAZYPOD_FONT_FAMILY_SERIF, 500 },
        { "SERIF 600", CRAZYPOD_FONT_FAMILY_SERIF, 600 },
        { "SERIF 700", CRAZYPOD_FONT_FAMILY_SERIF, 700 },
        { "SERIF 800", CRAZYPOD_FONT_FAMILY_SERIF, 800 },
        { "SERIF 900", CRAZYPOD_FONT_FAMILY_SERIF, 900 },
        { "MONO 100", CRAZYPOD_FONT_FAMILY_MONO, 100 },
        { "MONO 200", CRAZYPOD_FONT_FAMILY_MONO, 200 },
        { "MONO 300", CRAZYPOD_FONT_FAMILY_MONO, 300 },
        { "MONO 400", CRAZYPOD_FONT_FAMILY_MONO, 400 },
        { "MONO 500", CRAZYPOD_FONT_FAMILY_MONO, 500 },
        { "MONO 600", CRAZYPOD_FONT_FAMILY_MONO, 600 },
        { "MONO 700", CRAZYPOD_FONT_FAMILY_MONO, 700 },
        { "MONO 800", CRAZYPOD_FONT_FAMILY_MONO, 800 },
        { "MONO 900", CRAZYPOD_FONT_FAMILY_MONO, 900 },
    };
    static const uint32_t colors[] = {
        0x5ac8fa, 0xf5f5f7, 0xffcc00,
        0x34c759, 0xff375f, 0xbf5af2,
    };
    lv_obj_t *catalog = lv_obj_create(NULL);
    unsigned index;

    if(!crazypod_runtime_fonts_ready())
        return false;
    crazypod_shell_open_product();
    lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lv_layer_sys(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_style_all(catalog);
    lv_obj_set_pos(catalog, 0, 0);
    lv_obj_set_size(catalog, 320, 240);
    lv_obj_set_style_bg_color(catalog, lv_color_hex(0x101114), 0);
    lv_obj_set_style_bg_opa(catalog, LV_OPA_COVER, 0);
    lv_screen_load(catalog);
    add_runtime_font_probe_label(
        catalog, "一个人的精彩\n萧亚轩 / 红蔷薇",
        crazypod_runtime_font(), 10, 8, 300, 42, 0xf5f5f7);
    for(index = 0; index < ARRAYLEN(probes); ++index) {
        int column = index / 9;
        int row = index % 9;

        add_runtime_font_probe_label(
            catalog, probes[index].name,
            crazypod_runtime_font_resolve(
                probes[index].family, 12, probes[index].weight,
                CRAZYPOD_FONT_STYLE_NORMAL, 16),
            8 + column * 104, 50 + row * 20, 98, 18,
            colors[index % ARRAYLEN(colors)]);
    }
    return true;
}

static void select_bounded(
    const struct crazypod_simulator_snapshot_host *host,
    int requested)
{
    int count = item_count();

    if(requested < 0)
        requested = 0;
    if(requested >= count)
        requested = count > 0 ? count - 1 : 0;
    current_route()->selected = requested;
    host->render(false);
}

static int notes_home_note_start(void)
{
    return crazypod_notes_feature_draft_available() ? 2 : 1;
}

static int notes_home_deleted_index(void)
{
    return notes_home_note_start() + crazypod_notes_count(false) + 1;
}

static bool books_has_continue(void)
{
    int index = crazypod_books_recent_index();
    const struct crazypod_book *book = crazypod_book_get(index);

    return book != NULL && book->progress > 0;
}

static void open_photos_route(
    const struct crazypod_simulator_snapshot_host *host,
    enum crazypod_route route)
{
    host->open_app(CRAZYPOD_APP_PHOTOS);
    host->push_route(route, -1);
}

static bool open_miniapp_snapshot(
    const struct crazypod_simulator_snapshot_host *host,
    const char *id, int page)
{
    struct cp_input_event event = {
        .struct_size = sizeof(event),
        .steps = 1,
    };
    int app_index;

    host->open_app(CRAZYPOD_APP_MINI_APPS);
    app_index = crazypod_miniapps_find(id);
    if(app_index < 0)
        return false;
    current_route()->selected = app_index;
    host->activate_selected();
    if(current_route()->route != MINIAPP_ROUTE_VIEW) {
        fprintf(
            stderr,
            "CrazyPod Mini App smoke failed: %d\n",
            crazypod_miniapps_feature_last_error());
        return false;
    }
    if(page >= 0) {
        if(page > 0) {
            event.type = CP_INPUT_WHEEL_CLOCKWISE;
            event.steps = (uint8_t)page;
            (void)crazypod_miniapps_event(&event);
        }
        event.type = CP_INPUT_SELECT;
        event.steps = 1;
        (void)crazypod_miniapps_event(&event);
        if(!crazypod_miniapps_is_open())
            return false;
        host->render(false);
    }
    {
        const char *actions = getenv("CRAZYPOD_SIM_ACTIONS");
        char buffer[1025];
        char *cursor;
        char *save = NULL;
        unsigned count = 0;

        if(actions == NULL || actions[0] == '\0')
            return true;
        if(strlen(actions) >= sizeof(buffer))
            return false;
        strlcpy(buffer, actions, sizeof(buffer));
        cursor = strtok_r(buffer, ",", &save);
        while(cursor != NULL && count++ < 64u) {
            char *separator = strchr(cursor, ':');
            int steps = 1;

            if(separator != NULL) {
                char *end = NULL;
                *separator++ = '\0';
                steps = (int)strtol(separator, &end, 10);
                if(end == separator || *end != '\0' ||
                   steps < 1 || steps > 32)
                    return false;
            }
            if(strcmp(cursor, "cw") == 0)
                event.type = CP_INPUT_WHEEL_CLOCKWISE;
            else if(strcmp(cursor, "ccw") == 0)
                event.type = CP_INPUT_WHEEL_COUNTERCLOCKWISE;
            else if(strcmp(cursor, "left") == 0)
                event.type = CP_INPUT_LEFT;
            else if(strcmp(cursor, "right") == 0)
                event.type = CP_INPUT_RIGHT;
            else if(strcmp(cursor, "select") == 0)
                event.type = CP_INPUT_SELECT;
            else if(strcmp(cursor, "play") == 0)
                event.type = CP_INPUT_PLAY;
            else if(strcmp(cursor, "menu") == 0)
                event.type = CP_INPUT_MENU;
            else
                return false;
            event.steps = (uint8_t)steps;
            (void)crazypod_miniapps_event(&event);
            if(!crazypod_miniapps_is_open())
                return false;
            host->render(false);
            cursor = strtok_r(NULL, ",", &save);
        }
        return cursor == NULL;
    }
}

void crazypod_simulator_snapshot_write_profile(void)
{
    struct cp_diagnostics_snapshot snapshot;
    char line[512];
    int file;
    int length;

    if(getenv("CRAZYPOD_SIM_PROFILE") == NULL ||
       !crazypod_miniapp_native_diagnostics(&snapshot))
        return;
    mkdir("/.crazypod");
    file = open("/.crazypod/scenario-profile.json",
                O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(file < 0)
        return;
    length = snprintf(
        line, sizeof(line),
        "{\"format\":1,\"memoryUsed\":%lu,\"memoryHighWater\":%lu,"
        "\"memoryLimit\":%lu,\"uiHandlesUsed\":%lu,"
        "\"uiHandlesHighWater\":%lu,\"updateLastMs\":%lu,"
        "\"updateMaxMs\":%lu,\"logCount\":%lu,\"logDropped\":%lu}\n",
        (unsigned long)snapshot.memory_used,
        (unsigned long)snapshot.memory_high_water,
        (unsigned long)snapshot.memory_limit,
        (unsigned long)snapshot.ui_handles_used,
        (unsigned long)snapshot.ui_handles_high_water,
        (unsigned long)snapshot.update_last_ms,
        (unsigned long)snapshot.update_max_ms,
        (unsigned long)snapshot.log_count,
        (unsigned long)snapshot.log_dropped);
    if(length > 0)
        (void)write(file, line,
            (size_t)length < sizeof(line) ? (size_t)length : sizeof(line) - 1u);
    close(file);
}

static bool open_now_playing_theme_snapshot(
    const struct crazypod_simulator_snapshot_host *host,
    const char *id)
{
    bool custom = id != NULL;
    (void)crazypod_now_playing_theme_choice_count();
    int theme_index = custom
        ? crazypod_now_playing_themes_find(id) : -1;
    int index = custom ? theme_index + 1 : 0;

    if((custom && theme_index < 0) ||
       !crazypod_now_playing_theme_select(index)) {
        fprintf(stderr,
                "CrazyPod now-playing theme smoke failed: index=%d count=%d\n",
                theme_index, crazypod_now_playing_themes_count());
        return false;
    }
    host->open_root_route(MUSIC_ROUTE_NOW_PLAYING);
    if(custom && !crazypod_now_playing_theme_open())
        fprintf(stderr,
                "CrazyPod now-playing theme open failed: %d\n",
                crazypod_now_playing_theme_last_error());
    return custom
        ? crazypod_now_playing_theme_open()
        : !crazypod_now_playing_theme_open();
}

static bool cycle_all_now_playing_themes(
    const struct crazypod_simulator_snapshot_host *host)
{
    int count = crazypod_now_playing_theme_choice_count();
    int index;

    if(count <= 1)
        return false;
    for(index = 1; index < count; ++index) {
        if(!crazypod_now_playing_theme_select(index))
            return false;
        host->open_root_route(MUSIC_ROUTE_NOW_PLAYING);
        if(!crazypod_now_playing_theme_open()) {
            fprintf(stderr,
                    "CrazyPod theme cycle failed: index=%d error=%d\n",
                    index, crazypod_now_playing_theme_last_error());
            return false;
        }
        host->render(false);
    }
    return true;
}

static bool open_now_playing_theme_from_home_hold(void)
{
    int theme_index;
    const struct route_state *route;

    (void)crazypod_now_playing_theme_choice_count();
    theme_index = crazypod_now_playing_themes_find(
        "now-playing-neon");
    if(theme_index < 0 ||
       !crazypod_now_playing_theme_select(theme_index + 1) ||
       crazypod_shell_product_active())
        return false;

    crazypod_app_input_handle(BUTTON_MENU, 0, current_tick);
    crazypod_app_input_tick(current_tick + HZ / 2, false);
    route = crazypod_ui_routes_current();
    if(route == NULL || route->route != MUSIC_ROUTE_NOW_PLAYING ||
       !crazypod_now_playing_theme_open())
        return false;

    crazypod_app_input_handle(
        BUTTON_MENU | BUTTON_REPEAT, 0, current_tick + HZ / 2 + 1);
    crazypod_app_input_handle(
        BUTTON_MENU | BUTTON_REL, 0, current_tick + HZ / 2 + 2);
    route = crazypod_ui_routes_current();
    return route != NULL && route->route == MUSIC_ROUTE_NOW_PLAYING &&
        crazypod_now_playing_theme_open();
}

static bool exercise_now_playing_theme_controls(
    const struct crazypod_simulator_snapshot_host *host)
{
    static const long buttons[] = {
        BUTTON_SCROLL_FWD,
        BUTTON_SCROLL_BACK,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_SELECT,
        BUTTON_PLAY,
    };
    unsigned int index;

    if(!open_now_playing_theme_snapshot(
           host, "now-playing-neon"))
        return false;
    for(index = 0; index <
         sizeof(buttons) / sizeof(buttons[0]); ++index) {
        struct crazypod_input_event event =
            crazypod_input_event_make(buttons[index], 0);

        if(!crazypod_now_playing_theme_handle_input(&event) ||
           !crazypod_now_playing_theme_open())
            return false;
    }
    host->render(false);
    return crazypod_now_playing_theme_open();
}

static bool exercise_signal_theme_controls(
    const struct crazypod_simulator_snapshot_host *host)
{
    static const long buttons[] = {
        BUTTON_SCROLL_FWD,
        BUTTON_SCROLL_BACK,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_SELECT,
        BUTTON_PLAY,
    };
    unsigned int index;

    if(!open_now_playing_theme_snapshot(
           host, "now-playing-signal"))
        return false;
    for(index = 0; index <
         sizeof(buttons) / sizeof(buttons[0]); ++index) {
        struct crazypod_input_event event =
            crazypod_input_event_make(buttons[index], 0);

        if(!crazypod_now_playing_theme_handle_input(&event) ||
           !crazypod_now_playing_theme_open())
            return false;
    }
    host->render(false);
    return crazypod_now_playing_theme_open();
}

static bool open_signal_theme_panel(
    const struct crazypod_simulator_snapshot_host *host,
    int action)
{
    struct crazypod_input_event right =
        crazypod_input_event_make(BUTTON_RIGHT, 0);
    struct crazypod_input_event select =
        crazypod_input_event_make(BUTTON_SELECT, 0);
    int index;

    if(crazypod_music_track_count() < 2 ||
       !crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0) ||
       !open_now_playing_theme_snapshot(host, "now-playing-signal") ||
       !crazypod_now_playing_theme_handle_input(&select))
        return false;
    if(action >= 0) {
        for(index = 0; index < action; ++index)
            if(!crazypod_now_playing_theme_handle_input(&right))
                return false;
        if(!crazypod_now_playing_theme_handle_input(&select))
            return false;
    }
    host->render(false);
    return crazypod_now_playing_theme_open();
}

static bool exercise_signal_theme_panels(
    const struct crazypod_simulator_snapshot_host *host)
{
    struct crazypod_input_event right =
        crazypod_input_event_make(BUTTON_RIGHT, 0);
    struct crazypod_input_event select =
        crazypod_input_event_make(BUTTON_SELECT, 0);
    struct crazypod_input_event menu =
        crazypod_input_event_make(BUTTON_MENU, 0);
    struct crazypod_input_event wheel =
        crazypod_input_event_make(BUTTON_SCROLL_FWD, 0);

    if(crazypod_music_track_count() < 2 ||
       !crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0) ||
       !open_now_playing_theme_snapshot(host, "now-playing-signal") ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible() ||
       !crazypod_now_playing_theme_handle_input(&menu) ||
       !crazypod_miniapps_feature_modal_visible())
        return false;

    if(!crazypod_now_playing_theme_handle_input(&right) ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible() ||
       !crazypod_now_playing_theme_handle_input(&right) ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible() ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible())
        return false;

    if(!crazypod_now_playing_theme_handle_input(&right) ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible() ||
       !crazypod_now_playing_theme_handle_input(&menu) ||
       !crazypod_miniapps_feature_modal_visible() ||
       !crazypod_now_playing_theme_handle_input(&right) ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible() ||
       !crazypod_now_playing_theme_handle_input(&wheel) ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible() ||
       !crazypod_now_playing_theme_handle_input(&menu) ||
       crazypod_miniapps_feature_modal_visible())
        return false;

    host->render(false);
    return crazypod_now_playing_theme_open();
}

static bool open_now_playing_theme_panel(
    const struct crazypod_simulator_snapshot_host *host,
    bool queue)
{
    struct crazypod_input_event select =
        crazypod_input_event_make(BUTTON_SELECT, 0);

    if(crazypod_music_track_count() < 2 ||
       !crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0) ||
       !open_now_playing_theme_snapshot(
           host, "now-playing-neon") ||
       !crazypod_now_playing_theme_handle_input(&select))
        return false;
    if(queue && !crazypod_now_playing_theme_handle_input(&select))
        return false;
    host->render(false);
    return crazypod_now_playing_theme_open();
}

static bool scroll_now_playing_theme_panel_repeated(
    const struct crazypod_simulator_snapshot_host *host)
{
    struct crazypod_input_event wheel =
        crazypod_input_event_make(
            BUTTON_SCROLL_FWD | BUTTON_REPEAT, 0);

    if(!open_now_playing_theme_panel(host, false) ||
       !crazypod_now_playing_theme_handle_input(&wheel) ||
       !crazypod_now_playing_theme_handle_input(&wheel) ||
       !crazypod_now_playing_theme_handle_input(&wheel))
        return false;
    return crazypod_miniapps_feature_input_count() == 1 &&
        crazypod_now_playing_theme_open();
}

static bool exercise_now_playing_transform_layers(
    const struct crazypod_simulator_snapshot_host *host,
    bool playback_mode)
{
    const char *theme_id = getenv("CRAZYPOD_SIM_THEME_ID");
    struct crazypod_input_event select =
        crazypod_input_event_make(BUTTON_SELECT, 0);
    struct crazypod_input_event right =
        crazypod_input_event_make(BUTTON_RIGHT, 0);
    struct crazypod_input_event wheel =
        crazypod_input_event_make(BUTTON_SCROLL_FWD, 0);

    if(theme_id == NULL || theme_id[0] == '\0' ||
       !open_now_playing_theme_snapshot(host, theme_id) ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible())
        return false;
    if(playback_mode) {
        if(!crazypod_now_playing_theme_handle_input(&right) ||
           !crazypod_now_playing_theme_handle_input(&right) ||
           !crazypod_now_playing_theme_handle_input(&select) ||
           !crazypod_now_playing_theme_handle_input(&wheel) ||
           !crazypod_now_playing_theme_handle_input(&wheel) ||
           !crazypod_now_playing_theme_handle_input(&wheel))
            return false;
    }
    else if(!crazypod_now_playing_theme_handle_input(&wheel) ||
            !crazypod_now_playing_theme_handle_input(&wheel))
        return false;
    host->render(false);
    return crazypod_now_playing_theme_open() &&
        crazypod_miniapps_feature_modal_visible();
}

static bool return_from_saved_seek_with_menu(
    const struct crazypod_simulator_snapshot_host *host)
{
    struct crazypod_input_event right =
        crazypod_input_event_make(BUTTON_RIGHT, 0);
    struct crazypod_input_event select =
        crazypod_input_event_make(BUTTON_SELECT, 0);
    struct crazypod_input_event menu =
        crazypod_input_event_make(BUTTON_MENU, 0);
    unsigned index;

    if(!open_now_playing_theme_panel(host, false))
        return false;
    for(index = 0; index < 4; ++index)
        if(!crazypod_now_playing_theme_handle_input(&right))
            return false;
    if(!crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible() ||
       !crazypod_now_playing_theme_handle_input(&select) ||
       !crazypod_miniapps_feature_modal_visible() ||
       !crazypod_now_playing_theme_handle_input(&menu) ||
       crazypod_miniapps_feature_modal_visible())
        return false;
    host->render(false);
    return crazypod_now_playing_theme_open();
}

static bool exit_now_playing_theme_with_menu(
    const struct crazypod_simulator_snapshot_host *host)
{
    struct crazypod_input_event menu =
        crazypod_input_event_make(BUTTON_MENU, 0);

    if(!open_now_playing_theme_snapshot(
           host, "now-playing-neon") ||
       crazypod_now_playing_theme_handle_input(&menu))
        return false;
    host->pop_route();
    return !crazypod_now_playing_theme_open();
}

static bool open_now_playing_theme_media(
    const struct crazypod_simulator_snapshot_host *host,
    unsigned selected_index)
{
    const char *theme_id = getenv("CRAZYPOD_SIM_THEME_ID");

    if(theme_id == NULL || theme_id[0] == '\0')
        theme_id = "now-playing-neon";
    if(crazypod_music_track_count() < 2 ||
       selected_index >= (unsigned)crazypod_music_track_count() ||
       !crazypod_music_play(
           CRAZYPOD_SCOPE_ALL, 0, (int)selected_index) ||
       !open_now_playing_theme_snapshot(
           host, theme_id))
        return false;
    return crazypod_now_playing_theme_open();
}

static bool open_now_playing_theme_media_next(
    const struct crazypod_simulator_snapshot_host *host)
{
    struct crazypod_input_event event =
        crazypod_input_event_make(BUTTON_RIGHT, 0);

    return open_now_playing_theme_media(host, 0) &&
        crazypod_now_playing_theme_handle_input(&event) &&
        crazypod_now_playing_theme_open();
}

static bool open_now_playing_theme_real_library(
    const struct crazypod_simulator_snapshot_host *host,
    const char *id)
{
    crazypod_music_cancel_scan();
    crazypod_music_scan();
    if(crazypod_music_track_count() <= 0 ||
       !crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0))
        return false;
    return open_now_playing_theme_snapshot(host, id);
}

static bool exercise_now_playing_theme_media_controls(
    const struct crazypod_simulator_snapshot_host *host)
{
    if(!open_now_playing_theme_media(host, 0))
        return false;
    return exercise_now_playing_theme_controls(host);
}

static bool build_now_playing_theme_media_catalog(
    const struct crazypod_simulator_snapshot_host *host)
{
    crazypod_music_cancel_scan();
    crazypod_music_scan();
    return crazypod_music_track_count() >= 2 &&
        open_now_playing_theme_snapshot(host, NULL);
}

static bool open_now_playing_default_media(
    const struct crazypod_simulator_snapshot_host *host)
{
    return crazypod_music_track_count() >= 2 &&
        crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0) &&
        open_now_playing_theme_snapshot(host, NULL);
}

static bool show_miniapp_exit_prompt(
    const struct crazypod_simulator_snapshot_host *host)
{
    if(!crazypod_miniapps_feature_simulate_long_menu(
           current_tick, HZ))
        return false;
    host->render(false);
    return crazypod_miniapps_feature_exit_prompt_visible();
}

static struct crazypod_simulator_snapshot_host capability_action_host;

static void service_capability_action(lv_timer_t *timer)
{
    struct cp_input_event event = {
        .struct_size = sizeof(event),
        .type = CP_INPUT_SELECT,
        .steps = 1,
    };

    (void)crazypod_miniapps_event(&event);
    capability_action_host.render(false);
    lv_timer_delete(timer);
}

static bool open_capability_action(
    const struct crazypod_simulator_snapshot_host *host,
    int page)
{
    if(!open_miniapp_snapshot(host, "capability-lab", page))
        return false;
    capability_action_host = *host;
    lv_timer_create(service_capability_action, 250, NULL);
    return crazypod_miniapps_is_open();
}

static bool open_capability_directory(
    const struct crazypod_simulator_snapshot_host *host,
    int focused_entry)
{
    struct cp_input_event event = {
        .struct_size = sizeof(event),
        .type = CP_INPUT_WHEEL_CLOCKWISE,
        .steps = (uint8_t)focused_entry,
    };

    if(!open_miniapp_snapshot(host, "capability-lab", -1))
        return false;
    if(focused_entry > 0)
        (void)crazypod_miniapps_event(&event);
    return crazypod_miniapps_is_open();
}

struct game2048_scene_event {
    uint8_t type;
    uint8_t steps;
};

static void schedule_game2048_scene(
    const struct crazypod_simulator_snapshot_host *host,
    const struct game2048_scene_event *events, uint8_t count,
    bool show_pause);

static bool send_miniapp_event(
    const struct crazypod_simulator_snapshot_host *host,
    uint8_t type, int steps)
{
    struct cp_input_event event = {
        .struct_size = sizeof(event),
        .type = type,
        .steps = (uint8_t)(steps > 0 ? steps : 1),
    };

    bool handled = crazypod_miniapps_event(&event);

    host->render(false);
    lv_refr_now(NULL);
    crazypod_present_now();
    return handled;
}

static struct {
    struct crazypod_simulator_snapshot_host host;
    struct game2048_scene_event events[4];
    uint8_t count;
    uint8_t index;
    bool show_pause;
} game2048_scene_plan;

static void service_game2048_scene(lv_timer_t *timer)
{
    while(game2048_scene_plan.index < game2048_scene_plan.count &&
          crazypod_miniapps_is_open()) {
        const struct game2048_scene_event *event =
            &game2048_scene_plan.events[game2048_scene_plan.index++];

        (void)send_miniapp_event(
            &game2048_scene_plan.host,
            event->type, event->steps);
    }
    if(game2048_scene_plan.show_pause &&
       crazypod_miniapps_is_open()) {
        (void)crazypod_miniapps_ui_event(
            3, CP_UI_EVENT_SELECT, 0, 0);
        game2048_scene_plan.host.render(false);
    }
    lv_timer_delete(timer);
}

static void schedule_game2048_scene(
    const struct crazypod_simulator_snapshot_host *host,
    const struct game2048_scene_event *events, uint8_t count,
    bool show_pause)
{
    game2048_scene_plan.host = *host;
    memcpy(
        game2048_scene_plan.events,
        events,
        count * sizeof(*events));
    game2048_scene_plan.count = count;
    game2048_scene_plan.index = 0;
    game2048_scene_plan.show_pause = show_pause;
    lv_timer_create(service_game2048_scene, 150, NULL);
}

static bool open_game2048_scene(
    const struct crazypod_simulator_snapshot_host *host,
    const char *scene)
{
    struct game2048_scene_event events[3];
    uint8_t count = 0;

    if(!open_miniapp_snapshot(host, "game2048", -1))
        return false;
    if(strcmp(scene, "game2048") == 0)
        return true;
    if(strcmp(scene, "game2048-exit-prompt") == 0)
        return show_miniapp_exit_prompt(host);
    if(strcmp(scene, "game2048-game") == 0 ||
       strcmp(scene, "game2048-moved") == 0 ||
       strcmp(scene, "game2048-pause") == 0)
        events[count++] = (struct game2048_scene_event) {
            CP_INPUT_SELECT, 1
        };
    else
        return false;
    if(strcmp(scene, "game2048-moved") == 0) {
        events[count++] = (struct game2048_scene_event) {
            CP_INPUT_LEFT, 1
        };
        events[count++] = (struct game2048_scene_event) {
            CP_INPUT_RIGHT, 1
        };
    }
    schedule_game2048_scene(
        host, events, count,
        strcmp(scene, "game2048-pause") == 0);
    return true;
}

static bool open_game2048_exit_scene(
    const struct crazypod_simulator_snapshot_host *host,
    bool open_notes)
{
    if(host->pop_route == NULL ||
       !open_miniapp_snapshot(host, "game2048", -1))
        return false;
    host->pop_route();
    if(current_route() == NULL ||
       current_route()->route != UTILITIES_ROUTE_MENU ||
       crazypod_miniapps_is_open())
        return false;
    if(!open_notes)
        return true;
    host->pop_route();
    host->open_app(CRAZYPOD_APP_NOTES);
    return current_route() != NULL &&
        current_route()->route == NOTES_ROUTE_MENU;
}

bool crazypod_simulator_snapshot_prepare(
    const struct crazypod_simulator_snapshot_host *host)
{
    const char *screen = getenv("CRAZYPOD_SIM_SCREEN");
    const char *language = getenv("CRAZYPOD_SIM_LANGUAGE");
    int preview_index;
    int language_index;

    if(host == NULL || getenv("CRAZYPOD_SIM_DUMP") == NULL)
        return false;
    if(language != NULL) {
        for(language_index = 0;
            language_index < CRAZYPOD_LANGUAGE_COUNT;
            ++language_index) {
            if(strcmp(language,
                      crazypod_language_code(
                          (enum crazypod_language)language_index)) == 0) {
                crazypod_language_set(
                    (enum crazypod_language)language_index);
                break;
            }
        }
    }
    if(screen == NULL || strcmp(screen, "home") == 0)
        return true;
    if(strcmp(screen, "runtime-font-catalog") == 0)
        return open_runtime_font_catalog();
    if(strcmp(screen, "power") == 0)
        host->show_power_prompt();
    else if(sscanf(screen, "music-%d", &preview_index) == 1) {
        host->open_app(CRAZYPOD_APP_MUSIC);
        if(current_route() != NULL &&
           current_route()->route == MUSIC_ROUTE_MENU)
            select_bounded(host, preview_index);
    }
    else if(sscanf(screen, "play-video-%d", &preview_index) == 1) {
        enum crazypod_video_result result;
        int count;

        open_photos_route(host, PHOTOS_ROUTE_VIDEOS);
        count = item_count();
        select_bounded(host, preview_index);
        lv_refr_now(NULL);
        crazypod_present_now();
        if(count > 0) {
            result = crazypod_video_play(current_route()->selected);
            fprintf(stderr, "CrazyPod video smoke: %d (%s)\n",
                    result, crazypod_video_result_message(result));
        }
        host->render(false);
        return false;
    }
    else if(sscanf(screen, "videos-%d", &preview_index) == 1) {
        open_photos_route(host, PHOTOS_ROUTE_VIDEOS);
        select_bounded(host, preview_index);
    }
    else if(sscanf(screen, "media-%d", &preview_index) == 1 ||
            sscanf(screen, "photos-%d", &preview_index) == 1) {
        host->open_app(CRAZYPOD_APP_PHOTOS);
        if(preview_index < 0)
            preview_index = 0;
        if(preview_index > 3)
            preview_index = 3;
        current_route()->selected = preview_index;
        host->render(false);
    }
    else if(sscanf(screen, "delete-photos-%d", &preview_index) == 1) {
        open_photos_route(host, PHOTOS_ROUTE_DELETE_PHOTOS);
        select_bounded(host, preview_index);
    }
    else if(sscanf(
                screen, "delete-photo-confirm-%d",
                &preview_index) == 1) {
        open_photos_route(host, PHOTOS_ROUTE_DELETE_PHOTOS);
        select_bounded(host, preview_index);
        if(item_count() > 0)
            host->activate_selected();
    }
    else if(sscanf(screen, "books-%d", &preview_index) == 1) {
        host->open_app(CRAZYPOD_APP_BOOKS);
        select_bounded(host, preview_index);
    }
    else if(strcmp(screen, "notes-new") == 0) {
        host->open_app(CRAZYPOD_APP_NOTES);
        select_bounded(host, 0);
    }
    else if(strcmp(screen, "notes-draft") == 0) {
        host->open_app(CRAZYPOD_APP_NOTES);
        select_bounded(
            host, crazypod_notes_feature_draft_available() ? 1 : 0);
    }
    else if(strcmp(screen, "notes-item") == 0) {
        host->open_app(CRAZYPOD_APP_NOTES);
        select_bounded(
            host, crazypod_notes_count(false) > 0
                ? notes_home_note_start() : 0);
    }
    else if(strcmp(screen, "notes-search") == 0) {
        host->open_app(CRAZYPOD_APP_NOTES);
        select_bounded(host, notes_home_deleted_index() - 1);
    }
    else if(strcmp(screen, "notes-deleted") == 0) {
        host->open_app(CRAZYPOD_APP_NOTES);
        select_bounded(host, notes_home_deleted_index());
    }
    else if(strcmp(screen, "more") == 0)
        host->open_app(CRAZYPOD_APP_EXTRAS);
    else if(strcmp(screen, "more-second") == 0) {
        host->open_app(CRAZYPOD_APP_EXTRAS);
        select_bounded(host, 1);
    }
    else if(strcmp(screen, "settings-main-menu") == 0)
        host->open_root_route(SETTINGS_ROUTE_MAIN_MENU);
    else if(strcmp(screen, "settings-language") == 0) {
        host->open_root_route(SETTINGS_ROUTE_MENU);
        select_bounded(host, item_count() - 1);
        host->activate_selected();
    }
    else if(strcmp(screen, "settings-reduce-motion") == 0) {
        host->open_root_route(SETTINGS_ROUTE_DISPLAY);
        select_bounded(host, item_count() - 1);
    }
    else if(strcmp(screen, "notes") == 0)
        host->open_app(CRAZYPOD_APP_NOTES);
    else if(strcmp(screen, "miniapps-list") == 0)
        host->open_app(CRAZYPOD_APP_MINI_APPS);
    else if(strcmp(screen, "customize") == 0)
        host->open_app(CRAZYPOD_APP_CUSTOMIZE);
    else if(strcmp(screen, "now-playing-theme") == 0)
        return open_now_playing_theme_snapshot(
            host, "now-playing-neon");
    else if(strcmp(screen, "now-playing-theme-cycle-all") == 0)
        return cycle_all_now_playing_themes(host);
    else if(strcmp(screen, "now-playing-theme-home-hold") == 0)
        return open_now_playing_theme_from_home_hold();
    else if(strcmp(screen, "now-playing-theme-rerender") == 0) {
        if(!open_now_playing_theme_snapshot(
               host, "now-playing-neon"))
            return false;
        host->render(false);
        return crazypod_now_playing_theme_open() &&
            crazypod_miniapps_feature_surface_attached(
                crazypod_shell_product_content());
    }
    else if(strcmp(screen, "now-playing-theme-controls") == 0)
        return exercise_now_playing_theme_controls(host);
    else if(strcmp(screen, "now-playing-signal-controls") == 0)
        return exercise_signal_theme_controls(host);
    else if(strcmp(screen, "now-playing-signal-all-panels") == 0)
        return exercise_signal_theme_panels(host);
    else if(strcmp(screen, "now-playing-signal-panel") == 0)
        return open_signal_theme_panel(host, -1);
    else if(strcmp(screen, "now-playing-signal-queue") == 0)
        return open_signal_theme_panel(host, 0);
    else if(strcmp(screen, "now-playing-signal-mode") == 0)
        return open_signal_theme_panel(host, 2);
    else if(strcmp(screen, "now-playing-signal-lyrics") == 0)
        return open_signal_theme_panel(host, 3);
    else if(strcmp(screen, "now-playing-signal-seek") == 0)
        return open_signal_theme_panel(host, 4);
    else if(strcmp(screen, "now-playing-theme-panel") == 0)
        return open_now_playing_theme_panel(host, false);
    else if(strcmp(screen, "now-playing-theme-panel-repeat") == 0)
        return scroll_now_playing_theme_panel_repeated(host);
    else if(strcmp(screen, "now-playing-transform-layers") == 0)
        return exercise_now_playing_transform_layers(host, false);
    else if(strcmp(screen, "now-playing-transform-mode-layers") == 0)
        return exercise_now_playing_transform_layers(host, true);
    else if(strcmp(screen, "now-playing-theme-seek-menu-back") == 0)
        return return_from_saved_seek_with_menu(host);
    else if(strcmp(screen, "now-playing-theme-queue") == 0)
        return open_now_playing_theme_panel(host, true);
    else if(strcmp(screen, "now-playing-theme-menu-exit") == 0)
        return exit_now_playing_theme_with_menu(host);
    else if(strcmp(screen, "now-playing-theme-media-controls") == 0)
        return exercise_now_playing_theme_media_controls(host);
    else if(strcmp(screen, "now-playing-theme-media") == 0)
        return open_now_playing_theme_media(host, 0);
    else if(strcmp(screen, "now-playing-theme-media-next") == 0)
        return open_now_playing_theme_media_next(host);
    else if(strcmp(screen, "now-playing-theme-media-catalog") == 0)
        return build_now_playing_theme_media_catalog(host);
    else if(strcmp(screen, "now-playing-default-media") == 0)
        return open_now_playing_default_media(host);
    else if(strcmp(screen, "now-playing-default") == 0)
        return open_now_playing_theme_snapshot(host, NULL);
    else if(strcmp(screen, "note-compose") == 0) {
        host->open_app(CRAZYPOD_APP_NOTES);
        host->begin_note_composer(0, false);
    }
    else if(strcmp(screen, "books") == 0)
        host->open_app(CRAZYPOD_APP_BOOKS);
    else if(strcmp(screen, "books-reading") == 0) {
        host->open_app(CRAZYPOD_APP_BOOKS);
        select_bounded(host, (books_has_continue() ? 1 : 0) + 4);
    }
    else if(strcmp(screen, "book-reader") == 0) {
        host->open_app(CRAZYPOD_APP_BOOKS);
        if(crazypod_books_count() > 0)
            crazypod_books_feature_begin_reader(0, 0);
    }
    else if(strcmp(screen, "book-reader-next") == 0) {
        host->open_app(CRAZYPOD_APP_BOOKS);
        if(crazypod_books_count() > 0) {
            crazypod_books_feature_begin_reader(0, 0);
            crazypod_books_feature_turn_page(1);
        }
    }
    else if(strcmp(screen, "clock") == 0)
        host->open_root_route(CLOCK_ROUTE_VIEW);
    else if(strcmp(screen, "stopwatch") == 0)
        host->open_root_route(STOPWATCH_ROUTE_VIEW);
    else if(strcmp(screen, "workouts") == 0)
        host->open_root_route(WORKOUT_ROUTE_MENU);
    else if(strcmp(screen, "workout-ready") == 0) {
        crazypod_organizer_feature_simulator_workout(
            0, 0, current_tick, false);
        host->open_root_route(WORKOUT_ROUTE_READY);
    }
    else if(strcmp(screen, "workout-active") == 0) {
        crazypod_organizer_feature_simulator_workout(
            0, 62 * HZ, current_tick, true);
        host->open_root_route(WORKOUT_ROUTE_ACTIVE);
    }
    else if(strcmp(screen, "workout-detail") == 0) {
        host->open_root_route(WORKOUT_ROUTE_HISTORY);
        if(crazypod_workouts_count() > 0)
            host->push_route(WORKOUT_ROUTE_DETAIL, 0);
    }
    else if(strcmp(screen, "calendar") == 0) {
        host->open_app(CRAZYPOD_APP_CALENDAR);
        host->push_route(CALENDAR_ROUTE_MONTH, -1);
    }
    else if(strcmp(screen, "calendar-day") == 0) {
        host->open_app(CRAZYPOD_APP_CALENDAR);
        host->show_calendar_day(
            crazypod_organizer_feature_focus_date());
        host->render(true);
    }
    else if(strcmp(screen, "contacts") == 0)
        host->open_app(CRAZYPOD_APP_CONTACTS);
    else if(strcmp(screen, "contact-detail") == 0) {
        host->open_app(CRAZYPOD_APP_CONTACTS);
        if(crazypod_contacts_count() > 0)
            host->push_route(CONTACTS_ROUTE_DETAIL, 0);
    }
    else if(strcmp(screen, "game2048-exit-list") == 0)
        return open_game2048_exit_scene(host, false);
    else if(strcmp(screen, "game2048-exit-notes") == 0)
        return open_game2048_exit_scene(host, true);
    else if(strncmp(screen, "game2048", 8) == 0)
        return open_game2048_scene(host, screen);
    else if(strcmp(screen, "capability-lab") == 0)
        return open_capability_directory(host, 0);
    else if(strcmp(screen, "capability-lab-controls") == 0)
        return open_miniapp_snapshot(
            host, "capability-lab", 0);
    else if(strcmp(screen, "capability-lab-assets") == 0)
        return open_miniapp_snapshot(
            host, "capability-lab", 1);
    else if(strcmp(screen, "capability-lab-lifecycle") == 0)
        return open_miniapp_snapshot(
            host, "capability-lab", 2);
    else if(strcmp(screen, "capability-lab-modal") == 0)
        return open_capability_action(host, 2);
    else if(strcmp(screen, "native-reference-clicked") == 0) {
        if(!open_miniapp_snapshot(host, "native-reference", -1))
            return false;
        (void)crazypod_miniapps_ui_event(
            1, CP_UI_EVENT_SELECT, 0, 0);
        (void)crazypod_miniapps_ui_event(
            1, CP_UI_EVENT_SELECT, 0, 0);
        host->render(false);
        return crazypod_miniapps_is_open();
    }
    else if(strncmp(
                screen, "now-playing-theme-media-step-", 29) == 0) {
        char *end = NULL;
        long step = strtol(screen + 29, &end, 10);

        if(end == screen + 29 || *end != '\0' ||
           step < 0 || step >= crazypod_music_track_count())
            return false;
        return open_now_playing_theme_media(host, (unsigned)step);
    }
    else if(crazypod_now_playing_theme_choice_count() > 1 &&
            crazypod_now_playing_themes_find(screen) >= 0)
        return getenv("CRAZYPOD_SIM_REAL_LIBRARY") != NULL
            ? open_now_playing_theme_real_library(host, screen)
            : open_now_playing_theme_snapshot(host, screen);
    else
        return open_miniapp_snapshot(host, screen, -1);
    return true;
}

#endif
