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
#include "miniapps/crazypod_miniapp_storage.h"
#include "miniapps/installer/crazypod_miniapp_manifest.h"
#include "miniapps/catalog/crazypod_miniapp_catalog.h"
#include "miniapps/modal/crazypod_miniapp_modal.h"
#include "miniapps/notification/crazypod_miniapp_notification.h"
#include "miniapps/alarm/crazypod_miniapp_alarm_service.h"
#include "miniapps/runtime/crazypod_miniapp_host_system.h"
#include "miniapps/runtime/crazypod_miniapp_resource_host.h"
#include "miniapps/installer/crazypod_miniapp_resource_validator.h"
#include "miniapps/installer/crazypod_cpk_reader.h"
#include "miniapps/installer/crazypod_miniapp_install_record.h"
#include "miniapps/installer/crazypod_cpk_verifier.h"
#include "miniapps/installer/crazypod_miniapp_stage.h"
#include "miniapps/catalog/crazypod_miniapp_registry_loader.h"
#include "miniapps/runtime/crazypod_miniapp_native_validator.h"
#include "miniapps/runtime/crazypod_miniapp_verification_cache.h"
#include "miniapps/runtime/crazypod_miniapp_installed_verifier.h"

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

#if CONFIG_BINFMT == BINFMT_ROCK
#define MINIAPP_EXPECTED_TARGET "ipod6g"
#define MINIAPP_EXPECTED_BINARY "app.arm"
#else
#define MINIAPP_EXPECTED_TARGET "simulator"
#define MINIAPP_EXPECTED_BINARY "app.dylib"
#endif

#define MINIAPP_MANIFEST_MAX CRAZYPOD_MINIAPP_MANIFEST_MAX
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
#define MINIAPP_SCAN_LIMIT 64
#define MINIAPP_REMOVE_DEPTH 3
#define MINIAPP_REMOVE_ENTRIES 32
#define MINIAPP_SPACE_RESERVE (128u * 1024u)

#define MINIAPP_INSTALL_MAGIC 0x4350494eu /* CPIN */
#define MINIAPP_DISK_VERSION 1u

#define ZIP_SIG_EOCD 0x06054b50u
#define ZIP_SIG_CENTRAL 0x02014b50u
#define ZIP_SIG_LOCAL 0x04034b50u
#define ZIP_METHOD_STORE 0u

static int active_index = -1;
static void *active_handle;
static const struct cp_miniapp_ops *active_ops;
static const struct miniapp_binary_header_runtime *active_header;
static bool active_close_requested;
static bool active_ui_changed;

#if CONFIG_BINFMT == BINFMT_ROCK
extern unsigned char pluginbuf[];
#endif

static size_t bounded_length(const char *text, size_t capacity)
{
    size_t length = 0;

    if(text == NULL)
        return capacity;
    while(length < capacity && text[length] != '\0')
        ++length;
    return length;
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

int crazypod_miniapps_count(void)
{
    return crazypod_miniapp_catalog_count();
}

const struct crazypod_miniapp_metadata *
crazypod_miniapps_metadata(int index)
{
    return crazypod_miniapp_catalog_get(index);
}

int crazypod_miniapps_find(const char *id)
{
    if(!valid_id(id))
        return -1;
    return crazypod_miniapp_catalog_find(id);
}

static const struct crazypod_miniapp_metadata *current_metadata(void)
{
    return crazypod_miniapp_catalog_get(active_index);
}

static int host_state_read(void *buffer, size_t capacity)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();

    if(metadata == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    return crazypod_miniapp_storage_read(
        metadata->id, buffer, capacity);
}

static int host_state_write(const void *buffer, size_t size)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();

    if(metadata == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    return crazypod_miniapp_storage_write(
        metadata->id, buffer, size);
}

static int host_alarm_set_slot(
    uint8_t slot, uint32_t deadline_epoch, uint32_t token)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();

    if(metadata == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    return crazypod_miniapp_alarm_set(
        metadata->id, slot, deadline_epoch, token,
        crazypod_miniapp_host_epoch_seconds());
}

static int host_alarm_set(uint32_t deadline_epoch, uint32_t token)
{
    return host_alarm_set_slot(0, deadline_epoch, token);
}

static void host_alarm_cancel_slot(uint8_t slot)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();

    if(metadata == NULL)
        return;
    crazypod_miniapp_alarm_cancel(
        metadata->id, slot, crazypod_miniapp_host_epoch_seconds());
}

static void host_alarm_cancel(void)
{
    host_alarm_cancel_slot(0);
}

static bool host_alarm_fired_slot(uint8_t slot, uint32_t *token)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();

    if(metadata == NULL)
        return false;
    return crazypod_miniapp_alarm_fired(
        metadata->id, slot, crazypod_miniapp_host_epoch_seconds(), token);
}

static bool host_alarm_fired(uint32_t *token)
{
    return host_alarm_fired_slot(0, token);
}

static void host_alarm_acknowledge_slot(uint8_t slot)
{
    const struct crazypod_miniapp_metadata *metadata = current_metadata();

    if(metadata == NULL)
        return;
    crazypod_miniapp_alarm_acknowledge_slot(
        metadata->id, slot, crazypod_miniapp_host_epoch_seconds());
}

static void host_alarm_acknowledge(void)
{
    host_alarm_acknowledge_slot(0);
}

static int host_ui_toast(const char *text, uint32_t duration_ms)
{
    if(current_metadata() == NULL || text == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    return crazypod_miniapp_notification_show(
        text, duration_ms, current_tick, HZ);
}

static int host_ui_request_close(void)
{
    if(current_metadata() == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    active_close_requested = true;
    return CRAZYPOD_MINIAPP_OK;
}

static int host_resource_stat(
    const char *id, struct cp_resource_info *info)
{
    return crazypod_miniapp_resource_stat(
        current_metadata(), id, info);
}

static int host_resource_read(
    const char *id, uint32_t offset, void *buffer, size_t capacity)
{
    return crazypod_miniapp_resource_read(
        current_metadata(), id, offset, buffer, capacity);
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

static const struct cp_host_api host_api = {
    .abi_version = CP_MINIAPP_ABI_VERSION,
    .struct_size = sizeof(struct cp_host_api),
    .epoch_seconds = crazypod_miniapp_host_epoch_seconds,
    .monotonic_ms = crazypod_miniapp_host_monotonic_ms,
    .state_read = host_state_read,
    .state_write = host_state_write,
    .alarm_set = host_alarm_set,
    .alarm_cancel = host_alarm_cancel,
    .alarm_fired = host_alarm_fired,
    .alarm_acknowledge = host_alarm_acknowledge,
    .format_number = crazypod_miniapp_host_format_number,
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
    .system_info = crazypod_miniapp_host_system_info,
    .format_duration = crazypod_miniapp_host_format_duration,
    .format_datetime = crazypod_miniapp_host_format_datetime,
    .alarm_set_slot = host_alarm_set_slot,
    .alarm_cancel_slot = host_alarm_cancel_slot,
    .alarm_fired_slot = host_alarm_fired_slot,
    .alarm_acknowledge_slot = host_alarm_acknowledge_slot,
    .ui_toast = host_ui_toast,
    .ui_request_close = host_ui_request_close,
    .ui_text_input = crazypod_miniapp_modal_text_input,
    .ui_choice = crazypod_miniapp_modal_choice,
    .ui_confirm = crazypod_miniapp_modal_confirm,
    .ui_poll_result = crazypod_miniapp_modal_poll,
    .ui_cancel = crazypod_miniapp_modal_cancel,
    .resource_stat = host_resource_stat,
    .resource_read = host_resource_read,
    .now_playing = crazypod_miniapp_host_now_playing
};

bool crazypod_miniapps_alarm_service(
    struct crazypod_miniapp_alarm *alarm)
{
    return crazypod_miniapp_alarm_service(
        crazypod_miniapp_host_epoch_seconds(), alarm);
}

int crazypod_miniapps_alarm_acknowledge(const char *id)
{
    return crazypod_miniapp_alarm_acknowledge(
        id, crazypod_miniapp_host_epoch_seconds());
}

int crazypod_miniapps_alarm_delivery_acknowledge(
    const char *id, uint32_t deadline_epoch, uint32_t token)
{
    return crazypod_miniapp_alarm_delivery_acknowledge(
        id, deadline_epoch, token);
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

int crazypod_miniapps_open(int index)
{
    const struct crazypod_miniapp_metadata *metadata;
    struct miniapp_binary_header_runtime *header;
    const struct cp_miniapp_ops *ops;
    void *handle;
    uint32_t binary_file_size;
    int result;

    if(index < 0 || index >= crazypod_miniapp_catalog_count())
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(active_index >= 0)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    metadata = crazypod_miniapp_catalog_get(index);
    if(!crazypod_miniapp_verification_cache_contains(metadata->id, metadata->version_code)) {
        result = crazypod_miniapp_installed_verify(metadata);
        if(result != CRAZYPOD_MINIAPP_OK)
            return result;
        crazypod_miniapp_verification_cache_mark(metadata->id, metadata->version_code);
    }
    result = crazypod_miniapp_native_installed_validate(metadata, &binary_file_size);
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
       !crazypod_miniapp_native_header_valid(header,
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
    crazypod_miniapp_notification_reset();
    crazypod_miniapp_modal_open();
    ops = header->entry(&host_api);
    if(!runtime_ops_valid(header, ops, metadata)) {
        crazypod_miniapp_modal_close();
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
    crazypod_miniapp_notification_reset();
    crazypod_miniapp_modal_close();
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
    bool modal_changed = crazypod_miniapp_modal_take_changed();
    bool notification_changed =
        crazypod_miniapp_notification_take_changed(current_tick);

    changed = crazypod_miniapps_is_open() &&
        (active_ui_changed || modal_changed || notification_changed);
    active_ui_changed = false;
    return changed;
}

bool crazypod_miniapps_toast(char *buffer, size_t capacity)
{
    return crazypod_miniapps_is_open() &&
        crazypod_miniapp_notification_get(
            current_tick, buffer, capacity);
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
    if(crazypod_miniapp_modal_event(event))
        return true;
    return active_ops->event(event);
}

bool crazypod_miniapps_tick(void)
{
    crazypod_miniapps_alarm_service(NULL);
    if(!crazypod_miniapps_is_open())
        return false;
    return active_ops->tick(crazypod_miniapp_host_epoch_seconds(),
                            crazypod_miniapp_host_monotonic_ms());
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
    crazypod_miniapp_modal_render(scene);
    if(!scene_valid(scene)) {
        cp_scene_reset(scene);
        return false;
    }
    return true;
}

#endif
