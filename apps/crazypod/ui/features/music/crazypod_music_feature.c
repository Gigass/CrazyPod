#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "../../../crazypod_collation.h"
#include "../../../crazypod_music.h"
#include "../../../crazypod_playlist.h"
#include "crazypod_music_activation.h"
#include "crazypod_music_input.h"
#include "crazypod_music_item_preview.h"
#include "crazypod_music_root_preview.h"
#include "crazypod_album_flow_screen.h"
#include "crazypod_search_screen.h"
#include "crazypod_music_feature.h"

#define EDITOR_ACTION_COUNT 3
#define EDITOR_CHARACTER_COUNT 36

static const char *const editor_characters[EDITOR_CHARACTER_COUNT] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

static bool is_podcast_path(const char *path)
{
    return path != NULL &&
        (strstr(path, "/Podcasts/") != NULL ||
         strstr(path, "/podcasts/") != NULL);
}

static int podcast_count(void)
{
    int count = 0;
    int i;

    for(i = 0; i < crazypod_music_track_count(); ++i) {
        const struct crazypod_track *track = crazypod_music_track(i);

        if(track != NULL && is_podcast_path(track->path))
            ++count;
    }
    return count;
}

static int podcast_track_index(int position)
{
    int visible = 0;
    int i;

    for(i = 0; i < crazypod_music_track_count(); ++i) {
        const struct crazypod_track *track = crazypod_music_track(i);

        if(track != NULL && is_podcast_path(track->path) &&
           visible++ == position)
            return i;
    }
    return -1;
}

static const char *editor_title(int index)
{
    if(index >= 0 && index < EDITOR_CHARACTER_COUNT)
        return editor_characters[index];
    if(index == EDITOR_CHARACTER_COUNT)
        return "Space";
    if(index == EDITOR_CHARACTER_COUNT + 1)
        return "Delete";
    if(index == EDITOR_CHARACTER_COUNT + 2)
        return "View Results";
    return "";
}

int crazypod_music_feature_item_count(
    const struct route_state *state, const char *search_query)
{
    switch(state->route) {
    case MUSIC_ROUTE_MENU:
        return 8;
    case MUSIC_ROUTE_ALBUM_FLOW:
    case MUSIC_ROUTE_ALBUMS:
        return crazypod_music_album_count();
    case MUSIC_ROUTE_ALL:
    case MUSIC_ROUTE_SONGS:
        return crazypod_music_track_count();
    case MUSIC_ROUTE_SEARCH:
        return EDITOR_CHARACTER_COUNT + EDITOR_ACTION_COUNT;
    case MUSIC_ROUTE_SEARCH_RESULTS:
        return crazypod_music_search_count(search_query);
    case MUSIC_ROUTE_PLAYLISTS:
        return crazypod_music_playlist_count();
    case MUSIC_ROUTE_PLAYLIST_SONGS: {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(state->group);

        return playlist != NULL ? playlist->track_count : 0;
    }
    case MUSIC_ROUTE_ARTISTS:
        return crazypod_music_artist_count();
    case MUSIC_ROUTE_ARTIST_SONGS:
        return crazypod_music_artist_track_count(state->group);
    case MUSIC_ROUTE_ALBUM_SONGS:
        return crazypod_music_album_track_count(state->group);
    case PODCASTS_ROUTE_MENU:
        return podcast_count();
    default:
        return 0;
    }
}

const char *crazypod_music_feature_title(
    const struct route_state *state)
{
    switch(state->route) {
    case MUSIC_ROUTE_MENU:
        return "MUSIC";
    case MUSIC_ROUTE_ALL:
        return "ALL MUSIC";
    case MUSIC_ROUTE_PLAYLISTS:
        return "PLAYLISTS";
    case MUSIC_ROUTE_PLAYLIST_SONGS: {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(state->group);

        return playlist != NULL ? playlist->name : "PLAYLIST";
    }
    case MUSIC_ROUTE_ARTISTS:
        return "ARTISTS";
    case MUSIC_ROUTE_ARTIST_SONGS:
        return crazypod_music_artist(state->group);
    case MUSIC_ROUTE_ALBUMS:
    case MUSIC_ROUTE_ALBUM_FLOW:
        return "ALBUMS";
    case MUSIC_ROUTE_ALBUM_SONGS: {
        const struct crazypod_album *album =
            crazypod_music_album(state->group);

        return album != NULL ? album->title : "ALBUM";
    }
    case MUSIC_ROUTE_SONGS:
        return "SONGS";
    case MUSIC_ROUTE_SEARCH:
        return "SEARCH";
    case MUSIC_ROUTE_SEARCH_RESULTS:
        return "RESULTS";
    case PODCASTS_ROUTE_MENU:
        return "PODCASTS";
    default:
        return "";
    }
}

bool crazypod_music_feature_item_title(
    const struct route_state *state, int index,
    const char *search_query, const char **title)
{
    const struct crazypod_track *track = NULL;

    switch(state->route) {
    case MUSIC_ROUTE_MENU: {
        static const char *const titles[] = {
            "Now Playing", "Album Flow", "All Music", "Playlists",
            "Artists", "Albums", "Songs", "Search"
        };

        *title = index >= 0 && index < 8 ? titles[index] : "";
        return true;
    }
    case MUSIC_ROUTE_SEARCH:
        *title = editor_title(index);
        return true;
    case MUSIC_ROUTE_PLAYLISTS: {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(index);

        *title = playlist != NULL ? playlist->name : "";
        return true;
    }
    case MUSIC_ROUTE_ARTISTS:
        *title = crazypod_music_artist(index);
        return true;
    case MUSIC_ROUTE_ALBUMS:
    case MUSIC_ROUTE_ALBUM_FLOW: {
        static char album_label[148];
        const struct crazypod_album *album =
            crazypod_music_album(index);
        bool duplicate_title = false;
        int i;

        if(album == NULL) {
            *title = "";
            return true;
        }
        for(i = 0; i < crazypod_music_album_count(); ++i) {
            const struct crazypod_album *other =
                crazypod_music_album(i);

            if(i != index && other != NULL &&
               strcmp(other->title, album->title) == 0) {
                duplicate_title = true;
                break;
            }
        }
        snprintf(album_label, sizeof(album_label),
                 duplicate_title ? "%s · %s" : "%s",
                 duplicate_title ? album->artist : album->title,
                 album->title);
        *title = album_label;
        return true;
    }
    case MUSIC_ROUTE_ALL:
    case MUSIC_ROUTE_SONGS:
        track = crazypod_music_track(index);
        break;
    case MUSIC_ROUTE_SEARCH_RESULTS:
        track = crazypod_music_search_track(search_query, index);
        break;
    case MUSIC_ROUTE_PLAYLIST_SONGS:
        track = crazypod_music_playlist_track(state->group, index);
        break;
    case MUSIC_ROUTE_ARTIST_SONGS:
        track = crazypod_music_artist_track(state->group, index);
        break;
    case MUSIC_ROUTE_ALBUM_SONGS:
        track = crazypod_music_album_track(state->group, index);
        break;
    case PODCASTS_ROUTE_MENU:
        track = crazypod_music_track(podcast_track_index(index));
        break;
    default:
        return false;
    }
    *title = track != NULL ? track->title : "";
    return true;
}

static bool alpha_jump_route(enum crazypod_route route)
{
    switch(route) {
    case MUSIC_ROUTE_ALL:
    case MUSIC_ROUTE_SONGS:
    case MUSIC_ROUTE_ARTISTS:
    case MUSIC_ROUTE_ARTIST_SONGS:
    case MUSIC_ROUTE_ALBUMS:
    case MUSIC_ROUTE_SEARCH_RESULTS:
    case PODCASTS_ROUTE_MENU:
        return true;
    default:
        return false;
    }
}

bool crazypod_music_feature_alpha_jump_available(
    const struct route_state *state)
{
    return state != NULL &&
        alpha_jump_route(state->route) &&
        crazypod_music_feature_item_count(
            state, crazypod_music_search_query()) >= 24;
}

static const char *alpha_jump_title_at(
    int index, void *context)
{
    const struct route_state *state = context;
    const char *title = "";

    if(crazypod_music_feature_item_title(
           state, index, crazypod_music_search_query(),
           &title))
        return title != NULL ? title : "";
    return "";
}

bool crazypod_music_feature_alpha_jump_target(
    const struct route_state *state, int direction,
    int *target, char *key)
{
    int count;

    if(!crazypod_music_feature_alpha_jump_available(state))
        return false;
    count = crazypod_music_feature_item_count(
        state, crazypod_music_search_query());
    return crazypod_collation_section_target(
        count, state->selected, direction,
        alpha_jump_title_at, (void *)state,
        target, key);
}

bool crazypod_music_feature_activate(
    const struct route_state *state,
    const struct crazypod_music_activation_host *host)
{
    const struct crazypod_music_activation_result action =
        crazypod_music_activation_execute(state);

    if(action.kind == CRAZYPOD_MUSIC_ACTIVATION_UNHANDLED)
        return false;
    if(action.kind == CRAZYPOD_MUSIC_ACTIVATION_RENDER)
        host->render(false);
    else if(action.kind == CRAZYPOD_MUSIC_ACTIVATION_PUSH)
        host->push(action.route, action.group);
    else if(action.kind ==
            CRAZYPOD_MUSIC_ACTIVATION_OPEN_ALBUM_FLOW)
        host->push_selected(
            MUSIC_ROUTE_ALBUM_FLOW, -1,
            host->initial_album_index());
    else if(action.kind ==
            CRAZYPOD_MUSIC_ACTIVATION_REQUEST_NOW_PLAYING)
        host->request_now_playing();
    else if(action.kind ==
            CRAZYPOD_MUSIC_ACTIVATION_SHOW_NOW_ACTIONS)
        host->show_now_actions();
    return true;
}

const struct crazypod_track *crazypod_music_feature_route_track(
    const struct route_state *state, int index)
{
    switch(state->route) {
    case MUSIC_ROUTE_ALL:
    case MUSIC_ROUTE_SONGS:
        return crazypod_music_track(index);
    case MUSIC_ROUTE_SEARCH_RESULTS:
        return crazypod_music_search_track(
            crazypod_music_search_query(), index);
    case MUSIC_ROUTE_PLAYLIST_SONGS:
        return crazypod_music_playlist_track(
            state->group, index);
    case MUSIC_ROUTE_ARTIST_SONGS:
        return crazypod_music_artist_track(
            state->group, index);
    case MUSIC_ROUTE_ALBUM_SONGS:
        return crazypod_music_album_track(
            state->group, index);
    case MUSIC_ROUTE_QUEUE: {
        const char *path = crazypod_queue_path(index);

        return crazypod_music_track(
            crazypod_music_find_track(path));
    }
    case PODCASTS_ROUTE_MENU:
        return crazypod_music_track(
            crazypod_music_podcast_track_index(index));
    default:
        return NULL;
    }
}

void crazypod_music_feature_render_album_flow(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font)
{
    crazypod_album_flow_screen_render(
        parent, state, metadata_font);
}

void crazypod_music_feature_reset_view(void)
{
    crazypod_album_flow_screen_reset();
}

int crazypod_music_feature_sync_album_flow(void)
{
    return crazypod_album_flow_screen_sync();
}

bool crazypod_music_feature_render_special(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font, int item_count,
    const char *(*item_title)(
        const struct route_state *state, int index),
    uint32_t primary_color, uint32_t secondary_color,
    uint32_t panel_color, bool gradient_highlight,
    crazypod_music_panel_factory make_panel)
{
    if(state->route == MUSIC_ROUTE_ALBUM_FLOW) {
        crazypod_album_flow_screen_render(
            parent, state, metadata_font);
        return true;
    }
    if(state->route != MUSIC_ROUTE_SEARCH)
        return false;
    {
        const struct crazypod_search_screen_context context = {
            .parent = parent,
            .query = crazypod_music_search_query(),
            .item_count = item_count,
            .primary_color = primary_color,
            .secondary_color = secondary_color,
            .panel_color = panel_color,
            .gradient_highlight = gradient_highlight,
            .metadata_font = metadata_font,
            .item_title = item_title,
            .make_panel = make_panel,
        };

        crazypod_search_screen_render(state, &context);
    }
    return true;
}

static struct crazypod_feature_input_context music_input_context;

static void music_input_render(void)
{
    music_input_context.render(false);
}

static void music_show_search_results(void)
{
    if(crazypod_music_search_count(
           crazypod_music_search_query()) > 0)
        music_input_context.push(
            MUSIC_ROUTE_SEARCH_RESULTS, -1);
}

bool crazypod_music_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context)
{
    const struct crazypod_music_input_actions actions = {
        .move_selection = context->move,
        .activate = context->activate,
        .render = music_input_render,
        .leave = context->pop,
        .show_search_results = music_show_search_results,
    };

    if(state->route != MUSIC_ROUTE_SEARCH)
        return false;
    music_input_context = *context;
    return crazypod_music_search_input_handle(
        event, &actions);
}

void crazypod_music_feature_render_root_preview(
    lv_obj_t *parent, int selected, bool defer_media)
{
    crazypod_music_root_preview_render(
        parent, selected, defer_media);
}

void crazypod_music_feature_render_item_preview(
    lv_obj_t *parent, const struct route_state *state,
    const struct crazypod_track *track,
    const lv_font_t *metadata_font)
{
    crazypod_music_item_preview_render(
        parent, state, track, metadata_font);
}

#endif
