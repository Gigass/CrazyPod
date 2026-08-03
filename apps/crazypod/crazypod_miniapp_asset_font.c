#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "file.h"

#include "crazypod_miniapp_asset_font.h"
#include "crazypod_miniapps.h"
#include "crazypod_runtime_font.h"

#define ASSET_FONT_MAX (4u * 1024u * 1024u)
#define ASSET_FONT_IO_BUFFER_SIZE 1024u
#define ASSET_FONT_PATH_SIZE (CRAZYPOD_MINIAPP_PATH_SIZE + 48u)

static bool valid_id(const char *id)
{
    size_t index;
    size_t length;

    if(id == NULL)
        return false;
    length = strlen(id);
    if(length == 0 || length >= CP_NATIVE_RESOURCE_ID_SIZE ||
       id[0] < 'a' || id[0] > 'z')
        return false;
    for(index = 1; index < length; ++index) {
        char value = id[index];

        if(!((value >= 'a' && value <= 'z') ||
             (value >= '0' && value <= '9') ||
             value == '_' || value == '-' || value == '.'))
            return false;
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

static bool materialize(
    const char *id, uint32_t size,
    const char *path, const char *temporary)
{
    uint8_t buffer[ASSET_FONT_IO_BUFFER_SIZE];
    uint32_t offset = 0;
    int fd;
    bool success = true;

    (void)remove(temporary);
    fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if(fd < 0)
        return false;
    while(offset < size) {
        uint32_t amount = size - offset;
        int count;

        if(amount > sizeof(buffer))
            amount = sizeof(buffer);
        count = crazypod_miniapps_resource_read(
            id, offset, buffer, amount);
        if(count != (int)amount || !write_exact(fd, buffer, amount)) {
            success = false;
            break;
        }
        offset += amount;
    }
    if(fsync(fd) < 0)
        success = false;
    if(close(fd) < 0)
        success = false;
    if(!success || rename(temporary, path) < 0) {
        (void)remove(temporary);
        return false;
    }
    return true;
}

const lv_font_t *crazypod_miniapp_asset_font(const char *id)
{
    const struct crazypod_miniapp_metadata *metadata;
    struct cp_resource_info info = {
        .struct_size = sizeof(struct cp_resource_info),
    };
    char path[ASSET_FONT_PATH_SIZE];
    char temporary[ASSET_FONT_PATH_SIZE];
    const lv_font_t *font;
    int length;
    int fd;

    if(!valid_id(id) ||
       crazypod_miniapps_resource_stat(id, &info) != CRAZYPOD_MINIAPP_OK ||
       info.type != CP_RESOURCE_FONT ||
       info.size < 36u || info.size > ASSET_FONT_MAX)
        return NULL;
    metadata = crazypod_miniapps_current_metadata();
    if(metadata == NULL)
        return NULL;
    length = snprintf(
        path, sizeof(path), "%s/.font-%s-%08lx.fnt",
        metadata->install_path, id, (unsigned long)info.crc32);
    if(length < 0 || (size_t)length >= sizeof(path))
        return NULL;
    length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if(length < 0 || (size_t)length >= sizeof(temporary))
        return NULL;
    fd = open(path, O_RDONLY);
    if(fd >= 0) {
        bool expected_size = filesize(fd) == (int)info.size;

        close(fd);
        if(expected_size) {
            font = crazypod_runtime_asset_font(id, path);
            if(font != NULL)
                return font;
        }
        (void)remove(path);
    }
    if(!materialize(id, info.size, path, temporary))
        return NULL;
    font = crazypod_runtime_asset_font(id, path);
    if(font == NULL)
        (void)remove(path);
    return font;
}

void crazypod_miniapp_asset_fonts_reset(void)
{
    crazypod_runtime_asset_fonts_reset();
}

#endif
