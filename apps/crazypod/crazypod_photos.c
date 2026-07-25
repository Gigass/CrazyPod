#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bmp.h"
#include "buflib.h"
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "jpeg_load.h"
#include "kernel.h"
#include "lcd.h"
#include "resize.h"
#include "string-extra.h"
#include "system.h"

#include "src/misc/cache/instance/lv_image_cache.h"

#include "crazypod_image.h"
#include "crazypod_photos.h"

#define CRAZYPOD_PHOTO_DIRECTORY "/Pictures"
#define CRAZYPOD_PHOTO_DIRECTORY_DEPTH 4
#define CRAZYPOD_PHOTO_THREAD_STACK_SIZE 0x4000
#define CRAZYPOD_PHOTO_WAKE 1
#define CRAZYPOD_PHOTO_DECODE_EXTRA (64 * 1024)
#define CRAZYPOD_PHOTO_DIRECTORY_PATH "/.crazypod"
#define CRAZYPOD_PHOTO_CACHE_DIRECTORY "/.crazypod/cache"
#define CRAZYPOD_PHOTO_THUMB_CACHE_PATH \
    CRAZYPOD_PHOTO_CACHE_DIRECTORY "/photos.thm"
#define CRAZYPOD_PHOTO_THUMB_CACHE_MAGIC 0x43505431u
#define CRAZYPOD_PHOTO_THUMB_CACHE_VERSION 1
#define CRAZYPOD_PHOTO_THUMB_CACHE_ENTRIES 768
#define CRAZYPOD_PHOTO_THUMB_CACHE_MAX_BYTES (8 * 1024 * 1024)
#define CRAZYPOD_PHOTO_VIEW_CACHE_PATH \
    CRAZYPOD_PHOTO_CACHE_DIRECTORY "/photos.view"
#define CRAZYPOD_PHOTO_VIEW_CACHE_MAGIC 0x43505631u
#define CRAZYPOD_PHOTO_VIEW_CACHE_VERSION 1
#define CRAZYPOD_PHOTO_VIEW_CACHE_ENTRIES 512
#define CRAZYPOD_PHOTO_VIEW_CACHE_MAX_BYTES (160 * 1024 * 1024)
#define CRAZYPOD_PHOTO_FAVORITES_PATH \
    CRAZYPOD_PHOTO_DIRECTORY_PATH "/photo-favorites.cfg"
#define CRAZYPOD_PHOTO_FAVORITES_TMP \
    CRAZYPOD_PHOTO_DIRECTORY_PATH "/photo-favorites.tmp"

struct photo_entry {
    char path[MAX_PATH];
    uint32_t size;
    uint32_t mtime;
    uint32_t key;
    bool favorite;
};

struct photo_slot {
    fb_data pixels[2][CRAZYPOD_PHOTO_THUMB_SIZE *
                      CRAZYPOD_PHOTO_THUMB_SIZE]
        CACHEALIGN_AT_LEAST_ATTR(16);
    lv_image_dsc_t descriptor[2];
    char requested_path[MAX_PATH];
    uint32_t requested_size;
    uint32_t requested_mtime;
    unsigned request_serial;
    unsigned decoded_serial;
    int requested_index;
    int decoded_index;
    int active_bank;
    bool pending;
    bool valid;
};

struct photo_view_slot {
    fb_data pixels[2][CRAZYPOD_PHOTO_VIEW_WIDTH *
                      CRAZYPOD_PHOTO_VIEW_HEIGHT]
        CACHEALIGN_AT_LEAST_ATTR(16);
    lv_image_dsc_t descriptor[2];
    char requested_path[MAX_PATH];
    uint32_t requested_size;
    uint32_t requested_mtime;
    unsigned request_serial;
    unsigned decoded_serial;
    int requested_index;
    int decoded_index;
    int active_bank;
    int progress;
    bool pending;
    bool valid;
};

struct photo_decode_request {
    char path[MAX_PATH];
    uint32_t size;
    uint32_t mtime;
    unsigned serial;
    int index;
    int slot;
    int width;
    int height;
    bool view;
};

struct photo_thumb_cache_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t key;
    uint32_t source_size;
    uint32_t source_mtime;
    uint16_t width;
    uint16_t height;
    uint32_t data_size;
};

struct photo_thumb_cache_entry {
    uint32_t key;
    uint32_t source_size;
    uint32_t source_mtime;
    uint32_t data_offset;
    uint16_t width;
    uint16_t height;
    uint32_t data_size;
};

static struct photo_entry photos[CRAZYPOD_PHOTO_MAX_FILES];
static struct photo_slot thumbnail_slots[CRAZYPOD_PHOTO_THUMB_SLOTS];
static struct photo_view_slot view_slot;
static fb_data photo_viewport_pixels[
    CRAZYPOD_PHOTO_VIEWPORT_WIDTH *
    CRAZYPOD_PHOTO_VIEWPORT_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t photo_viewport_descriptor;
static const uint8_t *photo_viewport_source;
static const uint8_t *photo_crop_preview_source;
static int photo_crop_preview_image_y;
static int photo_viewport_index = -1;
static int photo_viewport_zoom_percent;
static int photo_viewport_pan_x;
static int photo_viewport_pan_y;
static int photo_count;
static int favorite_count;
static unsigned photo_publish_generation;
static unsigned photo_view_publish_generation;
static bool photo_worker_decoding;
static bool photo_suspended;
static bool photo_wake_queued;
static struct photo_thumb_cache_entry
    photo_thumb_cache[CRAZYPOD_PHOTO_THUMB_CACHE_ENTRIES];
static int photo_thumb_cache_count;
static uint32_t photo_thumb_cache_size;
static struct photo_thumb_cache_entry
    photo_view_cache[CRAZYPOD_PHOTO_VIEW_CACHE_ENTRIES];
static int photo_view_cache_count;
static uint32_t photo_view_cache_size;
static struct mutex photo_mutex;
static struct event_queue photo_queue;
static long photo_stack[CRAZYPOD_PHOTO_THREAD_STACK_SIZE /
                        sizeof(long)];

static bool write_exact(int fd, const void *data, size_t size)
{
    const uint8_t *cursor = data;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);

        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool read_exact(int fd, void *data, size_t size)
{
    uint8_t *cursor = data;

    while(size > 0) {
        ssize_t count = read(fd, cursor, size);

        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static uint32_t hash_path(const char *path)
{
    uint32_t hash = 2166136261u;

    while(path != NULL && *path != '\0') {
        hash ^= (uint8_t)*path++;
        hash *= 16777619u;
    }
    return hash;
}

static const char *path_basename(const char *path)
{
    const char *name;

    if(path == NULL)
        return "";
    name = strrchr(path, '/');
    return name != NULL ? name + 1 : path;
}

static bool path_supported(const char *path)
{
    const char *extension = path != NULL ? strrchr(path, '.') : NULL;

    return extension != NULL &&
        (strcasecmp(extension, ".jpg") == 0 ||
         strcasecmp(extension, ".jpeg") == 0 ||
         strcasecmp(extension, ".bmp") == 0);
}

static bool directory_supported(const char *name)
{
    const char *extension = name != NULL ? strrchr(name, '.') : NULL;
    size_t length = name != NULL ? strlen(name) : 0;

    if(name == NULL || name[0] == '.')
        return false;
    if(extension != NULL &&
       (strcasecmp(extension, ".photoslibrary") == 0 ||
        strcasecmp(extension, ".photolibrary") == 0 ||
        strcasecmp(extension, ".app") == 0))
        return false;
    return length < 6 ||
        strcasecmp(name + length - 6, "_files") != 0;
}

static bool append_path(char *destination, size_t size,
                        const char *directory, const char *name)
{
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);

    if(directory_length + 1 + name_length >= size)
        return false;
    memcpy(destination, directory, directory_length);
    destination[directory_length] = '/';
    memcpy(destination + directory_length + 1, name, name_length + 1);
    return true;
}

static int compare_photo_entries(const struct photo_entry *left,
                                 const struct photo_entry *right)
{
    if(left->mtime != right->mtime)
        return left->mtime > right->mtime ? -1 : 1;
    return strcasecmp(left->path, right->path);
}

static void insert_photo(const char *path, const struct dirinfo *info)
{
    struct photo_entry entry;
    int position;

    if(photo_count >= CRAZYPOD_PHOTO_MAX_FILES)
        return;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.path, sizeof(entry.path), "%s", path);
    entry.size = info->size <= 0 ? 0 :
        (uint64_t)info->size > UINT32_MAX ? UINT32_MAX :
        (uint32_t)info->size;
    entry.mtime = info->mtime <= 0 ? 0 :
        (uint64_t)info->mtime > UINT32_MAX ? UINT32_MAX :
        (uint32_t)info->mtime;
    entry.key = hash_path(entry.path);
    position = photo_count;
    while(position > 0 &&
          compare_photo_entries(&entry, &photos[position - 1]) < 0) {
        photos[position] = photos[position - 1];
        --position;
    }
    photos[position] = entry;
    ++photo_count;
}

static void scan_directory(const char *path, int depth)
{
    DIR *directory;
    struct DIRENT *entry;

    if(depth > CRAZYPOD_PHOTO_DIRECTORY_DEPTH ||
       photo_count >= CRAZYPOD_PHOTO_MAX_FILES)
        return;
    directory = opendir(path);
    if(directory == NULL)
        return;
    while(photo_count < CRAZYPOD_PHOTO_MAX_FILES &&
          (entry = readdir(directory)) != NULL) {
        struct dirinfo info;
        char child[MAX_PATH];

        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0 ||
           entry->d_name[0] == '.')
            continue;
        if(!append_path(child, sizeof(child), path, entry->d_name))
            continue;
        info = dir_get_info(directory, entry);
        if(info.attribute & ATTR_DIRECTORY) {
            if(directory_supported(entry->d_name))
                scan_directory(child, depth + 1);
        }
        else if(path_supported(child))
            insert_photo(child, &info);
        if((photo_count & 15) == 0)
            yield();
    }
    closedir(directory);
}

static void load_favorites(void)
{
    char line[MAX_PATH];
    int fd;
    int used = 0;

    favorite_count = 0;
    fd = open(CRAZYPOD_PHOTO_FAVORITES_PATH, O_RDONLY);
    if(fd < 0)
        return;
    while(true) {
        char character;
        ssize_t count = read(fd, &character, 1);

        if(count <= 0)
            character = '\n';
        if(character == '\r')
            continue;
        if(character != '\n' && used < MAX_PATH - 1) {
            line[used++] = character;
            continue;
        }
        if(used > 0) {
            int index;

            line[used] = '\0';
            for(index = 0; index < photo_count; ++index) {
                if(strcmp(photos[index].path, line) == 0) {
                    if(!photos[index].favorite) {
                        photos[index].favorite = true;
                        ++favorite_count;
                    }
                    break;
                }
            }
        }
        used = 0;
        if(count <= 0)
            break;
    }
    close(fd);
}

static bool save_favorites(void)
{
    bool complete = true;
    int fd;
    int index;

    mkdir(CRAZYPOD_PHOTO_DIRECTORY_PATH);
    fd = open(CRAZYPOD_PHOTO_FAVORITES_TMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    for(index = 0; index < photo_count; ++index) {
        if(!photos[index].favorite)
            continue;
        complete =
            write_exact(fd, photos[index].path,
                        strlen(photos[index].path)) &&
            write_exact(fd, "\n", 1);
        if(!complete)
            break;
    }
    if(complete)
        complete = fsync(fd) >= 0;
    close(fd);
    if(!complete) {
        remove(CRAZYPOD_PHOTO_FAVORITES_TMP);
        return false;
    }
    if(rename(CRAZYPOD_PHOTO_FAVORITES_TMP,
              CRAZYPOD_PHOTO_FAVORITES_PATH) < 0) {
        remove(CRAZYPOD_PHOTO_FAVORITES_TMP);
        return false;
    }
    return true;
}

static void rebuild_thumb_cache_index(void)
{
    struct photo_thumb_cache_header header;
    off_t valid_size = 0;
    int fd;

    photo_thumb_cache_count = 0;
    photo_thumb_cache_size = 0;
    fd = open(CRAZYPOD_PHOTO_THUMB_CACHE_PATH, O_RDWR);
    if(fd < 0)
        return;
    while(photo_thumb_cache_count <
          CRAZYPOD_PHOTO_THUMB_CACHE_ENTRIES) {
        off_t header_offset = lseek(fd, 0, SEEK_CUR);
        off_t data_offset;
        uint32_t expected_size;
        struct photo_thumb_cache_entry *entry;

        if(header_offset < 0 ||
           !read_exact(fd, &header, sizeof(header)))
            break;
        expected_size =
            (uint32_t)header.width * header.height *
            sizeof(fb_data);
        if(header.magic != CRAZYPOD_PHOTO_THUMB_CACHE_MAGIC ||
           header.version != CRAZYPOD_PHOTO_THUMB_CACHE_VERSION ||
           header.header_size != sizeof(header) ||
           header.width == 0 ||
           header.height == 0 ||
           header.width > CRAZYPOD_PHOTO_THUMB_SIZE ||
           header.height > CRAZYPOD_PHOTO_THUMB_SIZE ||
           header.data_size != expected_size)
            break;
        data_offset = lseek(fd, 0, SEEK_CUR);
        if(data_offset < 0 ||
           lseek(fd, header.data_size, SEEK_CUR) < 0)
            break;
        valid_size = data_offset + header.data_size;
        entry =
            &photo_thumb_cache[photo_thumb_cache_count++];
        entry->key = header.key;
        entry->source_size = header.source_size;
        entry->source_mtime = header.source_mtime;
        entry->data_offset = data_offset;
        entry->width = header.width;
        entry->height = header.height;
        entry->data_size = header.data_size;
    }
    if(filesize(fd) != valid_size)
        ftruncate(fd, valid_size);
    close(fd);
    photo_thumb_cache_size =
        valid_size <= 0 ? 0 :
        (uint64_t)valid_size > UINT32_MAX ? UINT32_MAX :
        (uint32_t)valid_size;
}

static bool load_cached_thumbnail(
    const struct photo_decode_request *request,
    lv_image_dsc_t *descriptor, fb_data *pixels)
{
    int index;

    for(index = photo_thumb_cache_count - 1;
        index >= 0; --index) {
        const struct photo_thumb_cache_entry *entry =
            &photo_thumb_cache[index];
        int fd;

        if(entry->key != hash_path(request->path) ||
           entry->source_size != request->size ||
           entry->source_mtime != request->mtime)
            continue;
        fd = open(CRAZYPOD_PHOTO_THUMB_CACHE_PATH, O_RDONLY);
        if(fd < 0)
            return false;
        if(lseek(fd, entry->data_offset, SEEK_SET) < 0 ||
           !read_exact(fd, pixels, entry->data_size)) {
            close(fd);
            return false;
        }
        close(fd);
        return crazypod_image_configure_rgb565(
            descriptor, pixels, entry->width, entry->height);
    }
    return false;
}

static void reset_thumb_cache(void)
{
    remove(CRAZYPOD_PHOTO_THUMB_CACHE_PATH);
    photo_thumb_cache_count = 0;
    photo_thumb_cache_size = 0;
}

static void store_cached_thumbnail(
    const struct photo_decode_request *request,
    const lv_image_dsc_t *descriptor)
{
    struct photo_thumb_cache_header header;
    struct photo_thumb_cache_entry *entry;
    off_t header_offset;
    int fd;
    bool complete;

    if(descriptor == NULL || descriptor->data == NULL ||
       descriptor->header.w <= 0 || descriptor->header.h <= 0)
        return;
    if(photo_thumb_cache_count >=
           CRAZYPOD_PHOTO_THUMB_CACHE_ENTRIES ||
       photo_thumb_cache_size + sizeof(header) +
           descriptor->data_size >
           CRAZYPOD_PHOTO_THUMB_CACHE_MAX_BYTES)
        reset_thumb_cache();
    memset(&header, 0, sizeof(header));
    header.magic = CRAZYPOD_PHOTO_THUMB_CACHE_MAGIC;
    header.version = CRAZYPOD_PHOTO_THUMB_CACHE_VERSION;
    header.header_size = sizeof(header);
    header.key = hash_path(request->path);
    header.source_size = request->size;
    header.source_mtime = request->mtime;
    header.width = descriptor->header.w;
    header.height = descriptor->header.h;
    header.data_size = descriptor->data_size;
    fd = open(CRAZYPOD_PHOTO_THUMB_CACHE_PATH,
              O_RDWR | O_CREAT, 0666);
    if(fd < 0)
        return;
    header_offset = lseek(fd, 0, SEEK_END);
    complete = header_offset >= 0 &&
        write_exact(fd, &header, sizeof(header)) &&
        write_exact(fd, descriptor->data,
                    descriptor->data_size) &&
        fsync(fd) >= 0;
    if(!complete) {
        if(header_offset >= 0)
            ftruncate(fd, header_offset);
        close(fd);
        return;
    }
    close(fd);
    entry =
        &photo_thumb_cache[photo_thumb_cache_count++];
    entry->key = header.key;
    entry->source_size = header.source_size;
    entry->source_mtime = header.source_mtime;
    entry->data_offset = header_offset + sizeof(header);
    entry->width = header.width;
    entry->height = header.height;
    entry->data_size = header.data_size;
    photo_thumb_cache_size =
        entry->data_offset + entry->data_size;
}

static void rebuild_view_cache_index(void)
{
    struct photo_thumb_cache_header header;
    off_t valid_size = 0;
    int fd;

    photo_view_cache_count = 0;
    photo_view_cache_size = 0;
    fd = open(CRAZYPOD_PHOTO_VIEW_CACHE_PATH, O_RDWR);
    if(fd < 0)
        return;
    while(photo_view_cache_count <
          CRAZYPOD_PHOTO_VIEW_CACHE_ENTRIES) {
        off_t header_offset = lseek(fd, 0, SEEK_CUR);
        off_t data_offset;
        uint32_t expected_size;
        struct photo_thumb_cache_entry *entry;

        if(header_offset < 0 ||
           !read_exact(fd, &header, sizeof(header)))
            break;
        expected_size =
            (uint32_t)header.width * header.height *
            sizeof(fb_data);
        if(header.magic != CRAZYPOD_PHOTO_VIEW_CACHE_MAGIC ||
           header.version != CRAZYPOD_PHOTO_VIEW_CACHE_VERSION ||
           header.header_size != sizeof(header) ||
           header.width == 0 ||
           header.height == 0 ||
           header.width > CRAZYPOD_PHOTO_VIEW_WIDTH ||
           header.height > CRAZYPOD_PHOTO_VIEW_HEIGHT ||
           header.data_size != expected_size)
            break;
        data_offset = lseek(fd, 0, SEEK_CUR);
        if(data_offset < 0 ||
           lseek(fd, header.data_size, SEEK_CUR) < 0)
            break;
        valid_size = data_offset + header.data_size;
        entry = &photo_view_cache[photo_view_cache_count++];
        entry->key = header.key;
        entry->source_size = header.source_size;
        entry->source_mtime = header.source_mtime;
        entry->data_offset = data_offset;
        entry->width = header.width;
        entry->height = header.height;
        entry->data_size = header.data_size;
    }
    if(filesize(fd) != valid_size)
        ftruncate(fd, valid_size);
    close(fd);
    photo_view_cache_size =
        valid_size <= 0 ? 0 :
        (uint64_t)valid_size > UINT32_MAX ? UINT32_MAX :
        (uint32_t)valid_size;
}

static bool load_cached_view(
    const struct photo_decode_request *request,
    lv_image_dsc_t *descriptor, fb_data *pixels)
{
    int index;

    for(index = photo_view_cache_count - 1;
        index >= 0; --index) {
        const struct photo_thumb_cache_entry *entry =
            &photo_view_cache[index];
        int fd;

        if(entry->key != hash_path(request->path) ||
           entry->source_size != request->size ||
           entry->source_mtime != request->mtime)
            continue;
        fd = open(CRAZYPOD_PHOTO_VIEW_CACHE_PATH, O_RDONLY);
        if(fd < 0)
            return false;
        if(lseek(fd, entry->data_offset, SEEK_SET) < 0 ||
           !read_exact(fd, pixels, entry->data_size)) {
            close(fd);
            return false;
        }
        close(fd);
        return crazypod_image_configure_rgb565(
            descriptor, pixels, entry->width, entry->height);
    }
    return false;
}

static void store_cached_view(
    const struct photo_decode_request *request,
    const lv_image_dsc_t *descriptor)
{
    struct photo_thumb_cache_header header;
    struct photo_thumb_cache_entry *entry;
    off_t header_offset;
    int fd;
    bool complete;

    if(descriptor == NULL || descriptor->data == NULL ||
       descriptor->header.w <= 0 || descriptor->header.h <= 0 ||
       photo_view_cache_count >=
           CRAZYPOD_PHOTO_VIEW_CACHE_ENTRIES ||
       photo_view_cache_size + sizeof(header) +
           descriptor->data_size >
           CRAZYPOD_PHOTO_VIEW_CACHE_MAX_BYTES)
        return;
    memset(&header, 0, sizeof(header));
    header.magic = CRAZYPOD_PHOTO_VIEW_CACHE_MAGIC;
    header.version = CRAZYPOD_PHOTO_VIEW_CACHE_VERSION;
    header.header_size = sizeof(header);
    header.key = hash_path(request->path);
    header.source_size = request->size;
    header.source_mtime = request->mtime;
    header.width = descriptor->header.w;
    header.height = descriptor->header.h;
    header.data_size = descriptor->data_size;
    fd = open(CRAZYPOD_PHOTO_VIEW_CACHE_PATH,
              O_RDWR | O_CREAT, 0666);
    if(fd < 0)
        return;
    header_offset = lseek(fd, 0, SEEK_END);
    complete = header_offset >= 0 &&
        write_exact(fd, &header, sizeof(header)) &&
        write_exact(fd, descriptor->data,
                    descriptor->data_size) &&
        fsync(fd) >= 0;
    if(!complete) {
        if(header_offset >= 0)
            ftruncate(fd, header_offset);
        close(fd);
        return;
    }
    close(fd);
    entry = &photo_view_cache[photo_view_cache_count++];
    entry->key = header.key;
    entry->source_size = header.source_size;
    entry->source_mtime = header.source_mtime;
    entry->data_offset = header_offset + sizeof(header);
    entry->width = header.width;
    entry->height = header.height;
    entry->data_size = header.data_size;
    photo_view_cache_size =
        entry->data_offset + entry->data_size;
}

static bool decode_photo(const struct photo_decode_request *request,
                         lv_image_dsc_t *descriptor,
                         fb_data *destination)
{
    struct bitmap bitmap;
    fb_data *decode_buffer;
    size_t pixel_bytes =
        (size_t)request->width * request->height * sizeof(fb_data);
    size_t decode_bytes = pixel_bytes + CRAZYPOD_PHOTO_DECODE_EXTRA;
    int decode_handle;
    int result;
    int row;

    decode_handle = core_alloc_ex(decode_bytes, &buflib_ops_locked);
    if(decode_handle < 0)
        return false;
    decode_buffer = core_get_data(decode_handle);
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = request->width;
    bitmap.height = request->height;
    bitmap.data = (unsigned char *)decode_buffer;
    crazypod_image_decode_lock();
    if(path_supported(request->path) &&
       (strcasecmp(strrchr(request->path, '.'), ".jpg") == 0 ||
        strcasecmp(strrchr(request->path, '.'), ".jpeg") == 0)) {
        result = read_jpeg_file(
            request->path, &bitmap, decode_bytes,
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
            &format_native);
    }
    else {
        result = read_bmp_file(
            request->path, &bitmap, decode_bytes,
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
            &format_native);
    }
    crazypod_image_decode_unlock();
    if(result < 0 || bitmap.width <= 0 || bitmap.height <= 0 ||
       bitmap.width > request->width ||
       bitmap.height > request->height || bitmap.data == NULL) {
        core_free(decode_handle);
        return false;
    }
    for(row = 0; row < bitmap.height; ++row) {
        memcpy(destination + row * bitmap.width,
               (fb_data *)bitmap.data + row * bitmap.width,
               (size_t)bitmap.width * sizeof(fb_data));
    }
    core_free(decode_handle);
    return crazypod_image_configure_rgb565(
        descriptor, destination, bitmap.width, bitmap.height);
}

static bool find_request(struct photo_decode_request *request)
{
    int slot;

    memset(request, 0, sizeof(*request));
    mutex_lock(&photo_mutex);
    if(photo_suspended) {
        photo_wake_queued = false;
        mutex_unlock(&photo_mutex);
        return false;
    }
    if(view_slot.pending) {
        request->view = true;
        request->index = view_slot.requested_index;
        request->serial = view_slot.request_serial;
        request->size = view_slot.requested_size;
        request->mtime = view_slot.requested_mtime;
        request->width = CRAZYPOD_PHOTO_VIEW_WIDTH;
        request->height = CRAZYPOD_PHOTO_VIEW_HEIGHT;
        snprintf(request->path, sizeof(request->path), "%s",
                 view_slot.requested_path);
        view_slot.pending = false;
        view_slot.progress = 25;
        photo_worker_decoding = true;
        mutex_unlock(&photo_mutex);
        return true;
    }
    for(slot = 0; slot < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot) {
        if(!thumbnail_slots[slot].pending)
            continue;
        request->slot = slot;
        request->index = thumbnail_slots[slot].requested_index;
        request->serial = thumbnail_slots[slot].request_serial;
        request->size = thumbnail_slots[slot].requested_size;
        request->mtime = thumbnail_slots[slot].requested_mtime;
        request->width = CRAZYPOD_PHOTO_THUMB_SIZE;
        request->height = CRAZYPOD_PHOTO_THUMB_SIZE;
        snprintf(request->path, sizeof(request->path), "%s",
                 thumbnail_slots[slot].requested_path);
        thumbnail_slots[slot].pending = false;
        photo_worker_decoding = true;
        mutex_unlock(&photo_mutex);
        return true;
    }
    photo_worker_decoding = false;
    photo_wake_queued = false;
    mutex_unlock(&photo_mutex);
    return false;
}

static void publish_request(const struct photo_decode_request *request,
                            int bank, bool valid)
{
    mutex_lock(&photo_mutex);
    if(request->view) {
        if(view_slot.request_serial == request->serial &&
           view_slot.requested_size == request->size &&
           view_slot.requested_mtime == request->mtime &&
           strcmp(view_slot.requested_path, request->path) == 0) {
            view_slot.active_bank = bank;
            view_slot.decoded_index = request->index;
            view_slot.decoded_serial = request->serial;
            view_slot.valid = valid;
            view_slot.progress = valid ? 100 : -1;
            ++photo_publish_generation;
            ++photo_view_publish_generation;
        }
    }
    else if(request->slot >= 0 &&
            request->slot < CRAZYPOD_PHOTO_THUMB_SLOTS) {
        struct photo_slot *slot = &thumbnail_slots[request->slot];

        if(slot->request_serial == request->serial &&
           slot->requested_size == request->size &&
           slot->requested_mtime == request->mtime &&
           strcmp(slot->requested_path, request->path) == 0) {
            slot->active_bank = bank;
            slot->decoded_index = request->index;
            slot->decoded_serial = request->serial;
            slot->valid = valid;
            ++photo_publish_generation;
        }
    }
    mutex_unlock(&photo_mutex);
}

static void update_view_progress(
    const struct photo_decode_request *request, int progress)
{
    if(request == NULL || !request->view)
        return;
    mutex_lock(&photo_mutex);
    if(view_slot.request_serial == request->serial &&
       view_slot.requested_index == request->index &&
       strcmp(view_slot.requested_path, request->path) == 0)
        view_slot.progress = progress;
    mutex_unlock(&photo_mutex);
}

static void finish_request(void)
{
    mutex_lock(&photo_mutex);
    photo_worker_decoding = false;
    mutex_unlock(&photo_mutex);
}

static void photo_thread(void)
{
    struct queue_event event;

    while(true) {
        queue_wait(&photo_queue, &event);
        if(event.id != CRAZYPOD_PHOTO_WAKE)
            continue;
        while(true) {
            struct photo_decode_request request;
            lv_image_dsc_t *descriptor;
            fb_data *pixels;
            int bank;
            bool cache_hit = false;
            bool valid;

            if(!find_request(&request))
                break;
            if(request.view) {
                bank = 1 - view_slot.active_bank;
                descriptor = &view_slot.descriptor[bank];
                pixels = view_slot.pixels[bank];
            }
            else {
                struct photo_slot *slot =
                    &thumbnail_slots[request.slot];

                bank = 1 - slot->active_bank;
                descriptor = &slot->descriptor[bank];
                pixels = slot->pixels[bank];
            }
            if(request.view)
                update_view_progress(&request, 35);
            if(request.view)
                cache_hit = load_cached_view(
                    &request, descriptor, pixels);
            else
                cache_hit = load_cached_thumbnail(
                    &request, descriptor, pixels);
            if(request.view && !cache_hit)
                update_view_progress(&request, 45);
            valid = cache_hit ||
                decode_photo(&request, descriptor, pixels);
            if(request.view && valid)
                update_view_progress(&request, 90);
            publish_request(&request, bank, valid);
            if(valid && request.view && !cache_hit)
                store_cached_view(&request, descriptor);
            else if(valid && !request.view && !cache_hit)
                store_cached_thumbnail(&request, descriptor);
            finish_request();
            yield();
        }
    }
}

static void wake_worker(void)
{
    bool post = false;

    mutex_lock(&photo_mutex);
    if(!photo_suspended && !photo_wake_queued) {
        photo_wake_queued = true;
        post = true;
    }
    mutex_unlock(&photo_mutex);
    if(post)
        queue_post(&photo_queue, CRAZYPOD_PHOTO_WAKE, 0);
}

void crazypod_photos_init(void)
{
    int slot;

    memset(photos, 0, sizeof(photos));
    memset(thumbnail_slots, 0, sizeof(thumbnail_slots));
    memset(&view_slot, 0, sizeof(view_slot));
    for(slot = 0; slot < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot) {
        thumbnail_slots[slot].requested_index = -1;
        thumbnail_slots[slot].decoded_index = -1;
    }
    view_slot.requested_index = -1;
    view_slot.decoded_index = -1;
    mutex_init(&photo_mutex);
    queue_init(&photo_queue, false);
    create_thread(photo_thread, photo_stack, sizeof(photo_stack), 0,
                  "crazypod photos"
                  IF_PRIO(, PRIORITY_USER_INTERFACE)
                  IF_COP(, CPU));
    mkdir(CRAZYPOD_PHOTO_DIRECTORY);
    mkdir(CRAZYPOD_PHOTO_DIRECTORY_PATH);
    mkdir(CRAZYPOD_PHOTO_CACHE_DIRECTORY);
    rebuild_thumb_cache_index();
    rebuild_view_cache_index();
    crazypod_photos_refresh();
}

void crazypod_photos_refresh(void)
{
    mutex_lock(&photo_mutex);
    photo_suspended = true;
    mutex_unlock(&photo_mutex);
    while(photo_worker_decoding)
        yield();
    photo_count = 0;
    favorite_count = 0;
    memset(photos, 0, sizeof(photos));
    scan_directory(CRAZYPOD_PHOTO_DIRECTORY, 0);
    load_favorites();
    mutex_lock(&photo_mutex);
    memset(thumbnail_slots, 0, sizeof(thumbnail_slots));
    memset(&view_slot, 0, sizeof(view_slot));
    view_slot.requested_index = -1;
    view_slot.decoded_index = -1;
    photo_suspended = false;
    photo_wake_queued = false;
    ++photo_publish_generation;
    ++photo_view_publish_generation;
    photo_viewport_index = -1;
    photo_viewport_source = NULL;
    photo_crop_preview_source = NULL;
    mutex_unlock(&photo_mutex);
}

void crazypod_photos_suspend(void)
{
    mutex_lock(&photo_mutex);
    photo_suspended = true;
    mutex_unlock(&photo_mutex);
    while(photo_worker_decoding)
        yield();
}

void crazypod_photos_resume(void)
{
    mutex_lock(&photo_mutex);
    photo_suspended = false;
    photo_wake_queued = false;
    mutex_unlock(&photo_mutex);
    crazypod_photos_refresh();
}

int crazypod_photo_count(void)
{
    return photo_count;
}

int crazypod_photo_favorite_count(void)
{
    return favorite_count;
}

int crazypod_photo_favorite_index(int favorite_index)
{
    int index;

    if(favorite_index < 0)
        return -1;
    for(index = 0; index < photo_count; ++index) {
        if(photos[index].favorite && favorite_index-- == 0)
            return index;
    }
    return -1;
}

const char *crazypod_photo_path(int index)
{
    return index >= 0 && index < photo_count ? photos[index].path : "";
}

const char *crazypod_photo_name(int index)
{
    return path_basename(crazypod_photo_path(index));
}

bool crazypod_photo_is_favorite(int index)
{
    return index >= 0 && index < photo_count && photos[index].favorite;
}

bool crazypod_photo_toggle_favorite(int index)
{
    bool previous;

    if(index < 0 || index >= photo_count)
        return false;
    previous = photos[index].favorite;
    photos[index].favorite = !previous;
    favorite_count += previous ? -1 : 1;
    if(!save_favorites()) {
        photos[index].favorite = previous;
        favorite_count += previous ? 1 : -1;
        return false;
    }
    ++photo_publish_generation;
    return true;
}

const lv_image_dsc_t *crazypod_photo_thumbnail(int slot_index, int index)
{
    struct photo_slot *slot;
    const lv_image_dsc_t *result = NULL;
    bool changed = false;

    if(slot_index < 0 || slot_index >= CRAZYPOD_PHOTO_THUMB_SLOTS ||
       index < 0 || index >= photo_count)
        return NULL;
    mutex_lock(&photo_mutex);
    slot = &thumbnail_slots[slot_index];
    if(slot->decoded_index == index &&
       slot->decoded_serial == slot->request_serial &&
       slot->valid &&
       strcmp(slot->requested_path, photos[index].path) == 0) {
        result = &slot->descriptor[slot->active_bank];
    }
    else if(slot->requested_index != index ||
            slot->requested_size != photos[index].size ||
            slot->requested_mtime != photos[index].mtime ||
            strcmp(slot->requested_path, photos[index].path) != 0) {
        slot->requested_index = index;
        slot->requested_size = photos[index].size;
        slot->requested_mtime = photos[index].mtime;
        snprintf(slot->requested_path, sizeof(slot->requested_path),
                 "%s", photos[index].path);
        ++slot->request_serial;
        slot->pending = true;
        slot->valid = false;
        changed = true;
    }
    mutex_unlock(&photo_mutex);
    if(changed)
        wake_worker();
    return result;
}

const lv_image_dsc_t *crazypod_photo_view(int index)
{
    const lv_image_dsc_t *result = NULL;
    bool changed = false;

    if(index < 0 || index >= photo_count)
        return NULL;
    mutex_lock(&photo_mutex);
    if(view_slot.decoded_index == index &&
       view_slot.decoded_serial == view_slot.request_serial &&
       view_slot.valid &&
       strcmp(view_slot.requested_path, photos[index].path) == 0) {
        result = &view_slot.descriptor[view_slot.active_bank];
    }
    else if(view_slot.requested_index != index ||
            view_slot.requested_size != photos[index].size ||
            view_slot.requested_mtime != photos[index].mtime ||
            strcmp(view_slot.requested_path, photos[index].path) != 0) {
        view_slot.requested_index = index;
        view_slot.requested_size = photos[index].size;
        view_slot.requested_mtime = photos[index].mtime;
        snprintf(view_slot.requested_path,
                 sizeof(view_slot.requested_path), "%s",
                 photos[index].path);
        ++view_slot.request_serial;
        view_slot.pending = true;
        view_slot.valid = false;
        view_slot.progress = 10;
        changed = true;
    }
    mutex_unlock(&photo_mutex);
    if(changed)
        wake_worker();
    return result;
}

int crazypod_photo_view_progress(int index)
{
    int progress;

    if(index < 0 || index >= photo_count)
        return -1;
    mutex_lock(&photo_mutex);
    if(view_slot.requested_index != index ||
       strcmp(view_slot.requested_path, photos[index].path) != 0)
        progress = 0;
    else
        progress = view_slot.progress;
    mutex_unlock(&photo_mutex);
    return progress;
}

static const lv_image_dsc_t *ready_thumbnail(int index)
{
    const lv_image_dsc_t *result = NULL;
    int slot_index;

    if(index < 0 || index >= photo_count)
        return NULL;
    mutex_lock(&photo_mutex);
    for(slot_index = 0;
        slot_index < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot_index) {
        struct photo_slot *slot = &thumbnail_slots[slot_index];

        if(slot->decoded_index == index && slot->valid &&
           strcmp(slot->requested_path, photos[index].path) == 0) {
            result = &slot->descriptor[slot->active_bank];
            break;
        }
    }
    mutex_unlock(&photo_mutex);
    return result;
}

static fb_data sample_rgb565_bilinear(
    const fb_data *source, int width, int height,
    int x_q8, int y_q8)
{
    int x0 = x_q8 >> 8;
    int y0 = y_q8 >> 8;
    int x1;
    int y1;
    int fx;
    int fy;
    fb_data p00;
    fb_data p10;
    fb_data p01;
    fb_data p11;
    unsigned red0;
    unsigned red1;
    unsigned green0;
    unsigned green1;
    unsigned blue0;
    unsigned blue1;

    if(x0 < 0)
        x0 = 0;
    if(y0 < 0)
        y0 = 0;
    if(x0 >= width)
        x0 = width - 1;
    if(y0 >= height)
        y0 = height - 1;
    x1 = x0 + 1 < width ? x0 + 1 : x0;
    y1 = y0 + 1 < height ? y0 + 1 : y0;
    fx = x_q8 & 255;
    fy = y_q8 & 255;
    p00 = source[y0 * width + x0];
    p10 = source[y0 * width + x1];
    p01 = source[y1 * width + x0];
    p11 = source[y1 * width + x1];
    red0 = RGB_UNPACK_RED(p00) * (256 - fx) +
        RGB_UNPACK_RED(p10) * fx;
    red1 = RGB_UNPACK_RED(p01) * (256 - fx) +
        RGB_UNPACK_RED(p11) * fx;
    green0 = RGB_UNPACK_GREEN(p00) * (256 - fx) +
        RGB_UNPACK_GREEN(p10) * fx;
    green1 = RGB_UNPACK_GREEN(p01) * (256 - fx) +
        RGB_UNPACK_GREEN(p11) * fx;
    blue0 = RGB_UNPACK_BLUE(p00) * (256 - fx) +
        RGB_UNPACK_BLUE(p10) * fx;
    blue1 = RGB_UNPACK_BLUE(p01) * (256 - fx) +
        RGB_UNPACK_BLUE(p11) * fx;
    return LCD_RGBPACK(
        (red0 * (256 - fy) + red1 * fy) >> 16,
        (green0 * (256 - fy) + green1 * fy) >> 16,
        (blue0 * (256 - fy) + blue1 * fy) >> 16);
}

const lv_image_dsc_t *crazypod_photo_render_viewport(
    int index, int zoom_percent, int *pan_x, int *pan_y)
{
    const lv_image_dsc_t *source_descriptor =
        crazypod_photo_view(index);
    const fb_data *source;
    uint32_t scale_x;
    uint32_t scale_y;
    uint32_t scale;
    int source_width;
    int source_height;
    int display_width;
    int display_height;
    int image_x;
    int image_y;
    int overflow_x;
    int overflow_y;
    int shift_x;
    int shift_y;
    int retained_x;
    int retained_y;
    int retained_width;
    int retained_height;
    bool incremental_pan;
    int y;

    if(pan_x == NULL || pan_y == NULL)
        return NULL;
    if(source_descriptor == NULL)
        source_descriptor = ready_thumbnail(index);
    if(source_descriptor == NULL)
        return NULL;
    source = (const fb_data *)source_descriptor->data;
    source_width = source_descriptor->header.w;
    source_height = source_descriptor->header.h;
    scale_x =
        (uint32_t)CRAZYPOD_PHOTO_VIEWPORT_WIDTH *
        LV_SCALE_NONE / source_width;
    scale_y =
        (uint32_t)CRAZYPOD_PHOTO_VIEWPORT_HEIGHT *
        LV_SCALE_NONE / source_height;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if(zoom_percent < 100)
        zoom_percent = 100;
    if(zoom_percent > 500)
        zoom_percent = 500;
    scale = scale * (uint32_t)zoom_percent / 100;
    if(scale == 0)
        scale = 1;
    display_width = source_width * scale / LV_SCALE_NONE;
    display_height = source_height * scale / LV_SCALE_NONE;
    overflow_x = display_width > CRAZYPOD_PHOTO_VIEWPORT_WIDTH
        ? (display_width - CRAZYPOD_PHOTO_VIEWPORT_WIDTH) / 2
        : 0;
    overflow_y = display_height > CRAZYPOD_PHOTO_VIEWPORT_HEIGHT
        ? (display_height - CRAZYPOD_PHOTO_VIEWPORT_HEIGHT) / 2
        : 0;
    if(zoom_percent == 100) {
        *pan_x = 0;
        *pan_y = 0;
    }
    if(*pan_x < -overflow_x)
        *pan_x = -overflow_x;
    if(*pan_x > overflow_x)
        *pan_x = overflow_x;
    if(*pan_y < -overflow_y)
        *pan_y = -overflow_y;
    if(*pan_y > overflow_y)
        *pan_y = overflow_y;
    if(photo_viewport_descriptor.header.magic ==
           LV_IMAGE_HEADER_MAGIC &&
       photo_viewport_source == source_descriptor->data &&
       photo_viewport_index == index &&
       photo_viewport_zoom_percent == zoom_percent &&
       photo_viewport_pan_x == *pan_x &&
       photo_viewport_pan_y == *pan_y)
        return &photo_viewport_descriptor;
    image_x =
        (CRAZYPOD_PHOTO_VIEWPORT_WIDTH - display_width) / 2 +
        *pan_x;
    image_y =
        (CRAZYPOD_PHOTO_VIEWPORT_HEIGHT - display_height) / 2 +
        *pan_y;
    shift_x = *pan_x - photo_viewport_pan_x;
    shift_y = *pan_y - photo_viewport_pan_y;
    incremental_pan =
        photo_viewport_descriptor.header.magic ==
            LV_IMAGE_HEADER_MAGIC &&
        photo_viewport_source == source_descriptor->data &&
        photo_viewport_index == index &&
        photo_viewport_zoom_percent == zoom_percent &&
        shift_x > -CRAZYPOD_PHOTO_VIEWPORT_WIDTH &&
        shift_x < CRAZYPOD_PHOTO_VIEWPORT_WIDTH &&
        shift_y > -CRAZYPOD_PHOTO_VIEWPORT_HEIGHT &&
        shift_y < CRAZYPOD_PHOTO_VIEWPORT_HEIGHT;
    if(photo_viewport_descriptor.header.magic ==
       LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&photo_viewport_descriptor);
    retained_x = shift_x > 0 ? shift_x : 0;
    retained_y = shift_y > 0 ? shift_y : 0;
    retained_width =
        CRAZYPOD_PHOTO_VIEWPORT_WIDTH -
        (shift_x < 0 ? -shift_x : shift_x);
    retained_height =
        CRAZYPOD_PHOTO_VIEWPORT_HEIGHT -
        (shift_y < 0 ? -shift_y : shift_y);
    if(incremental_pan) {
        int y_start;
        int y_end;
        int y_step;

        if(shift_y > 0) {
            y_start = CRAZYPOD_PHOTO_VIEWPORT_HEIGHT - 1;
            y_end = retained_y - 1;
            y_step = -1;
        }
        else {
            y_start = retained_y;
            y_end = retained_y + retained_height;
            y_step = 1;
        }
        for(y = y_start; y != y_end; y += y_step) {
            int source_y = y - shift_y;
            int source_x = shift_x < 0 ? -shift_x : 0;

            memmove(
                &photo_viewport_pixels[
                    y * CRAZYPOD_PHOTO_VIEWPORT_WIDTH +
                    retained_x],
                &photo_viewport_pixels[
                    source_y * CRAZYPOD_PHOTO_VIEWPORT_WIDTH +
                    source_x],
                retained_width * sizeof(fb_data));
        }
    }
    else {
        memset(photo_viewport_pixels, 0,
               sizeof(photo_viewport_pixels));
        retained_width = 0;
        retained_height = 0;
    }
    for(y = 0; y < CRAZYPOD_PHOTO_VIEWPORT_HEIGHT; ++y) {
        int display_y = y - image_y;
        int source_y_q8 = 0;
        int x;

        if(display_y >= 0 && display_y < display_height)
            source_y_q8 =
                display_y * source_height * 256 / display_height;
        for(x = 0; x < CRAZYPOD_PHOTO_VIEWPORT_WIDTH; ++x) {
            int display_x = x - image_x;
            int source_x_q8;

            if(incremental_pan &&
               x >= retained_x &&
               x < retained_x + retained_width &&
               y >= retained_y &&
               y < retained_y + retained_height)
                continue;
            photo_viewport_pixels[
                y * CRAZYPOD_PHOTO_VIEWPORT_WIDTH + x] = 0;

            if(display_y < 0 || display_y >= display_height ||
               display_x < 0 || display_x >= display_width)
                continue;
            source_x_q8 =
                display_x * source_width * 256 / display_width;
            photo_viewport_pixels[
                y * CRAZYPOD_PHOTO_VIEWPORT_WIDTH + x] =
                sample_rgb565_bilinear(
                    source, source_width, source_height,
                    source_x_q8, source_y_q8);
        }
    }
    crazypod_image_configure_rgb565(
        &photo_viewport_descriptor, photo_viewport_pixels,
        CRAZYPOD_PHOTO_VIEWPORT_WIDTH,
        CRAZYPOD_PHOTO_VIEWPORT_HEIGHT);
    photo_viewport_source = source_descriptor->data;
    photo_viewport_index = index;
    photo_viewport_zoom_percent = zoom_percent;
    photo_viewport_pan_x = *pan_x;
    photo_viewport_pan_y = *pan_y;
    return &photo_viewport_descriptor;
}

const lv_image_dsc_t *crazypod_photo_render_crop_preview(
    int index, int center_y)
{
    const lv_image_dsc_t *source_descriptor =
        crazypod_photo_view(index);
    const int preview_height = 168;
    const fb_data *source;
    int source_width;
    int source_height;
    int display_height;
    int image_y;
    int y;

    if(source_descriptor == NULL)
        return NULL;
    source = (const fb_data *)source_descriptor->data;
    source_width = source_descriptor->header.w;
    source_height = source_descriptor->header.h;
    if(source == NULL || source_width <= 0 || source_height <= 0)
        return NULL;
    display_height =
        source_height * CRAZYPOD_PHOTO_VIEWPORT_WIDTH /
        source_width;
    if(display_height < 1)
        display_height = 1;
    if(display_height <= preview_height)
        image_y = (preview_height - display_height) / 2;
    else {
        if(center_y < 0)
            center_y = source_height / 2;
        image_y = preview_height / 2 -
            center_y * display_height / source_height;
        if(image_y > 0)
            image_y = 0;
        if(image_y < preview_height - display_height)
            image_y = preview_height - display_height;
    }
    if(photo_viewport_descriptor.header.magic ==
           LV_IMAGE_HEADER_MAGIC &&
       photo_viewport_descriptor.header.w ==
           CRAZYPOD_PHOTO_VIEWPORT_WIDTH &&
       photo_viewport_descriptor.header.h == preview_height &&
       photo_crop_preview_source == source_descriptor->data &&
       photo_crop_preview_image_y == image_y)
        return &photo_viewport_descriptor;
    if(photo_viewport_descriptor.header.magic ==
       LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&photo_viewport_descriptor);
    memset(
        photo_viewport_pixels, 0,
        CRAZYPOD_PHOTO_VIEWPORT_WIDTH * preview_height *
        sizeof(fb_data));
    for(y = 0; y < preview_height; ++y) {
        int display_y = y - image_y;
        int source_y_q8 = 0;
        int x;

        if(display_y >= 0 && display_y < display_height)
            source_y_q8 =
                display_y * source_height * 256 /
                display_height;
        for(x = 0;
            x < CRAZYPOD_PHOTO_VIEWPORT_WIDTH; ++x) {
            int source_x_q8;

            if(display_y < 0 ||
               display_y >= display_height)
                continue;
            source_x_q8 =
                x * source_width * 256 /
                CRAZYPOD_PHOTO_VIEWPORT_WIDTH;
            photo_viewport_pixels[
                y * CRAZYPOD_PHOTO_VIEWPORT_WIDTH + x] =
                sample_rgb565_bilinear(
                    source, source_width, source_height,
                    source_x_q8, source_y_q8);
        }
    }
    crazypod_image_configure_rgb565(
        &photo_viewport_descriptor, photo_viewport_pixels,
        CRAZYPOD_PHOTO_VIEWPORT_WIDTH, preview_height);
    photo_crop_preview_source = source_descriptor->data;
    photo_crop_preview_image_y = image_y;
    photo_viewport_source = NULL;
    photo_viewport_index = -1;
    return &photo_viewport_descriptor;
}

unsigned crazypod_photo_generation(void)
{
    unsigned generation;

    mutex_lock(&photo_mutex);
    generation = photo_publish_generation;
    mutex_unlock(&photo_mutex);
    return generation;
}

unsigned crazypod_photo_view_generation(void)
{
    unsigned generation;

    mutex_lock(&photo_mutex);
    generation = photo_view_publish_generation;
    mutex_unlock(&photo_mutex);
    return generation;
}

bool crazypod_photos_busy(void)
{
    bool busy;
    int slot;

    mutex_lock(&photo_mutex);
    busy = photo_worker_decoding || view_slot.pending;
    for(slot = 0; !busy && slot < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot)
        busy = thumbnail_slots[slot].pending;
    mutex_unlock(&photo_mutex);
    return busy;
}

#endif
