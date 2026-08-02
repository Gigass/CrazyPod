#ifndef CRAZYPOD_MINIAPPS_H
#define CRAZYPOD_MINIAPPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../miniapps/sdk/crazypod_miniapp_native.h"

#define CRAZYPOD_MINIAPP_MAX_APPS 32
#define CRAZYPOD_MINIAPP_ID_SIZE 33
#define CRAZYPOD_MINIAPP_NAME_SIZE 49
#define CRAZYPOD_MINIAPP_VERSION_SIZE 25
#define CRAZYPOD_MINIAPP_SYMBOL_SIZE 9
#define CRAZYPOD_MINIAPP_SUMMARY_SIZE 97
#define CRAZYPOD_MINIAPP_FILE_NAME_SIZE 17
#define CRAZYPOD_MINIAPP_RUNTIME_SIZE 17
#define CRAZYPOD_MINIAPP_PATH_SIZE 260

enum crazypod_miniapp_kind {
    CRAZYPOD_MINIAPP_KIND_APP = 0,
    CRAZYPOD_MINIAPP_KIND_NOW_PLAYING_THEME
};

enum crazypod_miniapp_result {
    CRAZYPOD_MINIAPP_OK = 0,
    CRAZYPOD_MINIAPP_ALREADY_INSTALLED = 1,
    CRAZYPOD_MINIAPP_DOWNGRADE_IGNORED = 2,
    CRAZYPOD_MINIAPP_ERROR_IO = -1,
    CRAZYPOD_MINIAPP_ERROR_FORMAT = -2,
    CRAZYPOD_MINIAPP_ERROR_LIMIT = -3,
    CRAZYPOD_MINIAPP_ERROR_MANIFEST = -4,
    CRAZYPOD_MINIAPP_ERROR_PLATFORM = -5,
    CRAZYPOD_MINIAPP_ERROR_VERSION = -6,
    CRAZYPOD_MINIAPP_ERROR_CRC = -7,
    CRAZYPOD_MINIAPP_ERROR_SIGNATURE = -8,
    CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED = -9,
    CRAZYPOD_MINIAPP_ERROR_SPACE = -10,
    CRAZYPOD_MINIAPP_ERROR_BUSY = -11,
    CRAZYPOD_MINIAPP_ERROR_ABI = -12,
    CRAZYPOD_MINIAPP_ERROR_STATE = -13
};

struct crazypod_miniapp_metadata {
    char id[CRAZYPOD_MINIAPP_ID_SIZE];
    char name[CRAZYPOD_MINIAPP_NAME_SIZE];
    char version[CRAZYPOD_MINIAPP_VERSION_SIZE];
    char symbol[CRAZYPOD_MINIAPP_SYMBOL_SIZE];
    char summary[CRAZYPOD_MINIAPP_SUMMARY_SIZE];
    char runtime[CRAZYPOD_MINIAPP_RUNTIME_SIZE];
    char target[CRAZYPOD_MINIAPP_RUNTIME_SIZE];
    char entry[CRAZYPOD_MINIAPP_FILE_NAME_SIZE];
    char icon[CRAZYPOD_MINIAPP_FILE_NAME_SIZE];
    uint32_t version_code;
    uint32_t abi_version;
    uint32_t abi_minor;
    uint32_t react_profile;
    uint32_t package_format;
    uint32_t accent_rgb;
    uint8_t kind;
    uint32_t assets_size;
    uint32_t icon_size;
    uint32_t binary_size;
    uint32_t profile_size;
    char install_path[CRAZYPOD_MINIAPP_PATH_SIZE];
    char profile_path[CRAZYPOD_MINIAPP_PATH_SIZE];
    char assets_path[CRAZYPOD_MINIAPP_PATH_SIZE];
    char icon_path[CRAZYPOD_MINIAPP_PATH_SIZE];
    char binary_path[CRAZYPOD_MINIAPP_PATH_SIZE];
};

struct crazypod_miniapp_ui_host {
    int (*begin_update)(void);
    uint32_t (*create)(uint8_t object_type);
    int (*insert)(uint32_t child, uint32_t parent, uint32_t before);
    int (*set_i32)(uint32_t target, uint16_t property, int32_t value);
    int (*set_color)(uint32_t target, uint16_t property, uint32_t rgb);
    int (*set_string)(
        uint32_t target, uint16_t property, const char *value);
    int (*set_bytes)(
        uint32_t target, uint16_t property,
        const void *data, size_t size);
    int (*listen)(
        uint32_t target, uint8_t event_type, uint32_t handler);
    int (*animate)(
        uint32_t target, uint16_t property,
        int32_t from, int32_t to,
        uint32_t duration_ms, uint32_t delay_ms,
        uint16_t easing, uint32_t completion_handler);
    int (*commit_drawing)(
        uint32_t target, const void *data, size_t size);
    int (*remove)(uint32_t target);
    int (*end_update)(void);
    bool (*input)(const struct cp_input_event *event);
    void (*reset)(void);
};

void crazypod_miniapps_set_ui_host(
    const struct crazypod_miniapp_ui_host *host);
int crazypod_miniapps_ui_begin_update(void);
uint32_t crazypod_miniapps_ui_create(uint8_t object_type);
int crazypod_miniapps_ui_insert(
    uint32_t child, uint32_t parent, uint32_t before);
int crazypod_miniapps_ui_set_i32(
    uint32_t target, uint16_t property, int32_t value);
int crazypod_miniapps_ui_set_color(
    uint32_t target, uint16_t property, uint32_t rgb);
int crazypod_miniapps_ui_set_string(
    uint32_t target, uint16_t property, const char *value);
int crazypod_miniapps_ui_set_bytes(
    uint32_t target, uint16_t property,
    const void *data, size_t size);
int crazypod_miniapps_ui_listen(
    uint32_t target, uint8_t event_type, uint32_t handler);
int crazypod_miniapps_ui_animate(
    uint32_t target, uint16_t property,
    int32_t from, int32_t to,
    uint32_t duration_ms, uint32_t delay_ms,
    uint16_t easing, uint32_t completion_handler);
int crazypod_miniapps_ui_commit_drawing(
    uint32_t target, const void *data, size_t size);
int crazypod_miniapps_ui_remove(uint32_t target);
int crazypod_miniapps_ui_end_update(void);

int crazypod_miniapps_init(void);
int crazypod_miniapps_prepare(void);
int crazypod_miniapps_rescan(void);
int crazypod_miniapps_install(const char *package_path);

int crazypod_miniapps_count(void);
const struct crazypod_miniapp_metadata *
crazypod_miniapps_metadata(int index);
int crazypod_miniapps_find(const char *id);

int crazypod_miniapps_open(int index);
int crazypod_miniapps_open_id(const char *id);
void crazypod_miniapps_close(void);
bool crazypod_miniapps_is_open(void);
int crazypod_miniapps_current(void);
const struct crazypod_miniapp_metadata *
crazypod_miniapps_current_metadata(void);
enum crazypod_miniapp_kind crazypod_miniapps_current_kind(void);
int crazypod_now_playing_themes_count(void);
const struct crazypod_miniapp_metadata *
crazypod_now_playing_themes_metadata(int index);
int crazypod_now_playing_themes_find(const char *id);
int crazypod_now_playing_themes_open_id(const char *id);
bool crazypod_miniapps_take_ui_refresh(void);

bool crazypod_miniapps_event(const struct cp_input_event *event);
bool crazypod_miniapps_ui_event(
    uint32_t handler, uint8_t event_type,
    uint32_t target, int32_t value);
bool crazypod_miniapps_tick(void);
bool crazypod_miniapps_has_scheduled_work(void);
int crazypod_miniapps_resource_stat(
    const char *id, struct cp_resource_info *info);
int crazypod_miniapps_resource_read(
    const char *id, uint32_t offset, void *buffer, size_t capacity);

#endif
