#ifndef CRAZYPOD_MINIAPP_INSTALL_RECORD_H
#define CRAZYPOD_MINIAPP_INSTALL_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#include "crazypod_cpk_reader.h"
#include "../../crazypod_miniapps.h"

struct install_file_record {
    uint32_t size;
    uint32_t crc32;
};

struct install_record {
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    uint32_t version_code;
    struct install_file_record files[MINIAPP_CPK_ENTRIES];
    uint32_t checksum;
};

bool crazypod_miniapp_install_record_write(
    const char *directory, const struct cpk_reader *reader,
    uint32_t version_code);
bool crazypod_miniapp_install_record_read(
    const char *directory, struct install_record *record);
bool crazypod_miniapp_install_directory_validate(
    const char *directory, const char *expected_id,
    struct crazypod_miniapp_metadata *metadata,
    struct install_record *record);

#endif
