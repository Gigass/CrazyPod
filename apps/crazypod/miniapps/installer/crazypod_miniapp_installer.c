#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "errno.h"
#include "file.h"

#include "../../crazypod_miniapps.h"
#include "../catalog/crazypod_miniapp_registry_loader.h"
#include "../runtime/crazypod_miniapp_installed_verifier.h"
#include "../runtime/crazypod_miniapp_verification_cache.h"
#include "crazypod_cpk_reader.h"
#include "crazypod_cpk_verifier.h"
#include "crazypod_miniapp_install_record.h"
#include "crazypod_miniapp_manifest.h"
#include "crazypod_miniapp_stage.h"

#define MINIAPP_ROOT "/.crazypod/miniapps"
#define DATA_ROOT "/.crazypod/miniapp-data"
#define USER_ROOT "/MiniApps"
#define USER_INSTALL "/MiniApps/Install"
#define SYSTEM_PACKAGES "/.rockbox/crazypod/miniapps/packages"
#define SCAN_LIMIT 64

static struct {
    struct crazypod_miniapp_metadata metadata;
    struct crazypod_miniapp_metadata verified_metadata;
    char manifest[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    bool in_progress;
    bool rescan_in_progress;
    bool initialized;
    bool packages_scanned;
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
       crazypod_miniapp_registry_package_matches(
           metadata->id, &reader, &installer.verified_metadata) &&
       crazypod_miniapp_installed_verify(
           &installer.verified_metadata) == CRAZYPOD_MINIAPP_OK) {
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

static int scan_directory(const char *path)
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
        result = crazypod_miniapps_install(package);
        if(result < 0 && first_error == CRAZYPOD_MINIAPP_OK)
            first_error = result;
    }
    closedir(directory);
    return first_error;
}

static int rescan_packages(bool recover_publication)
{
    int result;
    int user_result;

    crazypod_miniapp_verification_cache_clear();
    (void)ensure_directory("/.crazypod");
    (void)ensure_directory(MINIAPP_ROOT);
    (void)ensure_directory(DATA_ROOT);
    (void)ensure_directory(USER_ROOT);
    (void)ensure_directory(USER_INSTALL);
    if(recover_publication)
        crazypod_miniapp_stage_recover_all();
    installer.rescan_in_progress = true;
    result = scan_directory(SYSTEM_PACKAGES);
    user_result = scan_directory(USER_INSTALL);
    installer.rescan_in_progress = false;
    (void)crazypod_miniapp_registry_rebuild();
    installer.packages_scanned = true;
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
    return rescan_packages(true);
}

int crazypod_miniapps_init(void)
{
    int result;

    if(crazypod_miniapps_is_open())
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    if(installer.initialized)
        return CRAZYPOD_MINIAPP_OK;

    /*
     * Keep init limited to publication recovery and the installed catalog so
     * the UI can present the boot screen before prepare scans package inboxes.
     */
    crazypod_miniapp_stage_recover_all();
    result = crazypod_miniapp_registry_rebuild();
    installer.initialized = true;
    installer.packages_scanned = false;
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
    if(installer.packages_scanned)
        return CRAZYPOD_MINIAPP_OK;
    if(crazypod_miniapps_is_open())
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    return rescan_packages(false);
}

#endif
