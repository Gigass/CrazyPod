#ifndef CRAZYPOD_MINIAPP_RESOURCE_VALIDATOR_H
#define CRAZYPOD_MINIAPP_RESOURCE_VALIDATOR_H

#include <stdbool.h>
#include <stdint.h>

bool crazypod_miniapp_resource_container_valid(
    int fd, uint32_t base_offset, uint32_t total_size);

#endif
