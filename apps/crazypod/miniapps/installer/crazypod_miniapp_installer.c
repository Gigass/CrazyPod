#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "errno.h"
#include "file.h"

#include "../../crazypod_miniapps.h"
#include "../catalog/crazypod_miniapp_catalog.h"
#include "../catalog/crazypod_miniapp_registry_loader.h"
#include "../runtime/crazypod_miniapp_installed_verifier.h"
#include "../runtime/crazypod_miniapp_verification_cache.h"
#include "crazypod_cpk_reader.h"
#include "crazypod_cpk_verifier.h"
#include "crazypod_miniapp_install_record.h"
#include "crazypod_miniapp_manifest.h"
#include "crazypod_miniapp_package_index.h"
#include "crazypod_miniapp_stage.h"

#define MINIAPP_ROOT "/.crazypod/miniapps"
#define DATA_ROOT "/.crazypod/miniapp-data"
#define USER_ROOT "/MiniApps"
#define SYSTEM_PACKAGES "/.rockbox/crazypod/miniapps/packages"
#define SCAN_LIMIT 64
#define SEMANTIC_FONT_ROOT "/.rockbox/fonts/crazypod-aot"

static struct {
    struct crazypod_miniapp_metadata metadata;
    struct crazypod_miniapp_metadata verified_metadata;
    char manifest[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    bool in_progress;
    bool rescan_in_progress;
    bool initialized;
} installer;

static bool ensure_directory(const char *path)
{
    if(mkdir(path) == 0)
        return true;
    return errno == EEXIST && dir_exists(path);
}

static bool make_path(
    char *path, size_t capacity,
    const char *directory, const char *name)
{
    int length = snprintf(path, capacity, "%s/%s", directory, name);
    return length >= 0 && (size_t)length < capacity;
}

static bool semantic_font_set_available(const char *font_set)
{
    static const char *const locales[] = { "jp", "kr", "sc", "tc" };
    const char *cursor = font_set;

    while(cursor != NULL && *cursor != '\0') {
        const char *family;
        unsigned family_length;
        unsigned weight = 0;
        unsigned size = 0;
        unsigned locale;
        char path[MAX_PATH];

        if(strncmp(cursor, "system:", 7) == 0) {
            family = "system";
            family_length = 7;
        }
        else if(strncmp(cursor, "serif:", 6) == 0) {
            family = "serif";
            family_length = 6;
        }
        else if(strncmp(cursor, "mono:", 5) == 0) {
            family = "mono";
            family_length = 5;
        }
        else {
            return false;
        }
        cursor += family_length;
        while(*cursor >= '0' && *cursor <= '9') {
            weight = weight * 10u + (unsigned)(*cursor++ - '0');
        }
        if(*cursor++ != ':' || weight < 100u || weight > 900u ||
           weight % 100u != 0)
            return false;
        while(*cursor >= '0' && *cursor <= '9') {
            size = size * 10u + (unsigned)(*cursor++ - '0');
        }
        if(size < 6u || size > 48u ||
           (*cursor != '\0' && *cursor != ','))
            return false;
        for(locale = 0; locale < sizeof(locales) / sizeof(locales[0]);
            ++locale) {
            int length = snprintf(
                path, sizeof(path), "%s/%s-%s-%u-%u.fnt",
                SEMANTIC_FONT_ROOT, locales[locale], family, weight, size);
            int fd;

            if(length < 0 || (size_t)length >= sizeof(path))
                return false;
            fd = open(path, O_RDONLY);
            if(fd < 0)
                return false;
            close(fd);
        }
        if(*cursor == ',') {
            cursor++;
            if(*cursor == '\0')
                return false;
        }
    }
    return true;
}

int crazypod_miniapps_install(const char *package_path)
{
    struct cpk_reader reader;
    struct crazypod_miniapp_metadata *metadata = &installer.metadata;
    uint32_t installed_version;
    uint32_t extracted_size = sizeof(struct install_record);
    bool same_version = false;
    int result;
    int index;

    if(package_path == NULL || package_path[0] != '/')
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(crazypod_miniapps_is_open() || installer.in_progress)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    installer.in_progress = true;
    result = crazypod_cpk_open(package_path, &reader);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    result = crazypod_cpk_validate_local_headers(&reader);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    result = crazypod_cpk_read_entry(
        &reader, CPK_MANIFEST, installer.manifest,
        CRAZYPOD_MINIAPP_MANIFEST_MAX);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    result = crazypod_miniapp_manifest_parse(
        installer.manifest, reader.entries[CPK_MANIFEST].size, metadata);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    if(!semantic_font_set_available(metadata->font_set)) {
        result = CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
        goto done;
    }
    if(metadata->package_format != CP_NATIVE_PACKAGE_FORMAT ||
       reader.entry_count != MINIAPP_CPK_MAX_ENTRIES) {
        result = CRAZYPOD_MINIAPP_ERROR_FORMAT;
        goto done;
    }
    if(crazypod_miniapp_registry_installed_version(
           metadata->id, &installed_version)) {
        same_version = installed_version == metadata->version_code;
        if(installed_version > metadata->version_code) {
            result = CRAZYPOD_MINIAPP_DOWNGRADE_IGNORED;
            goto done;
        }
    }
    if(same_version &&
       crazypod_miniapp_registry_installed_metadata(
           metadata->id, &installer.verified_metadata) &&
       crazypod_miniapp_installed_verify(
           &installer.verified_metadata) == CRAZYPOD_MINIAPP_OK) {
        /* A versionCode identifies immutable package contents.  Ignore a
         * second, different package at the same version when the installed
         * copy is healthy; otherwise scan order lets a stale user-inbox CPK
         * overwrite a newer system CPK on every boot. */
        crazypod_miniapp_verification_cache_mark(
            metadata->id, metadata->version_code);
        result = CRAZYPOD_MINIAPP_ALREADY_INSTALLED;
        goto done;
    }
    for(index = 0; index < reader.entry_count; ++index) {
        result = crazypod_cpk_verify_crc(&reader, index);
        if(result != CRAZYPOD_MINIAPP_OK)
            goto done;
    }
    if(!crazypod_cpk_profile_valid(&reader) ||
       !crazypod_cpk_assets_valid(&reader)) {
        result = CRAZYPOD_MINIAPP_ERROR_FORMAT;
        goto done;
    }
    if(!crazypod_cpk_icon_valid(&reader)) {
        result = CRAZYPOD_MINIAPP_ERROR_FORMAT;
        goto done;
    }
    for(index = 0; index < reader.entry_count; ++index)
        extracted_size += reader.entries[index].size;
    if(!crazypod_miniapp_stage_has_space(extracted_size)) {
        result = CRAZYPOD_MINIAPP_ERROR_SPACE;
        goto done;
    }
    if(!ensure_directory("/.crazypod") ||
       !ensure_directory(MINIAPP_ROOT) ||
       !crazypod_miniapp_stage_recover_id(metadata->id)) {
        result = CRAZYPOD_MINIAPP_ERROR_IO;
        goto done;
    }
    result = crazypod_miniapp_stage_publish(
        &reader, metadata, &installer.verified_metadata);
    if(result == CRAZYPOD_MINIAPP_OK) {
        crazypod_miniapp_verification_cache_mark(
            metadata->id, metadata->version_code);
        if(!installer.rescan_in_progress) {
            (void)crazypod_miniapp_registry_rebuild();
        }
    }
done:
    crazypod_cpk_close(&reader);
    installer.in_progress = false;
    return result;
}

static bool has_cpk_extension(const char *name)
{
    size_t length = name != NULL ? strlen(name) : 0;
    return length > 4 && length < MAX_PATH &&
           !(name[0] == '.' && name[1] == '_') &&
           strcmp(name + length - 4, ".cpk") == 0;
}

static bool cached_package_ready(
    uint8_t source, const char *name,
    const struct dirinfo *info)
{
    const struct crazypod_miniapp_metadata *metadata;
    char id[CRAZYPOD_MINIAPP_ID_SIZE];
    uint32_t version_code;
    uint64_t size = info->size > 0 ? (uint64_t)info->size : 0;
    int64_t mtime = info->mtime > 0 ? (int64_t)info->mtime : 0;
    int index;

    if(!crazypod_miniapp_package_index_lookup(
           source, name, size, mtime,
           id, sizeof(id), &version_code))
        return false;
    index = crazypod_miniapp_catalog_find(id);
    metadata = crazypod_miniapp_catalog_get(index);
    if(metadata == NULL || metadata->version_code < version_code)
        return false;
    (void)crazypod_miniapp_package_index_note(
        source, name, size, mtime,
        metadata->id, metadata->version_code);
    crazypod_miniapp_verification_cache_mark(
        metadata->id, metadata->version_code);
    return true;
}

static int scan_directory(
    const char *path, uint8_t source,
    bool *publication_changed)
{
    DIR *directory = opendir(path);
    struct dirent *entry;
    int first_error = CRAZYPOD_MINIAPP_OK;
    int scanned = 0;

    if(directory == NULL)
        return CRAZYPOD_MINIAPP_OK;
    while((entry = readdir(directory)) != NULL && scanned < SCAN_LIMIT) {
        struct dirinfo info;
        char package[MAX_PATH];
        int result;
        if(!has_cpk_extension(entry->d_name))
            continue;
        info = dir_get_info(directory, entry);
        if((info.attribute & ATTR_DIRECTORY) != 0 ||
           !make_path(package, sizeof(package), path, entry->d_name))
            continue;
        ++scanned;
        if(cached_package_ready(source, entry->d_name, &info))
            continue;
        result = crazypod_miniapps_install(package);
        if(result >= CRAZYPOD_MINIAPP_OK) {
            (void)crazypod_miniapp_package_index_note(
                source, entry->d_name,
                info.size > 0 ? (uint64_t)info.size : 0,
                info.mtime > 0 ? (int64_t)info.mtime : 0,
                installer.metadata.id,
                installer.metadata.version_code);
            if(result == CRAZYPOD_MINIAPP_OK)
                *publication_changed = true;
        }
        if(result < 0 && first_error == CRAZYPOD_MINIAPP_OK)
            first_error = result;
    }
    closedir(directory);
    return first_error;
}

static int rescan_packages(void)
{
    int result;
    int user_result;
    bool publication_changed = false;

    crazypod_miniapp_verification_cache_clear();
    (void)ensure_directory("/.crazypod");
    (void)ensure_directory(MINIAPP_ROOT);
    (void)ensure_directory(DATA_ROOT);
    (void)ensure_directory(USER_ROOT);
    crazypod_miniapp_package_index_begin();
    installer.rescan_in_progress = true;
    result = scan_directory(
        SYSTEM_PACKAGES, CRAZYPOD_MINIAPP_PACKAGE_SYSTEM,
        &publication_changed);
    user_result = scan_directory(
        USER_ROOT, CRAZYPOD_MINIAPP_PACKAGE_USER,
        &publication_changed);
    installer.rescan_in_progress = false;
    (void)crazypod_miniapp_package_index_finish();
    if(publication_changed)
        (void)crazypod_miniapp_registry_rebuild();
    return result < 0 ? result : user_result;
}

int crazypod_miniapps_rescan(void)
{
    int result;

    if(crazypod_miniapps_is_open())
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    if(!installer.initialized) {
        result = crazypod_miniapps_init();
        if(result < 0)
            return result;
    }
    return rescan_packages();
}

int crazypod_miniapps_init(void)
{
    int result;

    if(crazypod_miniapps_is_open())
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    if(installer.initialized)
        return CRAZYPOD_MINIAPP_OK;

    /* Keep init limited to publication recovery and the installed catalog.
     * The UI performs the package scan explicitly after the boot screen is
     * visible and before the desktop becomes interactive. */
    crazypod_miniapp_stage_recover_all();
    result = crazypod_miniapp_registry_rebuild();
    installer.initialized = true;
    return result;
}

int crazypod_miniapps_prepare(void)
{
    int result;

    if(!installer.initialized) {
        result = crazypod_miniapps_init();
        if(result < 0)
            return result;
    }
    if(crazypod_miniapps_is_open())
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    return CRAZYPOD_MINIAPP_OK;
}

#endif
