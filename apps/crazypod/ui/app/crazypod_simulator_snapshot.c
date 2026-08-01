#include "config.h"

#if defined(IPOD_6G) && defined(SIMULATOR)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"
#include "lvgl.h"

#include "../../crazypod_books.h"
#include "../../crazypod_frameclock.h"
#include "../../crazypod_l10n.h"
#include "../../crazypod_lcd.h"
#include "../../crazypod_miniapps.h"
#include "../../crazypod_music.h"
#include "../../crazypod_notes.h"
#include "../../crazypod_organizer.h"
#include "../../crazypod_videos.h"
#include "../../crazypod_workouts.h"
#include "../features/books/crazypod_books_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../navigation/crazypod_route_query.h"
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
    return true;
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
    else
        return open_miniapp_snapshot(host, screen, -1);
    return true;
}

#endif
