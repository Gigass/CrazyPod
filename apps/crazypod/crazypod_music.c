#include "config.h"

#include "crazypod_l10n.h"

#ifdef IPOD_6G

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "fs_attr.h"
#include "kernel.h"
#include "metadata.h"
#include "system.h"

#include "crazypod_collation.h"
#include "crazypod_music.h"
#include "crazypod_music_storage.h"
#include "crazypod_playlist.h"
#include "crazypod_state.h"

#define CRAZYPOD_SCAN_DEPTH 16
#define CRAZYPOD_PLAYLIST_PATHS 64
#define CRAZYPOD_SEARCH_CACHE_QUERY_SIZE 96
#define CRAZYPOD_STATE_DIRECTORY "/.crazypod"
#define CRAZYPOD_CACHE_DIRECTORY CRAZYPOD_STATE_DIRECTORY "/cache"
#define CRAZYPOD_MUSIC_CACHE_PATH \
    CRAZYPOD_CACHE_DIRECTORY "/music-library.bin"
#define CRAZYPOD_MUSIC_CACHE_TEMP \
    CRAZYPOD_CACHE_DIRECTORY "/music-library.tmp"
#define CRAZYPOD_MEDIA_INVALID_PATH \
    CRAZYPOD_CACHE_DIRECTORY "/media.invalid"
#define CRAZYPOD_FAVORITES_PATH \
    CRAZYPOD_STATE_DIRECTORY "/favorites.m3u8"
#define CRAZYPOD_FAVORITES_TEMP \
    CRAZYPOD_STATE_DIRECTORY "/favorites.tmp"
#define CRAZYPOD_FAVORITES_NAME CP_TR("My Favorites")
#define CRAZYPOD_MUSIC_CACHE_MAGIC 0x43504d4cu
#define CRAZYPOD_MUSIC_CACHE_VERSION 6u

struct music_source_fingerprint {
    uint32_t file_count;
    uint32_t xor_hash;
    uint32_t sum_hash;
};

struct music_cache_header {
    uint32_t magic;
    uint32_t version;
    uint32_t track_entry_size;
    uint32_t playlist_entry_size;
    uint32_t playlist_index_size;
    uint32_t track_count;
    uint32_t playlist_count;
    uint32_t playlist_track_count;
    struct music_source_fingerprint source_fingerprint;
    uint32_t checksum;
};

static struct crazypod_music_storage catalog_storage;
#define tracks catalog_storage.tracks
#define albums catalog_storage.albums
#define artists catalog_storage.artists
#define artist_first_tracks catalog_storage.artist_first_tracks
#define artist_track_counts catalog_storage.artist_track_counts
#define artist_track_indices catalog_storage.artist_track_indices
#define album_track_indices catalog_storage.album_track_indices
#define path_track_indices catalog_storage.path_track_indices
#define favorite_track_indices catalog_storage.favorite_track_indices
#define search_track_indices catalog_storage.search_track_indices
static struct crazypod_playlist playlists[CRAZYPOD_MAX_PLAYLISTS];
static uint32_t playlist_track_indices[CRAZYPOD_MAX_PLAYLIST_TRACKS];
static struct crazypod_playlist favorites_playlist;
static char playlist_paths[CRAZYPOD_PLAYLIST_PATHS][MAX_PATH];
static char search_cache_query[CRAZYPOD_SEARCH_CACHE_QUERY_SIZE];
static int track_count;
static int artist_count;
static int album_count;
static int playlist_count;
static int playlist_track_count;
static int playlist_path_count;
static int favorite_track_count;
static bool favorites_playlist_exists;
static int search_result_count;
static volatile bool scanning;
static volatile bool validating;
static volatile bool scan_abort_requested;
static volatile bool scan_suspended;
static volatile unsigned scan_generation;
static volatile bool catalog_ready;
static volatile enum crazypod_music_catalog_validation
    catalog_validation;
static volatile enum crazypod_music_scan_failure scan_failure;
static struct music_source_fingerprint catalog_fingerprint;
static struct music_source_fingerprint scan_fingerprint;
static unsigned catalog_epoch;
static unsigned search_cache_generation;
static long scan_stack[(DEFAULT_STACK_SIZE + 0x3000) / sizeof(long)];
static struct mutex catalog_mutex;

static void wait_for_scan_resume(void);
static int compare_track_order(const struct crazypod_track *left,
                               const struct crazypod_track *right);
static void load_favorites(void);

static uint32_t checksum_update(uint32_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;

    while(size-- > 0) {
        hash ^= *bytes++;
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t rotate_left32(uint32_t value, unsigned amount)
{
    amount &= 31;
    return amount == 0 ? value :
        (value << amount) | (value >> (32 - amount));
}

static void fingerprint_add(
    struct music_source_fingerprint *fingerprint,
    const char *path, off_t size, time_t mtime, uint8_t type)
{
    uint64_t source_size = size > 0 ? (uint64_t)size : 0;
    uint32_t source_mtime = mtime > 0
        ? (uint64_t)mtime > UINT32_MAX
            ? UINT32_MAX : (uint32_t)mtime
        : 0;
    uint32_t hash = 2166136261u;

    hash = checksum_update(hash, &type, sizeof(type));
    hash = checksum_update(hash, path, strlen(path) + 1);
    hash = checksum_update(
        hash, &source_size, sizeof(source_size));
    hash = checksum_update(
        hash, &source_mtime, sizeof(source_mtime));
    ++fingerprint->file_count;
    fingerprint->xor_hash ^=
        rotate_left32(hash, hash & 31);
    fingerprint->sum_hash += hash * 0x9e3779b1u;
}

static bool fingerprint_equal(
    const struct music_source_fingerprint *left,
    const struct music_source_fingerprint *right)
{
    return left->file_count == right->file_count &&
        left->xor_hash == right->xor_hash &&
        left->sum_hash == right->sum_hash;
}

static uint32_t music_cache_checksum(
    const struct music_cache_header *source)
{
    struct music_cache_header header = *source;
    uint32_t hash = 2166136261u;

    header.checksum = 0;
    hash = checksum_update(hash, &header, sizeof(header));
    hash = checksum_update(
        hash, tracks,
        (size_t)header.track_count * sizeof(tracks[0]));
    hash = checksum_update(
        hash, playlists,
        (size_t)header.playlist_count * sizeof(playlists[0]));
    return checksum_update(
        hash, playlist_track_indices,
        (size_t)header.playlist_track_count *
        sizeof(playlist_track_indices[0]));
}

static bool read_exact(int fd, void *data, size_t size)
{
    unsigned char *cursor = data;

    while(size > 0) {
        ssize_t count = read(fd, cursor, size);

        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool write_exact(int fd, const void *data, size_t size)
{
    const unsigned char *cursor = data;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);

        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool write_exact_while_scanning(
    int fd, const void *data, size_t size)
{
    const unsigned char *cursor = data;

    while(size > 0) {
        size_t chunk = size > 16384 ? 16384 : size;
        ssize_t count;

        wait_for_scan_resume();
        if(scan_abort_requested)
            return false;
        count = write(fd, cursor, chunk);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool music_cache_contents_valid(
    const struct music_cache_header *header)
{
    uint32_t i;

    for(i = 0; i < header->track_count; ++i) {
        const struct crazypod_track *track = &tracks[i];

        if(memchr(track->path, '\0', sizeof(track->path)) == NULL ||
           memchr(track->title, '\0', sizeof(track->title)) == NULL ||
           memchr(track->artist, '\0', sizeof(track->artist)) == NULL ||
           memchr(track->album, '\0', sizeof(track->album)) == NULL ||
           memchr(track->album_artist, '\0',
                  sizeof(track->album_artist)) == NULL ||
           track->path[0] != '/')
            return false;
    }
    for(i = 0; i < header->playlist_count; ++i) {
        const struct crazypod_playlist *playlist = &playlists[i];
        uint64_t end = (uint64_t)playlist->first_track +
            playlist->track_count;

        if(memchr(playlist->name, '\0',
                  sizeof(playlist->name)) == NULL ||
           end > header->playlist_track_count)
            return false;
    }
    for(i = 0; i < header->playlist_track_count; ++i) {
        if(playlist_track_indices[i] >= header->track_count)
            return false;
    }
    return true;
}

static bool media_cache_invalid(void)
{
    int fd = open(CRAZYPOD_MEDIA_INVALID_PATH, O_RDONLY);

    if(fd < 0)
        return false;
    close(fd);
    return true;
}

static void mark_media_cache_invalid(void)
{
    static const uint32_t marker = 0x43504d49u;
    int fd;

    mkdir(CRAZYPOD_STATE_DIRECTORY);
    mkdir(CRAZYPOD_CACHE_DIRECTORY);
    fd = open(CRAZYPOD_MEDIA_INVALID_PATH,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return;
    if(write_exact(fd, &marker, sizeof(marker)))
        (void)fsync(fd);
    close(fd);
}

static bool music_cache_load(void)
{
    struct music_cache_header header;
    int fd;
    bool valid;

    if(media_cache_invalid())
        return false;
    fd = open(CRAZYPOD_MUSIC_CACHE_PATH, O_RDONLY);
    if(fd < 0)
        return false;
    valid =
        read_exact(fd, &header, sizeof(header)) &&
        header.magic == CRAZYPOD_MUSIC_CACHE_MAGIC &&
        header.version == CRAZYPOD_MUSIC_CACHE_VERSION &&
        header.track_entry_size == sizeof(tracks[0]) &&
        header.playlist_entry_size == sizeof(playlists[0]) &&
        header.playlist_index_size ==
            sizeof(playlist_track_indices[0]) &&
        header.track_count <= INT_MAX &&
        header.playlist_count <= CRAZYPOD_MAX_PLAYLISTS &&
        header.playlist_track_count <= CRAZYPOD_MAX_PLAYLIST_TRACKS;
    if(valid) {
        valid = crazypod_music_storage_allocate_tracks(
                    &catalog_storage, header.track_count) &&
                crazypod_music_storage_allocate_groups(
                    &catalog_storage, header.track_count);
        if(!valid)
            scan_failure = CRAZYPOD_MUSIC_SCAN_NO_MEMORY;
    }
    if(valid) {
        valid =
            read_exact(
                fd, tracks,
                (size_t)header.track_count * sizeof(tracks[0])) &&
            read_exact(
                fd, playlists,
                (size_t)header.playlist_count *
                sizeof(playlists[0])) &&
            read_exact(
                fd, playlist_track_indices,
                (size_t)header.playlist_track_count *
                sizeof(playlist_track_indices[0]));
    }
    close(fd);
    if(!valid ||
       header.checksum != music_cache_checksum(&header) ||
       !music_cache_contents_valid(&header)) {
        crazypod_music_storage_release(&catalog_storage);
        return false;
    }

    track_count = (int)header.track_count;
    playlist_count = (int)header.playlist_count;
    playlist_track_count = (int)header.playlist_track_count;
    catalog_fingerprint = header.source_fingerprint;
    playlist_path_count = 0;
    return true;
}

static bool music_cache_save(unsigned expected_epoch)
{
    struct music_cache_header header;
    bool complete;
    int fd;

    mkdir(CRAZYPOD_STATE_DIRECTORY);
    mkdir(CRAZYPOD_CACHE_DIRECTORY);
    memset(&header, 0, sizeof(header));
    header.magic = CRAZYPOD_MUSIC_CACHE_MAGIC;
    header.version = CRAZYPOD_MUSIC_CACHE_VERSION;
    header.track_entry_size = sizeof(tracks[0]);
    header.playlist_entry_size = sizeof(playlists[0]);
    header.playlist_index_size =
        sizeof(playlist_track_indices[0]);
    header.track_count = (uint32_t)track_count;
    header.playlist_count = (uint32_t)playlist_count;
    header.playlist_track_count = (uint32_t)playlist_track_count;
    header.source_fingerprint = scan_fingerprint;
    header.checksum = music_cache_checksum(&header);

    wait_for_scan_resume();
    if(scan_abort_requested)
        return false;
    remove(CRAZYPOD_MUSIC_CACHE_TEMP);
    fd = open(CRAZYPOD_MUSIC_CACHE_TEMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    complete =
        write_exact_while_scanning(fd, &header, sizeof(header)) &&
        write_exact_while_scanning(
            fd, tracks, (size_t)track_count * sizeof(tracks[0])) &&
        write_exact_while_scanning(
            fd, playlists,
            (size_t)playlist_count * sizeof(playlists[0])) &&
        write_exact_while_scanning(
            fd, playlist_track_indices,
            (size_t)playlist_track_count *
            sizeof(playlist_track_indices[0]));
    if(complete) {
        wait_for_scan_resume();
        complete = !scan_abort_requested && fsync(fd) >= 0;
    }
    close(fd);
    mutex_lock(&catalog_mutex);
    complete = complete && !scan_abort_requested &&
        expected_epoch == catalog_epoch &&
        rename(CRAZYPOD_MUSIC_CACHE_TEMP,
               CRAZYPOD_MUSIC_CACHE_PATH) >= 0;
    if(complete)
        remove(CRAZYPOD_MEDIA_INVALID_PATH);
    mutex_unlock(&catalog_mutex);
    if(!complete) {
        remove(CRAZYPOD_MUSIC_CACHE_TEMP);
        return false;
    }
    return true;
}

static void wait_for_scan_resume(void)
{
    while(scan_suspended && !scan_abort_requested)
        sleep(HZ / 4 > 0 ? HZ / 4 : 1);
}

static int compare_text(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    while(*left != '\0' && *right != '\0') {
        a = (unsigned char)tolower((unsigned char)*left++);
        b = (unsigned char)tolower((unsigned char)*right++);
        if(a != b)
            return a < b ? -1 : 1;
    }

    if(*left == *right)
        return 0;
    return *left == '\0' ? -1 : 1;
}

static bool text_contains(const char *text, const char *query)
{
    size_t query_length;
    const char *cursor;

    if(text == NULL || query == NULL)
        return false;
    query_length = strlen(query);
    if(query_length == 0)
        return true;
    for(cursor = text; *cursor != '\0'; ++cursor) {
        size_t i;
        for(i = 0; i < query_length; ++i) {
            unsigned char left = (unsigned char)cursor[i];
            unsigned char right = (unsigned char)query[i];
            if(left == '\0' ||
               tolower(left) != tolower(right))
                break;
        }
        if(i == query_length)
            return true;
    }
    return false;
}

static bool track_matches(const struct crazypod_track *track,
                          const char *query)
{
    return track != NULL &&
           (text_contains(track->title, query) ||
            text_contains(track->artist, query) ||
            text_contains(track->album, query));
}

static void refresh_search_cache(const char *query)
{
    int i;

    if(query == NULL)
        query = "";
    if(search_cache_generation == scan_generation &&
       strcmp(search_cache_query, query) == 0)
        return;

    snprintf(search_cache_query, sizeof(search_cache_query), "%s", query);
    search_result_count = 0;
    for(i = 0; i < track_count; ++i) {
        if(track_matches(&tracks[i], query))
            search_track_indices[search_result_count++] = (uint32_t)i;
    }
    search_cache_generation = scan_generation;
}

static void copy_text(char *destination, size_t size, const char *source,
                      const char *fallback)
{
    if(source == NULL || source[0] == '\0')
        source = fallback;
    snprintf(destination, size, "%s", source != NULL ? source : "");
}

static const char *path_basename_local(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static void title_from_path(char *title, size_t size, const char *path)
{
    const char *name = path_basename_local(path);
    const char *dot = strrchr(name, '.');
    size_t length = dot != NULL ? (size_t)(dot - name) : strlen(name);

    if(length >= size)
        length = size - 1;
    memcpy(title, name, length);
    title[length] = '\0';
}

static const char *extension(const char *path)
{
    const char *dot = strrchr(path_basename_local(path), '.');
    return dot != NULL ? dot + 1 : "";
}

static bool is_playlist_file(const char *path)
{
    const char *ext = extension(path);
    return compare_text(ext, "m3u") == 0 || compare_text(ext, "m3u8") == 0;
}

static bool is_artwork_file(const char *path)
{
    const char *ext = extension(path);

    return compare_text(ext, "jpeg") == 0 ||
        compare_text(ext, "jpg") == 0 ||
        compare_text(ext, "bmp") == 0;
}

static bool should_skip_directory(const char *name)
{
    if(name[0] == '.')
        return true;
    return compare_text(name, "System Volume Information") == 0 ||
           compare_text(name, "RECYCLER") == 0;
}

static bool append_path(char *output, size_t size, const char *directory,
                        const char *name)
{
    int written;

    if(strcmp(directory, "/") == 0)
        written = snprintf(output, size, "/%s", name);
    else
        written = snprintf(output, size, "%s/%s", directory, name);
    return written > 0 && (size_t)written < size;
}

static int compare_tracks(const void *left_ptr, const void *right_ptr)
{
    const struct crazypod_track *left = left_ptr;
    const struct crazypod_track *right = right_ptr;
    int result = crazypod_collation_compare(
        left->title, right->title);

    if(result == 0)
        result = crazypod_collation_compare(
            left->artist, right->artist);
    if(result == 0)
        result = crazypod_collation_compare(
            left->album, right->album);
    if(result == 0)
        result = compare_text(left->path, right->path);
    return result;
}

static int compare_artist_track_indices(const void *left_ptr,
                                        const void *right_ptr)
{
    const struct crazypod_track *left =
        &tracks[*(const uint32_t *)left_ptr];
    const struct crazypod_track *right =
        &tracks[*(const uint32_t *)right_ptr];
    int result = crazypod_collation_compare(
        left->artist, right->artist);

    return result != 0 ? result : compare_tracks(left, right);
}

static int compare_album_track_indices(const void *left_ptr,
                                       const void *right_ptr)
{
    const struct crazypod_track *left =
        &tracks[*(const uint32_t *)left_ptr];
    const struct crazypod_track *right =
        &tracks[*(const uint32_t *)right_ptr];
    int result = crazypod_collation_compare(
        left->album, right->album);

    if(result == 0)
        result = crazypod_collation_compare(
            left->album_artist, right->album_artist);
    return result != 0 ? result : compare_track_order(left, right);
}

static int compare_path_track_indices(const void *left_ptr,
                                      const void *right_ptr)
{
    const struct crazypod_track *left =
        &tracks[*(const uint32_t *)left_ptr];
    const struct crazypod_track *right =
        &tracks[*(const uint32_t *)right_ptr];

    return compare_text(left->path, right->path);
}

static bool count_directory_tracks(
    const char *path, int depth, uint32_t *count)
{
    DIR *directory;
    struct DIRENT *entry;
    unsigned visited = 0;

    if(depth > CRAZYPOD_SCAN_DEPTH)
        return true;
    directory = opendir(path);
    if(directory == NULL)
        return true;

    while(!scan_abort_requested &&
          scan_failure == CRAZYPOD_MUSIC_SCAN_OK) {
        struct dirinfo info;
        char child[MAX_PATH];

        wait_for_scan_resume();
        if(scan_abort_requested)
            break;
        entry = readdir(directory);
        if(entry == NULL)
            break;
        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0 ||
           entry->d_name[0] == '.')
            continue;
        if(!append_path(child, sizeof(child), path, entry->d_name))
            continue;

        info = dir_get_info(directory, entry);
        if(info.attribute & ATTR_DIRECTORY) {
            if(!should_skip_directory(entry->d_name) &&
               !count_directory_tracks(child, depth + 1, count)) {
                closedir(directory);
                return false;
            }
        }
        else if(!is_playlist_file(child) &&
                !is_artwork_file(child) &&
                probe_file_format(child) != AFMT_UNKNOWN) {
            if(*count >= INT_MAX) {
                scan_failure = CRAZYPOD_MUSIC_SCAN_NO_MEMORY;
                closedir(directory);
                return false;
            }
            ++*count;
        }

        if((++visited & 15) == 0)
            yield();
    }

    closedir(directory);
    return !scan_abort_requested &&
        scan_failure == CRAZYPOD_MUSIC_SCAN_OK;
}

/* Keep the large metadata frame out of recursive scan_directory frames. */
static void NO_INLINE add_track(const char *path, off_t source_size,
                                time_t source_mtime, int format)
{
    struct mp3entry metadata;
    struct crazypod_track *track;
    int fd;

    wait_for_scan_resume();
    if(scan_abort_requested)
        return;
    if((size_t)track_count >= catalog_storage.track_capacity) {
        /* Files changed between the capacity pass and metadata pass. Never
         * publish a silently truncated library; a later scan can retry. */
        scan_failure = CRAZYPOD_MUSIC_SCAN_LIBRARY_CHANGED;
        return;
    }
    if(format == AFMT_UNKNOWN)
        return;

    fd = open(path, O_RDONLY);
    if(fd < 0)
        return;

    memset(&metadata, 0, sizeof(metadata));
    if(!get_metadata(&metadata, fd, path)) {
        close(fd);
        return;
    }
    track = &tracks[track_count++];
    memset(track, 0, sizeof(*track));
    copy_text(track->path, sizeof(track->path), path, "");
    if(metadata.title != NULL && metadata.title[0] != '\0')
        copy_text(track->title, sizeof(track->title), metadata.title, "");
    else
        title_from_path(track->title, sizeof(track->title), path);
    copy_text(track->artist, sizeof(track->artist), metadata.artist,
              CP_TR("Unknown Artist"));
    copy_text(track->album, sizeof(track->album), metadata.album,
              CP_TR("Unknown Album"));
    copy_text(track->album_artist, sizeof(track->album_artist),
              metadata.albumartist,
              track->artist[0] != '\0' ? track->artist : CP_TR("Unknown Artist"));
    track->duration_ms = metadata.length;
    track->source_size = source_size > 0
        ? (uint32_t)source_size : 0;
    track->source_mtime = source_mtime > 0
        ? (uint32_t)source_mtime : 0;
    track->year = metadata.year > 0 ? metadata.year : 0;
    track->track_number = metadata.tracknum > 0 ? metadata.tracknum : 0;
    track->disc_number = metadata.discnum > 0 ? metadata.discnum : 0;
    track->format = metadata.codectype < 256 ? metadata.codectype : 0;
    if(metadata.has_embedded_albumart) {
        track->artwork_embedded = true;
        track->artwork_offset = metadata.albumart.pos;
        track->artwork_size = metadata.albumart.size;
        track->artwork_type = metadata.albumart.type;
    }
    close(fd);
}

static void remember_playlist(const char *path)
{
    if(playlist_path_count >= CRAZYPOD_PLAYLIST_PATHS)
        return;
    copy_text(playlist_paths[playlist_path_count], MAX_PATH, path, "");
    ++playlist_path_count;
}

static void scan_directory(const char *path, int depth,
                           bool catalog_content)
{
    DIR *directory;
    struct DIRENT *entry;
    unsigned visited = 0;

    if(depth > CRAZYPOD_SCAN_DEPTH)
        return;

    directory = opendir(path);
    if(directory == NULL)
        return;

    while(!scan_abort_requested &&
          scan_failure == CRAZYPOD_MUSIC_SCAN_OK) {
        struct dirinfo info;
        char child[MAX_PATH];

        wait_for_scan_resume();
        if(scan_abort_requested)
            break;
        entry = readdir(directory);
        if(entry == NULL)
            break;
        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0)
            continue;
        if(entry->d_name[0] == '.')
            continue;
        if(!append_path(child, sizeof(child), path, entry->d_name))
            continue;

        info = dir_get_info(directory, entry);
        if(info.attribute & ATTR_DIRECTORY) {
            if(!should_skip_directory(entry->d_name))
                scan_directory(
                    child, depth + 1, catalog_content);
        }
        else if(catalog_content && is_playlist_file(child)) {
            fingerprint_add(
                &scan_fingerprint, child,
                info.size, info.mtime, 2);
            remember_playlist(child);
        }
        else if(is_artwork_file(child))
            fingerprint_add(
                &scan_fingerprint, child,
                info.size, info.mtime, 3);
        else if(catalog_content) {
            int format = probe_file_format(child);

            if(format != AFMT_UNKNOWN) {
                fingerprint_add(
                    &scan_fingerprint, child,
                    info.size, info.mtime, 1);
                add_track(
                    child, info.size, info.mtime, format);
            }
        }

        if(scan_abort_requested ||
           scan_failure != CRAZYPOD_MUSIC_SCAN_OK)
            break;
        if((++visited & 15) == 0)
            yield();
    }

    closedir(directory);
}

static bool validate_directory(
    const char *path, int depth,
    struct music_source_fingerprint *fingerprint,
    bool catalog_content)
{
    DIR *directory;
    struct DIRENT *entry;
    unsigned visited = 0;

    if(depth > CRAZYPOD_SCAN_DEPTH)
        return true;
    directory = opendir(path);
    if(directory == NULL)
        return true;

    while(!scan_abort_requested) {
        struct dirinfo info;
        char child[MAX_PATH];

        wait_for_scan_resume();
        if(scan_abort_requested)
            break;
        entry = readdir(directory);
        if(entry == NULL)
            break;
        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0 ||
           entry->d_name[0] == '.')
            continue;
        if(!append_path(child, sizeof(child), path, entry->d_name))
            continue;

        info = dir_get_info(directory, entry);
        if(info.attribute & ATTR_DIRECTORY) {
            if(!should_skip_directory(entry->d_name) &&
               !validate_directory(
                   child, depth + 1, fingerprint,
                   catalog_content)) {
                closedir(directory);
                return false;
            }
        }
        else if(catalog_content && is_playlist_file(child))
            fingerprint_add(
                fingerprint, child, info.size, info.mtime, 2);
        else if(is_artwork_file(child))
            fingerprint_add(
                fingerprint, child, info.size, info.mtime, 3);
        else if(catalog_content &&
                probe_file_format(child) != AFMT_UNKNOWN)
            fingerprint_add(
                fingerprint, child, info.size, info.mtime, 1);

        if((++visited & 15) == 0)
            yield();
    }

    closedir(directory);
    return !scan_abort_requested;
}

static void build_groups(void)
{
    int i;

    for(i = 0; i < track_count; ++i) {
        artist_track_indices[i] = (uint32_t)i;
        album_track_indices[i] = (uint32_t)i;
        path_track_indices[i] = (uint32_t)i;
    }

    if(track_count > 1) {
        qsort(artist_track_indices, track_count,
              sizeof(artist_track_indices[0]),
              compare_artist_track_indices);
        qsort(album_track_indices, track_count,
              sizeof(album_track_indices[0]),
              compare_album_track_indices);
        qsort(path_track_indices, track_count,
              sizeof(path_track_indices[0]),
              compare_path_track_indices);
    }

    artist_count = 0;
    for(i = 0; i < track_count; ++i) {
        const struct crazypod_track *track =
            &tracks[artist_track_indices[i]];

        if(artist_count == 0 ||
           compare_text(artists[artist_count - 1],
                        track->artist) != 0) {
            copy_text(artists[artist_count],
                      sizeof(artists[artist_count]),
                      track->artist, CP_TR("Unknown Artist"));
            artist_first_tracks[artist_count] = (uint32_t)i;
            artist_track_counts[artist_count] = 0;
            ++artist_count;
        }
        ++artist_track_counts[artist_count - 1];
    }

    album_count = 0;
    for(i = 0; i < track_count; ++i) {
        const struct crazypod_track *track =
            &tracks[album_track_indices[i]];
        struct crazypod_album *album =
            album_count > 0 ? &albums[album_count - 1] : NULL;

        if(album == NULL ||
           compare_text(album->title, track->album) != 0 ||
           compare_text(album->artist,
                        track->album_artist) != 0) {
            album = &albums[album_count++];
            copy_text(album->title, sizeof(album->title),
                      track->album, CP_TR("Unknown Album"));
            copy_text(album->artist, sizeof(album->artist),
                      track->album_artist, CP_TR("Unknown Artist"));
            album->first_track = (uint32_t)i;
            album->track_count = 0;
        }
        ++album->track_count;
    }
}

static int find_track_by_path(const char *path)
{
    int low = 0;
    int high = track_count - 1;

    while(low <= high) {
        int middle = low + (high - low) / 2;
        int track_index = path_track_indices[middle];
        int result = compare_text(tracks[track_index].path, path);

        if(result == 0)
            return track_index;
        if(result < 0)
            low = middle + 1;
        else
            high = middle - 1;
    }
    return -1;
}

static void normalize_playlist_path(char *output, size_t size,
                                    const char *playlist_path,
                                    const char *entry)
{
    char combined[MAX_PATH];
    int component_starts[64];
    int component_count = 0;
    size_t read_index = 0;
    size_t write_index = 1;
    const char *slash;
    size_t directory_length;
    size_t i;

    while(*entry == ' ' || *entry == '\t')
        ++entry;

    if(entry[0] == '/' ||
       (isalpha((unsigned char)entry[0]) && entry[1] == ':')) {
        const char *absolute = entry[0] == '/' ? entry : entry + 2;
        size_t absolute_length;
        if(absolute[0] == '/')
            ++absolute;
        absolute_length = strlen(absolute);
        if(absolute_length > sizeof(combined) - 2)
            absolute_length = sizeof(combined) - 2;
        combined[0] = '/';
        memcpy(combined + 1, absolute, absolute_length);
        combined[absolute_length + 1] = '\0';
    }
    else
    {
        size_t entry_length;
        slash = strrchr(playlist_path, '/');
        directory_length = slash != NULL ? (size_t)(slash - playlist_path) : 0;
        if(directory_length >= sizeof(combined) - 1)
            directory_length = sizeof(combined) - 2;
        if(directory_length > 0)
            memcpy(combined, playlist_path, directory_length);
        else
            combined[0] = '/';
        if(directory_length == 0)
            directory_length = 1;
        combined[directory_length++] = '/';
        entry_length = strlen(entry);
        if(entry_length > sizeof(combined) - directory_length - 1)
            entry_length = sizeof(combined) - directory_length - 1;
        memcpy(combined + directory_length, entry, entry_length);
        combined[directory_length + entry_length] = '\0';
    }

    for(i = 0; combined[i] != '\0'; ++i) {
        if(combined[i] == '\\')
            combined[i] = '/';
    }

    if(size == 0)
        return;
    output[0] = '/';
    output[1 < size ? 1 : 0] = '\0';

    while(combined[read_index] != '\0') {
        size_t start;
        size_t length;

        while(combined[read_index] == '/')
            ++read_index;
        if(combined[read_index] == '\0')
            break;
        start = read_index;
        while(combined[read_index] != '\0' &&
              combined[read_index] != '/')
            ++read_index;
        length = read_index - start;

        if(length == 1 && combined[start] == '.')
            continue;
        if(length == 2 && combined[start] == '.' &&
           combined[start + 1] == '.') {
            if(component_count > 0) {
                write_index = (size_t)component_starts[--component_count];
                output[write_index] = '\0';
            }
            continue;
        }
        if(component_count >= (int)(sizeof(component_starts) /
                                    sizeof(component_starts[0])))
            break;
        if(write_index > 1 && write_index + 1 < size)
            output[write_index++] = '/';
        component_starts[component_count++] = (int)(write_index > 1
            ? write_index - 1 : write_index);
        if(write_index + length >= size)
            length = size - write_index - 1;
        memcpy(output + write_index, combined + start, length);
        write_index += length;
        output[write_index] = '\0';
    }
}

static int read_playlist_line(int fd, char *line, size_t size)
{
    size_t length = 0;
    char character;
    int result;

    while((result = read(fd, &character, 1)) == 1) {
        if(character == '\n')
            break;
        if(character == '\r')
            continue;
        if(length + 1 < size)
            line[length++] = character;
    }
    line[length] = '\0';
    return result == 1 || length > 0 ? (int)length : -1;
}

static void refresh_favorites_playlist(void)
{
    copy_text(favorites_playlist.name,
              sizeof(favorites_playlist.name),
              CRAZYPOD_FAVORITES_NAME, "");
    favorites_playlist.first_track = 0;
    favorites_playlist.track_count =
        (uint32_t)favorite_track_count;
}

static int favorite_position(int track_index)
{
    int position;

    for(position = 0;
        position < favorite_track_count;
        ++position) {
        if(favorite_track_indices[position] == (uint32_t)track_index)
            return position;
    }
    return -1;
}

static bool save_favorites(void)
{
    static const char header[] = "#EXTM3U\n";
    bool complete;
    int fd;
    int position;

    mkdir(CRAZYPOD_STATE_DIRECTORY);
    remove(CRAZYPOD_FAVORITES_TEMP);
    fd = open(CRAZYPOD_FAVORITES_TEMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;

    complete = write_exact(fd, header, sizeof(header) - 1);
    for(position = 0;
        complete && position < favorite_track_count;
        ++position) {
        const struct crazypod_track *track =
            &tracks[favorite_track_indices[position]];
        size_t length = strlen(track->path);

        complete =
            write_exact(fd, track->path, length) &&
            write_exact(fd, "\n", 1);
    }
    if(complete)
        complete = fsync(fd) >= 0;
    close(fd);
    if(!complete ||
       rename(CRAZYPOD_FAVORITES_TEMP,
              CRAZYPOD_FAVORITES_PATH) < 0) {
        remove(CRAZYPOD_FAVORITES_TEMP);
        return false;
    }
    return true;
}

static void load_favorites(void)
{
    char line[MAX_PATH];
    int fd;

    favorite_track_count = 0;
    favorites_playlist_exists = false;
    refresh_favorites_playlist();
    fd = open(CRAZYPOD_FAVORITES_PATH, O_RDONLY);
    if(fd < 0)
        return;
    favorites_playlist_exists = true;

    while(!scan_abort_requested &&
          favorite_track_count < track_count &&
          read_playlist_line(fd, line, sizeof(line)) >= 0) {
        int track_index;
        size_t length;

        wait_for_scan_resume();
        if(scan_abort_requested)
            break;
        if((unsigned char)line[0] == 0xef &&
           (unsigned char)line[1] == 0xbb &&
           (unsigned char)line[2] == 0xbf)
            memmove(line, line + 3, strlen(line + 3) + 1);
        length = strlen(line);
        while(length > 0 &&
              (line[length - 1] == ' ' ||
               line[length - 1] == '\t'))
            line[--length] = '\0';
        if(line[0] != '/')
            continue;
        track_index = find_track_by_path(line);
        if(track_index >= 0 &&
           favorite_position(track_index) < 0)
            favorite_track_indices[favorite_track_count++] =
                (uint32_t)track_index;
    }
    close(fd);
    refresh_favorites_playlist();
}

static void parse_playlists(void)
{
    int path_index;

    playlist_count = 0;
    playlist_track_count = 0;
    for(path_index = 0;
        !scan_abort_requested &&
        path_index < playlist_path_count &&
        playlist_count < CRAZYPOD_MAX_PLAYLISTS;
        ++path_index) {
        struct crazypod_playlist *playlist = &playlists[playlist_count];
        char line[MAX_PATH];
        int fd;

        wait_for_scan_resume();
        if(scan_abort_requested)
            break;
        fd = open(playlist_paths[path_index], O_RDONLY);
        if(fd < 0)
            continue;

        memset(playlist, 0, sizeof(*playlist));
        title_from_path(playlist->name, sizeof(playlist->name),
                        playlist_paths[path_index]);
        playlist->first_track = playlist_track_count;

        while(!scan_abort_requested) {
            char resolved[MAX_PATH];
            int track_index;
            size_t length;

            wait_for_scan_resume();
            if(scan_abort_requested ||
               read_playlist_line(fd, line, sizeof(line)) < 0)
                break;
            if((unsigned char)line[0] == 0xef &&
               (unsigned char)line[1] == 0xbb &&
               (unsigned char)line[2] == 0xbf)
                memmove(line, line + 3, strlen(line + 3) + 1);
            if(line[0] == '\0' || line[0] == '#')
                continue;
            length = strlen(line);
            while(length > 0 &&
                  (line[length - 1] == ' ' || line[length - 1] == '\t'))
                line[--length] = '\0';

            normalize_playlist_path(resolved, sizeof(resolved),
                                    playlist_paths[path_index], line);
            track_index = find_track_by_path(resolved);
            if(track_index >= 0 &&
               playlist_track_count < CRAZYPOD_MAX_PLAYLIST_TRACKS) {
                playlist_track_indices[playlist_track_count++] = track_index;
                ++playlist->track_count;
            }
        }

        close(fd);
        if(playlist->track_count > 0)
            ++playlist_count;
    }
}

static int compare_track_order(const struct crazypod_track *left,
                               const struct crazypod_track *right)
{
    if(left->disc_number != right->disc_number)
        return left->disc_number < right->disc_number ? -1 : 1;
    if(left->track_number != right->track_number)
        return left->track_number < right->track_number ? -1 : 1;
    {
        int result = compare_text(left->title, right->title);
        return result != 0 ? result : compare_text(left->path, right->path);
    }
}

void crazypod_music_init(void)
{
    mutex_init(&catalog_mutex);
    crazypod_music_storage_release(&catalog_storage);
    crazypod_music_storage_init(&catalog_storage);
    track_count = 0;
    artist_count = 0;
    album_count = 0;
    playlist_count = 0;
    playlist_track_count = 0;
    playlist_path_count = 0;
    favorite_track_count = 0;
    favorites_playlist_exists = false;
    refresh_favorites_playlist();
    scanning = false;
    validating = false;
    scan_abort_requested = false;
    scan_suspended = false;
    scan_generation = 0;
    catalog_epoch = 1;
    scan_failure = CRAZYPOD_MUSIC_SCAN_OK;
    memset(&catalog_fingerprint, 0,
           sizeof(catalog_fingerprint));
    memset(&scan_fingerprint, 0,
           sizeof(scan_fingerprint));
    catalog_ready = music_cache_load();
    catalog_validation =
        catalog_ready
            ? CRAZYPOD_MUSIC_VALIDATION_UNCHECKED
            : CRAZYPOD_MUSIC_VALIDATION_FAILED;
    if(catalog_ready) {
        build_groups();
        load_favorites();
    }
    search_cache_generation = (unsigned)-1;
    search_cache_query[0] = '\0';
    search_result_count = 0;
}

void crazypod_music_scan(void)
{
    uint32_t candidate_count = 0;
    unsigned build_epoch;
    bool include_ipod_music =
        crazypod_state_read_ipod_music();
    bool published = false;

    scanning = true;
    mutex_lock(&catalog_mutex);
    build_epoch = ++catalog_epoch;
    catalog_ready = false;
    scan_failure = CRAZYPOD_MUSIC_SCAN_OK;
    crazypod_music_storage_release(&catalog_storage);
    track_count = 0;
    artist_count = 0;
    album_count = 0;
    playlist_count = 0;
    playlist_track_count = 0;
    playlist_path_count = 0;
    favorite_track_count = 0;
    favorites_playlist_exists = false;
    refresh_favorites_playlist();
    memset(&scan_fingerprint, 0,
           sizeof(scan_fingerprint));
    mutex_unlock(&catalog_mutex);

    if(!count_directory_tracks("/Music", 0, &candidate_count) ||
       (!scan_abort_requested && include_ipod_music &&
        !count_directory_tracks(
            "/iPod_Control/Music", 0, &candidate_count)) ||
       (!scan_abort_requested &&
        !count_directory_tracks("/Podcasts", 0, &candidate_count))) {
        if(!scan_abort_requested &&
           scan_failure == CRAZYPOD_MUSIC_SCAN_OK)
            scan_failure = CRAZYPOD_MUSIC_SCAN_LIBRARY_CHANGED;
    }
    if(!scan_abort_requested &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK &&
       !crazypod_music_storage_allocate_tracks(
           &catalog_storage, candidate_count))
        scan_failure = CRAZYPOD_MUSIC_SCAN_NO_MEMORY;

    if(!scan_abort_requested &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK)
        scan_directory("/Music", 0, true);
    if(!scan_abort_requested && include_ipod_music &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK)
        scan_directory("/iPod_Control/Music", 0, true);
    if(!scan_abort_requested &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK)
        scan_directory("/Podcasts", 0, true);
    if(!scan_abort_requested &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK)
        scan_directory(ROCKBOX_DIR "/albumart", 0, false);
    if(!scan_abort_requested &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK) {
        crazypod_music_storage_shrink_tracks(
            &catalog_storage, (size_t)track_count);
        if(!crazypod_music_storage_allocate_groups(
               &catalog_storage, (size_t)track_count))
            scan_failure = CRAZYPOD_MUSIC_SCAN_NO_MEMORY;
    }
    if(!scan_abort_requested &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK) {
        wait_for_scan_resume();
        if(!scan_abort_requested && track_count > 1)
            qsort(tracks, track_count, sizeof(tracks[0]), compare_tracks);
    }
    if(!scan_abort_requested &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK) {
        wait_for_scan_resume();
        if(!scan_abort_requested)
            build_groups();
    }
    if(!scan_abort_requested &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK) {
        wait_for_scan_resume();
        if(!scan_abort_requested) {
            parse_playlists();
            load_favorites();
        }
    }
    if(!scan_abort_requested &&
       scan_failure == CRAZYPOD_MUSIC_SCAN_OK) {
        wait_for_scan_resume();
        if(!scan_abort_requested) {
            (void)music_cache_save(build_epoch);
            if(!scan_abort_requested) {
                mutex_lock(&catalog_mutex);
                if(!scan_abort_requested && build_epoch == catalog_epoch) {
                    catalog_fingerprint = scan_fingerprint;
                    ++scan_generation;
                    catalog_ready = true;
                    catalog_validation =
                        CRAZYPOD_MUSIC_VALIDATION_CURRENT;
                    published = true;
                }
                else if(!scan_abort_requested)
                    scan_failure =
                        CRAZYPOD_MUSIC_SCAN_LIBRARY_CHANGED;
                mutex_unlock(&catalog_mutex);
            }
        }
    }
    if(!published && (scan_abort_requested ||
       scan_failure != CRAZYPOD_MUSIC_SCAN_OK)) {
        mutex_lock(&catalog_mutex);
        crazypod_music_storage_release(&catalog_storage);
        track_count = 0;
        artist_count = 0;
        album_count = 0;
        playlist_count = 0;
        playlist_track_count = 0;
        playlist_path_count = 0;
        favorite_track_count = 0;
        favorites_playlist_exists = false;
        refresh_favorites_playlist();
        catalog_ready = false;
        catalog_validation =
            CRAZYPOD_MUSIC_VALIDATION_FAILED;
        ++scan_generation;
        mutex_unlock(&catalog_mutex);
    }
    mutex_lock(&catalog_mutex);
    scanning = false;
    mutex_unlock(&catalog_mutex);
}

static void scan_thread(void)
{
    crazypod_music_scan();
}

static void validation_thread(void)
{
    struct music_source_fingerprint fingerprint;
    struct music_source_fingerprint expected_fingerprint;
    unsigned expected_epoch;
    bool include_ipod_music =
        crazypod_state_read_ipod_music();
    bool complete;

    mutex_lock(&catalog_mutex);
    expected_fingerprint = catalog_fingerprint;
    expected_epoch = catalog_epoch;
    mutex_unlock(&catalog_mutex);
    memset(&fingerprint, 0, sizeof(fingerprint));
    complete = validate_directory(
        "/Music", 0, &fingerprint, true) &&
        (!include_ipod_music || validate_directory(
            "/iPod_Control/Music", 0, &fingerprint, true)) &&
        validate_directory(
            "/Podcasts", 0, &fingerprint, true) &&
        validate_directory(
            ROCKBOX_DIR "/albumart", 0, &fingerprint, false);
    mutex_lock(&catalog_mutex);
    if(expected_epoch == catalog_epoch && catalog_ready) {
        if(scan_abort_requested)
            catalog_validation =
                CRAZYPOD_MUSIC_VALIDATION_UNCHECKED;
        else if(!complete)
            catalog_validation =
                CRAZYPOD_MUSIC_VALIDATION_FAILED;
        else
            catalog_validation = fingerprint_equal(
                &fingerprint, &expected_fingerprint)
                    ? CRAZYPOD_MUSIC_VALIDATION_CURRENT
                    : CRAZYPOD_MUSIC_VALIDATION_STALE;
    }
    validating = false;
    mutex_unlock(&catalog_mutex);
}

bool crazypod_music_scan_async(void)
{
    unsigned int id;

    mutex_lock(&catalog_mutex);
    if(scanning || validating) {
        mutex_unlock(&catalog_mutex);
        return false;
    }
    scan_abort_requested = false;
    scanning = true;
    mutex_unlock(&catalog_mutex);
    id = create_thread(scan_thread, scan_stack, sizeof(scan_stack), 0,
                       "crazypod scan"
                       IF_PRIO(, PRIORITY_BACKGROUND)
                       IF_COP(, CPU));
    if(id == 0) {
        mutex_lock(&catalog_mutex);
        scanning = false;
        mutex_unlock(&catalog_mutex);
        return false;
    }
    return true;
}

bool crazypod_music_validate_catalog_async(void)
{
    unsigned int id;

    mutex_lock(&catalog_mutex);
    if(!catalog_ready ||
       catalog_validation !=
           CRAZYPOD_MUSIC_VALIDATION_UNCHECKED ||
       scanning || validating) {
        mutex_unlock(&catalog_mutex);
        return false;
    }
    scan_abort_requested = false;
    validating = true;
    catalog_validation =
        CRAZYPOD_MUSIC_VALIDATION_RUNNING;
    mutex_unlock(&catalog_mutex);
    id = create_thread(
        validation_thread, scan_stack, sizeof(scan_stack), 0,
        "crazypod validate"
        IF_PRIO(, PRIORITY_BACKGROUND)
        IF_COP(, CPU));
    if(id == 0) {
        mutex_lock(&catalog_mutex);
        validating = false;
        catalog_validation =
            CRAZYPOD_MUSIC_VALIDATION_FAILED;
        mutex_unlock(&catalog_mutex);
        return false;
    }
    return true;
}

void crazypod_music_require_catalog_validation(void)
{
    mutex_lock(&catalog_mutex);
    catalog_validation = catalog_ready
        ? CRAZYPOD_MUSIC_VALIDATION_UNCHECKED
        : CRAZYPOD_MUSIC_VALIDATION_FAILED;
    mutex_unlock(&catalog_mutex);
}

enum crazypod_music_catalog_validation
crazypod_music_catalog_validation(void)
{
    enum crazypod_music_catalog_validation validation;

    mutex_lock(&catalog_mutex);
    validation = catalog_validation;
    mutex_unlock(&catalog_mutex);
    return validation;
}

bool crazypod_music_take_catalog_stale(void)
{
    bool stale;

    mutex_lock(&catalog_mutex);
    if(catalog_validation !=
       CRAZYPOD_MUSIC_VALIDATION_STALE)
        stale = false;
    else {
        catalog_validation =
            CRAZYPOD_MUSIC_VALIDATION_FAILED;
        stale = true;
    }
    mutex_unlock(&catalog_mutex);
    return stale;
}

void crazypod_music_cancel_scan(void)
{
    if(!scanning && !validating)
        return;

    scan_abort_requested = true;
    while(scanning || validating)
        yield();
}

bool crazypod_music_is_scanning(void)
{
    return scanning;
}

unsigned crazypod_music_scan_generation(void)
{
    return scan_generation;
}

bool crazypod_music_catalog_ready(void)
{
    bool ready;

    mutex_lock(&catalog_mutex);
    ready = catalog_ready;
    mutex_unlock(&catalog_mutex);
    return ready;
}

enum crazypod_music_scan_failure
crazypod_music_scan_failure_reason(void)
{
    return scan_failure;
}

void crazypod_music_invalidate_catalog(void)
{
    mutex_lock(&catalog_mutex);
    ++catalog_epoch;
    catalog_ready = false;
    catalog_validation =
        CRAZYPOD_MUSIC_VALIDATION_FAILED;
    mutex_unlock(&catalog_mutex);
    mark_media_cache_invalid();
    remove(CRAZYPOD_MUSIC_CACHE_TEMP);
    remove(CRAZYPOD_MUSIC_CACHE_PATH);
}

void crazypod_music_set_scan_suspended(bool suspended)
{
    scan_suspended = suspended;
}

int crazypod_music_track_count(void)
{
    int count;

    mutex_lock(&catalog_mutex);
    count = catalog_ready ? track_count : 0;
    mutex_unlock(&catalog_mutex);
    return count;
}

bool crazypod_music_copy_track(int index, struct crazypod_track *track)
{
    bool copied = false;

    if(track == NULL)
        return false;
    mutex_lock(&catalog_mutex);
    if(catalog_ready && index >= 0 && index < track_count) {
        *track = tracks[index];
        copied = true;
    }
    mutex_unlock(&catalog_mutex);
    return copied;
}

int crazypod_music_find_track(const char *path)
{
    int index;

    mutex_lock(&catalog_mutex);
    index = catalog_ready && path != NULL
        ? find_track_by_path(path) : -1;
    mutex_unlock(&catalog_mutex);
    return index;
}

int crazypod_music_artist_count(void)
{
    int count;

    mutex_lock(&catalog_mutex);
    count = catalog_ready ? artist_count : 0;
    mutex_unlock(&catalog_mutex);
    return count;
}

bool crazypod_music_copy_artist(int index, char *artist, size_t size)
{
    bool copied = false;

    if(artist == NULL || size == 0)
        return false;
    artist[0] = '\0';
    mutex_lock(&catalog_mutex);
    if(catalog_ready && index >= 0 && index < artist_count) {
        snprintf(artist, size, "%s", artists[index]);
        copied = true;
    }
    mutex_unlock(&catalog_mutex);
    return copied;
}

int crazypod_music_artist_track_count(int artist_index)
{
    int count = 0;

    mutex_lock(&catalog_mutex);
    if(catalog_ready && artist_index >= 0 &&
       artist_index < artist_count)
        count = artist_track_counts[artist_index];
    mutex_unlock(&catalog_mutex);
    return count;
}

bool crazypod_music_copy_artist_track(int artist_index, int track_index,
                                      struct crazypod_track *track)
{
    int pool_index;
    bool copied = false;

    if(track == NULL)
        return false;
    mutex_lock(&catalog_mutex);
    if(catalog_ready && artist_index >= 0 &&
       artist_index < artist_count && track_index >= 0 &&
       (uint32_t)track_index < artist_track_counts[artist_index]) {
        pool_index = artist_first_tracks[artist_index] + track_index;
        *track = tracks[artist_track_indices[pool_index]];
        copied = true;
    }
    mutex_unlock(&catalog_mutex);
    return copied;
}

int crazypod_music_album_count(void)
{
    int count;

    mutex_lock(&catalog_mutex);
    count = catalog_ready ? album_count : 0;
    mutex_unlock(&catalog_mutex);
    return count;
}

bool crazypod_music_copy_album(int index, struct crazypod_album *album)
{
    bool copied = false;

    if(album == NULL)
        return false;
    mutex_lock(&catalog_mutex);
    if(catalog_ready && index >= 0 && index < album_count) {
        *album = albums[index];
        copied = true;
    }
    mutex_unlock(&catalog_mutex);
    return copied;
}

int crazypod_music_album_track_count(int album_index)
{
    int count = 0;

    mutex_lock(&catalog_mutex);
    if(catalog_ready && album_index >= 0 && album_index < album_count &&
       albums[album_index].track_count <= INT_MAX)
        count = (int)albums[album_index].track_count;
    mutex_unlock(&catalog_mutex);
    return count;
}

bool crazypod_music_copy_album_track(int album_index, int track_index,
                                     struct crazypod_track *track)
{
    const struct crazypod_album *album;
    int pool_index;
    bool copied = false;

    if(track == NULL)
        return false;
    mutex_lock(&catalog_mutex);
    if(catalog_ready && album_index >= 0 && album_index < album_count) {
        album = &albums[album_index];
        if(track_index >= 0 &&
           (uint32_t)track_index < album->track_count) {
            pool_index = album->first_track + track_index;
            *track = tracks[album_track_indices[pool_index]];
            copied = true;
        }
    }
    mutex_unlock(&catalog_mutex);
    return copied;
}

int crazypod_music_playlist_count(void)
{
    int count;

    mutex_lock(&catalog_mutex);
    count = catalog_ready
        ? playlist_count + (favorites_playlist_exists ? 1 : 0) : 0;
    mutex_unlock(&catalog_mutex);
    return count;
}

bool crazypod_music_copy_playlist(int index,
                                  struct crazypod_playlist *playlist)
{
    bool copied = false;

    if(playlist == NULL)
        return false;
    mutex_lock(&catalog_mutex);
    if(catalog_ready && index >= 0) {
        if(index < playlist_count) {
            *playlist = playlists[index];
            copied = true;
        }
        else if(favorites_playlist_exists && index == playlist_count) {
            *playlist = favorites_playlist;
            copied = true;
        }
    }
    mutex_unlock(&catalog_mutex);
    return copied;
}

bool crazypod_music_copy_playlist_track(int playlist_index, int track_index,
                                        struct crazypod_track *track)
{
    const struct crazypod_playlist *playlist = NULL;
    int pool_index;
    int library_index = -1;
    bool copied = false;

    if(track == NULL)
        return false;
    mutex_lock(&catalog_mutex);
    if(catalog_ready && playlist_index >= 0) {
        if(playlist_index < playlist_count)
            playlist = &playlists[playlist_index];
        else if(favorites_playlist_exists &&
                playlist_index == playlist_count)
            playlist = &favorites_playlist;
        if(playlist != NULL && track_index >= 0 &&
           (uint32_t)track_index < playlist->track_count) {
            if(favorites_playlist_exists &&
               playlist_index == playlist_count)
                library_index = favorite_track_indices[track_index];
            else {
                pool_index = playlist->first_track + track_index;
                library_index = playlist_track_indices[pool_index];
            }
            if(library_index >= 0 && library_index < track_count) {
                *track = tracks[library_index];
                copied = true;
            }
        }
    }
    mutex_unlock(&catalog_mutex);
    return copied;
}

bool crazypod_music_track_is_favorite(const char *path)
{
    int track_index;
    bool favorite;

    mutex_lock(&catalog_mutex);
    track_index = catalog_ready && path != NULL
        ? find_track_by_path(path) : -1;
    favorite = track_index >= 0 && favorite_position(track_index) >= 0;
    mutex_unlock(&catalog_mutex);
    return favorite;
}

bool crazypod_music_toggle_favorite(const char *path)
{
    int track_index;
    int position;
    bool existed;
    bool changed = false;

    mutex_lock(&catalog_mutex);
    track_index = catalog_ready && path != NULL
        ? find_track_by_path(path) : -1;
    existed = favorites_playlist_exists;
    if(track_index < 0)
        goto out;
    position = favorite_position(track_index);
    if(position < 0) {
        if(favorite_track_count >= track_count)
            goto out;
        favorite_track_indices[favorite_track_count++] =
            (uint32_t)track_index;
        refresh_favorites_playlist();
        if(!save_favorites()) {
            --favorite_track_count;
            favorites_playlist_exists = existed;
            refresh_favorites_playlist();
            goto out;
        }
        favorites_playlist_exists = true;
    }
    else {
        int next;

        for(next = position;
            next + 1 < favorite_track_count;
            ++next)
            favorite_track_indices[next] =
                favorite_track_indices[next + 1];
        --favorite_track_count;
        refresh_favorites_playlist();
        if(!save_favorites()) {
            for(next = favorite_track_count;
                next > position;
                --next)
                favorite_track_indices[next] =
                    favorite_track_indices[next - 1];
            favorite_track_indices[position] =
                (uint32_t)track_index;
            favorites_playlist_exists = existed;
            ++favorite_track_count;
            refresh_favorites_playlist();
            goto out;
        }
    }
    refresh_favorites_playlist();
    changed = true;
out:
    mutex_unlock(&catalog_mutex);
    return changed;
}

int crazypod_music_search_count(const char *query)
{
    int count = 0;

    mutex_lock(&catalog_mutex);
    if(catalog_ready) {
        refresh_search_cache(query);
        count = search_result_count;
    }
    mutex_unlock(&catalog_mutex);
    return count;
}

bool crazypod_music_copy_search_track(const char *query, int result_index,
                                      struct crazypod_track *track)
{
    bool copied = false;

    if(track == NULL)
        return false;
    mutex_lock(&catalog_mutex);
    if(catalog_ready) {
        refresh_search_cache(query);
        if(result_index >= 0 && result_index < search_result_count) {
            *track = tracks[search_track_indices[result_index]];
            copied = true;
        }
    }
    mutex_unlock(&catalog_mutex);
    return copied;
}

bool crazypod_music_play_track(int library_index)
{
    struct crazypod_track track;
    const char *path[1];

    if(!crazypod_music_copy_track(library_index, &track))
        return false;
    path[0] = track.path;
    return crazypod_queue_replace(path, 1, 0);
}

static const char **allocate_queue_paths(int count, int *handle,
                                         char **path_storage)
{
    const char **paths;
    size_t pointer_bytes;
    size_t path_bytes;
    size_t bytes;
    int i;

    *handle = 0;
    *path_storage = NULL;
    if(count <= 0 ||
       (size_t)count > SIZE_MAX / sizeof(const char *) ||
       (size_t)count > SIZE_MAX / MAX_PATH)
        return NULL;
    pointer_bytes = (size_t)count * sizeof(const char *);
    path_bytes = (size_t)count * MAX_PATH;
    if(pointer_bytes > SIZE_MAX - path_bytes)
        return NULL;
    bytes = pointer_bytes + path_bytes;
    *handle = core_alloc(bytes);
    if(*handle <= 0)
        return NULL;
    core_pin(*handle);
    paths = core_get_data(*handle);
    *path_storage = (char *)paths + pointer_bytes;
    for(i = 0; i < count; ++i)
        paths[i] = *path_storage + (size_t)i * MAX_PATH;
    return paths;
}

static void release_queue_paths(int handle)
{
    if(handle > 0) {
        core_unpin(handle);
        core_free(handle);
    }
}

bool crazypod_music_play_search(const char *query, int selected_index)
{
    const char **queue_paths;
    char *path_storage;
    int queue_handle;
    int count = 0;
    int start = -1;
    int i;
    bool queued;

    mutex_lock(&catalog_mutex);
    if(!catalog_ready) {
        mutex_unlock(&catalog_mutex);
        return false;
    }
    refresh_search_cache(query);
    queue_paths = allocate_queue_paths(
        search_result_count, &queue_handle, &path_storage);
    if(queue_paths == NULL) {
        mutex_unlock(&catalog_mutex);
        return false;
    }
    for(i = 0; i < search_result_count; ++i) {
        int track_index = search_track_indices[i];

        if(i == selected_index)
            start = count;
        snprintf(path_storage + (size_t)count * MAX_PATH, MAX_PATH,
                 "%s", tracks[track_index].path);
        ++count;
    }
    mutex_unlock(&catalog_mutex);
    if(start < 0)
        start = 0;
    queued = crazypod_queue_replace(queue_paths, count, start);
    release_queue_paths(queue_handle);
    return queued;
}

bool crazypod_music_play(enum crazypod_music_scope scope, int group_index,
                         int selected_index)
{
    const char **queue_paths;
    char *path_storage;
    char selected_path[MAX_PATH];
    const struct crazypod_playlist *playlist = NULL;
    const struct crazypod_album *album = NULL;
    int queue_handle;
    int requested_count;
    int count = 0;
    int start = -1;
    int i;
    bool queued;

    selected_path[0] = '\0';
    mutex_lock(&catalog_mutex);
    if(!catalog_ready) {
        mutex_unlock(&catalog_mutex);
        return false;
    }
    if(scope == CRAZYPOD_SCOPE_PLAYLIST) {
        if(group_index >= 0 && group_index < playlist_count)
            playlist = &playlists[group_index];
        else if(favorites_playlist_exists && group_index == playlist_count)
            playlist = &favorites_playlist;
        requested_count = playlist != NULL && playlist->track_count <= INT_MAX
            ? (int)playlist->track_count : 0;
    }
    else if(scope == CRAZYPOD_SCOPE_ALBUM) {
        if(group_index >= 0 && group_index < album_count)
            album = &albums[group_index];
        requested_count = album != NULL && album->track_count <= INT_MAX
            ? (int)album->track_count : 0;
    }
    else if(scope == CRAZYPOD_SCOPE_ARTIST) {
        requested_count = group_index >= 0 && group_index < artist_count &&
            artist_track_counts[group_index] <= INT_MAX
                ? (int)artist_track_counts[group_index] : 0;
    }
    else
        requested_count = track_count;
    queue_paths = allocate_queue_paths(
        requested_count, &queue_handle, &path_storage);
    if(queue_paths == NULL) {
        mutex_unlock(&catalog_mutex);
        return false;
    }

    if(scope == CRAZYPOD_SCOPE_PLAYLIST) {
        if(playlist == NULL) {
            mutex_unlock(&catalog_mutex);
            release_queue_paths(queue_handle);
            return false;
        }
        for(i = 0; i < (int)playlist->track_count; ++i) {
            int library_index;

            if(favorites_playlist_exists && group_index == playlist_count)
                library_index = favorite_track_indices[i];
            else
                library_index = playlist_track_indices[
                    playlist->first_track + i];
            if(library_index < 0 || library_index >= track_count)
                continue;
            if(i == selected_index)
                start = count;
            snprintf(path_storage + (size_t)count * MAX_PATH, MAX_PATH,
                     "%s", tracks[library_index].path);
            ++count;
        }
    }
    else if(scope == CRAZYPOD_SCOPE_ALBUM) {
        for(i = 0; album != NULL && i < (int)album->track_count; ++i) {
            int library_index = album_track_indices[album->first_track + i];

            if(library_index < 0 || library_index >= track_count)
                continue;
            snprintf(path_storage + (size_t)count * MAX_PATH, MAX_PATH,
                     "%s", tracks[library_index].path);
            if(i == selected_index)
                snprintf(selected_path, sizeof(selected_path), "%s",
                         tracks[library_index].path);
            ++count;
        }
        if(selected_path[0] != '\0') {
            for(i = 0; i < count; ++i) {
                if(strcmp(queue_paths[i], selected_path) == 0) {
                    start = i;
                    break;
                }
            }
        }
    }
    else {
        const char *artist = scope == CRAZYPOD_SCOPE_ARTIST &&
            group_index >= 0 && group_index < artist_count
                ? artists[group_index] : NULL;
        int visible_index = 0;

        for(i = 0; i < track_count; ++i) {
            bool include = scope == CRAZYPOD_SCOPE_ALL;
            if(scope == CRAZYPOD_SCOPE_ARTIST && artist != NULL)
                include = compare_text(tracks[i].artist, artist) == 0;

            if(include) {
                if(visible_index == selected_index)
                    start = count;
                snprintf(path_storage + (size_t)count * MAX_PATH,
                         MAX_PATH, "%s", tracks[i].path);
                ++count;
                ++visible_index;
            }
        }
    }
    mutex_unlock(&catalog_mutex);

    if(count <= 0) {
        release_queue_paths(queue_handle);
        return false;
    }
    if(start < 0)
        start = 0;
    queued = crazypod_queue_replace(queue_paths, count, start);
    release_queue_paths(queue_handle);
    return queued;
}

bool crazypod_music_shuffle_all(unsigned int seed)
{
    const char **queue_paths;
    char *path_storage;
    int queue_handle;
    int count;
    int i;
    bool queued;

    mutex_lock(&catalog_mutex);
    count = catalog_ready ? track_count : 0;
    if(count <= 0) {
        mutex_unlock(&catalog_mutex);
        return false;
    }
    queue_paths = allocate_queue_paths(count, &queue_handle, &path_storage);
    if(queue_paths == NULL) {
        mutex_unlock(&catalog_mutex);
        return false;
    }
    for(i = 0; i < count; ++i)
        snprintf(path_storage + (size_t)i * MAX_PATH, MAX_PATH,
                 "%s", tracks[i].path);
    mutex_unlock(&catalog_mutex);
    queued = crazypod_queue_replace_shuffled(
        queue_paths, count, seed);
    release_queue_paths(queue_handle);
    return queued;
}

#endif
