#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "core_alloc.h"

#include "crazypod_music_storage.h"

static bool checked_product(size_t count, size_t item_size, size_t *result)
{
    if(item_size != 0 && count > SIZE_MAX / item_size)
        return false;
    *result = count * item_size;
    return true;
}

static bool checked_add(size_t *total, size_t amount)
{
    if(*total > SIZE_MAX - amount)
        return false;
    *total += amount;
    return true;
}

static void release_tracks(struct crazypod_music_storage *storage)
{
    if(storage->tracks_handle > 0) {
        core_unpin(storage->tracks_handle);
        storage->tracks_handle = core_free(storage->tracks_handle);
    }
    storage->track_capacity = 0;
    storage->tracks = NULL;
}

static void release_groups(struct crazypod_music_storage *storage)
{
    if(storage->groups_handle > 0) {
        core_unpin(storage->groups_handle);
        storage->groups_handle = core_free(storage->groups_handle);
    }
    storage->group_capacity = 0;
    storage->albums = NULL;
    storage->artists = NULL;
    storage->artist_first_tracks = NULL;
    storage->artist_track_counts = NULL;
    storage->artist_track_indices = NULL;
    storage->album_track_indices = NULL;
    storage->path_track_indices = NULL;
    storage->favorite_track_indices = NULL;
    storage->search_track_indices = NULL;
}

void crazypod_music_storage_init(struct crazypod_music_storage *storage)
{
    if(storage != NULL)
        memset(storage, 0, sizeof(*storage));
}

void crazypod_music_storage_release(struct crazypod_music_storage *storage)
{
    if(storage == NULL)
        return;
    release_groups(storage);
    release_tracks(storage);
}

bool crazypod_music_storage_allocate_tracks(
    struct crazypod_music_storage *storage, size_t count)
{
    size_t bytes;
    int handle;

    if(storage == NULL ||
       !checked_product(count, sizeof(struct crazypod_track), &bytes))
        return false;
    release_groups(storage);
    release_tracks(storage);
    if(count == 0)
        return true;

    /* Track pointers are exposed by the public music API, so this long-lived
     * block must stay immovable. It replaces a larger static BSS allocation
     * and is sized to the actual library. */
    handle = core_alloc(bytes);
    if(handle <= 0)
        return false;
    core_pin(handle);
    storage->tracks_handle = handle;
    storage->track_capacity = count;
    storage->tracks = core_get_data(handle);
    return true;
}

void crazypod_music_storage_shrink_tracks(
    struct crazypod_music_storage *storage, size_t count)
{
    size_t bytes;

    if(storage == NULL || count >= storage->track_capacity ||
       !checked_product(count, sizeof(struct crazypod_track), &bytes))
        return;
    if(count == 0) {
        release_tracks(storage);
        return;
    }
    core_unpin(storage->tracks_handle);
    if(core_shrink(storage->tracks_handle, NULL, bytes)) {
        storage->track_capacity = count;
        storage->tracks = core_get_data(storage->tracks_handle);
    }
    core_pin(storage->tracks_handle);
}

bool crazypod_music_storage_allocate_groups(
    struct crazypod_music_storage *storage, size_t count)
{
    unsigned char *cursor;
    size_t albums_bytes;
    size_t artists_bytes;
    size_t index_bytes;
    size_t total = 0;
    int handle;
    int i;

    if(storage == NULL || count > storage->track_capacity ||
       !checked_product(count, sizeof(struct crazypod_album),
                        &albums_bytes) ||
       !checked_product(count, sizeof(*storage->artists),
                        &artists_bytes) ||
       !checked_product(count, sizeof(uint32_t), &index_bytes) ||
       !checked_add(&total, albums_bytes) ||
       !checked_add(&total, artists_bytes))
        return false;
    for(i = 0; i < 7; ++i) {
        if(!checked_add(&total, index_bytes))
            return false;
    }

    release_groups(storage);
    if(count == 0)
        return true;
    handle = core_alloc(total);
    if(handle <= 0)
        return false;
    core_pin(handle);

    storage->groups_handle = handle;
    storage->group_capacity = count;
    cursor = core_get_data(handle);
    storage->albums = (struct crazypod_album *)cursor;
    cursor += albums_bytes;
    storage->artists = (char (*)[72])cursor;
    cursor += artists_bytes;
    storage->artist_first_tracks = (uint32_t *)cursor;
    cursor += index_bytes;
    storage->artist_track_counts = (uint32_t *)cursor;
    cursor += index_bytes;
    storage->artist_track_indices = (uint32_t *)cursor;
    cursor += index_bytes;
    storage->album_track_indices = (uint32_t *)cursor;
    cursor += index_bytes;
    storage->path_track_indices = (uint32_t *)cursor;
    cursor += index_bytes;
    storage->favorite_track_indices = (uint32_t *)cursor;
    cursor += index_bytes;
    storage->search_track_indices = (uint32_t *)cursor;
    return true;
}

#endif
