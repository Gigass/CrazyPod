#include "config.h"

#ifdef IPOD_6G

#include "kernel.h"
#include "timefuncs.h"

#include "../../crazypod_l10n.h"
#include "../../crazypod_music.h"
#include "../../crazypod_organizer.h"
#include "../../crazypod_photos.h"
#include "../../crazypod_state.h"
#include "../features/books/crazypod_books_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../navigation/crazypod_route_registry.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../shell/crazypod_home_actions.h"
#include "../shell/crazypod_shell.h"
#include "../shell/crazypod_notification.h"
#include "crazypod_app_launcher.h"
#include "crazypod_choice_coordinator.h"
#include "crazypod_playback.h"
#include "crazypod_scene_transition.h"

static struct crazypod_app_launcher_host host;
static bool shuffle_pending;

static int today_date(void)
{
    struct tm *now = get_time();

    return (now->tm_year + 1900) * 10000 +
        (now->tm_mon + 1) * 100 + now->tm_mday;
}

static void open_music(void)
{
    (void)crazypod_music_validate_catalog_async();
    crazypod_shell_open_product();
    host.boost(true);
    crazypod_ui_routes_reset(MUSIC_ROUTE_MENU, -1, 0);
    if(!crazypod_music_library_loaded()) {
        host.begin_music_scan();
        return;
    }
    host.render(true);
}

void crazypod_app_launcher_open_now_playing(void)
{
    /* This entry point is also used by the dock shortcut while another
     * product route is visible. Tear down modal/runtime owners first; merely
     * replacing the route would leave a Mini App alive but no longer serviced.
     */
    if(crazypod_home_actions_visible())
        crazypod_home_actions_dismiss(false);
    if(crazypod_now_playing_overlay_visible())
        crazypod_now_playing_overlay_dismiss(false);
    if(crazypod_choice_coordinator_visible())
        crazypod_choice_coordinator_dismiss(false);
    if(crazypod_miniapps_feature_is_open()) {
        crazypod_miniapps_feature_reset_input();
        crazypod_miniapps_feature_close();
    }
    crazypod_notes_feature_save_draft();
    crazypod_organizer_feature_pause_workout(current_tick);
    if(crazypod_shell_product_active() &&
       crazypod_music_library_loading())
        crazypod_music_library_leave(current_tick);
    shuffle_pending = false;
    (void)crazypod_music_validate_catalog_async();
    crazypod_shell_open_product();
    host.boost(true);
    /* Home is the parent of this shortcut.  Leave Now Playing as the route
     * root so Menu closes the product layer instead of revealing Music. */
    crazypod_ui_routes_clear();
    host.request_now_playing();
}

static void open_shuffle_screen(void)
{
    crazypod_shell_open_product();
    host.boost(true);
    /* Shuffle is launched from Home, so Now Playing must be the root route.
     * Otherwise Menu reveals the transient Music loading route. */
    crazypod_ui_routes_clear();
    host.request_now_playing();
}

static void start_shuffle(void)
{
    if(crazypod_playback_commands_ready()) {
        if(!crazypod_playback_shuffle_all_async())
            crazypod_notification_show(
                CRAZYPOD_NOTIFICATION_ERROR,
                CP_TR("No track available"));
        return;
    }
    if(!crazypod_music_shuffle_all((unsigned int)current_tick)) {
        crazypod_notification_show(
            CRAZYPOD_NOTIFICATION_ERROR,
            CP_TR("No track available"));
        return;
    }
    crazypod_state_forget_resume();
    crazypod_state_mark_dirty();
}

static void request_shuffle(void)
{
    shuffle_pending = true;
    open_shuffle_screen();
    (void)crazypod_music_validate_catalog_async();
    if(!crazypod_music_library_loaded())
        host.begin_music_scan();
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
    if(id != CRAZYPOD_APP_SHUFFLE)
        shuffle_pending = false;
    switch(id) {
    case CRAZYPOD_APP_MUSIC:
        shuffle_pending = false;
        open_music();
        break;
    case CRAZYPOD_APP_SHUFFLE:
        request_shuffle();
        break;
    case CRAZYPOD_APP_LOCK:
        host.show_lock(true);
        break;
    case CRAZYPOD_APP_PHOTOS:
        host.boost(true);
        crazypod_photos_set_route_suspended(false);
        crazypod_photos_ensure_catalog();
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
        (void)crazypod_music_validate_catalog_async();
        open_route(PODCASTS_ROUTE_MENU);
        if(!crazypod_music_library_loaded())
            host.begin_music_scan();
        break;
    case CRAZYPOD_APP_MINI_APPS:
        host.boost(true);
        (void)crazypod_miniapps_feature_prepare();
        open_route(UTILITIES_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_GAMEBOY:
        host.boost(true);
        crazypod_miniapps_feature_open_gameboy();
        open_route(GAMEBOY_ROUTE_LIBRARY);
        break;
    case CRAZYPOD_APP_CLOCK:
        open_route(CLOCK_ROUTE_MENU);
        break;
    case CRAZYPOD_APP_CONTACTS:
        crazypod_organizer_ensure_loaded();
        open_route(CONTACTS_ROUTE_LIST);
        break;
    case CRAZYPOD_APP_CALENDAR:
        crazypod_organizer_ensure_loaded();
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

void crazypod_app_launcher_process_pending(void)
{
    enum crazypod_music_catalog_validation validation;

    if(!shuffle_pending || crazypod_scene_transition_active() ||
       crazypod_music_is_scanning() ||
       !crazypod_music_library_loaded())
        return;
    validation = crazypod_music_catalog_validation();
    if(validation != CRAZYPOD_MUSIC_VALIDATION_CURRENT &&
       validation != CRAZYPOD_MUSIC_VALIDATION_FAILED)
        return;

    shuffle_pending = false;
    if(crazypod_music_track_count() > 0)
        start_shuffle();
}

void crazypod_app_launcher_cancel_pending(void)
{
    shuffle_pending = false;
}

#ifdef SIMULATOR
void crazypod_app_launcher_simulate_shuffle_ready(void)
{
    shuffle_pending = false;
    open_shuffle_screen();
}
#endif

#endif
