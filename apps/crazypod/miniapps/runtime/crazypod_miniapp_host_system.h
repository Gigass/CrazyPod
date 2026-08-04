#ifndef CRAZYPOD_MINIAPP_HOST_SYSTEM_H
#define CRAZYPOD_MINIAPP_HOST_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t crazypod_miniapp_host_monotonic_ms(void);
uint32_t crazypod_miniapp_host_epoch_seconds(void);
bool crazypod_miniapp_host_session_begin(size_t reserved_size);
bool crazypod_miniapp_host_memory_reserve(size_t size);
bool crazypod_miniapp_host_memory_replace(
    size_t old_size, size_t new_size);
void crazypod_miniapp_host_memory_release(size_t size);
size_t crazypod_miniapp_host_memory_used(void);
size_t crazypod_miniapp_host_memory_high_water(void);
size_t crazypod_miniapp_host_memory_limit(void);
void crazypod_miniapp_host_memory_reset_high_water(void);
void crazypod_miniapp_host_session_finish(void);

#endif
