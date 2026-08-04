#ifndef CRAZYPOD_SHA256_H
#define CRAZYPOD_SHA256_H

#include <stddef.h>
#include <stdint.h>

struct crazypod_sha256 {
    uint32_t state[8];
    uint64_t bytes;
    uint8_t buffer[64];
    size_t used;
};

struct crazypod_hmac_sha256 {
    struct crazypod_sha256 inner;
    struct crazypod_sha256 outer;
};

void crazypod_sha256_init(struct crazypod_sha256 *context);
void crazypod_sha256_update(
    struct crazypod_sha256 *context, const void *data, size_t size);
void crazypod_sha256_final(
    struct crazypod_sha256 *context, uint8_t digest[32]);
void crazypod_hmac_sha256_init(
    struct crazypod_hmac_sha256 *context,
    const void *key, size_t key_size);
void crazypod_hmac_sha256_update(
    struct crazypod_hmac_sha256 *context,
    const void *data, size_t size);
void crazypod_hmac_sha256_final(
    struct crazypod_hmac_sha256 *context, uint8_t digest[32]);

#endif
