#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "file.h"

#include "../../crazypod_miniapps.h"
#include "crazypod_miniapp_package_index.h"

#ifndef CRAZYPOD_MINIAPP_PACKAGE_INDEX_PATH
#define CRAZYPOD_MINIAPP_PACKAGE_INDEX_PATH \
    "/.crazypod/miniapp-package-index.bin"
#endif
#ifndef CRAZYPOD_MINIAPP_PACKAGE_INDEX_TEMP
#define CRAZYPOD_MINIAPP_PACKAGE_INDEX_TEMP \
    "/.crazypod/miniapp-package-index.tmp"
#endif

#define PACKAGE_INDEX_MAGIC 0x43505049u
#define PACKAGE_INDEX_VERSION 1u
#define PACKAGE_INDEX_CAPACITY 128u
#define PACKAGE_INDEX_NAME_SIZE 128u

struct package_index_entry {
    uint64_t size;
    int64_t mtime;
    uint32_t version_code;
    uint8_t source;
    uint8_t reserved[3];
    char name[PACKAGE_INDEX_NAME_SIZE];
    char id[CRAZYPOD_MINIAPP_ID_SIZE];
};

struct package_index_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint16_t entry_size;
    uint16_t entry_count;
    uint16_t abi_major;
    uint16_t abi_minor;
    uint16_t package_format;
    uint16_t reserved;
    uint32_t checksum;
};

static struct {
    struct package_index_entry entries[PACKAGE_INDEX_CAPACITY];
    bool seen[PACKAGE_INDEX_CAPACITY];
    unsigned count;
} package_index;

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

static uint32_t checksum_update(
    uint32_t value, const void *data, size_t size)
{
    const uint8_t *bytes = data;

    while(size-- > 0) {
        value ^= *bytes++;
        value *= 16777619u;
    }
    return value;
}

static uint32_t package_index_checksum(
    const struct package_index_header *source,
    const struct package_index_entry *entries,
    unsigned count)
{
    struct package_index_header header = *source;
    uint32_t value = 2166136261u;

    header.checksum = 0;
    value = checksum_update(value, &header, sizeof(header));
    return checksum_update(
        value, entries, count * sizeof(entries[0]));
}

static bool terminated(const char *value, size_t capacity)
{
    return memchr(value, '\0', capacity) != NULL;
}

static bool valid_entry(const struct package_index_entry *entry)
{
    size_t name_length;
    size_t id_length;

    if((entry->source != CRAZYPOD_MINIAPP_PACKAGE_SYSTEM &&
        entry->source != CRAZYPOD_MINIAPP_PACKAGE_USER) ||
       entry->size == 0 || entry->version_code == 0 ||
       !terminated(entry->name, sizeof(entry->name)) ||
       !terminated(entry->id, sizeof(entry->id)))
        return false;
    name_length = strlen(entry->name);
    id_length = strlen(entry->id);
    return name_length > 4 &&
        strcmp(entry->name + name_length - 4, ".cpk") == 0 &&
        id_length > 0 && id_length < sizeof(entry->id);
}

static bool package_index_load(void)
{
    struct package_index_header header = { 0 };
    off_t expected_size;
    uint32_t checksum;
    int fd = open(CRAZYPOD_MINIAPP_PACKAGE_INDEX_PATH, O_RDONLY);
    unsigned index;
    bool valid;

    if(fd < 0)
        return false;
    valid = read_exact(fd, &header, sizeof(header)) &&
        header.magic == PACKAGE_INDEX_MAGIC &&
        header.version == PACKAGE_INDEX_VERSION &&
        header.header_size == sizeof(header) &&
        header.entry_size == sizeof(package_index.entries[0]) &&
        header.entry_count <= PACKAGE_INDEX_CAPACITY &&
        header.abi_major == CP_NATIVE_ABI_MAJOR &&
        header.abi_minor == CP_NATIVE_ABI_MINOR &&
        header.package_format == CP_NATIVE_PACKAGE_FORMAT;
    expected_size = (off_t)sizeof(header) +
        (off_t)header.entry_count * sizeof(package_index.entries[0]);
    if(valid)
        valid = filesize(fd) == expected_size &&
            read_exact(
                fd, package_index.entries,
                header.entry_count * sizeof(package_index.entries[0]));
    close(fd);
    if(!valid)
        return false;
    checksum = package_index_checksum(
        &header, package_index.entries, header.entry_count);
    if(checksum != header.checksum)
        return false;
    for(index = 0; index < header.entry_count; ++index)
        if(!valid_entry(&package_index.entries[index]))
            return false;
    package_index.count = header.entry_count;
    return true;
}

static bool package_index_save(void)
{
    struct package_index_header header;
    bool complete;
    int fd;

    memset(&header, 0, sizeof(header));
    header.magic = PACKAGE_INDEX_MAGIC;
    header.version = PACKAGE_INDEX_VERSION;
    header.header_size = sizeof(header);
    header.entry_size = sizeof(package_index.entries[0]);
    header.entry_count = package_index.count;
    header.abi_major = CP_NATIVE_ABI_MAJOR;
    header.abi_minor = CP_NATIVE_ABI_MINOR;
    header.package_format = CP_NATIVE_PACKAGE_FORMAT;
    header.checksum = package_index_checksum(
        &header, package_index.entries, package_index.count);
    remove(CRAZYPOD_MINIAPP_PACKAGE_INDEX_TEMP);
    fd = open(
        CRAZYPOD_MINIAPP_PACKAGE_INDEX_TEMP,
        O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    complete = write_exact(fd, &header, sizeof(header)) &&
        write_exact(
            fd, package_index.entries,
            package_index.count * sizeof(package_index.entries[0])) &&
        fsync(fd) == 0;
    if(close(fd) < 0)
        complete = false;
    if(!complete || rename(
           CRAZYPOD_MINIAPP_PACKAGE_INDEX_TEMP,
           CRAZYPOD_MINIAPP_PACKAGE_INDEX_PATH) < 0) {
        remove(CRAZYPOD_MINIAPP_PACKAGE_INDEX_TEMP);
        return false;
    }
    return true;
}

static int find_entry(uint8_t source, const char *name)
{
    unsigned index;

    for(index = 0; index < package_index.count; ++index)
        if(package_index.entries[index].source == source &&
           strcmp(package_index.entries[index].name, name) == 0)
            return (int)index;
    return -1;
}

void crazypod_miniapp_package_index_begin(void)
{
    memset(&package_index, 0, sizeof(package_index));
    (void)package_index_load();
}

bool crazypod_miniapp_package_index_lookup(
    uint8_t source, const char *name,
    uint64_t size, int64_t mtime,
    char *id, size_t id_capacity,
    uint32_t *version_code)
{
    int index;
    size_t id_length;

    if(name == NULL || id == NULL || id_capacity == 0 ||
       version_code == NULL)
        return false;
    index = find_entry(source, name);
    if(index < 0 || package_index.entries[index].size != size ||
       package_index.entries[index].mtime != mtime)
        return false;
    id_length = strlen(package_index.entries[index].id);
    if(id_length >= id_capacity)
        return false;
    memcpy(id, package_index.entries[index].id, id_length + 1);
    *version_code = package_index.entries[index].version_code;
    return true;
}

bool crazypod_miniapp_package_index_note(
    uint8_t source, const char *name,
    uint64_t size, int64_t mtime,
    const char *id, uint32_t version_code)
{
    struct package_index_entry *entry;
    size_t name_length;
    size_t id_length;
    int index;

    if(name == NULL || id == NULL || size == 0 || version_code == 0)
        return false;
    name_length = strlen(name);
    id_length = strlen(id);
    if(name_length == 0 || name_length >= PACKAGE_INDEX_NAME_SIZE ||
       id_length == 0 || id_length >= CRAZYPOD_MINIAPP_ID_SIZE)
        return false;
    index = find_entry(source, name);
    if(index < 0) {
        if(package_index.count >= PACKAGE_INDEX_CAPACITY)
            return false;
        index = (int)package_index.count++;
    }
    entry = &package_index.entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->size = size;
    entry->mtime = mtime;
    entry->version_code = version_code;
    entry->source = source;
    memcpy(entry->name, name, name_length + 1);
    memcpy(entry->id, id, id_length + 1);
    package_index.seen[index] = true;
    return true;
}

bool crazypod_miniapp_package_index_finish(void)
{
    unsigned source;
    unsigned destination = 0;

    for(source = 0; source < package_index.count; ++source) {
        if(!package_index.seen[source])
            continue;
        if(destination != source)
            package_index.entries[destination] =
                package_index.entries[source];
        ++destination;
    }
    package_index.count = destination;
    return package_index_save();
}

#endif
