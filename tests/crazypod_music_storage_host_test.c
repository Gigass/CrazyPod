#include <assert.h>
#include <string.h>

#include "core_alloc.h"
#include "crazypod_music_storage.h"

#define LARGE_LIBRARY_TRACKS 4097
#define SHRUNK_LIBRARY_TRACKS 3001

int main(void)
{
    struct crazypod_music_storage storage;

    crazypod_music_storage_init(&storage);
    assert(crazypod_music_storage_allocate_tracks(
        &storage, LARGE_LIBRARY_TRACKS));
    assert(storage.track_capacity == LARGE_LIBRARY_TRACKS);
    assert(storage.tracks != NULL);
    assert(test_core_alloc_active_handles() == 1);
    assert(test_core_alloc_pin_count() == 1);

    strcpy(storage.tracks[SHRUNK_LIBRARY_TRACKS - 1].title,
           "last retained track");
    crazypod_music_storage_shrink_tracks(
        &storage, SHRUNK_LIBRARY_TRACKS);
    assert(storage.track_capacity == SHRUNK_LIBRARY_TRACKS);
    assert(strcmp(storage.tracks[SHRUNK_LIBRARY_TRACKS - 1].title,
                  "last retained track") == 0);
    assert(test_core_alloc_pin_count() == 1);

    assert(crazypod_music_storage_allocate_groups(
        &storage, SHRUNK_LIBRARY_TRACKS));
    assert(storage.group_capacity == SHRUNK_LIBRARY_TRACKS);
    assert(storage.albums != NULL);
    assert(storage.artists != NULL);
    assert(storage.search_track_indices != NULL);
    storage.album_track_indices[SHRUNK_LIBRARY_TRACKS - 1] =
        SHRUNK_LIBRARY_TRACKS - 1;
    assert(storage.album_track_indices[SHRUNK_LIBRARY_TRACKS - 1] ==
           SHRUNK_LIBRARY_TRACKS - 1);
    assert(test_core_alloc_active_handles() == 2);
    assert(test_core_alloc_pin_count() == 2);

    crazypod_music_storage_release(&storage);
    assert(test_core_alloc_active_handles() == 0);
    assert(test_core_alloc_pin_count() == 0);
    return 0;
}
