#ifndef CRAZYPOD_MINIAPPS_H
#define CRAZYPOD_MINIAPPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../miniapps/sdk/crazypod_miniapp.h"

#define CRAZYPOD_MINIAPP_MAX_APPS 32
#define CRAZYPOD_MINIAPP_ID_SIZE 33
#define CRAZYPOD_MINIAPP_NAME_SIZE 49
#define CRAZYPOD_MINIAPP_VERSION_SIZE 25
#define CRAZYPOD_MINIAPP_SYMBOL_SIZE 9
#define CRAZYPOD_MINIAPP_SUMMARY_SIZE 97
#define CRAZYPOD_MINIAPP_TARGET_SIZE 17
#define CRAZYPOD_MINIAPP_BINARY_SIZE 17
#define CRAZYPOD_MINIAPP_PATH_SIZE 260
#define CRAZYPOD_MINIAPP_SIGNATURE_SIZE 64
#define CRAZYPOD_MINIAPP_RESOURCES_SIZE 17

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
    char target[CRAZYPOD_MINIAPP_TARGET_SIZE];
    char binary[CRAZYPOD_MINIAPP_BINARY_SIZE];
    char icon[CRAZYPOD_MINIAPP_BINARY_SIZE];
    uint32_t version_code;
    uint32_t abi_version;
    uint32_t package_format;
    uint32_t accent_rgb;
    uint8_t binary_sha256[32];
    uint8_t icon_sha256[32];
    uint8_t resources_sha256[32];
    char install_path[CRAZYPOD_MINIAPP_PATH_SIZE];
    char binary_path[CRAZYPOD_MINIAPP_PATH_SIZE];
    char icon_path[CRAZYPOD_MINIAPP_PATH_SIZE];
    char resources_path[CRAZYPOD_MINIAPP_PATH_SIZE];
};

struct crazypod_miniapp_alarm {
    char id[CRAZYPOD_MINIAPP_ID_SIZE];
    char name[CRAZYPOD_MINIAPP_NAME_SIZE];
    uint32_t deadline_epoch;
    uint32_t token;
    bool fired;
};

int crazypod_miniapps_init(void);
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
bool crazypod_miniapps_take_close_request(void);
bool crazypod_miniapps_take_ui_refresh(void);
bool crazypod_miniapps_toast(char *buffer, size_t capacity);

bool crazypod_miniapps_event(const struct cp_input_event *event);
bool crazypod_miniapps_tick(void);
bool crazypod_miniapps_render(struct cp_scene *scene);
int crazypod_miniapps_resource_stat(
    const char *id, struct cp_resource_info *info);
int crazypod_miniapps_resource_read(
    const char *id, uint32_t offset, void *buffer, size_t capacity);

bool crazypod_miniapps_alarm_service(
    struct crazypod_miniapp_alarm *alarm);
int crazypod_miniapps_alarm_acknowledge(const char *id);
int crazypod_miniapps_alarm_delivery_acknowledge(
    const char *id, uint32_t deadline_epoch, uint32_t token);

#endif
