#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "dir.h"
#include "file.h"

#include "../crazypod_image.h"
#include "../crazypod_photos.h"
#include "crazypod_photo_cache.h"
#include "crazypod_photo_catalog.h"

#define CACHE_DIRECTORY "/.crazypod/cache"
#define THUMB_PATH CACHE_DIRECTORY "/photos.thm"
#define VIEW_PATH CACHE_DIRECTORY "/photos.view"
#define THUMB_MAGIC 0x43505431u
#define VIEW_MAGIC 0x43505631u
#define CACHE_VERSION 1
#define THUMB_ENTRIES 768
#define VIEW_ENTRIES 512
#define THUMB_MAX_BYTES (8u * 1024u * 1024u)
#define VIEW_MAX_BYTES (160u * 1024u * 1024u)

struct disk_header {
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

struct cache_entry {
    uint32_t key;
    uint32_t source_size;
    uint32_t source_mtime;
    uint32_t data_offset;
    uint16_t width;
    uint16_t height;
    uint32_t data_size;
};

struct cache_store {
    struct cache_entry *entries;
    int count;
    int capacity;
    uint32_t size;
    uint32_t max_size;
    uint32_t magic;
    int max_width;
    int max_height;
    const char *path;
    bool reset_when_full;
};

static struct cache_entry thumb_entries[THUMB_ENTRIES];
static struct cache_entry view_entries[VIEW_ENTRIES];
static struct cache_store thumb_store = {
    .entries = thumb_entries,
    .capacity = THUMB_ENTRIES,
    .max_size = THUMB_MAX_BYTES,
    .magic = THUMB_MAGIC,
    .max_width = CRAZYPOD_PHOTO_THUMB_SIZE,
    .max_height = CRAZYPOD_PHOTO_THUMB_SIZE,
    .path = THUMB_PATH,
    .reset_when_full = true,
};
static struct cache_store view_store = {
    .entries = view_entries,
    .capacity = VIEW_ENTRIES,
    .max_size = VIEW_MAX_BYTES,
    .magic = VIEW_MAGIC,
    .max_width = CRAZYPOD_PHOTO_VIEW_WIDTH,
    .max_height = CRAZYPOD_PHOTO_VIEW_HEIGHT,
    .path = VIEW_PATH,
};

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

static struct cache_store *store_for(bool view)
{
    return view ? &view_store : &thumb_store;
}

static void rebuild(struct cache_store *store)
{
    struct disk_header header;
    off_t valid_size = 0;
    int fd;

    store->count = 0;
    store->size = 0;
    fd = open(store->path, O_RDWR);
    if(fd < 0)
        return;
    while(store->count < store->capacity) {
        off_t header_offset = lseek(fd, 0, SEEK_CUR);
        off_t data_offset;
        uint32_t expected_size;
        struct cache_entry *entry;

        if(header_offset < 0 ||
           !read_exact(fd, &header, sizeof(header)))
            break;
        expected_size =
            (uint32_t)header.width * header.height * sizeof(fb_data);
        if(header.magic != store->magic ||
           header.version != CACHE_VERSION ||
           header.header_size != sizeof(header) ||
           header.width == 0 || header.height == 0 ||
           header.width > store->max_width ||
           header.height > store->max_height ||
           header.data_size != expected_size)
            break;
        data_offset = lseek(fd, 0, SEEK_CUR);
        if(data_offset < 0 ||
           lseek(fd, header.data_size, SEEK_CUR) < 0)
            break;
        valid_size = data_offset + header.data_size;
        entry = &store->entries[store->count++];
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
    store->size =
        valid_size <= 0 ? 0 :
        (uint64_t)valid_size > UINT32_MAX ? UINT32_MAX :
        (uint32_t)valid_size;
}

void crazypod_photo_cache_init(void)
{
    mkdir("/.crazypod");
    mkdir(CACHE_DIRECTORY);
    rebuild(&thumb_store);
    rebuild(&view_store);
}

void crazypod_photo_cache_invalidate(void)
{
    remove(THUMB_PATH);
    remove(VIEW_PATH);
    thumb_store.count = 0;
    thumb_store.size = 0;
    view_store.count = 0;
    view_store.size = 0;
}

bool crazypod_photo_cache_load(
    bool view, const char *path, uint32_t source_size,
    uint32_t source_mtime, lv_image_dsc_t *descriptor,
    fb_data *pixels)
{
    struct cache_store *store = store_for(view);
    uint32_t key = crazypod_photo_catalog_key(path);
    int index;

    for(index = store->count - 1; index >= 0; --index) {
        const struct cache_entry *entry = &store->entries[index];
        int fd;

        if(entry->key != key ||
           entry->source_size != source_size ||
           entry->source_mtime != source_mtime)
            continue;
        fd = open(store->path, O_RDONLY);
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

void crazypod_photo_cache_store(
    bool view, const char *path, uint32_t source_size,
    uint32_t source_mtime, const lv_image_dsc_t *descriptor)
{
    struct cache_store *store = store_for(view);
    struct disk_header header;
    struct cache_entry *entry;
    off_t header_offset;
    int fd;
    bool complete;

    if(descriptor == NULL || descriptor->data == NULL ||
       descriptor->header.w <= 0 || descriptor->header.h <= 0)
        return;
    if(store->count >= store->capacity ||
       store->size + sizeof(header) + descriptor->data_size >
           store->max_size) {
        if(!store->reset_when_full)
            return;
        remove(store->path);
        store->count = 0;
        store->size = 0;
    }
    memset(&header, 0, sizeof(header));
    header.magic = store->magic;
    header.version = CACHE_VERSION;
    header.header_size = sizeof(header);
    header.key = crazypod_photo_catalog_key(path);
    header.source_size = source_size;
    header.source_mtime = source_mtime;
    header.width = descriptor->header.w;
    header.height = descriptor->header.h;
    header.data_size = descriptor->data_size;
    fd = open(store->path, O_RDWR | O_CREAT, 0666);
    if(fd < 0)
        return;
    header_offset = lseek(fd, 0, SEEK_END);
    complete = header_offset >= 0 &&
        write_exact(fd, &header, sizeof(header)) &&
        write_exact(fd, descriptor->data, descriptor->data_size) &&
        fsync(fd) >= 0;
    if(!complete) {
        if(header_offset >= 0)
            ftruncate(fd, header_offset);
        close(fd);
        return;
    }
    close(fd);
    entry = &store->entries[store->count++];
    entry->key = header.key;
    entry->source_size = source_size;
    entry->source_mtime = source_mtime;
    entry->data_offset = header_offset + sizeof(header);
    entry->width = header.width;
    entry->height = header.height;
    entry->data_size = header.data_size;
    store->size = entry->data_offset + entry->data_size;
}

#endif
