#ifndef CRAZYPOD_MUSIC_H
#define CRAZYPOD_MUSIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "file.h"

#define CRAZYPOD_MAX_TRACKS 2048
#define CRAZYPOD_MAX_PLAYLISTS 64
#define CRAZYPOD_MAX_PLAYLIST_TRACKS 8192

struct crazypod_track {
    char path[MAX_PATH];
    char title[96];
    char artist[72];
    char album[72];
    char album_artist[72];
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
    bool artwork_embedded;
};

struct crazypod_playlist {
    char name[72];
    uint16_t first_track;
    uint16_t track_count;
};

struct crazypod_album {
    char title[72];
    char artist[72];
    uint16_t first_track;
    uint16_t track_count;
};

enum crazypod_music_scope {
    CRAZYPOD_SCOPE_ALL,
    CRAZYPOD_SCOPE_ARTIST,
    CRAZYPOD_SCOPE_ALBUM,
    CRAZYPOD_SCOPE_PLAYLIST,
};

enum crazypod_music_catalog_validation {
    CRAZYPOD_MUSIC_VALIDATION_UNCHECKED,
    CRAZYPOD_MUSIC_VALIDATION_RUNNING,
    CRAZYPOD_MUSIC_VALIDATION_CURRENT,
    CRAZYPOD_MUSIC_VALIDATION_STALE,
    CRAZYPOD_MUSIC_VALIDATION_FAILED,
};

void crazypod_music_init(void);
void crazypod_music_scan(void);
bool crazypod_music_scan_async(void);
bool crazypod_music_validate_catalog_async(void);
void crazypod_music_require_catalog_validation(void);
enum crazypod_music_catalog_validation
crazypod_music_catalog_validation(void);
bool crazypod_music_take_catalog_stale(void);
void crazypod_music_cancel_scan(void);
bool crazypod_music_is_scanning(void);
unsigned crazypod_music_scan_generation(void);
bool crazypod_music_catalog_ready(void);
void crazypod_music_invalidate_catalog(void);
void crazypod_music_set_scan_suspended(bool suspended);

int crazypod_music_track_count(void);
const struct crazypod_track *crazypod_music_track(int index);
int crazypod_music_find_track(const char *path);
int crazypod_music_artist_count(void);
const char *crazypod_music_artist(int index);
int crazypod_music_artist_track_count(int artist_index);
const struct crazypod_track *crazypod_music_artist_track(int artist_index,
                                                          int track_index);
int crazypod_music_album_count(void);
const struct crazypod_album *crazypod_music_album(int index);
int crazypod_music_album_track_count(int album_index);
const struct crazypod_track *crazypod_music_album_track(int album_index,
                                                         int track_index);
int crazypod_music_playlist_count(void);
const struct crazypod_playlist *crazypod_music_playlist(int index);
const struct crazypod_track *crazypod_music_playlist_track(int playlist_index,
                                                            int track_index);
bool crazypod_music_track_is_favorite(const char *path);
bool crazypod_music_toggle_favorite(const char *path);
int crazypod_music_search_count(const char *query);
const struct crazypod_track *crazypod_music_search_track(const char *query,
                                                          int result_index);

bool crazypod_music_play(enum crazypod_music_scope scope, int group_index,
                         int selected_index);
bool crazypod_music_play_track(int library_index);
bool crazypod_music_play_search(const char *query, int selected_index);

#endif
