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

#ifndef MINIAPP_SYSTEM_ROOT
#define MINIAPP_SYSTEM_ROOT "/.crazypod"
#endif
#ifndef MINIAPP_DATA_ROOT
#define MINIAPP_DATA_ROOT MINIAPP_SYSTEM_ROOT "/miniapp-data"
#endif
#ifndef MINIAPP_USER_ROOT
#define MINIAPP_USER_ROOT "/"
#endif
#ifndef MINIAPP_EXPORT_PARENT
#define MINIAPP_EXPORT_PARENT "/MiniApps"
#endif
#ifndef MINIAPP_EXPORT_ROOT
#define MINIAPP_EXPORT_ROOT MINIAPP_EXPORT_PARENT "/Export"
#endif
#define MINIAPP_STATE_NAME "state.bin"
#define MINIAPP_STATE_TEMP_NAME "state.tmp"
#define MINIAPP_STATE_MAX CRAZYPOD_MINIAPP_STORAGE_MAX
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
       !ensure_directory(MINIAPP_SYSTEM_ROOT) ||
       !ensure_directory(MINIAPP_DATA_ROOT))
        return false;
    length = snprintf(
        directory, sizeof(directory), "%s/%s", MINIAPP_DATA_ROOT, id);
    return length >= 0 && (size_t)length < sizeof(directory) &&
           ensure_directory(directory);
}

static bool valid_relative_path(const char *path)
{
    const char *segment = path;
    size_t length;

    if(path == NULL || path[0] == '\0' || path[0] == '/' ||
       path[0] == '\\')
        return false;
    length = strlen(path);
    if(length > 160 || path[length - 1] == '/' ||
       path[length - 1] == '\\')
        return false;
    while(*segment != '\0') {
        const char *end = segment;
        size_t segment_length;

        while(*end != '\0' && *end != '/' && *end != '\\') {
            unsigned char value = (unsigned char)*end;

            if(value < 0x20 || value == ':' || value == '*'
               || value == '?' || value == '"' || value == '<'
               || value == '>' || value == '|')
                return false;
            ++end;
        }
        segment_length = (size_t)(end - segment);
        if(segment_length == 0 ||
           (segment_length == 1 && segment[0] == '.') ||
           (segment_length == 2 && segment[0] == '.' &&
            segment[1] == '.'))
            return false;
        segment = *end == '\0' ? end : end + 1;
    }
    return true;
}

static bool path_has_prefix(
    const char *path, const char *prefix)
{
    size_t length;

    if(path == NULL || prefix == NULL)
        return false;
    length = strlen(prefix);
    if(strncmp(path, prefix, length) != 0)
        return false;
    return path[length] == '\0' || path[length] == '/' ||
           (length == 1 && prefix[0] == '/');
}

static bool user_path_readable(const char *path)
{
    size_t length;

    if(path == NULL || path[0] != '/')
        return false;
    length = strlen(path);
    if(length < 2 || length >= MAX_PATH ||
       path[length - 1] == '/' ||
       !path_has_prefix(path, MINIAPP_USER_ROOT))
        return false;
    if(path_has_prefix(path, MINIAPP_SYSTEM_ROOT) ||
       path_has_prefix(path, "/.rockbox") ||
       path_has_prefix(path, "/.crazypod") ||
       path_has_prefix(path, "/MiniApps/Install"))
        return false;
    return true;
}

static bool valid_export_filename(const char *filename)
{
    size_t index;
    size_t length;

    if(filename == NULL)
        return false;
    length = strlen(filename);
    if(length == 0 || length > 96 ||
       filename[0] == '.' ||
       strcmp(filename, ".") == 0 ||
       strcmp(filename, "..") == 0)
        return false;
    for(index = 0; index < length; ++index) {
        unsigned char value = (unsigned char)filename[index];

        if(value < 0x20 || value == '/' || value == '\\' ||
           value == ':' || value == '*' || value == '?' ||
           value == '"' || value == '<' || value == '>' ||
           value == '|')
            return false;
    }
    return true;
}

static bool file_path(
    char *path, size_t capacity,
    const char *id, const char *relative_path)
{
    int length;

    if(!valid_id(id) || !valid_relative_path(relative_path))
        return false;
    length = snprintf(
        path, capacity, "%s/%s/%s",
        MINIAPP_DATA_ROOT, id, relative_path);
    return length >= 0 && (size_t)length < capacity;
}

static bool ensure_file_parent(
    const char *id, const char *relative_path)
{
    char path[MAX_PATH];
    char *cursor;
    size_t prefix;

    if(!ensure_data_directory(id) ||
       !file_path(path, sizeof(path), id, relative_path))
        return false;
    prefix = strlen(MINIAPP_DATA_ROOT) + strlen(id) + 2u;
    for(cursor = path + prefix; *cursor != '\0'; ++cursor) {
        if(*cursor != '/' && *cursor != '\\')
            continue;
        *cursor = '\0';
        if(!ensure_directory(path))
            return false;
        *cursor = '/';
    }
    return true;
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

int crazypod_miniapp_file_size(
    const char *id, const char *relative_path)
{
    char path[MAX_PATH];
    off_t size;
    int fd;

    if(!file_path(path, sizeof(path), id, relative_path))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    close(fd);
    if(size < 0 || size > (off_t)CRAZYPOD_MINIAPP_FILE_MAX)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    return (int)size;
}

int crazypod_miniapp_file_read(
    const char *id, const char *relative_path,
    void *buffer, size_t capacity)
{
    char path[MAX_PATH];
    off_t size;
    int fd;
    bool success;

    if(buffer == NULL ||
       !file_path(path, sizeof(path), id, relative_path))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    success =
        size >= 0 &&
        size <= (off_t)CRAZYPOD_MINIAPP_FILE_MAX &&
        (uint64_t)size <= capacity &&
        read_exact(fd, buffer, (size_t)size);
    close(fd);
    if(!success)
        return size > (off_t)CRAZYPOD_MINIAPP_FILE_MAX
            ? CRAZYPOD_MINIAPP_ERROR_LIMIT
            : CRAZYPOD_MINIAPP_ERROR_IO;
    return (int)size;
}

int crazypod_miniapp_file_write(
    const char *id, const char *relative_path,
    const void *buffer, size_t size)
{
    char path[MAX_PATH];
    char temporary[MAX_PATH];
    int fd;
    int length;
    bool success;

    if(size > CRAZYPOD_MINIAPP_FILE_MAX)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if((buffer == NULL && size > 0) ||
       !ensure_file_parent(id, relative_path) ||
       !file_path(path, sizeof(path), id, relative_path))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    length = snprintf(
        temporary, sizeof(temporary), "%s.cptmp", path);
    if(length < 0 || (size_t)length >= sizeof(temporary))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    success =
        write_exact(fd, buffer, size) && fsync(fd) == 0;
    if(close(fd) < 0)
        success = false;
    if(!success || rename(temporary, path) < 0) {
        remove(temporary);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_file_remove(
    const char *id, const char *relative_path)
{
    char path[MAX_PATH];

    if(!file_path(path, sizeof(path), id, relative_path))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    if(remove(path) < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_user_file_size(const char *path)
{
    off_t size;
    int fd;

    if(!user_path_readable(path))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    close(fd);
    if(size < 0 || size > (off_t)INT32_MAX)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    return (int)size;
}

int crazypod_miniapp_user_file_read(
    const char *path, uint32_t offset,
    void *buffer, size_t capacity)
{
    off_t size;
    size_t amount;
    int fd;
    bool success;

    if(buffer == NULL || capacity == 0 ||
       capacity > CRAZYPOD_MINIAPP_USER_CHUNK_MAX ||
       !user_path_readable(path))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    if(size < 0 || (uint64_t)offset > (uint64_t)size) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    amount = (size_t)(
        (uint64_t)size - offset < capacity
            ? (uint64_t)size - offset : capacity);
    success = lseek(fd, (off_t)offset, SEEK_SET) >= 0 &&
              read_exact(fd, buffer, amount);
    close(fd);
    return success ? (int)amount : CRAZYPOD_MINIAPP_ERROR_IO;
}

int crazypod_miniapp_user_file_export(
    const char *id, const char *filename,
    const void *buffer, size_t size,
    char *output_path, size_t output_capacity)
{
    char directory[MAX_PATH];
    char temporary[MAX_PATH];
    int length;
    int fd;
    bool success;

    if(!valid_id(id) || !valid_export_filename(filename) ||
       (buffer == NULL && size > 0) ||
       size > CRAZYPOD_MINIAPP_EXPORT_MAX ||
       output_path == NULL || output_capacity == 0 ||
       !ensure_directory(MINIAPP_EXPORT_PARENT) ||
       !ensure_directory(MINIAPP_EXPORT_ROOT))
        return size > CRAZYPOD_MINIAPP_EXPORT_MAX
            ? CRAZYPOD_MINIAPP_ERROR_LIMIT
            : CRAZYPOD_MINIAPP_ERROR_STATE;
    length = snprintf(
        directory, sizeof(directory), "%s/%s",
        MINIAPP_EXPORT_ROOT, id);
    if(length < 0 || (size_t)length >= sizeof(directory) ||
       !ensure_directory(directory))
        return CRAZYPOD_MINIAPP_ERROR_IO;
    length = snprintf(
        output_path, output_capacity, "%s/%s",
        directory, filename);
    if(length < 0 || (size_t)length >= output_capacity)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    length = snprintf(
        temporary, sizeof(temporary), "%s.cptmp", output_path);
    if(length < 0 || (size_t)length >= sizeof(temporary))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    success = write_exact(fd, buffer, size) && fsync(fd) == 0;
    if(close(fd) < 0)
        success = false;
    if(!success || rename(temporary, output_path) < 0) {
        remove(temporary);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    return CRAZYPOD_MINIAPP_OK;
}

#endif
