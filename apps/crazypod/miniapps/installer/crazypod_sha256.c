#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "crazypod_sha256.h"

static const uint32_t constants[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,
    0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
    0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,
    0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,
    0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
    0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,
    0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,
    0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,
    0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
};

static uint32_t rotate(uint32_t value, unsigned bits)
{
    return (value >> bits) | (value << (32u - bits));
}

static uint32_t read_be32(const uint8_t *value)
{
    return ((uint32_t)value[0] << 24) | ((uint32_t)value[1] << 16) |
           ((uint32_t)value[2] << 8) | value[3];
}

static void transform(struct crazypod_sha256 *context, const uint8_t block[64])
{
    uint32_t words[64];
    uint32_t a,b,c,d,e,f,g,h;
    unsigned index;

    for(index = 0; index < 16; ++index)
        words[index] = read_be32(block + index * 4u);
    for(index = 16; index < 64; ++index) {
        uint32_t x = words[index - 15u];
        uint32_t y = words[index - 2u];
        uint32_t s0 = rotate(x, 7) ^ rotate(x, 18) ^ (x >> 3);
        uint32_t s1 = rotate(y, 17) ^ rotate(y, 19) ^ (y >> 10);
        words[index] = words[index - 16u] + s0 +
            words[index - 7u] + s1;
    }
    a=context->state[0]; b=context->state[1]; c=context->state[2];
    d=context->state[3]; e=context->state[4]; f=context->state[5];
    g=context->state[6]; h=context->state[7];
    for(index = 0; index < 64; ++index) {
        uint32_t s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t first = h + s1 + choice + constants[index] + words[index];
        uint32_t s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t second = s0 + majority;
        h=g; g=f; f=e; e=d+first; d=c; c=b; b=a; a=first+second;
    }
    context->state[0]+=a; context->state[1]+=b;
    context->state[2]+=c; context->state[3]+=d;
    context->state[4]+=e; context->state[5]+=f;
    context->state[6]+=g; context->state[7]+=h;
}

void crazypod_sha256_init(struct crazypod_sha256 *context)
{
    static const uint32_t initial[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u,
    };
    memcpy(context->state, initial, sizeof(initial));
    context->bytes = 0;
    context->used = 0;
}

void crazypod_sha256_update(
    struct crazypod_sha256 *context, const void *data, size_t size)
{
    const uint8_t *bytes = data;

    context->bytes += size;
    while(size > 0) {
        size_t amount = sizeof(context->buffer) - context->used;
        if(amount > size)
            amount = size;
        memcpy(context->buffer + context->used, bytes, amount);
        context->used += amount;
        bytes += amount;
        size -= amount;
        if(context->used == sizeof(context->buffer)) {
            transform(context, context->buffer);
            context->used = 0;
        }
    }
}

void crazypod_sha256_final(
    struct crazypod_sha256 *context, uint8_t digest[32])
{
    uint64_t bits = context->bytes * 8u;
    unsigned index;

    context->buffer[context->used++] = 0x80u;
    if(context->used > 56u) {
        memset(context->buffer + context->used, 0,
               sizeof(context->buffer) - context->used);
        transform(context, context->buffer);
        context->used = 0;
    }
    memset(context->buffer + context->used, 0, 56u - context->used);
    for(index = 0; index < 8; ++index)
        context->buffer[63u - index] = (uint8_t)(bits >> (index * 8u));
    transform(context, context->buffer);
    for(index = 0; index < 8; ++index) {
        digest[index * 4u] = (uint8_t)(context->state[index] >> 24);
        digest[index * 4u + 1u] = (uint8_t)(context->state[index] >> 16);
        digest[index * 4u + 2u] = (uint8_t)(context->state[index] >> 8);
        digest[index * 4u + 3u] = (uint8_t)context->state[index];
    }
}

void crazypod_hmac_sha256_init(
    struct crazypod_hmac_sha256 *context,
    const void *key, size_t key_size)
{
    uint8_t block[64];
    uint8_t digest[32];
    size_t index;

    memset(block, 0, sizeof(block));
    if(key_size > sizeof(block)) {
        crazypod_sha256_init(&context->inner);
        crazypod_sha256_update(&context->inner, key, key_size);
        crazypod_sha256_final(&context->inner, digest);
        memcpy(block, digest, sizeof(digest));
    }
    else
        memcpy(block, key, key_size);
    for(index = 0; index < sizeof(block); ++index)
        block[index] ^= 0x36u;
    crazypod_sha256_init(&context->inner);
    crazypod_sha256_update(&context->inner, block, sizeof(block));
    for(index = 0; index < sizeof(block); ++index)
        block[index] ^= 0x36u ^ 0x5cu;
    crazypod_sha256_init(&context->outer);
    crazypod_sha256_update(&context->outer, block, sizeof(block));
    memset(digest, 0, sizeof(digest));
    memset(block, 0, sizeof(block));
}

void crazypod_hmac_sha256_update(
    struct crazypod_hmac_sha256 *context,
    const void *data, size_t size)
{
    crazypod_sha256_update(&context->inner, data, size);
}

void crazypod_hmac_sha256_final(
    struct crazypod_hmac_sha256 *context, uint8_t digest[32])
{
    uint8_t inner[32];

    crazypod_sha256_final(&context->inner, inner);
    crazypod_sha256_update(&context->outer, inner, sizeof(inner));
    crazypod_sha256_final(&context->outer, digest);
    memset(inner, 0, sizeof(inner));
}

#endif
