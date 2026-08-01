#include "config.h"

#ifdef IPOD_6G

#include <limits.h>
#include <string.h>

#include "file.h"

#include "crazypod_miniapp_resource_host.h"

#define RESOURCE_HEADER_SIZE 16u
#define RESOURCE_ENTRY_SIZE 52u

static uint16_t read_le16(const uint8_t *value)
{
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8);
}

static uint32_t read_le32(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static bool read_at_exact(
    int fd, uint32_t offset, void *buffer, size_t size)
{
    uint8_t *cursor = buffer;

    if(lseek(fd, (off_t)offset, SEEK_SET) < 0)
        return false;
    while(size > 0) {
        ssize_t count = read(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static int find_resource(
    const struct crazypod_miniapp_metadata *metadata,
    const char *id, struct cp_resource_info *info,
    uint32_t *data_offset, int *fd_out)
{
    uint8_t header[RESOURCE_HEADER_SIZE];
    uint8_t entry[RESOURCE_ENTRY_SIZE];
    size_t id_length;
    uint16_t count;
    uint16_t index;
    int fd;

    if(metadata == NULL ||
       metadata->package_format != CP_NATIVE_PACKAGE_FORMAT ||
       id == NULL || info == NULL ||
       data_offset == NULL || fd_out == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    id_length = strlen(id);
    if(id_length == 0 || id_length >= 32u)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    fd = open(metadata->assets_path, O_RDONLY);
    if(fd < 0 || !read_at_exact(fd, 0, header, sizeof(header))) {
        if(fd >= 0)
            close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    count = read_le16(header + 6);
    for(index = 0; index < count; ++index) {
        int comparison;
        if(!read_at_exact(
               fd, RESOURCE_HEADER_SIZE +
                       (uint32_t)index * RESOURCE_ENTRY_SIZE,
               entry, sizeof(entry)) ||
           memchr(entry, '\0', 32) == NULL) {
            close(fd);
            return CRAZYPOD_MINIAPP_ERROR_IO;
        }
        comparison = strcmp(id, (const char *)entry);
        if(comparison > 0)
            continue;
        if(comparison < 0) {
            close(fd);
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        }
        memset(info, 0, sizeof(*info));
        info->struct_size = sizeof(*info);
        info->type = entry[32];
        info->frame_count = entry[33];
        info->width = read_le16(entry + 34);
        info->height = read_le16(entry + 36);
        info->frame_duration_ms = read_le16(entry + 38);
        info->size = read_le32(entry + 44);
        info->crc32 = read_le32(entry + 48);
        *data_offset = read_le32(entry + 40);
        *fd_out = fd;
        return CRAZYPOD_MINIAPP_OK;
    }
    close(fd);
    return CRAZYPOD_MINIAPP_ERROR_FORMAT;
}

int crazypod_miniapp_resource_stat(
    const struct crazypod_miniapp_metadata *metadata,
    const char *id, struct cp_resource_info *info)
{
    struct cp_resource_info found;
    uint32_t offset;
    int fd;
    int result;

    if(info == NULL || info->struct_size < sizeof(*info))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    result = find_resource(metadata, id, &found, &offset, &fd);
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    close(fd);
    *info = found;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_resource_read(
    const struct crazypod_miniapp_metadata *metadata,
    const char *id, uint32_t offset,
    void *buffer, size_t capacity)
{
    struct cp_resource_info info;
    uint32_t data_offset;
    uint32_t amount;
    int fd;
    int result;

    if(buffer == NULL && capacity > 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    result = find_resource(
        metadata, id, &info, &data_offset, &fd);
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    if(offset > info.size) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    }
    amount = info.size - offset;
    if(amount > capacity)
        amount = (uint32_t)capacity;
    if(amount > INT32_MAX ||
       (amount > 0 &&
        !read_at_exact(fd, data_offset + offset, buffer, amount))) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    close(fd);
    return (int)amount;
}

#endif
