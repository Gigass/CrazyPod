#ifndef CRAZYPOD_MINIAPP_NATIVE_VALIDATOR_H
#define CRAZYPOD_MINIAPP_NATIVE_VALIDATOR_H

#include "load_code.h"

#include "../installer/crazypod_cpk_reader.h"
#include "../../crazypod_miniapps.h"

struct miniapp_binary_header_runtime {
    struct lc_header lc_header;
    cp_miniapp_entry_fn entry;
    unsigned char *bss_start;
    uint32_t host_api_size;
    uint32_t ops_size;
};

bool crazypod_miniapp_native_header_valid(
    const struct miniapp_binary_header_runtime *header,
    uint32_t file_size);
int crazypod_miniapp_native_package_validate(
    const struct cpk_reader *reader);
int crazypod_miniapp_native_installed_validate(
    const struct crazypod_miniapp_metadata *metadata,
    uint32_t *file_size);

#endif
