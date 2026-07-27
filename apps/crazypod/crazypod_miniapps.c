#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "crc32.h"
#include "dir.h"
#include "disk.h"
#include "errno.h"
#include "file.h"
#include "kernel.h"
#include "load_code.h"
#include "metadata.h"
#include "mv.h"
#include "power.h"
#include "powermgmt.h"
#include "timefuncs.h"
#include "usb.h"

#include "crazypod_crypto.h"
#include "crazypod_miniapps.h"
#include "crazypod_state.h"

#define MINIAPP_ROOT "/.crazypod/miniapps"
#define MINIAPP_DATA_ROOT "/.crazypod/miniapp-data"
#define MINIAPP_USER_ROOT "/MiniApps"
#define MINIAPP_USER_INSTALL "/MiniApps/Install"
#define MINIAPP_SYSTEM_PACKAGES "/.rockbox/crazypod/miniapps/packages"

#define MINIAPP_MANIFEST_NAME "manifest.ini"
#define MINIAPP_ICON_NAME "icon.bmp"
#define MINIAPP_SIGNATURE_NAME "signature.ed25519"
#define MINIAPP_RESOURCES_NAME "resources.bin"
#define MINIAPP_INSTALL_RECORD_NAME ".install.bin"
#define MINIAPP_STATE_NAME "state.bin"
#define MINIAPP_STATE_TEMP_NAME "state.tmp"
#define MINIAPP_ALARM_NAME "alarm.bin"
#define MINIAPP_ALARM_TEMP_NAME "alarm.tmp"
#define MINIAPP_NOTIFICATION_NAME "notification.bin"
#define MINIAPP_NOTIFICATION_TEMP_NAME "notification.tmp"
#define MINIAPP_ALARM_FILENAME_SIZE 24

#if CONFIG_BINFMT == BINFMT_ROCK
#define MINIAPP_EXPECTED_TARGET "ipod6g"
#define MINIAPP_EXPECTED_BINARY "app.arm"
#else
#define MINIAPP_EXPECTED_TARGET "simulator"
#define MINIAPP_EXPECTED_BINARY "app.dylib"
#endif

#define MINIAPP_CPK_V1_ENTRIES 4
#define MINIAPP_CPK_V2_ENTRIES 5
#define MINIAPP_CPK_MAX_ENTRIES MINIAPP_CPK_V2_ENTRIES
#define MINIAPP_MANIFEST_MAX 2048u
#define MINIAPP_ICON_BYTES 102454u
#define MINIAPP_BINARY_MAX ((uint32_t)PLUGIN_BUFFER_SIZE)
#define MINIAPP_CPK_MAX (MINIAPP_BINARY_MAX + 640u * 1024u)
#define MINIAPP_RESOURCES_MAX (512u * 1024u)
#define MINIAPP_RESOURCE_MAX (128u * 1024u)
#define MINIAPP_RESOURCE_COUNT_MAX 32u
#define MINIAPP_RESOURCE_HEADER_SIZE 16u
#define MINIAPP_RESOURCE_ENTRY_SIZE 52u
#define MINIAPP_RESOURCE_MAGIC 0x53525043u /* CPRS */
#define MINIAPP_RESOURCE_VERSION 1u
#define MINIAPP_IO_BUFFER 1024u
#define MINIAPP_STATE_MAX (16u * 1024u)
#define MINIAPP_SCAN_LIMIT 64
#define MINIAPP_REMOVE_DEPTH 3
#define MINIAPP_REMOVE_ENTRIES 32
#define MINIAPP_SPACE_RESERVE (128u * 1024u)

#define MINIAPP_INSTALL_MAGIC 0x4350494eu /* CPIN */
#define MINIAPP_STATE_MAGIC 0x43505354u /* CPST */
#define MINIAPP_ALARM_MAGIC 0x4350414cu /* CPAL */
#define MINIAPP_DISK_VERSION 1u

#define ZIP_SIG_EOCD 0x06054b50u
#define ZIP_SIG_CENTRAL 0x02014b50u
#define ZIP_SIG_LOCAL 0x04034b50u
#define ZIP_METHOD_STORE 0u

enum cpk_entry_id {
    CPK_MANIFEST = 0,
    CPK_BINARY,
    CPK_ICON,
    CPK_SIGNATURE,
    CPK_RESOURCES
};

struct cpk_entry {
    char name[24];
    uint32_t crc32;
    uint32_t size;
    uint32_t local_offset;
    uint32_t data_offset;
    uint32_t span_end;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t method;
    uint16_t dos_time;
    uint16_t dos_date;
};

struct cpk_reader {
    int fd;
    uint32_t file_size;
    uint32_t central_offset;
    uint8_t entry_count;
    struct cpk_entry entries[MINIAPP_CPK_MAX_ENTRIES];
};

struct install_file_record {
    uint32_t size;
    uint32_t crc32;
};

struct install_record {
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    uint32_t version_code;
    struct install_file_record files[MINIAPP_CPK_V1_ENTRIES];
    uint32_t checksum;
};

struct state_header {
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    uint32_t data_size;
    uint32_t data_crc32;
    uint32_t checksum;
};

enum alarm_flags {
    ALARM_ACTIVE = 1u << 0,
    ALARM_FIRED = 1u << 1
};

struct alarm_record {
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    uint32_t deadline_epoch;
    uint32_t token;
    uint32_t flags;
    uint32_t checksum;
};

struct miniapp_binary_header_runtime {
    struct lc_header lc_header;
    cp_miniapp_entry_fn entry;
    unsigned char *bss_start;
    uint32_t host_api_size;
    uint32_t ops_size;
};

struct file_sink {
    int fd;
};

struct install_workspace {
    struct crazypod_miniapp_metadata metadata;
    struct crazypod_miniapp_metadata verified_metadata;
    char manifest[MINIAPP_MANIFEST_MAX + 1];
};

static struct crazypod_miniapp_metadata registry[CRAZYPOD_MINIAPP_MAX_APPS];
static int registry_count;
struct verified_app {
    char id[CRAZYPOD_MINIAPP_ID_SIZE];
    uint32_t version_code;
};
static struct verified_app
    verified_apps[CRAZYPOD_MINIAPP_MAX_APPS];
static int verified_app_count;
static struct install_workspace install_workspace;
static bool install_in_progress;

static int active_index = -1;
static void *active_handle;
static const struct cp_miniapp_ops *active_ops;
static const struct miniapp_binary_header_runtime *active_header;
static bool active_close_requested;
static bool active_ui_changed;
static char active_toast[CP_MINIAPP_TOAST_TEXT_SIZE];
static long active_toast_until;

enum host_modal_type {
    HOST_MODAL_NONE = 0,
    HOST_MODAL_TEXT,
    HOST_MODAL_CHOICE,
    HOST_MODAL_CONFIRM
};

struct host_modal {
    enum host_modal_type type;
    uint32_t request_id;
    uint16_t max_bytes;
    uint16_t item_count;
    int16_t selected;
    uint8_t character;
    char title[CP_MINIAPP_UI_TITLE_SIZE];
    char message[CP_MINIAPP_UI_MESSAGE_SIZE];
    char confirm_label[CP_MINIAPP_UI_CHOICE_LABEL_SIZE];
    char value[CP_MINIAPP_UI_VALUE_SIZE];
    struct cp_ui_choice_item items[CP_MINIAPP_UI_CHOICE_MAX];
};

static struct host_modal active_modal;
static struct cp_ui_result active_modal_result;
static bool active_modal_result_ready;

#if CONFIG_BINFMT == BINFMT_ROCK
extern unsigned char pluginbuf[];
#endif

static uint16_t read_le16(const uint8_t *value)
{
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8);
}

static uint32_t read_le32(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static uint32_t crc_finish(uint32_t crc)
{
    return ~crc;
}

static uint32_t crc_buffer(const void *buffer, size_t size)
{
    return crc_finish(crc_32r(buffer, (uint32_t)size, 0xffffffffu));
}

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

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const uint8_t *cursor = buffer;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool read_at_exact(int fd, uint32_t offset, void *buffer, size_t size)
{
    if(lseek(fd, (off_t)offset, SEEK_SET) != (off_t)offset)
        return false;
    return read_exact(fd, buffer, size);
}

static bool make_path(char *path, size_t capacity,
                      const char *left, const char *right)
{
    int length = snprintf(path, capacity, "%s/%s", left, right);
    return length >= 0 && (size_t)length < capacity;
}

static bool make_tagged_path(char *path, size_t capacity,
                             const char *tag, const char *id)
{
    int length = snprintf(path, capacity, "%s/.%s-%s",
                          MINIAPP_ROOT, tag, id);
    return length >= 0 && (size_t)length < capacity;
}

static bool ensure_directory(const char *path)
{
    if(mkdir(path) == 0)
        return true;
    return errno == EEXIST && dir_exists(path);
}

static size_t bounded_length(const char *text, size_t capacity)
{
    size_t length = 0;

    if(text == NULL)
        return capacity;
    while(length < capacity && text[length] != '\0')
        ++length;
    return length;
}

static bool copy_text(char *destination, size_t capacity,
                      const char *source, size_t length)
{
    if(destination == NULL || source == NULL || capacity == 0 ||
       length >= capacity)
        return false;
    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}

static bool valid_utf8_text(const char *text, bool allow_space)
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
        } else if((value & 0xf0) == 0xe0) {
            continuation = 2;
            codepoint = value & 0x0f;
            minimum = 0x800u;
        } else if((value & 0xf8) == 0xf0) {
            continuation = 3;
            codepoint = value & 0x07;
            minimum = 0x10000u;
        } else {
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

static bool valid_id(const char *id)
{
    size_t index;
    size_t length = bounded_length(id, CRAZYPOD_MINIAPP_ID_SIZE);

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

static void verified_apps_clear(void)
{
    memset(verified_apps, 0, sizeof(verified_apps));
    verified_app_count = 0;
}

static bool verified_app_matches(
    const char *id, uint32_t version_code)
{
    int index;

    for(index = 0; index < verified_app_count; ++index) {
        if(verified_apps[index].version_code == version_code &&
           strcmp(verified_apps[index].id, id) == 0)
            return true;
    }
    return false;
}

static void verified_app_mark(
    const char *id, uint32_t version_code)
{
    int index;

    if(!valid_id(id) || version_code == 0)
        return;
    for(index = 0; index < verified_app_count; ++index) {
        if(strcmp(verified_apps[index].id, id) == 0) {
            verified_apps[index].version_code = version_code;
            return;
        }
    }
    if(verified_app_count >= CRAZYPOD_MINIAPP_MAX_APPS)
        return;
    copy_text(
        verified_apps[verified_app_count].id,
        sizeof(verified_apps[verified_app_count].id),
        id, strlen(id));
    verified_apps[verified_app_count].version_code = version_code;
    ++verified_app_count;
}

static bool parse_uint32(const char *text, size_t length, uint32_t *value)
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

static bool parse_rgb(const char *text, size_t length, uint32_t *value)
{
    uint32_t result = 0;
    size_t index;

    if(length != 6)
        return false;
    for(index = 0; index < length; ++index) {
        unsigned int digit;
        if(text[index] >= '0' && text[index] <= '9')
            digit = (unsigned int)(text[index] - '0');
        else if(text[index] >= 'A' && text[index] <= 'F')
            digit = (unsigned int)(text[index] - 'A') + 10u;
        else if(text[index] >= 'a' && text[index] <= 'f')
            digit = (unsigned int)(text[index] - 'a') + 10u;
        else
            return false;
        result = (result << 4) | digit;
    }
    *value = result;
    return true;
}

static bool parse_sha256(const char *text, size_t length, uint8_t digest[32])
{
    size_t index;

    if(length != 64)
        return false;
    for(index = 0; index < 32; ++index) {
        unsigned int high;
        unsigned int low;
        char first = text[index * 2];
        char second = text[index * 2 + 1];

        if(first >= '0' && first <= '9')
            high = (unsigned int)(first - '0');
        else if(first >= 'a' && first <= 'f')
            high = (unsigned int)(first - 'a') + 10u;
        else if(first >= 'A' && first <= 'F')
            high = (unsigned int)(first - 'A') + 10u;
        else
            return false;
        if(second >= '0' && second <= '9')
            low = (unsigned int)(second - '0');
        else if(second >= 'a' && second <= 'f')
            low = (unsigned int)(second - 'a') + 10u;
        else if(second >= 'A' && second <= 'F')
            low = (unsigned int)(second - 'A') + 10u;
        else
            return false;
        digest[index] = (uint8_t)((high << 4) | low);
    }
    return true;
}

static int parse_manifest(char *buffer, size_t size,
                          struct crazypod_miniapp_metadata *metadata)
{
    char *cursor = buffer;
    char *end = buffer + size;
    uint32_t seen = 0;
    uint32_t format = 0;
    char resources[CRAZYPOD_MINIAPP_RESOURCES_SIZE] = "";

    if(buffer == NULL || metadata == NULL || size == 0 ||
       size > MINIAPP_MANIFEST_MAX ||
       memchr(buffer, '\0', size) != NULL)
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
    memset(metadata, 0, sizeof(*metadata));
    buffer[size] = '\0';

    while(cursor < end) {
        char *line = cursor;
        char *equals;
        size_t line_length;
        size_t key_length;
        const char *value;
        size_t value_length;
        uint32_t bit;

        while(cursor < end && *cursor != '\n')
            ++cursor;
        line_length = (size_t)(cursor - line);
        if(cursor < end)
            ++cursor;
        if(line_length > 0 && line[line_length - 1] == '\r')
            --line_length;
        if(line_length == 0)
            return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        equals = memchr(line, '=', line_length);
        if(equals == NULL || equals == line)
            return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        key_length = (size_t)(equals - line);
        value = equals + 1;
        value_length = line_length - key_length - 1;
        if(value_length == 0)
            return CRAZYPOD_MINIAPP_ERROR_MANIFEST;

        if(key_length == 6 && memcmp(line, "format", 6) == 0) {
            bit = 1u << 0;
            if(!parse_uint32(value, value_length, &format))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 2 && memcmp(line, "id", 2) == 0) {
            bit = 1u << 1;
            if(!copy_text(metadata->id, sizeof(metadata->id),
                          value, value_length))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 4 && memcmp(line, "name", 4) == 0) {
            bit = 1u << 2;
            if(!copy_text(metadata->name, sizeof(metadata->name),
                          value, value_length))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 7 && memcmp(line, "version", 7) == 0) {
            bit = 1u << 3;
            if(!copy_text(metadata->version, sizeof(metadata->version),
                          value, value_length))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 12 &&
                  memcmp(line, "version_code", 12) == 0) {
            bit = 1u << 4;
            if(!parse_uint32(value, value_length,
                             &metadata->version_code))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 3 && memcmp(line, "abi", 3) == 0) {
            bit = 1u << 5;
            if(!parse_uint32(value, value_length,
                             &metadata->abi_version))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 6 && memcmp(line, "target", 6) == 0) {
            bit = 1u << 6;
            if(!copy_text(metadata->target, sizeof(metadata->target),
                          value, value_length))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 6 && memcmp(line, "binary", 6) == 0) {
            bit = 1u << 7;
            if(!copy_text(metadata->binary, sizeof(metadata->binary),
                          value, value_length))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 13 &&
                  memcmp(line, "binary_sha256", 13) == 0) {
            bit = 1u << 8;
            if(!parse_sha256(value, value_length,
                             metadata->binary_sha256))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 4 && memcmp(line, "icon", 4) == 0) {
            bit = 1u << 9;
            if(!copy_text(metadata->icon, sizeof(metadata->icon),
                          value, value_length))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 11 &&
                  memcmp(line, "icon_sha256", 11) == 0) {
            bit = 1u << 10;
            if(!parse_sha256(value, value_length,
                             metadata->icon_sha256))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 6 && memcmp(line, "symbol", 6) == 0) {
            bit = 1u << 11;
            if(!copy_text(metadata->symbol, sizeof(metadata->symbol),
                          value, value_length))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 6 && memcmp(line, "accent", 6) == 0) {
            bit = 1u << 12;
            if(!parse_rgb(value, value_length, &metadata->accent_rgb))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 7 && memcmp(line, "summary", 7) == 0) {
            bit = 1u << 13;
            if(!copy_text(metadata->summary, sizeof(metadata->summary),
                          value, value_length))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 9 &&
                  memcmp(line, "resources", 9) == 0) {
            bit = 1u << 14;
            if(!copy_text(resources, sizeof(resources),
                          value, value_length))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else if(key_length == 16 &&
                  memcmp(line, "resources_sha256", 16) == 0) {
            bit = 1u << 15;
            if(!parse_sha256(value, value_length,
                             metadata->resources_sha256))
                return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        } else {
            return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        }
        if((seen & bit) != 0)
            return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
        seen |= bit;
    }

    if(((format == 1u && seen != 0x3fffu) ||
        (format == 2u && seen != 0xffffu) ||
        (format != 1u && format != 2u)) ||
       metadata->version_code == 0 ||
       !valid_id(metadata->id) ||
       !valid_utf8_text(metadata->name, true) ||
       !valid_utf8_text(metadata->version, false) ||
       !valid_utf8_text(metadata->symbol, true) ||
       !valid_utf8_text(metadata->summary, true))
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
    metadata->package_format = format;
    if(metadata->abi_version != CP_MINIAPP_ABI_VERSION)
        return CRAZYPOD_MINIAPP_ERROR_ABI;
    if(strcmp(metadata->target, MINIAPP_EXPECTED_TARGET) != 0 ||
       strcmp(metadata->binary, MINIAPP_EXPECTED_BINARY) != 0 ||
       strcmp(metadata->icon, MINIAPP_ICON_NAME) != 0 ||
       (format == 2u &&
        strcmp(resources, MINIAPP_RESOURCES_NAME) != 0))
        return CRAZYPOD_MINIAPP_ERROR_PLATFORM;
    return CRAZYPOD_MINIAPP_OK;
}

static int expected_entry(const char *name)
{
    if(strcmp(name, MINIAPP_MANIFEST_NAME) == 0)
        return CPK_MANIFEST;
    if(strcmp(name, MINIAPP_EXPECTED_BINARY) == 0)
        return CPK_BINARY;
    if(strcmp(name, MINIAPP_ICON_NAME) == 0)
        return CPK_ICON;
    if(strcmp(name, MINIAPP_SIGNATURE_NAME) == 0)
        return CPK_SIGNATURE;
    if(strcmp(name, MINIAPP_RESOURCES_NAME) == 0)
        return CPK_RESOURCES;
    return -1;
}

static bool entry_size_valid(int entry, uint32_t size)
{
    switch(entry) {
    case CPK_MANIFEST:
        return size > 0 && size <= MINIAPP_MANIFEST_MAX;
    case CPK_BINARY:
        return size > 0 && size <= MINIAPP_BINARY_MAX;
    case CPK_ICON:
        return size == MINIAPP_ICON_BYTES;
    case CPK_SIGNATURE:
        return size == CRAZYPOD_MINIAPP_SIGNATURE_SIZE;
    case CPK_RESOURCES:
        return size >= MINIAPP_RESOURCE_HEADER_SIZE &&
               size <= MINIAPP_RESOURCES_MAX;
    default:
        return false;
    }
}

static int cpk_open_shallow(const char *path, struct cpk_reader *reader)
{
    uint8_t eocd[22];
    uint32_t central_size;
    uint32_t central_offset;
    uint64_t cursor;
    uint32_t seen = 0;
    uint16_t entry_count;
    off_t file_size;
    int index;

    memset(reader, 0, sizeof(*reader));
    reader->fd = -1;
    reader->fd = open(path, O_RDONLY);
    if(reader->fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    file_size = filesize(reader->fd);
    if(file_size < (off_t)sizeof(eocd) ||
       file_size > (off_t)MINIAPP_CPK_MAX)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    reader->file_size = (uint32_t)file_size;
    if(!read_at_exact(reader->fd, reader->file_size - sizeof(eocd),
                      eocd, sizeof(eocd)))
        return CRAZYPOD_MINIAPP_ERROR_IO;
    entry_count = read_le16(eocd + 8);
    if(read_le32(eocd) != ZIP_SIG_EOCD ||
       read_le16(eocd + 4) != 0 ||
       read_le16(eocd + 6) != 0 ||
       (entry_count != MINIAPP_CPK_V1_ENTRIES &&
        entry_count != MINIAPP_CPK_V2_ENTRIES) ||
       read_le16(eocd + 10) != entry_count ||
       read_le16(eocd + 20) != 0)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    reader->entry_count = (uint8_t)entry_count;
    central_size = read_le32(eocd + 12);
    central_offset = read_le32(eocd + 16);
    if((uint64_t)central_offset + central_size !=
       (uint64_t)reader->file_size - sizeof(eocd))
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    reader->central_offset = central_offset;
    cursor = central_offset;

    for(index = 0; index < reader->entry_count; ++index) {
        uint8_t header[46];
        char name[24];
        uint16_t name_length;
        uint16_t extra_length;
        uint16_t comment_length;
        int slot;
        struct cpk_entry *entry;

        if(cursor + sizeof(header) >
           (uint64_t)central_offset + central_size ||
           !read_at_exact(reader->fd, (uint32_t)cursor,
                          header, sizeof(header)))
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        if(read_le32(header) != ZIP_SIG_CENTRAL)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        name_length = read_le16(header + 28);
        extra_length = read_le16(header + 30);
        comment_length = read_le16(header + 32);
        if(name_length == 0 || name_length >= sizeof(name) ||
           extra_length != 0 || comment_length != 0 ||
           read_le16(header + 34) != 0)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        cursor += sizeof(header);
        if(cursor + name_length >
           (uint64_t)central_offset + central_size ||
           !read_at_exact(reader->fd, (uint32_t)cursor,
                          name, name_length))
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        name[name_length] = '\0';
        slot = expected_entry(name);
        if(slot < 0 || (seen & (1u << slot)) != 0)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        seen |= 1u << slot;
        entry = &reader->entries[slot];
        memcpy(entry->name, name, name_length + 1);
        entry->version_needed = read_le16(header + 6);
        entry->flags = read_le16(header + 8);
        entry->method = read_le16(header + 10);
        entry->dos_time = read_le16(header + 12);
        entry->dos_date = read_le16(header + 14);
        entry->crc32 = read_le32(header + 16);
        if(read_le32(header + 20) != read_le32(header + 24))
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        entry->size = read_le32(header + 24);
        entry->local_offset = read_le32(header + 42);
        if(entry->version_needed > 20 ||
           entry->flags != 0 ||
           entry->method != ZIP_METHOD_STORE ||
           !entry_size_valid(slot, entry->size) ||
           entry->local_offset >= central_offset)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        cursor += name_length;
    }
    if(seen != (reader->entry_count == MINIAPP_CPK_V1_ENTRIES
                    ? 0x0fu : 0x1fu) ||
       cursor != (uint64_t)central_offset + central_size)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    return CRAZYPOD_MINIAPP_OK;
}

static int cpk_validate_local_headers(struct cpk_reader *reader)
{
    int order[MINIAPP_CPK_MAX_ENTRIES] = { 0, 1, 2, 3, 4 };
    int index;

    for(index = 0; index < reader->entry_count; ++index) {
        struct cpk_entry *entry = &reader->entries[index];
        uint8_t header[30];
        char name[24];
        uint16_t name_length;
        uint16_t extra_length;
        uint64_t data_offset;
        uint64_t span_end;

        if(!read_at_exact(reader->fd, entry->local_offset,
                          header, sizeof(header)) ||
           read_le32(header) != ZIP_SIG_LOCAL)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        name_length = read_le16(header + 26);
        extra_length = read_le16(header + 28);
        if(name_length == 0 || name_length >= sizeof(name) ||
           extra_length != 0 ||
           read_le16(header + 4) != entry->version_needed ||
           read_le16(header + 6) != entry->flags ||
           read_le16(header + 8) != entry->method ||
           read_le16(header + 10) != entry->dos_time ||
           read_le16(header + 12) != entry->dos_date ||
           read_le32(header + 14) != entry->crc32 ||
           read_le32(header + 18) != entry->size ||
           read_le32(header + 22) != entry->size)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        if(!read_at_exact(reader->fd,
                          entry->local_offset + sizeof(header),
                          name, name_length))
            return CRAZYPOD_MINIAPP_ERROR_IO;
        name[name_length] = '\0';
        if(strcmp(name, entry->name) != 0)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        data_offset = (uint64_t)entry->local_offset +
                      sizeof(header) + name_length;
        span_end = data_offset + entry->size;
        if(data_offset > UINT32_MAX || span_end > reader->central_offset)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        entry->data_offset = (uint32_t)data_offset;
        entry->span_end = (uint32_t)span_end;
    }

    for(index = 1; index < reader->entry_count; ++index) {
        int position = index;
        int value = order[index];
        while(position > 0 &&
              reader->entries[order[position - 1]].local_offset >
              reader->entries[value].local_offset) {
            order[position] = order[position - 1];
            --position;
        }
        order[position] = value;
    }
    if(reader->entries[order[0]].local_offset != 0)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    for(index = 1; index < reader->entry_count; ++index) {
        if(reader->entries[order[index - 1]].span_end !=
           reader->entries[order[index]].local_offset)
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    }
    if(reader->entries[order[reader->entry_count - 1]].span_end !=
       reader->central_offset)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    return CRAZYPOD_MINIAPP_OK;
}

static bool valid_resource_id_bytes(const uint8_t id[32])
{
    size_t index;
    size_t length = 0;

    while(length < 32 && id[length] != 0)
        ++length;
    if(length == 0 || length == 32 ||
       id[0] < 'a' || id[0] > 'z')
        return false;
    for(index = 1; index < length; ++index) {
        uint8_t value = id[index];
        if(!((value >= 'a' && value <= 'z') ||
             (value >= '0' && value <= '9') ||
             value == '_' || value == '-' || value == '.'))
            return false;
    }
    for(index = length + 1; index < 32; ++index) {
        if(id[index] != 0)
            return false;
    }
    return true;
}

static bool validate_resource_container_fd(
    int fd, uint32_t base_offset, uint32_t total_size)
{
    uint8_t header[MINIAPP_RESOURCE_HEADER_SIZE];
    uint8_t entry[MINIAPP_RESOURCE_ENTRY_SIZE];
    uint8_t payload[MINIAPP_IO_BUFFER];
    uint8_t previous_id[32];
    uint32_t expected_offset;
    uint32_t index_crc = 0xffffffffu;
    uint16_t count;
    uint16_t index;

    if(total_size < MINIAPP_RESOURCE_HEADER_SIZE ||
       total_size > MINIAPP_RESOURCES_MAX ||
       !read_at_exact(fd, base_offset, header, sizeof(header)) ||
       read_le32(header) != MINIAPP_RESOURCE_MAGIC ||
       read_le16(header + 4) != MINIAPP_RESOURCE_VERSION)
        return false;
    count = read_le16(header + 6);
    if(count > MINIAPP_RESOURCE_COUNT_MAX ||
       read_le32(header + 8) != total_size ||
       (uint64_t)MINIAPP_RESOURCE_HEADER_SIZE +
           (uint64_t)count * MINIAPP_RESOURCE_ENTRY_SIZE > total_size)
        return false;
    memset(previous_id, 0, sizeof(previous_id));
    expected_offset = MINIAPP_RESOURCE_HEADER_SIZE +
                      (uint32_t)count * MINIAPP_RESOURCE_ENTRY_SIZE;
    for(index = 0; index < count; ++index) {
        uint32_t offset;
        uint32_t size;
        uint16_t width;
        uint16_t height;
        uint32_t remaining;
        uint32_t payload_offset;
        uint32_t payload_crc = 0xffffffffu;

        if(!read_at_exact(
               fd, base_offset + MINIAPP_RESOURCE_HEADER_SIZE +
                       (uint32_t)index * MINIAPP_RESOURCE_ENTRY_SIZE,
               entry, sizeof(entry)) ||
           !valid_resource_id_bytes(entry) ||
           (index > 0 && memcmp(previous_id, entry, 32) >= 0) ||
           entry[33] != 0 || read_le16(entry + 38) != 0)
            return false;
        memcpy(previous_id, entry, sizeof(previous_id));
        index_crc = crc_32r(entry, sizeof(entry), index_crc);
        width = read_le16(entry + 34);
        height = read_le16(entry + 36);
        offset = read_le32(entry + 40);
        size = read_le32(entry + 44);
        if(size > MINIAPP_RESOURCE_MAX || offset != expected_offset ||
           (uint64_t)offset + size > total_size)
            return false;
        if(entry[32] == CP_RESOURCE_BITMAP_RGB565) {
            if(width == 0 || height == 0 ||
               width > 160 || height > 160 ||
               size != (uint32_t)width * height * 2u)
                return false;
        } else if(entry[32] != CP_RESOURCE_BLOB ||
                  width != 0 || height != 0) {
            return false;
        }
        remaining = size;
        payload_offset = offset;
        while(remaining > 0) {
            uint32_t amount = remaining > sizeof(payload)
                ? (uint32_t)sizeof(payload) : remaining;
            if(!read_at_exact(
                   fd, base_offset + payload_offset,
                   payload, amount))
                return false;
            payload_crc = crc_32r(payload, amount, payload_crc);
            payload_offset += amount;
            remaining -= amount;
        }
        if(crc_finish(payload_crc) != read_le32(entry + 48))
            return false;
        expected_offset += size;
    }
    return expected_offset == total_size &&
           crc_finish(index_crc) == read_le32(header + 12);
}

static bool validate_cpk_resources(const struct cpk_reader *reader)
{
    const struct cpk_entry *entry;

    if(reader->entry_count != MINIAPP_CPK_V2_ENTRIES)
        return false;
    entry = &reader->entries[CPK_RESOURCES];
    return validate_resource_container_fd(
        reader->fd, entry->data_offset, entry->size);
}

typedef bool (*cpk_sink_fn)(void *context, const void *buffer, size_t size);

static int cpk_stream_entry(const struct cpk_reader *reader, int entry_index,
                            cpk_sink_fn sink, void *sink_context)
{
    const struct cpk_entry *entry = &reader->entries[entry_index];
    uint8_t buffer[MINIAPP_IO_BUFFER];
    uint32_t remaining = entry->size;
    uint32_t offset = entry->data_offset;
    uint32_t crc = 0xffffffffu;

    while(remaining > 0) {
        uint32_t amount = remaining > sizeof(buffer)
            ? (uint32_t)sizeof(buffer) : remaining;
        if(!read_at_exact(reader->fd, offset, buffer, amount))
            return CRAZYPOD_MINIAPP_ERROR_IO;
        crc = crc_32r(buffer, amount, crc);
        if(sink != NULL && !sink(sink_context, buffer, amount))
            return CRAZYPOD_MINIAPP_ERROR_IO;
        offset += amount;
        remaining -= amount;
    }
    if(crc_finish(crc) != entry->crc32)
        return CRAZYPOD_MINIAPP_ERROR_CRC;
    return CRAZYPOD_MINIAPP_OK;
}

static int cpk_read_entry(const struct cpk_reader *reader, int entry_index,
                          void *buffer, size_t capacity)
{
    const struct cpk_entry *entry = &reader->entries[entry_index];
    uint32_t crc;

    if(entry->size > capacity)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(!read_at_exact(reader->fd, entry->data_offset,
                      buffer, entry->size))
        return CRAZYPOD_MINIAPP_ERROR_IO;
    crc = crc_buffer(buffer, entry->size);
    return crc == entry->crc32
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_CRC;
}

static void cpk_close(struct cpk_reader *reader)
{
    if(reader != NULL && reader->fd >= 0) {
        close(reader->fd);
        reader->fd = -1;
    }
}

static bool valid_icon_header(const struct cpk_reader *reader)
{
    uint8_t header[54];
    const struct cpk_entry *entry = &reader->entries[CPK_ICON];
    int32_t width;
    int32_t height;

    if(!read_at_exact(reader->fd, entry->data_offset,
                      header, sizeof(header)) ||
       header[0] != 'B' || header[1] != 'M' ||
       read_le32(header + 2) != MINIAPP_ICON_BYTES ||
       read_le32(header + 10) != 54 ||
       read_le32(header + 14) != 40)
        return false;
    width = (int32_t)read_le32(header + 18);
    height = (int32_t)read_le32(header + 22);
    return width == 160 && (height == 160 || height == -160) &&
           read_le16(header + 26) == 1 &&
           read_le16(header + 28) == 32 &&
           read_le32(header + 30) == 0 &&
           read_le32(header + 34) == 102400u;
}

static bool native_header_valid(
    const struct miniapp_binary_header_runtime *header,
    uint32_t file_size)
{
#if CONFIG_BINFMT == BINFMT_ROCK
    uintptr_t load;
    uintptr_t end;
    uintptr_t bss;
    uintptr_t entry;

    if(header == NULL ||
       header->lc_header.magic != CP_MINIAPP_BINARY_MAGIC ||
       header->lc_header.target_id != TARGET_ID ||
       header->lc_header.api_version != CP_MINIAPP_ABI_VERSION ||
       header->host_api_size < CP_HOST_API_V1_SIZE ||
       header->host_api_size > sizeof(struct cp_host_api) ||
       header->ops_size != sizeof(struct cp_miniapp_ops) ||
       header->entry == NULL)
        return false;
    load = (uintptr_t)header->lc_header.load_addr;
    end = (uintptr_t)header->lc_header.end_addr;
    bss = (uintptr_t)header->bss_start;
    entry = (uintptr_t)header->entry;
    if(load != (uintptr_t)pluginbuf || end <= load ||
       end - load > PLUGIN_BUFFER_SIZE ||
       file_size > PLUGIN_BUFFER_SIZE ||
       bss < load || bss > end ||
       file_size > bss - load ||
       entry < load || entry >= bss)
        return false;
#else
    (void)header;
    (void)file_size;
#endif
    return true;
}

static int validate_packaged_binary(const struct cpk_reader *reader)
{
#if CONFIG_BINFMT == BINFMT_ROCK
    struct miniapp_binary_header_runtime header;
    const struct cpk_entry *entry = &reader->entries[CPK_BINARY];

    if(entry->size < sizeof(header) ||
       !read_at_exact(reader->fd, entry->data_offset,
                      &header, sizeof(header)))
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(!native_header_valid(&header, entry->size))
        return CRAZYPOD_MINIAPP_ERROR_ABI;
#else
    (void)reader;
#endif
    return CRAZYPOD_MINIAPP_OK;
}

struct sha256_sink {
    struct crazypod_sha256_context *context;
};

static bool sha256_sink_update(void *context, const void *buffer, size_t size)
{
    struct sha256_sink *sink = context;
    crazypod_sha256_update(sink->context, buffer, size);
    return true;
}

static int verify_cpk_sha256(const struct cpk_reader *reader, int entry,
                             const uint8_t expected[32])
{
    struct crazypod_sha256_context context;
    struct sha256_sink sink;
    uint8_t digest[32];
    int result;

    crazypod_sha256_init(&context);
    sink.context = &context;
    result = cpk_stream_entry(reader, entry,
                              sha256_sink_update, &sink);
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    crazypod_sha256_final(&context, digest);
    return memcmp(digest, expected, sizeof(digest)) == 0
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
}

static int verify_cpk_signature(const struct cpk_reader *reader,
                                const uint8_t *manifest,
                                size_t manifest_size)
{
    uint8_t signature[CRAZYPOD_MINIAPP_SIGNATURE_SIZE];
    int result;

    result = cpk_read_entry(reader, CPK_SIGNATURE,
                            signature, sizeof(signature));
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    return crazypod_ed25519_verify(
               signature, manifest, manifest_size,
               crazypod_miniapp_development_public_key)
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
}

static bool file_sink_write(void *context, const void *buffer, size_t size)
{
    struct file_sink *sink = context;
    return write_exact(sink->fd, buffer, size);
}

static int extract_entry(const struct cpk_reader *reader, int entry,
                         const char *path)
{
    struct file_sink sink;
    int result;

    sink.fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if(sink.fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    result = cpk_stream_entry(reader, entry, file_sink_write, &sink);
    if(result == CRAZYPOD_MINIAPP_OK && fsync(sink.fd) < 0)
        result = CRAZYPOD_MINIAPP_ERROR_IO;
    if(close(sink.fd) < 0 && result == CRAZYPOD_MINIAPP_OK)
        result = CRAZYPOD_MINIAPP_ERROR_IO;
    if(result != CRAZYPOD_MINIAPP_OK)
        remove(path);
    return result;
}

static uint32_t install_checksum(const struct install_record *record)
{
    return crc_buffer(record, offsetof(struct install_record, checksum));
}

static bool write_install_record(const char *directory,
                                 const struct cpk_reader *reader,
                                 uint32_t version_code)
{
    struct install_record record;
    char path[MAX_PATH];
    int fd;
    int index;
    bool success;

    if(!make_path(path, sizeof(path), directory,
                  MINIAPP_INSTALL_RECORD_NAME))
        return false;
    memset(&record, 0, sizeof(record));
    record.magic = MINIAPP_INSTALL_MAGIC;
    record.version = MINIAPP_DISK_VERSION;
    record.struct_size = sizeof(record);
    record.version_code = version_code;
    for(index = 0; index < MINIAPP_CPK_V1_ENTRIES; ++index) {
        record.files[index].size = reader->entries[index].size;
        record.files[index].crc32 = reader->entries[index].crc32;
    }
    record.checksum = install_checksum(&record);
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, &record, sizeof(record)) && fsync(fd) == 0;
    if(close(fd) < 0)
        success = false;
    if(!success)
        remove(path);
    return success;
}

static bool read_install_record(const char *directory,
                                struct install_record *record)
{
    char path[MAX_PATH];
    int fd;
    bool success;

    if(!make_path(path, sizeof(path), directory,
                  MINIAPP_INSTALL_RECORD_NAME))
        return false;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return false;
    success = filesize(fd) == (off_t)sizeof(*record) &&
              read_exact(fd, record, sizeof(*record));
    close(fd);
    return success &&
           record->magic == MINIAPP_INSTALL_MAGIC &&
           record->version == MINIAPP_DISK_VERSION &&
           record->struct_size == sizeof(*record) &&
           record->checksum == install_checksum(record);
}

static bool file_has_size(const char *path, uint32_t size)
{
    int fd = open(path, O_RDONLY);
    bool matches;

    if(fd < 0)
        return false;
    matches = filesize(fd) == (off_t)size;
    close(fd);
    return matches;
}

static int load_manifest_file(const char *directory,
                              struct crazypod_miniapp_metadata *metadata,
                              uint32_t *size_out, uint32_t *crc_out)
{
    char path[MAX_PATH];
    char buffer[MINIAPP_MANIFEST_MAX + 1];
    off_t size;
    int fd;
    int result;

    if(!make_path(path, sizeof(path), directory, MINIAPP_MANIFEST_NAME))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    if(size <= 0 || size > (off_t)MINIAPP_MANIFEST_MAX) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_MANIFEST;
    }
    if(!read_exact(fd, buffer, (size_t)size)) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    close(fd);
    result = parse_manifest(buffer, (size_t)size, metadata);
    if(result == CRAZYPOD_MINIAPP_OK) {
        if(size_out != NULL)
            *size_out = (uint32_t)size;
        if(crc_out != NULL)
            *crc_out = crc_buffer(buffer, (size_t)size);
    }
    return result;
}

static bool validate_install_directory(
    const char *directory, const char *expected_id,
    struct crazypod_miniapp_metadata *metadata,
    struct install_record *record_out)
{
    struct install_record record;
    uint32_t manifest_size;
    uint32_t manifest_crc;
    char path[MAX_PATH];

    if(load_manifest_file(directory, metadata,
                          &manifest_size, &manifest_crc) !=
       CRAZYPOD_MINIAPP_OK ||
       strcmp(metadata->id, expected_id) != 0 ||
       !read_install_record(directory, &record) ||
       record.version_code != metadata->version_code ||
       record.files[CPK_MANIFEST].size != manifest_size ||
       record.files[CPK_MANIFEST].crc32 != manifest_crc)
        return false;
    if(!make_path(path, sizeof(path), directory, metadata->binary) ||
       !file_has_size(path, record.files[CPK_BINARY].size) ||
       !make_path(path, sizeof(path), directory, MINIAPP_ICON_NAME) ||
       !file_has_size(path, record.files[CPK_ICON].size) ||
       !make_path(path, sizeof(path), directory, MINIAPP_SIGNATURE_NAME) ||
       !file_has_size(path, record.files[CPK_SIGNATURE].size))
        return false;
    if(record.files[CPK_BINARY].size == 0 ||
       record.files[CPK_BINARY].size > MINIAPP_BINARY_MAX ||
       record.files[CPK_ICON].size != MINIAPP_ICON_BYTES ||
       record.files[CPK_SIGNATURE].size !=
           CRAZYPOD_MINIAPP_SIGNATURE_SIZE)
        return false;
    if(metadata->package_format == 2u) {
        int fd;
        off_t size;

        if(!make_path(path, sizeof(path), directory,
                      MINIAPP_RESOURCES_NAME))
            return false;
        fd = open(path, O_RDONLY);
        if(fd < 0)
            return false;
        size = filesize(fd);
        if(size < (off_t)MINIAPP_RESOURCE_HEADER_SIZE ||
           size > (off_t)MINIAPP_RESOURCES_MAX ||
           !validate_resource_container_fd(
               fd, 0, (uint32_t)size)) {
            close(fd);
            return false;
        }
        close(fd);
    }
    if(record_out != NULL)
        *record_out = record;
    return true;
}

static bool remove_tree(const char *path, int depth)
{
    DIR *directory;
    struct dirent *entry;
    int entries = 0;
    bool success = true;

    if(path == NULL || depth < 0)
        return false;
    directory = opendir(path);
    if(directory == NULL)
        return !file_exists(path) && !dir_exists(path);
    while((entry = readdir(directory)) != NULL) {
        struct dirinfo info;
        char child[MAX_PATH];

        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0)
            continue;
        if(++entries > MINIAPP_REMOVE_ENTRIES ||
           !make_path(child, sizeof(child), path, entry->d_name)) {
            success = false;
            break;
        }
        info = dir_get_info(directory, entry);
        if((info.attribute & ATTR_DIRECTORY) != 0) {
            if(depth == 0 || !remove_tree(child, depth - 1)) {
                success = false;
                break;
            }
        } else if(remove(child) < 0) {
            success = false;
            break;
        }
    }
    closedir(directory);
    return success && rmdir(path) == 0;
}

static bool recover_id(const char *id)
{
    char final_path[MAX_PATH];
    char stage_path[MAX_PATH];
    char backup_path[MAX_PATH];
    bool final_exists;

    if(!valid_id(id) ||
       !make_path(final_path, sizeof(final_path), MINIAPP_ROOT, id) ||
       !make_tagged_path(stage_path, sizeof(stage_path), "stage", id) ||
       !make_tagged_path(backup_path, sizeof(backup_path), "backup", id))
        return false;
    final_exists = dir_exists(final_path);
    if(dir_exists(backup_path)) {
        if(final_exists) {
            if(!remove_tree(backup_path, MINIAPP_REMOVE_DEPTH))
                return false;
        } else {
            if(rename(backup_path, final_path) < 0)
                return false;
            final_exists = true;
        }
    }
    if(dir_exists(stage_path) &&
       !remove_tree(stage_path, MINIAPP_REMOVE_DEPTH))
        return false;
    return final_exists || !dir_exists(final_path);
}

static void recover_tagged_directories(const char *tag)
{
    DIR *directory = opendir(MINIAPP_ROOT);
    struct dirent *entry;
    char prefix[16];
    size_t prefix_length;
    int scanned = 0;

    if(directory == NULL)
        return;
    snprintf(prefix, sizeof(prefix), ".%s-", tag);
    prefix_length = strlen(prefix);
    while((entry = readdir(directory)) != NULL &&
          scanned < MINIAPP_SCAN_LIMIT) {
        const char *id;
        if(strncmp(entry->d_name, prefix, prefix_length) != 0)
            continue;
        ++scanned;
        id = entry->d_name + prefix_length;
        if(valid_id(id))
            recover_id(id);
    }
    closedir(directory);
}

static void recover_installs(void)
{
    recover_tagged_directories("backup");
    recover_tagged_directories("stage");
}

static bool enough_space(uint32_t extracted_size)
{
    sector_t free_sectors = 0;
    uint64_t free_bytes;
    unsigned int sector_size;

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    volume_recalc_free(IF_MV(0));
#endif
    volume_size(IF_MV(0,) NULL, &free_sectors);
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    sector_size = (unsigned int)disk_get_log_sector_size(IF_MD(0));
#else
    /* Hosted volume_size() reports counts normalized to 512-byte sectors. */
    sector_size = 512u;
#endif
    if(sector_size == 0)
        return false;
    free_bytes = (uint64_t)free_sectors * sector_size;
    return free_bytes >= (uint64_t)extracted_size +
                         MINIAPP_SPACE_RESERVE;
}

static int populate_paths(struct crazypod_miniapp_metadata *metadata,
                          const char *directory)
{
    if(!copy_text(metadata->install_path, sizeof(metadata->install_path),
                  directory, strlen(directory)) ||
       !make_path(metadata->binary_path, sizeof(metadata->binary_path),
                  directory, metadata->binary) ||
       !make_path(metadata->icon_path, sizeof(metadata->icon_path),
                  directory, MINIAPP_ICON_NAME) ||
       (metadata->package_format == 2u &&
        !make_path(metadata->resources_path,
                   sizeof(metadata->resources_path),
                   directory, MINIAPP_RESOURCES_NAME)))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    return CRAZYPOD_MINIAPP_OK;
}

static void sort_registry(void)
{
    int index;

    for(index = 1; index < registry_count; ++index) {
        struct crazypod_miniapp_metadata value = registry[index];
        int position = index;
        while(position > 0 &&
              strcmp(registry[position - 1].id, value.id) > 0) {
            registry[position] = registry[position - 1];
            --position;
        }
        registry[position] = value;
    }
}

static int rebuild_registry(void)
{
    DIR *directory;
    struct dirent *entry;

    registry_count = 0;
    directory = opendir(MINIAPP_ROOT);
    if(directory == NULL)
        return CRAZYPOD_MINIAPP_OK;
    while((entry = readdir(directory)) != NULL &&
          registry_count < CRAZYPOD_MINIAPP_MAX_APPS) {
        struct dirinfo info;
        struct crazypod_miniapp_metadata metadata;
        char path[MAX_PATH];

        if(!valid_id(entry->d_name))
            continue;
        info = dir_get_info(directory, entry);
        if((info.attribute & ATTR_DIRECTORY) == 0 ||
           !make_path(path, sizeof(path), MINIAPP_ROOT, entry->d_name) ||
           !validate_install_directory(path, entry->d_name,
                                       &metadata, NULL) ||
           populate_paths(&metadata, path) != CRAZYPOD_MINIAPP_OK)
            continue;
        registry[registry_count++] = metadata;
    }
    closedir(directory);
    sort_registry();
    return CRAZYPOD_MINIAPP_OK;
}

static int current_installed_version(const char *id, uint32_t *version)
{
    struct crazypod_miniapp_metadata metadata;
    char path[MAX_PATH];

    if(!make_path(path, sizeof(path), MINIAPP_ROOT, id) ||
       !dir_exists(path) ||
       !validate_install_directory(path, id, &metadata, NULL))
        return 0;
    *version = metadata.version_code;
    return 1;
}

static bool installed_file_matches_entry(
    const char *path, const struct cpk_reader *reader, int entry_index)
{
    uint8_t installed[MINIAPP_IO_BUFFER / 2u];
    uint8_t packaged[MINIAPP_IO_BUFFER / 2u];
    const struct cpk_entry *entry = &reader->entries[entry_index];
    uint32_t remaining = entry->size;
    uint32_t package_offset = entry->data_offset;
    int fd = open(path, O_RDONLY);

    if(fd < 0 || filesize(fd) != (off_t)entry->size) {
        if(fd >= 0)
            close(fd);
        return false;
    }
    while(remaining > 0) {
        uint32_t amount = remaining > sizeof(installed)
            ? (uint32_t)sizeof(installed) : remaining;
        if(!read_exact(fd, installed, amount) ||
           !read_at_exact(reader->fd, package_offset,
                          packaged, amount) ||
           memcmp(installed, packaged, amount) != 0) {
            close(fd);
            return false;
        }
        package_offset += amount;
        remaining -= amount;
    }
    close(fd);
    return true;
}

static bool installed_package_matches(const char *id,
                                      const struct cpk_reader *reader)
{
    static const char *const names[MINIAPP_CPK_MAX_ENTRIES] = {
        MINIAPP_MANIFEST_NAME,
        MINIAPP_EXPECTED_BINARY,
        MINIAPP_ICON_NAME,
        MINIAPP_SIGNATURE_NAME,
        MINIAPP_RESOURCES_NAME
    };
    struct install_record record;
    char directory[MAX_PATH];
    char path[MAX_PATH];
    int index;

    if(!make_path(directory, sizeof(directory), MINIAPP_ROOT, id) ||
       !validate_install_directory(
           directory, id, &install_workspace.verified_metadata, &record))
        return false;
    for(index = 0; index < reader->entry_count; ++index) {
        if((index < MINIAPP_CPK_V1_ENTRIES &&
            (record.files[index].size != reader->entries[index].size ||
             record.files[index].crc32 != reader->entries[index].crc32)) ||
           !make_path(path, sizeof(path), directory, names[index]) ||
           !installed_file_matches_entry(path, reader, index))
            return false;
    }
    return true;
}

static int commit_stage(const char *id)
{
    char final_path[MAX_PATH];
    char stage_path[MAX_PATH];
    char backup_path[MAX_PATH];
    bool had_final;

    if(!make_path(final_path, sizeof(final_path), MINIAPP_ROOT, id) ||
       !make_tagged_path(stage_path, sizeof(stage_path), "stage", id) ||
       !make_tagged_path(backup_path, sizeof(backup_path), "backup", id))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(dir_exists(backup_path) &&
       !remove_tree(backup_path, MINIAPP_REMOVE_DEPTH))
        return CRAZYPOD_MINIAPP_ERROR_IO;
    had_final = dir_exists(final_path);
    if(had_final && rename(final_path, backup_path) < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    if(rename(stage_path, final_path) < 0) {
        if(had_final)
            rename(backup_path, final_path);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    if(had_final && dir_exists(backup_path))
        remove_tree(backup_path, MINIAPP_REMOVE_DEPTH);
    return CRAZYPOD_MINIAPP_OK;
}

static int build_stage(const struct cpk_reader *reader,
                       const struct crazypod_miniapp_metadata *metadata)
{
    char stage_path[MAX_PATH];
    char file_path[MAX_PATH];
    int result;
    int index;
    const char *const names[MINIAPP_CPK_MAX_ENTRIES] = {
        MINIAPP_MANIFEST_NAME,
        MINIAPP_EXPECTED_BINARY,
        MINIAPP_ICON_NAME,
        MINIAPP_SIGNATURE_NAME,
        MINIAPP_RESOURCES_NAME
    };

    if(!make_tagged_path(stage_path, sizeof(stage_path),
                         "stage", metadata->id))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(dir_exists(stage_path) &&
       !remove_tree(stage_path, MINIAPP_REMOVE_DEPTH))
        return CRAZYPOD_MINIAPP_ERROR_IO;
    if(mkdir(stage_path) < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    for(index = 0; index < reader->entry_count; ++index) {
        if(!make_path(file_path, sizeof(file_path),
                      stage_path, names[index])) {
            result = CRAZYPOD_MINIAPP_ERROR_LIMIT;
            goto fail;
        }
        result = extract_entry(reader, index, file_path);
        if(result != CRAZYPOD_MINIAPP_OK)
            goto fail;
    }
    if(!write_install_record(stage_path, reader,
                             metadata->version_code)) {
        result = CRAZYPOD_MINIAPP_ERROR_IO;
        goto fail;
    }
    {
        if(!validate_install_directory(stage_path, metadata->id,
                                       &install_workspace.verified_metadata,
                                       NULL)) {
            result = CRAZYPOD_MINIAPP_ERROR_FORMAT;
            goto fail;
        }
    }
    return CRAZYPOD_MINIAPP_OK;

fail:
    remove_tree(stage_path, MINIAPP_REMOVE_DEPTH);
    return result;
}

int crazypod_miniapps_install(const char *package_path)
{
    struct cpk_reader reader;
    struct crazypod_miniapp_metadata *metadata =
        &install_workspace.metadata;
    char *manifest = install_workspace.manifest;
    uint32_t installed_version;
    uint32_t extracted_size = sizeof(struct install_record);
    bool same_version = false;
    int result;
    int index;

    if(package_path == NULL || package_path[0] != '/')
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(active_index >= 0 || install_in_progress)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    install_in_progress = true;
    result = cpk_open_shallow(package_path, &reader);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    result = cpk_validate_local_headers(&reader);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    result = cpk_read_entry(&reader, CPK_MANIFEST,
                            manifest, MINIAPP_MANIFEST_MAX);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    result = parse_manifest(manifest,
                            reader.entries[CPK_MANIFEST].size,
                            metadata);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    if((metadata->package_format == 1u &&
        reader.entry_count != MINIAPP_CPK_V1_ENTRIES) ||
       (metadata->package_format == 2u &&
        reader.entry_count != MINIAPP_CPK_V2_ENTRIES)) {
        result = CRAZYPOD_MINIAPP_ERROR_FORMAT;
        goto done;
    }
    if(current_installed_version(metadata->id, &installed_version)) {
        same_version = installed_version == metadata->version_code;
        if(installed_version > metadata->version_code) {
            result = CRAZYPOD_MINIAPP_DOWNGRADE_IGNORED;
            goto done;
        }
    }
    result = verify_cpk_signature(
        &reader, (const uint8_t *)manifest,
        reader.entries[CPK_MANIFEST].size);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    result = verify_cpk_sha256(&reader, CPK_BINARY,
                               metadata->binary_sha256);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    result = verify_cpk_sha256(&reader, CPK_ICON,
                               metadata->icon_sha256);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    if(metadata->package_format == 2u) {
        result = verify_cpk_sha256(
            &reader, CPK_RESOURCES, metadata->resources_sha256);
        if(result != CRAZYPOD_MINIAPP_OK)
            goto done;
        if(!validate_cpk_resources(&reader)) {
            result = CRAZYPOD_MINIAPP_ERROR_FORMAT;
            goto done;
        }
    }
    if(!valid_icon_header(&reader)) {
        result = CRAZYPOD_MINIAPP_ERROR_FORMAT;
        goto done;
    }
    result = validate_packaged_binary(&reader);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    if(same_version &&
       installed_package_matches(metadata->id, &reader)) {
        verified_app_mark(metadata->id, metadata->version_code);
        result = CRAZYPOD_MINIAPP_ALREADY_INSTALLED;
        goto done;
    }
    for(index = 0; index < reader.entry_count; ++index)
        extracted_size += reader.entries[index].size;
    if(!enough_space(extracted_size)) {
        result = CRAZYPOD_MINIAPP_ERROR_SPACE;
        goto done;
    }
    if(!ensure_directory("/.crazypod") ||
       !ensure_directory(MINIAPP_ROOT)) {
        result = CRAZYPOD_MINIAPP_ERROR_IO;
        goto done;
    }
    if(!recover_id(metadata->id)) {
        result = CRAZYPOD_MINIAPP_ERROR_IO;
        goto done;
    }
    result = build_stage(&reader, metadata);
    if(result != CRAZYPOD_MINIAPP_OK)
        goto done;
    result = commit_stage(metadata->id);
    if(result == CRAZYPOD_MINIAPP_OK) {
        verified_app_mark(metadata->id, metadata->version_code);
        rebuild_registry();
    }

done:
    cpk_close(&reader);
    install_in_progress = false;
    return result;
}

static bool has_cpk_extension(const char *name)
{
    size_t length = bounded_length(name, MAX_PATH);
    return length > 4 && length < MAX_PATH &&
           !(name[0] == '.' && name[1] == '_') &&
           strcmp(name + length - 4, ".cpk") == 0;
}

static int scan_package_directory(const char *path)
{
    DIR *directory = opendir(path);
    struct dirent *entry;
    int first_error = CRAZYPOD_MINIAPP_OK;
    int scanned = 0;

    if(directory == NULL)
        return CRAZYPOD_MINIAPP_OK;
    while((entry = readdir(directory)) != NULL &&
          scanned < MINIAPP_SCAN_LIMIT) {
        struct dirinfo info;
        char package[MAX_PATH];
        int result;

        if(!has_cpk_extension(entry->d_name))
            continue;
        info = dir_get_info(directory, entry);
        if((info.attribute & ATTR_DIRECTORY) != 0 ||
           !make_path(package, sizeof(package), path, entry->d_name))
            continue;
        ++scanned;
        result = crazypod_miniapps_install(package);
        if(result < 0 && first_error == CRAZYPOD_MINIAPP_OK)
            first_error = result;
    }
    closedir(directory);
    return first_error;
}

int crazypod_miniapps_rescan(void)
{
    int result;
    int user_result;

    if(active_index >= 0)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    verified_apps_clear();
    ensure_directory("/.crazypod");
    ensure_directory(MINIAPP_ROOT);
    ensure_directory(MINIAPP_DATA_ROOT);
    ensure_directory(MINIAPP_USER_ROOT);
    ensure_directory(MINIAPP_USER_INSTALL);
    recover_installs();
    rebuild_registry();
    result = scan_package_directory(MINIAPP_SYSTEM_PACKAGES);
    user_result = scan_package_directory(MINIAPP_USER_INSTALL);
    rebuild_registry();
    return result < 0 ? result : user_result;
}

int crazypod_miniapps_init(void)
{
    if(active_index >= 0)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    active_handle = NULL;
    active_ops = NULL;
    active_header = NULL;
    return crazypod_miniapps_rescan();
}

int crazypod_miniapps_count(void)
{
    return registry_count;
}

const struct crazypod_miniapp_metadata *
crazypod_miniapps_metadata(int index)
{
    if(index < 0 || index >= registry_count)
        return NULL;
    return &registry[index];
}

int crazypod_miniapps_find(const char *id)
{
    int index;

    if(!valid_id(id))
        return -1;
    for(index = 0; index < registry_count; ++index)
        if(strcmp(registry[index].id, id) == 0)
            return index;
    return -1;
}

static bool data_path(char *path, size_t capacity,
                      const char *id, const char *file)
{
    char directory[MAX_PATH];

    return valid_id(id) &&
           make_path(directory, sizeof(directory), MINIAPP_DATA_ROOT, id) &&
           make_path(path, capacity, directory, file);
}

static bool ensure_data_directory(const char *id)
{
    char directory[MAX_PATH];

    return valid_id(id) &&
           ensure_directory("/.crazypod") &&
           ensure_directory(MINIAPP_DATA_ROOT) &&
           make_path(directory, sizeof(directory), MINIAPP_DATA_ROOT, id) &&
           ensure_directory(directory);
}

static const struct crazypod_miniapp_metadata *current_metadata(void)
{
    return active_index >= 0 && active_index < registry_count
        ? &registry[active_index] : NULL;
}

static uint32_t host_epoch_seconds(void)
{
    struct tm *now = get_time();
    time_t epoch;

    if(now == NULL)
        return 0;
#if CONFIG_RTC
    if(!valid_time(now))
        return 0;
#endif
    epoch = mktime(now);
    return epoch > 0 ? (uint32_t)epoch : 0;
}

static uint32_t host_monotonic_ms(void)
{
    return (uint32_t)(((uint64_t)(uint32_t)current_tick * 1000u) / HZ);
}

static uint32_t state_header_checksum(const struct state_header *header)
{
    return crc_buffer(header, offsetof(struct state_header, checksum));
}

static int host_state_read(void *buffer, size_t capacity)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();
    struct state_header header;
    char path[MAX_PATH];
    int fd;

    if(metadata == NULL || buffer == NULL ||
       !data_path(path, sizeof(path), metadata->id, MINIAPP_STATE_NAME))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return 0;
    if(filesize(fd) < (off_t)sizeof(header) ||
       !read_exact(fd, &header, sizeof(header)) ||
       header.magic != MINIAPP_STATE_MAGIC ||
       header.version != MINIAPP_DISK_VERSION ||
       header.struct_size != sizeof(header) ||
       header.data_size > MINIAPP_STATE_MAX ||
       header.data_size > capacity ||
       filesize(fd) != (off_t)(sizeof(header) + header.data_size) ||
       header.checksum != state_header_checksum(&header) ||
       !read_exact(fd, buffer, header.data_size) ||
       crc_buffer(buffer, header.data_size) != header.data_crc32) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    }
    close(fd);
    return (int)header.data_size;
}

static int host_state_write(const void *buffer, size_t size)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();
    struct state_header header;
    char path[MAX_PATH];
    char temporary[MAX_PATH];
    int fd;
    bool success;

    if(metadata == NULL || (size > 0 && buffer == NULL) ||
       size > MINIAPP_STATE_MAX ||
       !ensure_data_directory(metadata->id) ||
       !data_path(path, sizeof(path), metadata->id, MINIAPP_STATE_NAME) ||
       !data_path(temporary, sizeof(temporary), metadata->id,
                  MINIAPP_STATE_TEMP_NAME))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    memset(&header, 0, sizeof(header));
    header.magic = MINIAPP_STATE_MAGIC;
    header.version = MINIAPP_DISK_VERSION;
    header.struct_size = sizeof(header);
    header.data_size = (uint32_t)size;
    header.data_crc32 = crc_buffer(buffer, size);
    header.checksum = state_header_checksum(&header);
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    success = write_exact(fd, &header, sizeof(header)) &&
              write_exact(fd, buffer, size) &&
              fsync(fd) == 0;
    if(close(fd) < 0)
        success = false;
    if(!success || rename(temporary, path) < 0) {
        remove(temporary);
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    }
    return CRAZYPOD_MINIAPP_OK;
}

static uint32_t alarm_checksum(const struct alarm_record *record)
{
    return crc_buffer(record, offsetof(struct alarm_record, checksum));
}

static int alarm_record_load(const char *id, const char *filename,
                             struct alarm_record *record)
{
    char path[MAX_PATH];
    int fd;
    bool valid;

    memset(record, 0, sizeof(*record));
    if(!data_path(path, sizeof(path), id, filename))
        return -1;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return 0;
    valid = filesize(fd) == (off_t)sizeof(*record) &&
            read_exact(fd, record, sizeof(*record));
    close(fd);
    if(!valid ||
       record->magic != MINIAPP_ALARM_MAGIC ||
       record->version != MINIAPP_DISK_VERSION ||
       record->struct_size != sizeof(*record) ||
       (record->flags & ~(ALARM_ACTIVE | ALARM_FIRED)) != 0 ||
       ((record->flags & ALARM_FIRED) != 0 &&
        (record->flags & ALARM_ACTIVE) == 0) ||
       ((record->flags & ALARM_ACTIVE) != 0 &&
        record->deadline_epoch == 0) ||
       record->checksum != alarm_checksum(record))
        return -1;
    return 1;
}

static bool alarm_record_save(const char *id, const char *filename,
                              const char *temporary_filename,
                              struct alarm_record *record)
{
    char path[MAX_PATH];
    char temporary[MAX_PATH];
    int fd;
    bool success;

    if(!ensure_data_directory(id) ||
       !data_path(path, sizeof(path), id, filename) ||
       !data_path(temporary, sizeof(temporary), id, temporary_filename))
        return false;
    record->magic = MINIAPP_ALARM_MAGIC;
    record->version = MINIAPP_DISK_VERSION;
    record->struct_size = sizeof(*record);
    record->checksum = alarm_checksum(record);
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, record, sizeof(*record)) &&
              fsync(fd) == 0;
    if(close(fd) < 0)
        success = false;
    if(!success || rename(temporary, path) < 0) {
        remove(temporary);
        return false;
    }
    return true;
}

static bool alarm_slot_filenames(
    uint8_t slot, bool notification,
    char *filename, size_t filename_capacity,
    char *temporary, size_t temporary_capacity)
{
    const char *base = notification ? "notification" : "alarm";

    if(slot >= CP_MINIAPP_ALARM_SLOT_COUNT ||
       filename == NULL || filename_capacity == 0 ||
       temporary == NULL || temporary_capacity == 0)
        return false;
    if(slot == 0) {
        snprintf(filename, filename_capacity, "%s",
                 notification ? MINIAPP_NOTIFICATION_NAME
                              : MINIAPP_ALARM_NAME);
        snprintf(temporary, temporary_capacity, "%s",
                 notification ? MINIAPP_NOTIFICATION_TEMP_NAME
                              : MINIAPP_ALARM_TEMP_NAME);
    }
    else {
        snprintf(filename, filename_capacity, "%s%u.bin",
                 base, (unsigned)slot);
        snprintf(temporary, temporary_capacity, "%s%u.tmp",
                 base, (unsigned)slot);
    }
    filename[filename_capacity - 1] = '\0';
    temporary[temporary_capacity - 1] = '\0';
    return true;
}

static int alarm_load_slot(
    const char *id, uint8_t slot, struct alarm_record *record)
{
    char filename[MINIAPP_ALARM_FILENAME_SIZE];
    char temporary[MINIAPP_ALARM_FILENAME_SIZE];

    if(!alarm_slot_filenames(slot, false, filename, sizeof(filename),
                             temporary, sizeof(temporary)))
        return -1;
    return alarm_record_load(id, filename, record);
}

static bool alarm_save_slot(
    const char *id, uint8_t slot, struct alarm_record *record)
{
    char filename[MINIAPP_ALARM_FILENAME_SIZE];
    char temporary[MINIAPP_ALARM_FILENAME_SIZE];

    return alarm_slot_filenames(slot, false, filename, sizeof(filename),
                                temporary, sizeof(temporary)) &&
           alarm_record_save(id, filename, temporary, record);
}

static int notification_load_slot(
    const char *id, uint8_t slot, struct alarm_record *record)
{
    char filename[MINIAPP_ALARM_FILENAME_SIZE];
    char temporary[MINIAPP_ALARM_FILENAME_SIZE];
    int loaded;

    if(!alarm_slot_filenames(slot, true, filename, sizeof(filename),
                             temporary, sizeof(temporary)))
        return -1;
    loaded = alarm_record_load(id, filename, record);
    if(loaded == 1 && record->flags != 0 &&
       record->flags != ALARM_ACTIVE &&
       record->flags != (ALARM_ACTIVE | ALARM_FIRED))
        return -1;
    return loaded;
}

static bool notification_save_slot(
    const char *id, uint8_t slot, struct alarm_record *record)
{
    char filename[MINIAPP_ALARM_FILENAME_SIZE];
    char temporary[MINIAPP_ALARM_FILENAME_SIZE];

    return alarm_slot_filenames(slot, true, filename, sizeof(filename),
                                temporary, sizeof(temporary)) &&
           alarm_record_save(id, filename, temporary, record);
}

static bool same_alarm_event(const struct alarm_record *left,
                             const struct alarm_record *right)
{
    return left->deadline_epoch == right->deadline_epoch &&
           left->token == right->token;
}

static int host_alarm_set_slot(
    uint8_t slot, uint32_t deadline_epoch, uint32_t token)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();
    struct alarm_record record;
    uint32_t now;
    int loaded;

    if(metadata == NULL ||
       slot >= CP_MINIAPP_ALARM_SLOT_COUNT ||
       deadline_epoch == 0 || token == 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    crazypod_miniapps_alarm_service(NULL);
    loaded = alarm_load_slot(metadata->id, slot, &record);
    now = host_epoch_seconds();
    if(loaded == 1 &&
       ((record.flags & ALARM_FIRED) != 0 ||
        ((record.flags & ALARM_ACTIVE) != 0 &&
         now != 0 && record.deadline_epoch <= now)))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    memset(&record, 0, sizeof(record));
    record.deadline_epoch = deadline_epoch;
    record.token = token;
    record.flags = ALARM_ACTIVE;
    return alarm_save_slot(metadata->id, slot, &record)
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_STATE;
}

static int host_alarm_set(uint32_t deadline_epoch, uint32_t token)
{
    return host_alarm_set_slot(0, deadline_epoch, token);
}

static void host_alarm_cancel_slot(uint8_t slot)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();
    struct alarm_record record;
    uint32_t now;
    int loaded;

    if(metadata == NULL || slot >= CP_MINIAPP_ALARM_SLOT_COUNT)
        return;
    crazypod_miniapps_alarm_service(NULL);
    loaded = alarm_load_slot(metadata->id, slot, &record);
    if(loaded != 1)
        return;
    now = host_epoch_seconds();
    if((record.flags & ALARM_ACTIVE) != 0 &&
       (record.flags & ALARM_FIRED) == 0 &&
       now != 0 && record.deadline_epoch <= now)
        return;
    memset(&record, 0, sizeof(record));
    alarm_save_slot(metadata->id, slot, &record);
}

static void host_alarm_cancel(void)
{
    host_alarm_cancel_slot(0);
}

static bool host_alarm_fired_slot(uint8_t slot, uint32_t *token)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();
    struct alarm_record record;

    if(metadata == NULL || slot >= CP_MINIAPP_ALARM_SLOT_COUNT)
        return false;
    crazypod_miniapps_alarm_service(NULL);
    if(alarm_load_slot(metadata->id, slot, &record) != 1 ||
       (record.flags & (ALARM_ACTIVE | ALARM_FIRED)) !=
           (ALARM_ACTIVE | ALARM_FIRED))
        return false;
    if(token != NULL)
        *token = record.token;
    return true;
}

static bool host_alarm_fired(uint32_t *token)
{
    return host_alarm_fired_slot(0, token);
}

static void host_alarm_acknowledge_slot(uint8_t slot)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();
    struct alarm_record record;

    if(metadata == NULL || slot >= CP_MINIAPP_ALARM_SLOT_COUNT)
        return;
    crazypod_miniapps_alarm_service(NULL);
    if(alarm_load_slot(metadata->id, slot, &record) != 1 ||
       (record.flags & ALARM_FIRED) == 0)
        return;
    memset(&record, 0, sizeof(record));
    (void)alarm_save_slot(metadata->id, slot, &record);
}

static void host_alarm_acknowledge(void)
{
    host_alarm_acknowledge_slot(0);
}

static void host_format_number(double value, char *buffer, size_t capacity)
{
    if(buffer == NULL || capacity == 0)
        return;
    snprintf(buffer, capacity, "%.12g", value);
    buffer[capacity - 1] = '\0';
}

static int host_system_info(struct cp_system_info *info)
{
    struct tm *now;
    int battery;
    int minutes;
    int audio;

    if(info == NULL ||
       info->struct_size < sizeof(struct cp_system_info))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    memset(info, 0, sizeof(*info));
    info->struct_size = sizeof(*info);
    battery = battery_level();
    minutes = battery_time();
    if(battery < 0)
        battery = -1;
    else if(battery > 100)
        battery = 100;
    if(minutes < 0)
        minutes = -1;
    else if(minutes > INT16_MAX)
        minutes = INT16_MAX;
    info->battery_percent = (int16_t)battery;
    info->battery_minutes = (int16_t)minutes;
    now = get_time();
#if CONFIG_RTC
    if(now != NULL && valid_time(now))
        info->flags |= CP_SYSTEM_TIME_VALID;
#else
    if(now != NULL)
        info->flags |= CP_SYSTEM_TIME_VALID;
#endif
#if CONFIG_CHARGING
    if(power_input_present())
        info->flags |= CP_SYSTEM_EXTERNAL_POWER;
#if CONFIG_CHARGING >= CHARGING_MONITOR
    if(charging_state())
        info->flags |= CP_SYSTEM_CHARGING;
#endif
#endif
#if defined(HAVE_USBSTACK)
    if(usb_inserted())
        info->flags |= CP_SYSTEM_USB_CONNECTED;
#endif
    audio = audio_status();
    if((audio & AUDIO_STATUS_PLAY) != 0)
        info->flags |= CP_SYSTEM_AUDIO_PLAYING;
    if((audio & AUDIO_STATUS_PAUSE) != 0)
        info->flags |= CP_SYSTEM_AUDIO_PAUSED;
    if(crazypod_state_reduce_motion())
        info->flags |= CP_SYSTEM_REDUCE_MOTION;
    return CRAZYPOD_MINIAPP_OK;
}

static void host_format_duration(
    uint32_t seconds, char *buffer, size_t capacity)
{
    uint32_t hours;
    uint32_t minutes;

    if(buffer == NULL || capacity == 0)
        return;
    hours = seconds / 3600u;
    minutes = (seconds % 3600u) / 60u;
    if(hours > 0)
        snprintf(buffer, capacity, "%lu:%02lu:%02lu",
                 (unsigned long)hours, (unsigned long)minutes,
                 (unsigned long)(seconds % 60u));
    else
        snprintf(buffer, capacity, "%lu:%02lu",
                 (unsigned long)minutes,
                 (unsigned long)(seconds % 60u));
    buffer[capacity - 1] = '\0';
}

static void host_format_datetime(
    uint32_t epoch_seconds, enum cp_datetime_format format,
    char *buffer, size_t capacity)
{
    time_t timestamp = (time_t)epoch_seconds;
    struct tm *value;

    if(buffer == NULL || capacity == 0)
        return;
    buffer[0] = '\0';
    if(epoch_seconds == 0 || format > CP_DATETIME_DATE_TIME)
        return;
    value = gmtime(&timestamp);
    if(value == NULL)
        return;
    if(format == CP_DATETIME_DATE)
        snprintf(buffer, capacity, "%04d-%02d-%02d",
                 value->tm_year + 1900, value->tm_mon + 1,
                 value->tm_mday);
    else if(format == CP_DATETIME_TIME)
        snprintf(buffer, capacity, "%02d:%02d",
                 value->tm_hour, value->tm_min);
    else
        snprintf(buffer, capacity, "%04d-%02d-%02d %02d:%02d",
                 value->tm_year + 1900, value->tm_mon + 1,
                 value->tm_mday, value->tm_hour, value->tm_min);
    buffer[capacity - 1] = '\0';
}

static int host_ui_toast(const char *text, uint32_t duration_ms)
{
    size_t length;
    long duration_ticks;

    if(current_metadata() == NULL || text == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    length = bounded_length(text, sizeof(active_toast));
    if(length == 0 || length >= sizeof(active_toast))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(duration_ms == 0)
        duration_ms = 2000;
    if(duration_ms < 500)
        duration_ms = 500;
    if(duration_ms > 5000)
        duration_ms = 5000;
    copy_text(active_toast, sizeof(active_toast), text, length);
    duration_ticks =
        (long)(((uint64_t)duration_ms * HZ + 999u) / 1000u);
    if(duration_ticks < 1)
        duration_ticks = 1;
    active_toast_until = current_tick + duration_ticks;
    active_ui_changed = true;
    return CRAZYPOD_MINIAPP_OK;
}

static int host_ui_request_close(void)
{
    if(current_metadata() == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    active_close_requested = true;
    return CRAZYPOD_MINIAPP_OK;
}

static bool host_ui_request_available(uint32_t request_id)
{
    return current_metadata() != NULL && request_id != 0 &&
           active_modal.type == HOST_MODAL_NONE &&
           !active_modal_result_ready;
}

static int host_ui_text_input(
    uint32_t request_id, const char *title,
    const char *initial_value, uint16_t max_bytes)
{
    size_t title_length;
    size_t value_length;
    size_t index;

    if(!host_ui_request_available(request_id) ||
       title == NULL || initial_value == NULL)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    title_length = bounded_length(title, sizeof(active_modal.title));
    value_length = bounded_length(
        initial_value, sizeof(active_modal.value));
    if(title_length == 0 ||
       title_length >= sizeof(active_modal.title) ||
       value_length >= sizeof(active_modal.value) ||
       max_bytes == 0 ||
       max_bytes >= sizeof(active_modal.value) ||
       value_length > max_bytes)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(!valid_utf8_text(title, true))
        return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    for(index = 0; index < value_length; ++index) {
        unsigned char value = (unsigned char)initial_value[index];
        if(value < 0x20 || value > 0x7e)
            return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    }
    memset(&active_modal, 0, sizeof(active_modal));
    active_modal.type = HOST_MODAL_TEXT;
    active_modal.request_id = request_id;
    active_modal.max_bytes = max_bytes;
    copy_text(active_modal.title, sizeof(active_modal.title),
              title, title_length);
    copy_text(active_modal.value, sizeof(active_modal.value),
              initial_value, value_length);
    active_ui_changed = true;
    return CRAZYPOD_MINIAPP_OK;
}

static int host_ui_choice(
    uint32_t request_id, const char *title,
    const struct cp_ui_choice_item *items,
    uint16_t item_count, int16_t selected_index)
{
    size_t title_length;
    uint16_t index;

    if(!host_ui_request_available(request_id) ||
       title == NULL || items == NULL)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    title_length = bounded_length(title, sizeof(active_modal.title));
    if(title_length == 0 ||
       title_length >= sizeof(active_modal.title) ||
       item_count == 0 || item_count > CP_MINIAPP_UI_CHOICE_MAX ||
       selected_index < 0 || selected_index >= (int16_t)item_count)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(!valid_utf8_text(title, true))
        return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    for(index = 0; index < item_count; ++index) {
        size_t length = bounded_length(
            items[index].label, sizeof(items[index].label));
        if(length == 0 || length >= sizeof(items[index].label))
            return CRAZYPOD_MINIAPP_ERROR_LIMIT;
        if(!valid_utf8_text(items[index].label, true))
            return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    }
    memset(&active_modal, 0, sizeof(active_modal));
    active_modal.type = HOST_MODAL_CHOICE;
    active_modal.request_id = request_id;
    active_modal.item_count = item_count;
    active_modal.selected = selected_index;
    copy_text(active_modal.title, sizeof(active_modal.title),
              title, title_length);
    memcpy(active_modal.items, items,
           (size_t)item_count * sizeof(items[0]));
    active_ui_changed = true;
    return CRAZYPOD_MINIAPP_OK;
}

static int host_ui_confirm(
    uint32_t request_id, const char *title,
    const char *message, const char *confirm_label)
{
    size_t title_length;
    size_t message_length;
    size_t label_length;

    if(!host_ui_request_available(request_id) ||
       title == NULL || message == NULL || confirm_label == NULL)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    title_length = bounded_length(title, sizeof(active_modal.title));
    message_length = bounded_length(
        message, sizeof(active_modal.message));
    label_length = bounded_length(
        confirm_label, sizeof(active_modal.confirm_label));
    if(title_length == 0 ||
       title_length >= sizeof(active_modal.title) ||
       message_length == 0 ||
       message_length >= sizeof(active_modal.message) ||
       label_length == 0 ||
       label_length >= sizeof(active_modal.confirm_label))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(!valid_utf8_text(title, true) ||
       !valid_utf8_text(message, true) ||
       !valid_utf8_text(confirm_label, true))
        return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    memset(&active_modal, 0, sizeof(active_modal));
    active_modal.type = HOST_MODAL_CONFIRM;
    active_modal.request_id = request_id;
    active_modal.selected = 0;
    copy_text(active_modal.title, sizeof(active_modal.title),
              title, title_length);
    copy_text(active_modal.message, sizeof(active_modal.message),
              message, message_length);
    copy_text(active_modal.confirm_label,
              sizeof(active_modal.confirm_label),
              confirm_label, label_length);
    active_ui_changed = true;
    return CRAZYPOD_MINIAPP_OK;
}

static void host_ui_finish(int status)
{
    memset(&active_modal_result, 0, sizeof(active_modal_result));
    active_modal_result.struct_size = sizeof(active_modal_result);
    active_modal_result.request_id = active_modal.request_id;
    active_modal_result.status = status;
    active_modal_result.selected_index =
        active_modal.type == HOST_MODAL_CHOICE ||
        active_modal.type == HOST_MODAL_CONFIRM
            ? active_modal.selected : -1;
    if(active_modal.type == HOST_MODAL_TEXT)
        cp_text_copy(active_modal_result.value,
                     sizeof(active_modal_result.value),
                     active_modal.value);
    active_modal_result_ready = true;
    memset(&active_modal, 0, sizeof(active_modal));
    active_ui_changed = true;
}

static int host_ui_poll_result(struct cp_ui_result *result)
{
    if(result == NULL || result->struct_size < sizeof(*result))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    if(!active_modal_result_ready)
        return 0;
    *result = active_modal_result;
    memset(&active_modal_result, 0, sizeof(active_modal_result));
    active_modal_result_ready = false;
    return 1;
}

static int host_ui_cancel(uint32_t request_id)
{
    if(active_modal.type == HOST_MODAL_NONE ||
       active_modal.request_id != request_id)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    host_ui_finish(CP_UI_RESULT_CANCELLED);
    return CRAZYPOD_MINIAPP_OK;
}

static int resource_find(
    const char *id, struct cp_resource_info *info,
    uint32_t *data_offset, int *fd_out)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();
    uint8_t header[MINIAPP_RESOURCE_HEADER_SIZE];
    uint8_t entry[MINIAPP_RESOURCE_ENTRY_SIZE];
    size_t id_length;
    uint16_t count;
    uint16_t index;
    int fd;

    if(metadata == NULL || metadata->package_format != 2u ||
       id == NULL || info == NULL || data_offset == NULL ||
       fd_out == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    id_length = bounded_length(id, CP_MINIAPP_RESOURCE_ID_SIZE);
    if(id_length == 0 || id_length >= CP_MINIAPP_RESOURCE_ID_SIZE)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    fd = open(metadata->resources_path, O_RDONLY);
    if(fd < 0 ||
       !read_at_exact(fd, 0, header, sizeof(header))) {
        if(fd >= 0)
            close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    count = read_le16(header + 6);
    for(index = 0; index < count; ++index) {
        int comparison;

        if(!read_at_exact(
               fd, MINIAPP_RESOURCE_HEADER_SIZE +
                       (uint32_t)index * MINIAPP_RESOURCE_ENTRY_SIZE,
               entry, sizeof(entry)) ||
           memchr(entry, '\0', 32) == NULL) {
            close(fd);
            return CRAZYPOD_MINIAPP_ERROR_IO;
        }
        comparison = strcmp(id, (const char *)entry);
        if(comparison > 0)
            continue;
        if(comparison < 0) {
            close(fd);
            return CRAZYPOD_MINIAPP_ERROR_FORMAT;
        }
        memset(info, 0, sizeof(*info));
        info->struct_size = sizeof(*info);
        info->type = entry[32];
        info->width = read_le16(entry + 34);
        info->height = read_le16(entry + 36);
        info->size = read_le32(entry + 44);
        info->crc32 = read_le32(entry + 48);
        *data_offset = read_le32(entry + 40);
        *fd_out = fd;
        return CRAZYPOD_MINIAPP_OK;
    }
    close(fd);
    return CRAZYPOD_MINIAPP_ERROR_FORMAT;
}

static int host_resource_stat(
    const char *id, struct cp_resource_info *info)
{
    struct cp_resource_info found;
    uint32_t offset;
    int fd;
    int result;

    if(info == NULL || info->struct_size < sizeof(*info))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    result = resource_find(id, &found, &offset, &fd);
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    close(fd);
    *info = found;
    return CRAZYPOD_MINIAPP_OK;
}

static int host_resource_read(
    const char *id, uint32_t offset, void *buffer, size_t capacity)
{
    struct cp_resource_info info;
    uint32_t data_offset;
    uint32_t amount;
    int fd;
    int result;

    if(buffer == NULL && capacity > 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    result = resource_find(id, &info, &data_offset, &fd);
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    if(offset > info.size) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    }
    amount = info.size - offset;
    if(amount > capacity)
        amount = (uint32_t)capacity;
    if(amount > INT32_MAX ||
       (amount > 0 &&
        !read_at_exact(fd, data_offset + offset, buffer, amount))) {
        close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    close(fd);
    return (int)amount;
}

int crazypod_miniapps_resource_stat(
    const char *id, struct cp_resource_info *info)
{
    return host_resource_stat(id, info);
}

int crazypod_miniapps_resource_read(
    const char *id, uint32_t offset, void *buffer, size_t capacity)
{
    return host_resource_read(id, offset, buffer, capacity);
}

static int host_now_playing(struct cp_now_playing *info)
{
    const struct mp3entry *track;
    int status;

    if(info == NULL || info->struct_size < sizeof(*info))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    memset(info, 0, sizeof(*info));
    info->struct_size = sizeof(*info);
    status = audio_status();
    track = audio_current_track();
    if(track == NULL)
        return CRAZYPOD_MINIAPP_OK;
    info->flags |= CP_NOW_PLAYING_AVAILABLE;
    if((status & AUDIO_STATUS_PLAY) != 0)
        info->flags |= CP_NOW_PLAYING_PLAYING;
    if((status & AUDIO_STATUS_PAUSE) != 0)
        info->flags |= CP_NOW_PLAYING_PAUSED;
    info->elapsed_ms = track->elapsed;
    info->duration_ms = track->length;
    cp_text_copy(info->title, sizeof(info->title), track->title);
    cp_text_copy(info->artist, sizeof(info->artist), track->artist);
    cp_text_copy(info->album, sizeof(info->album), track->album);
    return CRAZYPOD_MINIAPP_OK;
}

static const struct cp_host_api host_api = {
    .abi_version = CP_MINIAPP_ABI_VERSION,
    .struct_size = sizeof(struct cp_host_api),
    .epoch_seconds = host_epoch_seconds,
    .monotonic_ms = host_monotonic_ms,
    .state_read = host_state_read,
    .state_write = host_state_write,
    .alarm_set = host_alarm_set,
    .alarm_cancel = host_alarm_cancel,
    .alarm_fired = host_alarm_fired,
    .alarm_acknowledge = host_alarm_acknowledge,
    .format_number = host_format_number,
    .capabilities =
        CP_CAP_SYSTEM_INFO |
        CP_CAP_FORMAT_DURATION |
        CP_CAP_FORMAT_DATETIME |
        CP_CAP_MULTIPLE_ALARMS |
        CP_CAP_UI_TOAST |
        CP_CAP_UI_REQUEST_CLOSE |
        CP_CAP_DRAW_DIVIDER |
        CP_CAP_DRAW_PROGRESS |
        CP_CAP_UI_MODAL |
        CP_CAP_RESOURCES |
        CP_CAP_DRAW_BITMAP |
        CP_CAP_NOW_PLAYING,
    .system_info = host_system_info,
    .format_duration = host_format_duration,
    .format_datetime = host_format_datetime,
    .alarm_set_slot = host_alarm_set_slot,
    .alarm_cancel_slot = host_alarm_cancel_slot,
    .alarm_fired_slot = host_alarm_fired_slot,
    .alarm_acknowledge_slot = host_alarm_acknowledge_slot,
    .ui_toast = host_ui_toast,
    .ui_request_close = host_ui_request_close,
    .ui_text_input = host_ui_text_input,
    .ui_choice = host_ui_choice,
    .ui_confirm = host_ui_confirm,
    .ui_poll_result = host_ui_poll_result,
    .ui_cancel = host_ui_cancel,
    .resource_stat = host_resource_stat,
    .resource_read = host_resource_read,
    .now_playing = host_now_playing
};

bool crazypod_miniapps_alarm_service(
    struct crazypod_miniapp_alarm *alarm)
{
    uint32_t now = host_epoch_seconds();
    struct crazypod_miniapp_alarm first;
    bool found = false;
    int index;

    if(now == 0)
        return false;
    memset(&first, 0, sizeof(first));
    for(index = 0; index < registry_count; ++index) {
        uint8_t slot;

        for(slot = 0; slot < CP_MINIAPP_ALARM_SLOT_COUNT; ++slot) {
            struct alarm_record record;
            struct alarm_record notification;
            int loaded =
                alarm_load_slot(registry[index].id, slot, &record);
            int notification_loaded = notification_load_slot(
                registry[index].id, slot, &notification);

            if(notification_loaded == 1 &&
               notification.flags == ALARM_ACTIVE) {
                bool ready = false;

                if(loaded == 1 &&
                   (record.flags & ALARM_ACTIVE) != 0 &&
                   same_alarm_event(&record, &notification)) {
                    if((record.flags & ALARM_FIRED) != 0)
                        ready = true;
                    else {
                        record.flags |= ALARM_FIRED;
                        ready = alarm_save_slot(
                            registry[index].id, slot, &record);
                    }
                }
                else {
                    ready = true;
                }
                if(ready) {
                    notification.flags |= ALARM_FIRED;
                    if(!notification_save_slot(
                           registry[index].id, slot, &notification))
                        notification.flags = ALARM_ACTIVE;
                }
            }

            if(loaded == 1 &&
               (record.flags & ALARM_ACTIVE) != 0 &&
               (record.flags & ALARM_FIRED) == 0 &&
               record.deadline_epoch <= now) {
                bool queued = false;

                if(notification_loaded != 1 ||
                   notification.flags == 0) {
                    notification = record;
                    notification.flags = ALARM_ACTIVE;
                    queued = notification_save_slot(
                        registry[index].id, slot, &notification);
                }
                else if(same_alarm_event(&record, &notification)) {
                    queued = true;
                }
                if(queued) {
                    record.flags |= ALARM_FIRED;
                    if(alarm_save_slot(
                           registry[index].id, slot, &record) &&
                       notification.flags == ALARM_ACTIVE) {
                        notification.flags |= ALARM_FIRED;
                        notification_save_slot(
                            registry[index].id, slot, &notification);
                    }
                }
            }
            if(alarm != NULL && !found &&
               notification_load_slot(
                   registry[index].id, slot, &notification) == 1 &&
               notification.flags ==
                   (ALARM_ACTIVE | ALARM_FIRED)) {
                copy_text(first.id, sizeof(first.id),
                          registry[index].id,
                          strlen(registry[index].id));
                copy_text(first.name, sizeof(first.name),
                          registry[index].name,
                          strlen(registry[index].name));
                first.deadline_epoch = notification.deadline_epoch;
                first.token = notification.token;
                first.fired = true;
                found = true;
            }
        }
    }
    if(found)
        *alarm = first;
    return found;
}

int crazypod_miniapps_alarm_acknowledge(const char *id)
{
    struct alarm_record record;
    int loaded;

    if(crazypod_miniapps_find(id) < 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    crazypod_miniapps_alarm_service(NULL);
    loaded = alarm_load_slot(id, 0, &record);
    if(loaded < 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    if(loaded == 0 || (record.flags & ALARM_FIRED) == 0)
        return CRAZYPOD_MINIAPP_OK;
    memset(&record, 0, sizeof(record));
    return alarm_save_slot(id, 0, &record)
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_alarm_delivery_acknowledge(
    const char *id, uint32_t deadline_epoch, uint32_t token)
{
    struct alarm_record notification;
    uint8_t slot;

    if(crazypod_miniapps_find(id) < 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    for(slot = 0; slot < CP_MINIAPP_ALARM_SLOT_COUNT; ++slot) {
        int loaded = notification_load_slot(id, slot, &notification);

        if(loaded < 0)
            return CRAZYPOD_MINIAPP_ERROR_STATE;
        if(loaded != 1 || notification.flags == 0)
            continue;
        if(notification.flags ==
               (ALARM_ACTIVE | ALARM_FIRED) &&
           notification.deadline_epoch == deadline_epoch &&
           notification.token == token) {
            memset(&notification, 0, sizeof(notification));
            return notification_save_slot(id, slot, &notification)
                ? CRAZYPOD_MINIAPP_OK
                : CRAZYPOD_MINIAPP_ERROR_STATE;
        }
    }
    return CRAZYPOD_MINIAPP_OK;
}

static int verify_file_sha256(const char *path, uint32_t expected_size,
                              uint32_t expected_crc32,
                              const uint8_t expected_digest[32])
{
    struct crazypod_sha256_context context;
    uint8_t buffer[MINIAPP_IO_BUFFER];
    uint8_t digest[32];
    uint32_t remaining = expected_size;
    uint32_t crc = 0xffffffffu;
    int fd = open(path, O_RDONLY);

    if(fd < 0 || filesize(fd) != (off_t)expected_size) {
        if(fd >= 0)
            close(fd);
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
    if(crc_finish(crc) != expected_crc32)
        return CRAZYPOD_MINIAPP_ERROR_CRC;
    return memcmp(digest, expected_digest, sizeof(digest)) == 0
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
}

static int verify_resource_file_sha256(
    const char *path, const uint8_t expected_digest[32])
{
    struct crazypod_sha256_context context;
    uint8_t buffer[MINIAPP_IO_BUFFER];
    uint8_t digest[32];
    int fd = open(path, O_RDONLY);
    off_t size;
    off_t remaining;

    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    if(size < (off_t)MINIAPP_RESOURCE_HEADER_SIZE ||
       size > (off_t)MINIAPP_RESOURCES_MAX) {
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
        ? CRAZYPOD_MINIAPP_OK
        : CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
}

static int verify_installed_signature(
    const struct crazypod_miniapp_metadata *metadata)
{
    struct install_record record;
    uint8_t *manifest = (uint8_t *)install_workspace.manifest;
    uint8_t signature[CRAZYPOD_MINIAPP_SIGNATURE_SIZE];
    char path[MAX_PATH];
    int fd;
    int result;

    if(!read_install_record(metadata->install_path, &record))
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(record.files[CPK_MANIFEST].size > MINIAPP_MANIFEST_MAX ||
       !make_path(path, sizeof(path), metadata->install_path,
                  MINIAPP_MANIFEST_NAME))
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    fd = open(path, O_RDONLY);
    if(fd < 0 ||
       filesize(fd) != (off_t)record.files[CPK_MANIFEST].size ||
       !read_exact(fd, manifest, record.files[CPK_MANIFEST].size)) {
        if(fd >= 0)
            close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    close(fd);
    if(crc_buffer(manifest, record.files[CPK_MANIFEST].size) !=
       record.files[CPK_MANIFEST].crc32)
        return CRAZYPOD_MINIAPP_ERROR_CRC;
    if(!make_path(path, sizeof(path), metadata->install_path,
                  MINIAPP_SIGNATURE_NAME))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    fd = open(path, O_RDONLY);
    if(fd < 0 ||
       filesize(fd) !=
           CRAZYPOD_MINIAPP_SIGNATURE_SIZE ||
       !read_exact(fd, signature, sizeof(signature))) {
        if(fd >= 0)
            close(fd);
        return CRAZYPOD_MINIAPP_ERROR_IO;
    }
    close(fd);
    if(!crazypod_ed25519_verify(
           signature, manifest, record.files[CPK_MANIFEST].size,
           crazypod_miniapp_development_public_key))
        return CRAZYPOD_MINIAPP_ERROR_SIGNATURE;
    result = verify_file_sha256(
        metadata->binary_path, record.files[CPK_BINARY].size,
        record.files[CPK_BINARY].crc32, metadata->binary_sha256);
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    result = verify_file_sha256(
        metadata->icon_path, record.files[CPK_ICON].size,
        record.files[CPK_ICON].crc32, metadata->icon_sha256);
    if(result == CRAZYPOD_MINIAPP_OK &&
       metadata->package_format == 2u)
        result = verify_resource_file_sha256(
            metadata->resources_path, metadata->resources_sha256);
    return result;
}

static bool runtime_string_matches(
    const struct miniapp_binary_header_runtime *header,
    const char *runtime, const char *expected, size_t maximum)
{
    size_t length;

    if(runtime == NULL || expected == NULL)
        return false;
#if CONFIG_BINFMT == BINFMT_ROCK
    if((uintptr_t)runtime < (uintptr_t)header->lc_header.load_addr ||
       (uintptr_t)runtime >= (uintptr_t)header->lc_header.end_addr)
        return false;
    maximum = (size_t)((uintptr_t)header->lc_header.end_addr -
                       (uintptr_t)runtime);
#else
    (void)header;
#endif
    length = bounded_length(runtime, maximum);
    return length < maximum && strcmp(runtime, expected) == 0;
}

static bool runtime_ops_valid(
    const struct miniapp_binary_header_runtime *header,
    const struct cp_miniapp_ops *ops,
    const struct crazypod_miniapp_metadata *metadata)
{
    if(ops == NULL || ops->abi_version != CP_MINIAPP_ABI_VERSION ||
       ops->struct_size < sizeof(*ops) ||
       ops->open == NULL || ops->close == NULL ||
       ops->event == NULL || ops->tick == NULL ||
       ops->render == NULL)
        return false;
#if CONFIG_BINFMT == BINFMT_ROCK
    uintptr_t ops_address = (uintptr_t)ops;
    uintptr_t load = (uintptr_t)header->lc_header.load_addr;
    uintptr_t end = (uintptr_t)header->lc_header.end_addr;

    if(ops_address < load || ops_address > end ||
       sizeof(*ops) > end - ops_address)
        return false;
#endif
    return runtime_string_matches(header, ops->id, metadata->id,
                                  CRAZYPOD_MINIAPP_ID_SIZE) &&
           runtime_string_matches(header, ops->name, metadata->name,
                                  CRAZYPOD_MINIAPP_NAME_SIZE) &&
           runtime_string_matches(header, ops->version, metadata->version,
                                  CRAZYPOD_MINIAPP_VERSION_SIZE);
}

static int validate_installed_native_header(
    const struct crazypod_miniapp_metadata *metadata,
    uint32_t *file_size_out)
{
#if CONFIG_BINFMT == BINFMT_ROCK
    struct miniapp_binary_header_runtime header;
    int fd = open(metadata->binary_path, O_RDONLY);
    off_t size;
    bool valid;

    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    valid = size >= (off_t)sizeof(header) &&
            size <= (off_t)MINIAPP_BINARY_MAX &&
            read_exact(fd, &header, sizeof(header)) &&
            native_header_valid(&header, (uint32_t)size);
    close(fd);
    if(valid && file_size_out != NULL)
        *file_size_out = (uint32_t)size;
    return valid ? CRAZYPOD_MINIAPP_OK
                 : CRAZYPOD_MINIAPP_ERROR_ABI;
#else
    (void)metadata;
    if(file_size_out != NULL)
        *file_size_out = 0;
    return CRAZYPOD_MINIAPP_OK;
#endif
}

int crazypod_miniapps_open(int index)
{
    const struct crazypod_miniapp_metadata *metadata;
    struct miniapp_binary_header_runtime *header;
    const struct cp_miniapp_ops *ops;
    void *handle;
    uint32_t binary_file_size;
    int result;

    if(index < 0 || index >= registry_count)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(active_index >= 0)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    metadata = &registry[index];
    if(!verified_app_matches(metadata->id, metadata->version_code)) {
        result = verify_installed_signature(metadata);
        if(result != CRAZYPOD_MINIAPP_OK)
            return result;
        verified_app_mark(metadata->id, metadata->version_code);
    }
    result = validate_installed_native_header(metadata, &binary_file_size);
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
#if CONFIG_BINFMT == BINFMT_ROCK
    handle = lc_open(metadata->binary_path, pluginbuf,
                     PLUGIN_BUFFER_SIZE);
#else
    handle = lc_open(metadata->binary_path, NULL, 0);
#endif
    if(handle == NULL)
        return CRAZYPOD_MINIAPP_ERROR_ABI;
    header = lc_get_header(handle);
    if(header == NULL ||
       header->lc_header.magic != CP_MINIAPP_BINARY_MAGIC ||
       header->lc_header.target_id != TARGET_ID ||
       header->lc_header.api_version != CP_MINIAPP_ABI_VERSION ||
       header->host_api_size < CP_HOST_API_V1_SIZE ||
       header->host_api_size > sizeof(host_api) ||
       header->ops_size != sizeof(struct cp_miniapp_ops) ||
       header->entry == NULL ||
       !native_header_valid(header,
                            binary_file_size)) {
        lc_close(handle);
        return CRAZYPOD_MINIAPP_ERROR_ABI;
    }
#if CONFIG_BINFMT == BINFMT_ROCK
    memset(header->bss_start, 0,
           (size_t)(header->lc_header.end_addr - header->bss_start));
#endif
    active_index = index;
    active_handle = handle;
    active_header = header;
    active_close_requested = false;
    active_ui_changed = false;
    active_toast[0] = '\0';
    active_toast_until = 0;
    memset(&active_modal, 0, sizeof(active_modal));
    memset(&active_modal_result, 0, sizeof(active_modal_result));
    active_modal_result_ready = false;
    ops = header->entry(&host_api);
    if(!runtime_ops_valid(header, ops, metadata)) {
        active_index = -1;
        active_handle = NULL;
        active_header = NULL;
        lc_close(handle);
        return CRAZYPOD_MINIAPP_ERROR_ABI;
    }
    active_ops = ops;
    (active_ops->open)();
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapps_open_id(const char *id)
{
    int index = crazypod_miniapps_find(id);
    return index >= 0 ? crazypod_miniapps_open(index)
                      : CRAZYPOD_MINIAPP_ERROR_FORMAT;
}

void crazypod_miniapps_close(void)
{
    void *handle = active_handle;

    if(active_index < 0)
        return;
    if(active_ops != NULL && active_ops->close != NULL)
        (active_ops->close)();
    active_index = -1;
    active_ops = NULL;
    active_header = NULL;
    active_handle = NULL;
    active_close_requested = false;
    active_ui_changed = false;
    active_toast[0] = '\0';
    active_toast_until = 0;
    memset(&active_modal, 0, sizeof(active_modal));
    memset(&active_modal_result, 0, sizeof(active_modal_result));
    active_modal_result_ready = false;
    if(handle != NULL)
        lc_close(handle);
}

bool crazypod_miniapps_is_open(void)
{
    return active_index >= 0 && active_ops != NULL;
}

int crazypod_miniapps_current(void)
{
    return crazypod_miniapps_is_open() ? active_index : -1;
}

bool crazypod_miniapps_take_close_request(void)
{
    bool requested =
        crazypod_miniapps_is_open() && active_close_requested;

    active_close_requested = false;
    return requested;
}

bool crazypod_miniapps_take_ui_refresh(void)
{
    bool changed;

    if(active_toast[0] != '\0' &&
       !TIME_BEFORE(current_tick, active_toast_until)) {
        active_toast[0] = '\0';
        active_toast_until = 0;
        active_ui_changed = true;
    }
    changed = crazypod_miniapps_is_open() && active_ui_changed;
    active_ui_changed = false;
    return changed;
}

bool crazypod_miniapps_toast(char *buffer, size_t capacity)
{
    if(buffer == NULL || capacity == 0)
        return false;
    buffer[0] = '\0';
    if(!crazypod_miniapps_is_open() ||
       active_toast[0] == '\0' ||
       !TIME_BEFORE(current_tick, active_toast_until)) {
        active_toast[0] = '\0';
        active_toast_until = 0;
        return false;
    }
    snprintf(buffer, capacity, "%s", active_toast);
    buffer[capacity - 1] = '\0';
    return true;
}

static bool modal_event(const struct cp_input_event *event)
{
    static const char characters[] =
        " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        ".,:+-*/()=%!?_";
    int direction = 0;

    if(active_modal.type == HOST_MODAL_NONE)
        return false;
    if(event->type == CP_INPUT_WHEEL_CLOCKWISE)
        direction = 1;
    else if(event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE)
        direction = -1;
    if(active_modal.type == HOST_MODAL_TEXT) {
        size_t length = strlen(active_modal.value);

        if(direction != 0) {
            int next = (int)active_modal.character + direction;
            int count = (int)sizeof(characters) - 1;
            if(next < 0)
                next = count - 1;
            if(next >= count)
                next = 0;
            active_modal.character = (uint8_t)next;
        } else if(event->type == CP_INPUT_SELECT) {
            if(length < active_modal.max_bytes &&
               length + 1 < sizeof(active_modal.value)) {
                active_modal.value[length] =
                    characters[active_modal.character];
                active_modal.value[length + 1] = '\0';
            }
        } else if(event->type == CP_INPUT_LEFT ||
                  event->type == CP_INPUT_PLAY) {
            if(length > 0)
                active_modal.value[length - 1] = '\0';
        } else if(event->type == CP_INPUT_RIGHT) {
            host_ui_finish(CP_UI_RESULT_ACCEPTED);
        } else if(event->type == CP_INPUT_MENU) {
            host_ui_finish(CP_UI_RESULT_CANCELLED);
        }
    } else if(active_modal.type == HOST_MODAL_CHOICE) {
        if(direction != 0) {
            int next = active_modal.selected + direction;
            if(next < 0)
                next = 0;
            if(next >= active_modal.item_count)
                next = active_modal.item_count - 1;
            active_modal.selected = (int16_t)next;
        } else if(event->type == CP_INPUT_SELECT ||
                  event->type == CP_INPUT_RIGHT) {
            host_ui_finish(CP_UI_RESULT_ACCEPTED);
        } else if(event->type == CP_INPUT_MENU ||
                  event->type == CP_INPUT_LEFT) {
            host_ui_finish(CP_UI_RESULT_CANCELLED);
        }
    } else if(active_modal.type == HOST_MODAL_CONFIRM) {
        if(direction != 0 || event->type == CP_INPUT_LEFT ||
           event->type == CP_INPUT_RIGHT)
            active_modal.selected = active_modal.selected == 0 ? 1 : 0;
        else if(event->type == CP_INPUT_SELECT)
            host_ui_finish(active_modal.selected == 1
                               ? CP_UI_RESULT_ACCEPTED
                               : CP_UI_RESULT_CANCELLED);
        else if(event->type == CP_INPUT_MENU)
            host_ui_finish(CP_UI_RESULT_CANCELLED);
    }
    active_ui_changed = true;
    return true;
}

static struct cp_draw_command *modal_command(
    struct cp_scene *scene, enum cp_draw_type type,
    int x, int y, int width, int height)
{
    struct cp_draw_command *command = cp_scene_add(scene, type);

    if(command == NULL)
        return NULL;
    command->x = x;
    command->y = y;
    command->width = width;
    command->height = height;
    return command;
}

static void modal_text(
    struct cp_scene *scene, const char *text,
    int x, int y, int width, enum cp_font_token font,
    enum cp_text_align align, bool focused)
{
    struct cp_draw_command *command =
        modal_command(scene, CP_DRAW_TEXT, x, y, width, 20);

    if(command == NULL)
        return;
    command->font = font;
    command->align = align;
    command->foreground = focused
        ? CP_COLOR_ACCENT_FOREGROUND : CP_COLOR_WHITE;
    if(focused)
        command->flags |= CP_DRAW_FOCUSED;
    cp_text_copy(command->text, sizeof(command->text), text);
}

static void modal_wrapped_text(
    struct cp_scene *scene, const char *text,
    int x, int y, int width)
{
    const char *cursor = text;
    int line;

    for(line = 0; line < 3 && cursor[0] != '\0'; ++line) {
        char value[CP_MINIAPP_TEXT_SIZE];
        size_t remaining = strlen(cursor);
        size_t amount = remaining < sizeof(value) - 1
            ? remaining : sizeof(value) - 1;

        while(amount > 0 && remaining > amount &&
              (((unsigned char)cursor[amount] & 0xc0) == 0x80))
            --amount;
        if(amount == 0)
            return;
        memcpy(value, cursor, amount);
        value[amount] = '\0';
        modal_text(scene, value, x, y + line * 22, width,
                   CP_FONT_BODY, CP_ALIGN_CENTER, false);
        cursor += amount;
    }
}

static void render_modal(struct cp_scene *scene)
{
    struct cp_draw_command *panel;

    if(active_modal.type == HOST_MODAL_NONE)
        return;
    if(scene->command_count > CP_MINIAPP_MAX_COMMANDS - 14)
        scene->command_count = CP_MINIAPP_MAX_COMMANDS - 14;
    panel = modal_command(scene, CP_DRAW_RECT, 16, 34, 288, 194);
    if(panel == NULL)
        return;
    panel->background = CP_COLOR_SURFACE_RAISED;
    panel->border = CP_COLOR_WHITE;
    panel->border_width = 1;
    panel->border_opacity = 80;
    panel->radius = 14;
    modal_text(scene, active_modal.title, 30, 48, 260,
               CP_FONT_TITLE, CP_ALIGN_LEFT, false);

    if(active_modal.type == HOST_MODAL_TEXT) {
        char character[8];
        char counter[24];
        struct cp_draw_command *field =
            modal_command(scene, CP_DRAW_RECT, 28, 80, 264, 52);

        if(field != NULL) {
            field->background = CP_COLOR_SURFACE;
            field->radius = 8;
        }
        modal_text(scene, active_modal.value, 38, 96, 244,
                   CP_FONT_BODY, CP_ALIGN_LEFT, false);
        character[0] = '[';
        character[1] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
                       ".,:+-*/()=%!?_"[active_modal.character];
        character[2] = ']';
        character[3] = '\0';
        modal_text(scene, character, 112, 143, 96,
                   CP_FONT_DISPLAY, CP_ALIGN_CENTER, true);
        snprintf(counter, sizeof(counter), "%lu/%u",
                 (unsigned long)strlen(active_modal.value),
                 (unsigned)active_modal.max_bytes);
        modal_text(scene, counter, 220, 151, 58,
                   CP_FONT_CAPTION, CP_ALIGN_RIGHT, false);
        modal_text(scene, "SELECT ADD  PLAY DELETE", 34, 184, 252,
                   CP_FONT_CAPTION, CP_ALIGN_CENTER, false);
        modal_text(scene, "RIGHT DONE  MENU CANCEL", 34, 204, 252,
                   CP_FONT_CAPTION, CP_ALIGN_CENTER, false);
    } else if(active_modal.type == HOST_MODAL_CHOICE) {
        int start = active_modal.selected - 2;
        int row;

        if(start < 0)
            start = 0;
        if(start > active_modal.item_count - 5)
            start = active_modal.item_count > 5
                ? active_modal.item_count - 5 : 0;
        for(row = 0; row < 5 && start + row < active_modal.item_count;
            ++row) {
            int item = start + row;
            struct cp_draw_command *background =
                modal_command(scene, CP_DRAW_RECT,
                              28, 78 + row * 27, 264, 23);
            bool focused = item == active_modal.selected;

            if(background != NULL) {
                background->background = focused
                    ? CP_COLOR_ACCENT : CP_COLOR_SURFACE;
                background->opacity = focused ? 255 : 160;
                background->radius = 7;
            }
            modal_text(scene, active_modal.items[item].label,
                       38, 81 + row * 27, 244,
                       CP_FONT_LABEL, CP_ALIGN_LEFT, focused);
        }
    } else {
        struct cp_draw_command *cancel =
            modal_command(scene, CP_DRAW_RECT, 34, 164, 116, 40);
        struct cp_draw_command *confirm =
            modal_command(scene, CP_DRAW_RECT, 170, 164, 116, 40);

        modal_wrapped_text(
            scene, active_modal.message, 34, 84, 252);
        if(cancel != NULL) {
            cancel->background = active_modal.selected == 0
                ? CP_COLOR_ACCENT : CP_COLOR_SURFACE;
            cancel->radius = 8;
        }
        if(confirm != NULL) {
            confirm->background = active_modal.selected == 1
                ? CP_COLOR_ACCENT : CP_COLOR_SURFACE;
            confirm->radius = 8;
        }
        modal_text(scene, "Cancel", 34, 176, 116,
                   CP_FONT_LABEL, CP_ALIGN_CENTER,
                   active_modal.selected == 0);
        modal_text(scene, active_modal.confirm_label, 170, 176, 116,
                   CP_FONT_LABEL, CP_ALIGN_CENTER,
                   active_modal.selected == 1);
    }
}

bool crazypod_miniapps_event(const struct cp_input_event *event)
{
    if(!crazypod_miniapps_is_open() || event == NULL ||
       event->struct_size < sizeof(*event) ||
       event->type > CP_INPUT_MENU ||
       ((event->type == CP_INPUT_WHEEL_CLOCKWISE ||
         event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE) &&
        event->steps == 0))
        return false;
    if(active_modal.type != HOST_MODAL_NONE)
        return modal_event(event);
    return active_ops->event(event);
}

bool crazypod_miniapps_tick(void)
{
    crazypod_miniapps_alarm_service(NULL);
    if(!crazypod_miniapps_is_open())
        return false;
    return active_ops->tick(host_epoch_seconds(),
                            host_monotonic_ms());
}

static bool scene_valid(struct cp_scene *scene)
{
    int index;
    int bitmap_count = 0;

    if(scene->struct_size < sizeof(*scene) ||
       scene->background >= CP_COLOR_COUNT ||
       scene->command_count > CP_MINIAPP_MAX_COMMANDS)
        return false;
    for(index = 0; index < scene->command_count; ++index) {
        struct cp_draw_command *command = &scene->commands[index];
        if(command->type > CP_DRAW_BITMAP ||
           command->font >= CP_FONT_COUNT ||
           command->align > CP_ALIGN_RIGHT ||
           command->foreground >= CP_COLOR_COUNT ||
           command->background >= CP_COLOR_COUNT ||
           command->border >= CP_COLOR_COUNT ||
           command->track_color >= CP_COLOR_COUNT ||
           command->progress_color >= CP_COLOR_COUNT ||
            command->width < 0 || command->height < 0)
            return false;
        command->text[CP_MINIAPP_TEXT_SIZE - 1] = '\0';
        if(command->type == CP_DRAW_BITMAP) {
            struct cp_resource_info info;

            memset(&info, 0, sizeof(info));
            info.struct_size = sizeof(info);
            ++bitmap_count;
            if(bitmap_count > 1 ||
               host_resource_stat(command->text, &info) !=
                   CRAZYPOD_MINIAPP_OK ||
               info.type != CP_RESOURCE_BITMAP_RGB565)
                return false;
        }
    }
    return true;
}

bool crazypod_miniapps_render(struct cp_scene *scene)
{
    if(!crazypod_miniapps_is_open() || scene == NULL)
        return false;
    cp_scene_reset(scene);
    active_ops->render(scene);
    render_modal(scene);
    if(!scene_valid(scene)) {
        cp_scene_reset(scene);
        return false;
    }
    return true;
}

#endif
