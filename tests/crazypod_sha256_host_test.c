#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "crazypod_sha256.h"

static void decode(const char *hex, uint8_t output[32])
{
    unsigned index;
    for(index = 0; index < 32; ++index) {
        unsigned high = hex[index * 2] <= '9'
            ? (unsigned)(hex[index * 2] - '0')
            : (unsigned)(hex[index * 2] - 'a' + 10);
        unsigned low = hex[index * 2 + 1] <= '9'
            ? (unsigned)(hex[index * 2 + 1] - '0')
            : (unsigned)(hex[index * 2 + 1] - 'a' + 10);
        output[index] = (uint8_t)((high << 4) | low);
    }
}

int main(void)
{
    static const char message[] =
        "The quick brown fox jumps over the lazy dog";
    struct crazypod_sha256 sha;
    struct crazypod_hmac_sha256 hmac;
    uint8_t actual[32];
    uint8_t expected[32];

    crazypod_sha256_init(&sha);
    crazypod_sha256_update(&sha, "abc", 3);
    crazypod_sha256_final(&sha, actual);
    decode("ba7816bf8f01cfea414140de5dae2223"
           "b00361a396177a9cb410ff61f20015ad", expected);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);

    crazypod_hmac_sha256_init(&hmac, "key", 3);
    crazypod_hmac_sha256_update(&hmac, message, sizeof(message) - 1u);
    crazypod_hmac_sha256_final(&hmac, actual);
    decode("f7bc83f430538424b13298e6aa6fb143"
           "ef4d59a14946175997479dbc2d1a3cd8", expected);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    return 0;
}
