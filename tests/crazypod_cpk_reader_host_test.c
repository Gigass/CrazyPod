#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "crazypod_cpk_reader.h"
#include "crazypod_cpk_verifier.h"
#include "crazypod_miniapps.h"

static void write_variant(
    const char *path, const void *data, size_t size)
{
    const char *cursor = data;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    assert(fd >= 0);
    while(size > 0) {
        ssize_t count = write(fd, cursor, size);

        assert(count > 0);
        cursor += count;
        size -= (size_t)count;
    }
    assert(close(fd) == 0);
}

int main(int argc, char **argv)
{
    struct cpk_reader reader;
    char manifest[CP_NATIVE_MANIFEST_MAX + 1];
    char corrupt[512];
    char extracted[512];
    char truncated[512];
    uint8_t *package;
    uint32_t app_offset;
    uint32_t package_size;
    char value;
    int index;
    int fd;

    assert(argc == 2);
    assert(crazypod_cpk_open(argv[1], &reader) ==
           CRAZYPOD_MINIAPP_OK);
    assert(reader.entry_count == MINIAPP_CPK_MAX_ENTRIES);
    assert(crazypod_cpk_validate_local_headers(&reader) ==
           CRAZYPOD_MINIAPP_OK);
    assert(strcmp(reader.entries[CPK_MANIFEST].name, "manifest.json") == 0);
    assert(strcmp(reader.entries[CPK_APP].name, "app.dylib") == 0);
    assert(strcmp(reader.entries[CPK_PROFILE].name, "profile.bin") == 0);
    assert(strcmp(reader.entries[CPK_ASSETS].name, "assets.bin") == 0);
    assert(strcmp(reader.entries[CPK_ICON].name, "icon.bin") == 0);
    assert(reader.entries[CPK_APP].size > 1000);
    assert(reader.entries[CPK_PROFILE].size >= 16);
    assert(reader.entries[CPK_ASSETS].size >= 16);
    assert(reader.entries[CPK_ICON].size == 51216);
    for(index = 0; index < MINIAPP_CPK_ENTRIES; ++index)
        assert(crazypod_cpk_verify_crc(&reader, index) ==
               CRAZYPOD_MINIAPP_OK);
    assert(crazypod_cpk_read_entry(
        &reader, CPK_MANIFEST, manifest,
        CP_NATIVE_MANIFEST_MAX) ==
        CRAZYPOD_MINIAPP_OK);
    manifest[reader.entries[CPK_MANIFEST].size] = '\0';
    assert(strstr(
        manifest, "\"format\":5") != NULL);
    assert(crazypod_cpk_profile_valid(&reader));
    assert(crazypod_cpk_assets_valid(&reader));
    assert(crazypod_cpk_icon_valid(&reader));
    app_offset = reader.entries[CPK_APP].data_offset;
    package_size = reader.file_size;

    snprintf(extracted, sizeof(extracted), "%s.extracted", argv[1]);
    assert(crazypod_cpk_extract_entry(
        &reader, CPK_APP, extracted) == CRAZYPOD_MINIAPP_OK);
    fd = open(extracted, O_RDONLY);
    assert(fd >= 0 && read(fd, &value, 1) == 1);
    close(fd);
    unlink(extracted);
    crazypod_cpk_close(&reader);

    package = malloc(package_size);
    assert(package != NULL);
    fd = open(argv[1], O_RDONLY);
    assert(fd >= 0);
    assert(read(fd, package, package_size) == (ssize_t)package_size);
    assert(close(fd) == 0);

    snprintf(corrupt, sizeof(corrupt), "%s.corrupt", argv[1]);
    package[app_offset] ^= 0x01u;
    write_variant(corrupt, package, package_size);
    assert(crazypod_cpk_open(corrupt, &reader) ==
           CRAZYPOD_MINIAPP_OK);
    assert(crazypod_cpk_validate_local_headers(&reader) ==
           CRAZYPOD_MINIAPP_OK);
    assert(crazypod_cpk_verify_crc(&reader, CPK_APP) ==
           CRAZYPOD_MINIAPP_ERROR_CRC);
    crazypod_cpk_close(&reader);
    unlink(corrupt);

    snprintf(truncated, sizeof(truncated), "%s.truncated", argv[1]);
    write_variant(truncated, package, package_size - 1u);
    assert(crazypod_cpk_open(truncated, &reader) !=
           CRAZYPOD_MINIAPP_OK);
    crazypod_cpk_close(&reader);
    unlink(truncated);
    free(package);
    return 0;
}
