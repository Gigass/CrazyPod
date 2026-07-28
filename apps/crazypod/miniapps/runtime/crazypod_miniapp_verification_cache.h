#ifndef CRAZYPOD_MINIAPP_VERIFICATION_CACHE_H
#define CRAZYPOD_MINIAPP_VERIFICATION_CACHE_H

#include <stdbool.h>
#include <stdint.h>

void crazypod_miniapp_verification_cache_clear(void);
bool crazypod_miniapp_verification_cache_contains(
    const char *id, uint32_t version_code);
void crazypod_miniapp_verification_cache_mark(
    const char *id, uint32_t version_code);

#endif
