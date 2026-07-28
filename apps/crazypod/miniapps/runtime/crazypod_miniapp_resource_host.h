#ifndef CRAZYPOD_MINIAPP_RESOURCE_HOST_H
#define CRAZYPOD_MINIAPP_RESOURCE_HOST_H

#include "../../crazypod_miniapps.h"

int crazypod_miniapp_resource_stat(
    const struct crazypod_miniapp_metadata *metadata,
    const char *id, struct cp_resource_info *info);
int crazypod_miniapp_resource_read(
    const struct crazypod_miniapp_metadata *metadata,
    const char *id, uint32_t offset,
    void *buffer, size_t capacity);

#endif
