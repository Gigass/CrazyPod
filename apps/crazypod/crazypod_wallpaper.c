#include "config.h"

#ifdef IPOD_6G

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

#define WALLPAPER_WIDTH LCD_WIDTH
#define WALLPAPER_HEIGHT LCD_HEIGHT
#define WALLPAPER_SOURCE_ROW_BYTES (WALLPAPER_WIDTH * 4)
#define CUSTOM_WALLPAPER_BYTES \
    (WALLPAPER_WIDTH * WALLPAPER_HEIGHT * sizeof(fb_data))
#define WALLPAPER_DECODE_BYTES \
    (CUSTOM_WALLPAPER_BYTES + 64 * 1024)
#define WALLPAPER_PATH "/.rockbox/crazypod/default-home.bmp"
#define CUSTOM_WALLPAPER_DIRECTORY "/.crazypod"
#define CUSTOM_WALLPAPER_CACHE_DIRECTORY "/.crazypod/cache"
#define CUSTOM_HOME_NATIVE_PATH "/.crazypod/cache/home.wall"
#define CUSTOM_HOME_NATIVE_TEMP "/.crazypod/cache/home.wall.tmp"
#define CUSTOM_MENU_NATIVE_PATH "/.crazypod/cache/menu.wall"
#define CUSTOM_MENU_NATIVE_TEMP "/.crazypod/cache/menu.wall.tmp"
#define CUSTOM_WALLPAPER_MAGIC 0x43505731u
#define CUSTOM_WALLPAPER_VERSION 1
#define FROSTED_CAPSULE_X 8
#define FROSTED_CAPSULE_Y 174
#define FROSTED_CAPSULE_WIDTH 304
#define FROSTED_CAPSULE_HEIGHT 58
#define FROSTED_BLUR_RADIUS 7
#define FROSTED_CORNER_RADIUS 29
#define FROSTED_SAMPLE_COUNT (FROSTED_BLUR_RADIUS * 2 + 1)
#define FROSTED_WORK_HEIGHT \
    (FROSTED_CAPSULE_HEIGHT + FROSTED_BLUR_RADIUS * 2)
#define FROSTED_CAPSULE_ROW_BYTES \
    (FROSTED_CAPSULE_WIDTH * sizeof(fb_data))
#define FROSTED_CAPSULE_BYTES \
    (FROSTED_CAPSULE_ROW_BYTES * FROSTED_CAPSULE_HEIGHT)
#define FROSTED_RING_ROW_BYTES (FROSTED_CAPSULE_WIDTH * 3)
#define FROSTED_RING_BYTES \
    (FROSTED_RING_ROW_BYTES * FROSTED_SAMPLE_COUNT)

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
static lv_image_dsc_t custom_home_descriptor;
static lv_image_dsc_t custom_menu_descriptor;
static lv_image_dsc_t frosted_capsule_descriptor;
static bool wallpaper_valid;
static bool custom_home_valid;
static bool custom_menu_valid;
static bool frosted_capsule_valid;
static int frosted_capsule_handle = -1;

static void release_frosted_capsule(void)
{
    frosted_capsule_valid = false;
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

static const char *native_wallpaper_path(bool menu)
{
    return menu ? CUSTOM_MENU_NATIVE_PATH
                : CUSTOM_HOME_NATIVE_PATH;
}

static const char *native_wallpaper_temp(bool menu)
{
    return menu ? CUSTOM_MENU_NATIVE_TEMP
                : CUSTOM_HOME_NATIVE_TEMP;
}

static bool load_native_wallpaper(
    bool menu, const char *source_path, fb_data *pixels,
    lv_image_dsc_t *descriptor)
{
    struct custom_wallpaper_header header;
    int fd = open(native_wallpaper_path(menu), O_RDONLY);
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
    bool menu, const char *source_path, const fb_data *pixels)
{
    struct custom_wallpaper_header header;
    const char *temporary = native_wallpaper_temp(menu);
    const char *published = native_wallpaper_path(menu);
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
    if(fd < 0)
        return false;
    complete =
        write_exact(fd, &header, sizeof(header)) &&
        write_exact(fd, pixels, CUSTOM_WALLPAPER_BYTES) &&
        fsync(fd) >= 0;
    close(fd);
    if(!complete || rename(temporary, published) < 0) {
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
    bool menu, const char *path, fb_data *pixels,
    lv_image_dsc_t *descriptor)
{
    if(load_native_wallpaper(menu, path, pixels, descriptor))
        return true;
    if(!decode_custom_wallpaper(path, pixels, descriptor))
        return false;
    save_native_wallpaper(menu, path, pixels);
    return true;
}

void crazypod_wallpaper_reload_custom(void)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();

    custom_home_valid = appearance->home_wallpaper[0] != '\0' &&
        load_custom_wallpaper(
            false, appearance->home_wallpaper,
            custom_home_pixels, &custom_home_descriptor);
    custom_menu_valid = appearance->menu_wallpaper[0] != '\0' &&
        load_custom_wallpaper(
            true, appearance->menu_wallpaper,
            custom_menu_pixels, &custom_menu_descriptor);
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

static void blur_wallpaper_row(uint8_t *destination, int work_y)
{
    int source_y = clamp_coordinate(
        FROSTED_CAPSULE_Y - FROSTED_BLUR_RADIUS + work_y,
        WALLPAPER_HEIGHT);
    int x;

    for(x = 0; x < FROSTED_CAPSULE_WIDTH; ++x) {
        unsigned blue = 0;
        unsigned green = 0;
        unsigned red = 0;
        int offset;

        for(offset = -FROSTED_BLUR_RADIUS;
            offset <= FROSTED_BLUR_RADIUS; ++offset) {
            int source_x = clamp_coordinate(
                FROSTED_CAPSULE_X + x + offset, WALLPAPER_WIDTH);
            const fb_data *source =
                wallpaper_pixels +
                source_y * WALLPAPER_WIDTH + source_x;

            blue += RGB_UNPACK_BLUE(*source);
            green += RGB_UNPACK_GREEN(*source);
            red += RGB_UNPACK_RED(*source);
        }
        destination[x * 3] = blue / FROSTED_SAMPLE_COUNT;
        destination[x * 3 + 1] = green / FROSTED_SAMPLE_COUNT;
        destination[x * 3 + 2] = red / FROSTED_SAMPLE_COUNT;
    }
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

bool crazypod_wallpaper_select(bool menu, const char *path)
{
    bool loaded;

    if(menu) {
        loaded = decode_custom_wallpaper(
            path, custom_menu_pixels, &custom_menu_descriptor);
        if(!loaded ||
           !save_native_wallpaper(true, path, custom_menu_pixels))
            return false;
        custom_menu_valid = true;
    }
    else {
        loaded = decode_custom_wallpaper(
            path, custom_home_pixels, &custom_home_descriptor);
        if(!loaded ||
           !save_native_wallpaper(false, path, custom_home_pixels))
            return false;
        custom_home_valid = true;
        release_frosted_capsule();
    }
    return crazypod_appearance_set_wallpaper(menu, path);
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

bool crazypod_wallpaper_apply_crop(
    bool menu, const char *path,
    const lv_image_dsc_t *source_descriptor,
    int crop_x, int crop_y, int crop_width, int crop_height)
{
    const fb_data *source;
    fb_data *destination =
        menu ? custom_menu_pixels : custom_home_pixels;
    lv_image_dsc_t *descriptor =
        menu ? &custom_menu_descriptor : &custom_home_descriptor;
    int source_width;
    int source_height;
    int source_stride;
    int y;

    if(path == NULL || path[0] == '\0' ||
       source_descriptor == NULL ||
       source_descriptor->header.magic != LV_IMAGE_HEADER_MAGIC ||
       source_descriptor->header.cf != LV_COLOR_FORMAT_RGB565)
        return false;
    source_width = source_descriptor->header.w;
    source_height = source_descriptor->header.h;
    source_stride =
        source_descriptor->header.stride / sizeof(fb_data);
    if(source_width <= 0 || source_height <= 0 ||
       source_stride < source_width ||
       crop_x < 0 || crop_y < 0 ||
       crop_width <= 1 || crop_height <= 1 ||
       crop_x + crop_width > source_width ||
       crop_y + crop_height > source_height)
        return false;
    source = (const fb_data *)source_descriptor->data;
    for(y = 0; y < WALLPAPER_HEIGHT; ++y) {
        int source_y_q8 = crop_y * 256 +
            y * (crop_height - 1) * 256 /
            (WALLPAPER_HEIGHT - 1);
        int x;

        for(x = 0; x < WALLPAPER_WIDTH; ++x) {
            int source_x_q8 = crop_x * 256 +
                x * (crop_width - 1) * 256 /
                (WALLPAPER_WIDTH - 1);

            destination[y * WALLPAPER_WIDTH + x] =
                sample_crop_pixel(
                    source, source_width, source_height,
                    source_stride, source_x_q8, source_y_q8);
        }
    }
    if(!save_native_wallpaper(menu, path, destination) ||
       !crazypod_appearance_set_wallpaper(menu, path))
        return false;
    if(!crazypod_image_configure_rgb565(
           descriptor, destination,
           WALLPAPER_WIDTH, WALLPAPER_HEIGHT))
        return false;
    if(menu)
        custom_menu_valid = true;
    else {
        custom_home_valid = true;
        release_frosted_capsule();
    }
    return true;
}

void crazypod_wallpaper_clear(bool menu)
{
    if(menu) {
        custom_menu_valid = false;
        remove(CUSTOM_MENU_NATIVE_PATH);
    }
    else {
        custom_home_valid = false;
        remove(CUSTOM_HOME_NATIVE_PATH);
        release_frosted_capsule();
    }
    crazypod_appearance_set_wallpaper(menu, "");
}

bool crazypod_wallpaper_prepare_frosted_capsule(void)
{
    fb_data *frosted_capsule_pixels;
    uint8_t *frosted_ring;
    int ring_handle;
    int work_y;
    int y;

    if(frosted_capsule_valid)
        return true;
    if(!wallpaper_valid)
        return false;

    frosted_capsule_handle = core_alloc_ex(
        FROSTED_CAPSULE_BYTES, &buflib_ops_locked);
    if(frosted_capsule_handle < 0)
        return false;
    ring_handle = core_alloc(FROSTED_RING_BYTES);
    if(ring_handle < 0) {
        frosted_capsule_handle = core_free(frosted_capsule_handle);
        return false;
    }

    frosted_capsule_pixels = core_get_data(frosted_capsule_handle);
    frosted_ring = core_get_data(ring_handle);
    for(work_y = 0; work_y < FROSTED_SAMPLE_COUNT; ++work_y) {
        blur_wallpaper_row(
            frosted_ring +
                (work_y % FROSTED_SAMPLE_COUNT) * FROSTED_RING_ROW_BYTES,
            work_y);
    }

    for(y = 0; y < FROSTED_CAPSULE_HEIGHT; ++y) {
        int x;

        for(x = 0; x < FROSTED_CAPSULE_WIDTH; ++x) {
            unsigned channel_sum[3] = { 0, 0, 0 };
            const fb_data *background =
                wallpaper_pixels +
                (FROSTED_CAPSULE_Y + y) * WALLPAPER_WIDTH +
                FROSTED_CAPSULE_X + x;
            unsigned material[3];
            unsigned composited[3];
            unsigned alpha = rounded_capsule_alpha(x, y);
            unsigned inverse_alpha = 255 - alpha;
            unsigned sheen =
                (unsigned)(FROSTED_CAPSULE_HEIGHT - y) * 8 /
                FROSTED_CAPSULE_HEIGHT;
            int sample;
            int channel;

            for(sample = 0; sample < FROSTED_SAMPLE_COUNT; ++sample) {
                const uint8_t *source =
                    frosted_ring +
                    ((y + sample) % FROSTED_SAMPLE_COUNT) *
                        FROSTED_RING_ROW_BYTES +
                    x * 3;
                channel_sum[0] += source[0];
                channel_sum[1] += source[1];
                channel_sum[2] += source[2];
            }
            for(channel = 0; channel < 3; ++channel) {
                unsigned blurred =
                    channel_sum[channel] / FROSTED_SAMPLE_COUNT;

                material[channel] =
                    (blurred * 168 + sheen * 256) >> 8;
                composited[channel] =
                    (material[channel] * alpha +
                     (channel == 0 ? RGB_UNPACK_BLUE(*background) :
                      channel == 1 ? RGB_UNPACK_GREEN(*background) :
                                     RGB_UNPACK_RED(*background)) *
                         inverse_alpha + 127) / 255;
            }
            frosted_capsule_pixels[
                y * FROSTED_CAPSULE_WIDTH + x] =
                    LCD_RGBPACK(composited[2],
                                composited[1],
                                composited[0]);
        }

        work_y = y + FROSTED_SAMPLE_COUNT;
        if(work_y < FROSTED_WORK_HEIGHT) {
            blur_wallpaper_row(
                frosted_ring +
                    (y % FROSTED_SAMPLE_COUNT) *
                        FROSTED_RING_ROW_BYTES,
                work_y);
        }
    }
    core_free(ring_handle);

    memset(&frosted_capsule_descriptor, 0,
           sizeof(frosted_capsule_descriptor));
    frosted_capsule_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    frosted_capsule_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
    frosted_capsule_descriptor.header.w = FROSTED_CAPSULE_WIDTH;
    frosted_capsule_descriptor.header.h = FROSTED_CAPSULE_HEIGHT;
    frosted_capsule_descriptor.header.stride =
        FROSTED_CAPSULE_ROW_BYTES;
    frosted_capsule_descriptor.data_size = FROSTED_CAPSULE_BYTES;
    frosted_capsule_descriptor.data =
        (const uint8_t *)frosted_capsule_pixels;
    frosted_capsule_valid = true;
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

const lv_image_dsc_t *crazypod_frosted_wallpaper_capsule(void)
{
    return frosted_capsule_valid
        ? &frosted_capsule_descriptor : NULL;
}

#endif
