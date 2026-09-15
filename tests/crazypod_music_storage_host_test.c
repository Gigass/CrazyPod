#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core_alloc.h"
#include "crazypod_music_storage.h"

#define LARGE_LIBRARY_TRACKS 30000
#define SHRUNK_LIBRARY_TRACKS 29000
#define TEXT_POOL_BYTES (LARGE_LIBRARY_TRACKS * 200)

static struct crazypod_music_storage storage;

static void check_allocation_shape(void)
{
    crazypod_music_storage_init(&storage);
    assert(crazypod_music_storage_allocate_tracks(
        &storage, LARGE_LIBRARY_TRACKS));
    assert(storage.track_capacity == LARGE_LIBRARY_TRACKS);
    assert(storage.tracks != NULL);
    assert(test_core_alloc_active_handles() == 1);
    assert(test_core_alloc_pin_count() == 1);

    assert(crazypod_music_storage_allocate_text(
        &storage, TEXT_POOL_BYTES));
    assert(storage.text != NULL);
    /* Offset 0 always reads as the empty string, so a zeroed record is
     * blank rather than pointing at the first path in the pool. */
    assert(storage.text_used == 1);
    assert(crazypod_music_storage_text(&storage, 0)[0] == '\0');
}

static void check_text_pool(void)
{
    size_t first = crazypod_music_storage_add_text(
        &storage, "/Music/Artist/Album/01 Track.mp3", 259);
    size_t second = crazypod_music_storage_add_text(
        &storage, "Track Title", 95);

    assert(first == 1);
    assert(second > first);
    assert(strcmp(crazypod_music_storage_text(&storage, first),
                  "/Music/Artist/Album/01 Track.mp3") == 0);
    assert(strcmp(crazypod_music_storage_text(&storage, second),
                  "Track Title") == 0);
    /* An empty string never consumes pool space. */
    assert(crazypod_music_storage_add_text(&storage, "", 95) == 0);
    assert(crazypod_music_storage_add_text(&storage, NULL, 95) == 0);
    /* max_length shortens the stored copy. */
    {
        size_t clipped = crazypod_music_storage_add_text(
            &storage, "abcdefgh", 3);
        assert(strcmp(crazypod_music_storage_text(&storage, clipped),
                      "abc") == 0);
    }
}

static void check_pool_exhaustion(void)
{
    struct crazypod_music_storage small;
    size_t offset;

    crazypod_music_storage_init(&small);
    assert(crazypod_music_storage_allocate_text(&small, 8));
    offset = crazypod_music_storage_add_text(&small, "1234567", 259);
    assert(offset == 1);
    /* A strict append refuses rather than writing a partial path. */
    assert(crazypod_music_storage_add_text(&small, "x", 259) == SIZE_MAX);
    /* A display append keeps what fits; here nothing does. */
    assert(crazypod_music_storage_add_text_truncating(
        &small, "x", 259) == SIZE_MAX);
    crazypod_music_storage_release(&small);
    assert(test_core_alloc_active_handles() == 2);
}

static void check_display_truncation(void)
{
    struct crazypod_music_storage small;
    size_t offset;

    crazypod_music_storage_init(&small);
    assert(crazypod_music_storage_allocate_text(&small, 5));
    offset = crazypod_music_storage_add_text_truncating(
        &small, "abcdefgh", 95);
    assert(offset == 1);
    assert(strcmp(crazypod_music_storage_text(&small, offset),
                  "abcd") == 0);
    crazypod_music_storage_release(&small);
}

static void check_groups_sized_independently(void)
{
    /* The whole point of the split: album and artist tables are sized by
     * their own counts, not by the track count. */
    assert(crazypod_music_storage_allocate_indices(
        &storage, SHRUNK_LIBRARY_TRACKS));
    assert(storage.index_capacity == SHRUNK_LIBRARY_TRACKS);
    assert(storage.search_track_indices != NULL);
    storage.album_track_indices[SHRUNK_LIBRARY_TRACKS - 1] =
        SHRUNK_LIBRARY_TRACKS - 1;
    assert(storage.album_track_indices[SHRUNK_LIBRARY_TRACKS - 1] ==
           SHRUNK_LIBRARY_TRACKS - 1);

    assert(crazypod_music_storage_allocate_groups(&storage, 2500, 900));
    assert(storage.album_capacity == 2500);
    assert(storage.artist_capacity == 900);
    assert(storage.albums != NULL);
    assert(storage.artist_names != NULL);
    assert(storage.artist_track_counts != NULL);
    storage.albums[2499].track_count = 7;
    storage.artist_track_counts[899] = 3;
    assert(storage.albums[2499].track_count == 7);
    assert(storage.artist_track_counts[899] == 3);
}

static void check_shrink_preserves_contents(void)
{
    size_t kept = crazypod_music_storage_add_text(
        &storage, "kept across shrink", 95);

    strcpy((char *)&storage.tracks[SHRUNK_LIBRARY_TRACKS - 1], "xx");
    crazypod_music_storage_shrink_tracks(
        &storage, SHRUNK_LIBRARY_TRACKS);
    assert(storage.track_capacity == SHRUNK_LIBRARY_TRACKS);
    assert(memcmp(&storage.tracks[SHRUNK_LIBRARY_TRACKS - 1],
                  "xx", 2) == 0);

    crazypod_music_storage_shrink_text(&storage);
    assert(storage.text_capacity == storage.text_used);
    assert(strcmp(crazypod_music_storage_text(&storage, kept),
                  "kept across shrink") == 0);
    /* tracks, text, indices and groups are each pinned. */
    assert(test_core_alloc_pin_count() == 4);
}

static void report_footprint(void)
{
    size_t per_track = sizeof(struct crazypod_track_record) +
        5 * sizeof(uint32_t);

    printf("crazypod music storage: %zu bytes/track fixed, "
           "%zu MB for %d tracks with a 200-byte text budget\n",
           per_track,
           (per_track + 200) * (size_t)LARGE_LIBRARY_TRACKS / (1024 * 1024),
           LARGE_LIBRARY_TRACKS);
    /* The old layout stored five fixed char arrays a track and sized the
     * album and artist tables by the track count: 856 bytes a track, which
     * ran a 32 MB player out of memory somewhere above 4,000 tracks. */
    assert(per_track < 80);
}

int main(void)
{
    check_allocation_shape();
    check_text_pool();
    check_pool_exhaustion();
    check_display_truncation();
    check_groups_sized_independently();
    check_shrink_preserves_contents();

    crazypod_music_storage_release(&storage);
    assert(test_core_alloc_active_handles() == 0);
    assert(test_core_alloc_pin_count() == 0);
    report_footprint();
    return 0;
}
