#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdio.h>

#include "crc32.h"
#include "file.h"

#include "../installer/crazypod_cpk_reader.h"
#include "../installer/crazypod_miniapp_install_record.h"
#include "../installer/crazypod_miniapp_resource_validator.h"
#include "crazypod_miniapp_installed_verifier.h"

#define IO_BUFFER_SIZE 1024u

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

static bool make_path(
    char *path, size_t capacity,
    const char *directory, const char *name)
{
    int length = snprintf(path, capacity, "%s/%s", directory, name);
    return length >= 0 && (size_t)length < capacity;
}

static int verify_file(
    const char *path, const struct install_file_record *record)
{
    uint8_t buffer[IO_BUFFER_SIZE];
    uint32_t remaining = record->size;
    uint32_t crc = 0xffffffffu;
    int fd = open(path, O_RDONLY);

    if(fd < 0 || filesize(fd) != (off_t)record->size) {
        if(fd >= 0)
            close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    while(remaining > 0) {
        uint32_t amount = remaining > sizeof(buffer)
            ? (uint32_t)sizeof(buffer) : remaining;
        if(!read_exact(fd, buffer, amount)) {
            close(fd);
            return CRAZYPOD_MINIAPP_ERROR_IO;
        }
        crc = crc_32r(buffer, amount, crc);
        remaining -= amount;
    }
    close(fd);
    return ~crc == record->crc32
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_CRC;
}

int crazypod_miniapp_installed_verify(
    const struct crazypod_miniapp_metadata *metadata)
{
    struct install_record record;
    char path[MAX_PATH];
    int index;

    if(metadata == NULL ||
       !crazypod_miniapp_install_record_read(
           metadata->install_path, &record) ||
       record.version_code != metadata->version_code ||
       record.files[CPK_APP].size != metadata->binary_size ||
       record.files[CPK_PROFILE].size != metadata->profile_size ||
       record.files[CPK_ASSETS].size != metadata->assets_size ||
       record.files[CPK_ICON].size != metadata->icon_size)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    for(index = 0; index < MINIAPP_CPK_MAX_ENTRIES; ++index) {
        const char *name =
            index == CPK_MANIFEST ? "manifest.json" :
            index == CPK_APP ? metadata->entry :
            index == CPK_PROFILE ? "profile.bin" :
            index == CPK_ASSETS ? "assets.bin" : "icon.bin";
        int result;

        if(!make_path(
               path, sizeof(path),
               metadata->install_path, name))
            return CRAZYPOD_MINIAPP_ERROR_LIMIT;
        result = verify_file(path, &record.files[index]);
        if(result != CRAZYPOD_MINIAPP_OK)
            return result;
    }
    {
        int fd = open(metadata->assets_path, O_RDONLY);
        bool valid;

        if(fd < 0)
            return CRAZYPOD_MINIAPP_ERROR_IO;
        valid = crazypod_miniapp_resource_container_valid(
            fd, 0, metadata->assets_size);
        close(fd);
        if(!valid)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    }
    return CRAZYPOD_MINIAPP_OK;
}

#endif
