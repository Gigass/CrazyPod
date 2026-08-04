#ifndef CRAZYPOD_MINIAPP_PACKAGE_INDEX_H
#define CRAZYPOD_MINIAPP_PACKAGE_INDEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum crazypod_miniapp_package_source {
    CRAZYPOD_MINIAPP_PACKAGE_SYSTEM = 1,
    CRAZYPOD_MINIAPP_PACKAGE_USER = 2,
};

void crazypod_miniapp_package_index_begin(void);
bool crazypod_miniapp_package_index_lookup(
    uint8_t source, const char *name,
    uint64_t size, int64_t mtime,
    char *id, size_t id_capacity,
    uint32_t *version_code);
bool crazypod_miniapp_package_index_note(
    uint8_t source, const char *name,
    uint64_t size, int64_t mtime,
    const char *id, uint32_t version_code);
bool crazypod_miniapp_package_index_finish(void);

#endif
