#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "disk.h"
#include "file.h"
#include "mv.h"

#include "crazypod_cpk_verifier.h"
#include "crazypod_miniapp_install_record.h"
#include "crazypod_miniapp_stage.h"

#define MINIAPP_ROOT "/.crazypod/miniapps"
#define SCAN_LIMIT 64
#define REMOVE_DEPTH 3
#define REMOVE_ENTRIES 32
#define SPACE_RESERVE (128u * 1024u)

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

static bool tagged_path(
    char *path, size_t capacity,
    const char *tag, const char *id)
{
    int length = snprintf(path, capacity, "%s/.%s-%s",
                          MINIAPP_ROOT, tag, id);
    return length >= 0 && (size_t)length < capacity;
}

static bool remove_tree(const char *path, int depth)
{
    DIR *directory;
    struct dirent *entry;
    int entries = 0;
    bool success = true;

    if(path == NULL || depth < 0)
        return false;
    directory = opendir(path);
    if(directory == NULL)
        return !file_exists(path) && !dir_exists(path);
    while((entry = readdir(directory)) != NULL) {
        struct dirinfo info;
        char child[MAX_PATH];

        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0)
            continue;
        if(++entries > REMOVE_ENTRIES ||
           !make_path(child, sizeof(child), path, entry->d_name)) {
            success = false;
            break;
        }
        info = dir_get_info(directory, entry);
        if((info.attribute & ATTR_DIRECTORY) != 0) {
            if(depth == 0 || !remove_tree(child, depth - 1)) {
                success = false;
                break;
            }
        }
        else if(remove(child) < 0) {
            success = false;
            break;
        }
    }
    closedir(directory);
    return success && rmdir(path) == 0;
}

bool crazypod_miniapp_stage_recover_id(const char *id)
{
    char final_path[MAX_PATH];
    char stage_path[MAX_PATH];
    char backup_path[MAX_PATH];
    bool final_exists;

    if(!valid_id(id) ||
       !make_path(final_path, sizeof(final_path), MINIAPP_ROOT, id) ||
       !tagged_path(stage_path, sizeof(stage_path), "stage", id) ||
       !tagged_path(backup_path, sizeof(backup_path), "backup", id))
        return false;
    final_exists = dir_exists(final_path);
    if(dir_exists(backup_path)) {
        if(final_exists) {
            if(!remove_tree(backup_path, REMOVE_DEPTH))
                return false;
        }
        else {
            if(rename(backup_path, final_path) < 0)
                return false;
            final_exists = true;
        }
    }
    if(dir_exists(stage_path) &&
       !remove_tree(stage_path, REMOVE_DEPTH))
        return false;
    return final_exists || !dir_exists(final_path);
}

static void recover_tag(const char *tag)
{
    DIR *directory = opendir(MINIAPP_ROOT);
    struct dirent *entry;
    char prefix[16];
    size_t prefix_length;
    int scanned = 0;

    if(directory == NULL)
        return;
    snprintf(prefix, sizeof(prefix), ".%s-", tag);
    prefix_length = strlen(prefix);
    while((entry = readdir(directory)) != NULL && scanned < SCAN_LIMIT) {
        const char *id;
        if(strncmp(entry->d_name, prefix, prefix_length) != 0)
            continue;
        ++scanned;
        id = entry->d_name + prefix_length;
        if(valid_id(id))
            (void)crazypod_miniapp_stage_recover_id(id);
    }
    closedir(directory);
}

void crazypod_miniapp_stage_recover_all(void)
{
    recover_tag("backup");
    recover_tag("stage");
}

bool crazypod_miniapp_stage_has_space(uint32_t extracted_size)
{
    sector_t free_sectors = 0;
    uint64_t free_bytes;
    unsigned int sector_size;

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    volume_recalc_free(IF_MV(0));
#endif
    volume_size(IF_MV(0,) NULL, &free_sectors);
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    sector_size = (unsigned int)disk_get_log_sector_size(IF_MD(0));
#else
    sector_size = 512u;
#endif
    if(sector_size == 0)
        return false;
    free_bytes = (uint64_t)free_sectors * sector_size;
    return free_bytes >= (uint64_t)extracted_size + SPACE_RESERVE;
}

static int build(
    const struct cpk_reader *reader,
    const struct crazypod_miniapp_metadata *metadata,
    struct crazypod_miniapp_metadata *verified)
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
    char stage_path[MAX_PATH];
    char file_path[MAX_PATH];
    int result;
    int index;

    if(!tagged_path(stage_path, sizeof(stage_path),
                    "stage", metadata->id))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(dir_exists(stage_path) &&
       !remove_tree(stage_path, REMOVE_DEPTH))
        return CRAZYPOD_MINIAPP_ERROR_IO;
    if(mkdir(stage_path) < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    for(index = 0; index < reader->entry_count; ++index) {
        if(!make_path(file_path, sizeof(file_path),
                      stage_path, names[index])) {
            result = CRAZYPOD_MINIAPP_ERROR_LIMIT;
            goto fail;
        }
        result = crazypod_cpk_extract_entry(reader, index, file_path);
        if(result != CRAZYPOD_MINIAPP_OK)
            goto fail;
    }
    if(!crazypod_miniapp_install_record_write(
           stage_path, reader, metadata->version_code)) {
        result = CRAZYPOD_MINIAPP_ERROR_IO;
        goto fail;
    }
    if(!crazypod_miniapp_install_directory_validate(
           stage_path, metadata->id, verified, NULL)) {
        result = CRAZYPOD_MINIAPP_ERROR_FORMAT;
        goto fail;
    }
    return CRAZYPOD_MINIAPP_OK;
fail:
    (void)remove_tree(stage_path, REMOVE_DEPTH);
    return result;
}

static int commit(const char *id)
{
    char final_path[MAX_PATH];
    char stage_path[MAX_PATH];
    char backup_path[MAX_PATH];
    bool had_final;

    if(!make_path(final_path, sizeof(final_path), MINIAPP_ROOT, id) ||
       !tagged_path(stage_path, sizeof(stage_path), "stage", id) ||
       !tagged_path(backup_path, sizeof(backup_path), "backup", id))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(dir_exists(backup_path) &&
       !remove_tree(backup_path, REMOVE_DEPTH))
        return CRAZYPOD_MINIAPP_ERROR_IO;
    had_final = dir_exists(final_path);
    if(had_final && rename(final_path, backup_path) < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    if(rename(stage_path, final_path) < 0) {
        if(had_final)
            (void)rename(backup_path, final_path);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    if(had_final && dir_exists(backup_path))
        (void)remove_tree(backup_path, REMOVE_DEPTH);
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_stage_publish(
    const struct cpk_reader *reader,
    const struct crazypod_miniapp_metadata *metadata,
    struct crazypod_miniapp_metadata *verified_metadata)
{
    int result = build(reader, metadata, verified_metadata);
    return result == CRAZYPOD_MINIAPP_OK ? commit(metadata->id) : result;
}

#endif
