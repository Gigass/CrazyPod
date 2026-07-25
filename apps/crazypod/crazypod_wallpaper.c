#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "buflib.h"
#include "bmp.h"
#include "core_alloc.h"
#include "file.h"
#include "jpeg_load.h"
#include "lcd.h"
#include "resize.h"
#include "string-extra.h"
#include "system.h"

#include "crazypod_appearance.h"
#include "crazypod_wallpaper.h"

#define WALLPAPER_WIDTH LCD_WIDTH
#define WALLPAPER_HEIGHT LCD_HEIGHT
#define WALLPAPER_ROW_BYTES (WALLPAPER_WIDTH * 4)
#define WALLPAPER_BYTES (WALLPAPER_ROW_BYTES * WALLPAPER_HEIGHT)
#define CUSTOM_WALLPAPER_BYTES \
    (WALLPAPER_WIDTH * WALLPAPER_HEIGHT * sizeof(fb_data))
#define WALLPAPER_DECODE_BYTES \
    (CUSTOM_WALLPAPER_BYTES + 64 * 1024)
#define WALLPAPER_PATH "/.rockbox/crazypod/default-home.bmp"
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

static uint8_t wallpaper_pixels[WALLPAPER_BYTES]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t wallpaper_descriptor;
static fb_data custom_home_pixels[WALLPAPER_WIDTH * WALLPAPER_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data custom_menu_pixels[WALLPAPER_WIDTH * WALLPAPER_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data wallpaper_decode_buffer[
    WALLPAPER_DECODE_BYTES / sizeof(fb_data)]
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
    int format = FORMAT_NATIVE | FORMAT_RESIZE;
    int result;

    if(path == NULL || path[0] == '\0')
        return false;
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = WALLPAPER_WIDTH;
    bitmap.height = WALLPAPER_HEIGHT;
    bitmap.data = (unsigned char *)wallpaper_decode_buffer;

    if(path_has_extension(path, ".jpg") ||
       path_has_extension(path, ".jpeg"))
        result = read_jpeg_file(path, &bitmap,
                                sizeof(wallpaper_decode_buffer),
                                format, &format_native);
    else if(path_has_extension(path, ".bmp"))
        result = read_bmp_file(path, &bitmap,
                               sizeof(wallpaper_decode_buffer),
                               format, &format_native);
    else
        return false;

    if(result < 0 || bitmap.width != WALLPAPER_WIDTH ||
       bitmap.height != WALLPAPER_HEIGHT || bitmap.data == NULL)
        return false;
    memcpy(pixels, bitmap.data, CUSTOM_WALLPAPER_BYTES);
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->header.magic = LV_IMAGE_HEADER_MAGIC;
    descriptor->header.cf = LV_COLOR_FORMAT_RGB565;
    descriptor->header.w = WALLPAPER_WIDTH;
    descriptor->header.h = WALLPAPER_HEIGHT;
    descriptor->header.stride = WALLPAPER_WIDTH * sizeof(fb_data);
    descriptor->data_size = CUSTOM_WALLPAPER_BYTES;
    descriptor->data = (const uint8_t *)pixels;
    return true;
}

void crazypod_wallpaper_reload_custom(void)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();

    custom_home_valid = appearance->home_wallpaper[0] != '\0' &&
        decode_custom_wallpaper(
            appearance->home_wallpaper, custom_home_pixels,
            &custom_home_descriptor);
    custom_menu_valid = appearance->menu_wallpaper[0] != '\0' &&
        decode_custom_wallpaper(
            appearance->menu_wallpaper, custom_menu_pixels,
            &custom_menu_descriptor);
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
            const uint8_t *source =
                wallpaper_pixels +
                source_y * WALLPAPER_ROW_BYTES + source_x * 4;

            blue += source[0];
            green += source[1];
            red += source[2];
        }
        destination[x * 3] = blue / FROSTED_SAMPLE_COUNT;
        destination[x * 3 + 1] = green / FROSTED_SAMPLE_COUNT;
        destination[x * 3 + 2] = red / FROSTED_SAMPLE_COUNT;
    }
}

void crazypod_wallpaper_init(void)
{
    uint8_t header[54];
    uint8_t row[WALLPAPER_ROW_BYTES];
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
        if(!read_exact(fd, row, sizeof(row))) {
            close(fd);
            return;
        }
        memcpy(wallpaper_pixels + target_row * WALLPAPER_ROW_BYTES,
               row, sizeof(row));
    }
    close(fd);

    memset(&wallpaper_descriptor, 0, sizeof(wallpaper_descriptor));
    wallpaper_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    wallpaper_descriptor.header.cf = LV_COLOR_FORMAT_ARGB8888;
    wallpaper_descriptor.header.w = WALLPAPER_WIDTH;
    wallpaper_descriptor.header.h = WALLPAPER_HEIGHT;
    wallpaper_descriptor.header.stride = WALLPAPER_ROW_BYTES;
    wallpaper_descriptor.data_size = WALLPAPER_BYTES;
    wallpaper_descriptor.data = wallpaper_pixels;
    wallpaper_valid = true;
}

bool crazypod_wallpaper_select(bool menu, const char *path)
{
    bool loaded;

    if(menu) {
        loaded = decode_custom_wallpaper(
            path, custom_menu_pixels, &custom_menu_descriptor);
        if(!loaded)
            return false;
        custom_menu_valid = true;
    }
    else {
        loaded = decode_custom_wallpaper(
            path, custom_home_pixels, &custom_home_descriptor);
        if(!loaded)
            return false;
        custom_home_valid = true;
        release_frosted_capsule();
    }
    return crazypod_appearance_set_wallpaper(menu, path);
}

void crazypod_wallpaper_clear(bool menu)
{
    if(menu)
        custom_menu_valid = false;
    else {
        custom_home_valid = false;
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
            const uint8_t *background =
                wallpaper_pixels +
                (FROSTED_CAPSULE_Y + y) * WALLPAPER_ROW_BYTES +
                (FROSTED_CAPSULE_X + x) * 4;
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
                     background[channel] * inverse_alpha + 127) / 255;
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
