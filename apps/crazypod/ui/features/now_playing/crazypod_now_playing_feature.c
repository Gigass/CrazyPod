#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "kernel.h"

#include "../../../crazypod_artwork.h"
#include "../../../crazypod_music.h"
#include "../../../crazypod_playlist.h"
#include "crazypod_now_playing_feature.h"

#define NOW_ARTWORK_CACHE_SIZE CRAZYPOD_COVERFLOW_ARTWORK_SIZE

static struct {
    struct crazypod_now_playing_navigation_host host;
    char prefetch_track_path[MAX_PATH];
    char pending_track_path[MAX_PATH];
    bool open_pending;
    int pending_route_depth;
    enum crazypod_route pending_route;
    unsigned artwork_generation_seen;
    unsigned prefetch_generation_seen;
} navigation;

static const struct crazypod_track *current_track(void)
{
    const char *path =
        crazypod_queue_path(crazypod_queue_index());

    return crazypod_music_track(
        crazypod_music_find_track(path));
}

static void remember_artwork_generations(void)
{
    navigation.artwork_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);
    navigation.prefetch_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT);
}

int crazypod_now_playing_feature_item_count(
    const struct route_state *state)
{
    if(state->route == MUSIC_ROUTE_QUEUE)
        return crazypod_queue_count();
    return state->route == MUSIC_ROUTE_NOW_PLAYING ? 1 : 0;
}

const char *crazypod_now_playing_feature_title(
    const struct route_state *state)
{
    return state->route == MUSIC_ROUTE_QUEUE
        ? "UP NEXT" : "NOW PLAYING";
}

bool crazypod_now_playing_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    const char *path;
    const struct crazypod_track *track;

    if(state->route == MUSIC_ROUTE_NOW_PLAYING) {
        *title = "Now Playing";
        return true;
    }
    if(state->route != MUSIC_ROUTE_QUEUE)
        return false;
    path = crazypod_queue_path(index);
    track = crazypod_music_track(crazypod_music_find_track(path));
    *title = track != NULL ? track->title : "";
    return true;
}

void crazypod_now_playing_navigation_configure(
    const struct crazypod_now_playing_navigation_host *host)
{
    if(host != NULL)
        navigation.host = *host;
}

void crazypod_now_playing_navigation_initialize(void)
{
    navigation.prefetch_track_path[0] = '\0';
    navigation.pending_track_path[0] = '\0';
    navigation.open_pending = false;
    remember_artwork_generations();
}

void crazypod_now_playing_navigation_reset(void)
{
    navigation.pending_track_path[0] = '\0';
    navigation.open_pending = false;
}

int crazypod_now_playing_artwork_slot(
    const struct crazypod_track *track)
{
    return track != NULL &&
        strcmp(navigation.prefetch_track_path, track->path) == 0
            ? CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT
            : CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT;
}

void crazypod_now_playing_prefetch_queue_artwork(
    int queue_index)
{
    const char *path = crazypod_queue_path(queue_index);
    const struct crazypod_track *track =
        crazypod_music_track(crazypod_music_find_track(path));

    if(track == NULL) {
        navigation.prefetch_track_path[0] = '\0';
        return;
    }
    snprintf(
        navigation.prefetch_track_path,
        sizeof(navigation.prefetch_track_path),
        "%s", track->path);
    (void)crazypod_artwork_load_priority(
        CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT,
        track, NOW_ARTWORK_CACHE_SIZE, 0);
    if(navigation.host.boost != NULL)
        navigation.host.boost(HZ / 5);
}

void crazypod_now_playing_request_open(void)
{
    const struct crazypod_track *track = current_track();
    enum crazypod_artwork_state state;
    int slot;

    crazypod_now_playing_navigation_reset();
    if(track == NULL) {
        navigation.host.push_now_playing();
        return;
    }
    slot = crazypod_now_playing_artwork_slot(track);
    (void)crazypod_artwork_load_priority(
        slot, track, NOW_ARTWORK_CACHE_SIZE, 0);
    state = crazypod_artwork_state(
        slot, track, NOW_ARTWORK_CACHE_SIZE);
    if(state != CRAZYPOD_ARTWORK_PENDING) {
        remember_artwork_generations();
        navigation.host.push_now_playing();
        return;
    }

    navigation.open_pending = true;
    navigation.pending_route_depth =
        navigation.host.route_depth();
    navigation.pending_route =
        navigation.host.current_route();
    snprintf(
        navigation.pending_track_path,
        sizeof(navigation.pending_track_path),
        "%s", track->path);
    navigation.host.boost(HZ / 2);
}

void crazypod_now_playing_process_open(void)
{
    const struct crazypod_track *track;
    enum crazypod_artwork_state state;
    int slot;

    if(!navigation.open_pending)
        return;
    if(!navigation.host.product_active() ||
       navigation.host.route_depth() <= 0 ||
       navigation.host.route_depth() !=
           navigation.pending_route_depth ||
       navigation.host.current_route() !=
           navigation.pending_route) {
        crazypod_now_playing_navigation_reset();
        return;
    }
    track = current_track();
    if(track == NULL) {
        crazypod_now_playing_navigation_reset();
        navigation.host.push_now_playing();
        return;
    }
    if(strcmp(
           navigation.pending_track_path,
           track->path) != 0) {
        snprintf(
            navigation.pending_track_path,
            sizeof(navigation.pending_track_path),
            "%s", track->path);
    }
    slot = crazypod_now_playing_artwork_slot(track);
    (void)crazypod_artwork_load_priority(
        slot, track, NOW_ARTWORK_CACHE_SIZE, 0);
    state = crazypod_artwork_state(
        slot, track, NOW_ARTWORK_CACHE_SIZE);
    if(state == CRAZYPOD_ARTWORK_PENDING)
        return;
    crazypod_now_playing_navigation_reset();
    remember_artwork_generations();
    navigation.host.push_now_playing();
}

bool crazypod_now_playing_artwork_changed(void)
{
    unsigned artwork = crazypod_artwork_slot_generation(
        CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT);
    unsigned prefetch = crazypod_artwork_slot_generation(
        CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT);

    if(artwork == navigation.artwork_generation_seen &&
       prefetch == navigation.prefetch_generation_seen)
        return false;
    navigation.artwork_generation_seen = artwork;
    navigation.prefetch_generation_seen = prefetch;
    return true;
}

#endif
