#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <string.h>

#include "crc32.h"
#include "file.h"

#include "../../crazypod_miniapps.h"
#include "crazypod_miniapp_resource_validator.h"

#define RESOURCE_MAX (128u * 1024u)
#define RESOURCES_MAX (512u * 1024u)
#define RESOURCE_COUNT_MAX 32u
#define RESOURCE_HEADER_SIZE 16u
#define RESOURCE_ENTRY_SIZE 52u
#define RESOURCE_MAGIC 0x53525043u
#define RESOURCE_VERSION 1u
#define IO_BUFFER_SIZE 1024u

static uint16_t read_le16(const uint8_t *value)
{
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8);
}

static uint32_t read_le32(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static uint32_t crc_finish(uint32_t crc)
{
    return ~crc;
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

static bool valid_id(const uint8_t id[32])
{
    size_t index;
    size_t length = 0;

    while(length < 32 && id[length] != 0)
        ++length;
    if(length == 0 || length == 32 ||
       id[0] < 'a' || id[0] > 'z')
        return false;
    for(index = 1; index < length; ++index) {
        uint8_t value = id[index];
        if(!((value >= 'a' && value <= 'z') ||
             (value >= '0' && value <= '9') ||
             value == '_' || value == '-' || value == '.'))
            return false;
    }
    for(index = length + 1; index < 32; ++index)
        if(id[index] != 0)
            return false;
    return true;
}

bool crazypod_miniapp_resource_container_valid(
    int fd, uint32_t base_offset, uint32_t total_size)
{
    uint8_t header[RESOURCE_HEADER_SIZE];
    uint8_t entry[RESOURCE_ENTRY_SIZE];
    uint8_t payload[IO_BUFFER_SIZE];
    uint8_t previous_id[32] = { 0 };
    uint32_t expected_offset;
    uint32_t index_crc = 0xffffffffu;
    uint16_t count;
    uint16_t index;

    if(total_size < RESOURCE_HEADER_SIZE ||
       total_size > RESOURCES_MAX ||
       !read_at_exact(fd, base_offset, header, sizeof(header)) ||
       read_le32(header) != RESOURCE_MAGIC ||
       read_le16(header + 4) != RESOURCE_VERSION)
        return false;
    count = read_le16(header + 6);
    if(count > RESOURCE_COUNT_MAX ||
       read_le32(header + 8) != total_size ||
       (uint64_t)RESOURCE_HEADER_SIZE +
           (uint64_t)count * RESOURCE_ENTRY_SIZE > total_size)
        return false;
    expected_offset =
        RESOURCE_HEADER_SIZE + (uint32_t)count * RESOURCE_ENTRY_SIZE;
    for(index = 0; index < count; ++index) {
        uint32_t offset;
        uint32_t size;
        uint16_t width;
        uint16_t height;
        uint32_t remaining;
        uint32_t payload_offset;
        uint32_t payload_crc = 0xffffffffu;

        if(!read_at_exact(
               fd, base_offset + RESOURCE_HEADER_SIZE +
                   (uint32_t)index * RESOURCE_ENTRY_SIZE,
               entry, sizeof(entry)) ||
           !valid_id(entry) ||
           (index > 0 && memcmp(previous_id, entry, 32) >= 0) ||
           entry[33] != 0 || read_le16(entry + 38) != 0)
            return false;
        memcpy(previous_id, entry, sizeof(previous_id));
        index_crc = crc_32r(entry, sizeof(entry), index_crc);
        width = read_le16(entry + 34);
        height = read_le16(entry + 36);
        offset = read_le32(entry + 40);
        size = read_le32(entry + 44);
        if(size > RESOURCE_MAX || offset != expected_offset ||
           (uint64_t)offset + size > total_size)
            return false;
        if(entry[32] == CP_RESOURCE_BITMAP_RGB565) {
            if(width == 0 || height == 0 ||
               width > 160 || height > 160 ||
               size != (uint32_t)width * height * 2u)
                return false;
        }
        else if(entry[32] != CP_RESOURCE_BLOB ||
                width != 0 || height != 0) {
            return false;
        }
        remaining = size;
        payload_offset = offset;
        while(remaining > 0) {
            uint32_t amount = remaining > sizeof(payload)
                ? (uint32_t)sizeof(payload) : remaining;
            if(!read_at_exact(
                   fd, base_offset + payload_offset,
                   payload, amount))
                return false;
            payload_crc = crc_32r(payload, amount, payload_crc);
            payload_offset += amount;
            remaining -= amount;
        }
        if(crc_finish(payload_crc) != read_le32(entry + 48))
            return false;
        expected_offset += size;
    }
    return expected_offset == total_size &&
           crc_finish(index_crc) == read_le32(header + 12);
}

#endif
