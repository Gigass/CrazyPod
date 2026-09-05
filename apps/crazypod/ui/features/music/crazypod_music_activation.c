#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <string.h>

#include "../../../crazypod_music.h"
#include "../../presentation/crazypod_ui_text.h"
#include "crazypod_music_activation.h"

#define SEARCH_QUERY_SIZE 33
#define EDITOR_CHARACTER_COUNT 36

static const char *const editor_characters[EDITOR_CHARACTER_COUNT] = {
    CP_TR("A"), CP_TR("B"), CP_TR("C"), CP_TR("D"), CP_TR("E"), CP_TR("F"), CP_TR("G"), CP_TR("H"), CP_TR("I"), CP_TR("J"),
    CP_TR("K"), CP_TR("L"), CP_TR("M"), CP_TR("N"), CP_TR("O"), CP_TR("P"), CP_TR("Q"), CP_TR("R"), CP_TR("S"), CP_TR("T"),
    CP_TR("U"), CP_TR("V"), CP_TR("W"), "X", CP_TR("Y"), CP_TR("Z"),
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

static char search_query[SEARCH_QUERY_SIZE];

static struct crazypod_music_activation_result result(
    enum crazypod_music_activation_kind kind,
    enum crazypod_route route, int group)
{
    const struct crazypod_music_activation_result value = {
        .kind = kind,
        .route = route,
        .group = group,
    };

    return value;
}

const char *crazypod_music_search_query(void)
{
    return search_query;
}

void crazypod_music_search_backspace(void)
{
    crazypod_ui_text_backspace(search_query);
}

static bool is_podcast_path(const char *path)
{
    return path != NULL &&
        (strstr(path, "/Podcasts/") != NULL ||
         strstr(path, "/podcasts/") != NULL);
}

int crazypod_music_podcast_track_index(int position)
{
    int visible = 0;
    int index;

    for(index = 0; index < crazypod_music_track_count(); ++index) {
        struct crazypod_track track;

        if(crazypod_music_copy_track(index, &track) &&
           is_podcast_path(track.path) &&
           visible++ == position)
            return index;
    }
    return -1;
}

static struct crazypod_music_activation_result activate_menu(
    int selected)
{
    static const enum crazypod_route routes[] = {
        MUSIC_ROUTE_NOW_PLAYING,
        MUSIC_ROUTE_ALBUM_FLOW,
        MUSIC_ROUTE_ALL,
        MUSIC_ROUTE_PLAYLISTS,
        MUSIC_ROUTE_ARTISTS,
        MUSIC_ROUTE_ALBUMS,
        MUSIC_ROUTE_SONGS,
        MUSIC_ROUTE_SEARCH,
    };

    if(selected < 0 ||
       selected >= (int)(sizeof(routes) / sizeof(routes[0])))
        return result(
            CRAZYPOD_MUSIC_ACTIVATION_NONE, MUSIC_ROUTE_MENU, -1);
    if(selected == 0)
        return result(
            CRAZYPOD_MUSIC_ACTIVATION_REQUEST_NOW_PLAYING,
            routes[selected], -1);
    if(selected == 1)
        return result(
            CRAZYPOD_MUSIC_ACTIVATION_OPEN_ALBUM_FLOW,
            routes[selected], -1);
    if(selected == 7)
        search_query[0] = '\0';
    return result(
        CRAZYPOD_MUSIC_ACTIVATION_PUSH, routes[selected], -1);
}

static struct crazypod_music_activation_result activate_search(
    int selected)
{
    if(selected < EDITOR_CHARACTER_COUNT)
        crazypod_ui_text_append(
            search_query, sizeof(search_query),
            editor_characters[selected]);
    else if(selected == EDITOR_CHARACTER_COUNT)
        crazypod_ui_text_append(
            search_query, sizeof(search_query), " ");
    else if(selected == EDITOR_CHARACTER_COUNT + 1)
        crazypod_ui_text_backspace(search_query);
    else if(search_query[0] != '\0' &&
            crazypod_music_search_count(search_query) > 0)
        return result(
            CRAZYPOD_MUSIC_ACTIVATION_PUSH,
            MUSIC_ROUTE_SEARCH_RESULTS, -1);
    return result(
        CRAZYPOD_MUSIC_ACTIVATION_RENDER, MUSIC_ROUTE_SEARCH, -1);
}

struct crazypod_music_activation_result
crazypod_music_activation_execute(const struct route_state *state)
{
    if(state->route == MUSIC_ROUTE_MENU)
        return activate_menu(state->selected);
    if(state->route == MUSIC_ROUTE_SEARCH)
        return activate_search(state->selected);
    if(state->route == MUSIC_ROUTE_PLAYLISTS)
        return result(
            CRAZYPOD_MUSIC_ACTIVATION_PUSH,
            MUSIC_ROUTE_PLAYLIST_SONGS, state->selected);
    if(state->route == MUSIC_ROUTE_ARTISTS)
        return result(
            CRAZYPOD_MUSIC_ACTIVATION_PUSH,
            MUSIC_ROUTE_ARTIST_SONGS, state->selected);
    if(state->route == MUSIC_ROUTE_ALBUMS ||
       state->route == MUSIC_ROUTE_ALBUM_FLOW)
        return result(
            CRAZYPOD_MUSIC_ACTIVATION_PUSH,
            MUSIC_ROUTE_ALBUM_SONGS, state->selected);
    if(state->route == MUSIC_ROUTE_NOW_PLAYING)
        return result(
            CRAZYPOD_MUSIC_ACTIVATION_SHOW_NOW_ACTIONS,
            state->route, -1);
    if(state->route == PODCASTS_ROUTE_MENU) {
        int track_index =
            crazypod_music_podcast_track_index(state->selected);

        if(track_index >= 0) {
            return result(
                CRAZYPOD_MUSIC_ACTIVATION_PLAY_PODCAST,
                state->route, track_index);
        }
        return result(
            CRAZYPOD_MUSIC_ACTIVATION_NONE, state->route, -1);
    }
    return result(
        CRAZYPOD_MUSIC_ACTIVATION_UNHANDLED, state->route, -1);
}

#endif
