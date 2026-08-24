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
    "\"abiMinor\":20,"
    "\"reactProfile\":1,"
    "\"fontSet\":\"system:400:12,system:700:24\","
    "\"target\":\"simulator\","
    "\"entry\":\"app.dylib\","
    "\"icon\":\"icon.bin\","
    "\"symbol\":\"2048\","
    "\"summary\":\"Native 2048\","
    "\"accent\":\"#26cff5\""
    "}";

static const char valid_theme_manifest[] =
    "{"
    "\"format\":5,"
    "\"kind\":\"now-playing-theme\","
    "\"id\":\"now-playing-neon\","
    "\"name\":\"Atelier Hi-Fi\","
    "\"version\":\"1.3.0\","
    "\"versionCode\":10300,"
    "\"runtime\":\"native-aot\","
    "\"abiMajor\":1,"
    "\"abiMinor\":9,"
    "\"reactProfile\":1,"
    "\"target\":\"simulator\","
    "\"entry\":\"app.dylib\","
    "\"icon\":\"icon.bin\","
    "\"symbol\":\"HF\","
    "\"summary\":\"Skeuomorphic TSX theme\","
    "\"accent\":\"#d79a55\""
    "}";

static const char owned_status_theme_manifest[] =
    "{"
    "\"format\":5,"
    "\"kind\":\"now-playing-theme\","
    "\"statusBar\":\"theme\","
    "\"artworkSourceSize\":320,"
    "\"id\":\"now-playing-owned\","
    "\"name\":\"Owned Status\","
    "\"version\":\"1.0.0\","
    "\"versionCode\":10000,"
    "\"runtime\":\"native-aot\","
    "\"abiMajor\":1,"
    "\"abiMinor\":13,"
    "\"reactProfile\":1,"
    "\"target\":\"simulator\","
    "\"entry\":\"app.dylib\","
    "\"icon\":\"icon.bin\","
    "\"symbol\":\"OS\","
    "\"summary\":\"Theme-owned status bar\","
    "\"accent\":\"#ffffff\""
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
    char old_theme[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    char missing_artwork[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    char missing_font_set[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    char rejected_abi[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
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
    assert(strcmp(metadata.font_set,
                  "system:400:12,system:700:24") == 0);
    assert(strcmp(metadata.runtime, "native-aot") == 0);
    assert(strcmp(metadata.target, "simulator") == 0);
    assert(strcmp(metadata.entry, "app.dylib") == 0);
    assert(metadata.accent_rgb == 0x26cff5u);
    assert(metadata.kind == CRAZYPOD_MINIAPP_KIND_APP);
    snprintf(rejected_abi, sizeof(rejected_abi), "%s", valid_manifest);
    {
        char *minor = strstr(rejected_abi, "\"abiMinor\":20");
        char *value;

        assert(minor != NULL);
        value = minor + strlen("\"abiMinor\":");
        value[0] = '1';
        value[1] = '4';
    }
    assert(parse(rejected_abi, &metadata) ==
           CRAZYPOD_MINIAPP_ERROR_VERSION);
    snprintf(missing_font_set, sizeof(missing_font_set), "%s",
             valid_manifest);
    {
        const char field[] =
            "\"fontSet\":\"system:400:12,system:700:24\",";
        char *start = strstr(missing_font_set, field);

        assert(start != NULL);
        memmove(start, start + strlen(field),
                strlen(start + strlen(field)) + 1u);
    }
    assert(parse(missing_font_set, &metadata) ==
           CRAZYPOD_MINIAPP_ERROR_VERSION);

    assert(parse(valid_theme_manifest, &metadata) == CRAZYPOD_MINIAPP_OK);
    assert(metadata.kind == CRAZYPOD_MINIAPP_KIND_NOW_PLAYING_THEME);
    assert(metadata.status_bar == CRAZYPOD_MINIAPP_STATUS_BAR_SYSTEM);
    assert(metadata.artwork_source_size ==
           CRAZYPOD_MINIAPP_ARTWORK_SOURCE_DEFAULT);
    assert(parse(owned_status_theme_manifest, &metadata) ==
           CRAZYPOD_MINIAPP_OK);
    assert(metadata.status_bar == CRAZYPOD_MINIAPP_STATUS_BAR_THEME);
    assert(metadata.artwork_source_size == 320);
    snprintf(missing_artwork, sizeof(missing_artwork), "%s",
             owned_status_theme_manifest);
    {
        const char field[] = "\"artworkSourceSize\":320,";
        char *start = strstr(missing_artwork, field);

        assert(start != NULL);
        memmove(start, start + strlen(field),
                strlen(start + strlen(field)) + 1u);
    }
    assert(parse(missing_artwork, &metadata) ==
           CRAZYPOD_MINIAPP_ERROR_VERSION);
    snprintf(old_theme, sizeof(old_theme), "%s", valid_theme_manifest);
    {
        char *minor = strstr(old_theme, "\"abiMinor\":9");
        assert(minor != NULL);
        minor[strlen("\"abiMinor\":")] = '3';
    }
    assert(parse(old_theme, &metadata) == CRAZYPOD_MINIAPP_ERROR_VERSION);

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
