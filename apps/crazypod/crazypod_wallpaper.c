#include "config.h"

#ifdef IPOD_6G

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "buflib.h"
#include "bmp.h"
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "jpeg_load.h"
#include "lcd.h"
#include "resize.h"
#include "string-extra.h"
#include "system.h"

#include "crazypod_appearance.h"
#include "crazypod_image.h"
#include "crazypod_wallpaper.h"
#include "wallpaper/crazypod_wallpaper_crop_engine.h"
#include "wallpaper/crazypod_wallpaper_store.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#define WALLPAPER_WIDTH LCD_WIDTH
#define WALLPAPER_HEIGHT LCD_HEIGHT
#define WALLPAPER_SOURCE_ROW_BYTES (WALLPAPER_WIDTH * 4)
#define WALLPAPER_PIXEL_BYTES \
    (WALLPAPER_WIDTH * WALLPAPER_HEIGHT * sizeof(fb_data))
#define WALLPAPER_DECODE_BYTES \
    (WALLPAPER_PIXEL_BYTES + 64 * 1024)
#define WALLPAPER_PATH "/.rockbox/crazypod/default-home.bmp"
#define FROSTED_CAPSULE_SIDE_MARGIN 0
#define FROSTED_CAPSULE_BOTTOM_MARGIN 0
#define FROSTED_LOCK_MEDIA_SIDE_MARGIN 12
#define FROSTED_LOCK_MEDIA_BOTTOM_MARGIN 12
#define FROSTED_CAPSULE_X FROSTED_CAPSULE_SIDE_MARGIN
#define FROSTED_CAPSULE_Y 174
#define FROSTED_CAPSULE_WIDTH \
    (WALLPAPER_WIDTH - FROSTED_CAPSULE_SIDE_MARGIN * 2)
#define FROSTED_CAPSULE_HEIGHT \
    (WALLPAPER_HEIGHT - FROSTED_CAPSULE_Y - \
     FROSTED_CAPSULE_BOTTOM_MARGIN)
#define FROSTED_LOCK_MEDIA_X FROSTED_LOCK_MEDIA_SIDE_MARGIN
#define FROSTED_LOCK_MEDIA_WIDTH \
    (WALLPAPER_WIDTH - FROSTED_LOCK_MEDIA_SIDE_MARGIN * 2)
#define FROSTED_LOCK_MEDIA_HEIGHT 94
#define FROSTED_LOCK_MEDIA_Y \
    (WALLPAPER_HEIGHT - FROSTED_LOCK_MEDIA_HEIGHT - \
     FROSTED_LOCK_MEDIA_BOTTOM_MARGIN)

static fb_data wallpaper_pixels[WALLPAPER_WIDTH * WALLPAPER_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t wallpaper_descriptor;
static fb_data custom_home_pixels[WALLPAPER_WIDTH * WALLPAPER_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data custom_menu_pixels[WALLPAPER_WIDTH * WALLPAPER_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data custom_lock_pixels[WALLPAPER_WIDTH * WALLPAPER_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t custom_home_descriptor;
static lv_image_dsc_t custom_menu_descriptor;
static lv_image_dsc_t custom_lock_descriptor;
static bool wallpaper_valid;
static bool custom_home_valid;
static bool custom_menu_valid;
static bool custom_lock_valid;

struct frosted_region_cache {
    lv_image_dsc_t descriptor;
    bool valid;
    int handle;
    uint32_t source_key;
    uint32_t tint;
    unsigned tint_opa;
};

struct frosted_region_source {
    const fb_data *pixels;
    fb_data solid_pixel;
    int width;
    int height;
    int stride;
    int x;
    int y;
    int region_width;
    int region_height;
    uint32_t key;
};

static struct frosted_region_cache frosted_capsule_cache = {
    .handle = -1,
};
static struct frosted_region_cache frosted_lock_media_cache = {
    .handle = -1,
};

static void release_frosted_region(struct frosted_region_cache *cache)
{
    if(cache->descriptor.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&cache->descriptor);
    memset(&cache->descriptor, 0, sizeof(cache->descriptor));
    cache->valid = false;
    cache->source_key = 0;
    cache->tint = 0;
    cache->tint_opa = 0;
    if(cache->handle >= 0)
        cache->handle = core_free(cache->handle);
}

static void release_frosted_target(
    enum crazypod_wallpaper_target target)
{
    if(target == CRAZYPOD_WALLPAPER_HOME)
        release_frosted_region(&frosted_capsule_cache);
    else if(target == CRAZYPOD_WALLPAPER_LOCK)
        release_frosted_region(&frosted_lock_media_cache);
}

static uint16_t read_le16(const uint8_t *value)
{
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8);
}

static uint32_t read_le32(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static bool read_exact(int fd, void *buffer, size_t size)
{
    uint8_t *cursor = buffer;

    while(size > 0) {
        ssize_t count = read(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool valid_target(enum crazypod_wallpaper_target target)
{
    return target <= CRAZYPOD_WALLPAPER_LOCK;
}

 static enum crazypod_appearance_field appearance_field_for_target(
    enum crazypod_wallpaper_target target)
{
    if(target == CRAZYPOD_WALLPAPER_MENU)
        return CRAZYPOD_APPEARANCE_MENU_BACKGROUND;
    if(target == CRAZYPOD_WALLPAPER_LOCK)
        return CRAZYPOD_APPEARANCE_LOCK_BACKGROUND;
    return CRAZYPOD_APPEARANCE_HOME_BACKGROUND;
}

static fb_data *pixels_for_target(
    enum crazypod_wallpaper_target target)
{
    if(target == CRAZYPOD_WALLPAPER_MENU)
        return custom_menu_pixels;
    if(target == CRAZYPOD_WALLPAPER_LOCK)
        return custom_lock_pixels;
    return custom_home_pixels;
}

static lv_image_dsc_t *descriptor_for_target(
    enum crazypod_wallpaper_target target)
{
    if(target == CRAZYPOD_WALLPAPER_MENU)
        return &custom_menu_descriptor;
    if(target == CRAZYPOD_WALLPAPER_LOCK)
        return &custom_lock_descriptor;
    return &custom_home_descriptor;
}

static void set_target_valid(
    enum crazypod_wallpaper_target target, bool valid)
{
    if(target == CRAZYPOD_WALLPAPER_MENU)
        custom_menu_valid = valid;
    else if(target == CRAZYPOD_WALLPAPER_LOCK)
        custom_lock_valid = valid;
    else
        custom_home_valid = valid;
}

 static bool path_has_extension(const char *path, const char *extension)
{
    size_t path_length;
    size_t extension_length;

    if(path == NULL || extension == NULL)
        return false;
    path_length = strlen(path);
    extension_length = strlen(extension);
    return path_length >= extension_length &&
           strcasecmp(path + path_length - extension_length,
                      extension) == 0;
}

static bool decode_custom_wallpaper(const char *path, fb_data *pixels,
                                    lv_image_dsc_t *descriptor)
{
    struct bitmap bitmap;
    fb_data *decode_buffer;
    int decode_handle;
    int format = FORMAT_NATIVE | FORMAT_RESIZE;
    int result;
    bool valid;

    if(path == NULL || path[0] == '\0')
        return false;
    decode_handle = core_alloc_ex(
        WALLPAPER_DECODE_BYTES, &buflib_ops_locked);
    if(decode_handle < 0)
        return false;
    decode_buffer = core_get_data(decode_handle);
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = WALLPAPER_WIDTH;
    bitmap.height = WALLPAPER_HEIGHT;
    bitmap.data = (unsigned char *)decode_buffer;

    crazypod_image_decode_lock();
    if(path_has_extension(path, ".jpg") ||
       path_has_extension(path, ".jpeg"))
        result = read_jpeg_file(path, &bitmap,
                                WALLPAPER_DECODE_BYTES,
                                format, &format_native);
    else if(path_has_extension(path, ".bmp"))
        result = read_bmp_file(path, &bitmap,
                               WALLPAPER_DECODE_BYTES,
                               format, &format_native);
    else {
        crazypod_image_decode_unlock();
        core_free(decode_handle);
        return false;
    }
    crazypod_image_decode_unlock();

    valid = result >= 0 &&
        bitmap.width == WALLPAPER_WIDTH &&
        bitmap.height == WALLPAPER_HEIGHT &&
        bitmap.data != NULL;
    if(!valid) {
        core_free(decode_handle);
        return false;
    }
    memcpy(pixels, bitmap.data, WALLPAPER_PIXEL_BYTES);
    core_free(decode_handle);
    return crazypod_image_configure_rgb565(
        descriptor, pixels, WALLPAPER_WIDTH, WALLPAPER_HEIGHT);
}

static bool load_custom_wallpaper(
    enum crazypod_wallpaper_target target,
    const char *path, fb_data *pixels,
    lv_image_dsc_t *descriptor)
{
    if(crazypod_wallpaper_store_load(
           target, path, pixels, descriptor))
        return true;
    if(!decode_custom_wallpaper(path, pixels, descriptor))
        return false;
    crazypod_wallpaper_store_save(target, path, pixels, NULL);
    return true;
}

void crazypod_wallpaper_reload_custom(void)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    bool inherited_lock =
        crazypod_appearance_take_lock_inheritance_migration();
    bool clone_home_to_lock;

    custom_home_valid = appearance->home_wallpaper[0] != '\0' &&
        load_custom_wallpaper(
            CRAZYPOD_WALLPAPER_HOME, appearance->home_wallpaper,
            custom_home_pixels, &custom_home_descriptor);
    custom_menu_valid = appearance->menu_wallpaper[0] != '\0' &&
        load_custom_wallpaper(
            CRAZYPOD_WALLPAPER_MENU, appearance->menu_wallpaper,
            custom_menu_pixels, &custom_menu_descriptor);
    clone_home_to_lock =
        inherited_lock &&
        custom_home_valid &&
        appearance->lock_wallpaper[0] != '\0' &&
        strcmp(appearance->home_wallpaper,
               appearance->lock_wallpaper) == 0;
    if(clone_home_to_lock) {
        memcpy(custom_lock_pixels, custom_home_pixels,
               WALLPAPER_PIXEL_BYTES);
        custom_lock_valid = crazypod_image_configure_rgb565(
            &custom_lock_descriptor, custom_lock_pixels,
            WALLPAPER_WIDTH, WALLPAPER_HEIGHT);
        if(custom_lock_valid) {
            (void)crazypod_wallpaper_store_save(
                CRAZYPOD_WALLPAPER_LOCK,
                appearance->lock_wallpaper,
                custom_lock_pixels, NULL);
        }
    }
    else {
        custom_lock_valid = appearance->lock_wallpaper[0] != '\0' &&
            load_custom_wallpaper(
                CRAZYPOD_WALLPAPER_LOCK, appearance->lock_wallpaper,
                custom_lock_pixels, &custom_lock_descriptor);
    }
    release_frosted_target(CRAZYPOD_WALLPAPER_HOME);
    release_frosted_target(CRAZYPOD_WALLPAPER_LOCK);
}

void crazypod_wallpaper_init(void)
{
    uint8_t header[54];
    uint8_t row[WALLPAPER_SOURCE_ROW_BYTES];
    uint32_t pixel_offset;
    int32_t width;
    int32_t height;
    bool top_down;
    int source_row;
    int fd;

    wallpaper_valid = false;
    custom_home_valid = false;
    custom_menu_valid = false;
    custom_lock_valid = false;
    crazypod_wallpaper_reload_custom();

    fd = open(WALLPAPER_PATH, O_RDONLY);
    if(fd < 0)
        return;
    if(!read_exact(fd, header, sizeof(header)) ||
       header[0] != 'B' || header[1] != 'M') {
        close(fd);
        return;
    }

    pixel_offset = read_le32(header + 10);
    width = (int32_t)read_le32(header + 18);
    height = (int32_t)read_le32(header + 22);
    top_down = height < 0;
    if(width != WALLPAPER_WIDTH ||
       (height != WALLPAPER_HEIGHT && height != -WALLPAPER_HEIGHT) ||
       read_le16(header + 26) != 1 ||
       read_le16(header + 28) != 32 ||
       read_le32(header + 30) != 0 ||
       lseek(fd, (off_t)pixel_offset, SEEK_SET) < 0) {
        close(fd);
        return;
    }

    for(source_row = 0; source_row < WALLPAPER_HEIGHT; ++source_row) {
        int target_row = top_down
            ? source_row : WALLPAPER_HEIGHT - 1 - source_row;
        int x;

        if(!read_exact(fd, row, sizeof(row))) {
            close(fd);
            return;
        }
        for(x = 0; x < WALLPAPER_WIDTH; ++x) {
            const uint8_t *source = row + x * 4;

            wallpaper_pixels[target_row * WALLPAPER_WIDTH + x] =
                LCD_RGBPACK(source[2], source[1], source[0]);
        }
    }
    close(fd);

    crazypod_image_configure_rgb565(
        &wallpaper_descriptor, wallpaper_pixels,
        WALLPAPER_WIDTH, WALLPAPER_HEIGHT);
    wallpaper_valid = true;
}

bool crazypod_wallpaper_select(
    enum crazypod_wallpaper_target target, const char *path)
{
    fb_data *pixels;
    lv_image_dsc_t *descriptor;
    bool loaded;

    if(!valid_target(target))
        return false;
    pixels = pixels_for_target(target);
    descriptor = descriptor_for_target(target);
    loaded = decode_custom_wallpaper(path, pixels, descriptor);
    if(!loaded ||
       !crazypod_wallpaper_store_save(
           target, path, pixels, NULL))
        return false;
    set_target_valid(target, true);
    release_frosted_target(target);
    return crazypod_appearance_set_wallpaper(
        appearance_field_for_target(target), path);
}

int crazypod_wallpaper_crop_max_zoom(
    const lv_image_dsc_t *source_descriptor)
{
    return crazypod_wallpaper_crop_engine_max_zoom(source_descriptor);
}

enum crazypod_wallpaper_apply_result crazypod_wallpaper_apply_crop(
    enum crazypod_wallpaper_target target, const char *path,
    const lv_image_dsc_t *source_descriptor,
    int crop_x, int crop_y, int crop_width, int crop_height,
    crazypod_wallpaper_progress_cb progress_cb,
    void *progress_user_data)
{
    fb_data *destination;
    lv_image_dsc_t *descriptor;
    int source_width;
    int source_height;
    enum crazypod_wallpaper_apply_result result =
        CRAZYPOD_WALLPAPER_APPLY_OK;

    if(!valid_target(target) ||
       path == NULL || path[0] == '\0' ||
       source_descriptor == NULL ||
       source_descriptor->header.magic != LV_IMAGE_HEADER_MAGIC ||
       source_descriptor->header.cf != LV_COLOR_FORMAT_RGB565)
        return CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE;
    destination = pixels_for_target(target);
    descriptor = descriptor_for_target(target);
    source_width = source_descriptor->header.w;
    source_height = source_descriptor->header.h;
    if(source_width <= 0 || source_height <= 0 ||
       crop_x < 0 || crop_y < 0 ||
       crop_width <= 1 || crop_height <= 1 ||
       crop_x + crop_width > source_width ||
        crop_y + crop_height > source_height)
        return CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE;
    if(progress_cb != NULL)
        progress_cb(5, progress_user_data);
    result = crazypod_wallpaper_crop_engine_render(
        path, source_descriptor,
        crop_x, crop_y, crop_width, crop_height,
        destination, progress_cb, progress_user_data);
    if(result != CRAZYPOD_WALLPAPER_APPLY_OK)
        return result;
    if(!crazypod_wallpaper_store_save(
           target, path, destination, &result))
        return result;
    if(progress_cb != NULL)
        progress_cb(96, progress_user_data);
    if(!crazypod_appearance_set_wallpaper(
           appearance_field_for_target(target), path))
        return CRAZYPOD_WALLPAPER_APPLY_SETTINGS_FAILED;
    if(!crazypod_image_configure_rgb565(
           descriptor, destination,
           WALLPAPER_WIDTH, WALLPAPER_HEIGHT))
        return CRAZYPOD_WALLPAPER_APPLY_ACTIVATE_FAILED;
    set_target_valid(target, true);
    release_frosted_target(target);
    if(progress_cb != NULL)
        progress_cb(100, progress_user_data);
    return CRAZYPOD_WALLPAPER_APPLY_OK;
}

void crazypod_wallpaper_clear(enum crazypod_wallpaper_target target)
{
    if(!valid_target(target))
        return;
    set_target_valid(target, false);
    crazypod_wallpaper_store_remove(target);
    release_frosted_target(target);
    crazypod_appearance_set_wallpaper(
        appearance_field_for_target(target), "");
}

static void select_frosted_region_source(
    enum crazypod_wallpaper_target target,
    int x, int y, int width, int height,
    struct frosted_region_source *source)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    const fb_data *custom_pixels = target == CRAZYPOD_WALLPAPER_LOCK
        ? custom_lock_pixels : custom_home_pixels;
    const char *custom_path = target == CRAZYPOD_WALLPAPER_LOCK
        ? appearance->lock_wallpaper : appearance->home_wallpaper;
    bool custom_valid = target == CRAZYPOD_WALLPAPER_LOCK
        ? custom_lock_valid : custom_home_valid;
    int background = target == CRAZYPOD_WALLPAPER_LOCK
        ? appearance->lock_background : appearance->home_background;
    uint32_t color = target == CRAZYPOD_WALLPAPER_LOCK
        ? crazypod_appearance_lock_color()
        : crazypod_appearance_home_color();

    if(custom_valid) {
        source->pixels = custom_pixels;
        source->width = WALLPAPER_WIDTH;
        source->height = WALLPAPER_HEIGHT;
        source->stride = WALLPAPER_WIDTH;
        source->x = x;
        source->y = y;
        source->region_width = width;
        source->region_height = height;
        source->key = 1;
    }
    else if(custom_path[0] == '\0' && background == 0 &&
            wallpaper_valid) {
        source->pixels = wallpaper_pixels;
        source->width = WALLPAPER_WIDTH;
        source->height = WALLPAPER_HEIGHT;
        source->stride = WALLPAPER_WIDTH;
        source->x = x;
        source->y = y;
        source->region_width = width;
        source->region_height = height;
        source->key = 2;
    }
    else {
        source->solid_pixel = LCD_RGBPACK(
            (color >> 16) & 0xff,
            (color >> 8) & 0xff,
            color & 0xff);
        source->pixels = &source->solid_pixel;
        source->width = 1;
        source->height = 1;
        source->stride = 1;
        source->x = 0;
        source->y = 0;
        source->region_width = 1;
        source->region_height = 1;
        source->key = 0x03000000u | color;
    }
}

static bool prepare_frosted_region(
    struct frosted_region_cache *cache,
    enum crazypod_wallpaper_target target,
    int x, int y, int width, int height,
    uint32_t tint, unsigned tint_opa)
{
    struct frosted_region_source source;
    fb_data *render_pixels;
    fb_data *sample_pixels;
    fb_data *scratch_pixels;
    size_t sample_count =
        crazypod_image_glass_sample_pixels(width, height);
    int workspace_handle;

    select_frosted_region_source(
        target, x, y, width, height, &source);
    if(cache->valid && cache->source_key == source.key &&
       cache->tint == tint && cache->tint_opa == tint_opa)
        return true;
    if(cache->valid)
        release_frosted_region(cache);

    cache->handle = core_alloc_ex(
        width * height * sizeof(fb_data), &buflib_ops_locked);
    if(cache->handle < 0)
        return false;
    workspace_handle = core_alloc(
        sample_count * 2 * sizeof(fb_data));
    if(workspace_handle < 0) {
        cache->handle = core_free(cache->handle);
        return false;
    }

    render_pixels = core_get_data(cache->handle);
    sample_pixels = core_get_data(workspace_handle);
    scratch_pixels = sample_pixels + sample_count;
    if(!crazypod_image_render_glass_rgb565(
           source.pixels, source.width, source.height, source.stride,
           source.x, source.y,
           source.region_width, source.region_height,
           tint, tint_opa,
           sample_pixels, scratch_pixels, sample_count,
           render_pixels, width, height)) {
        core_free(workspace_handle);
        cache->handle = core_free(cache->handle);
        return false;
    }
    core_free(workspace_handle);
    if(!crazypod_image_configure_rgb565(
           &cache->descriptor, render_pixels, width, height)) {
        cache->handle = core_free(cache->handle);
        return false;
    }
    cache->valid = true;
    cache->source_key = source.key;
    cache->tint = tint;
    cache->tint_opa = tint_opa;
    return true;
}

bool crazypod_wallpaper_prepare_frosted_capsule(
    uint32_t tint, unsigned tint_opa)
{
    return prepare_frosted_region(
        &frosted_capsule_cache, CRAZYPOD_WALLPAPER_HOME,
        FROSTED_CAPSULE_X, FROSTED_CAPSULE_Y,
        FROSTED_CAPSULE_WIDTH, FROSTED_CAPSULE_HEIGHT,
        tint, tint_opa);
}

bool crazypod_wallpaper_prepare_frosted_lock_media(
    uint32_t tint, unsigned tint_opa)
{
    return prepare_frosted_region(
        &frosted_lock_media_cache, CRAZYPOD_WALLPAPER_LOCK,
        FROSTED_LOCK_MEDIA_X, FROSTED_LOCK_MEDIA_Y,
        FROSTED_LOCK_MEDIA_WIDTH, FROSTED_LOCK_MEDIA_HEIGHT,
        tint, tint_opa);
}

const lv_image_dsc_t *crazypod_default_wallpaper(void)
{
    return wallpaper_valid ? &wallpaper_descriptor : NULL;
}

const lv_image_dsc_t *crazypod_custom_home_wallpaper(void)
{
    return custom_home_valid ? &custom_home_descriptor : NULL;
}

const lv_image_dsc_t *crazypod_custom_menu_wallpaper(void)
{
    return custom_menu_valid ? &custom_menu_descriptor : NULL;
}

const lv_image_dsc_t *crazypod_custom_lock_wallpaper(void)
{
    return custom_lock_valid ? &custom_lock_descriptor : NULL;
}

const lv_image_dsc_t *crazypod_frosted_wallpaper_capsule(void)
{
    return frosted_capsule_cache.valid
        ? &frosted_capsule_cache.descriptor : NULL;
}

const lv_image_dsc_t *crazypod_frosted_lock_media(void)
{
    return frosted_lock_media_cache.valid
        ? &frosted_lock_media_cache.descriptor : NULL;
}

#endif
