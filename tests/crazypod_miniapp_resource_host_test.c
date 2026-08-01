#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "crazypod_miniapp_resource_host.h"
#include "crazypod_miniapp_resource_validator.h"
#include "crc32.h"

static void write_le16(unsigned char *value, unsigned number)
{
    value[0] = (unsigned char)number;
    value[1] = (unsigned char)(number >> 8);
}

static void write_le32(unsigned char *value, unsigned long number)
{
    value[0] = (unsigned char)number;
    value[1] = (unsigned char)(number >> 8);
    value[2] = (unsigned char)(number >> 16);
    value[3] = (unsigned char)(number >> 24);
}

int main(void)
{
    unsigned char container[72] = { 0 };
    struct crazypod_miniapp_metadata metadata = { 0 };
    struct cp_resource_info info = { .struct_size = sizeof(info) };
    char path[] = "/tmp/crazypod-resource-XXXXXX";
    unsigned char payload[4] = { 1, 2, 3, 4 };
    unsigned char result[4] = { 0 };
    int fd = mkstemp(path);

    assert(fd >= 0);
    write_le32(container, 0x53525043);
    write_le16(container + 4, 1);
    write_le16(container + 6, 1);
    write_le32(container + 8, sizeof(container));
    strcpy((char *)container + 16, "icon");
    container[48] = CP_RESOURCE_BITMAP_RGB565;
    write_le16(container + 50, 2);
    write_le16(container + 52, 1);
    write_le32(container + 56, 68);
    write_le32(container + 60, 4);
    write_le32(container + 64,
               ~crc_32r(payload, sizeof(payload), 0xffffffffu));
    memcpy(container + 68, payload, sizeof(payload));
    write_le32(container + 12,
               ~crc_32r(container + 16, 52, 0xffffffffu));
    assert(write(fd, container, sizeof(container)) ==
           (ssize_t)sizeof(container));
    close(fd);

    metadata.package_format = CP_NATIVE_PACKAGE_FORMAT;
    strcpy(metadata.assets_path, path);
    fd = open(path, O_RDONLY);
    assert(fd >= 0);
    assert(crazypod_miniapp_resource_container_valid(
        fd, 0, sizeof(container)));
    close(fd);
    assert(crazypod_miniapp_resource_stat(
        &metadata, "icon", &info) == CRAZYPOD_MINIAPP_OK);
    assert(info.width == 2 && info.height == 1 && info.size == 4);
    assert(info.frame_count == 0 && info.frame_duration_ms == 0);
    assert(crazypod_miniapp_resource_read(
        &metadata, "icon", 0, result, sizeof(result)) == 4);
    assert(memcmp(result, payload, sizeof(result)) == 0);
    assert(crazypod_miniapp_resource_read(
        &metadata, "icon", 5, result, sizeof(result)) ==
        CRAZYPOD_MINIAPP_ERROR_LIMIT);

    container[48] = CP_RESOURCE_SPRITE_SHEET;
    container[49] = 1;
    write_le16(container + 54, 75);
    write_le32(container + 12,
               ~crc_32r(container + 16, 52, 0xffffffffu));
    fd = open(path, O_WRONLY | O_TRUNC);
    assert(fd >= 0);
    assert(write(fd, container, sizeof(container)) ==
           (ssize_t)sizeof(container));
    close(fd);
    fd = open(path, O_RDONLY);
    assert(fd >= 0);
    assert(crazypod_miniapp_resource_container_valid(
        fd, 0, sizeof(container)));
    close(fd);
    memset(&info, 0, sizeof(info));
    info.struct_size = sizeof(info);
    assert(crazypod_miniapp_resource_stat(
        &metadata, "icon", &info) == CRAZYPOD_MINIAPP_OK);
    assert(info.frame_count == 1 && info.frame_duration_ms == 75);

    container[71] ^= 1;
    fd = open(path, O_WRONLY | O_TRUNC);
    assert(fd >= 0);
    assert(write(fd, container, sizeof(container)) ==
           (ssize_t)sizeof(container));
    close(fd);
    fd = open(path, O_RDONLY);
    assert(fd >= 0);
    assert(!crazypod_miniapp_resource_container_valid(
        fd, 0, sizeof(container)));
    close(fd);
    unlink(path);
    return 0;
}
