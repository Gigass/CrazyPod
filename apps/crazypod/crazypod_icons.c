#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "file.h"
#include "system.h"

#include "crazypod_appearance.h"
#include "crazypod_icons.h"

#define ICON_WIDTH 160
#define ICON_HEIGHT 160
#define ICON_ROW_BYTES (ICON_WIDTH * 4)
#define ICON_BYTES (ICON_ROW_BYTES * ICON_HEIGHT)

struct icon_slot {
    uint8_t premultiplied[ICON_BYTES] CACHEALIGN_AT_LEAST_ATTR(16);
    struct crazypod_icon image;
    bool valid;
};

static struct icon_slot slots[CRAZYPOD_ICON_COUNT];
static int loaded_theme = -1;

static const char *const theme_paths[CRAZYPOD_ICON_THEME_COUNT] = {
    "basic", "cel_frame", "anime_pop", "mecha_spec",
    "toy", "y2k", "flat", "skeuo",
    "lucid_pop", "noize_bloom", "soft_skeuo", "acrylic",
    "ink", "sticker", "sticker2", "voxel"
};

static const char *const app_paths[CRAZYPOD_ICON_COUNT] = {
    "music", "podcasts", "mini_apps", "shuffle",
    "screen_lock", "photos", "diy", "fitness", "books", "notes",
    "clock", "contacts", "calendar", "stopwatch",
    "extras", "settings"
};

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

static bool load_icon(struct icon_slot *slot, const char *path)
{
    uint8_t header[54];
    uint8_t row[ICON_ROW_BYTES];
    uint32_t pixel_offset;
    int32_t width;
    int32_t height;
    bool top_down;
    int fd;
    int source_row;

    fd = open(path, O_RDONLY);
    if(fd < 0)
        return false;
    if(!read_exact(fd, header, sizeof(header)) ||
       header[0] != 'B' || header[1] != 'M') {
        close(fd);
        return false;
    }

    pixel_offset = read_le32(header + 10);
    width = (int32_t)read_le32(header + 18);
    height = (int32_t)read_le32(header + 22);
    top_down = height < 0;
    if(width != ICON_WIDTH ||
       (height != ICON_HEIGHT && height != -ICON_HEIGHT) ||
       read_le16(header + 26) != 1 ||
       read_le16(header + 28) != 32 ||
       read_le32(header + 30) != 0 ||
       lseek(fd, (off_t)pixel_offset, SEEK_SET) < 0) {
        close(fd);
        return false;
    }

    for(source_row = 0; source_row < ICON_HEIGHT; ++source_row) {
        int target_row = top_down
            ? source_row : ICON_HEIGHT - 1 - source_row;
        uint8_t *target =
            slot->premultiplied + target_row * ICON_ROW_BYTES;
        int pixel;

        if(!read_exact(fd, row, sizeof(row))) {
            close(fd);
            return false;
        }
        for(pixel = 0; pixel < ICON_WIDTH; ++pixel) {
            const uint8_t *source = row + pixel * 4;
            uint8_t *destination = target + pixel * 4;
            unsigned alpha = source[3];
            int channel;

            for(channel = 0; channel < 3; ++channel) {
                unsigned product = source[channel] * alpha + 128;
                destination[channel] =
                    (product + (product >> 8)) >> 8;
            }
            destination[3] = alpha;
        }
    }
    close(fd);

    slot->image.pixels = slot->premultiplied;
    slot->image.width = ICON_WIDTH;
    slot->image.height = ICON_HEIGHT;
    slot->image.stride = ICON_ROW_BYTES;
    slot->valid = true;
    return true;
}

void crazypod_icons_init(void)
{
    memset(slots, 0, sizeof(slots));
    loaded_theme = -1;
    crazypod_icons_load_theme(crazypod_appearance_get()->icon_theme);
}

void crazypod_icons_load_theme(int theme)
{
    char path[MAX_PATH];
    bool complete = true;
    int i;

    if(theme < 0 || theme >= CRAZYPOD_ICON_THEME_COUNT)
        theme = 0;
    if(theme == loaded_theme)
        return;
    for(i = 0; i < CRAZYPOD_ICON_COUNT; ++i) {
        snprintf(path, sizeof(path),
                 "/.rockbox/crazypod/icons/%s/%s.bmp",
                 theme_paths[theme], app_paths[i]);
        if(!load_icon(&slots[i], path))
            complete = false;
    }
    if(complete)
        loaded_theme = theme;
}

const struct crazypod_icon *crazypod_icon_get(int index)
{
    if(index < 0 || index >= CRAZYPOD_ICON_COUNT || !slots[index].valid)
        return NULL;
    return &slots[index].image;
}

#endif
