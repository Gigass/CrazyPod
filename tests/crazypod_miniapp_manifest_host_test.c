#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "crazypod_miniapp_manifest.h"
#include "crazypod_miniapp_catalog.h"

static const char valid_manifest[] =
    "{"
    "\"format\":5,"
    "\"id\":\"game2048\","
    "\"name\":\"2048\","
    "\"version\":\"5.0.0\","
    "\"versionCode\":50000,"
    "\"runtime\":\"native-aot\","
    "\"abiMajor\":1,"
    "\"abiMinor\":1,"
    "\"reactProfile\":1,"
    "\"target\":\"simulator\","
    "\"entry\":\"app.dylib\","
    "\"icon\":\"icon.bin\","
    "\"symbol\":\"2048\","
    "\"summary\":\"Native 2048\","
    "\"accent\":\"#26cff5\""
    "}";

static int parse(
    const char *text, struct crazypod_miniapp_metadata *metadata)
{
    char buffer[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    size_t size = strlen(text);

    assert(size <= CRAZYPOD_MINIAPP_MANIFEST_MAX);
    memcpy(buffer, text, size);
    return crazypod_miniapp_manifest_parse(buffer, size, metadata);
}

int main(void)
{
    struct crazypod_miniapp_metadata metadata;
    struct crazypod_miniapp_metadata valid;
    struct crazypod_miniapp_metadata second;
    char duplicate[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    char bad_target[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    const char unicode_manifest[] =
        "{"
        "\"format\":5,"
        "\"id\":\"unicode\","
        "\"name\":\"\\u8ba1\\u65f6\\ud83d\\ude80\","
        "\"version\":\"1.0.0\","
        "\"versionCode\":1,"
        "\"runtime\":\"native-aot\","
        "\"abiMajor\":1,"
        "\"abiMinor\":0,"
        "\"reactProfile\":1,"
        "\"target\":\"simulator\","
        "\"entry\":\"app.dylib\","
        "\"icon\":\"icon.bin\","
        "\"symbol\":\"U\","
        "\"summary\":\"Unicode\","
        "\"accent\":\"#ffffff\""
        "}";

    assert(parse(valid_manifest, &metadata) == CRAZYPOD_MINIAPP_OK);
    valid = metadata;
    assert(strcmp(metadata.id, "game2048") == 0);
    assert(metadata.package_format == CP_NATIVE_PACKAGE_FORMAT);
    assert(metadata.abi_version == CP_NATIVE_ABI_MAJOR);
    assert(metadata.abi_minor == CP_NATIVE_ABI_MINOR);
    assert(metadata.react_profile == CP_NATIVE_REACT_PROFILE);
    assert(strcmp(metadata.runtime, "native-aot") == 0);
    assert(strcmp(metadata.target, "simulator") == 0);
    assert(strcmp(metadata.entry, "app.dylib") == 0);
    assert(metadata.accent_rgb == 0x26cff5u);

    assert(parse(unicode_manifest, &metadata) == CRAZYPOD_MINIAPP_OK);
    assert(strcmp(metadata.name, "计时🚀") == 0);

    snprintf(
        duplicate, sizeof(duplicate),
        "{\"format\":5,\"format\":5,"
        "\"id\":\"x\",\"name\":\"X\",\"version\":\"1\","
        "\"versionCode\":1,\"runtime\":\"native-aot\","
        "\"abiMajor\":1,\"abiMinor\":0,\"reactProfile\":1,"
        "\"target\":\"simulator\",\"entry\":\"app.dylib\","
        "\"icon\":\"icon.bin\",\"symbol\":\"X\","
        "\"summary\":\"X\",\"accent\":\"#000000\"}");
    assert(parse(duplicate, &metadata) ==
           CRAZYPOD_MINIAPP_ERROR_MANIFEST);

    snprintf(bad_target, sizeof(bad_target), "%s", valid_manifest);
    {
        char *target = strstr(bad_target, "simulator");
        assert(target != NULL);
        memcpy(target, "ipod6gxxx", 9);
    }
    assert(parse(bad_target, &metadata) ==
           CRAZYPOD_MINIAPP_ERROR_PLATFORM);

    assert(!crazypod_miniapp_text_valid("bad\x01text", true));
    assert(!crazypod_miniapp_text_valid("two words", false));
    assert(crazypod_miniapp_text_valid("计时器", true));

    metadata = valid;
    second = valid;
    snprintf(second.id, sizeof(second.id), "alarm");
    crazypod_miniapp_catalog_reset();
    assert(crazypod_miniapp_catalog_add(&metadata));
    assert(crazypod_miniapp_catalog_add(&second));
    crazypod_miniapp_catalog_sort();
    assert(crazypod_miniapp_catalog_count() == 2);
    assert(strcmp(crazypod_miniapp_catalog_get(0)->id, "alarm") == 0);
    assert(crazypod_miniapp_catalog_find("game2048") == 1);
    assert(crazypod_miniapp_catalog_get(-1) == NULL);
    return 0;
}
