#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "file.h"

#include "../../crazypod_miniapps.h"
#include "crazypod_cpk_verifier.h"
#include "crazypod_miniapp_resource_validator.h"
#include "crazypod_sha256.h"

#define IO_BUFFER_SIZE 1024u
#define ICON_BYTES 51216u
#define ICON_MAGIC 0x35495043u
#define PROFILE_MAGIC 0x35415043u
#ifndef TRUST_KEYS_PATH
#define TRUST_KEYS_PATH "/.crazypod/trusted-miniapp-keys.txt"
#endif
#ifndef DEVELOPER_MODE_PATH
#define DEVELOPER_MODE_PATH "/.crazypod/developer-mode.flag"
#endif

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

static int hex_value(char value)
{
    if(value >= '0' && value <= '9') return value - '0';
    if(value >= 'a' && value <= 'f') return value - 'a' + 10;
    if(value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static bool decode_hex(const char *text, uint8_t *output, size_t size)
{
    size_t index;

    for(index = 0; index < size; ++index) {
        int high = hex_value(text[index * 2u]);
        int low = hex_value(text[index * 2u + 1u]);
        if(high < 0 || low < 0)
            return false;
        output[index] = (uint8_t)((high << 4) | low);
    }
    return true;
}

static bool trusted_key(
    const char *key_id, uint8_t key[32])
{
    char buffer[4097];
    off_t size;
    int file = open(TRUST_KEYS_PATH, O_RDONLY);
    char *line;
    char *save = NULL;

    if(file < 0)
        return false;
    size = filesize(file);
    if(size <= 0 || size > 4096 ||
       !read_at_exact(file, 0, buffer, (size_t)size)) {
        close(file);
        return false;
    }
    close(file);
    buffer[size] = '\0';
    line = strtok_r(buffer, "\n", &save);
    while(line != NULL) {
        char *separator = strchr(line, ':');
        if(separator != NULL && separator - line <= 16 &&
           strlen(separator + 1) == 64u) {
            *separator = '\0';
            if(strcmp(line, key_id) == 0)
                return decode_hex(separator + 1, key, 32);
        }
        line = strtok_r(NULL, "\n", &save);
    }
    return false;
}

static void hmac_frame(
    struct crazypod_hmac_sha256 *hmac,
    const char *name, uint32_t size)
{
    uint8_t encoded[4] = {
        (uint8_t)size, (uint8_t)(size >> 8),
        (uint8_t)(size >> 16), (uint8_t)(size >> 24),
    };
    crazypod_hmac_sha256_update(hmac, name, strlen(name) + 1u);
    crazypod_hmac_sha256_update(hmac, encoded, sizeof(encoded));
}

static bool hmac_file_entry(
    const struct cpk_reader *reader, int entry_index,
    struct crazypod_hmac_sha256 *hmac)
{
    const struct cpk_entry *entry = &reader->entries[entry_index];
    uint8_t buffer[IO_BUFFER_SIZE];
    uint32_t remaining = entry->size;
    uint32_t offset = entry->data_offset;

    hmac_frame(hmac, entry->name, entry->size);
    while(remaining > 0) {
        uint32_t amount = remaining > sizeof(buffer)
            ? (uint32_t)sizeof(buffer) : remaining;
        if(!read_at_exact(reader->fd, offset, buffer, amount))
            return false;
        crazypod_hmac_sha256_update(hmac, buffer, amount);
        offset += amount;
        remaining -= amount;
    }
    return true;
}

static const char *signature_value(
    const char *manifest, size_t size)
{
    static const char marker[] = "\"signature\":\"";
    size_t marker_size = sizeof(marker) - 1u;
    size_t index;

    if(size < marker_size + 64u)
        return NULL;
    for(index = 0; index + marker_size + 64u <= size; ++index)
        if(memcmp(manifest + index, marker, marker_size) == 0)
            return manifest + index + marker_size;
    return NULL;
}

static bool developer_mode_enabled(void)
{
    int file = open(DEVELOPER_MODE_PATH, O_RDONLY);
    if(file < 0)
        return false;
    close(file);
    return true;
}

int crazypod_cpk_verify_trust(
    const struct cpk_reader *reader,
    const struct crazypod_miniapp_metadata *metadata,
    const char *manifest, size_t manifest_size,
    bool allow_unsigned)
{
    static const char prefix[] = "CPK5-HMAC-SHA256-V1";
    static const char zeros[64] = {
        '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
    };
    struct crazypod_hmac_sha256 hmac;
    uint8_t key[32];
    uint8_t expected[32];
    uint8_t actual[32];
    const char *signature;
    uint8_t difference = 0;
    int index;

    if(metadata->signature[0] == '\0')
        return allow_unsigned || developer_mode_enabled()
            ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
    signature = signature_value(manifest, manifest_size);
    if(signature == NULL ||
       !trusted_key(metadata->signing_key_id, key) ||
       !decode_hex(metadata->signature, expected, sizeof(expected)))
        return CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
    crazypod_hmac_sha256_init(&hmac, key, sizeof(key));
    memset(key, 0, sizeof(key));
    crazypod_hmac_sha256_update(&hmac, prefix, sizeof(prefix) - 1u);
    hmac_frame(&hmac, reader->entries[CPK_MANIFEST].name,
               reader->entries[CPK_MANIFEST].size);
    crazypod_hmac_sha256_update(&hmac, manifest,
                                (size_t)(signature - manifest));
    crazypod_hmac_sha256_update(&hmac, zeros, sizeof(zeros));
    crazypod_hmac_sha256_update(
        &hmac, signature + 64u,
        manifest_size - (size_t)(signature + 64u - manifest));
    for(index = CPK_APP; index < reader->entry_count; ++index)
        if(!hmac_file_entry(reader, index, &hmac))
            return CRAZYPOD_MINIAPP_ERROR_IO;
    crazypod_hmac_sha256_final(&hmac, actual);
    for(index = 0; index < 32; ++index)
        difference |= actual[index] ^ expected[index];
    memset(actual, 0, sizeof(actual));
    memset(expected, 0, sizeof(expected));
    return difference == 0
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
}

#endif
