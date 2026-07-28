#include "config.h"

#if defined(IPOD_6G) && defined(SIMULATOR)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"
#include "lvgl.h"

#include "../../crazypod_books.h"
#include "../../crazypod_frameclock.h"
#include "../../crazypod_lcd.h"
#include "../../crazypod_miniapps.h"
#include "../../crazypod_music.h"
#include "../../crazypod_notes.h"
#include "../../crazypod_organizer.h"
#include "../../crazypod_videos.h"
#include "../../crazypod_workouts.h"
#include "../features/books/crazypod_book_session.h"
#include "../features/books/crazypod_books_workflow.h"
#include "../features/music/crazypod_music_activation.h"
#include "../features/notes/crazypod_notes_controller.h"
#include "../features/organizer/crazypod_activity_controller.h"
#include "../features/organizer/crazypod_calendar_controller.h"
#include "../navigation/crazypod_route_query.h"
#include "crazypod_simulator_snapshot.h"

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
    return crazypod_notes_controller_draft_available() ? 2 : 1;
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

bool crazypod_simulator_snapshot_prepare(
    const struct crazypod_simulator_snapshot_host *host)
{
    const char *screen = getenv("CRAZYPOD_SIM_SCREEN");
    int preview_index;

    if(host == NULL || getenv("CRAZYPOD_SIM_DUMP") == NULL)
        return false;
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
        if(preview_index > 2)
            preview_index = 2;
        current_route()->selected = preview_index;
        host->render(false);
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
            host, crazypod_notes_controller_draft_available() ? 1 : 0);
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
    else if(strcmp(screen, "settings-reduce-motion") == 0) {
        host->open_root_route(SETTINGS_ROUTE_DISPLAY);
        select_bounded(host, item_count() - 1);
    }
    else if(strcmp(screen, "notes") == 0)
        host->open_app(CRAZYPOD_APP_NOTES);
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
            crazypod_books_workflow_begin_reader(0, 0);
    }
    else if(strcmp(screen, "book-reader-next") == 0) {
        host->open_app(CRAZYPOD_APP_BOOKS);
        if(crazypod_books_count() > 0) {
            crazypod_books_workflow_begin_reader(0, 0);
            crazypod_book_session_turn(1);
        }
    }
    else if(strcmp(screen, "clock") == 0)
        host->open_root_route(CLOCK_ROUTE_VIEW);
    else if(strcmp(screen, "stopwatch") == 0)
        host->open_root_route(STOPWATCH_ROUTE_VIEW);
    else if(strcmp(screen, "workouts") == 0)
        host->open_root_route(WORKOUT_ROUTE_MENU);
    else if(strcmp(screen, "workout-ready") == 0) {
        crazypod_activity_simulator_workout(0, 0, current_tick, false);
        host->open_root_route(WORKOUT_ROUTE_READY);
    }
    else if(strcmp(screen, "workout-active") == 0) {
        crazypod_activity_simulator_workout(
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
            crazypod_calendar_controller_focus_date());
        host->render(true);
    }
    else if(strcmp(screen, "contacts") == 0)
        host->open_app(CRAZYPOD_APP_CONTACTS);
    else if(strcmp(screen, "contact-detail") == 0) {
        host->open_app(CRAZYPOD_APP_CONTACTS);
        if(crazypod_contacts_count() > 0)
            host->push_route(CONTACTS_ROUTE_DETAIL, 0);
    }
    else if(strcmp(screen, "calculator") == 0 ||
            strcmp(screen, "pomodoro") == 0) {
        int app_index;

        host->open_app(CRAZYPOD_APP_MINI_APPS);
        app_index = crazypod_miniapps_find(screen);
        if(app_index < 0)
            return false;
        current_route()->selected = app_index;
        host->activate_selected();
        if(current_route()->route != MINIAPP_ROUTE_VIEW)
            return false;
    }
    else
        return false;
    return true;
}

#endif
