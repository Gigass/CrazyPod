#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "crazypod_miniapp_manifest.h"

#define ICON_NAME "icon.bin"
#define COMMON_REQUIRED_FIELDS \
    ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | \
     (1u << 4) | (1u << 5) | (1u << 7) | (1u << 9) | \
     (1u << 10) | (1u << 11))
#define CPK5_REQUIRED_FIELDS \
    (COMMON_REQUIRED_FIELDS | (1u << 12) | (1u << 13) | \
     (1u << 14) | (1u << 15) | (1u << 16))
#define CPK5_KIND_FIELD (1u << 17)
#define CPK5_STATUS_BAR_FIELD (1u << 18)
#define CPK5_ARTWORK_SOURCE_SIZE_FIELD (1u << 19)
#define CPK5_FONT_SET_FIELD (1u << 20)
#define CPK5_PERMISSIONS_FIELD (1u << 21)
#define CPK5_SIGNING_KEY_FIELD (1u << 22)
#define CPK5_SIGNATURE_FIELD (1u << 23)

#if CONFIG_BINFMT == BINFMT_ROCK
#define NATIVE_TARGET "ipod6g"
#define NATIVE_ENTRY "app.arm"
#else
#define NATIVE_TARGET "simulator"
#define NATIVE_ENTRY "app.dylib"
#endif

struct json_reader {
    const char *cursor;
    const char *end;
};

static void skip_space(struct json_reader *reader)
{
    while(reader->cursor < reader->end &&
          (*reader->cursor == ' ' || *reader->cursor == '\t' ||
           *reader->cursor == '\r' || *reader->cursor == '\n'))
        reader->cursor++;
}

static bool take(struct json_reader *reader, char expected)
{
    skip_space(reader);
    if(reader->cursor >= reader->end ||
       *reader->cursor != expected)
        return false;
    reader->cursor++;
    return true;
}

static int hex_digit(char value)
{
    if(value >= '0' && value <= '9')
        return value - '0';
    if(value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if(value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

static bool read_hex4(
    struct json_reader *reader, uint32_t *value)
{
    uint32_t result = 0;
    int index;

    if(reader->end - reader->cursor < 4)
        return false;
    for(index = 0; index < 4; ++index) {
        int digit = hex_digit(reader->cursor[index]);
        if(digit < 0)
            return false;
        result = (result << 4) | (uint32_t)digit;
    }
    reader->cursor += 4;
    *value = result;
    return true;
}

static bool append_codepoint(
    char *output, size_t capacity, size_t *length,
    uint32_t codepoint)
{
    uint8_t encoded[4];
    size_t count;

    if(codepoint == 0 || codepoint > 0x10ffffu ||
       (codepoint >= 0xd800u && codepoint <= 0xdfffu))
        return false;
    if(codepoint < 0x80u) {
        encoded[0] = (uint8_t)codepoint;
        count = 1;
    }
    else if(codepoint < 0x800u) {
        encoded[0] = 0xc0u | (uint8_t)(codepoint >> 6);
        encoded[1] = 0x80u | (uint8_t)(codepoint & 0x3fu);
        count = 2;
    }
    else if(codepoint < 0x10000u) {
        encoded[0] = 0xe0u | (uint8_t)(codepoint >> 12);
        encoded[1] = 0x80u | (uint8_t)((codepoint >> 6) & 0x3fu);
        encoded[2] = 0x80u | (uint8_t)(codepoint & 0x3fu);
        count = 3;
    }
    else {
        encoded[0] = 0xf0u | (uint8_t)(codepoint >> 18);
        encoded[1] = 0x80u | (uint8_t)((codepoint >> 12) & 0x3fu);
        encoded[2] = 0x80u | (uint8_t)((codepoint >> 6) & 0x3fu);
        encoded[3] = 0x80u | (uint8_t)(codepoint & 0x3fu);
        count = 4;
    }
    if(*length + count >= capacity)
        return false;
    memcpy(output + *length, encoded, count);
    *length += count;
    return true;
}

static bool read_string(
    struct json_reader *reader, char *output, size_t capacity)
{
    size_t length = 0;

    skip_space(reader);
    if(capacity == 0 || reader->cursor >= reader->end ||
       *reader->cursor++ != '"')
        return false;
    while(reader->cursor < reader->end) {
        uint8_t value = (uint8_t)*reader->cursor++;

        if(value == '"') {
            output[length] = '\0';
            return true;
        }
        if(value < 0x20u)
            return false;
        if(value != '\\') {
            if(length + 1 >= capacity)
                return false;
            output[length++] = (char)value;
            continue;
        }
        if(reader->cursor >= reader->end)
            return false;
        value = (uint8_t)*reader->cursor++;
        if(value == '"' || value == '\\' || value == '/') {
            if(length + 1 >= capacity)
                return false;
            output[length++] = (char)value;
        }
        else if(value == 'b' || value == 'f' ||
                value == 'n' || value == 'r' || value == 't') {
            char decoded = value == 'b' ? '\b' :
                           value == 'f' ? '\f' :
                           value == 'n' ? '\n' :
                           value == 'r' ? '\r' : '\t';
            if(length + 1 >= capacity)
                return false;
            output[length++] = decoded;
        }
        else if(value == 'u') {
            uint32_t codepoint;

            if(!read_hex4(reader, &codepoint))
                return false;
            if(codepoint >= 0xd800u && codepoint <= 0xdbffu) {
                uint32_t low;
                if(reader->end - reader->cursor < 6 ||
                   reader->cursor[0] != '\\' ||
                   reader->cursor[1] != 'u') {
                    return false;
                }
                reader->cursor += 2;
                if(!read_hex4(reader, &low) ||
                   low < 0xdc00u || low > 0xdfffu)
                    return false;
                codepoint = 0x10000u +
                    ((codepoint - 0xd800u) << 10) +
                    (low - 0xdc00u);
            }
            if(!append_codepoint(
                   output, capacity, &length, codepoint))
                return false;
        }
        else {
            return false;
        }
    }
    return false;
}

static bool read_uint32(
    struct json_reader *reader, uint32_t *value)
{
    uint32_t result = 0;
    const char *start;

    skip_space(reader);
    start = reader->cursor;
    if(start >= reader->end || *start < '0' || *start > '9')
        return false;
    if(*start == '0' && start + 1 < reader->end &&
       start[1] >= '0' && start[1] <= '9')
        return false;
    while(reader->cursor < reader->end &&
          *reader->cursor >= '0' && *reader->cursor <= '9') {
        unsigned int digit =
            (unsigned int)(*reader->cursor++ - '0');
        if(result > (UINT32_MAX - digit) / 10u)
            return false;
        result = result * 10u + digit;
    }
    *value = result;
    return true;
}

static bool valid_id(const char *id)
{
    size_t index;
    size_t length = strlen(id);

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

        if(value < 0x80u) {
            if(value < 0x20u || (!allow_space && value == ' '))
                return false;
            continue;
        }
        if((value & 0xe0u) == 0xc0u) {
            continuation = 1;
            codepoint = value & 0x1fu;
            minimum = 0x80u;
        }
        else if((value & 0xf0u) == 0xe0u) {
            continuation = 2;
            codepoint = value & 0x0fu;
            minimum = 0x800u;
        }
        else if((value & 0xf8u) == 0xf0u) {
            continuation = 3;
            codepoint = value & 0x07u;
            minimum = 0x10000u;
        }
        else {
            return false;
        }
        while(continuation-- > 0) {
            value = *cursor++;
            if((value & 0xc0u) != 0x80u)
                return false;
            codepoint = (codepoint << 6) | (value & 0x3fu);
        }
        if(codepoint < minimum || codepoint > 0x10ffffu ||
           (codepoint >= 0xd800u && codepoint <= 0xdfffu))
            return false;
    }
    return true;
}

static bool parse_rgb(const char *text, uint32_t *color)
{
    uint32_t result = 0;
    int index;

    if(strlen(text) != 7 || text[0] != '#')
        return false;
    for(index = 1; index < 7; ++index) {
        int digit = hex_digit(text[index]);
        if(digit < 0)
            return false;
        result = (result << 4) | (uint32_t)digit;
    }
    *color = result;
    return true;
}

static bool parse_permissions(char *text, uint32_t *permissions)
{
    char *cursor = text;
    uint32_t result = 0;

    if(text[0] == '\0')
        return false;
    while(*cursor != '\0') {
        char *separator = strchr(cursor, ',');
        uint32_t permission;

        if(separator != NULL)
            *separator = '\0';
        if(strcmp(cursor, "media-library.read") == 0)
            permission = CRAZYPOD_MINIAPP_PERMISSION_MEDIA_LIBRARY_READ;
        else if(strcmp(cursor, "user-files.read") == 0)
            permission = CRAZYPOD_MINIAPP_PERMISSION_USER_FILES_READ;
        else if(strcmp(cursor, "user-files.export") == 0)
            permission = CRAZYPOD_MINIAPP_PERMISSION_USER_FILES_EXPORT;
        else if(strcmp(cursor, "sound-effects.play") == 0)
            permission = CRAZYPOD_MINIAPP_PERMISSION_SOUND_EFFECTS_PLAY;
        else if(strcmp(cursor, "alarms.schedule") == 0)
            permission = CRAZYPOD_MINIAPP_PERMISSION_ALARMS_SCHEDULE;
        else
            return false;
        if((result & permission) != 0)
            return false;
        result |= permission;
        if(separator == NULL)
            break;
        cursor = separator + 1;
        if(*cursor == '\0')
            return false;
    }
    *permissions = result;
    return true;
}

static int read_field(
    struct json_reader *reader, const char *key,
    struct crazypod_miniapp_metadata *metadata,
    uint32_t *bit)
{
#define IS_KEY(value) (strcmp(key, value) == 0)
    if(IS_KEY("kind")) {
        char value[24];

        *bit = CPK5_KIND_FIELD;
        if(!read_string(reader, value, sizeof(value)))
            return false;
        if(strcmp(value, "miniapp") == 0)
            metadata->kind = CRAZYPOD_MINIAPP_KIND_APP;
        else if(strcmp(value, "now-playing-theme") == 0)
            metadata->kind =
                CRAZYPOD_MINIAPP_KIND_NOW_PLAYING_THEME;
        else
            return false;
        return true;
    }
    if(IS_KEY("statusBar")) {
        char value[12];

        *bit = CPK5_STATUS_BAR_FIELD;
        if(!read_string(reader, value, sizeof(value)))
            return false;
        if(strcmp(value, "system") == 0)
            metadata->status_bar =
                CRAZYPOD_MINIAPP_STATUS_BAR_SYSTEM;
        else if(strcmp(value, "theme") == 0)
            metadata->status_bar =
                CRAZYPOD_MINIAPP_STATUS_BAR_THEME;
        else
            return false;
        return true;
    }
    if(IS_KEY("artworkSourceSize")) {
        uint32_t value;

        *bit = CPK5_ARTWORK_SOURCE_SIZE_FIELD;
        if(!read_uint32(reader, &value) ||
           value < CRAZYPOD_MINIAPP_ARTWORK_SOURCE_MIN ||
           value > CRAZYPOD_MINIAPP_ARTWORK_SOURCE_MAX)
            return false;
        metadata->artwork_source_size = (uint16_t)value;
        return true;
    }
    if(IS_KEY("fontSet")) {
        *bit = CPK5_FONT_SET_FIELD;
        return read_string(
            reader, metadata->font_set, sizeof(metadata->font_set));
    }
    if(IS_KEY("permissions")) {
        char value[128];

        *bit = CPK5_PERMISSIONS_FIELD;
        if(!read_string(reader, value, sizeof(value)))
            return false;
        return parse_permissions(value, &metadata->permissions);
    }
    if(IS_KEY("signingKeyId")) {
        *bit = CPK5_SIGNING_KEY_FIELD;
        return read_string(reader, metadata->signing_key_id,
                           sizeof(metadata->signing_key_id));
    }
    if(IS_KEY("signature")) {
        size_t index;

        *bit = CPK5_SIGNATURE_FIELD;
        if(!read_string(reader, metadata->signature,
                        sizeof(metadata->signature)) ||
           strlen(metadata->signature) != 64u)
            return false;
        for(index = 0; index < 64u; ++index)
            if(hex_digit(metadata->signature[index]) < 0)
                return false;
        return true;
    }
    if(IS_KEY("format")) {
        *bit = 1u << 0;
        return read_uint32(reader, &metadata->package_format);
    }
    if(IS_KEY("id")) {
        *bit = 1u << 1;
        return read_string(
            reader, metadata->id, sizeof(metadata->id));
    }
    if(IS_KEY("name")) {
        *bit = 1u << 2;
        return read_string(
            reader, metadata->name, sizeof(metadata->name));
    }
    if(IS_KEY("version")) {
        *bit = 1u << 3;
        return read_string(
            reader, metadata->version, sizeof(metadata->version));
    }
    if(IS_KEY("versionCode")) {
        *bit = 1u << 4;
        return read_uint32(reader, &metadata->version_code);
    }
    if(IS_KEY("entry")) {
        *bit = 1u << 5;
        return read_string(
            reader, metadata->entry, sizeof(metadata->entry));
    }
    if(IS_KEY("icon")) {
        *bit = 1u << 7;
        return read_string(
            reader, metadata->icon, sizeof(metadata->icon));
    }
    if(IS_KEY("symbol")) {
        *bit = 1u << 9;
        return read_string(
            reader, metadata->symbol, sizeof(metadata->symbol));
    }
    if(IS_KEY("summary")) {
        *bit = 1u << 10;
        return read_string(
            reader, metadata->summary, sizeof(metadata->summary));
    }
    if(IS_KEY("accent")) {
        char value[8];
        *bit = 1u << 11;
        return read_string(reader, value, sizeof(value)) &&
            parse_rgb(value, &metadata->accent_rgb);
    }
    if(IS_KEY("runtime")) {
        *bit = 1u << 12;
        return read_string(
            reader, metadata->runtime, sizeof(metadata->runtime));
    }
    if(IS_KEY("abiMajor")) {
        *bit = 1u << 13;
        return read_uint32(reader, &metadata->abi_version);
    }
    if(IS_KEY("abiMinor")) {
        *bit = 1u << 14;
        return read_uint32(reader, &metadata->abi_minor);
    }
    if(IS_KEY("reactProfile")) {
        *bit = 1u << 15;
        return read_uint32(reader, &metadata->react_profile);
    }
    if(IS_KEY("target")) {
        *bit = 1u << 16;
        return read_string(
            reader, metadata->target, sizeof(metadata->target));
    }
    return false;
#undef IS_KEY
}

int crazypod_miniapp_manifest_parse(
    char *buffer, size_t size,
    struct crazypod_miniapp_metadata *metadata)
{
    struct json_reader reader;
    uint32_t seen = 0;

    if(buffer == NULL || metadata == NULL || size == 0 ||
       size > CRAZYPOD_MINIAPP_MANIFEST_MAX ||
       memchr(buffer, '\0', size) != NULL)
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
    memset(metadata, 0, sizeof(*metadata));
    reader.cursor = buffer;
    reader.end = buffer + size;
    if(!take(&reader, '{'))
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
    skip_space(&reader);
    while(reader.cursor < reader.end && *reader.cursor != '}') {
        char key[24];
        uint32_t bit = 0;

        if(!read_string(&reader, key, sizeof(key)) ||
           !take(&reader, ':') ||
           !read_field(&reader, key, metadata, &bit) ||
           bit == 0 || (seen & bit) != 0)
            return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        seen |= bit;
        skip_space(&reader);
        if(reader.cursor < reader.end && *reader.cursor == ',') {
            reader.cursor++;
            skip_space(&reader);
            if(reader.cursor >= reader.end ||
               *reader.cursor == '}')
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        }
        else {
            break;
        }
    }
    if(!take(&reader, '}'))
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
    skip_space(&reader);
    if(reader.cursor != reader.end ||
       metadata->version_code == 0 ||
       !valid_id(metadata->id) ||
       !crazypod_miniapp_text_valid(metadata->name, true) ||
       !crazypod_miniapp_text_valid(metadata->version, false) ||
       !crazypod_miniapp_text_valid(metadata->symbol, true) ||
       !crazypod_miniapp_text_valid(metadata->summary, true))
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
    if(metadata->package_format == CP_NATIVE_PACKAGE_FORMAT) {
        uint32_t optional_fields = CPK5_KIND_FIELD |
            CPK5_STATUS_BAR_FIELD | CPK5_ARTWORK_SOURCE_SIZE_FIELD |
            CPK5_FONT_SET_FIELD | CPK5_PERMISSIONS_FIELD |
            CPK5_SIGNING_KEY_FIELD | CPK5_SIGNATURE_FIELD;

        if((seen & CPK5_REQUIRED_FIELDS) != CPK5_REQUIRED_FIELDS ||
           (seen & ~(CPK5_REQUIRED_FIELDS | optional_fields)) != 0 ||
           strcmp(metadata->runtime, "native-aot") != 0 ||
           metadata->abi_version != CP_NATIVE_ABI_MAJOR ||
           metadata->abi_minor > CP_NATIVE_ABI_MINOR ||
           metadata->abi_minor == CP_NATIVE_ABI_REJECTED_MINOR ||
           metadata->react_profile != CP_NATIVE_REACT_PROFILE)
            return CRAZYPOD_MINIAPP_ERROR_VERSION;
        if(metadata->kind ==
               CRAZYPOD_MINIAPP_KIND_NOW_PLAYING_THEME &&
           ((seen & CPK5_KIND_FIELD) == 0 ||
            metadata->abi_minor < 4u))
            return CRAZYPOD_MINIAPP_ERROR_VERSION;
        if((seen & CPK5_STATUS_BAR_FIELD) != 0 &&
           (metadata->abi_minor < 10u ||
            (metadata->status_bar ==
                 CRAZYPOD_MINIAPP_STATUS_BAR_THEME &&
             metadata->kind !=
                 CRAZYPOD_MINIAPP_KIND_NOW_PLAYING_THEME)))
            return CRAZYPOD_MINIAPP_ERROR_VERSION;
        if((seen & CPK5_ARTWORK_SOURCE_SIZE_FIELD) != 0 &&
           (metadata->abi_minor < 11u ||
            metadata->kind !=
                CRAZYPOD_MINIAPP_KIND_NOW_PLAYING_THEME))
            return CRAZYPOD_MINIAPP_ERROR_VERSION;
        if((seen & CPK5_FONT_SET_FIELD) != 0 &&
           metadata->abi_minor < 16u)
            return CRAZYPOD_MINIAPP_ERROR_VERSION;
        if((seen & CPK5_PERMISSIONS_FIELD) != 0 &&
           metadata->abi_minor < 17u)
            return CRAZYPOD_MINIAPP_ERROR_VERSION;
        if(((seen & CPK5_SIGNING_KEY_FIELD) != 0) !=
           ((seen & CPK5_SIGNATURE_FIELD) != 0) ||
           ((seen & CPK5_SIGNING_KEY_FIELD) != 0 &&
            (metadata->abi_minor < 17u ||
             metadata->signing_key_id[0] == '\0')))
            return CRAZYPOD_MINIAPP_ERROR_VERSION;
        if(metadata->kind ==
               CRAZYPOD_MINIAPP_KIND_NOW_PLAYING_THEME) {
            if(metadata->abi_minor >= 11u &&
               (seen & CPK5_ARTWORK_SOURCE_SIZE_FIELD) == 0)
                return CRAZYPOD_MINIAPP_ERROR_VERSION;
            if(metadata->artwork_source_size == 0)
                metadata->artwork_source_size =
                    CRAZYPOD_MINIAPP_ARTWORK_SOURCE_DEFAULT;
        }
        if(strcmp(metadata->target, NATIVE_TARGET) != 0 ||
           strcmp(metadata->entry, NATIVE_ENTRY) != 0 ||
           strcmp(metadata->icon, ICON_NAME) != 0)
            return CRAZYPOD_MINIAPP_ERROR_PLATFORM;
        return CRAZYPOD_MINIAPP_OK;
    }
    return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
}

#endif
