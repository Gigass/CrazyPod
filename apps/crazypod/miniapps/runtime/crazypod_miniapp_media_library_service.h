#ifndef CRAZYPOD_MINIAPP_MEDIA_LIBRARY_SERVICE_H
#define CRAZYPOD_MINIAPP_MEDIA_LIBRARY_SERVICE_H

#include <stddef.h>
#include <stdint.h>

int crazypod_miniapp_media_library_service_call(
    uint32_t operation, const void *request, size_t request_size,
    void *response, size_t response_capacity);

#endif
