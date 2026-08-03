#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "file.h"

#include "crazypod_miniapp_install_record.h"
#include "crazypod_miniapp_manifest.h"
#include "crazypod_miniapp_resource_validator.h"

#define INSTALL_RECORD_NAME ".install.bin"
#define MANIFEST_NAME "manifest.json"
#define ASSETS_NAME "assets.bin"
#define ICON_NAME "icon.bin"
#define INSTALL_MAGIC 0x4350494eu
#define DISK_VERSION 3u
#define ICON_BYTES 51216u
#define RESOURCE_HEADER_SIZE 16u
#define ICON_MAGIC 0x35495043u
#define PROFILE_MAGIC 0x35415043u

static uint32_t checksum(const struct install_record *record)
{
    return ~crc_32r(
        record, (uint32_t)offsetof(struct install_record, checksum),
        0xffffffffu);
}

static bool make_path(
    char *path, size_t capacity,
    const char *directory, const char *name)
{
    int length = snprintf(path, capacity, "%s/%s", directory, name);
    return length >= 0 && (size_t)length < capacity;
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

bool crazypod_miniapp_install_record_write(
    const char *directory, const struct cpk_reader *reader,
    uint32_t version_code)
{
    struct install_record record = { 0 };
    char path[MAX_PATH];
    int fd;
    int index;
    bool success;

    if(!make_path(path, sizeof(path), directory, INSTALL_RECORD_NAME))
        return false;
    record.magic = INSTALL_MAGIC;
    record.version = DISK_VERSION;
    record.struct_size = sizeof(record);
    record.version_code = version_code;
    for(index = 0; index < MINIAPP_CPK_ENTRIES; ++index) {
        record.files[index].size = reader->entries[index].size;
        record.files[index].crc32 = reader->entries[index].crc32;
    }
    record.checksum = checksum(&record);
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, &record, sizeof(record)) && fsync(fd) == 0;
    if(close(fd) < 0)
        success = false;
    if(!success)
        remove(path);
    return success;
}

bool crazypod_miniapp_install_record_read(
    const char *directory, struct install_record *record)
{
    char path[MAX_PATH];
    int fd;
    bool success;

    if(!make_path(path, sizeof(path), directory, INSTALL_RECORD_NAME))
        return false;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return false;
    success = filesize(fd) == (off_t)sizeof(*record) &&
              read_exact(fd, record, sizeof(*record));
    close(fd);
    return success && record->magic == INSTALL_MAGIC &&
           record->version == DISK_VERSION &&
           record->struct_size == sizeof(*record) &&
           record->checksum == checksum(record);
}

static bool file_has_size(const char *path, uint32_t size)
{
    int fd = open(path, O_RDONLY);
    bool matches;
    if(fd < 0)
        return false;
    matches = filesize(fd) == (off_t)size;
    close(fd);
    return matches;
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

static bool icon_header_valid(
    const char *path, uint32_t expected_size)
{
    uint8_t header[16];
    int fd = open(path, O_RDONLY);
    bool valid;

    if(fd < 0)
        return false;
    valid = filesize(fd) == (off_t)expected_size &&
        read_exact(fd, header, sizeof(header)) &&
        read_le32(header) == ICON_MAGIC;
    if(valid) {
        valid = expected_size == ICON_BYTES &&
            read_le16(header + 4) == 1u &&
            read_le16(header + 6) == 160u &&
            read_le16(header + 8) == 160u &&
            read_le16(header + 10) == 1u &&
            read_le32(header + 12) == 160u * 160u * 2u;
    }
    close(fd);
    return valid;
}

static bool profile_header_valid(
    const char *path, uint32_t expected_size)
{
    uint8_t header[16];
    int fd = open(path, O_RDONLY);
    bool valid;

    if(fd < 0)
        return false;
    valid = expected_size == sizeof(header) &&
        filesize(fd) == (off_t)expected_size &&
        read_exact(fd, header, sizeof(header)) &&
        read_le32(header) == PROFILE_MAGIC &&
        read_le16(header + 4) == 1u &&
        read_le16(header + 6) == sizeof(header) &&
        read_le16(header + 8) == CP_NATIVE_ABI_MAJOR &&
        read_le16(header + 10) <= CP_NATIVE_ABI_MINOR &&
        read_le16(header + 10) != CP_NATIVE_ABI_REJECTED_MINOR &&
        read_le16(header + 12) == CP_NATIVE_REACT_PROFILE &&
        read_le16(header + 14) == 0;
    close(fd);
    return valid;
}

static int load_manifest(
    const char *directory,
    struct crazypod_miniapp_metadata *metadata,
    uint32_t *size_out, uint32_t *crc_out)
{
    char path[MAX_PATH];
    char buffer[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    off_t size;
    int fd;
    int result;

    if(!make_path(path, sizeof(path), directory, MANIFEST_NAME))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    if(size <= 0 || size > (off_t)CRAZYPOD_MINIAPP_MANIFEST_MAX ||
       !read_exact(fd, buffer, (size_t)size)) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
    }
    close(fd);
    result = crazypod_miniapp_manifest_parse(
        buffer, (size_t)size, metadata);
    if(result == CRAZYPOD_MINIAPP_OK) {
        *size_out = (uint32_t)size;
        *crc_out = ~crc_32r(
            buffer, (uint32_t)size, 0xffffffffu);
    }
    return result;
}

bool crazypod_miniapp_install_directory_validate(
    const char *directory, const char *expected_id,
    struct crazypod_miniapp_metadata *metadata,
    struct install_record *record_out)
{
    struct install_record record;
    uint32_t manifest_size;
    uint32_t manifest_crc;
    char path[MAX_PATH];

    if(load_manifest(directory, metadata,
                     &manifest_size, &manifest_crc) !=
           CRAZYPOD_MINIAPP_OK ||
       strcmp(metadata->id, expected_id) != 0 ||
       !crazypod_miniapp_install_record_read(directory, &record) ||
       record.version_code != metadata->version_code ||
       record.files[CPK_MANIFEST].size != manifest_size ||
       record.files[CPK_MANIFEST].crc32 != manifest_crc)
        return false;
    if(!make_path(
           path, sizeof(path), directory,
           metadata->entry) ||
       !file_has_size(path, record.files[CPK_APP].size) ||
       !make_path(path, sizeof(path), directory, "profile.bin") ||
       !profile_header_valid(
           path, record.files[CPK_PROFILE].size) ||
       !make_path(path, sizeof(path), directory, ICON_NAME) ||
       !icon_header_valid(
           path, record.files[CPK_ICON].size))
        return false;
    if(record.files[CPK_APP].size == 0 ||
       record.files[CPK_APP].size >
           8u * 1024u * 1024u ||
       record.files[CPK_PROFILE].size != 16u ||
       record.files[CPK_ASSETS].size < RESOURCE_HEADER_SIZE ||
       record.files[CPK_ASSETS].size > CP_NATIVE_ASSET_MAX ||
       record.files[CPK_ICON].size != ICON_BYTES)
        return false;
    {
        int fd;
        off_t size;
        if(!make_path(path, sizeof(path), directory, ASSETS_NAME))
            return false;
        fd = open(path, O_RDONLY);
        if(fd < 0)
            return false;
        size = filesize(fd);
        if(size < (off_t)RESOURCE_HEADER_SIZE ||
           size > (off_t)CP_NATIVE_ASSET_MAX ||
           !crazypod_miniapp_resource_container_valid(
               fd, 0, (uint32_t)size)) {
            close(fd);
            return false;
        }
        close(fd);
    }
    metadata->binary_size = record.files[CPK_APP].size;
    metadata->profile_size = record.files[CPK_PROFILE].size;
    metadata->assets_size = record.files[CPK_ASSETS].size;
    metadata->icon_size = record.files[CPK_ICON].size;
    if(record_out != NULL)
        *record_out = record;
    return true;
}

#endif
