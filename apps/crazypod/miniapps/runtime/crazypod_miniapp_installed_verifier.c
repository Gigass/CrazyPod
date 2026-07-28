#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "file.h"

#include "../../crazypod_crypto.h"
#include "../installer/crazypod_cpk_reader.h"
#include "../installer/crazypod_miniapp_install_record.h"
#include "../installer/crazypod_miniapp_manifest.h"
#include "crazypod_miniapp_installed_verifier.h"

#define IO_BUFFER_SIZE 1024u
#define RESOURCE_HEADER_SIZE 16u
#define RESOURCES_MAX (512u * 1024u)

static uint8_t manifest[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];

static bool read_exact(int fd, void *buffer, size_t size)
{
    uint8_t *cursor = buffer;
    while(size > 0) {
        ssize_t count = read(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool make_path(
    char *path, size_t capacity,
    const char *directory, const char *name)
{
    int length = snprintf(path, capacity, "%s/%s", directory, name);
    return length >= 0 && (size_t)length < capacity;
}

static int verify_file(
    const char *path, uint32_t expected_size,
    uint32_t expected_crc, const uint8_t expected_digest[32])
{
    struct crazypod_sha256_context context;
    uint8_t buffer[IO_BUFFER_SIZE];
    uint8_t digest[32];
    uint32_t remaining = expected_size;
    uint32_t crc = 0xffffffffu;
    int fd = open(path, O_RDONLY);

    if(fd < 0 || filesize(fd) != (off_t)expected_size) {
        if(fd >= 0) close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    crazypod_sha256_init(&context);
    while(remaining > 0) {
        uint32_t amount = remaining > sizeof(buffer)
            ? (uint32_t)sizeof(buffer) : remaining;
        if(!read_exact(fd, buffer, amount)) {
            close(fd);
            return CRAZYPOD_MINIAPP_ERROR_IO;
        }
        crc = crc_32r(buffer, amount, crc);
        crazypod_sha256_update(&context, buffer, amount);
        remaining -= amount;
    }
    close(fd);
    crazypod_sha256_final(&context, digest);
    if(~crc != expected_crc)
        return CRAZYPOD_MINIAPP_ERROR_CRC;
    return memcmp(digest, expected_digest, sizeof(digest)) == 0
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
}

static int verify_resources(
    const char *path, const uint8_t expected_digest[32])
{
    struct crazypod_sha256_context context;
    uint8_t buffer[IO_BUFFER_SIZE];
    uint8_t digest[32];
    int fd = open(path, O_RDONLY);
    off_t size;
    off_t remaining;

    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    if(size < (off_t)RESOURCE_HEADER_SIZE ||
       size > (off_t)RESOURCES_MAX) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    }
    crazypod_sha256_init(&context);
    remaining = size;
    while(remaining > 0) {
        size_t amount = remaining > (off_t)sizeof(buffer)
            ? sizeof(buffer) : (size_t)remaining;
        if(!read_exact(fd, buffer, amount)) {
            close(fd);
            return CRAZYPOD_MINIAPP_ERROR_IO;
        }
        crazypod_sha256_update(&context, buffer, amount);
        remaining -= (off_t)amount;
    }
    close(fd);
    crazypod_sha256_final(&context, digest);
    return memcmp(digest, expected_digest, sizeof(digest)) == 0
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
}

int crazypod_miniapp_installed_verify(
    const struct crazypod_miniapp_metadata *metadata)
{
    struct install_record record;
    uint8_t signature[CRAZYPOD_MINIAPP_SIGNATURE_SIZE];
    char path[MAX_PATH];
    int fd;
    int result;

    if(!crazypod_miniapp_install_record_read(
           metadata->install_path, &record))
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(record.files[CPK_MANIFEST].size >
           CRAZYPOD_MINIAPP_MANIFEST_MAX ||
       !make_path(path, sizeof(path), metadata->install_path,
                  "manifest.ini"))
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    fd = open(path, O_RDONLY);
    if(fd < 0 ||
       filesize(fd) != (off_t)record.files[CPK_MANIFEST].size ||
       !read_exact(fd, manifest, record.files[CPK_MANIFEST].size)) {
        if(fd >= 0) close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    close(fd);
    if(~crc_32r(manifest, record.files[CPK_MANIFEST].size,
                0xffffffffu) != record.files[CPK_MANIFEST].crc32)
        return CRAZYPOD_MINIAPP_ERROR_CRC;
    if(!make_path(path, sizeof(path), metadata->install_path,
                  "signature.ed25519"))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    fd = open(path, O_RDONLY);
    if(fd < 0 || filesize(fd) != CRAZYPOD_MINIAPP_SIGNATURE_SIZE ||
       !read_exact(fd, signature, sizeof(signature))) {
        if(fd >= 0) close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    close(fd);
    if(!crazypod_ed25519_verify(
           signature, manifest, record.files[CPK_MANIFEST].size,
           crazypod_miniapp_development_public_key))
        return CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
    result = verify_file(
        metadata->binary_path, record.files[CPK_BINARY].size,
        record.files[CPK_BINARY].crc32, metadata->binary_sha256);
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    result = verify_file(
        metadata->icon_path, record.files[CPK_ICON].size,
        record.files[CPK_ICON].crc32, metadata->icon_sha256);
    if(result == CRAZYPOD_MINIAPP_OK &&
       metadata->package_format == 2u)
        result = verify_resources(
            metadata->resources_path, metadata->resources_sha256);
    return result;
}

#endif
