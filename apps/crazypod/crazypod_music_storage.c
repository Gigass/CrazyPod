#include "config.h"

#ifdef HAVE_CRAZYPOD_UI

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

static void release_handle(int *handle)
{
    if(*handle > 0) {
        core_unpin(*handle);
        *handle = core_free(*handle);
    }
    *handle = 0;
}

static void release_groups(struct crazypod_music_storage *storage)
{
    release_handle(&storage->groups_handle);
    storage->album_capacity = 0;
    storage->artist_capacity = 0;
    storage->albums = NULL;
    storage->artist_names = NULL;
    storage->artist_first_tracks = NULL;
    storage->artist_track_counts = NULL;
}

static void release_indices(struct crazypod_music_storage *storage)
{
    release_handle(&storage->indices_handle);
    storage->index_capacity = 0;
    storage->artist_track_indices = NULL;
    storage->album_track_indices = NULL;
    storage->path_track_indices = NULL;
    storage->favorite_track_indices = NULL;
    storage->search_track_indices = NULL;
}

static void release_text(struct crazypod_music_storage *storage)
{
    release_handle(&storage->text_handle);
    storage->text_capacity = 0;
    storage->text_used = 0;
    storage->text = NULL;
}

static void release_tracks(struct crazypod_music_storage *storage)
{
    release_handle(&storage->tracks_handle);
    storage->track_capacity = 0;
    storage->tracks = NULL;
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
    release_indices(storage);
    release_text(storage);
    release_tracks(storage);
}

bool crazypod_music_storage_allocate_tracks(
    struct crazypod_music_storage *storage, size_t count)
{
    size_t bytes;
    int handle;

    if(storage == NULL ||
       !checked_product(count, sizeof(struct crazypod_track_record),
                        &bytes))
        return false;
    release_groups(storage);
    release_indices(storage);
    release_text(storage);
    release_tracks(storage);
    if(count == 0)
        return true;

    /* Scanning and queue construction address this block directly. Keep it
     * immovable while allocated; public readers receive copied values. */
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
       !checked_product(count, sizeof(struct crazypod_track_record),
                        &bytes))
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

bool crazypod_music_storage_allocate_text(
    struct crazypod_music_storage *storage, size_t bytes)
{
    int handle;

    if(storage == NULL)
        return false;
    release_text(storage);
    /* Offset 0 is reserved for the empty string so that a zeroed record
     * reads as blank rather than as the first path in the pool. */
    if(!checked_add(&bytes, 1))
        return false;
    handle = core_alloc(bytes);
    if(handle <= 0)
        return false;
    core_pin(handle);
    storage->text_handle = handle;
    storage->text_capacity = bytes;
    storage->text = core_get_data(handle);
    storage->text[0] = '\0';
    storage->text_used = 1;
    return true;
}

void crazypod_music_storage_shrink_text(
    struct crazypod_music_storage *storage)
{
    if(storage == NULL || storage->text_handle <= 0 ||
       storage->text_used == 0 ||
       storage->text_used >= storage->text_capacity)
        return;
    core_unpin(storage->text_handle);
    if(core_shrink(storage->text_handle, NULL, storage->text_used)) {
        storage->text_capacity = storage->text_used;
        storage->text = core_get_data(storage->text_handle);
    }
    core_pin(storage->text_handle);
}

size_t crazypod_music_storage_add_text(
    struct crazypod_music_storage *storage, const char *text,
    size_t max_length)
{
    size_t length;
    size_t offset;

    if(storage == NULL || storage->text == NULL)
        return SIZE_MAX;
    if(text == NULL || text[0] == '\0')
        return 0;
    length = strlen(text);
    if(length > max_length)
        length = max_length;
    if(length + 1 > storage->text_capacity - storage->text_used)
        return SIZE_MAX;
    offset = storage->text_used;
    memcpy(storage->text + offset, text, length);
    storage->text[offset + length] = '\0';
    storage->text_used = offset + length + 1;
    return offset;
}

size_t crazypod_music_storage_add_text_truncating(
    struct crazypod_music_storage *storage, const char *text,
    size_t max_length)
{
    size_t available;

    if(storage == NULL || storage->text == NULL)
        return SIZE_MAX;
    available = storage->text_capacity - storage->text_used;
    if(available == 0)
        return SIZE_MAX;
    if(max_length > available - 1)
        max_length = available - 1;
    return crazypod_music_storage_add_text(storage, text, max_length);
}

bool crazypod_music_storage_allocate_indices(
    struct crazypod_music_storage *storage, size_t count)
{
    unsigned char *cursor;
    size_t index_bytes;
    size_t total = 0;
    int handle;
    int i;

    if(storage == NULL || count > storage->track_capacity ||
       !checked_product(count, sizeof(uint32_t), &index_bytes))
        return false;
    for(i = 0; i < 5; ++i) {
        if(!checked_add(&total, index_bytes))
            return false;
    }
    release_groups(storage);
    release_indices(storage);
    if(count == 0)
        return true;
    handle = core_alloc(total);
    if(handle <= 0)
        return false;
    core_pin(handle);

    storage->indices_handle = handle;
    storage->index_capacity = count;
    cursor = core_get_data(handle);
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

bool crazypod_music_storage_allocate_groups(
    struct crazypod_music_storage *storage,
    size_t album_count, size_t artist_count)
{
    unsigned char *cursor;
    size_t albums_bytes;
    size_t artist_bytes;
    size_t total = 0;
    int handle;
    int i;

    if(storage == NULL ||
       !checked_product(album_count,
                        sizeof(struct crazypod_album_record),
                        &albums_bytes) ||
       !checked_product(artist_count, sizeof(uint32_t), &artist_bytes) ||
       !checked_add(&total, albums_bytes))
        return false;
    for(i = 0; i < 3; ++i) {
        if(!checked_add(&total, artist_bytes))
            return false;
    }

    release_groups(storage);
    if(total == 0)
        return true;
    handle = core_alloc(total);
    if(handle <= 0)
        return false;
    core_pin(handle);

    storage->groups_handle = handle;
    storage->album_capacity = album_count;
    storage->artist_capacity = artist_count;
    cursor = core_get_data(handle);
    storage->albums = (struct crazypod_album_record *)cursor;
    cursor += albums_bytes;
    storage->artist_names = (uint32_t *)cursor;
    cursor += artist_bytes;
    storage->artist_first_tracks = (uint32_t *)cursor;
    cursor += artist_bytes;
    storage->artist_track_counts = (uint32_t *)cursor;
    return true;
}

#endif
