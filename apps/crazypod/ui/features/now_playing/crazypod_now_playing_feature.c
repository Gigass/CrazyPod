#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "kernel.h"
#include "lcd.h"
#include "playlist.h"
#include "settings.h"

#include "../../../crazypod_artwork.h"
#include "../../../crazypod_image.h"
#include "../../../crazypod_music.h"
#include "../../../crazypod_playlist.h"
#include "crazypod_now_playing_feature.h"

#define NOW_ARTWORK_PLACEHOLDER_SIZE CRAZYPOD_ARTWORK_CACHE_SIZE
#define NOW_ARTWORK_SLOT_COUNT 3

static const int now_artwork_slots[NOW_ARTWORK_SLOT_COUNT] = {
    CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT,
    CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT,
    CRAZYPOD_NOW_PREFETCH_SECOND_ARTWORK_SLOT,
};
static int configured_source_size = CRAZYPOD_ARTWORK_CACHE_SIZE;

static fb_data placeholder_pixels[
    NOW_ARTWORK_PLACEHOLDER_SIZE * NOW_ARTWORK_PLACEHOLDER_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t placeholder_descriptor;

static struct {
    struct crazypod_now_playing_navigation_host host;
    char slot_paths[NOW_ARTWORK_SLOT_COUNT][MAX_PATH];
    char target_path[MAX_PATH];
    char committed_path[MAX_PATH];
    const lv_image_dsc_t *committed_artwork;
    unsigned committed_generation;
    unsigned committed_source_generation;
    unsigned committed_generation_seen;
    int target_slot;
    int committed_slot;
    int source_size;
} navigation;

static bool copy_current_track(struct crazypod_track *track)
{
    char path[MAX_PATH];

    return crazypod_queue_copy_path(
            crazypod_queue_index(), path, sizeof(path)) &&
        crazypod_music_copy_track(crazypod_music_find_track(path), track);
}

static void prepare_placeholder(void)
{
    int ring_inner = 31 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int ring_outer = 42 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int stem_left = 66 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int stem_right = 72 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int stem_top = 42 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int stem_bottom = 83 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int flag_right = 88 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int note_x = 61 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int note_y = 87 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int note_radius = 10 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128;
    int y;

    for(y = 0; y < NOW_ARTWORK_PLACEHOLDER_SIZE; ++y) {
        int x;

        for(x = 0; x < NOW_ARTWORK_PLACEHOLDER_SIZE; ++x) {
            int dx = x - NOW_ARTWORK_PLACEHOLDER_SIZE / 2;
            int dy = y - NOW_ARTWORK_PLACEHOLDER_SIZE / 2;
            int radius = dx * dx + dy * dy;
            unsigned shade = 18u +
                (unsigned)(x + y) * 128u /
                    (24u * NOW_ARTWORK_PLACEHOLDER_SIZE);
            fb_data color = LCD_RGBPACK(shade, shade, shade + 6u);

            if(radius >= ring_inner * ring_inner &&
               radius <= ring_outer * ring_outer)
                color = LCD_RGBPACK(48, 49, 61);
            if((x >= stem_left && x <= stem_right &&
                y >= stem_top && y <= stem_bottom) ||
               (x >= stem_right && x <= flag_right &&
                y >= stem_top &&
                y <= 48 * NOW_ARTWORK_PLACEHOLDER_SIZE / 128) ||
               ((x - note_x) * (x - note_x) +
                (y - note_y) * (y - note_y) <=
                    note_radius * note_radius))
                color = LCD_RGBPACK(173, 177, 195);
            placeholder_pixels[y * NOW_ARTWORK_PLACEHOLDER_SIZE + x] =
                color;
        }
    }
    (void)crazypod_image_configure_rgb565(
        &placeholder_descriptor, placeholder_pixels,
        NOW_ARTWORK_PLACEHOLDER_SIZE, NOW_ARTWORK_PLACEHOLDER_SIZE);
}

static int slot_index_for_path(const char *path)
{
    int index;

    if(path == NULL || path[0] == '\0')
        return -1;
    for(index = 0; index < NOW_ARTWORK_SLOT_COUNT; ++index)
        if(strcmp(navigation.slot_paths[index], path) == 0)
            return index;
    return -1;
}

static bool copy_queue_track(int index, struct crazypod_track *track)
{
    char path[MAX_PATH];

    if(!crazypod_queue_copy_path(index, path, sizeof(path)))
        return false;
    return crazypod_music_copy_track(crazypod_music_find_track(path), track);
}

static bool copy_next_track(int steps, struct crazypod_track *track)
{
    int count = crazypod_queue_count();
    int index;

    if(count <= 0 || steps <= 0 ||
       crazypod_queue_repeat() == REPEAT_ONE)
        return false;
    index = crazypod_queue_index() + steps;
    if(index >= count) {
        if(crazypod_queue_repeat() != REPEAT_ALL)
            return false;
        index %= count;
    }
    return copy_queue_track(index, track);
}

static void commit_placeholder(const char *path)
{
    if(path == NULL)
        path = "";
    if(navigation.committed_artwork == &placeholder_descriptor &&
       strcmp(navigation.committed_path, path) == 0)
        return;
    navigation.committed_artwork = &placeholder_descriptor;
    navigation.committed_slot = -1;
    navigation.committed_source_generation = 0;
    snprintf(navigation.committed_path,
             sizeof(navigation.committed_path), "%s", path);
    ++navigation.committed_generation;
    if(navigation.committed_generation == 0)
        ++navigation.committed_generation;
}

static void commit_target_artwork(
    const struct crazypod_track *track,
    const lv_image_dsc_t *descriptor)
{
    unsigned source_generation = crazypod_artwork_slot_generation(
        now_artwork_slots[navigation.target_slot]);

    if(descriptor == NULL || track == NULL)
        return;
    if(navigation.committed_artwork == descriptor &&
       navigation.committed_source_generation == source_generation &&
       strcmp(navigation.committed_path, track->path) == 0)
        return;
    navigation.committed_artwork = descriptor;
    navigation.committed_slot = navigation.target_slot;
    navigation.committed_source_generation = source_generation;
    snprintf(navigation.committed_path,
             sizeof(navigation.committed_path), "%s", track->path);
    ++navigation.committed_generation;
    if(navigation.committed_generation == 0)
        ++navigation.committed_generation;
}

static int reserve_matching_slot(
    const struct crazypod_track *track, bool reserved[])
{
    int index;

    if(track == NULL)
        return -1;
    index = slot_index_for_path(track->path);
    if(index >= 0 && !reserved[index]) {
        reserved[index] = true;
        return index;
    }
    for(index = 0; index < NOW_ARTWORK_SLOT_COUNT; ++index) {
        if(reserved[index])
            continue;
        reserved[index] = true;
        return index;
    }
    return -1;
}

static void schedule_prefetch(
    const struct crazypod_track *track, int priority,
    bool reserved[])
{
    int index;

    if(track == NULL ||
       strcmp(track->path, navigation.target_path) == 0)
        return;
    index = reserve_matching_slot(track, reserved);
    if(index < 0)
        return;
    snprintf(navigation.slot_paths[index],
             sizeof(navigation.slot_paths[index]),
             "%s", track->path);
    (void)crazypod_artwork_load_source_priority(
        now_artwork_slots[index], track,
        navigation.source_size, priority);
}

int crazypod_now_playing_feature_item_count(
    const struct route_state *state)
{
    if(state->route == MUSIC_ROUTE_QUEUE)
        return crazypod_queue_count();
    return state->route == MUSIC_ROUTE_NOW_PLAYING ? 1 : 0;
}

enum crazypod_menu_icon crazypod_now_playing_feature_item_icon(
    const struct route_state *state, int index)
{
    return state->route == MUSIC_ROUTE_QUEUE && index >= 0
        ? CRAZYPOD_MENU_ICON_QUEUE
        : CRAZYPOD_MENU_ICON_NONE;
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
    static char queue_title[96];
    char path[MAX_PATH];
    struct crazypod_track track;
    bool have_track;

    if(state->route == MUSIC_ROUTE_NOW_PLAYING) {
        *title = CP_TR("Now Playing");
        return true;
    }
    if(state->route != MUSIC_ROUTE_QUEUE)
        return false;
    have_track = crazypod_queue_copy_path(index, path, sizeof(path)) &&
        crazypod_music_copy_track(crazypod_music_find_track(path), &track);
    snprintf(queue_title, sizeof(queue_title), "%s",
             have_track ? track.title : "");
    *title = queue_title;
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
    memset(navigation.slot_paths, 0, sizeof(navigation.slot_paths));
    navigation.target_path[0] = '\0';
    navigation.committed_path[0] = '\0';
    navigation.target_slot = 0;
    navigation.committed_slot = -1;
    navigation.source_size = configured_source_size;
    navigation.committed_generation = 1;
    navigation.committed_generation_seen = 1;
    navigation.committed_source_generation = 0;
    prepare_placeholder();
    navigation.committed_artwork = &placeholder_descriptor;
}

void crazypod_now_playing_artwork_set_source_size(int source_size)
{
    if(source_size < 16)
        source_size = 16;
    if(source_size > CRAZYPOD_NOW_ARTWORK_MAX_SIZE)
        source_size = CRAZYPOD_NOW_ARTWORK_MAX_SIZE;
    configured_source_size = source_size;
    navigation.source_size = source_size;
}

int crazypod_now_playing_artwork_slot(
    const struct crazypod_track *track)
{
    int index = track != NULL
        ? slot_index_for_path(track->path) : -1;

    if(index < 0)
        index = navigation.target_slot;
    if(index < 0 || index >= NOW_ARTWORK_SLOT_COUNT)
        index = 0;
    return now_artwork_slots[index];
}

void crazypod_now_playing_artwork_sync(void)
{
    struct crazypod_track track;
    struct crazypod_track first;
    struct crazypod_track second;
    bool have_track = copy_current_track(&track);
    bool have_first;
    bool have_second;
    const lv_image_dsc_t *descriptor;
    enum crazypod_artwork_state state;
    bool reserved[NOW_ARTWORK_SLOT_COUNT] = { false, false, false };
    int matched;

    if(!have_track) {
        navigation.target_path[0] = '\0';
        commit_placeholder("");
        return;
    }
    if(strcmp(navigation.target_path, track.path) != 0) {
        matched = slot_index_for_path(track.path);
        if(matched >= 0)
            navigation.target_slot = matched;
        snprintf(navigation.target_path,
                 sizeof(navigation.target_path), "%s", track.path);
        snprintf(navigation.slot_paths[navigation.target_slot],
                 sizeof(navigation.slot_paths[navigation.target_slot]),
                 "%s", track.path);
    }
    reserved[navigation.target_slot] = true;
    if(navigation.committed_slot >= 0 &&
       navigation.committed_slot != navigation.target_slot)
        reserved[navigation.committed_slot] = true;

    descriptor = crazypod_artwork_load_source_priority(
        now_artwork_slots[navigation.target_slot], &track,
        navigation.source_size, 0);
    state = crazypod_artwork_state(
        now_artwork_slots[navigation.target_slot], &track,
        navigation.source_size);
    if(state == CRAZYPOD_ARTWORK_IMAGE && descriptor != NULL)
        commit_target_artwork(&track, descriptor);
    else if(state == CRAZYPOD_ARTWORK_EMPTY ||
            state == CRAZYPOD_ARTWORK_ERROR)
        commit_placeholder(track.path);

    if(navigation.committed_slot == navigation.target_slot)
        reserved[navigation.committed_slot] = true;
    have_first = copy_next_track(1, &first);
    schedule_prefetch(have_first ? &first : NULL, 10, reserved);
    have_second = copy_next_track(2, &second);
    if(have_second &&
       (!have_first || strcmp(second.path, first.path) != 0))
        schedule_prefetch(&second, 20, reserved);
}

const lv_image_dsc_t *crazypod_now_playing_artwork_committed(
    const char **track_path, unsigned *generation)
{
    if(track_path != NULL)
        *track_path = navigation.committed_path;
    if(generation != NULL)
        *generation = navigation.committed_generation;
    return navigation.committed_artwork;
}

unsigned crazypod_now_playing_artwork_committed_generation(void)
{
    return navigation.committed_generation;
}

void crazypod_now_playing_request_open(void)
{
    struct crazypod_track track;

    crazypod_now_playing_artwork_sync();
    if(copy_current_track(&track) && navigation.host.boost != NULL)
        navigation.host.boost(HZ / 2);
    navigation.committed_generation_seen =
        navigation.committed_generation;
    navigation.host.push_now_playing();
}

bool crazypod_now_playing_artwork_changed(void)
{
    crazypod_now_playing_artwork_sync();
    if(navigation.committed_generation ==
       navigation.committed_generation_seen)
        return false;
    navigation.committed_generation_seen =
        navigation.committed_generation;
    return true;
}

#endif
