#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "file.h"
#include "system.h"

#include "crazypod_wallpaper.h"

#define WALLPAPER_WIDTH LCD_WIDTH
#define WALLPAPER_HEIGHT LCD_HEIGHT
#define WALLPAPER_ROW_BYTES (WALLPAPER_WIDTH * 4)
#define WALLPAPER_BYTES (WALLPAPER_ROW_BYTES * WALLPAPER_HEIGHT)
#define WALLPAPER_PATH "/.rockbox/crazypod/default-home.bmp"
#define FROSTED_CAPSULE_X 8
#define FROSTED_CAPSULE_Y 174
#define FROSTED_CAPSULE_WIDTH 304
#define FROSTED_CAPSULE_HEIGHT 58
#define FROSTED_CAPSULE_RADIUS 7
#define FROSTED_WORK_HEIGHT \
    (FROSTED_CAPSULE_HEIGHT + FROSTED_CAPSULE_RADIUS * 2)
#define FROSTED_CAPSULE_ROW_BYTES (FROSTED_CAPSULE_WIDTH * 4)
#define FROSTED_CAPSULE_BYTES \
    (FROSTED_CAPSULE_ROW_BYTES * FROSTED_CAPSULE_HEIGHT)

static uint8_t wallpaper_pixels[WALLPAPER_BYTES]
    CACHEALIGN_AT_LEAST_ATTR(16);
static uint8_t frosted_capsule_pixels[FROSTED_CAPSULE_BYTES]
    CACHEALIGN_AT_LEAST_ATTR(16);
static uint8_t frosted_horizontal[
    FROSTED_CAPSULE_WIDTH * FROSTED_WORK_HEIGHT * 3]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t wallpaper_descriptor;
static lv_image_dsc_t frosted_capsule_descriptor;
static bool wallpaper_valid;
static bool frosted_capsule_valid;

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

static int clamp_coordinate(int value, int maximum)
{
    if(value < 0)
        return 0;
    if(value >= maximum)
        return maximum - 1;
    return value;
}

static void prepare_frosted_capsule(void)
{
    const int sample_count = FROSTED_CAPSULE_RADIUS * 2 + 1;
    int work_y;
    int y;

    /*
     * LVGL has no backdrop-filter on this target. Build a real two-pass box
     * blur from the wallpaper pixels behind the capsule once at startup.
     * Sampling outside the capsule bounds prevents hard seams at its edges.
     */
    for(work_y = 0; work_y < FROSTED_WORK_HEIGHT; ++work_y) {
        int source_y = clamp_coordinate(
            FROSTED_CAPSULE_Y - FROSTED_CAPSULE_RADIUS + work_y,
            WALLPAPER_HEIGHT);
        int x;

        for(x = 0; x < FROSTED_CAPSULE_WIDTH; ++x) {
            unsigned blue = 0;
            unsigned green = 0;
            unsigned red = 0;
            int offset;
            uint8_t *destination =
                frosted_horizontal +
                (work_y * FROSTED_CAPSULE_WIDTH + x) * 3;

            for(offset = -FROSTED_CAPSULE_RADIUS;
                offset <= FROSTED_CAPSULE_RADIUS; ++offset) {
                int source_x = clamp_coordinate(
                    FROSTED_CAPSULE_X + x + offset,
                    WALLPAPER_WIDTH);
                const uint8_t *source =
                    wallpaper_pixels +
                    source_y * WALLPAPER_ROW_BYTES + source_x * 4;

                blue += source[0];
                green += source[1];
                red += source[2];
            }
            destination[0] = blue / sample_count;
            destination[1] = green / sample_count;
            destination[2] = red / sample_count;
        }
    }

    for(y = 0; y < FROSTED_CAPSULE_HEIGHT; ++y) {
        int x;
        for(x = 0; x < FROSTED_CAPSULE_WIDTH; ++x) {
            unsigned channel_sum[3] = { 0, 0, 0 };
            uint8_t *destination =
                frosted_capsule_pixels +
                y * FROSTED_CAPSULE_ROW_BYTES + x * 4;
            int sample_y;
            int channel;

            for(sample_y = y; sample_y < y + sample_count; ++sample_y) {
                const uint8_t *source =
                    frosted_horizontal +
                    (sample_y * FROSTED_CAPSULE_WIDTH + x) * 3;
                channel_sum[0] += source[0];
                channel_sum[1] += source[1];
                channel_sum[2] += source[2];
            }
            for(channel = 0; channel < 3; ++channel) {
                unsigned blurred = channel_sum[channel] / sample_count;
                unsigned sheen =
                    (unsigned)(FROSTED_CAPSULE_HEIGHT - y) * 8 /
                    FROSTED_CAPSULE_HEIGHT;

                /* Dark material tint keeps white metadata readable. */
                destination[channel] =
                    (uint8_t)((blurred * 168 + sheen * 256) >> 8);
            }
            destination[3] = 255;
        }
    }

    memset(&frosted_capsule_descriptor, 0,
           sizeof(frosted_capsule_descriptor));
    frosted_capsule_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    frosted_capsule_descriptor.header.cf = LV_COLOR_FORMAT_ARGB8888;
    frosted_capsule_descriptor.header.w = FROSTED_CAPSULE_WIDTH;
    frosted_capsule_descriptor.header.h = FROSTED_CAPSULE_HEIGHT;
    frosted_capsule_descriptor.header.stride =
        FROSTED_CAPSULE_ROW_BYTES;
    frosted_capsule_descriptor.data_size = FROSTED_CAPSULE_BYTES;
    frosted_capsule_descriptor.data = frosted_capsule_pixels;
    frosted_capsule_valid = true;
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
    frosted_capsule_valid = false;
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
    prepare_frosted_capsule();
}

const lv_image_dsc_t *crazypod_default_wallpaper(void)
{
    return wallpaper_valid ? &wallpaper_descriptor : NULL;
}

const lv_image_dsc_t *crazypod_frosted_wallpaper_capsule(void)
{
    return frosted_capsule_valid
        ? &frosted_capsule_descriptor : NULL;
}

#endif
