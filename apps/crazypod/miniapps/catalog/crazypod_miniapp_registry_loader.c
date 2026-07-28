#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "crazypod_miniapp_catalog.h"
#include "crazypod_miniapp_registry_loader.h"
#include "../installer/crazypod_miniapp_install_record.h"

#define MINIAPP_ROOT "/.crazypod/miniapps"
#define IO_BUFFER_SIZE 512u

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

static bool make_path(
    char *path, size_t capacity,
    const char *directory, const char *name)
{
    int length = snprintf(path, capacity, "%s/%s", directory, name);
    return length >= 0 && (size_t)length < capacity;
}

static bool copy_text(
    char *destination, size_t capacity, const char *source)
{
    size_t length = strlen(source);
    if(length >= capacity)
        return false;
    memcpy(destination, source, length + 1);
    return true;
}

static int populate_paths(
    struct crazypod_miniapp_metadata *metadata,
    const char *directory)
{
    if(!copy_text(metadata->install_path,
                  sizeof(metadata->install_path), directory) ||
       !make_path(metadata->binary_path, sizeof(metadata->binary_path),
                  directory, metadata->binary) ||
       !make_path(metadata->icon_path, sizeof(metadata->icon_path),
                  directory, "icon.bmp") ||
       (metadata->package_format == 2u &&
        !make_path(metadata->resources_path,
                   sizeof(metadata->resources_path),
                   directory, "resources.bin")))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_registry_rebuild(void)
{
    DIR *directory;
    struct dirent *entry;

    crazypod_miniapp_catalog_reset();
    directory = opendir(MINIAPP_ROOT);
    if(directory == NULL)
        return CRAZYPOD_MINIAPP_OK;
    while((entry = readdir(directory)) != NULL &&
          crazypod_miniapp_catalog_count() < CRAZYPOD_MINIAPP_MAX_APPS) {
        struct dirinfo info;
        struct crazypod_miniapp_metadata metadata;
        char path[MAX_PATH];

        if(!valid_id(entry->d_name))
            continue;
        info = dir_get_info(directory, entry);
        if((info.attribute & ATTR_DIRECTORY) == 0 ||
           !make_path(path, sizeof(path), MINIAPP_ROOT, entry->d_name) ||
           !crazypod_miniapp_install_directory_validate(
               path, entry->d_name, &metadata, NULL) ||
           populate_paths(&metadata, path) != CRAZYPOD_MINIAPP_OK)
            continue;
        (void)crazypod_miniapp_catalog_add(&metadata);
    }
    closedir(directory);
    crazypod_miniapp_catalog_sort();
    return CRAZYPOD_MINIAPP_OK;
}

bool crazypod_miniapp_registry_installed_version(
    const char *id, uint32_t *version)
{
    struct crazypod_miniapp_metadata metadata;
    char path[MAX_PATH];

    if(version == NULL ||
       !make_path(path, sizeof(path), MINIAPP_ROOT, id) ||
       !dir_exists(path) ||
       !crazypod_miniapp_install_directory_validate(
           path, id, &metadata, NULL))
        return false;
    *version = metadata.version_code;
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

static bool read_at_exact(
    int fd, uint32_t offset, void *buffer, size_t size)
{
    return lseek(fd, (off_t)offset, SEEK_SET) >= 0 &&
           read_exact(fd, buffer, size);
}

static bool file_matches(
    const char *path, const struct cpk_reader *reader, int entry_index)
{
    uint8_t installed[IO_BUFFER_SIZE];
    uint8_t packaged[IO_BUFFER_SIZE];
    const struct cpk_entry *entry = &reader->entries[entry_index];
    uint32_t remaining = entry->size;
    uint32_t package_offset = entry->data_offset;
    int fd = open(path, O_RDONLY);

    if(fd < 0 || filesize(fd) != (off_t)entry->size) {
        if(fd >= 0)
            close(fd);
        return false;
    }
    while(remaining > 0) {
        uint32_t amount = remaining > sizeof(installed)
            ? (uint32_t)sizeof(installed) : remaining;
        if(!read_exact(fd, installed, amount) ||
           !read_at_exact(reader->fd, package_offset,
                          packaged, amount) ||
           memcmp(installed, packaged, amount) != 0) {
            close(fd);
            return false;
        }
        package_offset += amount;
        remaining -= amount;
    }
    close(fd);
    return true;
}

bool crazypod_miniapp_registry_package_matches(
    const char *id, const struct cpk_reader *reader,
    struct crazypod_miniapp_metadata *verified_metadata)
{
    static const char *const names[MINIAPP_CPK_MAX_ENTRIES] = {
        "manifest.ini",
#if CONFIG_BINFMT == BINFMT_ROCK
        "app.arm",
#else
        "app.dylib",
#endif
        "icon.bmp", "signature.ed25519", "resources.bin",
    };
    struct install_record record;
    char directory[MAX_PATH];
    char path[MAX_PATH];
    int index;

    if(!make_path(directory, sizeof(directory), MINIAPP_ROOT, id) ||
       !crazypod_miniapp_install_directory_validate(
           directory, id, verified_metadata, &record))
        return false;
    for(index = 0; index < reader->entry_count; ++index) {
        if((index < MINIAPP_CPK_V1_ENTRIES &&
            (record.files[index].size != reader->entries[index].size ||
             record.files[index].crc32 != reader->entries[index].crc32)) ||
           !make_path(path, sizeof(path), directory, names[index]) ||
           !file_matches(path, reader, index))
            return false;
    }
    return true;
}

#endif
