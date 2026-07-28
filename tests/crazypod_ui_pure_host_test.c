#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "features/organizer/crazypod_calendar_model.h"
#include "presentation/crazypod_ui_menu_layout.h"
#include "presentation/crazypod_ui_text.h"
#include "features/crazypod_feature.h"
#include "navigation/crazypod_feature_dispatcher.h"
#include "navigation/crazypod_navigation_command.h"
#include "navigation/crazypod_route_registry.h"

static void test_calendar(void)
{
    char time[6];

    assert(crazypod_ui_calendar_days_in_month(2024, 1) == 29);
    assert(crazypod_ui_calendar_days_in_month(2100, 1) == 28);
    assert(crazypod_ui_calendar_weekday(2026, 6, 28) == 2);
    assert(crazypod_ui_calendar_shift_date(20241231, 1) == 20250101);
    assert(crazypod_ui_calendar_shift_date(20240301, -1) == 20240229);
    assert(crazypod_ui_calendar_parse_minutes("09:30") == 570);
    assert(crazypod_ui_calendar_parse_minutes("bad") == -1);
    crazypod_ui_calendar_format_time(time, sizeof(time), 570);
    assert(strcmp(time, "09:30") == 0);
}

static void test_note_layout(void)
{
    char window[128];
    const char *body =
        "1234567890123456789012345678901234"
        "second line";

    assert(crazypod_ui_text_note_line_count("") == 1);
    assert(crazypod_ui_text_note_line_count(body) == 2);
    crazypod_ui_text_note_window(body, 1, window, sizeof(window));
    assert(strcmp(window, "second line") == 0);
}

static void test_editor(void)
{
    char text[16] = "ab";
    char cursor_text[16];
    size_t cursor = 1;

    crazypod_ui_text_insert(text, sizeof(text), &cursor, "中");
    assert(strcmp(text, "a中b") == 0);
    assert(cursor == 4);
    crazypod_ui_text_backspace_at(text, &cursor);
    assert(strcmp(text, "ab") == 0);
    assert(cursor == 1);

    crazypod_ui_text_append(text, sizeof(text), "文");
    assert(strcmp(text, "ab文") == 0);
    crazypod_ui_text_backspace(text);
    assert(strcmp(text, "ab") == 0);

    cursor = strlen("a中");
    crazypod_ui_text_move_cursor("a中b", &cursor, -1);
    assert(cursor == 1);
    crazypod_ui_text_move_cursor("a中b", &cursor, 1);
    assert(cursor == strlen("a中"));

    assert(strcmp(crazypod_ui_text_with_cursor(
                      "abc", 1, cursor_text, sizeof(cursor_text)),
                  "a|bc") == 0);
}

static void test_menu_layout(void)
{
    int thumb_y;
    int thumb_height;

    assert(crazypod_ui_menu_window_start(4, 3, 7) == 0);
    assert(crazypod_ui_menu_window_start(20, 0, 7) == 0);
    assert(crazypod_ui_menu_window_start(20, 10, 7) == 7);
    assert(crazypod_ui_menu_window_start(20, 19, 7) == 13);
    crazypod_ui_menu_scroll_thumb(
        20, 19, 7, 66, 164, 12,
        &thumb_y, &thumb_height);
    assert(thumb_height == 57);
    assert(thumb_y == 173);
}

static void test_route_registry(void)
{
    int route;

    assert(crazypod_route_registry_validate());
    for(route = 0; route < CRAZYPOD_ROUTE_COUNT; ++route)
        assert(crazypod_route_registry_get(route) != NULL);
    assert(crazypod_route_registry_is_shell(EXTRAS_ROUTE_MENU));
    assert(crazypod_route_registry_feature(MUSIC_ROUTE_MENU)->id ==
           CRAZYPOD_FEATURE_MUSIC);
    assert(crazypod_route_registry_feature(MUSIC_ROUTE_QUEUE)->id ==
           CRAZYPOD_FEATURE_NOW_PLAYING);
    assert(crazypod_route_registry_feature(
               DIY_ROUTE_WALLPAPER_CROP)->id ==
           CRAZYPOD_FEATURE_CUSTOMIZE);
    assert(crazypod_route_registry_feature(MINIAPP_ROUTE_VIEW)->id ==
           CRAZYPOD_FEATURE_MINIAPPS);
    assert(crazypod_route_registry_has_flag(
        CLOCK_ROUTE_VIEW, CRAZYPOD_ROUTE_FLAG_FULLSCREEN));
    assert(crazypod_route_registry_has_flag(
        CLOCK_ROUTE_VIEW, CRAZYPOD_ROUTE_FLAG_DARK_STATUS));
    assert(crazypod_route_registry_has_flag(
        DIY_ROUTE_WALLPAPER_CROP,
        CRAZYPOD_ROUTE_FLAG_SOLID_BLACK));
    assert(crazypod_route_registry_has_flag(
        BOOKS_ROUTE_READER, CRAZYPOD_ROUTE_FLAG_BOOK_READER));
    assert(!crazypod_route_registry_has_flag(
        MUSIC_ROUTE_MENU, CRAZYPOD_ROUTE_FLAG_FULLSCREEN));
}

static void test_navigation_commands(void)
{
    struct crazypod_navigation_command command =
        crazypod_navigation_push(NOTES_ROUTE_READER, 7, 3);

    assert(command.kind == CRAZYPOD_NAVIGATION_PUSH);
    assert(command.route == NOTES_ROUTE_READER);
    assert(command.group == 7);
    assert(command.selected == 3);
    assert(command.transition);

    command = crazypod_navigation_render(false);
    assert(command.kind == CRAZYPOD_NAVIGATION_RENDER);
    assert(!command.transition);
    assert(crazypod_navigation_pop().kind == CRAZYPOD_NAVIGATION_POP);
    assert(crazypod_navigation_none().kind == CRAZYPOD_NAVIGATION_NONE);
}

static bool test_music_input_handler(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    int *calls = context;

    assert(state->route == MUSIC_ROUTE_MENU);
    assert(event->raw == 42);
    ++*calls;
    return true;
}

static int activation_calls;
static int render_calls;

static bool test_music_activate_handler(
    const struct route_state *state)
{
    assert(state->route == MUSIC_ROUTE_MENU);
    ++activation_calls;
    return true;
}

static void test_music_render_handler(
    const struct route_state *state)
{
    assert(state->route == MUSIC_ROUTE_MENU);
    ++render_calls;
}

static void test_feature_input_dispatcher(void)
{
    struct route_state state = { MUSIC_ROUTE_MENU, 0, -1 };
    const struct crazypod_input_event event = {
        42, 42, 0, false, false
    };
    int calls = 0;
    const struct crazypod_feature_bindings bindings = {
        .pressed = {
            [CRAZYPOD_FEATURE_MUSIC] =
                test_music_input_handler,
        },
        .activate = {
            [CRAZYPOD_FEATURE_MUSIC] =
                test_music_activate_handler,
        },
        .render = {
            [CRAZYPOD_FEATURE_MUSIC] =
                test_music_render_handler,
        },
        .context = &calls,
    };

    assert(!crazypod_feature_input_dispatch(
        &state, &event, CRAZYPOD_FEATURE_INPUT_RAW, &bindings));
    assert(crazypod_feature_input_dispatch(
        &state, &event, CRAZYPOD_FEATURE_INPUT_PRESSED, &bindings));
    assert(calls == 1);
    assert(crazypod_feature_activate_dispatch(&state, &bindings));
    assert(activation_calls == 1);
    assert(crazypod_feature_render_dispatch(&state, &bindings));
    assert(render_calls == 1);
    state.route = EXTRAS_ROUTE_MENU;
    assert(!crazypod_feature_input_dispatch(
        &state, &event, CRAZYPOD_FEATURE_INPUT_PRESSED, &bindings));
    assert(!crazypod_feature_activate_dispatch(&state, &bindings));
    assert(!crazypod_feature_render_dispatch(&state, &bindings));
}

int main(void)
{
    test_calendar();
    test_note_layout();
    test_editor();
    test_menu_layout();
    test_route_registry();
    test_navigation_commands();
    test_feature_input_dispatcher();
    return 0;
}
