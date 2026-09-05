#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "bmp.h"
#include "core_alloc.h"
#include "file.h"
#include "jpeg_load.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "crazypod_book_image.h"
#include "crazypod_book_png.h"
#include "crazypod_books.h"
#include "crazypod_image.h"

#define BOOK_IMAGE_WIDTH 304
#define BOOK_IMAGE_HEIGHT 204
#define BOOK_IMAGE_DECODE_EXTRA (64 * 1024)

struct book_image_slot {
    uint32_t key;
    bool valid;
    lv_image_dsc_t descriptor;
    fb_data pixels[BOOK_IMAGE_WIDTH * BOOK_IMAGE_HEIGHT];
};

static struct book_image_slot image_slot;

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

static bool decode_image(const char *path, struct book_image_slot *slot,
                         int max_width, int max_height)
{
    struct bitmap bitmap;
    struct crazypod_book_png_info png_info;
    fb_data *decode_buffer;
    size_t pixel_bytes =
        (size_t)BOOK_IMAGE_WIDTH * BOOK_IMAGE_HEIGHT * sizeof(fb_data);
    size_t decode_bytes = pixel_bytes + BOOK_IMAGE_DECODE_EXTRA;
    int decode_handle;
    int result;
    int row;
    bool is_png = extension_is(path, ".png");

    if(!extension_is(path, ".jpg") &&
       !extension_is(path, ".jpeg") &&
       !extension_is(path, ".bmp") && !is_png)
        return false;
    if(is_png) {
        if(!crazypod_book_png_inspect(path, &png_info))
            return false;
        decode_bytes = png_info.workspace_size;
    }
    decode_handle = core_alloc_ex(
        decode_bytes, &buflib_ops_locked);
    if(decode_handle < 0)
        return false;
    decode_buffer = core_get_data(decode_handle);
    crazypod_image_decode_lock();
    if(is_png) {
        int width;
        int height;

        result = crazypod_book_png_decode(
            path, max_width, max_height, slot->pixels,
            &width, &height, decode_buffer, decode_bytes) ? 0 : -1;
        crazypod_image_decode_unlock();
        core_free(decode_handle);
        return result == 0 && crazypod_image_configure_rgb565(
            &slot->descriptor, slot->pixels, width, height);
    }
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = max_width;
    bitmap.height = max_height;
    bitmap.data = (unsigned char *)decode_buffer;
    if(extension_is(path, ".jpg") || extension_is(path, ".jpeg"))
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
       bitmap.width > BOOK_IMAGE_WIDTH || bitmap.height > BOOK_IMAGE_HEIGHT ||
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

const lv_image_dsc_t *crazypod_book_image_get(
    int book_index, uint32_t page_offset,
    int max_width, int max_height)
{
    const struct crazypod_book *book = crazypod_book_get(book_index);
    char path[MAX_PATH];
    uint32_t key;

    if(max_width <= 0 || max_width > BOOK_IMAGE_WIDTH ||
       max_height <= 0 || max_height > BOOK_IMAGE_HEIGHT ||
       !crazypod_book_page_image(
           book_index, page_offset, path, sizeof(path)))
        return NULL;
    key = path_hash(path) ^ ((uint32_t)max_width << 16) ^
        (uint32_t)max_height;
    if(book != NULL)
        key ^= book->size ^ (book->mtime * 16777619u);
    if(image_slot.valid && image_slot.key == key)
        return &image_slot.descriptor;
    if(image_slot.valid)
        lv_image_cache_drop(&image_slot.descriptor);
    image_slot.valid = false;
    if(!decode_image(path, &image_slot, max_width, max_height))
        return NULL;
    image_slot.key = key;
    image_slot.valid = true;
    return &image_slot.descriptor;
}

void crazypod_book_image_reset(void)
{
    if(image_slot.valid)
        lv_image_cache_drop(&image_slot.descriptor);
    memset(&image_slot, 0, sizeof(image_slot));
}

#endif
