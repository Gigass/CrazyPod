#ifndef CRAZYPOD_MUSIC_STORAGE_H
#define CRAZYPOD_MUSIC_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "crazypod_music.h"

/*
 * A track record holds only fixed-width fields; its five strings live in
 * one shared text pool and are addressed by byte offset. Storing them
 * inline cost 572 bytes a track against roughly 170 packed, which put a
 * 4,000-track library at the edge of what the audio buffer could spare.
 * The public struct crazypod_track keeps its char arrays: callers still
 * receive a copy with the strings expanded.
 */
struct crazypod_track_record {
    uint32_t path_offset;
    uint32_t title_offset;
    uint32_t artist_offset;
    uint32_t album_offset;
    uint32_t album_artist_offset;
    uint32_t duration_ms;
    uint32_t artwork_offset;
    uint32_t artwork_size;
    uint32_t source_size;
    uint32_t source_mtime;
    uint16_t year;
    uint16_t track_number;
    uint16_t disc_number;
    uint8_t format;
    uint8_t artwork_type;
    uint8_t artwork_embedded;
    uint8_t reserved[3];
};

/* Albums and artists are named by offsets into the same pool. */
struct crazypod_album_record {
    uint32_t title_offset;
    uint32_t artist_offset;
    uint32_t first_track;
    uint32_t track_count;
};

struct crazypod_music_storage {
    int tracks_handle;
    int text_handle;
    int indices_handle;
    int groups_handle;
    size_t track_capacity;
    size_t text_capacity;
    size_t text_used;
    size_t index_capacity;
    size_t album_capacity;
    size_t artist_capacity;
    struct crazypod_track_record *tracks;
    char *text;
    struct crazypod_album_record *albums;
    uint32_t *artist_names;
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
/* The text pool is sized before scanning and shrunk to fit afterwards. */
bool crazypod_music_storage_allocate_text(
    struct crazypod_music_storage *storage, size_t bytes);
void crazypod_music_storage_shrink_text(
    struct crazypod_music_storage *storage);
/* Appends a NUL-terminated string, returning its offset, or SIZE_MAX when
 * the pool is full. Offset 0 is always the empty string. */
size_t crazypod_music_storage_add_text(
    struct crazypod_music_storage *storage, const char *text,
    size_t max_length);
static inline const char *crazypod_music_storage_text(
    const struct crazypod_music_storage *storage, uint32_t offset)
{
    return storage->text != NULL && offset < storage->text_capacity
        ? storage->text + offset : "";
}
/* As above, but shortens the string to whatever room is left rather than
 * refusing. Used for display text, where losing the tail of a long album
 * name beats refusing to build the library at all. */
size_t crazypod_music_storage_add_text_truncating(
    struct crazypod_music_storage *storage, const char *text,
    size_t max_length);
/* The per-track index arrays; sized by track count. */
bool crazypod_music_storage_allocate_indices(
    struct crazypod_music_storage *storage, size_t count);
/* Album and artist tables, sized by their own counts rather than by the
 * track count -- a library has far fewer albums than tracks. */
bool crazypod_music_storage_allocate_groups(
    struct crazypod_music_storage *storage,
    size_t album_count, size_t artist_count);

#endif
