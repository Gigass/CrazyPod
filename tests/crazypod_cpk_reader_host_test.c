#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "crazypod_cpk_reader.h"
#include "crazypod_cpk_verifier.h"
#include "crazypod_crypto.h"
#include "crazypod_miniapps.h"

const uint8_t crazypod_miniapp_development_public_key[32] = { 0 };

void crazypod_sha256_init(struct crazypod_sha256_context *context)
{
    memset(context, 0, sizeof(*context));
}

void crazypod_sha256_update(
    struct crazypod_sha256_context *context,
    const void *data, size_t size)
{
    (void)context; (void)data; (void)size;
}

void crazypod_sha256_final(
    struct crazypod_sha256_context *context, uint8_t digest[32])
{
    (void)context;
    memset(digest, 0, 32);
}

bool crazypod_ed25519_verify(
    const uint8_t signature[64], const uint8_t *message,
    size_t size, const uint8_t public_key[32])
{
    (void)signature; (void)message; (void)size; (void)public_key;
    return false;
}

int main(int argc, char **argv)
{
    struct cpk_reader reader;
    char value;
    char extracted[512];
    int fd;

    assert(argc == 2);
    assert(crazypod_cpk_open(argv[1], &reader) ==
           CRAZYPOD_MINIAPP_OK);
    assert(reader.entry_count == MINIAPP_CPK_V1_ENTRIES);
    assert(crazypod_cpk_validate_local_headers(&reader) ==
           CRAZYPOD_MINIAPP_OK);
    assert(reader.entries[CPK_MANIFEST].size == 1);
    assert(reader.entries[CPK_ICON].size == 102454);
    assert(crazypod_cpk_read_entry(
        &reader, CPK_MANIFEST, &value, 1) == CRAZYPOD_MINIAPP_OK);
    assert(value == 'x');
    snprintf(extracted, sizeof(extracted), "%s.extracted", argv[1]);
    assert(crazypod_cpk_extract_entry(
        &reader, CPK_BINARY, extracted) == CRAZYPOD_MINIAPP_OK);
    fd = open(extracted, O_RDONLY);
    assert(fd >= 0 && read(fd, &value, 1) == 1 && value == 'x');
    close(fd);
    unlink(extracted);
    assert(!crazypod_cpk_icon_valid(&reader));
    crazypod_cpk_close(&reader);
    return 0;
}
