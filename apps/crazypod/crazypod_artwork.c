#include "config.h"

#include "crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "albumart.h"
#include "bmp.h"
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "jpeg_load.h"
#include "kernel.h"
#include "metadata.h"
#include "misc.h"
#include "powermgmt.h"
#include "string-extra.h"

#include "crazypod_artwork.h"
#include "crazypod_image.h"

#define CRAZYPOD_ARTWORK_MAX_SIZE CRAZYPOD_NOW_ARTWORK_MAX_SIZE
#define CRAZYPOD_ARTWORK_BANKS 2
#define CRAZYPOD_ARTWORK_PIXELS \
    (CRAZYPOD_ARTWORK_MAX_SIZE * CRAZYPOD_ARTWORK_MAX_SIZE)
#define CRAZYPOD_DECODE_BUFFER_SIZE \
    (CRAZYPOD_ARTWORK_PIXELS * sizeof(fb_data) + 64 * 1024)
#define CRAZYPOD_ARTWORK_WAKE 1
#define CRAZYPOD_ARTWORK_DEFAULT_PRIORITY 100
#define CRAZYPOD_CACHE_MAGIC 0x43504632u
#define CRAZYPOD_CACHE_VERSION 10
#define CRAZYPOD_DIRECTORY "/.crazypod"
#define CRAZYPOD_CACHE_DIRECTORY CRAZYPOD_DIRECTORY "/cache"
#define CRAZYPOD_COVER_CACHE_DIRECTORY CRAZYPOD_CACHE_DIRECTORY "/CV10"
#define CRAZYPOD_COVER_CACHE_SHARDS 16
#define CRAZYPOD_ARTWORK_READY_PATH CRAZYPOD_CACHE_DIRECTORY "/artwork.ready"
#define CRAZYPOD_ARTWORK_READY_TMP CRAZYPOD_CACHE_DIRECTORY "/artwork.rtmp"
#define CRAZYPOD_ARTWORK_VALIDATE_PATH \
    CRAZYPOD_CACHE_DIRECTORY "/artwork.validate"
#define CRAZYPOD_ARTWORK_PROGRESS_PATH \
    CRAZYPOD_CACHE_DIRECTORY "/artwork.progress"
#define CRAZYPOD_ARTWORK_PROGRESS_TMP \
    CRAZYPOD_CACHE_DIRECTORY "/artwork.ptmp"
#define CRAZYPOD_MEDIA_INVALID_PATH CRAZYPOD_CACHE_DIRECTORY "/media.invalid"
#define CRAZYPOD_MUSIC_CATALOG_PATH CRAZYPOD_CACHE_DIRECTORY "/music-library.bin"
#define CRAZYPOD_MUSIC_CATALOG_TMP CRAZYPOD_CACHE_DIRECTORY "/music-library.tmp"
#define CRAZYPOD_PHOTO_CATALOG_PATH CRAZYPOD_CACHE_DIRECTORY "/photo-catalog.bin"
#define CRAZYPOD_PHOTO_CATALOG_TMP CRAZYPOD_CACHE_DIRECTORY "/photo-catalog.tmp"
#define CRAZYPOD_PHOTO_THUMB_CACHE CRAZYPOD_CACHE_DIRECTORY "/photos.thm"
#define CRAZYPOD_PHOTO_VIEW_CACHE CRAZYPOD_CACHE_DIRECTORY "/photos.view"
#define CRAZYPOD_VIDEO_CATALOG_PATH CRAZYPOD_CACHE_DIRECTORY "/video-catalog.bin"
#define CRAZYPOD_VIDEO_CATALOG_TMP CRAZYPOD_CACHE_DIRECTORY "/video-catalog.tmp"
#define CRAZYPOD_ARTWORK_READY_MAGIC 0x43504152u
#define CRAZYPOD_ARTWORK_READY_VERSION 1u
#define CRAZYPOD_ARTWORK_PROGRESS_MAGIC 0x43504150u
#define CRAZYPOD_ARTWORK_PROGRESS_VERSION 1u
#define CRAZYPOD_ARTWORK_PROGRESS_INTERVAL 64
#define CRAZYPOD_ARTWORK_CANDIDATE_COUNT 40
#define CRAZYPOD_ARTWORK_DIRECTORY_IMAGES 48
#define CRAZYPOD_ARTWORK_DIRECTORY_NAME 128
#define CRAZYPOD_ARTWORK_SOURCE_NONE 0
#define CRAZYPOD_ARTWORK_SOURCE_EXTERNAL 1
#define CRAZYPOD_ARTWORK_SOURCE_EMBEDDED 2
#define CRAZYPOD_ARTWORK_SCOPE_TRACK 0
#define CRAZYPOD_ARTWORK_SCOPE_GLOBAL 1
#define CRAZYPOD_ARTWORK_SCOPE_PARENT 2

struct artwork_slot {
    fb_data *pixels[CRAZYPOD_ARTWORK_BANKS];
    lv_image_dsc_t descriptor[CRAZYPOD_ARTWORK_BANKS];
    char requested_path[MAX_PATH];
    char requested_album[72];
    char requested_album_artist[72];
    char requested_artist[72];
    char decoded_path[MAX_PATH];
    int requested_size;
    int requested_priority;
    int decoded_size;
    int capacity_size;
    uint32_t requested_artwork_offset;
    uint32_t requested_artwork_size;
    uint32_t requested_source_size;
    uint32_t requested_source_mtime;
    unsigned request_serial;
    unsigned decoded_serial;
    unsigned publish_generation;
    unsigned requested_cache_generation;
    unsigned decoded_cache_generation;
    int active_bank;
    uint8_t requested_artwork_type;
    bool requested_artwork_embedded;
    bool requested_cache_only;
    bool requested_direct_source;
    bool decoded_cache_only;
    bool decoded_direct_source;
    bool decoded_cache_miss;
    bool valid;
    bool failed;
};

struct artwork_decode_request {
    char track_path[MAX_PATH];
    char album[72];
    char album_artist[72];
    char artist[72];
    int target_size;
    uint32_t offset;
    uint32_t size;
    uint32_t source_size;
    uint32_t source_mtime;
    uint8_t type;
    unsigned cache_generation;
    bool embedded;
    bool cache_only;
    bool direct_source;
};

struct artwork_source {
    char path[MAX_PATH];
    uint32_t file_size;
    uint32_t file_mtime;
    uint32_t offset;
    uint32_t size;
    uint8_t type;
    uint8_t kind;
};

struct artwork_cache_header {
    uint32_t magic;
    uint32_t version;
    uint32_t key_a;
    uint32_t key_b;
    uint32_t source_key_a;
    uint32_t source_key_b;
    uint32_t data_size;
    uint16_t requested_size;
    uint16_t width;
    uint16_t height;
    uint16_t reserved;
};

struct artwork_ready_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t album_count;
    uint32_t library_key;
    uint32_t checksum;
};

struct artwork_progress_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t album_count;
    uint32_t library_key;
    uint32_t next_index;
    uint32_t checksum;
};

struct artwork_candidate {
    char path[MAX_PATH];
    uint8_t scope;
    bool sized;
};

struct artwork_directory_entry {
    char name[CRAZYPOD_ARTWORK_DIRECTORY_NAME];
    uint32_t size;
    uint32_t mtime;
};

struct artwork_directory_cache {
    char path[MAX_PATH];
    struct artwork_directory_entry
        entries[CRAZYPOD_ARTWORK_DIRECTORY_IMAGES];
    int count;
    bool valid;
};

enum artwork_cache_result {
    ARTWORK_CACHE_MISS,
    ARTWORK_CACHE_IMAGE,
    ARTWORK_CACHE_EMPTY,
    ARTWORK_CACHE_ERROR,
};

static struct artwork_slot artwork_slots[CRAZYPOD_ARTWORK_SLOTS];
static fb_data coverflow_pixels[CRAZYPOD_COVERFLOW_ARTWORK_SLOTS]
    [CRAZYPOD_ARTWORK_BANKS][CRAZYPOD_COVERFLOW_ARTWORK_SIZE *
                             CRAZYPOD_COVERFLOW_ARTWORK_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data preview_pixels[CRAZYPOD_ARTWORK_BANKS]
    [CRAZYPOD_PREVIEW_ARTWORK_SIZE * CRAZYPOD_PREVIEW_ARTWORK_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data now_pixels[3][CRAZYPOD_ARTWORK_BANKS]
    [CRAZYPOD_NOW_ARTWORK_MAX_SIZE * CRAZYPOD_NOW_ARTWORK_MAX_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data capsule_pixels[CRAZYPOD_ARTWORK_BANKS]
    [CRAZYPOD_CAPSULE_ARTWORK_SIZE * CRAZYPOD_CAPSULE_ARTWORK_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static unsigned char decode_buffer[CRAZYPOD_DECODE_BUFFER_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
/* Product surfaces scale from the canonical 128px disk entry here. */
static fb_data canonical_scratch[
    CRAZYPOD_ARTWORK_CACHE_SIZE * CRAZYPOD_ARTWORK_CACHE_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static struct mutex artwork_mutex;
static struct event_queue artwork_queue;
static long artwork_stack[(DEFAULT_STACK_SIZE + 0x5000) / sizeof(long)];
static volatile unsigned artwork_publish_generation;
static volatile bool artwork_worker_decoding;
static bool artwork_suspended;
static bool artwork_storage_suspended;
static bool artwork_lock_suspended;
static bool artwork_wake_queued;
static bool artwork_prime_active;
static bool artwork_prime_processing;
static bool artwork_prime_failed;
static int artwork_prime_next;
static int artwork_prime_completed;
static int artwork_prime_total;
static int artwork_prime_persisted;
static int artwork_prime_order_handle;
static uint32_t *artwork_prime_order;
static struct artwork_candidate
    artwork_candidates[CRAZYPOD_ARTWORK_CANDIDATE_COUNT];
static struct artwork_directory_cache artwork_track_directory;
static struct artwork_directory_cache artwork_parent_directory;
static volatile unsigned artwork_cache_generation;
static bool artwork_validation_required;

static const char *file_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot != NULL ? dot + 1 : "";
}

static bool is_jpeg(const char *path)
{
    const char *extension = file_extension(path);
    return strcasecmp(extension, "jpg") == 0 ||
           strcasecmp(extension, "jpeg") == 0;
}

static bool is_supported_artwork(const char *path)
{
    return is_jpeg(path) ||
        strcasecmp(file_extension(path), "bmp") == 0;
}

static uint32_t hash_bytes(uint32_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;

    while(size-- > 0) {
        hash ^= *bytes++;
        hash *= 16777619u;
    }
    return hash;
}

static void artwork_cache_keys(const struct artwork_decode_request *request,
                               uint32_t *key_a, uint32_t *key_b)
{
    const char *slash = strrchr(request->track_path, '/');
    size_t directory_length = slash != NULL
        ? (size_t)(slash - request->track_path) : 0;
    uint32_t a = 2166136261u;
    uint32_t b = 0x9e3779b9u;

    a = hash_bytes(a, request->track_path, directory_length);
    a = hash_bytes(a, request->album, strlen(request->album));
    a = hash_bytes(a, request->album_artist,
                   strlen(request->album_artist));

    b = hash_bytes(b, request->album_artist,
                   strlen(request->album_artist));
    b = hash_bytes(b, request->album, strlen(request->album));
    b = hash_bytes(b, request->track_path, directory_length);
    *key_a = a;
    *key_b = b;
}

static void artwork_cache_path(const struct artwork_decode_request *request,
                               char *path, size_t path_size,
                               uint32_t *key_a, uint32_t *key_b,
                               bool create_directory)
{
    char directory[MAX_PATH];
    unsigned shard;

    artwork_cache_keys(request, key_a, key_b);
    shard = (*key_a >> 28) & 0x0f;
    snprintf(directory, sizeof(directory), "%s/%X",
             CRAZYPOD_COVER_CACHE_DIRECTORY, shard);
    if(create_directory)
        mkdir(directory);
    /* Keep FAT directory entries in 8.3 form while retaining 44 key bits. */
    snprintf(path, path_size, "%s/%X/%08lX.%03lX",
             CRAZYPOD_COVER_CACHE_DIRECTORY, shard,
             (unsigned long)*key_a,
             (unsigned long)(*key_b & 0x0fffu));
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


static bool artwork_media_cache_invalid(void)
{
    int fd = open(CRAZYPOD_MEDIA_INVALID_PATH, O_RDONLY);

    if(fd < 0)
        return false;
    close(fd);
    return true;
}

static uint32_t clamp_u32(uint64_t value)
{
    return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value;
}

static void artwork_candidate_add(const char *base, bool extensions,
                                  uint8_t scope, bool sized, int *count)
{
    static const char * const suffixes[] = { "jpeg", "jpg", "bmp" };
    int suffix;

    if(!extensions) {
        if(*count < CRAZYPOD_ARTWORK_CANDIDATE_COUNT)
            snprintf(artwork_candidates[*count].path,
                     MAX_PATH, "%s", base);
        if(*count < CRAZYPOD_ARTWORK_CANDIDATE_COUNT) {
            artwork_candidates[*count].scope = scope;
            artwork_candidates[(*count)++].sized = sized;
        }
        return;
    }
    for(suffix = 0; suffix < 3 &&
                    *count < CRAZYPOD_ARTWORK_CANDIDATE_COUNT;
        ++suffix) {
        snprintf(artwork_candidates[*count].path, MAX_PATH,
                 "%s%s", base, suffixes[suffix]);
        artwork_candidates[*count].scope = scope;
        artwork_candidates[(*count)++].sized = sized;
    }
}

static bool source_file_info(const char *path, uint32_t *size,
                             uint32_t *mtime)
{
    char directory_path[MAX_PATH];
    const char *slash = strrchr(path, '/');
    const char *name;
    DIR *directory;
    struct DIRENT *entry;
    size_t length;

    if(slash == NULL)
        return false;
    name = slash + 1;
    length = slash == path ? 1 : (size_t)(slash - path);
    if(length >= sizeof(directory_path))
        return false;
    memcpy(directory_path, path, length);
    directory_path[length] = '\0';
    directory = opendir(directory_path);
    if(directory == NULL)
        return false;
    while((entry = readdir(directory)) != NULL) {
        struct dirinfo info;

        if(strcasecmp(entry->d_name, name) != 0)
            continue;
        info = dir_get_info(directory, entry);
        if((info.attribute & ATTR_DIRECTORY) == 0) {
            *size = info.size > 0
                ? clamp_u32((uint64_t)info.size) : 0;
            *mtime = info.mtime > 0
                ? clamp_u32((uint64_t)info.mtime) : 0;
            closedir(directory);
            return true;
        }
        break;
    }
    closedir(directory);
    return false;
}

static void artwork_albumart_pass(
    const struct artwork_decode_request *request, const char *directory,
    const char *parent, const char *size, int *count)
{
    char base[MAX_PATH];
    const char *artist = request->album_artist[0] != '\0'
        ? request->album_artist : request->artist;
    size_t directory_length = strlen(directory);
    size_t album_length = strlen(request->album);

    strip_extension(base, sizeof(base) - strlen(size) - 5,
                    request->track_path);
    strcat(base, size);
    strcat(base, ".");
    artwork_candidate_add(base, true, CRAZYPOD_ARTWORK_SCOPE_TRACK,
                          size[0] != '\0', count);

    if(album_length > 0) {
        snprintf(base, sizeof(base), "%s%s%s.",
                 directory, request->album, size);
        fix_path_part(base, directory_length, album_length);
        artwork_candidate_add(base, true,
                              CRAZYPOD_ARTWORK_SCOPE_TRACK,
                              size[0] != '\0', count);
    }
    snprintf(base, sizeof(base), "%scover%s.", directory, size);
    artwork_candidate_add(base, true, CRAZYPOD_ARTWORK_SCOPE_TRACK,
                          size[0] != '\0', count);
    if(size[0] == '\0') {
        snprintf(base, sizeof(base), "%sfolder.jpg", directory);
        artwork_candidate_add(base, false,
                              CRAZYPOD_ARTWORK_SCOPE_TRACK,
                              false, count);
    }
    if(artist[0] != '\0' && album_length > 0) {
        snprintf(base, sizeof(base),
                 ROCKBOX_DIR "/albumart/%s-%s%s.",
                 artist, request->album, size);
        fix_path_part(base, strlen(ROCKBOX_DIR "/albumart/"),
                      MAX_PATH);
        artwork_candidate_add(base, true,
                              CRAZYPOD_ARTWORK_SCOPE_GLOBAL,
                              size[0] != '\0', count);
    }
    if(parent[0] != '\0' && album_length > 0) {
        snprintf(base, sizeof(base), "%s%s%s.",
                 parent, request->album, size);
        fix_path_part(base, strlen(parent), album_length);
        artwork_candidate_add(base, true,
                              CRAZYPOD_ARTWORK_SCOPE_PARENT,
                              size[0] != '\0', count);
    }
    if(parent[0] != '\0') {
        snprintf(base, sizeof(base), "%scover%s.", parent, size);
        artwork_candidate_add(base, true,
                              CRAZYPOD_ARTWORK_SCOPE_PARENT,
                              size[0] != '\0', count);
    }
}

static void artwork_consider_directory_entry(
    const char *name, uint32_t size, uint32_t mtime,
    uint8_t scope, int candidate_count,
    int *best_sized, uint32_t *sized_size, uint32_t *sized_mtime,
    int *best_generic, uint32_t *generic_size, uint32_t *generic_mtime)
{
    int candidate_index;

    for(candidate_index = 0;
        candidate_index < candidate_count; ++candidate_index) {
        const struct artwork_candidate *candidate =
            &artwork_candidates[candidate_index];
        const char *candidate_name;

        if(candidate->scope != scope)
            continue;
        candidate_name = strrchr(candidate->path, '/');
        if(candidate_name == NULL ||
           strcasecmp(candidate_name + 1, name) != 0)
            continue;
        if(candidate->sized) {
            if(candidate_index < *best_sized) {
                *best_sized = candidate_index;
                *sized_size = size;
                *sized_mtime = mtime;
            }
        }
        else if(candidate_index < *best_generic) {
            *best_generic = candidate_index;
            *generic_size = size;
            *generic_mtime = mtime;
        }
        break;
    }
}

static void artwork_scan_scope(
    const char *directory_path, uint8_t scope, int candidate_count,
    int *best_sized, uint32_t *sized_size, uint32_t *sized_mtime,
    int *best_generic, uint32_t *generic_size, uint32_t *generic_mtime)
{
    struct artwork_directory_cache *cache =
        scope == CRAZYPOD_ARTWORK_SCOPE_TRACK
            ? &artwork_track_directory
            : &artwork_parent_directory;
    DIR *directory;
    struct DIRENT *entry;
    bool overflow = false;
    int entry_index;

    if(cache->valid &&
       strcmp(cache->path, directory_path) == 0) {
        for(entry_index = 0;
            entry_index < cache->count; ++entry_index) {
            const struct artwork_directory_entry *cached =
                &cache->entries[entry_index];

            artwork_consider_directory_entry(
                cached->name, cached->size, cached->mtime,
                scope, candidate_count,
                best_sized, sized_size, sized_mtime,
                best_generic, generic_size, generic_mtime);
        }
        return;
    }

    cache->valid = false;
    cache->count = 0;
    cache->path[0] = '\0';
    directory = opendir(directory_path);
    if(directory == NULL)
        return;
    while((entry = readdir(directory)) != NULL) {
        struct dirinfo info;
        uint32_t entry_size;
        uint32_t entry_mtime;
        size_t name_length;

        if(!is_supported_artwork(entry->d_name))
            continue;
        info = dir_get_info(directory, entry);
        if((info.attribute & ATTR_DIRECTORY) != 0)
            continue;
        entry_size = info.size > 0
            ? clamp_u32((uint64_t)info.size) : 0;
        entry_mtime = info.mtime > 0
            ? clamp_u32((uint64_t)info.mtime) : 0;
        artwork_consider_directory_entry(
            entry->d_name, entry_size, entry_mtime,
            scope, candidate_count,
            best_sized, sized_size, sized_mtime,
            best_generic, generic_size, generic_mtime);
        name_length = strlen(entry->d_name);
        if(cache->count >= CRAZYPOD_ARTWORK_DIRECTORY_IMAGES ||
           name_length >= CRAZYPOD_ARTWORK_DIRECTORY_NAME) {
            overflow = true;
            continue;
        }
        memcpy(cache->entries[cache->count].name,
               entry->d_name, name_length + 1);
        cache->entries[cache->count].size = entry_size;
        cache->entries[cache->count].mtime = entry_mtime;
        ++cache->count;
    }
    closedir(directory);
    if(!overflow) {
        snprintf(cache->path, sizeof(cache->path), "%s",
                 directory_path);
        cache->valid = true;
    }
}

static bool resolve_external_artwork(
    const struct artwork_decode_request *request,
    char *path, uint32_t *size, uint32_t *mtime)
{
    char track_directory[MAX_PATH];
    char parent_directory[MAX_PATH];
    char sized[16];
    const char *slash = strrchr(request->track_path, '/');
    int candidate_count = 0;
    int track_sized = CRAZYPOD_ARTWORK_CANDIDATE_COUNT;
    int track_generic = CRAZYPOD_ARTWORK_CANDIDATE_COUNT;
    int parent_sized = CRAZYPOD_ARTWORK_CANDIDATE_COUNT;
    int parent_generic = CRAZYPOD_ARTWORK_CANDIDATE_COUNT;
    uint32_t track_sized_size = 0;
    uint32_t track_sized_mtime = 0;
    uint32_t track_generic_size = 0;
    uint32_t track_generic_mtime = 0;
    uint32_t parent_sized_size = 0;
    uint32_t parent_sized_mtime = 0;
    uint32_t parent_generic_size = 0;
    uint32_t parent_generic_mtime = 0;
    int lookup_size;
    int candidate_index;

    if(slash == NULL)
        return false;
    {
        size_t length = (size_t)(slash - request->track_path + 1);

        if(length >= sizeof(track_directory))
            return false;
        memcpy(track_directory, request->track_path, length);
        track_directory[length] = '\0';
    }
    snprintf(parent_directory, sizeof(parent_directory), "%s",
             track_directory);
    if(strcmp(parent_directory, "/") == 0)
        parent_directory[0] = '\0';
    else if(strlen(parent_directory) > 1) {
        char *end = parent_directory + strlen(parent_directory) - 1;
        char *parent_slash;

        *end = '\0';
        parent_slash = strrchr(parent_directory, '/');
        if(parent_slash != NULL)
            parent_slash[1] = '\0';
        else
            parent_directory[0] = '\0';
    }
    lookup_size = request->direct_source
        ? request->target_size : CRAZYPOD_ARTWORK_CACHE_SIZE;
    snprintf(sized, sizeof(sized), ".%dx%d",
             lookup_size, lookup_size);
    artwork_albumart_pass(request, track_directory,
                          parent_directory, sized, &candidate_count);
    artwork_albumart_pass(request, track_directory,
                          parent_directory, "", &candidate_count);

    artwork_scan_scope(
        track_directory, CRAZYPOD_ARTWORK_SCOPE_TRACK,
        candidate_count, &track_sized,
        &track_sized_size, &track_sized_mtime,
        &track_generic, &track_generic_size,
        &track_generic_mtime);
    if(track_sized < candidate_count) {
        candidate_index = track_sized;
        *size = track_sized_size;
        *mtime = track_sized_mtime;
        goto found;
    }

    for(candidate_index = 0;
        candidate_index < candidate_count; ++candidate_index) {
        const struct artwork_candidate *candidate =
            &artwork_candidates[candidate_index];

        if(candidate->scope != CRAZYPOD_ARTWORK_SCOPE_GLOBAL ||
           !candidate->sized)
            continue;
        if(file_exists(candidate->path) &&
           source_file_info(candidate->path, size, mtime))
            goto found;
    }

    if(parent_directory[0] != '\0') {
        artwork_scan_scope(
            parent_directory, CRAZYPOD_ARTWORK_SCOPE_PARENT,
            candidate_count, &parent_sized,
            &parent_sized_size, &parent_sized_mtime,
            &parent_generic, &parent_generic_size,
            &parent_generic_mtime);
        if(parent_sized < candidate_count) {
            candidate_index = parent_sized;
            *size = parent_sized_size;
            *mtime = parent_sized_mtime;
            goto found;
        }
    }
    if(track_generic < candidate_count) {
        candidate_index = track_generic;
        *size = track_generic_size;
        *mtime = track_generic_mtime;
        goto found;
    }

    for(candidate_index = 0;
        candidate_index < candidate_count; ++candidate_index) {
        const struct artwork_candidate *candidate =
            &artwork_candidates[candidate_index];

        if(candidate->scope != CRAZYPOD_ARTWORK_SCOPE_GLOBAL ||
           candidate->sized)
            continue;
        if(file_exists(candidate->path) &&
           source_file_info(candidate->path, size, mtime))
            goto found;
    }
    if(parent_generic < candidate_count) {
        candidate_index = parent_generic;
        *size = parent_generic_size;
        *mtime = parent_generic_mtime;
        goto found;
    }
    return false;

found:
    snprintf(path, MAX_PATH, "%s",
             artwork_candidates[candidate_index].path);
    return true;
}

static void artwork_source_keys(const struct artwork_source *source,
                                uint32_t *key_a, uint32_t *key_b)
{
    uint32_t a = 2166136261u;
    uint32_t b = 0x9e3779b9u;

    a = hash_bytes(a, &source->kind, sizeof(source->kind));
    a = hash_bytes(a, source->path, strlen(source->path));
    a = hash_bytes(a, &source->file_size, sizeof(source->file_size));
    a = hash_bytes(a, &source->file_mtime, sizeof(source->file_mtime));
    a = hash_bytes(a, &source->offset, sizeof(source->offset));
    a = hash_bytes(a, &source->size, sizeof(source->size));
    a = hash_bytes(a, &source->type, sizeof(source->type));

    b = hash_bytes(b, &source->type, sizeof(source->type));
    b = hash_bytes(b, &source->size, sizeof(source->size));
    b = hash_bytes(b, &source->offset, sizeof(source->offset));
    b = hash_bytes(b, &source->file_mtime, sizeof(source->file_mtime));
    b = hash_bytes(b, &source->file_size, sizeof(source->file_size));
    b = hash_bytes(b, source->path, strlen(source->path));
    b = hash_bytes(b, &source->kind, sizeof(source->kind));
    *key_a = a;
    *key_b = b;
}

static void resolve_artwork_source(
    const struct artwork_decode_request *request,
    struct artwork_source *source)
{
    char discovered_path[MAX_PATH];

    memset(source, 0, sizeof(*source));
    if(resolve_external_artwork(
           request, discovered_path, &source->file_size,
           &source->file_mtime)) {
        source->kind = CRAZYPOD_ARTWORK_SOURCE_EXTERNAL;
        source->type = is_jpeg(discovered_path)
            ? AA_TYPE_JPG : AA_TYPE_BMP;
        snprintf(source->path, sizeof(source->path), "%s",
                 discovered_path);
        return;
    }
    if(request->embedded) {
        source->kind = CRAZYPOD_ARTWORK_SOURCE_EMBEDDED;
        source->type = request->type;
        source->file_size = request->source_size;
        source->file_mtime = request->source_mtime;
        source->offset = request->offset;
        source->size = request->size;
        snprintf(source->path, sizeof(source->path), "%s",
                 request->track_path);
    }
}

static enum artwork_cache_result artwork_cache_load(
    const struct artwork_decode_request *request,
    const struct artwork_source *source, bool validate_source,
    fb_data *pixels, lv_image_dsc_t *descriptor)
{
    struct artwork_cache_header header;
    char path[MAX_PATH];
    uint32_t key_a;
    uint32_t key_b;
    uint32_t source_key_a = 0;
    uint32_t source_key_b = 0;
    fb_data *source_pixels;
    int output_width;
    int output_height;
    int fd;

    if(request->direct_source)
        return ARTWORK_CACHE_MISS;
    artwork_cache_path(request, path, sizeof(path), &key_a, &key_b,
                       false);
    if(validate_source) {
        if(source == NULL)
            return ARTWORK_CACHE_MISS;
        artwork_source_keys(source, &source_key_a, &source_key_b);
    }
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return ARTWORK_CACHE_MISS;
    if(!read_exact(fd, &header, sizeof(header)) ||
       header.magic != CRAZYPOD_CACHE_MAGIC ||
       header.version != CRAZYPOD_CACHE_VERSION ||
       header.key_a != key_a || header.key_b != key_b ||
       header.requested_size != CRAZYPOD_ARTWORK_CACHE_SIZE ||
       (validate_source &&
        (header.source_key_a != source_key_a ||
         header.source_key_b != source_key_b))) {
        close(fd);
        return ARTWORK_CACHE_MISS;
    }
    if(header.width == 0 || header.height == 0 ||
       header.data_size == 0) {
        close(fd);
        return header.reserved != 0
            ? ARTWORK_CACHE_ERROR : ARTWORK_CACHE_EMPTY;
    }
    if(header.width > CRAZYPOD_ARTWORK_MAX_SIZE ||
       header.height > CRAZYPOD_ARTWORK_MAX_SIZE ||
       header.data_size !=
           (uint32_t)header.width * header.height * sizeof(fb_data)) {
        close(fd);
        return ARTWORK_CACHE_MISS;
    }
    source_pixels =
        request->target_size == CRAZYPOD_ARTWORK_CACHE_SIZE
        ? pixels : canonical_scratch;
    if(!read_exact(fd, source_pixels, header.data_size)) {
        close(fd);
        return ARTWORK_CACHE_MISS;
    }
    close(fd);
    if(request->target_size == CRAZYPOD_ARTWORK_CACHE_SIZE) {
        crazypod_image_configure_rgb565(
            descriptor, pixels, header.width, header.height);
        return ARTWORK_CACHE_IMAGE;
    }

    output_width = header.width;
    output_height = header.height;
    if(output_width >= output_height) {
        output_height =
            output_height * request->target_size / output_width;
        output_width = request->target_size;
    }
    else {
        output_width =
            output_width * request->target_size / output_height;
        output_height = request->target_size;
    }
    if(output_width < 1)
        output_width = 1;
    if(output_height < 1)
        output_height = 1;
    if(!crazypod_image_scale_rgb565(
           source_pixels, header.width, header.height, header.width,
           pixels, output_width, output_height))
        return ARTWORK_CACHE_MISS;
    crazypod_image_configure_rgb565(
        descriptor, pixels, output_width, output_height);
    return ARTWORK_CACHE_IMAGE;
}

static bool artwork_cache_store(
    const struct artwork_decode_request *request,
    const struct artwork_source *source,
    const lv_image_dsc_t *descriptor, bool failed)
{
    struct artwork_cache_header header;
    char path[MAX_PATH];
    uint32_t key_a;
    uint32_t key_b;
    int fd;
    bool complete;

    artwork_cache_path(request, path, sizeof(path), &key_a, &key_b,
                       true);
    memset(&header, 0, sizeof(header));
    header.magic = CRAZYPOD_CACHE_MAGIC;
    header.version = CRAZYPOD_CACHE_VERSION;
    header.key_a = key_a;
    header.key_b = key_b;
    artwork_source_keys(source, &header.source_key_a,
                        &header.source_key_b);
    header.requested_size = CRAZYPOD_ARTWORK_CACHE_SIZE;
    header.reserved = failed ? 1u : 0u;
    if(descriptor != NULL) {
        header.width = descriptor->header.w;
        header.height = descriptor->header.h;
        header.data_size = descriptor->data_size;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    complete = write_exact(fd, &header, sizeof(header));
    if(complete && descriptor != NULL)
        complete = write_exact(fd, descriptor->data,
                               descriptor->data_size);
    close(fd);
    if(!complete)
        remove(path);
    else {
        ++artwork_cache_generation;
        if(artwork_cache_generation == 0)
            ++artwork_cache_generation;
    }
    return complete;
}

static bool decode_artwork(const struct artwork_source *source,
                           int target_size,
                           lv_image_dsc_t *descriptor)
{
    struct bitmap bitmap;
    int fd;
    int result = -1;
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;

    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = target_size;
    bitmap.height = target_size;
    bitmap.data = decode_buffer;

    if(source->kind == CRAZYPOD_ARTWORK_SOURCE_EXTERNAL) {
        crazypod_image_decode_lock();
        if((source->type & AA_CLEAR_FLAGS_MASK) == AA_TYPE_JPG)
            result = read_jpeg_file(source->path, &bitmap,
                                    sizeof(decode_buffer), format,
                                    &format_native);
        else
            result = read_bmp_file(source->path, &bitmap,
                                   sizeof(decode_buffer), format,
                                   &format_native);
        crazypod_image_decode_unlock();
    }
    else if(source->kind == CRAZYPOD_ARTWORK_SOURCE_EMBEDDED) {
        fd = open(source->path, O_RDONLY);
        if(fd < 0)
            return false;
        crazypod_image_decode_lock();
        if(lseek(fd, source->offset, SEEK_SET) >= 0) {
            if((source->type & AA_CLEAR_FLAGS_MASK) == AA_TYPE_JPG)
                result = clip_jpeg_fd(fd, source->type, source->size,
                                      &bitmap, sizeof(decode_buffer),
                                      format, &format_native);
            else if((source->type & AA_CLEAR_FLAGS_MASK) == AA_TYPE_BMP)
                result = read_bmp_fd(fd, &bitmap,
                                     sizeof(decode_buffer),
                                     format, &format_native);
        }
        crazypod_image_decode_unlock();
        close(fd);
    }

    if(result < 0 || bitmap.width <= 0 || bitmap.height <= 0 ||
       bitmap.data == NULL)
        return false;
    return crazypod_image_configure_rgb565(
        descriptor, (fb_data *)bitmap.data, bitmap.width, bitmap.height);
}

static void copy_decoded_pixels(const lv_image_dsc_t *source,
                                fb_data *destination,
                                lv_image_dsc_t *destination_descriptor)
{
    const fb_data *source_pixels = (const fb_data *)source->data;
    int source_stride = source->header.stride / sizeof(fb_data);
    int width = source->header.w;
    int height = source->header.h;
    int row;

    for(row = 0; row < height; ++row) {
        memcpy(destination + row * width,
               source_pixels + row * source_stride,
               (size_t)width * sizeof(fb_data));
    }
    crazypod_image_configure_rgb565(
        destination_descriptor, destination, width, height);
}

static bool publish_decoded_pixels(
    const lv_image_dsc_t *source, int target_size,
    fb_data *destination, lv_image_dsc_t *destination_descriptor)
{
    const fb_data *source_pixels = (const fb_data *)source->data;
    int source_stride = source->header.stride / sizeof(fb_data);
    int width = source->header.w;
    int height = source->header.h;
    int output_width = width;
    int output_height = height;

    if(target_size >= CRAZYPOD_ARTWORK_CACHE_SIZE) {
        copy_decoded_pixels(source, destination,
                            destination_descriptor);
        return true;
    }
    if(output_width >= output_height) {
        output_height =
            output_height * target_size / output_width;
        output_width = target_size;
    }
    else {
        output_width =
            output_width * target_size / output_height;
        output_height = target_size;
    }
    if(output_width < 1)
        output_width = 1;
    if(output_height < 1)
        output_height = 1;
    if(!crazypod_image_scale_rgb565(
           source_pixels, width, height, source_stride,
           destination, output_width, output_height))
        return false;
    return crazypod_image_configure_rgb565(
        destination_descriptor, destination,
        output_width, output_height);
}

static bool find_pending_request(int *slot_index, unsigned *serial,
                                 int *bank,
                                 struct artwork_decode_request *request)
{
    int best_slot = -1;
    int best_priority = 0x7fffffff;
    int i;

    mutex_lock(&artwork_mutex);
    for(i = 0; i < CRAZYPOD_ARTWORK_SLOTS; ++i) {
        struct artwork_slot *slot = &artwork_slots[i];
        if(slot->request_serial == slot->decoded_serial)
            continue;
        if(slot->requested_priority < best_priority) {
            best_slot = i;
            best_priority = slot->requested_priority;
        }
    }
    if(best_slot >= 0) {
        struct artwork_slot *slot = &artwork_slots[best_slot];
        *slot_index = best_slot;
        *serial = slot->request_serial;
        *bank = 1 - slot->active_bank;
        request->target_size = slot->requested_size;
        request->offset = slot->requested_artwork_offset;
        request->size = slot->requested_artwork_size;
        request->source_size = slot->requested_source_size;
        request->source_mtime = slot->requested_source_mtime;
        request->type = slot->requested_artwork_type;
        request->cache_generation =
            slot->requested_cache_generation;
        request->embedded = slot->requested_artwork_embedded;
        request->cache_only = slot->requested_cache_only;
        request->direct_source = slot->requested_direct_source;
        snprintf(request->track_path, sizeof(request->track_path),
                 "%s", slot->requested_path);
        snprintf(request->album, sizeof(request->album),
                 "%s", slot->requested_album);
        snprintf(request->album_artist,
                 sizeof(request->album_artist),
                 "%s", slot->requested_album_artist);
        snprintf(request->artist, sizeof(request->artist),
                 "%s", slot->requested_artist);
        mutex_unlock(&artwork_mutex);
        return true;
    }
    artwork_wake_queued = false;
    mutex_unlock(&artwork_mutex);
    return false;
}

static bool request_is_current(int slot_index, unsigned serial)
{
    bool current;

    mutex_lock(&artwork_mutex);
    current = artwork_slots[slot_index].request_serial == serial;
    mutex_unlock(&artwork_mutex);
    return current;
}

static bool begin_disk_work(void)
{
    bool allowed;

    mutex_lock(&artwork_mutex);
    allowed = !artwork_suspended;
    if(allowed)
        artwork_worker_decoding = true;
    mutex_unlock(&artwork_mutex);
    return allowed;
}

static void end_disk_work(void)
{
    mutex_lock(&artwork_mutex);
    artwork_worker_decoding = false;
    mutex_unlock(&artwork_mutex);
}

static bool album_request(
    int album_index, struct artwork_decode_request *request)
{
    const struct crazypod_track *track;

    memset(request, 0, sizeof(*request));
    track = crazypod_music_album_track(album_index, 0);
    if(track == NULL)
        return false;
    request->target_size = CRAZYPOD_ARTWORK_CACHE_SIZE;
    request->offset = track->artwork_offset;
    request->size = track->artwork_size;
    request->source_size = track->source_size;
    request->source_mtime = track->source_mtime;
    request->type = track->artwork_type;
    request->embedded = track->artwork_embedded;
    request->cache_only = false;
    snprintf(request->track_path, sizeof(request->track_path),
             "%s", track->path);
    snprintf(request->album, sizeof(request->album),
             "%s", track->album);
    snprintf(request->album_artist, sizeof(request->album_artist),
             "%s", track->album_artist);
    snprintf(request->artist, sizeof(request->artist),
             "%s", track->artist);
    return true;
}

static int compare_prime_album_paths(const void *left_ptr,
                                     const void *right_ptr)
{
    int left_index = (int)*(const uint32_t *)left_ptr;
    int right_index = (int)*(const uint32_t *)right_ptr;
    const struct crazypod_track *left =
        crazypod_music_album_track(left_index, 0);
    const struct crazypod_track *right =
        crazypod_music_album_track(right_index, 0);

    if(left == NULL)
        return right == NULL ? 0 : 1;
    if(right == NULL)
        return -1;
    return strcmp(left->path, right->path);
}

static void artwork_prime_order_release(void)
{
    if(artwork_prime_order_handle > 0) {
        core_unpin(artwork_prime_order_handle);
        artwork_prime_order_handle =
            core_free(artwork_prime_order_handle);
    }
    artwork_prime_order = NULL;
}

static uint32_t artwork_library_key(void)
{
    uint32_t hash = 2166136261u;
    int count = crazypod_music_album_count();
    int album_index;

    hash = hash_bytes(hash, &count, sizeof(count));
    for(album_index = 0; album_index < count; ++album_index) {
        const struct crazypod_track *track =
            crazypod_music_album_track(album_index, 0);
        const struct crazypod_album *album =
            crazypod_music_album(album_index);

        if(track == NULL || album == NULL)
            continue;
        hash = hash_bytes(hash, album->title,
                          strlen(album->title) + 1);
        hash = hash_bytes(hash, album->artist,
                          strlen(album->artist) + 1);
        hash = hash_bytes(hash, track->path,
                          strlen(track->path) + 1);
        hash = hash_bytes(hash, &track->source_size,
                          sizeof(track->source_size));
        hash = hash_bytes(hash, &track->source_mtime,
                          sizeof(track->source_mtime));
        hash = hash_bytes(hash, &track->artwork_offset,
                          sizeof(track->artwork_offset));
        hash = hash_bytes(hash, &track->artwork_size,
                          sizeof(track->artwork_size));
        hash = hash_bytes(hash, &track->artwork_type,
                          sizeof(track->artwork_type));
        hash = hash_bytes(hash, &track->artwork_embedded,
                          sizeof(track->artwork_embedded));
    }
    return hash;
}

static uint32_t artwork_ready_checksum(
    const struct artwork_ready_disk *source)
{
    struct artwork_ready_disk disk = *source;

    disk.checksum = 0;
    return hash_bytes(2166136261u, &disk, sizeof(disk));
}

static uint32_t artwork_progress_checksum(
    const struct artwork_progress_disk *source)
{
    struct artwork_progress_disk disk = *source;

    disk.checksum = 0;
    return hash_bytes(2166136261u, &disk, sizeof(disk));
}

static int artwork_progress_load(int total)
{
    struct artwork_progress_disk disk;
    bool valid;
    int fd;

    fd = open(CRAZYPOD_ARTWORK_PROGRESS_PATH, O_RDONLY);
    if(fd < 0)
        return 0;
    valid =
        read_exact(fd, &disk, sizeof(disk)) &&
        disk.magic == CRAZYPOD_ARTWORK_PROGRESS_MAGIC &&
        disk.version == CRAZYPOD_ARTWORK_PROGRESS_VERSION &&
        disk.album_count == (uint32_t)total &&
        disk.library_key == artwork_library_key() &&
        disk.next_index < (uint32_t)total &&
        disk.checksum == artwork_progress_checksum(&disk);
    close(fd);
    if(!valid) {
        remove(CRAZYPOD_ARTWORK_PROGRESS_TMP);
        remove(CRAZYPOD_ARTWORK_PROGRESS_PATH);
        return 0;
    }
    return (int)disk.next_index;
}

static bool artwork_progress_save(int next_index, int total)
{
    struct artwork_progress_disk disk;
    bool complete;
    int fd;

    if(next_index <= artwork_prime_persisted ||
       next_index <= 0 || next_index >= total)
        return true;
    memset(&disk, 0, sizeof(disk));
    disk.magic = CRAZYPOD_ARTWORK_PROGRESS_MAGIC;
    disk.version = CRAZYPOD_ARTWORK_PROGRESS_VERSION;
    disk.album_count = (uint32_t)total;
    disk.library_key = artwork_library_key();
    disk.next_index = (uint32_t)next_index;
    disk.checksum = artwork_progress_checksum(&disk);
    remove(CRAZYPOD_ARTWORK_PROGRESS_TMP);
    fd = open(CRAZYPOD_ARTWORK_PROGRESS_TMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    /*
     * One checkpoint commits the preceding cache-file batch. Checkpointing
     * every album would restore the original per-cover fsync bottleneck.
     */
    complete = write_exact(fd, &disk, sizeof(disk)) &&
        fsync(fd) >= 0;
    close(fd);
    if(!complete ||
       rename(CRAZYPOD_ARTWORK_PROGRESS_TMP,
              CRAZYPOD_ARTWORK_PROGRESS_PATH) < 0) {
        remove(CRAZYPOD_ARTWORK_PROGRESS_TMP);
        return false;
    }
    artwork_prime_persisted = next_index;
    return true;
}

static bool artwork_ready_save(void)
{
    struct artwork_ready_disk disk;
    bool complete;
    int fd;

    memset(&disk, 0, sizeof(disk));
    disk.magic = CRAZYPOD_ARTWORK_READY_MAGIC;
    disk.version = CRAZYPOD_ARTWORK_READY_VERSION;
    disk.album_count = (uint32_t)crazypod_music_album_count();
    disk.library_key = artwork_library_key();
    disk.checksum = artwork_ready_checksum(&disk);
    remove(CRAZYPOD_ARTWORK_READY_TMP);
    fd = open(CRAZYPOD_ARTWORK_READY_TMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    /*
     * Every per-album cache file is already closed. This single fsync commits
     * the batch and the completion record instead of spinning the disk up for
     * every cover.
     */
    complete = write_exact(fd, &disk, sizeof(disk)) &&
        fsync(fd) >= 0;
    close(fd);
    if(!complete ||
       rename(CRAZYPOD_ARTWORK_READY_TMP,
              CRAZYPOD_ARTWORK_READY_PATH) < 0) {
        remove(CRAZYPOD_ARTWORK_READY_TMP);
        return false;
    }
    remove(CRAZYPOD_ARTWORK_VALIDATE_PATH);
    remove(CRAZYPOD_ARTWORK_PROGRESS_TMP);
    remove(CRAZYPOD_ARTWORK_PROGRESS_PATH);
    artwork_prime_persisted = 0;
    artwork_validation_required = false;
    ++artwork_cache_generation;
    if(artwork_cache_generation == 0)
        ++artwork_cache_generation;
    return true;
}

static bool artwork_ready_load(void)
{
    struct artwork_ready_disk disk;
    bool valid;
    int fd;

    if(artwork_validation_required)
        return false;
    fd = open(CRAZYPOD_ARTWORK_READY_PATH, O_RDONLY);
    if(fd < 0)
        return false;
    valid =
        read_exact(fd, &disk, sizeof(disk)) &&
        disk.magic == CRAZYPOD_ARTWORK_READY_MAGIC &&
        disk.version == CRAZYPOD_ARTWORK_READY_VERSION &&
        disk.album_count ==
            (uint32_t)crazypod_music_album_count() &&
        disk.library_key == artwork_library_key() &&
        disk.checksum == artwork_ready_checksum(&disk);
    close(fd);
    return valid;
}

static void artwork_require_validation(void)
{
    static const uint32_t marker = CRAZYPOD_ARTWORK_READY_MAGIC;
    int fd;

    remove(CRAZYPOD_ARTWORK_READY_PATH);
    remove(CRAZYPOD_ARTWORK_PROGRESS_TMP);
    remove(CRAZYPOD_ARTWORK_PROGRESS_PATH);
    artwork_prime_persisted = 0;
    fd = open(CRAZYPOD_ARTWORK_VALIDATE_PATH,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd >= 0) {
        if(write_exact(fd, &marker, sizeof(marker)))
            (void)fsync(fd);
        close(fd);
    }
    artwork_validation_required = true;
}

static bool prime_request(int *album_index,
                          struct artwork_decode_request *request)
{
    mutex_lock(&artwork_mutex);
    if(artwork_suspended || !artwork_prime_active ||
       artwork_prime_processing ||
       artwork_prime_next >= artwork_prime_total) {
        if(artwork_prime_active &&
           artwork_prime_next >= artwork_prime_total)
            artwork_prime_active = false;
        mutex_unlock(&artwork_mutex);
        return false;
    }
    *album_index = artwork_prime_next;
    artwork_prime_processing = true;
    mutex_unlock(&artwork_mutex);

    if(!album_request(artwork_prime_order[*album_index], request))
        memset(request, 0, sizeof(*request));
    return true;
}

static bool finish_prime_request(int album_index, bool completed)
{
    bool checkpoint = false;
    int next_index = 0;
    int total = 0;

    mutex_lock(&artwork_mutex);
    if(completed && artwork_prime_active &&
       artwork_prime_next == album_index) {
        ++artwork_prime_next;
        artwork_prime_completed = artwork_prime_next;
        next_index = artwork_prime_next;
        total = artwork_prime_total;
        checkpoint =
            next_index < total &&
            next_index % CRAZYPOD_ARTWORK_PROGRESS_INTERVAL == 0;
        if(artwork_prime_next >= artwork_prime_total)
            artwork_prime_active = false;
    }
    artwork_prime_processing = false;
    mutex_unlock(&artwork_mutex);
    return !checkpoint ||
        artwork_progress_save(next_index, total);
}

static void fail_prime_request(void)
{
    mutex_lock(&artwork_mutex);
    artwork_prime_active = false;
    artwork_prime_processing = false;
    artwork_prime_failed = true;
    mutex_unlock(&artwork_mutex);
}

static void artwork_thread(void)
{
    struct queue_event event;

    while(true) {
        queue_wait(&artwork_queue, &event);
        if(event.id != CRAZYPOD_ARTWORK_WAKE)
            continue;

        while(true) {
            lv_image_dsc_t decoded_descriptor;
            lv_image_dsc_t published_descriptor;
            struct artwork_decode_request request;
            struct artwork_source source;
            enum artwork_cache_result cache_result;
            unsigned serial;
            int slot_index;
            int bank;
            bool source_resolved;
            bool valid;
            bool failed;

            if(find_pending_request(&slot_index, &serial, &bank,
                                    &request)) {
                if(!begin_disk_work())
                    break;
                memset(&decoded_descriptor, 0,
                       sizeof(decoded_descriptor));
                memset(&published_descriptor, 0,
                       sizeof(published_descriptor));
                memset(&source, 0, sizeof(source));
                source_resolved = artwork_validation_required;
                if(source_resolved)
                    resolve_artwork_source(&request, &source);
                cache_result = artwork_cache_load(
                    &request, source_resolved ? &source : NULL,
                    source_resolved,
                    artwork_slots[slot_index].pixels[bank],
                    &published_descriptor);
                valid = cache_result == ARTWORK_CACHE_IMAGE;
                failed = cache_result == ARTWORK_CACHE_ERROR;

                if(cache_result == ARTWORK_CACHE_MISS &&
                   !request.cache_only &&
                   request_is_current(slot_index, serial)) {
                    if(!source_resolved) {
                        resolve_artwork_source(&request, &source);
                        source_resolved = true;
                    }
                    valid = decode_artwork(
                        &source,
                        request.direct_source
                            ? request.target_size
                            : CRAZYPOD_ARTWORK_CACHE_SIZE,
                        &decoded_descriptor);
                    if(valid) {
                        if(!request.direct_source)
                            (void)artwork_cache_store(
                                &request, &source,
                                &decoded_descriptor, false);
                        valid = publish_decoded_pixels(
                            &decoded_descriptor,
                            request.target_size,
                            artwork_slots[slot_index].pixels[bank],
                            &published_descriptor);
                    }
                    else {
                        failed = source.kind !=
                            CRAZYPOD_ARTWORK_SOURCE_NONE;
                        if(!request.direct_source)
                            (void)artwork_cache_store(
                                &request, &source, NULL, failed);
                    }
                }

                mutex_lock(&artwork_mutex);
                if(artwork_slots[slot_index].request_serial == serial) {
                    struct artwork_slot *slot =
                        &artwork_slots[slot_index];
                    slot->descriptor[bank] = published_descriptor;
                    snprintf(slot->decoded_path,
                             sizeof(slot->decoded_path),
                             "%s", request.track_path);
                    slot->decoded_size = request.target_size;
                    slot->decoded_serial = serial;
                    slot->decoded_cache_only = request.cache_only;
                    slot->decoded_direct_source =
                        request.direct_source;
                    slot->decoded_cache_generation =
                        request.cache_generation;
                    slot->decoded_cache_miss =
                        request.cache_only &&
                        cache_result == ARTWORK_CACHE_MISS;
                    slot->valid = valid;
                    slot->failed = failed;
                    if(valid)
                        slot->active_bank = bank;
                    ++artwork_publish_generation;
                    slot->publish_generation =
                        artwork_publish_generation;
                }
                mutex_unlock(&artwork_mutex);
                end_disk_work();
                yield();
                continue;
            }

            {
                int album_index;
                bool completed = true;
                bool last_album;

                if(!prime_request(&album_index, &request))
                    break;
                if(!begin_disk_work()) {
                    (void)finish_prime_request(
                        album_index, false);
                    break;
                }
                reset_poweroff_timer();
                if(completed && request.track_path[0] != '\0') {
                    memset(&source, 0, sizeof(source));
                    source_resolved = artwork_validation_required;
                    if(source_resolved)
                        resolve_artwork_source(&request, &source);
                    cache_result = artwork_cache_load(
                        &request,
                        source_resolved ? &source : NULL,
                        source_resolved,
                        canonical_scratch, &decoded_descriptor);
                    if(cache_result == ARTWORK_CACHE_MISS) {
                        if(!source_resolved)
                            resolve_artwork_source(
                                &request, &source);
                        memset(&decoded_descriptor, 0,
                               sizeof(decoded_descriptor));
                        valid = decode_artwork(
                            &source, CRAZYPOD_ARTWORK_CACHE_SIZE,
                            &decoded_descriptor);
                        completed = artwork_cache_store(
                            &request, &source,
                            valid ? &decoded_descriptor : NULL,
                            !valid && source.kind !=
                                CRAZYPOD_ARTWORK_SOURCE_NONE);
                    }
                }
                last_album =
                    album_index + 1 >= crazypod_music_album_count();
                if(completed && last_album)
                    completed = artwork_ready_save();
                if(completed)
                    completed = finish_prime_request(
                        album_index, true);
                if(!completed)
                    fail_prime_request();
                end_disk_work();
                if(!completed)
                    break;
            }
            yield();
        }
    }
}

void crazypod_artwork_init(void)
{
    int i;
    char shard_directory[MAX_PATH];

    memset(artwork_slots, 0, sizeof(artwork_slots));
    for(i = 0; i < CRAZYPOD_ARTWORK_SLOTS; ++i) {
        artwork_slots[i].active_bank = 0;
        artwork_slots[i].requested_priority =
            CRAZYPOD_ARTWORK_DEFAULT_PRIORITY;
        if(i < CRAZYPOD_COVERFLOW_ARTWORK_SLOTS) {
            artwork_slots[i].pixels[0] = coverflow_pixels[i][0];
            artwork_slots[i].pixels[1] = coverflow_pixels[i][1];
            artwork_slots[i].capacity_size =
                CRAZYPOD_COVERFLOW_ARTWORK_SIZE;
        }
    }
    artwork_slots[CRAZYPOD_PREVIEW_ARTWORK_SLOT].pixels[0] =
        preview_pixels[0];
    artwork_slots[CRAZYPOD_PREVIEW_ARTWORK_SLOT].pixels[1] =
        preview_pixels[1];
    artwork_slots[CRAZYPOD_PREVIEW_ARTWORK_SLOT].capacity_size =
        CRAZYPOD_PREVIEW_ARTWORK_SIZE;
    artwork_slots[CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT].pixels[0] =
        now_pixels[0][0];
    artwork_slots[CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT].pixels[1] =
        now_pixels[0][1];
    artwork_slots[CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT].capacity_size =
        CRAZYPOD_NOW_ARTWORK_MAX_SIZE;
    artwork_slots[CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT].pixels[0] =
        now_pixels[1][0];
    artwork_slots[CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT].pixels[1] =
        now_pixels[1][1];
    artwork_slots[CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT].capacity_size =
        CRAZYPOD_NOW_ARTWORK_MAX_SIZE;
    artwork_slots[
        CRAZYPOD_NOW_PREFETCH_SECOND_ARTWORK_SLOT].pixels[0] =
            now_pixels[2][0];
    artwork_slots[
        CRAZYPOD_NOW_PREFETCH_SECOND_ARTWORK_SLOT].pixels[1] =
            now_pixels[2][1];
    artwork_slots[
        CRAZYPOD_NOW_PREFETCH_SECOND_ARTWORK_SLOT].capacity_size =
            CRAZYPOD_NOW_ARTWORK_MAX_SIZE;
    artwork_slots[CRAZYPOD_CAPSULE_ARTWORK_SLOT].pixels[0] =
        capsule_pixels[0];
    artwork_slots[CRAZYPOD_CAPSULE_ARTWORK_SLOT].pixels[1] =
        capsule_pixels[1];
    artwork_slots[CRAZYPOD_CAPSULE_ARTWORK_SLOT].capacity_size =
        CRAZYPOD_CAPSULE_ARTWORK_SIZE;
    mkdir(CRAZYPOD_DIRECTORY);
    mkdir(CRAZYPOD_CACHE_DIRECTORY);
    mkdir(CRAZYPOD_COVER_CACHE_DIRECTORY);
    for(i = 0; i < CRAZYPOD_COVER_CACHE_SHARDS; ++i) {
        snprintf(shard_directory, sizeof(shard_directory), "%s/%X",
                 CRAZYPOD_COVER_CACHE_DIRECTORY, i);
        mkdir(shard_directory);
    }
    artwork_validation_required =
        file_exists(CRAZYPOD_ARTWORK_VALIDATE_PATH) ||
        !file_exists(CRAZYPOD_ARTWORK_READY_PATH);
    if(artwork_media_cache_invalid()) {
        artwork_require_validation();
        /* Complete an interrupted USB invalidation before the photo/video
         * catalogs are loaded later in startup. Music has already refused
         * its cache because the marker was present. */
        remove(CRAZYPOD_MUSIC_CATALOG_TMP);
        remove(CRAZYPOD_MUSIC_CATALOG_PATH);
        remove(CRAZYPOD_PHOTO_CATALOG_TMP);
        remove(CRAZYPOD_PHOTO_CATALOG_PATH);
        remove(CRAZYPOD_PHOTO_THUMB_CACHE);
        remove(CRAZYPOD_PHOTO_VIEW_CACHE);
        remove(CRAZYPOD_VIDEO_CATALOG_TMP);
        remove(CRAZYPOD_VIDEO_CATALOG_PATH);
        remove(CRAZYPOD_MEDIA_INVALID_PATH);
    }
    artwork_publish_generation = 0;
    artwork_worker_decoding = false;
    artwork_suspended = false;
    artwork_storage_suspended = false;
    artwork_lock_suspended = false;
    artwork_wake_queued = false;
    artwork_prime_active = false;
    artwork_prime_processing = false;
    artwork_prime_failed = false;
    artwork_prime_next = 0;
    artwork_prime_completed = 0;
    artwork_prime_total = 0;
    artwork_prime_persisted = 0;
    artwork_prime_order_handle = 0;
    artwork_prime_order = NULL;
    memset(&artwork_track_directory, 0,
           sizeof(artwork_track_directory));
    memset(&artwork_parent_directory, 0,
           sizeof(artwork_parent_directory));
    artwork_cache_generation = 1;
    mutex_init(&artwork_mutex);
    queue_init(&artwork_queue, false);
    create_thread(artwork_thread, artwork_stack, sizeof(artwork_stack), 0,
                  "crazypod art"
                  IF_PRIO(, PRIORITY_BACKGROUND)
                  IF_COP(, CPU));
}

void crazypod_artwork_prime_library(void)
{
    int total = crazypod_music_album_count();
    int start_index;
    int album_index;

    crazypod_artwork_cancel_library_prime();
    if(total > 0 &&
       (size_t)total <= SIZE_MAX / sizeof(*artwork_prime_order)) {
        artwork_prime_order_handle = core_alloc(
            (size_t)total * sizeof(*artwork_prime_order));
        if(artwork_prime_order_handle <= 0) {
            mutex_lock(&artwork_mutex);
            artwork_prime_failed = true;
            artwork_prime_active = false;
            artwork_prime_total = total;
            mutex_unlock(&artwork_mutex);
            return;
        }
        core_pin(artwork_prime_order_handle);
        artwork_prime_order =
            core_get_data(artwork_prime_order_handle);
    }
    else if(total > 0) {
        mutex_lock(&artwork_mutex);
        artwork_prime_failed = true;
        artwork_prime_active = false;
        artwork_prime_total = total;
        mutex_unlock(&artwork_mutex);
        return;
    }
    for(album_index = 0; album_index < total; ++album_index)
        artwork_prime_order[album_index] = (uint32_t)album_index;
    if(total > 1)
        qsort(artwork_prime_order, (size_t)total,
              sizeof(artwork_prime_order[0]),
              compare_prime_album_paths);
    start_index = artwork_progress_load(total);
    mutex_lock(&artwork_mutex);
    artwork_prime_next = start_index;
    artwork_prime_completed = start_index;
    artwork_prime_total = total;
    artwork_prime_persisted = start_index;
    artwork_prime_failed = false;
    artwork_prime_active = total > 0;
    if(artwork_prime_active)
        artwork_wake_queued = true;
    mutex_unlock(&artwork_mutex);
    if(total > 0)
        queue_post(&artwork_queue, CRAZYPOD_ARTWORK_WAKE, 0);
}

bool crazypod_artwork_library_cache_ready(void)
{
    int total = crazypod_music_album_count();

    if(total == 0)
        return true;
    return artwork_ready_load();
}

void crazypod_artwork_invalidate_library_cache(void)
{
    int slot_index;

    artwork_require_validation();
    ++artwork_cache_generation;
    if(artwork_cache_generation == 0)
        ++artwork_cache_generation;

    mutex_lock(&artwork_mutex);
    artwork_prime_active = false;
    artwork_prime_processing = false;
    artwork_prime_next = 0;
    artwork_prime_completed = 0;
    artwork_prime_total = 0;
    artwork_prime_failed = false;
    for(slot_index = 0;
        slot_index < CRAZYPOD_ARTWORK_SLOTS;
        ++slot_index) {
        struct artwork_slot *slot = &artwork_slots[slot_index];

        slot->requested_path[0] = '\0';
        slot->requested_album[0] = '\0';
        slot->requested_album_artist[0] = '\0';
        slot->requested_artist[0] = '\0';
        slot->decoded_path[0] = '\0';
        slot->requested_size = 0;
        slot->decoded_size = 0;
        slot->requested_artwork_offset = 0;
        slot->requested_artwork_size = 0;
        slot->requested_source_size = 0;
        slot->requested_source_mtime = 0;
        ++slot->request_serial;
        /* Invalidated empty slots are settled, not decode requests.
         * A real load will advance request_serial again. */
        slot->decoded_serial = slot->request_serial;
        slot->requested_artwork_type = 0;
        slot->requested_artwork_embedded = false;
        slot->requested_cache_only = false;
        slot->requested_direct_source = false;
        slot->decoded_cache_only = false;
        slot->decoded_direct_source = false;
        slot->requested_cache_generation =
            artwork_cache_generation;
        slot->decoded_cache_generation =
            artwork_cache_generation;
        slot->decoded_cache_miss = false;
        slot->failed = false;
        slot->valid = false;
        slot->publish_generation = ++artwork_publish_generation;
    }
    artwork_wake_queued = false;
    mutex_unlock(&artwork_mutex);
}

void crazypod_artwork_cancel_library_prime(void)
{
    bool processing;
    int next_index;
    int total;

    mutex_lock(&artwork_mutex);
    artwork_prime_active = false;
    mutex_unlock(&artwork_mutex);
    do {
        mutex_lock(&artwork_mutex);
        processing = artwork_prime_processing;
        mutex_unlock(&artwork_mutex);
        if(processing)
            yield();
    } while(processing);
    mutex_lock(&artwork_mutex);
    next_index = artwork_prime_next;
    total = artwork_prime_total;
    mutex_unlock(&artwork_mutex);
    (void)artwork_progress_save(next_index, total);
    mutex_lock(&artwork_mutex);
    artwork_prime_next = 0;
    artwork_prime_completed = 0;
    artwork_prime_total = 0;
    artwork_prime_failed = false;
    mutex_unlock(&artwork_mutex);
    artwork_prime_order_release();
}

void crazypod_artwork_cancel_product_requests(void)
{
    bool busy;
    bool wake = false;
    int prime_next;
    int prime_total;
    int slot_index;

    /* Home only owns the capsule slot. Stop after the current decode so
     * stale preview/CoverFlow requests cannot continue disk work there. */
    mutex_lock(&artwork_mutex);
    artwork_suspended = true;
    artwork_prime_active = false;
    mutex_unlock(&artwork_mutex);
    do {
        mutex_lock(&artwork_mutex);
        busy = artwork_worker_decoding ||
            artwork_prime_processing;
        mutex_unlock(&artwork_mutex);
        if(busy)
            yield();
    } while(busy);

    mutex_lock(&artwork_mutex);
    prime_next = artwork_prime_next;
    prime_total = artwork_prime_total;
    mutex_unlock(&artwork_mutex);
    (void)artwork_progress_save(prime_next, prime_total);

    mutex_lock(&artwork_mutex);
    for(slot_index = 0;
        slot_index < CRAZYPOD_ARTWORK_SLOTS;
        ++slot_index) {
        struct artwork_slot *slot = &artwork_slots[slot_index];

        if(slot_index == CRAZYPOD_CAPSULE_ARTWORK_SLOT)
            continue;
        slot->requested_path[0] = '\0';
        slot->requested_album[0] = '\0';
        slot->requested_album_artist[0] = '\0';
        slot->requested_artist[0] = '\0';
        slot->requested_size = 0;
        slot->requested_artwork_offset = 0;
        slot->requested_artwork_size = 0;
        slot->requested_source_size = 0;
        slot->requested_source_mtime = 0;
        slot->requested_artwork_type = 0;
        slot->requested_artwork_embedded = false;
        slot->requested_cache_only = false;
        slot->requested_direct_source = false;
        slot->requested_cache_generation =
            artwork_cache_generation;
        ++slot->request_serial;
        slot->decoded_serial = slot->request_serial;
        slot->decoded_cache_generation =
            artwork_cache_generation;
    }
    artwork_wake_queued = false;
    if(!artwork_storage_suspended &&
       !artwork_lock_suspended) {
        struct artwork_slot *capsule =
            &artwork_slots[CRAZYPOD_CAPSULE_ARTWORK_SLOT];

        artwork_suspended = false;
        if(capsule->request_serial != capsule->decoded_serial) {
            artwork_wake_queued = true;
            wake = true;
        }
    }
    mutex_unlock(&artwork_mutex);
    if(wake)
        queue_post(&artwork_queue, CRAZYPOD_ARTWORK_WAKE, 0);
}

bool crazypod_artwork_library_priming(void)
{
    bool priming;

    mutex_lock(&artwork_mutex);
    priming = artwork_prime_active || artwork_prime_processing;
    mutex_unlock(&artwork_mutex);
    return priming;
}

bool crazypod_artwork_library_prime_failed(void)
{
    bool failed;

    mutex_lock(&artwork_mutex);
    failed = artwork_prime_failed;
    mutex_unlock(&artwork_mutex);
    return failed;
}

int crazypod_artwork_library_prime_completed(void)
{
    int completed;

    mutex_lock(&artwork_mutex);
    completed = artwork_prime_completed;
    mutex_unlock(&artwork_mutex);
    return completed;
}

int crazypod_artwork_library_prime_total(void)
{
    int total;

    mutex_lock(&artwork_mutex);
    total = artwork_prime_total;
    mutex_unlock(&artwork_mutex);
    return total;
}

static void artwork_apply_suspension(void)
{
    bool busy;
    bool suspended;
    bool wake = false;

    mutex_lock(&artwork_mutex);
    suspended = artwork_storage_suspended ||
        artwork_lock_suspended;
    /* Keep the worker gated throughout both transition directions. */
    artwork_suspended = true;
    mutex_unlock(&artwork_mutex);
    if(suspended) {
        do {
            mutex_lock(&artwork_mutex);
            busy = artwork_worker_decoding ||
                artwork_prime_processing;
            mutex_unlock(&artwork_mutex);
            if(busy)
                yield();
        } while(busy);
        return;
    }
    mutex_lock(&artwork_mutex);
    if(!artwork_storage_suspended &&
       !artwork_lock_suspended) {
        artwork_suspended = false;
        artwork_wake_queued = true;
        wake = true;
    }
    mutex_unlock(&artwork_mutex);
    if(wake)
        queue_post(&artwork_queue, CRAZYPOD_ARTWORK_WAKE, 0);
}

void crazypod_artwork_suspend(void)
{
    mutex_lock(&artwork_mutex);
    artwork_storage_suspended = true;
    mutex_unlock(&artwork_mutex);
    artwork_apply_suspension();
    artwork_track_directory.valid = false;
    artwork_parent_directory.valid = false;
}

void crazypod_artwork_resume(void)
{
    mutex_lock(&artwork_mutex);
    artwork_storage_suspended = false;
    mutex_unlock(&artwork_mutex);
    artwork_apply_suspension();
}

void crazypod_artwork_set_lock_suspended(bool suspended)
{
    bool changed;

    mutex_lock(&artwork_mutex);
    changed = artwork_lock_suspended != suspended;
    artwork_lock_suspended = suspended;
    mutex_unlock(&artwork_mutex);
    if(changed)
        artwork_apply_suspension();
}

static const lv_image_dsc_t *artwork_load_priority(
    int slot_index, const struct crazypod_track *track, int target_size,
    int priority, bool cache_only, bool direct_source)
{
    struct artwork_slot *slot;
    const lv_image_dsc_t *result = NULL;
    bool wake = false;

    if(slot_index < 0 || slot_index >= CRAZYPOD_ARTWORK_SLOTS ||
       track == NULL)
        return NULL;
    slot = &artwork_slots[slot_index];
    if(target_size < 16)
        target_size = 16;
    if(target_size > slot->capacity_size)
        target_size = slot->capacity_size;
    if(priority < 0)
        priority = 0;

    mutex_lock(&artwork_mutex);
    if(slot->decoded_serial == slot->request_serial &&
       slot->decoded_size == target_size &&
       strcmp(slot->decoded_path, track->path) == 0 &&
       slot->decoded_direct_source == direct_source &&
       (!slot->decoded_cache_only || cache_only) &&
       (!cache_only ||
        slot->decoded_cache_generation ==
            artwork_cache_generation)) {
        if(slot->valid)
            result = &slot->descriptor[slot->active_bank];
    }
    else if(slot->requested_size != target_size ||
            strcmp(slot->requested_path, track->path) != 0 ||
            slot->requested_cache_only != cache_only ||
            slot->requested_direct_source != direct_source ||
            (cache_only &&
             slot->requested_cache_generation !=
                 artwork_cache_generation)) {
        snprintf(slot->requested_path, sizeof(slot->requested_path),
                 "%s", track->path);
        snprintf(slot->requested_album, sizeof(slot->requested_album),
                 "%s", track->album);
        snprintf(slot->requested_album_artist,
                 sizeof(slot->requested_album_artist),
                 "%s", track->album_artist);
        snprintf(slot->requested_artist, sizeof(slot->requested_artist),
                 "%s", track->artist);
        slot->requested_size = target_size;
        slot->requested_priority = priority;
        slot->requested_artwork_offset = track->artwork_offset;
        slot->requested_artwork_size = track->artwork_size;
        slot->requested_source_size = track->source_size;
        slot->requested_source_mtime = track->source_mtime;
        slot->requested_artwork_type = track->artwork_type;
        slot->requested_artwork_embedded = track->artwork_embedded;
        slot->requested_cache_only = cache_only;
        slot->requested_direct_source = direct_source;
        slot->requested_cache_generation =
            artwork_cache_generation;
        ++slot->request_serial;
        if(slot->request_serial == 0)
            ++slot->request_serial;
        if(!artwork_wake_queued) {
            artwork_wake_queued = true;
            wake = true;
        }
    }
    else if(slot->request_serial != slot->decoded_serial &&
            priority < slot->requested_priority) {
        slot->requested_priority = priority;
    }
    mutex_unlock(&artwork_mutex);

    if(wake)
        queue_post(&artwork_queue, CRAZYPOD_ARTWORK_WAKE, 0);
    return result;
}

const lv_image_dsc_t *crazypod_artwork_load_priority(
    int slot_index, const struct crazypod_track *track, int target_size,
    int priority)
{
    return artwork_load_priority(slot_index, track, target_size,
                                 priority, false, false);
}

const lv_image_dsc_t *crazypod_artwork_load_cached_priority(
    int slot_index, const struct crazypod_track *track, int target_size,
    int priority)
{
    return artwork_load_priority(slot_index, track, target_size,
                                 priority, true, false);
}

const lv_image_dsc_t *crazypod_artwork_load_source_priority(
    int slot_index, const struct crazypod_track *track, int target_size,
    int priority)
{
    return artwork_load_priority(slot_index, track, target_size,
                                 priority, false, true);
}

const lv_image_dsc_t *crazypod_artwork_load(
    int slot_index, const struct crazypod_track *track, int target_size)
{
    return crazypod_artwork_load_priority(
        slot_index, track, target_size,
        CRAZYPOD_ARTWORK_DEFAULT_PRIORITY);
}

enum crazypod_artwork_state crazypod_artwork_state(
    int slot_index, const struct crazypod_track *track, int target_size)
{
    struct artwork_slot *slot;
    enum crazypod_artwork_state state = CRAZYPOD_ARTWORK_PENDING;

    if(slot_index < 0 || slot_index >= CRAZYPOD_ARTWORK_SLOTS ||
       track == NULL)
        return CRAZYPOD_ARTWORK_EMPTY;
    mutex_lock(&artwork_mutex);
    slot = &artwork_slots[slot_index];
    if(target_size < 16)
        target_size = 16;
    if(target_size > slot->capacity_size)
        target_size = slot->capacity_size;
    if(slot->decoded_serial == slot->request_serial &&
       slot->decoded_size == target_size &&
       strcmp(slot->decoded_path, track->path) == 0) {
        state = slot->valid ? CRAZYPOD_ARTWORK_IMAGE :
            slot->failed ? CRAZYPOD_ARTWORK_ERROR :
            CRAZYPOD_ARTWORK_EMPTY;
    }
    mutex_unlock(&artwork_mutex);
    return state;
}

unsigned crazypod_artwork_generation(void)
{
    unsigned generation;

    mutex_lock(&artwork_mutex);
    generation = artwork_publish_generation;
    mutex_unlock(&artwork_mutex);
    return generation;
}

unsigned crazypod_artwork_slot_generation(int slot_index)
{
    unsigned generation = 0;

    if(slot_index < 0 || slot_index >= CRAZYPOD_ARTWORK_SLOTS)
        return 0;
    mutex_lock(&artwork_mutex);
    generation = artwork_slots[slot_index].publish_generation;
    mutex_unlock(&artwork_mutex);
    return generation;
}

bool crazypod_artwork_busy(void)
{
    int i;
    bool busy;

    mutex_lock(&artwork_mutex);
    busy = artwork_worker_decoding ||
        (!artwork_suspended &&
         (artwork_prime_active || artwork_prime_processing));
    for(i = 0;
        !busy && !artwork_suspended &&
        i < CRAZYPOD_ARTWORK_SLOTS;
        ++i) {
        if(artwork_slots[i].request_serial !=
           artwork_slots[i].decoded_serial)
            busy = true;
    }
    mutex_unlock(&artwork_mutex);
    return busy;
}

#endif
