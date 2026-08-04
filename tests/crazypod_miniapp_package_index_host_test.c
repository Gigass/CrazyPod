#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "crazypod_miniapp_package_index.h"

static void expect_lookup(
    uint8_t source, const char *name,
    uint64_t size, int64_t mtime,
    bool expected, const char *expected_id,
    uint32_t expected_version)
{
    char id[64];
    uint32_t version = 0;
    bool found = crazypod_miniapp_package_index_lookup(
        source, name, size, mtime,
        id, sizeof(id), &version);

    assert(found == expected);
    if(expected) {
        assert(strcmp(id, expected_id) == 0);
        assert(version == expected_version);
    }
}

int main(void)
{
    int fd;

    unlink(CRAZYPOD_MINIAPP_PACKAGE_INDEX_PATH);
    unlink(CRAZYPOD_MINIAPP_PACKAGE_INDEX_TEMP);

    crazypod_miniapp_package_index_begin();
    expect_lookup(
        CRAZYPOD_MINIAPP_PACKAGE_USER,
        "alpha.cpk", 100, 10, false, "", 0);
    assert(crazypod_miniapp_package_index_note(
        CRAZYPOD_MINIAPP_PACKAGE_SYSTEM,
        "alpha.cpk", 100, 10, "alpha", 1));
    assert(crazypod_miniapp_package_index_note(
        CRAZYPOD_MINIAPP_PACKAGE_USER,
        "beta.cpk", 200, 20, "beta", 2));
    assert(crazypod_miniapp_package_index_finish());

    crazypod_miniapp_package_index_begin();
    expect_lookup(
        CRAZYPOD_MINIAPP_PACKAGE_SYSTEM,
        "alpha.cpk", 100, 10, true, "alpha", 1);
    expect_lookup(
        CRAZYPOD_MINIAPP_PACKAGE_USER,
        "beta.cpk", 200, 20, true, "beta", 2);
    expect_lookup(
        CRAZYPOD_MINIAPP_PACKAGE_USER,
        "beta.cpk", 200, 21, false, "", 0);
    assert(crazypod_miniapp_package_index_note(
        CRAZYPOD_MINIAPP_PACKAGE_SYSTEM,
        "alpha.cpk", 100, 10, "alpha", 1));
    assert(crazypod_miniapp_package_index_finish());

    crazypod_miniapp_package_index_begin();
    expect_lookup(
        CRAZYPOD_MINIAPP_PACKAGE_SYSTEM,
        "alpha.cpk", 100, 10, true, "alpha", 1);
    expect_lookup(
        CRAZYPOD_MINIAPP_PACKAGE_USER,
        "beta.cpk", 200, 20, false, "", 0);

    fd = open(
        CRAZYPOD_MINIAPP_PACKAGE_INDEX_PATH,
        O_WRONLY | O_TRUNC);
    assert(fd >= 0);
    assert(write(fd, "broken", 6) == 6);
    assert(close(fd) == 0);
    crazypod_miniapp_package_index_begin();
    expect_lookup(
        CRAZYPOD_MINIAPP_PACKAGE_SYSTEM,
        "alpha.cpk", 100, 10, false, "", 0);

    unlink(CRAZYPOD_MINIAPP_PACKAGE_INDEX_PATH);
    unlink(CRAZYPOD_MINIAPP_PACKAGE_INDEX_TEMP);
    return 0;
}
