#ifndef CRAZYPOD_MUSIC_STORAGE_H
#define CRAZYPOD_MUSIC_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "crazypod_music.h"

struct crazypod_music_storage {
    int tracks_handle;
    int groups_handle;
    size_t track_capacity;
    size_t group_capacity;
    struct crazypod_track *tracks;
    struct crazypod_album *albums;
    char (*artists)[72];
    uint32_t *artist_first_tracks;
    uint32_t *artist_track_counts;
    uint32_t *artist_track_indices;
    uint32_t *album_track_indices;
    uint32_t *path_track_indices;
    uint32_t *favorite_track_indices;
    uint32_t *search_track_indices;
};

void crazypod_music_storage_init(struct crazypod_music_storage *storage);
void crazypod_music_storage_release(struct crazypod_music_storage *storage);
bool crazypod_music_storage_allocate_tracks(
    struct crazypod_music_storage *storage, size_t count);
void crazypod_music_storage_shrink_tracks(
    struct crazypod_music_storage *storage, size_t count);
bool crazypod_music_storage_allocate_groups(
    struct crazypod_music_storage *storage, size_t count);

#endif
