#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "crazypod_miniapp_manifest.h"

#if CONFIG_BINFMT == BINFMT_ROCK
#define EXPECTED_TARGET "ipod6g"
#define EXPECTED_BINARY "app.arm"
#else
#define EXPECTED_TARGET "simulator"
#define EXPECTED_BINARY "app.dylib"
#endif

#define ICON_NAME "icon.bmp"
#define RESOURCES_NAME "resources.bin"

static bool copy_text(
    char *destination, size_t capacity,
    const char *source, size_t length)
{
    if(destination == NULL || source == NULL ||
       capacity == 0 || length >= capacity)
        return false;
    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}

static bool valid_id(const char *id)
{
    size_t index;
    size_t length;

    if(id == NULL)
        return false;
    length = strlen(id);
    if(length == 0 || length >= CRAZYPOD_MINIAPP_ID_SIZE ||
       id[0] < 'a' || id[0] > 'z')
        return false;
    for(index = 1; index < length; ++index) {
        char value = id[index];
        if(!((value >= 'a' && value <= 'z') ||
             (value >= '0' && value <= '9') ||
             value == '_' || value == '-'))
            return false;
    }
    return true;
}

bool crazypod_miniapp_text_valid(const char *text, bool allow_space)
{
    const uint8_t *cursor = (const uint8_t *)text;

    if(text == NULL || text[0] == '\0')
        return false;
    while(*cursor != 0) {
        uint8_t value = *cursor++;
        unsigned int continuation;
        uint32_t codepoint;
        uint32_t minimum;

        if(value < 0x80) {
            if(value < 0x20 || (!allow_space && value == ' '))
                return false;
            continue;
        }
        if((value & 0xe0) == 0xc0) {
            continuation = 1;
            codepoint = value & 0x1f;
            minimum = 0x80u;
        }
        else if((value & 0xf0) == 0xe0) {
            continuation = 2;
            codepoint = value & 0x0f;
            minimum = 0x800u;
        }
        else if((value & 0xf8) == 0xf0) {
            continuation = 3;
            codepoint = value & 0x07;
            minimum = 0x10000u;
        }
        else {
            return false;
        }
        while(continuation-- > 0) {
            value = *cursor++;
            if((value & 0xc0) != 0x80)
                return false;
            codepoint = (codepoint << 6) | (value & 0x3f);
        }
        if(codepoint < minimum || codepoint > 0x10ffffu ||
           (codepoint >= 0xd800u && codepoint <= 0xdfffu))
            return false;
    }
    return true;
}

static bool parse_uint32(
    const char *text, size_t length, uint32_t *value)
{
    uint32_t result = 0;
    size_t index;

    if(length == 0)
        return false;
    for(index = 0; index < length; ++index) {
        unsigned int digit;
        if(text[index] < '0' || text[index] > '9')
            return false;
        digit = (unsigned int)(text[index] - '0');
        if(result > (UINT32_MAX - digit) / 10u)
            return false;
        result = result * 10u + digit;
    }
    *value = result;
    return true;
}

static bool parse_hex(
    const char *text, size_t length,
    uint8_t *bytes, size_t byte_count)
{
    size_t index;

    if(length != byte_count * 2)
        return false;
    for(index = 0; index < byte_count; ++index) {
        unsigned int pair = 0;
        int nibble;
        int part;
        for(part = 0; part < 2; ++part) {
            char value = text[index * 2 + part];
            if(value >= '0' && value <= '9')
                nibble = value - '0';
            else if(value >= 'a' && value <= 'f')
                nibble = value - 'a' + 10;
            else if(value >= 'A' && value <= 'F')
                nibble = value - 'A' + 10;
            else
                return false;
            pair = (pair << 4) | (unsigned int)nibble;
        }
        bytes[index] = (uint8_t)pair;
    }
    return true;
}

static bool parse_rgb(
    const char *text, size_t length, uint32_t *value)
{
    uint8_t bytes[3];

    if(!parse_hex(text, length, bytes, sizeof(bytes)))
        return false;
    *value = ((uint32_t)bytes[0] << 16) |
             ((uint32_t)bytes[1] << 8) |
             bytes[2];
    return true;
}

static int parse_field(
    const char *key, size_t key_length,
    const char *value, size_t value_length,
    struct crazypod_miniapp_metadata *metadata,
    char *resources, uint32_t *format, uint32_t *bit)
{
#define KEY(name) \
    (key_length == sizeof(name) - 1 && \
     memcmp(key, name, sizeof(name) - 1) == 0)
#define COPY(member) \
    copy_text(metadata->member, sizeof(metadata->member), value, value_length)

    if(KEY("format")) {
        *bit = 1u << 0;
        return parse_uint32(value, value_length, format);
    }
    if(KEY("id")) {
        *bit = 1u << 1;
        return COPY(id);
    }
    if(KEY("name")) {
        *bit = 1u << 2;
        return COPY(name);
    }
    if(KEY("version")) {
        *bit = 1u << 3;
        return COPY(version);
    }
    if(KEY("version_code")) {
        *bit = 1u << 4;
        return parse_uint32(
            value, value_length, &metadata->version_code);
    }
    if(KEY("abi")) {
        *bit = 1u << 5;
        return parse_uint32(
            value, value_length, &metadata->abi_version);
    }
    if(KEY("target")) {
        *bit = 1u << 6;
        return COPY(target);
    }
    if(KEY("binary")) {
        *bit = 1u << 7;
        return COPY(binary);
    }
    if(KEY("binary_sha256")) {
        *bit = 1u << 8;
        return parse_hex(value, value_length,
                         metadata->binary_sha256, 32);
    }
    if(KEY("icon")) {
        *bit = 1u << 9;
        return COPY(icon);
    }
    if(KEY("icon_sha256")) {
        *bit = 1u << 10;
        return parse_hex(value, value_length,
                         metadata->icon_sha256, 32);
    }
    if(KEY("symbol")) {
        *bit = 1u << 11;
        return COPY(symbol);
    }
    if(KEY("accent")) {
        *bit = 1u << 12;
        return parse_rgb(value, value_length, &metadata->accent_rgb);
    }
    if(KEY("summary")) {
        *bit = 1u << 13;
        return COPY(summary);
    }
    if(KEY("resources")) {
        *bit = 1u << 14;
        return copy_text(resources, CRAZYPOD_MINIAPP_RESOURCES_SIZE,
                         value, value_length);
    }
    if(KEY("resources_sha256")) {
        *bit = 1u << 15;
        return parse_hex(value, value_length,
                         metadata->resources_sha256, 32);
    }
    return false;
#undef COPY
#undef KEY
}

int crazypod_miniapp_manifest_parse(
    char *buffer, size_t size,
    struct crazypod_miniapp_metadata *metadata)
{
    char *cursor = buffer;
    char *end = buffer + size;
    char resources[CRAZYPOD_MINIAPP_RESOURCES_SIZE] = "";
    uint32_t seen = 0;
    uint32_t format = 0;

    if(buffer == NULL || metadata == NULL || size == 0 ||
       size > CRAZYPOD_MINIAPP_MANIFEST_MAX ||
       memchr(buffer, '\0', size) != NULL)
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
    memset(metadata, 0, sizeof(*metadata));
    buffer[size] = '\0';

    while(cursor < end) {
        char *line = cursor;
        char *equals;
        size_t line_length;
        size_t key_length;
        size_t value_length;
        uint32_t bit = 0;

        while(cursor < end && *cursor != '\n')
            ++cursor;
        line_length = (size_t)(cursor - line);
        if(cursor < end)
            ++cursor;
        if(line_length > 0 && line[line_length - 1] == '\r')
            --line_length;
        equals = line_length > 0 ? memchr(line, '=', line_length) : NULL;
        if(equals == NULL || equals == line)
            return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        key_length = (size_t)(equals - line);
        value_length = line_length - key_length - 1;
        if(value_length == 0 ||
           !parse_field(line, key_length, equals + 1, value_length,
                        metadata, resources, &format, &bit) ||
           (seen & bit) != 0)
            return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        seen |= bit;
    }

    if((format == 1u && seen != 0x3fffu) ||
       (format == 2u && seen != 0xffffu) ||
       (format != 1u && format != 2u) ||
       metadata->version_code == 0 ||
       !valid_id(metadata->id) ||
       !crazypod_miniapp_text_valid(metadata->name, true) ||
       !crazypod_miniapp_text_valid(metadata->version, false) ||
       !crazypod_miniapp_text_valid(metadata->symbol, true) ||
       !crazypod_miniapp_text_valid(metadata->summary, true))
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;

    metadata->package_format = format;
    if(metadata->abi_version != CP_MINIAPP_ABI_VERSION)
        return CRAZYPOD_MINIAPP_ERROR_ABI;
    if(strcmp(metadata->target, EXPECTED_TARGET) != 0 ||
       strcmp(metadata->binary, EXPECTED_BINARY) != 0 ||
       strcmp(metadata->icon, ICON_NAME) != 0 ||
       (format == 2u && strcmp(resources, RESOURCES_NAME) != 0))
        return CRAZYPOD_MINIAPP_ERROR_PLATFORM;
    return CRAZYPOD_MINIAPP_OK;
}

#endif
