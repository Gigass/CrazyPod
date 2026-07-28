#include "config.h"

#ifdef IPOD_6G

#include "backlight.h"
#include "kernel.h"
#include "timefuncs.h"

#include "../../crazypod_books.h"
#include "../../crazypod_music.h"
#include "../features/books/crazypod_book_reader_input.h"
#include "../features/books/crazypod_book_session.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/miniapps/crazypod_miniapp_input.h"
#include "../features/music/crazypod_music_input.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_input.h"
#include "../features/organizer/crazypod_activity_input.h"
#include "../features/organizer/crazypod_calendar_input.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../features/settings/crazypod_eq_studio_input.h"
#include "../navigation/crazypod_input_event.h"
#include "crazypod_feature_input.h"
#include "crazypod_route_actions.h"

static struct crazypod_feature_input_host host;

static void activate(void);
static void move(int direction);

static int today_date(void)
{
    struct tm *now = get_time();

    return (now->tm_year + 1900) * 10000 +
        (now->tm_mon + 1) * 100 + now->tm_mday;
}

static void render(void)
{
    host.render(false);
}

static void show_music_search_results(void)
{
    if(crazypod_music_search_count(
           crazypod_music_search_query()) > 0)
        crazypod_route_actions_push(
            MUSIC_ROUTE_SEARCH_RESULTS, -1);
}

static void toggle_bookmark(void)
{
    crazypod_book_toggle_bookmark(
        crazypod_book_session_index(),
        crazypod_book_session_offset());
    host.render(false);
}

static void show_workout_confirmation(void)
{
    crazypod_route_actions_push(
        WORKOUT_ROUTE_FINISH_CONFIRM, -1);
}

static void show_notes_exit_actions(void)
{
    crazypod_route_actions_push(
        NOTES_ROUTE_EXIT_ACTIONS, -1);
}

static void show_notes_search_results(void)
{
    crazypod_route_actions_push(
        NOTES_ROUTE_SEARCH_RESULTS, -1);
}

static bool handle_customize_raw(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    (void)context;
    return state->route == DIY_ROUTE_WALLPAPER_CROP &&
        crazypod_wallpaper_crop_runtime_handle_input(
            event, host.now());
}

static bool handle_photos_raw(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    (void)context;
    if(state->route != PHOTOS_ROUTE_MENU &&
       state->route != PHOTOS_ROUTE_LIBRARY &&
       state->route != PHOTOS_ROUTE_FAVORITES &&
       state->route != PHOTOS_ROUTE_DETAIL)
        return false;
    return crazypod_photos_runtime_handle_input(
        (struct route_state *)state, event, host.now());
}

static bool handle_miniapps_raw(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_miniapp_input_actions actions = {
        .wake_display = backlight_on,
        .keep_boosted = host.boost,
        .close = crazypod_route_actions_pop,
    };

    (void)state;
    (void)context;
    return crazypod_miniapp_input_handle(
        event, HZ / 10, &actions);
}

static bool handle_settings_pressed(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_eq_studio_input_actions actions = {
        .render = render,
        .leave = crazypod_route_actions_pop,
    };

    (void)context;
    if(state->route != SETTINGS_ROUTE_EQ_STUDIO)
        return false;
    crazypod_eq_studio_input_handle(event, &actions);
    return true;
}

static bool handle_books_pressed(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_book_reader_input_actions actions = {
        .turn_page = crazypod_book_session_turn,
        .activate = activate,
        .toggle_bookmark = toggle_bookmark,
        .leave = crazypod_route_actions_pop,
    };

    (void)context;
    if(state->route != BOOKS_ROUTE_READER)
        return false;
    crazypod_book_reader_input_handle(event, &actions);
    return true;
}

static void activate(void)
{
    crazypod_route_actions_activate(host.now());
}

static void move(int direction)
{
    crazypod_route_actions_move(direction, host.now());
}

static bool handle_organizer_pressed(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_activity_input_actions activity = {
        .activate = activate,
        .render = render,
        .show_finish_confirmation =
            show_workout_confirmation,
        .leave = crazypod_route_actions_pop,
    };
    const struct crazypod_calendar_input_actions calendar = {
        .move_selection = move,
        .activate = activate,
        .render = render,
        .leave = crazypod_route_actions_pop,
    };

    (void)context;
    if(crazypod_activity_input_handle(
           state->route, event, host.now(), HZ, &activity))
        return true;
    return crazypod_calendar_input_handle(
        state, event, today_date(), &calendar);
}

static bool handle_notes_pressed(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_notes_input_actions actions = {
        .move_selection = move,
        .activate = activate,
        .render = render,
        .leave = crazypod_route_actions_pop,
        .show_exit_actions = show_notes_exit_actions,
        .show_search_results = show_notes_search_results,
    };

    (void)context;
    return crazypod_notes_input_handle(
        state->route,
        crazypod_route_actions_note_dirty(),
        event, &actions);
}

static bool handle_music_pressed(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_music_input_actions actions = {
        .move_selection = move,
        .activate = activate,
        .render = render,
        .leave = crazypod_route_actions_pop,
        .show_search_results = show_music_search_results,
    };

    (void)context;
    return state->route == MUSIC_ROUTE_SEARCH &&
        crazypod_music_search_input_handle(
            event, &actions);
}

static const struct crazypod_feature_bindings bindings = {
    .raw = {
        [CRAZYPOD_FEATURE_PHOTOS] = handle_photos_raw,
        [CRAZYPOD_FEATURE_CUSTOMIZE] =
            handle_customize_raw,
        [CRAZYPOD_FEATURE_MINIAPPS] =
            handle_miniapps_raw,
    },
    .pressed = {
        [CRAZYPOD_FEATURE_MUSIC] =
            handle_music_pressed,
        [CRAZYPOD_FEATURE_BOOKS] =
            handle_books_pressed,
        [CRAZYPOD_FEATURE_NOTES] =
            handle_notes_pressed,
        [CRAZYPOD_FEATURE_ORGANIZER] =
            handle_organizer_pressed,
        [CRAZYPOD_FEATURE_SETTINGS] =
            handle_settings_pressed,
    },
};

void crazypod_feature_input_configure(
    const struct crazypod_feature_input_host *new_host)
{
    if(new_host != NULL)
        host = *new_host;
}

const struct crazypod_feature_bindings *
crazypod_feature_input_bindings(void)
{
    return &bindings;
}

#endif
