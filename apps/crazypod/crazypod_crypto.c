/*
 * CrazyPod package hashing and signature verification.
 *
 * The Ed25519 field/group operations are a verify-only adaptation of
 * TweetNaCl 20140427 by Daniel J. Bernstein et al. TweetNaCl is public-domain
 * software: https://tweetnacl.cr.yp.to/20140427/tweetnacl.c
 *
 * This adaptation removes signing, key generation, randombytes, heap-backed
 * signed-message assembly, and unrelated primitives. SHA-512 is streamed over
 * R || A || M so the detached signature verifies the exact message bytes.
 * It also rejects non-canonical S values (S >= L), which the original compact
 * crypto_sign_open did not do.
 */

#include "crazypod_crypto.h"

#ifndef UINT64_C
#define UINT64_C(value) value##ULL
#endif

#ifndef INT64_C
#define INT64_C(value) value##LL
#endif

const uint8_t crazypod_miniapp_development_public_key[
    CRAZYPOD_ED25519_PUBLIC_KEY_SIZE] = {
    0x66, 0xe4, 0x00, 0x64, 0x95, 0x46, 0xfc, 0xb4,
    0xd5, 0xe3, 0x05, 0x03, 0xf9, 0x88, 0xef, 0x58,
    0xf3, 0xf4, 0x91, 0xc8, 0xee, 0xc1, 0xb7, 0x48,
    0x22, 0x14, 0x46, 0xf1, 0x70, 0x68, 0x04, 0xb3
};

static uint32_t rotate_right32(uint32_t value, unsigned int count)
{
    return (value >> count) | (value << (32u - count));
}

static uint32_t load_big_endian32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24) |
           ((uint32_t)source[1] << 16) |
           ((uint32_t)source[2] << 8) |
           (uint32_t)source[3];
}

static void store_big_endian32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value >> 24);
    destination[1] = (uint8_t)(value >> 16);
    destination[2] = (uint8_t)(value >> 8);
    destination[3] = (uint8_t)value;
}

static void store_big_endian64(uint8_t *destination, uint64_t value)
{
    int index;

    for(index = 7; index >= 0; --index) {
        destination[index] = (uint8_t)value;
        value >>= 8;
    }
}

static void sha256_transform(struct crazypod_sha256_context *context,
                             const uint8_t block[64])
{
    static const uint32_t round_constants[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    unsigned int index;

    for(index = 0; index < 16; ++index)
        words[index] = load_big_endian32(block + index * 4u);
    for(index = 16; index < 64; ++index) {
        uint32_t x = words[index - 15];
        uint32_t y = words[index - 2];
        uint32_t sigma0 = rotate_right32(x, 7) ^
                          rotate_right32(x, 18) ^ (x >> 3);
        uint32_t sigma1 = rotate_right32(y, 17) ^
                          rotate_right32(y, 19) ^ (y >> 10);
        words[index] = words[index - 16] + sigma0 +
                       words[index - 7] + sigma1;
    }

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];

    for(index = 0; index < 64; ++index) {
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t sum0 = rotate_right32(a, 2) ^
                        rotate_right32(a, 13) ^
                        rotate_right32(a, 22);
        uint32_t sum1 = rotate_right32(e, 6) ^
                        rotate_right32(e, 11) ^
                        rotate_right32(e, 25);
        uint32_t temporary1 =
            h + sum1 + choice + round_constants[index] + words[index];
        uint32_t temporary2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }

    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}

void crazypod_sha256_init(struct crazypod_sha256_context *context)
{
    size_t index;

    if(context == NULL)
        return;
    context->state[0] = 0x6a09e667u;
    context->state[1] = 0xbb67ae85u;
    context->state[2] = 0x3c6ef372u;
    context->state[3] = 0xa54ff53au;
    context->state[4] = 0x510e527fu;
    context->state[5] = 0x9b05688cu;
    context->state[6] = 0x1f83d9abu;
    context->state[7] = 0x5be0cd19u;
    context->total_size = 0;
    context->block_size = 0;
    for(index = 0; index < sizeof(context->block); ++index)
        context->block[index] = 0;
}

void crazypod_sha256_update(struct crazypod_sha256_context *context,
                            const void *data, size_t size)
{
    const uint8_t *bytes = data;

    if(context == NULL || (data == NULL && size != 0))
        return;
    context->total_size += (uint64_t)size;
    while(size > 0) {
        size_t available = sizeof(context->block) - context->block_size;
        size_t amount = size < available ? size : available;
        size_t index;

        for(index = 0; index < amount; ++index)
            context->block[context->block_size + index] = bytes[index];
        context->block_size += amount;
        bytes += amount;
        size -= amount;
        if(context->block_size == sizeof(context->block)) {
            sha256_transform(context, context->block);
            context->block_size = 0;
        }
    }
}

void crazypod_sha256_final(struct crazypod_sha256_context *context,
                           uint8_t digest[CRAZYPOD_SHA256_DIGEST_SIZE])
{
    uint64_t bit_size;
    size_t index;

    if(context == NULL || digest == NULL)
        return;
    bit_size = context->total_size * 8u;
    context->block[context->block_size++] = 0x80;
    if(context->block_size > 56) {
        while(context->block_size < sizeof(context->block))
            context->block[context->block_size++] = 0;
        sha256_transform(context, context->block);
        context->block_size = 0;
    }
    while(context->block_size < 56)
        context->block[context->block_size++] = 0;
    store_big_endian64(context->block + 56, bit_size);
    sha256_transform(context, context->block);
    for(index = 0; index < 8; ++index)
        store_big_endian32(digest + index * 4u, context->state[index]);
    context->block_size = 0;
}

/* Internal streaming SHA-512 used by Ed25519 verification. */
struct sha512_context {
    uint64_t state[8];
    uint64_t total_low;
    uint64_t total_high;
    uint8_t block[128];
    size_t block_size;
};

static uint64_t rotate_right64(uint64_t value, unsigned int count)
{
    return (value >> count) | (value << (64u - count));
}

static uint64_t load_big_endian64(const uint8_t *source)
{
    uint64_t value = 0;
    unsigned int index;

    for(index = 0; index < 8; ++index)
        value = (value << 8) | source[index];
    return value;
}

static void sha512_transform(struct sha512_context *context,
                             const uint8_t block[128])
{
    static const uint64_t round_constants[80] = {
        UINT64_C(0x428a2f98d728ae22), UINT64_C(0x7137449123ef65cd),
        UINT64_C(0xb5c0fbcfec4d3b2f), UINT64_C(0xe9b5dba58189dbbc),
        UINT64_C(0x3956c25bf348b538), UINT64_C(0x59f111f1b605d019),
        UINT64_C(0x923f82a4af194f9b), UINT64_C(0xab1c5ed5da6d8118),
        UINT64_C(0xd807aa98a3030242), UINT64_C(0x12835b0145706fbe),
        UINT64_C(0x243185be4ee4b28c), UINT64_C(0x550c7dc3d5ffb4e2),
        UINT64_C(0x72be5d74f27b896f), UINT64_C(0x80deb1fe3b1696b1),
        UINT64_C(0x9bdc06a725c71235), UINT64_C(0xc19bf174cf692694),
        UINT64_C(0xe49b69c19ef14ad2), UINT64_C(0xefbe4786384f25e3),
        UINT64_C(0x0fc19dc68b8cd5b5), UINT64_C(0x240ca1cc77ac9c65),
        UINT64_C(0x2de92c6f592b0275), UINT64_C(0x4a7484aa6ea6e483),
        UINT64_C(0x5cb0a9dcbd41fbd4), UINT64_C(0x76f988da831153b5),
        UINT64_C(0x983e5152ee66dfab), UINT64_C(0xa831c66d2db43210),
        UINT64_C(0xb00327c898fb213f), UINT64_C(0xbf597fc7beef0ee4),
        UINT64_C(0xc6e00bf33da88fc2), UINT64_C(0xd5a79147930aa725),
        UINT64_C(0x06ca6351e003826f), UINT64_C(0x142929670a0e6e70),
        UINT64_C(0x27b70a8546d22ffc), UINT64_C(0x2e1b21385c26c926),
        UINT64_C(0x4d2c6dfc5ac42aed), UINT64_C(0x53380d139d95b3df),
        UINT64_C(0x650a73548baf63de), UINT64_C(0x766a0abb3c77b2a8),
        UINT64_C(0x81c2c92e47edaee6), UINT64_C(0x92722c851482353b),
        UINT64_C(0xa2bfe8a14cf10364), UINT64_C(0xa81a664bbc423001),
        UINT64_C(0xc24b8b70d0f89791), UINT64_C(0xc76c51a30654be30),
        UINT64_C(0xd192e819d6ef5218), UINT64_C(0xd69906245565a910),
        UINT64_C(0xf40e35855771202a), UINT64_C(0x106aa07032bbd1b8),
        UINT64_C(0x19a4c116b8d2d0c8), UINT64_C(0x1e376c085141ab53),
        UINT64_C(0x2748774cdf8eeb99), UINT64_C(0x34b0bcb5e19b48a8),
        UINT64_C(0x391c0cb3c5c95a63), UINT64_C(0x4ed8aa4ae3418acb),
        UINT64_C(0x5b9cca4f7763e373), UINT64_C(0x682e6ff3d6b2b8a3),
        UINT64_C(0x748f82ee5defb2fc), UINT64_C(0x78a5636f43172f60),
        UINT64_C(0x84c87814a1f0ab72), UINT64_C(0x8cc702081a6439ec),
        UINT64_C(0x90befffa23631e28), UINT64_C(0xa4506cebde82bde9),
        UINT64_C(0xbef9a3f7b2c67915), UINT64_C(0xc67178f2e372532b),
        UINT64_C(0xca273eceea26619c), UINT64_C(0xd186b8c721c0c207),
        UINT64_C(0xeada7dd6cde0eb1e), UINT64_C(0xf57d4f7fee6ed178),
        UINT64_C(0x06f067aa72176fba), UINT64_C(0x0a637dc5a2c898a6),
        UINT64_C(0x113f9804bef90dae), UINT64_C(0x1b710b35131c471b),
        UINT64_C(0x28db77f523047d84), UINT64_C(0x32caab7b40c72493),
        UINT64_C(0x3c9ebe0a15c9bebc), UINT64_C(0x431d67c49c100d4c),
        UINT64_C(0x4cc5d4becb3e42b6), UINT64_C(0x597f299cfc657e2a),
        UINT64_C(0x5fcb6fab3ad6faec), UINT64_C(0x6c44198c4a475817)
    };
    uint64_t words[16];
    uint64_t a = context->state[0];
    uint64_t b = context->state[1];
    uint64_t c = context->state[2];
    uint64_t d = context->state[3];
    uint64_t e = context->state[4];
    uint64_t f = context->state[5];
    uint64_t g = context->state[6];
    uint64_t h = context->state[7];
    unsigned int index;

    for(index = 0; index < 16; ++index)
        words[index] = load_big_endian64(block + index * 8u);
    for(index = 0; index < 80; ++index) {
        uint64_t sum0;
        uint64_t sum1;
        uint64_t choice;
        uint64_t majority;
        uint64_t temporary1;
        uint64_t temporary2;

        if(index >= 16) {
            uint64_t x = words[(index + 1u) & 15u];
            uint64_t y = words[(index + 14u) & 15u];
            uint64_t sigma0 = rotate_right64(x, 1) ^
                              rotate_right64(x, 8) ^ (x >> 7);
            uint64_t sigma1 = rotate_right64(y, 19) ^
                              rotate_right64(y, 61) ^ (y >> 6);
            words[index & 15u] +=
                sigma0 + words[(index + 9u) & 15u] + sigma1;
        }

        sum1 = rotate_right64(e, 14) ^
               rotate_right64(e, 18) ^
               rotate_right64(e, 41);
        choice = (e & f) ^ (~e & g);
        temporary1 = h + sum1 + choice +
                     round_constants[index] + words[index & 15u];
        sum0 = rotate_right64(a, 28) ^
               rotate_right64(a, 34) ^
               rotate_right64(a, 39);
        majority = (a & b) ^ (a & c) ^ (b & c);
        temporary2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }

    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}

static void sha512_init(struct sha512_context *context)
{
    size_t index;

    context->state[0] = UINT64_C(0x6a09e667f3bcc908);
    context->state[1] = UINT64_C(0xbb67ae8584caa73b);
    context->state[2] = UINT64_C(0x3c6ef372fe94f82b);
    context->state[3] = UINT64_C(0xa54ff53a5f1d36f1);
    context->state[4] = UINT64_C(0x510e527fade682d1);
    context->state[5] = UINT64_C(0x9b05688c2b3e6c1f);
    context->state[6] = UINT64_C(0x1f83d9abfb41bd6b);
    context->state[7] = UINT64_C(0x5be0cd19137e2179);
    context->total_low = 0;
    context->total_high = 0;
    context->block_size = 0;
    for(index = 0; index < sizeof(context->block); ++index)
        context->block[index] = 0;
}

static void sha512_update(struct sha512_context *context,
                          const uint8_t *data, size_t size)
{
    uint64_t previous = context->total_low;

    context->total_low += (uint64_t)size;
    if(context->total_low < previous)
        ++context->total_high;
    while(size > 0) {
        size_t available = sizeof(context->block) - context->block_size;
        size_t amount = size < available ? size : available;
        size_t index;

        for(index = 0; index < amount; ++index)
            context->block[context->block_size + index] = data[index];
        context->block_size += amount;
        data += amount;
        size -= amount;
        if(context->block_size == sizeof(context->block)) {
            sha512_transform(context, context->block);
            context->block_size = 0;
        }
    }
}

static void sha512_final(struct sha512_context *context, uint8_t digest[64])
{
    uint64_t high_bits =
        (context->total_high << 3) | (context->total_low >> 61);
    uint64_t low_bits = context->total_low << 3;
    size_t index;

    context->block[context->block_size++] = 0x80;
    if(context->block_size > 112) {
        while(context->block_size < sizeof(context->block))
            context->block[context->block_size++] = 0;
        sha512_transform(context, context->block);
        context->block_size = 0;
    }
    while(context->block_size < 112)
        context->block[context->block_size++] = 0;
    store_big_endian64(context->block + 112, high_bits);
    store_big_endian64(context->block + 120, low_bits);
    sha512_transform(context, context->block);
    for(index = 0; index < 8; ++index)
        store_big_endian64(digest + index * 8u, context->state[index]);
}

typedef int64_t field_element[16];

static const field_element field_zero;
static const field_element field_one = { 1 };
static const field_element curve_d = {
    0x78a3, 0x1359, 0x4dca, 0x75eb,
    0xd8ab, 0x4141, 0x0a4d, 0x0070,
    0xe898, 0x7779, 0x4079, 0x8cc7,
    0xfe73, 0x2b6f, 0x6cee, 0x5203
};
static const field_element curve_d_times_two = {
    0xf159, 0x26b2, 0x9b94, 0xebd6,
    0xb156, 0x8283, 0x149a, 0x00e0,
    0xd130, 0xeef3, 0x80f2, 0x198e,
    0xfce7, 0x56df, 0xd9dc, 0x2406
};
static const field_element base_x = {
    0xd51a, 0x8f25, 0x2d60, 0xc956,
    0xa7b2, 0x9525, 0xc760, 0x692c,
    0xdc5c, 0xfdd6, 0xe231, 0xc0a4,
    0x53fe, 0xcd6e, 0x36d3, 0x2169
};
static const field_element base_y = {
    0x6658, 0x6666, 0x6666, 0x6666,
    0x6666, 0x6666, 0x6666, 0x6666,
    0x6666, 0x6666, 0x6666, 0x6666,
    0x6666, 0x6666, 0x6666, 0x6666
};
static const field_element square_root_minus_one = {
    0xa0b0, 0x4a0e, 0x1b27, 0xc4ee,
    0xe478, 0xad2f, 0x1806, 0x2f43,
    0xd7a7, 0x3dfb, 0x0099, 0x2b4d,
    0xdf0b, 0x4fc1, 0x2480, 0x2b83
};

static const uint8_t scalar_order[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

static int constant_compare32(const uint8_t left[32],
                              const uint8_t right[32])
{
    uint32_t difference = 0;
    unsigned int index;

    for(index = 0; index < 32; ++index)
        difference |= (uint32_t)(left[index] ^ right[index]);
    return (int)((1u & ((difference - 1u) >> 8)) - 1u);
}

static void field_copy(field_element destination,
                       const field_element source)
{
    int index;

    for(index = 0; index < 16; ++index)
        destination[index] = source[index];
}

static void field_carry(field_element value)
{
    int index;

    for(index = 0; index < 16; ++index) {
        int64_t carry;

        value[index] += INT64_C(1) << 16;
        carry = value[index] >> 16;
        value[(index + 1) * (index < 15)] +=
            carry - 1 + 37 * (carry - 1) * (index == 15);
        value[index] -= carry * (INT64_C(1) << 16);
    }
}

static void field_select(field_element left, field_element right, int select)
{
    int64_t mask = -(int64_t)select;
    int index;

    for(index = 0; index < 16; ++index) {
        int64_t difference = mask & (left[index] ^ right[index]);
        left[index] ^= difference;
        right[index] ^= difference;
    }
}

static void field_pack(uint8_t output[32], const field_element input)
{
    field_element reduced;
    field_element candidate;
    int pass;
    int index;

    field_copy(reduced, input);
    field_carry(reduced);
    field_carry(reduced);
    field_carry(reduced);
    for(pass = 0; pass < 2; ++pass) {
        int borrow;

        candidate[0] = reduced[0] - 0xffed;
        for(index = 1; index < 15; ++index) {
            candidate[index] =
                reduced[index] - 0xffff -
                ((candidate[index - 1] >> 16) & 1);
            candidate[index - 1] &= 0xffff;
        }
        candidate[15] =
            reduced[15] - 0x7fff -
            ((candidate[14] >> 16) & 1);
        borrow = (int)((candidate[15] >> 16) & 1);
        candidate[14] &= 0xffff;
        field_select(reduced, candidate, 1 - borrow);
    }
    for(index = 0; index < 16; ++index) {
        output[index * 2] = (uint8_t)reduced[index];
        output[index * 2 + 1] = (uint8_t)(reduced[index] >> 8);
    }
}

static int field_not_equal(const field_element left,
                           const field_element right)
{
    uint8_t packed_left[32];
    uint8_t packed_right[32];

    field_pack(packed_left, left);
    field_pack(packed_right, right);
    return constant_compare32(packed_left, packed_right);
}

static uint8_t field_parity(const field_element value)
{
    uint8_t packed[32];

    field_pack(packed, value);
    return packed[0] & 1u;
}

static void field_unpack(field_element output, const uint8_t input[32])
{
    int index;

    for(index = 0; index < 16; ++index)
        output[index] = input[index * 2] |
                        ((int64_t)input[index * 2 + 1] << 8);
    output[15] &= 0x7fff;
}

static void field_add(field_element output, const field_element left,
                      const field_element right)
{
    int index;

    for(index = 0; index < 16; ++index)
        output[index] = left[index] + right[index];
}

static void field_subtract(field_element output, const field_element left,
                           const field_element right)
{
    int index;

    for(index = 0; index < 16; ++index)
        output[index] = left[index] - right[index];
}

static void field_multiply(field_element output, const field_element left,
                           const field_element right)
{
    int64_t product[31];
    int left_index;
    int right_index;

    for(left_index = 0; left_index < 31; ++left_index)
        product[left_index] = 0;
    for(left_index = 0; left_index < 16; ++left_index)
        for(right_index = 0; right_index < 16; ++right_index)
            product[left_index + right_index] +=
                left[left_index] * right[right_index];
    for(left_index = 0; left_index < 15; ++left_index)
        product[left_index] += 38 * product[left_index + 16];
    for(left_index = 0; left_index < 16; ++left_index)
        output[left_index] = product[left_index];
    field_carry(output);
    field_carry(output);
}

static void field_square(field_element output, const field_element input)
{
    field_multiply(output, input, input);
}

static void field_inverse(field_element output, const field_element input)
{
    field_element value;
    int exponent;

    field_copy(value, input);
    for(exponent = 253; exponent >= 0; --exponent) {
        field_square(value, value);
        if(exponent != 2 && exponent != 4)
            field_multiply(value, value, input);
    }
    field_copy(output, value);
}

static void field_power2523(field_element output,
                            const field_element input)
{
    field_element value;
    int exponent;

    field_copy(value, input);
    for(exponent = 250; exponent >= 0; --exponent) {
        field_square(value, value);
        if(exponent != 1)
            field_multiply(value, value, input);
    }
    field_copy(output, value);
}

static void point_add(field_element point[4],
                      field_element other[4])
{
    field_element a;
    field_element b;
    field_element c;
    field_element d;
    field_element temporary;
    field_element e;
    field_element f;
    field_element g;
    field_element h;

    field_subtract(a, point[1], point[0]);
    field_subtract(temporary, other[1], other[0]);
    field_multiply(a, a, temporary);
    field_add(b, point[0], point[1]);
    field_add(temporary, other[0], other[1]);
    field_multiply(b, b, temporary);
    field_multiply(c, point[3], other[3]);
    field_multiply(c, c, curve_d_times_two);
    field_multiply(d, point[2], other[2]);
    field_add(d, d, d);
    field_subtract(e, b, a);
    field_subtract(f, d, c);
    field_add(g, d, c);
    field_add(h, b, a);
    field_multiply(point[0], e, f);
    field_multiply(point[1], h, g);
    field_multiply(point[2], g, f);
    field_multiply(point[3], e, h);
}

static void point_swap(field_element left[4], field_element right[4],
                       uint8_t select)
{
    int index;

    for(index = 0; index < 4; ++index)
        field_select(left[index], right[index], select);
}

static void point_pack(uint8_t output[32], field_element point[4])
{
    field_element x;
    field_element y;
    field_element inverse_z;

    field_inverse(inverse_z, point[2]);
    field_multiply(x, point[0], inverse_z);
    field_multiply(y, point[1], inverse_z);
    field_pack(output, y);
    output[31] ^= (uint8_t)(field_parity(x) << 7);
}

static void point_copy(field_element destination[4],
                       field_element source[4])
{
    int index;

    for(index = 0; index < 4; ++index)
        field_copy(destination[index], source[index]);
}

static bool point_has_small_order(field_element point[4])
{
    static const uint8_t identity[32] = { 1 };
    field_element multiplied[4];
    uint8_t encoded[32];
    int doubling;

    point_copy(multiplied, point);
    for(doubling = 0; doubling < 3; ++doubling)
        point_add(multiplied, multiplied);
    point_pack(encoded, multiplied);
    return constant_compare32(encoded, identity) == 0;
}

static void point_scalar_multiply(field_element output[4],
                                  field_element point[4],
                                  const uint8_t scalar[32])
{
    int bit;

    field_copy(output[0], field_zero);
    field_copy(output[1], field_one);
    field_copy(output[2], field_one);
    field_copy(output[3], field_zero);
    for(bit = 255; bit >= 0; --bit) {
        uint8_t selected =
            (uint8_t)((scalar[bit / 8] >> (bit & 7)) & 1u);
        point_swap(output, point, selected);
        point_add(point, output);
        point_add(output, output);
        point_swap(output, point, selected);
    }
}

static void point_scalar_base(field_element output[4],
                              const uint8_t scalar[32])
{
    field_element base[4];

    field_copy(base[0], base_x);
    field_copy(base[1], base_y);
    field_copy(base[2], field_one);
    field_multiply(base[3], base_x, base_y);
    point_scalar_multiply(output, base, scalar);
}

static int point_unpack_negative(field_element output[4],
                                 const uint8_t encoded[32])
{
    field_element temporary;
    field_element check;
    field_element numerator;
    field_element denominator;
    field_element denominator2;
    field_element denominator4;
    field_element denominator6;

    field_copy(output[2], field_one);
    field_unpack(output[1], encoded);
    field_square(numerator, output[1]);
    field_multiply(denominator, numerator, curve_d);
    field_subtract(numerator, numerator, output[2]);
    field_add(denominator, output[2], denominator);
    field_square(denominator2, denominator);
    field_square(denominator4, denominator2);
    field_multiply(denominator6, denominator4, denominator2);
    field_multiply(temporary, denominator6, numerator);
    field_multiply(temporary, temporary, denominator);
    field_power2523(temporary, temporary);
    field_multiply(temporary, temporary, numerator);
    field_multiply(temporary, temporary, denominator);
    field_multiply(temporary, temporary, denominator);
    field_multiply(output[0], temporary, denominator);
    field_square(check, output[0]);
    field_multiply(check, check, denominator);
    if(field_not_equal(check, numerator))
        field_multiply(output[0], output[0], square_root_minus_one);
    field_square(check, output[0]);
    field_multiply(check, check, denominator);
    if(field_not_equal(check, numerator))
        return -1;
    if(field_parity(output[0]) == (encoded[31] >> 7))
        field_subtract(output[0], field_zero, output[0]);
    field_multiply(output[3], output[0], output[1]);
    return 0;
}

static void scalar_reduce_mod_order(uint8_t output[32],
                                    int64_t value[64])
{
    int64_t carry;
    int64_t index;
    int64_t other;

    for(index = 63; index >= 32; --index) {
        carry = 0;
        for(other = index - 32; other < index - 12; ++other) {
            value[other] +=
                carry - 16 * value[index] *
                scalar_order[other - (index - 32)];
            carry = (value[other] + 128) >> 8;
            value[other] -= carry * 256;
        }
        value[other] += carry;
        value[index] = 0;
    }
    carry = 0;
    for(other = 0; other < 32; ++other) {
        value[other] +=
            carry - (value[31] >> 4) * scalar_order[other];
        carry = value[other] >> 8;
        value[other] &= 255;
    }
    for(other = 0; other < 32; ++other)
        value[other] -= carry * scalar_order[other];
    for(index = 0; index < 32; ++index) {
        value[index + 1] += value[index] >> 8;
        output[index] = (uint8_t)(value[index] & 255);
    }
}

static void scalar_reduce(uint8_t scalar[64])
{
    int64_t expanded[64];
    int index;

    for(index = 0; index < 64; ++index) {
        expanded[index] = scalar[index];
        scalar[index] = 0;
    }
    scalar_reduce_mod_order(scalar, expanded);
}

static bool scalar_is_canonical(const uint8_t scalar[32])
{
    int index;

    for(index = 31; index >= 0; --index) {
        if(scalar[index] < scalar_order[index])
            return true;
        if(scalar[index] > scalar_order[index])
            return false;
    }
    return false;
}

static bool encoded_y_is_canonical(const uint8_t encoded[32])
{
    int index;

    for(index = 31; index >= 0; --index) {
        uint8_t value = encoded[index];
        uint8_t prime = index == 0 ? 0xedu :
                        index == 31 ? 0x7fu : 0xffu;

        if(index == 31)
            value &= 0x7fu;
        if(value < prime)
            return true;
        if(value > prime)
            return false;
    }
    return false;
}

bool crazypod_ed25519_verify(
    const uint8_t signature[CRAZYPOD_ED25519_SIGNATURE_SIZE],
    const uint8_t *message, size_t size,
    const uint8_t public_key[CRAZYPOD_ED25519_PUBLIC_KEY_SIZE])
{
    struct sha512_context hash;
    uint8_t challenge[64];
    uint8_t encoded_check[32];
    field_element point[4];
    field_element public_point[4];
    field_element signature_point[4];

    if(signature == NULL || public_key == NULL ||
       (message == NULL && size != 0) ||
       !scalar_is_canonical(signature + 32) ||
       !encoded_y_is_canonical(signature) ||
       !encoded_y_is_canonical(public_key) ||
       point_unpack_negative(public_point, public_key) != 0 ||
       point_unpack_negative(signature_point, signature) != 0 ||
       point_has_small_order(public_point) ||
       point_has_small_order(signature_point))
        return false;

    sha512_init(&hash);
    sha512_update(&hash, signature, 32);
    sha512_update(&hash, public_key, 32);
    if(size != 0)
        sha512_update(&hash, message, size);
    sha512_final(&hash, challenge);
    scalar_reduce(challenge);
    point_scalar_multiply(point, public_point, challenge);
    point_scalar_base(public_point, signature + 32);
    point_add(point, public_point);
    point_pack(encoded_check, point);
    return constant_compare32(signature, encoded_check) == 0;
}
