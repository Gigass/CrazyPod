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
#include "src/misc/cache/instance/lv_image_cache.h"

#define WALLPAPER_WIDTH LCD_WIDTH
#define WALLPAPER_HEIGHT LCD_HEIGHT
#define WALLPAPER_SOURCE_ROW_BYTES (WALLPAPER_WIDTH * 4)
#define CUSTOM_WALLPAPER_BYTES \
    (WALLPAPER_WIDTH * WALLPAPER_HEIGHT * sizeof(fb_data))
#define WALLPAPER_DECODE_BYTES \
    (CUSTOM_WALLPAPER_BYTES + 64 * 1024)
#define WALLPAPER_CROP_MAX_PIXELS (4 * 1024 * 1024)
#define WALLPAPER_CROP_OVERSAMPLE 2
#define WALLPAPER_PATH "/.rockbox/crazypod/default-home.bmp"
#define CUSTOM_WALLPAPER_DIRECTORY "/.crazypod"
#define CUSTOM_WALLPAPER_CACHE_DIRECTORY "/.crazypod/cache"
#define CUSTOM_HOME_NATIVE_PATH "/.crazypod/cache/home.wall"
#define CUSTOM_HOME_NATIVE_TEMP "/.crazypod/cache/home.wall.tmp"
#define CUSTOM_MENU_NATIVE_PATH "/.crazypod/cache/menu.wall"
#define CUSTOM_MENU_NATIVE_TEMP "/.crazypod/cache/menu.wall.tmp"
#define CUSTOM_LOCK_NATIVE_PATH "/.crazypod/cache/lock.wall"
#define CUSTOM_LOCK_NATIVE_TEMP "/.crazypod/cache/lock.wall.tmp"
#define CUSTOM_WALLPAPER_MAGIC 0x43505731u
#define CUSTOM_WALLPAPER_VERSION 1
#define FROSTED_CAPSULE_X 8
#define FROSTED_CAPSULE_Y 174
#define FROSTED_CAPSULE_WIDTH 304
#define FROSTED_CAPSULE_HEIGHT 58
#define FROSTED_CORNER_RADIUS 29
#define FROSTED_CAPSULE_ROW_BYTES \
    (FROSTED_CAPSULE_WIDTH * sizeof(fb_data))
#define FROSTED_CAPSULE_BYTES \
    (FROSTED_CAPSULE_ROW_BYTES * FROSTED_CAPSULE_HEIGHT)

struct custom_wallpaper_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t source_key;
    uint16_t width;
    uint16_t height;
    uint32_t data_size;
};

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
static lv_image_dsc_t frosted_capsule_descriptor;
static bool wallpaper_valid;
static bool custom_home_valid;
static bool custom_menu_valid;
static bool custom_lock_valid;
static bool frosted_capsule_valid;
static int frosted_capsule_handle = -1;
static uint32_t frosted_capsule_source_key;
static uint32_t frosted_capsule_tint;
static unsigned frosted_capsule_tint_opa;

static void release_frosted_capsule(void)
{
    if(frosted_capsule_descriptor.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&frosted_capsule_descriptor);
    memset(&frosted_capsule_descriptor, 0,
           sizeof(frosted_capsule_descriptor));
    frosted_capsule_valid = false;
    frosted_capsule_source_key = 0;
    frosted_capsule_tint = 0;
    frosted_capsule_tint_opa = 0;
    if(frosted_capsule_handle >= 0)
        frosted_capsule_handle = core_free(frosted_capsule_handle);
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

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const uint8_t *cursor = buffer;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static uint32_t wallpaper_source_key(const char *path)
{
    uint32_t hash = 2166136261u;

    if(path == NULL)
        return hash;
    while(*path != '\0') {
        hash ^= (unsigned char)*path++;
        hash *= 16777619u;
    }
    return hash;
}

static bool valid_target(enum crazypod_wallpaper_target target)
{
    return target <= CRAZYPOD_WALLPAPER_LOCK;
}

static const char *native_wallpaper_path(
    enum crazypod_wallpaper_target target)
{
    if(target == CRAZYPOD_WALLPAPER_MENU)
        return CUSTOM_MENU_NATIVE_PATH;
    if(target == CRAZYPOD_WALLPAPER_LOCK)
        return CUSTOM_LOCK_NATIVE_PATH;
    return CUSTOM_HOME_NATIVE_PATH;
}

static const char *native_wallpaper_temp(
    enum crazypod_wallpaper_target target)
{
    if(target == CRAZYPOD_WALLPAPER_MENU)
        return CUSTOM_MENU_NATIVE_TEMP;
    if(target == CRAZYPOD_WALLPAPER_LOCK)
        return CUSTOM_LOCK_NATIVE_TEMP;
    return CUSTOM_HOME_NATIVE_TEMP;
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

static bool load_native_wallpaper(
    enum crazypod_wallpaper_target target,
    const char *source_path, fb_data *pixels,
    lv_image_dsc_t *descriptor)
{
    struct custom_wallpaper_header header;
    int fd = open(native_wallpaper_path(target), O_RDONLY);
    bool valid;

    if(fd < 0)
        return false;
    valid =
        read_exact(fd, &header, sizeof(header)) &&
        header.magic == CUSTOM_WALLPAPER_MAGIC &&
        header.version == CUSTOM_WALLPAPER_VERSION &&
        header.header_size == sizeof(header) &&
        header.source_key == wallpaper_source_key(source_path) &&
        header.width == WALLPAPER_WIDTH &&
        header.height == WALLPAPER_HEIGHT &&
        header.data_size == CUSTOM_WALLPAPER_BYTES &&
        read_exact(fd, pixels, CUSTOM_WALLPAPER_BYTES);
    close(fd);
    return valid &&
        crazypod_image_configure_rgb565(
            descriptor, pixels, WALLPAPER_WIDTH, WALLPAPER_HEIGHT);
}

static bool save_native_wallpaper(
    enum crazypod_wallpaper_target target,
    const char *source_path, const fb_data *pixels,
    enum crazypod_wallpaper_apply_result *error)
{
    struct custom_wallpaper_header header;
    const char *temporary = native_wallpaper_temp(target);
    const char *published = native_wallpaper_path(target);
    bool complete;
    int fd;

    mkdir(CUSTOM_WALLPAPER_DIRECTORY);
    mkdir(CUSTOM_WALLPAPER_CACHE_DIRECTORY);
    memset(&header, 0, sizeof(header));
    header.magic = CUSTOM_WALLPAPER_MAGIC;
    header.version = CUSTOM_WALLPAPER_VERSION;
    header.header_size = sizeof(header);
    header.source_key = wallpaper_source_key(source_path);
    header.width = WALLPAPER_WIDTH;
    header.height = WALLPAPER_HEIGHT;
    header.data_size = CUSTOM_WALLPAPER_BYTES;
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0) {
        if(error != NULL)
            *error = CRAZYPOD_WALLPAPER_APPLY_CACHE_OPEN_FAILED;
        return false;
    }
    complete =
        write_exact(fd, &header, sizeof(header)) &&
        write_exact(fd, pixels, CUSTOM_WALLPAPER_BYTES) &&
        fsync(fd) >= 0;
    close(fd);
    if(!complete) {
        if(error != NULL)
            *error = CRAZYPOD_WALLPAPER_APPLY_CACHE_WRITE_FAILED;
        remove(temporary);
        return false;
    }
    if(rename(temporary, published) < 0) {
        if(error != NULL)
            *error = CRAZYPOD_WALLPAPER_APPLY_CACHE_PUBLISH_FAILED;
        remove(temporary);
        return false;
    }
    return true;
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
    memcpy(pixels, bitmap.data, CUSTOM_WALLPAPER_BYTES);
    core_free(decode_handle);
    return crazypod_image_configure_rgb565(
        descriptor, pixels, WALLPAPER_WIDTH, WALLPAPER_HEIGHT);
}

static bool load_custom_wallpaper(
    enum crazypod_wallpaper_target target,
    const char *path, fb_data *pixels,
    lv_image_dsc_t *descriptor)
{
    if(load_native_wallpaper(target, path, pixels, descriptor))
        return true;
    if(!decode_custom_wallpaper(path, pixels, descriptor))
        return false;
    save_native_wallpaper(target, path, pixels, NULL);
    return true;
}

void crazypod_wallpaper_reload_custom(void)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();

    custom_home_valid = appearance->home_wallpaper[0] != '\0' &&
        load_custom_wallpaper(
            CRAZYPOD_WALLPAPER_HOME, appearance->home_wallpaper,
            custom_home_pixels, &custom_home_descriptor);
    custom_menu_valid = appearance->menu_wallpaper[0] != '\0' &&
        load_custom_wallpaper(
            CRAZYPOD_WALLPAPER_MENU, appearance->menu_wallpaper,
            custom_menu_pixels, &custom_menu_descriptor);
    custom_lock_valid = appearance->lock_wallpaper[0] != '\0' &&
        load_custom_wallpaper(
            CRAZYPOD_WALLPAPER_LOCK, appearance->lock_wallpaper,
            custom_lock_pixels, &custom_lock_descriptor);
    release_frosted_capsule();
}

static int clamp_coordinate(int value, int maximum)
{
    if(value < 0)
        return 0;
    if(value >= maximum)
        return maximum - 1;
    return value;
}

static uint8_t rounded_capsule_alpha(int x, int y)
{
    const int inner_radius = FROSTED_CORNER_RADIUS - 1;
    const int inner_squared = inner_radius * inner_radius;
    const int outer_squared =
        FROSTED_CORNER_RADIUS * FROSTED_CORNER_RADIUS;
    int dx = 0;
    int dy = 0;
    int distance_squared;

    if(x < FROSTED_CORNER_RADIUS)
        dx = inner_radius - x;
    else if(x >= FROSTED_CAPSULE_WIDTH - FROSTED_CORNER_RADIUS)
        dx = x - (FROSTED_CAPSULE_WIDTH - FROSTED_CORNER_RADIUS);
    if(y < FROSTED_CORNER_RADIUS)
        dy = inner_radius - y;
    else if(y >= FROSTED_CAPSULE_HEIGHT - FROSTED_CORNER_RADIUS)
        dy = y - (FROSTED_CAPSULE_HEIGHT - FROSTED_CORNER_RADIUS);

    distance_squared = dx * dx + dy * dy;
    if(distance_squared <= inner_squared)
        return 255;
    if(distance_squared >= outer_squared)
        return 0;
    return (uint8_t)(
        (outer_squared - distance_squared) * 255 /
        (outer_squared - inner_squared));
}

static fb_data blend_capsule_edge(
    fb_data material, fb_data background, unsigned alpha)
{
    unsigned inverse_alpha = 255 - alpha;

    return LCD_RGBPACK(
        (RGB_UNPACK_RED(material) * alpha +
         RGB_UNPACK_RED(background) * inverse_alpha + 127) / 255,
        (RGB_UNPACK_GREEN(material) * alpha +
         RGB_UNPACK_GREEN(background) * inverse_alpha + 127) / 255,
        (RGB_UNPACK_BLUE(material) * alpha +
         RGB_UNPACK_BLUE(background) * inverse_alpha + 127) / 255);
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
       !save_native_wallpaper(target, path, pixels, NULL))
        return false;
    set_target_valid(target, true);
    if(target == CRAZYPOD_WALLPAPER_HOME)
        release_frosted_capsule();
    return crazypod_appearance_set_wallpaper(
        appearance_field_for_target(target), path);
}

static fb_data sample_crop_pixel(
    const fb_data *source, int width, int height, int stride,
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
    p00 = source[y0 * stride + x0];
    p10 = source[y0 * stride + x1];
    p01 = source[y1 * stride + x0];
    p11 = source[y1 * stride + x1];
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

static int crop_decode_max_scale(int width, int height)
{
    uint64_t source_pixels;
    int scale = 1;

    if(width <= 0 || height <= 0)
        return 0;
    source_pixels = (uint64_t)width * height;
    if(source_pixels > WALLPAPER_CROP_MAX_PIXELS)
        return 0;
    while((uint64_t)(scale + 1) * (scale + 1) *
              source_pixels <=
          WALLPAPER_CROP_MAX_PIXELS)
        ++scale;
    return scale;
}

int crazypod_wallpaper_crop_max_zoom(
    const lv_image_dsc_t *source_descriptor)
{
    int source_width;
    int source_height;
    int maximum_width;
    int maximum_height;
    int decode_scale;
    int width_zoom;
    int height_zoom;
    int maximum_zoom;

    if(source_descriptor == NULL ||
       source_descriptor->header.magic != LV_IMAGE_HEADER_MAGIC ||
       source_descriptor->header.w <= 0 ||
       source_descriptor->header.h <= 0)
        return 100;
    source_width = source_descriptor->header.w;
    source_height = source_descriptor->header.h;
    if(source_width * 3 > source_height * 4) {
        maximum_height = source_height;
        maximum_width = maximum_height * 4 / 3;
    }
    else {
        maximum_width = source_width;
        maximum_height = maximum_width * 3 / 4;
    }
    decode_scale =
        crop_decode_max_scale(source_width, source_height);
    if(decode_scale < 1)
        return 100;
    width_zoom =
        maximum_width * decode_scale * 100 /
        WALLPAPER_WIDTH;
    height_zoom =
        maximum_height * decode_scale * 100 /
        WALLPAPER_HEIGHT;
    maximum_zoom =
        width_zoom < height_zoom ? width_zoom : height_zoom;
    if(maximum_zoom < 100)
        maximum_zoom = 100;
    if(maximum_zoom > 500)
        maximum_zoom = 500;
    return maximum_zoom;
}

static enum crazypod_wallpaper_apply_result decode_crop_source(
    const char *path, int width, int height,
    struct bitmap *bitmap, int *decode_handle)
{
    size_t decode_bytes;
    fb_data *decode_buffer;
    int format = FORMAT_NATIVE | FORMAT_RESIZE |
        FORMAT_KEEP_ASPECT;
    int required_bytes;
    int probe_handle = -1;
    void *probe_buffer = NULL;
    int result;
    bool jpeg;

    if(path == NULL || bitmap == NULL ||
       decode_handle == NULL || width <= 0 || height <= 0 ||
       (uint64_t)width * height >
           WALLPAPER_CROP_MAX_PIXELS)
        return CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE;
    jpeg = path_has_extension(path, ".jpg") ||
        path_has_extension(path, ".jpeg");
    if(jpeg) {
        /*
         * JPEG's FORMAT_RETURN_SIZE parser stores its state in bm->data.
         * JPEG_DECODE_OVERHEAD is guaranteed to hold that parser state.
         */
        probe_handle = core_alloc_ex(
            JPEG_DECODE_OVERHEAD, &buflib_ops_locked);
        if(probe_handle < 0)
            return CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED;
        probe_buffer = core_get_data(probe_handle);
    }
    memset(bitmap, 0, sizeof(*bitmap));
    bitmap->width = width;
    bitmap->height = height;
    bitmap->data = probe_buffer;
    crazypod_image_decode_lock();
    if(jpeg)
        required_bytes = read_jpeg_file(
            path, bitmap, (int)JPEG_DECODE_OVERHEAD,
            format | FORMAT_RETURN_SIZE, &format_native);
    else if(path_has_extension(path, ".bmp"))
        required_bytes = read_bmp_file(
            path, bitmap, 0,
            format | FORMAT_RETURN_SIZE, &format_native);
    else
        required_bytes = -1;
    crazypod_image_decode_unlock();
    if(probe_handle >= 0)
        probe_handle = core_free(probe_handle);
    if(required_bytes <= 0)
        return CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED;
    decode_bytes = (size_t)required_bytes;
    if(decode_bytes > INT_MAX)
        return CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED;
    *decode_handle = core_alloc_ex(
        decode_bytes, &buflib_ops_locked);
    if(*decode_handle < 0)
        return CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED;
    decode_buffer = core_get_data(*decode_handle);
    memset(bitmap, 0, sizeof(*bitmap));
    bitmap->width = width;
    bitmap->height = height;
    bitmap->data = (unsigned char *)decode_buffer;

    crazypod_image_decode_lock();
    if(jpeg)
        result = read_jpeg_file(
            path, bitmap, (int)decode_bytes,
            format, &format_native);
    else if(path_has_extension(path, ".bmp"))
        result = read_bmp_file(
            path, bitmap, (int)decode_bytes,
            format, &format_native);
    else
        result = -1;
    crazypod_image_decode_unlock();
    if(result < 0 || bitmap->width <= 0 ||
       bitmap->height <= 0 || bitmap->width > width ||
       bitmap->height > height || bitmap->data == NULL) {
        *decode_handle = core_free(*decode_handle);
        return CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED;
    }
    return CRAZYPOD_WALLPAPER_APPLY_OK;
}

static enum crazypod_wallpaper_apply_result render_crop_from_original(
    const char *path,
    const lv_image_dsc_t *preview_descriptor,
    int crop_x, int crop_y, int crop_width, int crop_height,
    fb_data *destination,
    crazypod_wallpaper_progress_cb progress_cb,
    void *progress_user_data)
{
    struct bitmap bitmap;
    const fb_data *source;
    int preview_width = preview_descriptor->header.w;
    int preview_height = preview_descriptor->header.h;
    int maximum_scale =
        crop_decode_max_scale(preview_width, preview_height);
    int width_scale =
        (WALLPAPER_WIDTH * WALLPAPER_CROP_OVERSAMPLE +
         crop_width - 1) / crop_width;
    int height_scale =
        (WALLPAPER_HEIGHT * WALLPAPER_CROP_OVERSAMPLE +
         crop_height - 1) / crop_height;
    int decode_scale =
        width_scale > height_scale ? width_scale : height_scale;
    int decode_width;
    int decode_height;
    int decode_handle = -1;
    int high_x;
    int high_y;
    int high_right;
    int high_bottom;
    int high_width;
    int high_height;
    int y;
    enum crazypod_wallpaper_apply_result decode_result;

    if(maximum_scale < 1 || destination == NULL)
        return CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE;
    if(decode_scale < 1)
        decode_scale = 1;
    if(decode_scale > maximum_scale)
        decode_scale = maximum_scale;
    decode_width = preview_width * decode_scale;
    decode_height = preview_height * decode_scale;
    if(progress_cb != NULL)
        progress_cb(12, progress_user_data);
    decode_result = decode_crop_source(
        path, decode_width, decode_height,
        &bitmap, &decode_handle);
    if(decode_result != CRAZYPOD_WALLPAPER_APPLY_OK)
        return decode_result;
    if(progress_cb != NULL)
        progress_cb(55, progress_user_data);
    high_x = (int)(
        ((int64_t)crop_x * bitmap.width +
         preview_width / 2) / preview_width);
    high_y = (int)(
        ((int64_t)crop_y * bitmap.height +
         preview_height / 2) / preview_height);
    high_right = (int)(
        ((int64_t)(crop_x + crop_width) * bitmap.width +
         preview_width / 2) / preview_width);
    high_bottom = (int)(
        ((int64_t)(crop_y + crop_height) * bitmap.height +
         preview_height / 2) / preview_height);
    if(high_x < 0)
        high_x = 0;
    if(high_y < 0)
        high_y = 0;
    if(high_right > bitmap.width)
        high_right = bitmap.width;
    if(high_bottom > bitmap.height)
        high_bottom = bitmap.height;
    high_width = high_right - high_x;
    high_height = high_bottom - high_y;
    if(high_width < WALLPAPER_WIDTH ||
       high_height < WALLPAPER_HEIGHT) {
        core_free(decode_handle);
        return CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED;
    }
    source = (const fb_data *)bitmap.data;
    for(y = 0; y < WALLPAPER_HEIGHT; ++y) {
        int source_y_q8 = high_y * 256 +
            y * (high_height - 1) * 256 /
            (WALLPAPER_HEIGHT - 1);
        int x;

        for(x = 0; x < WALLPAPER_WIDTH; ++x) {
            int source_x_q8 = high_x * 256 +
                x * (high_width - 1) * 256 /
                (WALLPAPER_WIDTH - 1);

            destination[y * WALLPAPER_WIDTH + x] =
                sample_crop_pixel(
                    source, bitmap.width, bitmap.height,
                    bitmap.width, source_x_q8, source_y_q8);
        }
        if(progress_cb != NULL && (y % 48) == 47)
            progress_cb(
                55 + (y + 1) * 35 / WALLPAPER_HEIGHT,
                progress_user_data);
    }
    core_free(decode_handle);
    if(progress_cb != NULL)
        progress_cb(90, progress_user_data);
    return CRAZYPOD_WALLPAPER_APPLY_OK;
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
    result = render_crop_from_original(
        path, source_descriptor,
        crop_x, crop_y, crop_width, crop_height,
        destination, progress_cb, progress_user_data);
    if(result != CRAZYPOD_WALLPAPER_APPLY_OK)
        return result;
    if(!save_native_wallpaper(
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
    if(target == CRAZYPOD_WALLPAPER_HOME)
        release_frosted_capsule();
    if(progress_cb != NULL)
        progress_cb(100, progress_user_data);
    return CRAZYPOD_WALLPAPER_APPLY_OK;
}

void crazypod_wallpaper_clear(enum crazypod_wallpaper_target target)
{
    if(!valid_target(target))
        return;
    set_target_valid(target, false);
    remove(native_wallpaper_path(target));
    if(target == CRAZYPOD_WALLPAPER_HOME)
        release_frosted_capsule();
    crazypod_appearance_set_wallpaper(
        appearance_field_for_target(target), "");
}

bool crazypod_wallpaper_prepare_frosted_capsule(
    uint32_t tint, unsigned tint_opa)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    fb_data *frosted_capsule_pixels;
    const fb_data *source_pixels;
    fb_data solid_pixel;
    fb_data *sample_pixels;
    fb_data *scratch_pixels;
    size_t sample_count =
        crazypod_image_glass_sample_pixels(
            FROSTED_CAPSULE_WIDTH, FROSTED_CAPSULE_HEIGHT);
    int source_width;
    int source_height;
    int source_stride;
    int source_x;
    int source_y;
    int source_region_width;
    int source_region_height;
    int workspace_handle;
    uint32_t source_key;

    if(custom_home_valid) {
        source_key = 1;
        source_pixels = custom_home_pixels;
        source_width = WALLPAPER_WIDTH;
        source_height = WALLPAPER_HEIGHT;
        source_stride = WALLPAPER_WIDTH;
        source_x = FROSTED_CAPSULE_X;
        source_y = FROSTED_CAPSULE_Y;
        source_region_width = FROSTED_CAPSULE_WIDTH;
        source_region_height = FROSTED_CAPSULE_HEIGHT;
    }
    else if(appearance->home_wallpaper[0] == '\0' &&
            appearance->home_background == 0 && wallpaper_valid) {
        source_key = 2;
        source_pixels = wallpaper_pixels;
        source_width = WALLPAPER_WIDTH;
        source_height = WALLPAPER_HEIGHT;
        source_stride = WALLPAPER_WIDTH;
        source_x = FROSTED_CAPSULE_X;
        source_y = FROSTED_CAPSULE_Y;
        source_region_width = FROSTED_CAPSULE_WIDTH;
        source_region_height = FROSTED_CAPSULE_HEIGHT;
    }
    else {
        uint32_t color = crazypod_appearance_home_color();

        source_key = 0x03000000u | color;
        solid_pixel = LCD_RGBPACK(
            (color >> 16) & 0xff,
            (color >> 8) & 0xff,
            color & 0xff);
        source_pixels = &solid_pixel;
        source_width = 1;
        source_height = 1;
        source_stride = 1;
        source_x = 0;
        source_y = 0;
        source_region_width = 1;
        source_region_height = 1;
    }
    if(frosted_capsule_valid &&
       frosted_capsule_source_key == source_key &&
       frosted_capsule_tint == tint &&
       frosted_capsule_tint_opa == tint_opa)
        return true;
    if(frosted_capsule_valid)
        release_frosted_capsule();

    frosted_capsule_handle = core_alloc_ex(
        FROSTED_CAPSULE_BYTES, &buflib_ops_locked);
    if(frosted_capsule_handle < 0)
        return false;
    workspace_handle = core_alloc(
        sample_count * 2 * sizeof(fb_data));
    if(workspace_handle < 0) {
        frosted_capsule_handle = core_free(frosted_capsule_handle);
        return false;
    }

    frosted_capsule_pixels = core_get_data(frosted_capsule_handle);
    sample_pixels = core_get_data(workspace_handle);
    scratch_pixels = sample_pixels + sample_count;
    if(!crazypod_image_render_glass_rgb565(
           source_pixels, source_width, source_height, source_stride,
           source_x, source_y,
           source_region_width, source_region_height,
           tint, tint_opa,
           sample_pixels, scratch_pixels, sample_count,
           frosted_capsule_pixels,
           FROSTED_CAPSULE_WIDTH, FROSTED_CAPSULE_HEIGHT)) {
        core_free(workspace_handle);
        frosted_capsule_handle = core_free(frosted_capsule_handle);
        return false;
    }
    {
        int y;

        for(y = 0; y < FROSTED_CAPSULE_HEIGHT; ++y) {
            int x;

            for(x = 0; x < FROSTED_CAPSULE_WIDTH; ++x) {
                unsigned alpha = rounded_capsule_alpha(x, y);
                int sample_x;
                int sample_y;
                fb_data background;
                fb_data *material;

                if(alpha == 255)
                    continue;
                sample_x = clamp_coordinate(
                    source_x +
                        x * source_region_width /
                            FROSTED_CAPSULE_WIDTH,
                    source_width);
                sample_y = clamp_coordinate(
                    source_y +
                        y * source_region_height /
                            FROSTED_CAPSULE_HEIGHT,
                    source_height);
                background =
                    source_pixels[sample_y * source_stride + sample_x];
                material =
                    frosted_capsule_pixels +
                    y * FROSTED_CAPSULE_WIDTH + x;
                *material =
                    blend_capsule_edge(*material, background, alpha);
            }
        }
    }
    core_free(workspace_handle);
    if(!crazypod_image_configure_rgb565(
           &frosted_capsule_descriptor,
           frosted_capsule_pixels,
           FROSTED_CAPSULE_WIDTH, FROSTED_CAPSULE_HEIGHT)) {
        frosted_capsule_handle = core_free(frosted_capsule_handle);
        return false;
    }
    frosted_capsule_valid = true;
    frosted_capsule_source_key = source_key;
    frosted_capsule_tint = tint;
    frosted_capsule_tint_opa = tint_opa;
    return true;
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
    return frosted_capsule_valid
        ? &frosted_capsule_descriptor : NULL;
}

#endif
