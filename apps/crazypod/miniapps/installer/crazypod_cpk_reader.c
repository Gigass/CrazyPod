#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <string.h>

#include "file.h"

#include "../../crazypod_miniapps.h"
#include "crazypod_cpk_reader.h"
#include "crazypod_miniapp_manifest.h"

#define ICON_BYTES 51216u
#define CPK_MAX (12u * 1024u * 1024u)
#define ZIP_SIG_EOCD 0x06054b50u
#define ZIP_SIG_CENTRAL 0x02014b50u
#define ZIP_SIG_LOCAL 0x04034b50u

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

static int expected_entry(const char *name)
{
    if(strcmp(name, "manifest.json") == 0) return CPK_MANIFEST;
    if(strcmp(name, "app.arm") == 0 ||
       strcmp(name, "app.dylib") == 0) return CPK_APP;
    if(strcmp(name, "profile.bin") == 0) return CPK_PROFILE;
    if(strcmp(name, "assets.bin") == 0) return CPK_ASSETS;
    if(strcmp(name, "icon.bin") == 0) return CPK_ICON;
    return -1;
}

static bool entry_size_valid(
    int entry, const char *name, uint32_t size)
{
    switch(entry) {
    case CPK_MANIFEST:
        return size > 0 && size <= CRAZYPOD_MINIAPP_MANIFEST_MAX;
    case CPK_APP:
        return size > 0 && size <= 8u * 1024u * 1024u;
    case CPK_PROFILE:
        return strcmp(name, "profile.bin") == 0 && size == 16u;
    case CPK_ASSETS:
        return size >= 16u && size <= CP_NATIVE_ASSET_MAX;
    case CPK_ICON:
        return size == ICON_BYTES;
    default:
        return false;
    }
}

int crazypod_cpk_open(
    const char *path, struct cpk_reader *reader)
{
    uint8_t eocd[22];
    uint32_t central_size;
    uint32_t central_offset;
    uint64_t cursor;
    uint32_t seen = 0;
    uint16_t entry_count;
    off_t file_size;
    int index;

    memset(reader, 0, sizeof(*reader));
    reader->fd = open(path, O_RDONLY);
    if(reader->fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    file_size = filesize(reader->fd);
    if(file_size < (off_t)sizeof(eocd) ||
       file_size > (off_t)CPK_MAX)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    reader->file_size = (uint32_t)file_size;
    if(!read_at_exact(reader->fd, reader->file_size - sizeof(eocd),
                      eocd, sizeof(eocd)))
        return CRAZYPOD_MINIAPP_ERROR_IO;
    entry_count = read_le16(eocd + 8);
    if(read_le32(eocd) != ZIP_SIG_EOCD ||
       read_le16(eocd + 4) != 0 || read_le16(eocd + 6) != 0 ||
       entry_count != MINIAPP_CPK_ENTRIES ||
       read_le16(eocd + 10) != entry_count ||
       read_le16(eocd + 20) != 0)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    reader->entry_count = (uint8_t)entry_count;
    central_size = read_le32(eocd + 12);
    central_offset = read_le32(eocd + 16);
    if((uint64_t)central_offset + central_size !=
       (uint64_t)reader->file_size - sizeof(eocd))
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    reader->central_offset = central_offset;
    cursor = central_offset;

    for(index = 0; index < reader->entry_count; ++index) {
        uint8_t header[46];
        char name[24];
        uint16_t name_length;
        uint16_t extra_length;
        uint16_t comment_length;
        int slot;
        struct cpk_entry *entry;

        if(cursor + sizeof(header) >
               (uint64_t)central_offset + central_size ||
           !read_at_exact(reader->fd, (uint32_t)cursor,
                          header, sizeof(header)) ||
           read_le32(header) != ZIP_SIG_CENTRAL)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        name_length = read_le16(header + 28);
        extra_length = read_le16(header + 30);
        comment_length = read_le16(header + 32);
        if(name_length == 0 || name_length >= sizeof(name) ||
           extra_length != 0 || comment_length != 0 ||
           read_le16(header + 34) != 0)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        cursor += sizeof(header);
        if(cursor + name_length >
               (uint64_t)central_offset + central_size ||
           !read_at_exact(reader->fd, (uint32_t)cursor,
                          name, name_length))
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        name[name_length] = '\0';
        slot = expected_entry(name);
        if(slot < 0 || (seen & (1u << slot)) != 0)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        seen |= 1u << slot;
        entry = &reader->entries[slot];
        memcpy(entry->name, name, name_length + 1);
        entry->version_needed = read_le16(header + 6);
        entry->flags = read_le16(header + 8);
        entry->method = read_le16(header + 10);
        entry->dos_time = read_le16(header + 12);
        entry->dos_date = read_le16(header + 14);
        entry->crc32 = read_le32(header + 16);
        if(read_le32(header + 20) != read_le32(header + 24))
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        entry->size = read_le32(header + 24);
        entry->local_offset = read_le32(header + 42);
        if(entry->version_needed > 20 || entry->flags != 0 ||
           entry->method != 0 ||
           !entry_size_valid(slot, name, entry->size) ||
           entry->local_offset >= central_offset)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        cursor += name_length;
    }
    if(seen != 0x1fu ||
       cursor != (uint64_t)central_offset + central_size)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_cpk_validate_local_headers(
    struct cpk_reader *reader)
{
    int order[MINIAPP_CPK_MAX_ENTRIES] = { 0, 1, 2, 3, 4 };
    int index;

    for(index = 0; index < reader->entry_count; ++index) {
        struct cpk_entry *entry = &reader->entries[index];
        uint8_t header[30];
        char name[24];
        uint16_t name_length;
        uint16_t extra_length;
        uint64_t data_offset;
        uint64_t span_end;

        if(!read_at_exact(reader->fd, entry->local_offset,
                          header, sizeof(header)) ||
           read_le32(header) != ZIP_SIG_LOCAL)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        name_length = read_le16(header + 26);
        extra_length = read_le16(header + 28);
        if(name_length == 0 || name_length >= sizeof(name) ||
           extra_length != 0 ||
           read_le16(header + 4) != entry->version_needed ||
           read_le16(header + 6) != entry->flags ||
           read_le16(header + 8) != entry->method ||
           read_le16(header + 10) != entry->dos_time ||
           read_le16(header + 12) != entry->dos_date ||
           read_le32(header + 14) != entry->crc32 ||
           read_le32(header + 18) != entry->size ||
           read_le32(header + 22) != entry->size)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        if(!read_at_exact(reader->fd,
                          entry->local_offset + sizeof(header),
                          name, name_length))
            return CRAZYPOD_MINIAPP_ERROR_IO;
        name[name_length] = '\0';
        if(strcmp(name, entry->name) != 0)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        data_offset = (uint64_t)entry->local_offset +
                      sizeof(header) + name_length;
        span_end = data_offset + entry->size;
        if(data_offset > UINT32_MAX || span_end > reader->central_offset)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        entry->data_offset = (uint32_t)data_offset;
        entry->span_end = (uint32_t)span_end;
    }
    for(index = 1; index < reader->entry_count; ++index) {
        int position = index;
        int value = order[index];
        while(position > 0 &&
              reader->entries[order[position - 1]].local_offset >
              reader->entries[value].local_offset) {
            order[position] = order[position - 1];
            --position;
        }
        order[position] = value;
    }
    if(reader->entries[order[0]].local_offset != 0)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    for(index = 1; index < reader->entry_count; ++index)
        if(reader->entries[order[index - 1]].span_end !=
           reader->entries[order[index]].local_offset)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(reader->entries[order[reader->entry_count - 1]].span_end !=
       reader->central_offset)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    return CRAZYPOD_MINIAPP_OK;
}

void crazypod_cpk_close(struct cpk_reader *reader)
{
    if(reader->fd >= 0)
        close(reader->fd);
    reader->fd = -1;
}

#endif
