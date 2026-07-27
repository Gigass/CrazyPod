#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../apps/crazypod/crazypod_crypto.h"

static uint8_t hex_nibble(char character)
{
    if(character >= '0' && character <= '9')
        return (uint8_t)(character - '0');
    if(character >= 'a' && character <= 'f')
        return (uint8_t)(character - 'a' + 10);
    if(character >= 'A' && character <= 'F')
        return (uint8_t)(character - 'A' + 10);
    assert(!"invalid hexadecimal input");
    return 0;
}

static size_t decode_hex(const char *hex, uint8_t *output, size_t capacity)
{
    size_t length = strlen(hex);
    size_t index;

    assert((length & 1u) == 0);
    assert(length / 2u <= capacity);
    for(index = 0; index < length / 2u; ++index) {
        output[index] = (uint8_t)(
            (hex_nibble(hex[index * 2u]) << 4) |
            hex_nibble(hex[index * 2u + 1u]));
    }
    return length / 2u;
}

static void assert_digest(const uint8_t actual[32],
                          const char *expected_hex)
{
    uint8_t expected[32];

    assert(decode_hex(expected_hex, expected, sizeof(expected)) ==
           sizeof(expected));
    assert(memcmp(actual, expected, sizeof(expected)) == 0);
}

static void test_sha256_standard_vectors(void)
{
    static const char long_message[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    struct crazypod_sha256_context context;
    uint8_t digest[32];
    uint8_t thousand_a[1000];
    size_t index;

    crazypod_sha256_init(&context);
    crazypod_sha256_update(&context, NULL, 0);
    crazypod_sha256_final(&context, digest);
    assert_digest(
        digest,
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855");

    crazypod_sha256_init(&context);
    crazypod_sha256_update(&context, "a", 1);
    crazypod_sha256_update(&context, "b", 1);
    crazypod_sha256_update(&context, "c", 1);
    crazypod_sha256_final(&context, digest);
    assert_digest(
        digest,
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad");

    crazypod_sha256_init(&context);
    for(index = 0; index < sizeof(long_message) - 1u; index += 7u) {
        size_t remaining = sizeof(long_message) - 1u - index;
        size_t amount = remaining < 7u ? remaining : 7u;
        crazypod_sha256_update(&context, long_message + index, amount);
    }
    crazypod_sha256_final(&context, digest);
    assert_digest(
        digest,
        "248d6a61d20638b8e5c026930c3e6039"
        "a33ce45964ff2167f6ecedd419db06c1");

    memset(thousand_a, 'a', sizeof(thousand_a));
    crazypod_sha256_init(&context);
    for(index = 0; index < 1000; ++index)
        crazypod_sha256_update(&context, thousand_a, sizeof(thousand_a));
    crazypod_sha256_final(&context, digest);
    assert_digest(
        digest,
        "cdc76e5c9914fb9281a1c7e284d73e67"
        "f1809a48a497200e046d39ccc7112cd0");
}

static void verify_rfc8032_vector(const char *public_key_hex,
                                  const char *message_hex,
                                  const char *signature_hex)
{
    uint8_t public_key[32];
    uint8_t message[128];
    uint8_t signature[64];
    size_t message_size;

    assert(decode_hex(public_key_hex, public_key, sizeof(public_key)) ==
           sizeof(public_key));
    message_size = decode_hex(message_hex, message, sizeof(message));
    assert(decode_hex(signature_hex, signature, sizeof(signature)) ==
           sizeof(signature));
    assert(crazypod_ed25519_verify(
        signature, message_size == 0 ? NULL : message,
        message_size, public_key));
}

static void test_rfc8032_vectors(void)
{
    /* RFC 8032 section 7.1, Ed25519 tests 1, 2, and 3. */
    verify_rfc8032_vector(
        "d75a980182b10ab7d54bfed3c964073a"
        "0ee172f3daa62325af021a68f707511a",
        "",
        "e5564300c360ac729086e2cc806e828a"
        "84877f1eb8e5d974d873e06522490155"
        "5fb8821590a33bacc61e39701cf9b46b"
        "d25bf5f0595bbe24655141438e7a100b");
    verify_rfc8032_vector(
        "3d4017c3e843895a92b70aa74d1b7ebc"
        "9c982ccf2ec4968cc0cd55f12af4660c",
        "72",
        "92a009a9f0d4cab8720e820b5f642540"
        "a2b27b5416503f8fb3762223ebdb69da"
        "085ac1e43e15996e458f3613d0f11d8c"
        "387b2eaeb4302aeeb00d291612bb0c00");
    verify_rfc8032_vector(
        "fc51cd8e6218a1a38da47ed00230f058"
        "0816ed13ba3303ac5deb911548908025",
        "af82",
        "6291d657deec24024827e69c3abe01a3"
        "0ce548a284743a445e3680d7db5ac3ac"
        "18ff9b538d16f290ae67f760984dc659"
        "4a7c15e9716ed28dc027beceea1ec40a");
}

static void test_ed25519_rejects_mutation_and_noncanonical_s(void)
{
    static const uint8_t order_l[32] = {
        0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
    };
    uint8_t public_key[32];
    uint8_t message[1] = { 0x72 };
    uint8_t signature[64];
    uint8_t original_signature[64];

    decode_hex(
        "3d4017c3e843895a92b70aa74d1b7ebc"
        "9c982ccf2ec4968cc0cd55f12af4660c",
        public_key, sizeof(public_key));
    decode_hex(
        "92a009a9f0d4cab8720e820b5f642540"
        "a2b27b5416503f8fb3762223ebdb69da"
        "085ac1e43e15996e458f3613d0f11d8c"
        "387b2eaeb4302aeeb00d291612bb0c00",
        signature, sizeof(signature));
    memcpy(original_signature, signature, sizeof(signature));

    signature[0] ^= 1u;
    assert(!crazypod_ed25519_verify(
        signature, message, sizeof(message), public_key));
    memcpy(signature, original_signature, sizeof(signature));

    message[0] ^= 1u;
    assert(!crazypod_ed25519_verify(
        signature, message, sizeof(message), public_key));
    message[0] ^= 1u;

    public_key[0] ^= 1u;
    assert(!crazypod_ed25519_verify(
        signature, message, sizeof(message), public_key));
    public_key[0] ^= 1u;

    memcpy(signature + 32, order_l, sizeof(order_l));
    assert(!crazypod_ed25519_verify(
        signature, message, sizeof(message), public_key));
    ++signature[32]; /* S = L + 1 must also be rejected. */
    assert(!crazypod_ed25519_verify(
        signature, message, sizeof(message), public_key));

    memset(signature + 32, 0xff, 32);
    assert(!crazypod_ed25519_verify(
        signature, message, sizeof(message), public_key));
    assert(!crazypod_ed25519_verify(
        NULL, message, sizeof(message), public_key));
    assert(!crazypod_ed25519_verify(
        original_signature, NULL, sizeof(message), public_key));
    assert(!crazypod_ed25519_verify(
        original_signature, message, sizeof(message), NULL));
}

static void test_ed25519_rejects_small_order_points(void)
{
    uint8_t identity_public_key[32] = { 1 };
    uint8_t forged_signature[64] = { 1 };
    static const uint8_t message[] = "not signed";

    /*
     * Without subgroup rejection, A = identity, R = identity, S = 0
     * satisfies the verification equation for every message.
     */
    assert(!crazypod_ed25519_verify(
        forged_signature, message, sizeof(message) - 1u,
        identity_public_key));
}

static void test_development_key_exact_manifest_bytes(void)
{
    static const uint8_t expected_public_key[32] = {
        0x66, 0xe4, 0x00, 0x64, 0x95, 0x46, 0xfc, 0xb4,
        0xd5, 0xe3, 0x05, 0x03, 0xf9, 0x88, 0xef, 0x58,
        0xf3, 0xf4, 0x91, 0xc8, 0xee, 0xc1, 0xb7, 0x48,
        0x22, 0x14, 0x46, 0xf1, 0x70, 0x68, 0x04, 0xb3
    };
    static const uint8_t manifest[] =
        "format=1\n"
        "id=pomodoro\n"
        "name=Pomodoro\n"
        "version=1.0.0\n";
    uint8_t changed[sizeof(manifest) - 1u];
    uint8_t signature[64];

    assert(memcmp(crazypod_miniapp_development_public_key,
                  expected_public_key, sizeof(expected_public_key)) == 0);
    assert(decode_hex(
        "265a5c68f2b0021f597148d285308e6f"
        "7fcfbcdb7ae6b1e1e1f0e4ca5048be09"
        "0ae277b89a947fd259b0650c5c41555d"
        "08c2c856f799a346d6672e329e4e7602",
        signature, sizeof(signature)) == sizeof(signature));
    assert(crazypod_ed25519_verify(
        signature, manifest, sizeof(manifest) - 1u,
        crazypod_miniapp_development_public_key));

    memcpy(changed, manifest, sizeof(changed));
    changed[19] ^= 1u;
    assert(!crazypod_ed25519_verify(
        signature, changed, sizeof(changed),
        crazypod_miniapp_development_public_key));
    assert(!crazypod_ed25519_verify(
        signature, manifest, sizeof(manifest) - 2u,
        crazypod_miniapp_development_public_key));
}

static void test_ed25519_streams_long_messages(void)
{
    uint8_t message[300];
    uint8_t signature[64];
    size_t index;

    for(index = 0; index < sizeof(message); ++index)
        message[index] = (uint8_t)index;
    assert(decode_hex(
        "1765cfab1a5e4152d7ff2ab496a32f573"
        "3dbfb8b33ebdb0b29d68dbe8ce7de2f2"
        "fb4ffda46bf1a8218054e67196e926fa"
        "89251a1809dc0f97126325da7a2040e",
        signature, sizeof(signature)) == sizeof(signature));
    assert(crazypod_ed25519_verify(
        signature, message, sizeof(message),
        crazypod_miniapp_development_public_key));
    message[sizeof(message) - 1u] ^= 1u;
    assert(!crazypod_ed25519_verify(
        signature, message, sizeof(message),
        crazypod_miniapp_development_public_key));
}

int main(void)
{
    test_sha256_standard_vectors();
    test_rfc8032_vectors();
    test_ed25519_rejects_mutation_and_noncanonical_s();
    test_ed25519_rejects_small_order_points();
    test_development_key_exact_manifest_bytes();
    test_ed25519_streams_long_messages();
    puts("crypto tests: ok");
    return 0;
}
