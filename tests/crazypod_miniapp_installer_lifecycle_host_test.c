#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "crazypod_cpk_reader.h"
#include "crazypod_miniapp_install_record.h"
#include "crazypod_miniapp_package_index.h"
#include "crazypod_miniapps.h"

struct test_directory {
    int entry_index;
    bool legacy;
    bool user;
};

static struct test_directory test_directory;
static struct dirent test_entry;
static int open_count;
static int registry_rebuild_count;
static int recovery_count;
static int verification_clear_count;
static int package_open_count;
static int publish_count;
static int installed_verify_count;
static int package_crc_count;
static int package_match_count;
static int verification_mark_count;
static bool installed_same_version;
static bool package_matches_install_record;
static bool package_cached[3];
static bool legacy_scan_enabled;
static struct crazypod_miniapp_metadata catalog_metadata;

DIR *test_opendir(const char *path)
{
    if(strcmp(path, "/.rockbox/crazypod/miniapps/packages") != 0 &&
       strcmp(path, "/MiniApps") != 0 &&
       (!legacy_scan_enabled || strcmp(path, "/MiniApps/Install") != 0))
        return NULL;
    ++open_count;
    test_directory.entry_index = 0;
    test_directory.legacy = strcmp(path, "/MiniApps/Install") == 0;
    test_directory.user =
        strncmp(path, "/MiniApps", strlen("/MiniApps")) == 0;
    return &test_directory;
}

struct dirent *test_readdir(DIR *directory)
{
    if(legacy_scan_enabled && directory->user && !directory->legacy)
        return NULL;
    if(directory->entry_index++ != 0)
        return NULL;
    snprintf(test_entry.d_name, sizeof(test_entry.d_name), "test.cpk");
    return &test_entry;
}

int test_closedir(DIR *directory)
{
    (void)directory;
    return 0;
}

struct dirinfo test_dir_get_info(
    DIR *directory, struct dirent *entry)
{
    struct dirinfo info = { 0 };

    (void)directory;
    (void)entry;
    info.size = 100;
    info.mtime = 123;
    return info;
}

bool test_dir_exists(const char *path)
{
    (void)path;
    return true;
}

int test_mkdir(const char *path)
{
    (void)path;
    return 0;
}

bool crazypod_miniapps_is_open(void)
{
    return false;
}

void crazypod_miniapp_stage_recover_all(void)
{
    ++recovery_count;
}

bool crazypod_miniapp_stage_recover_id(const char *id)
{
    (void)id;
    return true;
}

bool crazypod_miniapp_stage_has_space(uint32_t extracted_size)
{
    (void)extracted_size;
    return true;
}

int crazypod_miniapp_stage_publish(
    const struct cpk_reader *reader,
    const struct crazypod_miniapp_metadata *metadata,
    struct crazypod_miniapp_metadata *verified_metadata)
{
    (void)reader;
    (void)metadata;
    (void)verified_metadata;
    ++publish_count;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_registry_rebuild(void)
{
    ++registry_rebuild_count;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_catalog_find(const char *id)
{
    if(strcmp(id, "test") != 0 ||
       (!installed_same_version && publish_count == 0))
        return -1;
    return 0;
}

const struct crazypod_miniapp_metadata *
crazypod_miniapp_catalog_get(int index)
{
    if(index != 0)
        return NULL;
    memset(&catalog_metadata, 0, sizeof(catalog_metadata));
    snprintf(catalog_metadata.id,
             sizeof(catalog_metadata.id), "test");
    catalog_metadata.version_code = 1;
    return &catalog_metadata;
}

void crazypod_miniapp_package_index_begin(void)
{
}

bool crazypod_miniapp_package_index_lookup(
    uint8_t source, const char *name,
    uint64_t size, int64_t mtime,
    char *id, size_t id_capacity,
    uint32_t *version_code)
{
    assert(source < 3);
    assert(strcmp(name, "test.cpk") == 0);
    assert(size == 100);
    assert(mtime == 123);
    if(!package_cached[source])
        return false;
    snprintf(id, id_capacity, "test");
    *version_code = 1;
    return true;
}

bool crazypod_miniapp_package_index_note(
    uint8_t source, const char *name,
    uint64_t size, int64_t mtime,
    const char *id, uint32_t version_code)
{
    assert(source < 3);
    assert(strcmp(name, "test.cpk") == 0);
    assert(size == 100);
    assert(mtime == 123);
    assert(strcmp(id, "test") == 0);
    assert(version_code == 1);
    package_cached[source] = true;
    return true;
}

bool crazypod_miniapp_package_index_finish(void)
{
    return true;
}

bool crazypod_miniapp_registry_installed_version(
    const char *id, uint32_t *version)
{
    (void)id;
    if(!installed_same_version)
        return false;
    *version = 1;
    return true;
}

bool crazypod_miniapp_registry_installed_metadata(
    const char *id,
    struct crazypod_miniapp_metadata *verified_metadata)
{
    (void)id;
    if(!installed_same_version)
        return false;
    memset(verified_metadata, 0, sizeof(*verified_metadata));
    snprintf(verified_metadata->id,
             sizeof(verified_metadata->id), "test");
    snprintf(verified_metadata->install_path,
             sizeof(verified_metadata->install_path), "/installed/test");
    verified_metadata->version_code = 1;
    return true;
}

bool crazypod_miniapp_install_record_matches_package(
    const char *directory, const struct cpk_reader *reader,
    uint32_t version_code)
{
    assert(strcmp(directory, "/installed/test") == 0);
    assert(reader != NULL);
    assert(version_code == 1);
    ++package_match_count;
    return package_matches_install_record;
}

int crazypod_miniapp_installed_verify(
    const struct crazypod_miniapp_metadata *metadata)
{
    assert(metadata != NULL);
    ++installed_verify_count;
    return CRAZYPOD_MINIAPP_OK;
}

void crazypod_miniapp_verification_cache_clear(void)
{
    ++verification_clear_count;
}

void crazypod_miniapp_verification_cache_mark(
    const char *id, uint32_t version_code)
{
    (void)id;
    (void)version_code;
    ++verification_mark_count;
}

int crazypod_cpk_open(
    const char *path, struct cpk_reader *reader)
{
    (void)path;
    memset(reader, 0, sizeof(*reader));
    reader->entry_count = MINIAPP_CPK_ENTRIES;
    ++package_open_count;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_cpk_validate_local_headers(struct cpk_reader *reader)
{
    (void)reader;
    return CRAZYPOD_MINIAPP_OK;
}

void crazypod_cpk_close(struct cpk_reader *reader)
{
    (void)reader;
}

int crazypod_cpk_read_entry(
    const struct cpk_reader *reader, int entry,
    void *buffer, size_t capacity)
{
    (void)reader;
    (void)entry;
    if(capacity > 0)
        ((char *)buffer)[0] = '\0';
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_manifest_parse(
    char *buffer, size_t size,
    struct crazypod_miniapp_metadata *metadata)
{
    (void)buffer;
    (void)size;
    memset(metadata, 0, sizeof(*metadata));
    snprintf(metadata->id, sizeof(metadata->id), "test");
    metadata->version_code = 1;
    metadata->package_format = CP_NATIVE_PACKAGE_FORMAT;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_cpk_verify_crc(
    const struct cpk_reader *reader, int entry)
{
    (void)reader;
    (void)entry;
    ++package_crc_count;
    return CRAZYPOD_MINIAPP_OK;
}

bool crazypod_cpk_assets_valid(const struct cpk_reader *reader)
{
    (void)reader;
    return true;
}

bool crazypod_cpk_profile_valid(const struct cpk_reader *reader)
{
    (void)reader;
    return true;
}

bool crazypod_cpk_icon_valid(const struct cpk_reader *reader)
{
    (void)reader;
    return true;
}

int crazypod_cpk_verify_trust(
    const struct cpk_reader *reader,
    const struct crazypod_miniapp_metadata *metadata,
    const char *manifest, size_t manifest_size,
    bool allow_unsigned)
{
    (void)reader;
    (void)metadata;
    (void)manifest;
    (void)manifest_size;
    (void)allow_unsigned;
    return CRAZYPOD_MINIAPP_OK;
}

static void assert_boot_work(void)
{
    assert(crazypod_miniapps_init() == CRAZYPOD_MINIAPP_OK);
    assert(recovery_count == 1);
    assert(registry_rebuild_count == 1);
    assert(verification_clear_count == 0);
    assert(open_count == 0);
    assert(package_open_count == 0);
    assert(publish_count == 0);

    assert(crazypod_miniapps_init() == CRAZYPOD_MINIAPP_OK);
    assert(recovery_count == 1);
    assert(registry_rebuild_count == 1);
}

static void assert_cold_boot_rescan_prepares_registry(void)
{
    assert_boot_work();

    assert(crazypod_miniapps_rescan() == CRAZYPOD_MINIAPP_OK);
    assert(recovery_count == 1);
    assert(registry_rebuild_count == 2);
    assert(verification_clear_count == 1);
    assert(open_count == 2);
    assert(package_open_count == 2);
    assert(publish_count == 2);

    assert(crazypod_miniapps_rescan() == CRAZYPOD_MINIAPP_OK);
    assert(registry_rebuild_count == 2);
    assert(open_count == 4);
    assert(package_open_count == 2);
    assert(publish_count == 2);
    assert(verification_mark_count == 4);

    assert(crazypod_miniapps_prepare() == CRAZYPOD_MINIAPP_OK);
    assert(crazypod_miniapps_prepare() == CRAZYPOD_MINIAPP_OK);
    assert(recovery_count == 1);
    assert(registry_rebuild_count == 2);
    assert(verification_clear_count == 2);
    assert(open_count == 4);
    assert(package_open_count == 2);
    assert(publish_count == 2);

    assert(crazypod_miniapps_install("/manual.cpk") ==
           CRAZYPOD_MINIAPP_OK);
    assert(registry_rebuild_count == 3);
}

static void assert_usb_rescan_keeps_entry_immediate(void)
{
    assert_boot_work();

    assert(crazypod_miniapps_rescan() == CRAZYPOD_MINIAPP_OK);
    assert(recovery_count == 1);
    assert(registry_rebuild_count == 2);
    assert(package_open_count == 2);

    assert(crazypod_miniapps_prepare() == CRAZYPOD_MINIAPP_OK);
    assert(recovery_count == 1);
    assert(registry_rebuild_count == 2);
    assert(verification_clear_count == 1);
    assert(package_open_count == 2);

    assert(crazypod_miniapps_rescan() == CRAZYPOD_MINIAPP_OK);
    assert(package_open_count == 2);
    assert(registry_rebuild_count == 2);
}

static void assert_same_version_changed_package_republishes(void)
{
    assert_boot_work();
    installed_same_version = true;

    assert(crazypod_miniapps_rescan() == CRAZYPOD_MINIAPP_OK);
    assert(package_open_count == 2);
    assert(package_match_count == 2);
    assert(installed_verify_count == 0);
    assert(verification_mark_count == 2);
    assert(package_crc_count == MINIAPP_CPK_ENTRIES * 2);
    assert(publish_count == 2);

    assert(crazypod_miniapps_prepare() == CRAZYPOD_MINIAPP_OK);
    assert(package_open_count == 2);
    assert(installed_verify_count == 0);
    assert(verification_mark_count == 2);
    assert(package_crc_count == MINIAPP_CPK_ENTRIES * 2);
    assert(publish_count == 2);

    assert(crazypod_miniapps_rescan() == CRAZYPOD_MINIAPP_OK);
    assert(package_open_count == 2);
    assert(installed_verify_count == 0);
    assert(verification_mark_count == 4);
}

static void assert_same_package_timestamp_change_is_noop(void)
{
    assert_boot_work();
    installed_same_version = true;
    package_matches_install_record = true;

    assert(crazypod_miniapps_rescan() == CRAZYPOD_MINIAPP_OK);
    assert(package_open_count == 2);
    assert(package_match_count == 2);
    assert(package_crc_count == 0);
    assert(publish_count == 0);
    assert(registry_rebuild_count == 1);
    assert(verification_mark_count == 2);

    assert(crazypod_miniapps_rescan() == CRAZYPOD_MINIAPP_OK);
    assert(package_open_count == 2);
    assert(package_match_count == 2);
    assert(package_crc_count == 0);
    assert(publish_count == 0);
    assert(registry_rebuild_count == 1);
    assert(verification_mark_count == 4);
}

static void assert_incremental_rescan_yields_per_package(void)
{
    assert_boot_work();

    assert(crazypod_miniapps_rescan_begin() == CRAZYPOD_MINIAPP_OK);
    assert(crazypod_miniapps_rescan_active());
    assert(crazypod_miniapps_rescan_step());
    assert(open_count == 1);
    assert(package_open_count == 1);
    assert(publish_count == 1);
    assert(crazypod_miniapps_rescan_active());
    assert(crazypod_miniapps_rescan_step());
    assert(open_count == 2);
    assert(package_open_count == 2);
    assert(publish_count == 2);
    assert(crazypod_miniapps_rescan_active());
    assert(!crazypod_miniapps_rescan_step());
    assert(!crazypod_miniapps_rescan_active());
    assert(crazypod_miniapps_rescan_result() == CRAZYPOD_MINIAPP_OK);
    assert(registry_rebuild_count == 2);
}

static void assert_legacy_install_directory_is_scanned(void)
{
    assert_boot_work();
    legacy_scan_enabled = true;

    assert(crazypod_miniapps_rescan() == CRAZYPOD_MINIAPP_OK);
    assert(open_count == 3);
    assert(package_open_count == 2);
    assert(publish_count == 2);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if(strcmp(argv[1], "boot") == 0)
        assert_cold_boot_rescan_prepares_registry();
    else if(strcmp(argv[1], "usb") == 0) {
        assert_usb_rescan_keeps_entry_immediate();
    } else if(strcmp(argv[1], "same") == 0) {
        assert_same_version_changed_package_republishes();
    } else if(strcmp(argv[1], "identical") == 0) {
        assert_same_package_timestamp_change_is_noop();
    } else if(strcmp(argv[1], "legacy") == 0) {
        assert_legacy_install_directory_is_scanned();
    } else {
        assert(strcmp(argv[1], "incremental") == 0);
        assert_incremental_rescan_yields_per_package();
    }
    return 0;
}
