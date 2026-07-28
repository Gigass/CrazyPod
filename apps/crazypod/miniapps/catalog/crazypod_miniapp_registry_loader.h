#ifndef CRAZYPOD_MINIAPP_REGISTRY_LOADER_H
#define CRAZYPOD_MINIAPP_REGISTRY_LOADER_H

#include <stdbool.h>
#include <stdint.h>

#include "../installer/crazypod_cpk_reader.h"
#include "../../crazypod_miniapps.h"

int crazypod_miniapp_registry_rebuild(void);
bool crazypod_miniapp_registry_installed_version(
    const char *id, uint32_t *version);
bool crazypod_miniapp_registry_package_matches(
    const char *id, const struct cpk_reader *reader,
    struct crazypod_miniapp_metadata *verified_metadata);

#endif
