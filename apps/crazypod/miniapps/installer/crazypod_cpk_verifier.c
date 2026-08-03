#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "file.h"

#include "../../crazypod_miniapps.h"
#include "crazypod_cpk_verifier.h"
#include "crazypod_miniapp_resource_validator.h"

#define IO_BUFFER_SIZE 1024u
#define ICON_BYTES 51216u
#define ICON_MAGIC 0x35495043u
#define PROFILE_MAGIC 0x35415043u

typedef bool (*sink_fn)(void *context, const void *buffer, size_t size);

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

static int stream_entry(
    const struct cpk_reader *reader, int entry_index,
    sink_fn sink, void *context)
{
    const struct cpk_entry *entry = &reader->entries[entry_index];
    uint8_t buffer[IO_BUFFER_SIZE];
    uint32_t remaining = entry->size;
    uint32_t offset = entry->data_offset;
    uint32_t crc = 0xffffffffu;

    while(remaining > 0) {
        uint32_t amount = remaining > sizeof(buffer)
            ? (uint32_t)sizeof(buffer) : remaining;
        if(!read_at_exact(reader->fd, offset, buffer, amount))
            return CRAZYPOD_MINIAPP_ERROR_IO;
        crc = crc_32r(buffer, amount, crc);
        if(sink != NULL && !sink(context, buffer, amount))
            return CRAZYPOD_MINIAPP_ERROR_IO;
        offset += amount;
        remaining -= amount;
    }
    return ~crc == entry->crc32
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_CRC;
}

bool crazypod_cpk_assets_valid(const struct cpk_reader *reader)
{
    const struct cpk_entry *entry;
    if(reader->entry_count != MINIAPP_CPK_ENTRIES)
        return false;
    entry = &reader->entries[CPK_ASSETS];
    return crazypod_miniapp_resource_container_valid(
        reader->fd, entry->data_offset, entry->size);
}

bool crazypod_cpk_profile_valid(const struct cpk_reader *reader)
{
    const struct cpk_entry *entry = &reader->entries[CPK_PROFILE];
    uint8_t header[16];

    return strcmp(entry->name, "profile.bin") == 0 &&
           entry->size == sizeof(header) &&
           read_at_exact(
               reader->fd, entry->data_offset,
               header, sizeof(header)) &&
           read_le32(header) == PROFILE_MAGIC &&
           read_le16(header + 4) == 1u &&
           read_le16(header + 6) == sizeof(header) &&
           read_le16(header + 8) == CP_NATIVE_ABI_MAJOR &&
           read_le16(header + 10) <= CP_NATIVE_ABI_MINOR &&
           read_le16(header + 10) != CP_NATIVE_ABI_REJECTED_MINOR &&
           read_le16(header + 12) == CP_NATIVE_REACT_PROFILE &&
           read_le16(header + 14) == 0;
}

int crazypod_cpk_verify_crc(
    const struct cpk_reader *reader, int entry)
{
    if(reader == NULL || entry < 0 ||
       entry >= reader->entry_count)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    return stream_entry(reader, entry, NULL, NULL);
}

int crazypod_cpk_read_entry(
    const struct cpk_reader *reader, int entry_index,
    void *buffer, size_t capacity)
{
    const struct cpk_entry *entry = &reader->entries[entry_index];
    uint32_t crc;

    if(entry->size > capacity)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(!read_at_exact(
           reader->fd, entry->data_offset, buffer, entry->size))
        return CRAZYPOD_MINIAPP_ERROR_IO;
    crc = ~crc_32r(buffer, entry->size, 0xffffffffu);
    return crc == entry->crc32
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_CRC;
}

bool crazypod_cpk_icon_valid(const struct cpk_reader *reader)
{
    uint8_t header[16];
    const struct cpk_entry *entry = &reader->entries[CPK_ICON];

    if(!read_at_exact(reader->fd, entry->data_offset,
                      header, sizeof(header)))
        return false;
    return entry->size == ICON_BYTES &&
           read_le32(header) == ICON_MAGIC &&
           read_le16(header + 4) == 1u &&
           read_le16(header + 6) == 160u &&
           read_le16(header + 8) == 160u &&
           read_le16(header + 10) == 1u &&
           read_le32(header + 12) == 160u * 160u * 2u;
}

struct file_sink {
    int fd;
};

static bool write_file(void *context, const void *buffer, size_t size)
{
    return write_exact(((struct file_sink *)context)->fd, buffer, size);
}

int crazypod_cpk_extract_entry(
    const struct cpk_reader *reader, int entry, const char *path)
{
    struct file_sink sink;
    int result;

    sink.fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if(sink.fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    result = stream_entry(reader, entry, write_file, &sink);
    if(result == CRAZYPOD_MINIAPP_OK && fsync(sink.fd) < 0)
        result = CRAZYPOD_MINIAPP_ERROR_IO;
    if(close(sink.fd) < 0 && result == CRAZYPOD_MINIAPP_OK)
        result = CRAZYPOD_MINIAPP_ERROR_IO;
    if(result != CRAZYPOD_MINIAPP_OK)
        remove(path);
    return result;
}

#endif
