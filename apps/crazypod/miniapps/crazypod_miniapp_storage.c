#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "dir.h"
#include "errno.h"
#include "file.h"

#include "../crazypod_miniapps.h"
#include "crazypod_miniapp_storage.h"

#define MINIAPP_DATA_ROOT "/.crazypod/miniapp-data"
#define MINIAPP_STATE_NAME "state.bin"
#define MINIAPP_STATE_TEMP_NAME "state.tmp"
#define MINIAPP_STATE_MAX (16u * 1024u)
#define MINIAPP_STATE_MAGIC 0x43505354u
#define MINIAPP_DISK_VERSION 1u

struct state_header {
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    uint32_t data_size;
    uint32_t data_crc32;
    uint32_t checksum;
};

static uint32_t crc_buffer(const void *buffer, size_t size)
{
    return ~crc_32r(buffer, (uint32_t)size, 0xffffffffu);
}

static bool valid_id(const char *id)
{
    size_t index;
    size_t length;

    if(id == NULL)
        return false;
    length = strlen(id);
    if(length == 0 || length >= CRAZYPOD_MINIAPP_ID_SIZE ||
       id[0] < 'a' || id[0] > 'z')
        return false;
    for(index = 1; index < length; ++index) {
        char value = id[index];
        if(!((value >= 'a' && value <= 'z') ||
             (value >= '0' && value <= '9') ||
             value == '_' || value == '-'))
            return false;
    }
    return true;
}

static bool ensure_directory(const char *path)
{
    if(mkdir(path) == 0)
        return true;
    return errno == EEXIST && dir_exists(path);
}

static bool data_path(
    char *path, size_t capacity, const char *id, const char *file)
{
    int length;

    if(!valid_id(id))
        return false;
    length = snprintf(
        path, capacity, "%s/%s/%s", MINIAPP_DATA_ROOT, id, file);
    return length >= 0 && (size_t)length < capacity;
}

static bool ensure_data_directory(const char *id)
{
    char directory[MAX_PATH];
    int length;

    if(!valid_id(id) ||
       !ensure_directory("/.crazypod") ||
       !ensure_directory(MINIAPP_DATA_ROOT))
        return false;
    length = snprintf(
        directory, sizeof(directory), "%s/%s", MINIAPP_DATA_ROOT, id);
    return length >= 0 && (size_t)length < sizeof(directory) &&
           ensure_directory(directory);
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

static uint32_t header_checksum(const struct state_header *header)
{
    return crc_buffer(header, offsetof(struct state_header, checksum));
}

int crazypod_miniapp_storage_read(
    const char *id, void *buffer, size_t capacity)
{
    struct state_header header;
    char path[MAX_PATH];
    int fd;

    if(buffer == NULL ||
       !data_path(path, sizeof(path), id, MINIAPP_STATE_NAME))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return 0;
    if(filesize(fd) < (off_t)sizeof(header) ||
       !read_exact(fd, &header, sizeof(header)) ||
       header.magic != MINIAPP_STATE_MAGIC ||
       header.version != MINIAPP_DISK_VERSION ||
       header.struct_size != sizeof(header) ||
       header.data_size > MINIAPP_STATE_MAX ||
       header.data_size > capacity ||
       filesize(fd) != (off_t)(sizeof(header) + header.data_size) ||
       header.checksum != header_checksum(&header) ||
       !read_exact(fd, buffer, header.data_size) ||
       crc_buffer(buffer, header.data_size) != header.data_crc32) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    }
    close(fd);
    return (int)header.data_size;
}

int crazypod_miniapp_storage_write(
    const char *id, const void *buffer, size_t size)
{
    struct state_header header;
    char path[MAX_PATH];
    char temporary[MAX_PATH];
    int fd;
    bool success;

    if((size > 0 && buffer == NULL) ||
       size > MINIAPP_STATE_MAX ||
       !ensure_data_directory(id) ||
       !data_path(path, sizeof(path), id, MINIAPP_STATE_NAME) ||
       !data_path(
           temporary, sizeof(temporary), id, MINIAPP_STATE_TEMP_NAME))
        return CRAZYPOD_MINIAPP_ERROR_STATE;

    memset(&header, 0, sizeof(header));
    header.magic = MINIAPP_STATE_MAGIC;
    header.version = MINIAPP_DISK_VERSION;
    header.struct_size = sizeof(header);
    header.data_size = (uint32_t)size;
    header.data_crc32 = crc_buffer(buffer, size);
    header.checksum = header_checksum(&header);
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    success = write_exact(fd, &header, sizeof(header)) &&
              write_exact(fd, buffer, size) &&
              fsync(fd) == 0;
    if(close(fd) < 0)
        success = false;
    if(!success || rename(temporary, path) < 0) {
        remove(temporary);
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    }
    return CRAZYPOD_MINIAPP_OK;
}

#endif
