#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "../../crazypod_apps.h"
#include "../features/books/crazypod_books_feature.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../shell/crazypod_app_catalog.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../features/settings/crazypod_settings_feature.h"
#include "../navigation/crazypod_route_registry.h"
#include "crazypod_route_query.h"

int crazypod_route_query_item_count(
    const struct route_state *state, const char *music_search_query)
{
    const struct crazypod_feature *feature =
        crazypod_route_registry_feature(state->route);

    if(feature == NULL)
        return crazypod_route_registry_is_shell(state->route)
            ? crazypod_apps_hidden_count() : 0;

    switch(feature->id) {
    case CRAZYPOD_FEATURE_MUSIC:
        return crazypod_music_feature_item_count(
            state, music_search_query);
    case CRAZYPOD_FEATURE_NOW_PLAYING:
        return crazypod_now_playing_feature_item_count(state);
    case CRAZYPOD_FEATURE_BOOKS:
        return crazypod_books_feature_item_count(state);
    case CRAZYPOD_FEATURE_NOTES:
        return crazypod_notes_feature_item_count(state);
    case CRAZYPOD_FEATURE_PHOTOS:
        return crazypod_photos_feature_item_count(state);
    case CRAZYPOD_FEATURE_ORGANIZER:
        return crazypod_organizer_feature_item_count(state);
    case CRAZYPOD_FEATURE_CUSTOMIZE:
        return crazypod_customize_feature_item_count(state);
    case CRAZYPOD_FEATURE_SETTINGS:
        return crazypod_settings_feature_item_count(state);
    case CRAZYPOD_FEATURE_MINIAPPS:
        return crazypod_miniapps_feature_item_count(state);
    case CRAZYPOD_FEATURE_COUNT:
        return 0;
    }
    return 0;
}

const char *crazypod_route_query_item_title(
    const struct route_state *state, int index,
    const char *music_search_query,
    bool stopwatch_running, bool workout_running)
{
    const struct crazypod_feature *feature =
        crazypod_route_registry_feature(state->route);
    const char *title = "";
    bool handled = false;

    if(feature == NULL) {
        if(crazypod_route_registry_is_shell(state->route)) {
            const struct crazypod_app_descriptor *app =
                crazypod_app_catalog_find(
                    crazypod_apps_hidden_id(index));

            return app != NULL ? app->name : "";
        }
        return "";
    }

    switch(feature->id) {
    case CRAZYPOD_FEATURE_MUSIC:
        handled = crazypod_music_feature_item_title(
            state, index, music_search_query, &title);
        break;
    case CRAZYPOD_FEATURE_NOW_PLAYING:
        handled = crazypod_now_playing_feature_item_title(
            state, index, &title);
        break;
    case CRAZYPOD_FEATURE_BOOKS:
        handled = crazypod_books_feature_item_title(
            state, index, &title);
        break;
    case CRAZYPOD_FEATURE_NOTES:
        handled = crazypod_notes_feature_item_title(
            state, index, &title);
        break;
    case CRAZYPOD_FEATURE_PHOTOS:
        handled = crazypod_photos_feature_item_title(
            state, index, &title);
        break;
    case CRAZYPOD_FEATURE_ORGANIZER:
        handled = crazypod_organizer_feature_item_title(
            state, index, stopwatch_running,
            workout_running, &title);
        break;
    case CRAZYPOD_FEATURE_CUSTOMIZE:
        handled = crazypod_customize_feature_item_title(
            state, index, &title);
        break;
    case CRAZYPOD_FEATURE_SETTINGS:
        handled = crazypod_settings_feature_item_title(
            state, index, &title);
        break;
    case CRAZYPOD_FEATURE_MINIAPPS:
        handled = crazypod_miniapps_feature_item_title(
            state, index, &title);
        break;
    case CRAZYPOD_FEATURE_COUNT:
        break;
    }
    return handled ? title : "";
}

const char *crazypod_route_query_title(
    const struct route_state *state)
{
    const struct crazypod_feature *feature =
        crazypod_route_registry_feature(state->route);

    if(feature == NULL)
        return crazypod_route_registry_is_shell(state->route)
            ? CP_TR("MORE FEATURES") : "";

    switch(feature->id) {
    case CRAZYPOD_FEATURE_MUSIC:
        return crazypod_music_feature_title(state);
    case CRAZYPOD_FEATURE_NOW_PLAYING:
        return crazypod_now_playing_feature_title(state);
    case CRAZYPOD_FEATURE_BOOKS:
        return crazypod_books_feature_title(state);
    case CRAZYPOD_FEATURE_NOTES:
        return crazypod_notes_feature_title(state);
    case CRAZYPOD_FEATURE_PHOTOS:
        return crazypod_photos_feature_title(state);
    case CRAZYPOD_FEATURE_ORGANIZER:
        return crazypod_organizer_feature_title(state);
    case CRAZYPOD_FEATURE_CUSTOMIZE:
        return crazypod_customize_feature_title(state);
    case CRAZYPOD_FEATURE_SETTINGS:
        return crazypod_settings_feature_title(state);
    case CRAZYPOD_FEATURE_MINIAPPS:
        return crazypod_miniapps_feature_title(state);
    case CRAZYPOD_FEATURE_COUNT:
        return "";
    }
    return "";
}

bool crazypod_route_query_item_is_current(
    const struct route_state *state, int index)
{
    const struct crazypod_feature *feature =
        crazypod_route_registry_feature(state->route);

    if(feature == NULL)
        return false;
    if(feature->id == CRAZYPOD_FEATURE_SETTINGS)
        return crazypod_settings_feature_item_is_current(
            state, index);
    if(feature->id == CRAZYPOD_FEATURE_CUSTOMIZE)
        return crazypod_customize_feature_item_is_current(
            state, index);
    return false;
}

#endif
