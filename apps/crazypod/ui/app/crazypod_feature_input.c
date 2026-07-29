#include "config.h"

#ifdef IPOD_6G

#include "backlight.h"
#include "kernel.h"
#include "timefuncs.h"

#include "../features/books/crazypod_books_feature.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../features/settings/crazypod_settings_feature.h"
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
    const struct crazypod_feature_input_context input = {
        .now = host.now(),
        .ticks_per_second = HZ,
        .wake_display = backlight_on,
        .boost = host.boost,
        .pop = crazypod_route_actions_pop,
    };

    (void)context;
    return crazypod_miniapps_feature_handle_input(
        state, event, &input);
}

static bool handle_settings_pressed(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_feature_input_context input = {
        .now = host.now(),
        .render = host.render,
        .pop = crazypod_route_actions_pop,
    };

    (void)context;
    return crazypod_settings_feature_handle_input(
        state, event, &input);
}

static bool handle_books_pressed(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_feature_input_context input = {
        .now = host.now(),
        .render = host.render,
        .activate = activate,
        .pop = crazypod_route_actions_pop,
    };

    (void)context;
    return crazypod_books_feature_handle_input(
        state, event, &input);
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
    const struct crazypod_feature_input_context input = {
        .now = host.now(),
        .ticks_per_second = HZ,
        .today_date = today_date(),
        .activate = activate,
        .move = move,
        .render = host.render,
        .push = crazypod_route_actions_push,
        .pop = crazypod_route_actions_pop,
    };

    (void)context;
    return crazypod_organizer_feature_handle_input(
        state, event, &input);
}

static bool handle_notes_pressed(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_feature_input_context input = {
        .now = host.now(),
        .activate = activate,
        .move = move,
        .render = host.render,
        .push = crazypod_route_actions_push,
        .pop = crazypod_route_actions_pop,
    };

    (void)context;
    return crazypod_notes_feature_handle_input(
        state, event, &input);
}

static bool handle_music_pressed(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    const struct crazypod_feature_input_context input = {
        .now = host.now(),
        .activate = activate,
        .move = move,
        .render = host.render,
        .push = crazypod_route_actions_push,
        .pop = crazypod_route_actions_pop,
    };

    (void)context;
    return crazypod_music_feature_handle_input(
        state, event, &input);
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
