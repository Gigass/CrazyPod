#include "config.h"

#ifdef IPOD_6G

#include "timefuncs.h"

#include "../../crazypod_music.h"
#include "../../crazypod_organizer.h"
#include "../../crazypod_playlist.h"
#include "../../crazypod_state.h"
#include "../features/books/crazypod_books_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../navigation/crazypod_route_registry.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../shell/crazypod_shell.h"
#include "crazypod_app_launcher.h"

static struct crazypod_app_launcher_host host;

static int today_date(void)
{
    struct tm *now = get_time();

    return (now->tm_year + 1900) * 10000 +
        (now->tm_mon + 1) * 100 + now->tm_mday;
}

static void open_music(void)
{
    crazypod_shell_open_product();
    host.boost(true);
    crazypod_ui_routes_reset(MUSIC_ROUTE_MENU, -1, 0);
    if(!crazypod_music_library_loaded()) {
        host.begin_music_scan();
        return;
    }
    host.render(true);
}

static void open_route(enum crazypod_route route)
{
    crazypod_shell_open_product();
    host.boost(true);
    crazypod_ui_routes_reset(route, -1, 0);
    host.render(true);
}

void crazypod_app_launcher_configure(
    const struct crazypod_app_launcher_host *new_host)
{
    if(new_host != NULL)
        host = *new_host;
}

void crazypod_app_launcher_open_root(enum crazypod_route route)
{
    if(crazypod_route_registry_get(route) != NULL)
        open_route(route);
}

void crazypod_app_launcher_open_books(void)
{
    crazypod_books_feature_ensure_metadata();
    open_route(BOOKS_ROUTE_MENU);
}

void crazypod_app_launcher_open(enum crazypod_app_id id)
{
    switch(id) {
    case CRAZYPOD_APP_MUSIC:
        open_music();
        break;
    case CRAZYPOD_APP_SHUFFLE:
        open_music();
        if(crazypod_music_track_count() > 0) {
            crazypod_queue_set_shuffle(true);
            crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0);
            crazypod_state_forget_resume();
            crazypod_state_mark_dirty();
            host.request_now_playing();
        }
        break;
    case CRAZYPOD_APP_LOCK:
        host.show_lock(true);
        break;
    case CRAZYPOD_APP_PHOTOS:
        crazypod_photos_feature_reset_controller();
        crazypod_photos_feature_reset_view();
        open_route(PHOTOS_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_CUSTOMIZE:
        open_route(DIY_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_WORKOUTS:
        open_route(WORKOUT_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_EXTRAS:
        open_route(EXTRAS_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_NOTES:
        crazypod_notes_feature_refresh_draft();
        open_route(NOTES_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_BOOKS:
        crazypod_app_launcher_open_books();
        break;
    case CRAZYPOD_APP_PODCASTS:
        open_route(PODCASTS_ROUTE_MENU);
        if(!crazypod_music_library_loaded())
            host.begin_music_scan();
        break;
    case CRAZYPOD_APP_MINI_APPS:
        open_route(UTILITIES_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_CLOCK:
        open_route(CLOCK_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_CONTACTS:
        crazypod_organizer_scan();
        open_route(CONTACTS_ROUTE_LIST);
        break;
    case CRAZYPOD_APP_CALENDAR:
        crazypod_organizer_scan();
        crazypod_organizer_feature_set_focus_date(
            today_date());
        open_route(CALENDAR_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_STOPWATCH:
        open_route(STOPWATCH_ROUTE_VIEW);
        break;
    case CRAZYPOD_APP_SETTINGS:
        open_route(SETTINGS_ROUTE_MENU);
        break;
    default:
        break;
    }
}

#endif
