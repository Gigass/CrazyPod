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
       !make_path(metadata->profile_path,
                  sizeof(metadata->profile_path),
                  directory, "profile.bin") ||
       !make_path(metadata->assets_path,
                  sizeof(metadata->assets_path),
                  directory, "assets.bin") ||
       !make_path(metadata->icon_path, sizeof(metadata->icon_path),
                  directory, "icon.bin") ||
       !make_path(metadata->binary_path,
                  sizeof(metadata->binary_path),
                  directory, metadata->entry))
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

bool crazypod_miniapp_registry_package_matches(
    const char *id, const struct cpk_reader *reader,
    struct crazypod_miniapp_metadata *verified_metadata)
{
    struct install_record record;
    char directory[MAX_PATH];
    int index;

    if(!make_path(directory, sizeof(directory), MINIAPP_ROOT, id) ||
       !crazypod_miniapp_install_directory_validate(
           directory, id, verified_metadata, &record))
        return false;
    for(index = 0; index < reader->entry_count; ++index) {
        if(record.files[index].size != reader->entries[index].size ||
           record.files[index].crc32 != reader->entries[index].crc32)
            return false;
    }
    return populate_paths(verified_metadata, directory) ==
        CRAZYPOD_MINIAPP_OK;
}

#endif
