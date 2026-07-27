#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "bmp.h"
#include "core_alloc.h"
#include "jpeg_load.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "crazypod_book_cover.h"
#include "crazypod_books.h"
#include "crazypod_image.h"

#define BOOK_COVER_SLOTS 4
#define BOOK_COVER_WIDTH 72
#define BOOK_COVER_HEIGHT 101
#define BOOK_COVER_DECODE_EXTRA (64 * 1024)

struct book_cover_slot {
    uint32_t key;
    bool valid;
    lv_image_dsc_t descriptor;
    fb_data pixels[BOOK_COVER_WIDTH * BOOK_COVER_HEIGHT];
};

static struct book_cover_slot cover_slots[BOOK_COVER_SLOTS];
static int next_cover_slot;

static uint32_t path_hash(const char *path)
{
    uint32_t hash = 2166136261u;

    while(*path != '\0') {
        hash ^= (unsigned char)*path++;
        hash *= 16777619u;
    }
    return hash;
}

static int ascii_lower(int value)
{
    return value >= 'A' && value <= 'Z'
        ? value - 'A' + 'a' : value;
}

static bool extension_is(const char *path, const char *wanted)
{
    const char *dot = strrchr(path, '.');

    if(dot == NULL)
        return false;
    while(*dot != '\0' && *wanted != '\0') {
        if(ascii_lower((unsigned char)*dot) !=
           ascii_lower((unsigned char)*wanted))
            return false;
        ++dot;
        ++wanted;
    }
    return *dot == '\0' && *wanted == '\0';
}

static bool decode_cover(const char *path, struct book_cover_slot *slot,
                         int max_width, int max_height)
{
    struct bitmap bitmap;
    fb_data *decode_buffer;
    size_t pixel_bytes =
        BOOK_COVER_WIDTH * BOOK_COVER_HEIGHT * sizeof(fb_data);
    size_t decode_bytes = pixel_bytes + BOOK_COVER_DECODE_EXTRA;
    int decode_handle;
    int result;
    int row;

    if(!extension_is(path, ".jpg") &&
       !extension_is(path, ".jpeg") &&
       !extension_is(path, ".bmp"))
        return false;
    decode_handle = core_alloc_ex(
        decode_bytes, &buflib_ops_locked);
    if(decode_handle < 0)
        return false;
    decode_buffer = core_get_data(decode_handle);
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = max_width;
    bitmap.height = max_height;
    bitmap.data = (unsigned char *)decode_buffer;

    crazypod_image_decode_lock();
    if(extension_is(path, ".jpg") ||
       extension_is(path, ".jpeg"))
        result = read_jpeg_file(
            path, &bitmap, decode_bytes,
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
            &format_native);
    else
        result = read_bmp_file(
            path, &bitmap, decode_bytes,
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
            &format_native);
    crazypod_image_decode_unlock();

    if(result < 0 || bitmap.width <= 0 || bitmap.height <= 0 ||
       bitmap.width > BOOK_COVER_WIDTH ||
       bitmap.height > BOOK_COVER_HEIGHT ||
       bitmap.data == NULL) {
        core_free(decode_handle);
        return false;
    }
    for(row = 0; row < bitmap.height; ++row) {
        memcpy(slot->pixels + row * bitmap.width,
               (fb_data *)bitmap.data + row * bitmap.width,
               (size_t)bitmap.width * sizeof(fb_data));
    }
    core_free(decode_handle);
    return crazypod_image_configure_rgb565(
        &slot->descriptor, slot->pixels,
        bitmap.width, bitmap.height);
}

const lv_image_dsc_t *crazypod_book_cover_get(
    int book_index, int max_width, int max_height)
{
    const struct crazypod_book *book;
    uint32_t key;
    struct book_cover_slot *slot;
    int i;

    if(max_width <= 0 || max_width > BOOK_COVER_WIDTH ||
       max_height <= 0 || max_height > BOOK_COVER_HEIGHT ||
       !crazypod_book_probe(book_index))
        return NULL;
    book = crazypod_book_get(book_index);
    if(book == NULL || book->cover_path[0] == '\0')
        return NULL;

    key = path_hash(book->cover_path) ^
        book->size ^ (book->mtime * 16777619u) ^
        ((uint32_t)max_width << 16) ^ (uint32_t)max_height;
    for(i = 0; i < BOOK_COVER_SLOTS; ++i) {
        if(cover_slots[i].valid && cover_slots[i].key == key)
            return &cover_slots[i].descriptor;
    }

    slot = &cover_slots[next_cover_slot];
    next_cover_slot =
        (next_cover_slot + 1) % BOOK_COVER_SLOTS;
    if(slot->valid)
        lv_image_cache_drop(&slot->descriptor);
    slot->valid = false;
    if(!decode_cover(
           book->cover_path, slot, max_width, max_height))
        return NULL;
    slot->key = key;
    slot->valid = true;
    return &slot->descriptor;
}

void crazypod_book_cover_reset(void)
{
    int i;

    for(i = 0; i < BOOK_COVER_SLOTS; ++i) {
        if(cover_slots[i].valid)
            lv_image_cache_drop(&cover_slots[i].descriptor);
    }
    memset(cover_slots, 0, sizeof(cover_slots));
    next_cover_slot = 0;
}

#endif
