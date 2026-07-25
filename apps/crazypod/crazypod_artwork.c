#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "albumart.h"
#include "bmp.h"
#include "dir.h"
#include "file.h"
#include "jpeg_load.h"
#include "kernel.h"
#include "metadata.h"
#include "string-extra.h"

#include "crazypod_artwork.h"
#include "crazypod_image.h"

#define CRAZYPOD_ARTWORK_MAX_SIZE CRAZYPOD_COVERFLOW_ARTWORK_SIZE
#define CRAZYPOD_ARTWORK_BANKS 2
#define CRAZYPOD_ARTWORK_PIXELS \
    (CRAZYPOD_ARTWORK_MAX_SIZE * CRAZYPOD_ARTWORK_MAX_SIZE)
#define CRAZYPOD_DECODE_BUFFER_SIZE \
    (CRAZYPOD_ARTWORK_PIXELS * sizeof(fb_data) + 48 * 1024)
#define CRAZYPOD_ARTWORK_WAKE 1
#define CRAZYPOD_ARTWORK_DEFAULT_PRIORITY 100
#define CRAZYPOD_CACHE_MAGIC 0x43504632u
#define CRAZYPOD_CACHE_VERSION 6
#define CRAZYPOD_DIRECTORY "/.crazypod"
#define CRAZYPOD_CACHE_DIRECTORY CRAZYPOD_DIRECTORY "/cache"
#define CRAZYPOD_COVER_CACHE_DIRECTORY CRAZYPOD_CACHE_DIRECTORY "/CV6"
#define CRAZYPOD_COVER_CACHE_SHARDS 16
#define CRAZYPOD_COVER_PACK_DIRECTORY CRAZYPOD_CACHE_DIRECTORY "/CF7"
#define CRAZYPOD_COVER_PACK_INDEX CRAZYPOD_COVER_PACK_DIRECTORY "/covers.idx"
#define CRAZYPOD_COVER_PACK_INDEX_TMP CRAZYPOD_COVER_PACK_DIRECTORY "/covers.tmp"
#define CRAZYPOD_COVER_PACK_DATA CRAZYPOD_COVER_PACK_DIRECTORY "/covers.dat"
#define CRAZYPOD_COVER_PACK_MAGIC 0x43465037u
#define CRAZYPOD_COVER_PACK_VERSION 1
#define CRAZYPOD_COVER_PACK_IMAGE 1
#define CRAZYPOD_COVER_PACK_EMPTY 2
#define CRAZYPOD_COVER_PACK_HASH_SIZE 4096

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
    int active_bank;
    uint8_t requested_artwork_type;
    bool requested_artwork_embedded;
    bool requested_cache_only;
    bool decoded_cache_only;
    bool decoded_cache_miss;
    bool valid;
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
    bool embedded;
    bool cache_only;
};

struct artwork_cache_header {
    uint32_t magic;
    uint32_t version;
    uint32_t key_a;
    uint32_t key_b;
    uint32_t data_size;
    uint16_t requested_size;
    uint16_t width;
    uint16_t height;
    uint16_t reserved;
};

struct cover_pack_header {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t entry_size;
    uint32_t artwork_size;
    uint32_t data_size;
};

struct cover_pack_entry {
    uint32_t key_a;
    uint32_t key_b;
    uint32_t offset;
    uint32_t data_size;
    uint16_t width;
    uint16_t height;
    uint8_t state;
    uint8_t reserved[3];
};

enum artwork_cache_result {
    ARTWORK_CACHE_MISS,
    ARTWORK_CACHE_IMAGE,
    ARTWORK_CACHE_EMPTY,
};

static struct artwork_slot artwork_slots[CRAZYPOD_ARTWORK_SLOTS];
static fb_data coverflow_pixels[CRAZYPOD_COVERFLOW_ARTWORK_SLOTS]
    [CRAZYPOD_ARTWORK_BANKS][CRAZYPOD_COVERFLOW_ARTWORK_SIZE *
                             CRAZYPOD_COVERFLOW_ARTWORK_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data preview_pixels[CRAZYPOD_ARTWORK_BANKS]
    [CRAZYPOD_PREVIEW_ARTWORK_SIZE * CRAZYPOD_PREVIEW_ARTWORK_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data now_pixels[2][CRAZYPOD_ARTWORK_BANKS]
    [CRAZYPOD_COVERFLOW_ARTWORK_SIZE *
     CRAZYPOD_COVERFLOW_ARTWORK_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data capsule_pixels[CRAZYPOD_ARTWORK_BANKS]
    [CRAZYPOD_CAPSULE_ARTWORK_SIZE * CRAZYPOD_CAPSULE_ARTWORK_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static unsigned char decode_buffer[CRAZYPOD_DECODE_BUFFER_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static struct mutex artwork_mutex;
static struct event_queue artwork_queue;
static long artwork_stack[(DEFAULT_STACK_SIZE + 0x5000) / sizeof(long)];
static volatile unsigned artwork_publish_generation;
static volatile bool artwork_worker_decoding;
static bool artwork_suspended;
static bool artwork_wake_queued;
static bool artwork_prime_active;
static bool artwork_prime_processing;
static bool artwork_prime_failed;
static int artwork_prime_next;
static int artwork_prime_completed;
static int artwork_prime_total;
static struct cover_pack_entry
    cover_pack_entries[CRAZYPOD_MAX_TRACKS];
static struct cover_pack_entry
    cover_pack_build_entries[CRAZYPOD_MAX_TRACKS];
static int16_t cover_pack_hash[CRAZYPOD_COVER_PACK_HASH_SIZE];
static int cover_pack_entry_count;
static int cover_pack_build_count;
static uint32_t cover_pack_data_size;
static uint32_t cover_pack_build_data_size;
static int cover_pack_data_fd = -1;
static bool cover_pack_building;

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
    a = hash_bytes(a, &request->target_size,
                   sizeof(request->target_size));

    b = hash_bytes(b, request->album_artist,
                   strlen(request->album_artist));
    b = hash_bytes(b, request->album, strlen(request->album));
    b = hash_bytes(b, request->track_path, directory_length);
    b = hash_bytes(b, &request->offset, sizeof(request->offset));
    b = hash_bytes(b, &request->size, sizeof(request->size));
    b = hash_bytes(b, &request->source_size,
                   sizeof(request->source_size));
    b = hash_bytes(b, &request->source_mtime,
                   sizeof(request->source_mtime));
    b = hash_bytes(b, &request->target_size,
                   sizeof(request->target_size));
    *key_a = a;
    *key_b = b;
}

static void artwork_cache_path(const struct artwork_decode_request *request,
                               char *path, size_t path_size,
                               uint32_t *key_a, uint32_t *key_b,
                               bool create_directory)
{
    char directory[MAX_PATH];
    uint32_t filename_key;
    unsigned shard;

    artwork_cache_keys(request, key_a, key_b);
    shard = (*key_b >> 28) & 0x0f;
    filename_key = *key_a ^ (*key_b << 11) ^ (*key_b >> 21);
    snprintf(directory, sizeof(directory), "%s/%X",
             CRAZYPOD_COVER_CACHE_DIRECTORY, shard);
    if(create_directory)
        mkdir(directory);
    snprintf(path, path_size, "%s/%X/%08lX.RGB",
             CRAZYPOD_COVER_CACHE_DIRECTORY, shard,
             (unsigned long)filename_key);
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

static void cover_pack_close_data(void)
{
    if(cover_pack_data_fd >= 0) {
        close(cover_pack_data_fd);
        cover_pack_data_fd = -1;
    }
}

static unsigned cover_pack_hash_slot(uint32_t key_a, uint32_t key_b)
{
    uint32_t mixed = key_a ^ (key_b + 0x9e3779b9u +
                              (key_a << 6) + (key_a >> 2));

    mixed ^= mixed >> 16;
    return mixed & (CRAZYPOD_COVER_PACK_HASH_SIZE - 1);
}

static void cover_pack_hash_build(void)
{
    int i;

    for(i = 0; i < CRAZYPOD_COVER_PACK_HASH_SIZE; ++i)
        cover_pack_hash[i] = -1;
    for(i = 0; i < cover_pack_entry_count; ++i) {
        unsigned slot = cover_pack_hash_slot(
            cover_pack_entries[i].key_a, cover_pack_entries[i].key_b);

        while(cover_pack_hash[slot] >= 0)
            slot = (slot + 1) & (CRAZYPOD_COVER_PACK_HASH_SIZE - 1);
        cover_pack_hash[slot] = i;
    }
}

static int cover_pack_lookup(uint32_t key_a, uint32_t key_b)
{
    unsigned slot = cover_pack_hash_slot(key_a, key_b);
    unsigned probes;

    for(probes = 0; probes < CRAZYPOD_COVER_PACK_HASH_SIZE; ++probes) {
        int index = cover_pack_hash[slot];

        if(index < 0)
            return -1;
        if(cover_pack_entries[index].key_a == key_a &&
           cover_pack_entries[index].key_b == key_b)
            return index;
        slot = (slot + 1) & (CRAZYPOD_COVER_PACK_HASH_SIZE - 1);
    }
    return -1;
}

static bool cover_pack_entry_valid(
    const struct cover_pack_entry *entry, uint32_t data_size)
{
    if(entry->state == CRAZYPOD_COVER_PACK_EMPTY)
        return entry->data_size == 0;
    if(entry->state != CRAZYPOD_COVER_PACK_IMAGE ||
       entry->width == 0 || entry->height == 0 ||
       entry->width > CRAZYPOD_ARTWORK_MAX_SIZE ||
       entry->height > CRAZYPOD_ARTWORK_MAX_SIZE ||
       entry->data_size !=
           (uint32_t)entry->width * entry->height * sizeof(fb_data))
        return false;
    return entry->offset <= data_size &&
           entry->data_size <= data_size - entry->offset;
}

static bool cover_pack_load_index(void)
{
    struct cover_pack_header header;
    off_t data_file_size;
    int index_fd;
    int data_fd;
    int i;

    cover_pack_close_data();
    cover_pack_entry_count = 0;
    cover_pack_data_size = 0;
    cover_pack_hash_build();
    index_fd = open(CRAZYPOD_COVER_PACK_INDEX, O_RDONLY);
    if(index_fd < 0)
        return false;
    if(!read_exact(index_fd, &header, sizeof(header)) ||
       header.magic != CRAZYPOD_COVER_PACK_MAGIC ||
       header.version != CRAZYPOD_COVER_PACK_VERSION ||
       header.entry_count > CRAZYPOD_MAX_TRACKS ||
       header.entry_size != sizeof(struct cover_pack_entry) ||
       header.artwork_size != CRAZYPOD_COVERFLOW_ARTWORK_SIZE ||
       !read_exact(index_fd, cover_pack_entries,
                   header.entry_count * sizeof(cover_pack_entries[0]))) {
        close(index_fd);
        return false;
    }
    close(index_fd);

    data_fd = open(CRAZYPOD_COVER_PACK_DATA, O_RDONLY);
    if(data_fd < 0)
        return false;
    data_file_size = lseek(data_fd, 0, SEEK_END);
    if(data_file_size < 0 ||
       (uint64_t)data_file_size < header.data_size) {
        close(data_fd);
        return false;
    }
    for(i = 0; i < (int)header.entry_count; ++i) {
        if(!cover_pack_entry_valid(&cover_pack_entries[i],
                                   header.data_size)) {
            close(data_fd);
            return false;
        }
    }
    cover_pack_entry_count = header.entry_count;
    cover_pack_data_size = header.data_size;
    cover_pack_data_fd = data_fd;
    cover_pack_hash_build();
    return true;
}

static enum artwork_cache_result cover_pack_load(
    const struct artwork_decode_request *request, fb_data *pixels,
    lv_image_dsc_t *descriptor)
{
    struct artwork_decode_request canonical_request;
    struct cover_pack_entry *entry;
    fb_data *source_pixels;
    uint32_t key_a;
    uint32_t key_b;
    int output_width;
    int output_height;
    int index;

    if(request->target_size <= 0 ||
       request->target_size > CRAZYPOD_COVERFLOW_ARTWORK_SIZE)
        return ARTWORK_CACHE_MISS;
    canonical_request = *request;
    canonical_request.target_size = CRAZYPOD_COVERFLOW_ARTWORK_SIZE;
    artwork_cache_keys(&canonical_request, &key_a, &key_b);
    index = cover_pack_lookup(key_a, key_b);
    if(index < 0)
        return ARTWORK_CACHE_MISS;
    entry = &cover_pack_entries[index];
    if(entry->state == CRAZYPOD_COVER_PACK_EMPTY)
        return ARTWORK_CACHE_EMPTY;
    if(cover_pack_data_fd < 0) {
        cover_pack_data_fd =
            open(CRAZYPOD_COVER_PACK_DATA, O_RDONLY);
        if(cover_pack_data_fd < 0)
            return ARTWORK_CACHE_MISS;
    }
    source_pixels = request->target_size ==
        CRAZYPOD_COVERFLOW_ARTWORK_SIZE
            ? pixels : (fb_data *)decode_buffer;
    if(lseek(cover_pack_data_fd, entry->offset, SEEK_SET) < 0 ||
       !read_exact(cover_pack_data_fd, source_pixels, entry->data_size))
        return ARTWORK_CACHE_MISS;
    if(request->target_size == CRAZYPOD_COVERFLOW_ARTWORK_SIZE) {
        crazypod_image_configure_rgb565(
            descriptor, pixels, entry->width, entry->height);
        return ARTWORK_CACHE_IMAGE;
    }

    output_width = entry->width;
    output_height = entry->height;
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
           source_pixels, entry->width, entry->height, entry->width,
           pixels, output_width, output_height))
        return ARTWORK_CACHE_MISS;
    crazypod_image_configure_rgb565(
        descriptor, pixels, output_width, output_height);
    return ARTWORK_CACHE_IMAGE;
}

static bool cover_pack_begin(void)
{
    off_t end;
    int flags = O_RDWR | O_CREAT;

    cover_pack_close_data();
    if(cover_pack_entry_count == 0)
        flags |= O_TRUNC;
    cover_pack_data_fd =
        open(CRAZYPOD_COVER_PACK_DATA, flags, 0666);
    if(cover_pack_data_fd < 0)
        return false;
    end = lseek(cover_pack_data_fd, 0, SEEK_END);
    if(end < 0 || (uint64_t)end > UINT32_MAX) {
        cover_pack_close_data();
        return false;
    }
    cover_pack_build_count = 0;
    cover_pack_build_data_size = (uint32_t)end;
    cover_pack_building = true;
    return true;
}

static bool cover_pack_contains(
    const struct artwork_decode_request *request)
{
    uint32_t key_a;
    uint32_t key_b;

    artwork_cache_keys(request, &key_a, &key_b);
    return cover_pack_lookup(key_a, key_b) >= 0;
}

static bool cover_pack_append(
    const struct artwork_decode_request *request,
    const lv_image_dsc_t *descriptor)
{
    struct cover_pack_entry *entry;
    uint32_t key_a;
    uint32_t key_b;
    int existing;

    if(!cover_pack_building ||
       cover_pack_build_count >= CRAZYPOD_MAX_TRACKS)
        return false;
    artwork_cache_keys(request, &key_a, &key_b);
    entry = &cover_pack_build_entries[cover_pack_build_count];
    memset(entry, 0, sizeof(*entry));
    existing = cover_pack_lookup(key_a, key_b);
    if(existing >= 0) {
        *entry = cover_pack_entries[existing];
        ++cover_pack_build_count;
        return true;
    }

    entry->key_a = key_a;
    entry->key_b = key_b;
    if(descriptor == NULL) {
        entry->state = CRAZYPOD_COVER_PACK_EMPTY;
        ++cover_pack_build_count;
        return true;
    }
    if(descriptor->header.cf != LV_COLOR_FORMAT_RGB565 ||
       descriptor->header.w == 0 || descriptor->header.h == 0 ||
       descriptor->data_size >
           UINT32_MAX - cover_pack_build_data_size ||
       lseek(cover_pack_data_fd, cover_pack_build_data_size,
             SEEK_SET) < 0 ||
       !write_exact(cover_pack_data_fd, descriptor->data,
                    descriptor->data_size))
        return false;
    entry->offset = cover_pack_build_data_size;
    entry->data_size = descriptor->data_size;
    entry->width = descriptor->header.w;
    entry->height = descriptor->header.h;
    entry->state = CRAZYPOD_COVER_PACK_IMAGE;
    cover_pack_build_data_size += descriptor->data_size;
    ++cover_pack_build_count;
    return true;
}

static bool cover_pack_finish(void)
{
    struct cover_pack_header header;
    int index_fd;

    if(!cover_pack_building || cover_pack_data_fd < 0)
        return false;
    if(fsync(cover_pack_data_fd) < 0)
        return false;
    memset(&header, 0, sizeof(header));
    header.magic = CRAZYPOD_COVER_PACK_MAGIC;
    header.version = CRAZYPOD_COVER_PACK_VERSION;
    header.entry_count = cover_pack_build_count;
    header.entry_size = sizeof(struct cover_pack_entry);
    header.artwork_size = CRAZYPOD_COVERFLOW_ARTWORK_SIZE;
    header.data_size = cover_pack_build_data_size;

    remove(CRAZYPOD_COVER_PACK_INDEX_TMP);
    index_fd = open(CRAZYPOD_COVER_PACK_INDEX_TMP,
                    O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(index_fd < 0)
        return false;
    if(!write_exact(index_fd, &header, sizeof(header)) ||
       !write_exact(index_fd, cover_pack_build_entries,
                    cover_pack_build_count *
                    sizeof(cover_pack_build_entries[0])) ||
       fsync(index_fd) < 0) {
        close(index_fd);
        remove(CRAZYPOD_COVER_PACK_INDEX_TMP);
        return false;
    }
    close(index_fd);
    if(rename(CRAZYPOD_COVER_PACK_INDEX_TMP,
              CRAZYPOD_COVER_PACK_INDEX) < 0) {
        remove(CRAZYPOD_COVER_PACK_INDEX_TMP);
        return false;
    }
    cover_pack_building = false;
    return cover_pack_load_index();
}

static void cover_pack_abort(void)
{
    cover_pack_building = false;
    cover_pack_build_count = 0;
    cover_pack_close_data();
    (void)cover_pack_load_index();
}

static enum artwork_cache_result artwork_cache_load(
    const struct artwork_decode_request *request, fb_data *pixels,
    lv_image_dsc_t *descriptor)
{
    struct artwork_cache_header header;
    char path[MAX_PATH];
    uint32_t key_a;
    uint32_t key_b;
    int fd;

    artwork_cache_path(request, path, sizeof(path), &key_a, &key_b,
                       false);
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return ARTWORK_CACHE_MISS;
    if(!read_exact(fd, &header, sizeof(header)) ||
       header.magic != CRAZYPOD_CACHE_MAGIC ||
       header.version != CRAZYPOD_CACHE_VERSION ||
       header.key_a != key_a || header.key_b != key_b ||
       header.requested_size != request->target_size) {
        close(fd);
        return ARTWORK_CACHE_MISS;
    }
    if(header.width == 0 || header.height == 0 ||
       header.data_size == 0) {
        close(fd);
        return ARTWORK_CACHE_EMPTY;
    }
    if(header.width > CRAZYPOD_ARTWORK_MAX_SIZE ||
       header.height > CRAZYPOD_ARTWORK_MAX_SIZE ||
       header.data_size !=
           (uint32_t)header.width * header.height * sizeof(fb_data) ||
       !read_exact(fd, pixels, header.data_size)) {
        close(fd);
        return ARTWORK_CACHE_MISS;
    }
    close(fd);
    crazypod_image_configure_rgb565(
        descriptor, pixels, header.width, header.height);
    return ARTWORK_CACHE_IMAGE;
}

static void artwork_cache_store(
    const struct artwork_decode_request *request,
    const lv_image_dsc_t *descriptor)
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
    header.requested_size = request->target_size;
    if(descriptor != NULL) {
        header.width = descriptor->header.w;
        header.height = descriptor->header.h;
        header.data_size = descriptor->data_size;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return;
    complete = write_exact(fd, &header, sizeof(header));
    if(complete && descriptor != NULL)
        complete = write_exact(fd, descriptor->data,
                               descriptor->data_size);
    if(complete)
        fsync(fd);
    close(fd);
}

static bool decode_artwork(const struct artwork_decode_request *request,
                           lv_image_dsc_t *descriptor)
{
    struct mp3entry metadata;
    struct bitmap bitmap;
    struct dim dimensions;
    char discovered_path[MAX_PATH];
    const char *artwork_path = "";
    int fd;
    int result = -1;
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;

    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = request->target_size;
    bitmap.height = request->target_size;
    bitmap.data = decode_buffer;

    memset(&metadata, 0, sizeof(metadata));
    snprintf(metadata.path, sizeof(metadata.path),
             "%s", request->track_path);
    metadata.album = (char *)request->album;
    metadata.artist = (char *)request->artist;
    dimensions.width = request->target_size;
    dimensions.height = request->target_size;
    if(find_albumart(&metadata, discovered_path,
                     sizeof(discovered_path), &dimensions))
        artwork_path = discovered_path;

    if(artwork_path[0] != '\0') {
        crazypod_image_decode_lock();
        if(is_jpeg(artwork_path))
            result = read_jpeg_file(artwork_path, &bitmap,
                                    sizeof(decode_buffer), format,
                                    &format_native);
        else
            result = read_bmp_file(artwork_path, &bitmap,
                                   sizeof(decode_buffer), format,
                                   &format_native);
        crazypod_image_decode_unlock();
    }
    else if(request->embedded) {
        fd = open(request->track_path, O_RDONLY);
        if(fd < 0)
            return false;
        crazypod_image_decode_lock();
        if(lseek(fd, request->offset, SEEK_SET) >= 0) {
            if(request->type == AA_TYPE_JPG)
                result = clip_jpeg_fd(fd, 0, request->size,
                                      &bitmap, sizeof(decode_buffer),
                                      format, &format_native);
            else if(request->type == AA_TYPE_BMP)
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
        request->embedded = slot->requested_artwork_embedded;
        request->cache_only = slot->requested_cache_only;
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

static bool prime_request(int *album_index,
                          struct artwork_decode_request *request)
{
    const struct crazypod_track *track;

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

    memset(request, 0, sizeof(*request));
    track = crazypod_music_album_track(*album_index, 0);
    if(track == NULL)
        return true;
    request->target_size = CRAZYPOD_COVERFLOW_ARTWORK_SIZE;
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

static void finish_prime_request(int album_index, bool completed)
{
    mutex_lock(&artwork_mutex);
    if(completed && artwork_prime_active &&
       artwork_prime_next == album_index) {
        ++artwork_prime_next;
        artwork_prime_completed = artwork_prime_next;
        if(artwork_prime_next >= artwork_prime_total)
            artwork_prime_active = false;
    }
    artwork_prime_processing = false;
    mutex_unlock(&artwork_mutex);
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
            enum artwork_cache_result cache_result;
            unsigned serial;
            int slot_index;
            int bank;
            bool valid;

            if(find_pending_request(&slot_index, &serial, &bank,
                                    &request)) {
                if(!begin_disk_work())
                    break;
                memset(&decoded_descriptor, 0,
                       sizeof(decoded_descriptor));
                memset(&published_descriptor, 0,
                       sizeof(published_descriptor));
                cache_result = cover_pack_load(
                    &request, artwork_slots[slot_index].pixels[bank],
                    &published_descriptor);
                if(cache_result == ARTWORK_CACHE_MISS &&
                   !request.cache_only) {
                    cache_result = artwork_cache_load(
                        &request,
                        artwork_slots[slot_index].pixels[bank],
                        &published_descriptor);
                }
                valid = cache_result == ARTWORK_CACHE_IMAGE;

                if(cache_result == ARTWORK_CACHE_MISS &&
                   !request.cache_only &&
                   request_is_current(slot_index, serial)) {
                    valid = decode_artwork(&request,
                                           &decoded_descriptor);
                    if(valid) {
                        copy_decoded_pixels(
                            &decoded_descriptor,
                            artwork_slots[slot_index].pixels[bank],
                            &published_descriptor);
                        artwork_cache_store(&request,
                                            &published_descriptor);
                    }
                    else {
                        artwork_cache_store(&request, NULL);
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
                    slot->decoded_cache_miss =
                        request.cache_only &&
                        cache_result == ARTWORK_CACHE_MISS;
                    slot->valid = valid;
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
                    finish_prime_request(album_index, false);
                    break;
                }
                if(!cover_pack_building)
                    completed = cover_pack_begin();
                if(completed && request.track_path[0] != '\0') {
                    if(cover_pack_contains(&request)) {
                        completed =
                            cover_pack_append(&request, NULL);
                    }
                    else {
                        memset(&decoded_descriptor, 0,
                               sizeof(decoded_descriptor));
                        cache_result = artwork_cache_load(
                            &request, (fb_data *)decode_buffer,
                            &decoded_descriptor);
                        valid =
                            cache_result == ARTWORK_CACHE_IMAGE;
                        if(cache_result == ARTWORK_CACHE_MISS)
                            valid = decode_artwork(
                                &request, &decoded_descriptor);
                        completed = cover_pack_append(
                            &request,
                            valid ? &decoded_descriptor : NULL);
                    }
                }
                last_album =
                    album_index + 1 >= crazypod_music_album_count();
                if(completed && last_album)
                    completed = cover_pack_finish();
                if(!completed)
                    cover_pack_abort();
                end_disk_work();
                if(completed)
                    finish_prime_request(album_index, true);
                else {
                    fail_prime_request();
                    break;
                }
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
        CRAZYPOD_COVERFLOW_ARTWORK_SIZE;
    artwork_slots[CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT].pixels[0] =
        now_pixels[1][0];
    artwork_slots[CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT].pixels[1] =
        now_pixels[1][1];
    artwork_slots[CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT].capacity_size =
        CRAZYPOD_COVERFLOW_ARTWORK_SIZE;
    artwork_slots[CRAZYPOD_CAPSULE_ARTWORK_SLOT].pixels[0] =
        capsule_pixels[0];
    artwork_slots[CRAZYPOD_CAPSULE_ARTWORK_SLOT].pixels[1] =
        capsule_pixels[1];
    artwork_slots[CRAZYPOD_CAPSULE_ARTWORK_SLOT].capacity_size =
        CRAZYPOD_CAPSULE_ARTWORK_SIZE;
    mkdir(CRAZYPOD_DIRECTORY);
    mkdir(CRAZYPOD_CACHE_DIRECTORY);
    mkdir(CRAZYPOD_COVER_CACHE_DIRECTORY);
    mkdir(CRAZYPOD_COVER_PACK_DIRECTORY);
    for(i = 0; i < CRAZYPOD_COVER_CACHE_SHARDS; ++i) {
        snprintf(shard_directory, sizeof(shard_directory), "%s/%X",
                 CRAZYPOD_COVER_CACHE_DIRECTORY, i);
        mkdir(shard_directory);
    }
    artwork_publish_generation = 0;
    artwork_worker_decoding = false;
    artwork_suspended = false;
    artwork_wake_queued = false;
    artwork_prime_active = false;
    artwork_prime_processing = false;
    artwork_prime_failed = false;
    artwork_prime_next = 0;
    artwork_prime_completed = 0;
    artwork_prime_total = 0;
    cover_pack_building = false;
    cover_pack_build_count = 0;
    cover_pack_entry_count = 0;
    cover_pack_data_size = 0;
    cover_pack_data_fd = -1;
    (void)cover_pack_load_index();
    mutex_init(&artwork_mutex);
    queue_init(&artwork_queue, false);
    create_thread(artwork_thread, artwork_stack, sizeof(artwork_stack), 0,
                  "crazypod art"
                  IF_PRIO(, PRIORITY_USER_INTERFACE)
                  IF_COP(, CPU));
}

void crazypod_artwork_prime_library(void)
{
    int total = crazypod_music_album_count();

    crazypod_artwork_cancel_library_prime();
    mutex_lock(&artwork_mutex);
    artwork_prime_next = 0;
    artwork_prime_completed = 0;
    artwork_prime_total = total;
    artwork_prime_failed = false;
    artwork_prime_active = total > 0;
    if(artwork_prime_active)
        artwork_wake_queued = true;
    mutex_unlock(&artwork_mutex);
    if(total > 0)
        queue_post(&artwork_queue, CRAZYPOD_ARTWORK_WAKE, 0);
}

void crazypod_artwork_cancel_library_prime(void)
{
    bool processing;

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
    if(cover_pack_building)
        cover_pack_abort();
    mutex_lock(&artwork_mutex);
    artwork_prime_next = 0;
    artwork_prime_completed = 0;
    artwork_prime_total = 0;
    artwork_prime_failed = false;
    mutex_unlock(&artwork_mutex);
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

void crazypod_artwork_suspend(void)
{
    bool busy;

    mutex_lock(&artwork_mutex);
    artwork_suspended = true;
    mutex_unlock(&artwork_mutex);
    do {
        mutex_lock(&artwork_mutex);
        busy = artwork_worker_decoding || artwork_prime_processing;
        mutex_unlock(&artwork_mutex);
        if(busy)
            yield();
    } while(busy);
    cover_pack_close_data();
}

void crazypod_artwork_resume(void)
{
    (void)cover_pack_load_index();
    mutex_lock(&artwork_mutex);
    artwork_suspended = false;
    artwork_wake_queued = true;
    mutex_unlock(&artwork_mutex);
    queue_post(&artwork_queue, CRAZYPOD_ARTWORK_WAKE, 0);
}

static const lv_image_dsc_t *artwork_load_priority(
    int slot_index, const struct crazypod_track *track, int target_size,
    int priority, bool cache_only)
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
       (!slot->decoded_cache_only || cache_only) &&
       !(cache_only && slot->decoded_cache_miss)) {
        if(slot->valid)
            result = &slot->descriptor[slot->active_bank];
    }
    else if(slot->requested_size != target_size ||
            strcmp(slot->requested_path, track->path) != 0 ||
            slot->requested_cache_only != cache_only ||
            (cache_only && slot->decoded_cache_miss &&
             slot->decoded_serial == slot->request_serial)) {
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
                                 priority, false);
}

const lv_image_dsc_t *crazypod_artwork_load_cached_priority(
    int slot_index, const struct crazypod_track *track, int target_size,
    int priority)
{
    return artwork_load_priority(slot_index, track, target_size,
                                 priority, true);
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
        state = slot->valid
            ? CRAZYPOD_ARTWORK_IMAGE : CRAZYPOD_ARTWORK_EMPTY;
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
           artwork_prime_active ||
           artwork_prime_processing;
    for(i = 0; !busy && i < CRAZYPOD_ARTWORK_SLOTS; ++i) {
        if(artwork_slots[i].request_serial !=
           artwork_slots[i].decoded_serial)
            busy = true;
    }
    mutex_unlock(&artwork_mutex);
    return busy;
}

#endif
