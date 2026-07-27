#ifndef CRAZYPOD_CRYPTO_H
#define CRAZYPOD_CRYPTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CRAZYPOD_SHA256_DIGEST_SIZE 32u
#define CRAZYPOD_ED25519_PUBLIC_KEY_SIZE 32u
#define CRAZYPOD_ED25519_SIGNATURE_SIZE 64u

struct crazypod_sha256_context {
    uint32_t state[8];
    uint64_t total_size;
    uint8_t block[64];
    size_t block_size;
};

void crazypod_sha256_init(struct crazypod_sha256_context *context);
void crazypod_sha256_update(struct crazypod_sha256_context *context,
                            const void *data, size_t size);
void crazypod_sha256_final(struct crazypod_sha256_context *context,
                           uint8_t digest[CRAZYPOD_SHA256_DIGEST_SIZE]);

bool crazypod_ed25519_verify(
    const uint8_t signature[CRAZYPOD_ED25519_SIGNATURE_SIZE],
    const uint8_t *message, size_t size,
    const uint8_t public_key[CRAZYPOD_ED25519_PUBLIC_KEY_SIZE]);

extern const uint8_t crazypod_miniapp_development_public_key[
    CRAZYPOD_ED25519_PUBLIC_KEY_SIZE];

#endif
