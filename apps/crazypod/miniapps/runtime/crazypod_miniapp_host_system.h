#ifndef CRAZYPOD_MINIAPP_HOST_SYSTEM_H
#define CRAZYPOD_MINIAPP_HOST_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t crazypod_miniapp_host_monotonic_ms(void);
uint32_t crazypod_miniapp_host_epoch_seconds(void);
bool crazypod_miniapp_host_session_begin(size_t reserved_size);
bool crazypod_miniapp_host_memory_reserve(size_t size);
void crazypod_miniapp_host_memory_release(size_t size);
size_t crazypod_miniapp_host_memory_used(void);
void crazypod_miniapp_host_session_finish(void);

#endif
