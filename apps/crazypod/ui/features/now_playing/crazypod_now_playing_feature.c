#include "config.h"

#include "../../../crazypod_l10n.h"

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
        ? CP_TR("UP NEXT") : CP_TR("NOW PLAYING");
}

bool crazypod_now_playing_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    const char *path;
    const struct crazypod_track *track;

    if(state->route == MUSIC_ROUTE_NOW_PLAYING) {
        *title = CP_TR("Now Playing");
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
    remember_artwork_generations();
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
    int slot;

    if(track != NULL) {
        slot = crazypod_now_playing_artwork_slot(track);
        (void)crazypod_artwork_load_priority(
            slot, track, NOW_ARTWORK_CACHE_SIZE, 0);
        if(navigation.host.boost != NULL)
            navigation.host.boost(HZ / 2);
    }
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
