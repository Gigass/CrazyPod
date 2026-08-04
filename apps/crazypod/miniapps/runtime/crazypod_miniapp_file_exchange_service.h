#ifndef CRAZYPOD_MINIAPP_FILE_EXCHANGE_SERVICE_H
#define CRAZYPOD_MINIAPP_FILE_EXCHANGE_SERVICE_H

#include <stddef.h>
#include <stdint.h>

struct crazypod_miniapp_metadata;

int crazypod_miniapp_file_exchange_service_call(
    const struct crazypod_miniapp_metadata *metadata,
    uint32_t operation, const void *request, size_t request_size,
    void *response, size_t response_capacity);

#endif
