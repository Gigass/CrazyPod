#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "../crazypod_image.h"
#include "crazypod_wallpaper_store.h"

#define WIDTH LCD_WIDTH
#define HEIGHT LCD_HEIGHT
#define PIXEL_BYTES (WIDTH * HEIGHT * sizeof(fb_data))
#define STATE_DIRECTORY "/.crazypod"
#define CACHE_DIRECTORY STATE_DIRECTORY "/cache"
#define HOME_PATH CACHE_DIRECTORY "/home.wall"
#define HOME_TEMP CACHE_DIRECTORY "/home.wall.tmp"
#define MENU_PATH CACHE_DIRECTORY "/menu.wall"
#define MENU_TEMP CACHE_DIRECTORY "/menu.wall.tmp"
#define LOCK_PATH CACHE_DIRECTORY "/lock.wall"
#define LOCK_TEMP CACHE_DIRECTORY "/lock.wall.tmp"
#define STORE_MAGIC 0x43505731u
#define STORE_VERSION 1

struct store_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t source_key;
    uint16_t width;
    uint16_t height;
    uint32_t data_size;
};

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

static uint32_t source_key(const char *path)
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

static const char *published_path(
    enum crazypod_wallpaper_target target)
{
    if(target == CRAZYPOD_WALLPAPER_MENU)
        return MENU_PATH;
    if(target == CRAZYPOD_WALLPAPER_LOCK)
        return LOCK_PATH;
    return HOME_PATH;
}

static const char *temporary_path(
    enum crazypod_wallpaper_target target)
{
    if(target == CRAZYPOD_WALLPAPER_MENU)
        return MENU_TEMP;
    if(target == CRAZYPOD_WALLPAPER_LOCK)
        return LOCK_TEMP;
    return HOME_TEMP;
}

void crazypod_wallpaper_store_init(void)
{
    mkdir(STATE_DIRECTORY);
    mkdir(CACHE_DIRECTORY);
}

bool crazypod_wallpaper_store_load(
    enum crazypod_wallpaper_target target,
    const char *source_path, fb_data *pixels,
    lv_image_dsc_t *descriptor)
{
    struct store_header header;
    int fd = open(published_path(target), O_RDONLY);
    bool valid;

    if(fd < 0)
        return false;
    valid =
        read_exact(fd, &header, sizeof(header)) &&
        header.magic == STORE_MAGIC &&
        header.version == STORE_VERSION &&
        header.header_size == sizeof(header) &&
        header.source_key == source_key(source_path) &&
        header.width == WIDTH && header.height == HEIGHT &&
        header.data_size == PIXEL_BYTES &&
        read_exact(fd, pixels, PIXEL_BYTES);
    close(fd);
    return valid && crazypod_image_configure_rgb565(
        descriptor, pixels, WIDTH, HEIGHT);
}

bool crazypod_wallpaper_store_save(
    enum crazypod_wallpaper_target target,
    const char *source_path, const fb_data *pixels,
    enum crazypod_wallpaper_apply_result *error)
{
    struct store_header header;
    const char *temporary = temporary_path(target);
    const char *published = published_path(target);
    bool complete;
    int fd;

    crazypod_wallpaper_store_init();
    memset(&header, 0, sizeof(header));
    header.magic = STORE_MAGIC;
    header.version = STORE_VERSION;
    header.header_size = sizeof(header);
    header.source_key = source_key(source_path);
    header.width = WIDTH;
    header.height = HEIGHT;
    header.data_size = PIXEL_BYTES;
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0) {
        if(error != NULL)
            *error = CRAZYPOD_WALLPAPER_APPLY_CACHE_OPEN_FAILED;
        return false;
    }
    complete = write_exact(fd, &header, sizeof(header)) &&
        write_exact(fd, pixels, PIXEL_BYTES) && fsync(fd) >= 0;
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

void crazypod_wallpaper_store_remove(
    enum crazypod_wallpaper_target target)
{
    remove(published_path(target));
}

#endif
