#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "file.h"

#include "../../crazypod_crypto.h"
#include "../../crazypod_miniapps.h"
#include "crazypod_cpk_verifier.h"
#include "crazypod_miniapp_resource_validator.h"

#define IO_BUFFER_SIZE 1024u
#define ICON_BYTES 102454u

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

bool crazypod_cpk_resources_valid(const struct cpk_reader *reader)
{
    const struct cpk_entry *entry;
    if(reader->entry_count != MINIAPP_CPK_V2_ENTRIES)
        return false;
    entry = &reader->entries[CPK_RESOURCES];
    return crazypod_miniapp_resource_container_valid(
        reader->fd, entry->data_offset, entry->size);
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
    uint8_t header[54];
    const struct cpk_entry *entry = &reader->entries[CPK_ICON];
    int32_t width;
    int32_t height;

    if(!read_at_exact(reader->fd, entry->data_offset,
                      header, sizeof(header)) ||
       header[0] != 'B' || header[1] != 'M' ||
       read_le32(header + 2) != ICON_BYTES ||
       read_le32(header + 10) != 54 ||
       read_le32(header + 14) != 40)
        return false;
    width = (int32_t)read_le32(header + 18);
    height = (int32_t)read_le32(header + 22);
    return width == 160 && (height == 160 || height == -160) &&
           read_le16(header + 26) == 1 &&
           read_le16(header + 28) == 32 &&
           read_le32(header + 30) == 0 &&
           read_le32(header + 34) == 102400u;
}

struct sha_sink {
    struct crazypod_sha256_context context;
};

static bool update_sha(void *context, const void *buffer, size_t size)
{
    struct sha_sink *sink = context;
    crazypod_sha256_update(&sink->context, buffer, size);
    return true;
}

int crazypod_cpk_verify_sha256(
    const struct cpk_reader *reader, int entry,
    const uint8_t expected[32])
{
    struct sha_sink sink;
    uint8_t digest[32];
    int result;

    crazypod_sha256_init(&sink.context);
    result = stream_entry(reader, entry, update_sha, &sink);
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    crazypod_sha256_final(&sink.context, digest);
    return memcmp(digest, expected, sizeof(digest)) == 0
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
}

int crazypod_cpk_verify_signature(
    const struct cpk_reader *reader,
    const uint8_t *manifest, size_t manifest_size)
{
    uint8_t signature[CRAZYPOD_MINIAPP_SIGNATURE_SIZE];
    int result = crazypod_cpk_read_entry(
        reader, CPK_SIGNATURE, signature, sizeof(signature));

    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    return crazypod_ed25519_verify(
               signature, manifest, manifest_size,
               crazypod_miniapp_development_public_key)
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
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
