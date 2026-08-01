#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "load_code.h"

#include "../../crazypod_miniapps.h"
#include "../crazypod_miniapp_storage.h"
#include "crazypod_miniapp_host_system.h"
#include "crazypod_miniapp_native_runtime.h"
#include "crazypod_miniapp_resource_host.h"

#if CONFIG_BINFMT == BINFMT_ROCK
extern unsigned char pluginbuf[];
#endif

struct cp_native_binary_header_runtime {
    struct lc_header lc_header;
    cp_native_entry_fn entry;
    unsigned char *bss_start;
    uint32_t host_api_size;
    uint32_t ui_api_size;
    uint32_t app_ops_size;
    uint16_t abi_minor;
    uint16_t react_profile;
};

static struct {
    const struct crazypod_miniapp_metadata *metadata;
    void *handle;
    const struct cp_native_app_ops *ops;
    bool close_requested;
} native;

static int ui_animate(
    cp_ui_handle_t target, uint16_t property,
    const struct cp_native_ui_animation *animation)
{
    if(animation == NULL || animation->flags != 0)
        return CP_NATIVE_ERROR_ARGUMENT;
    return crazypod_miniapps_ui_animate(
        target, property, animation->from, animation->to,
        animation->duration_ms, animation->delay_ms,
        animation->easing, animation->completion_handler);
}

static const struct cp_native_ui_api ui_api = {
    .abi_major = CP_NATIVE_ABI_MAJOR,
    .abi_minor = CP_NATIVE_ABI_MINOR,
    .struct_size = sizeof(struct cp_native_ui_api),
    .begin_update = crazypod_miniapps_ui_begin_update,
    .create = crazypod_miniapps_ui_create,
    .insert = crazypod_miniapps_ui_insert,
    .set_i32 = crazypod_miniapps_ui_set_i32,
    .set_color = crazypod_miniapps_ui_set_color,
    .set_string = crazypod_miniapps_ui_set_string,
    .set_bytes = crazypod_miniapps_ui_set_bytes,
    .listen = crazypod_miniapps_ui_listen,
    .animate = ui_animate,
    .commit_drawing = crazypod_miniapps_ui_commit_drawing,
    .remove = crazypod_miniapps_ui_remove,
    .end_update = crazypod_miniapps_ui_end_update,
};

static int host_state_read(void *buffer, size_t capacity)
{
    return native.metadata != NULL
        ? crazypod_miniapp_storage_read(
            native.metadata->id, buffer, capacity)
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

static int host_state_write(const void *buffer, size_t size)
{
    return native.metadata != NULL
        ? crazypod_miniapp_storage_write(
            native.metadata->id, buffer, size)
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

static int host_resource_stat(
    const char *id, struct cp_resource_info *info)
{
    return crazypod_miniapp_resource_stat(
        native.metadata, id, info);
}

static int host_resource_read(
    const char *id, uint32_t offset,
    void *buffer, size_t capacity)
{
    return crazypod_miniapp_resource_read(
        native.metadata, id, offset, buffer, capacity);
}

static int host_request_close(void)
{
    if(native.ops == NULL)
        return CP_NATIVE_ERROR_STATE;
    native.close_requested = true;
    return CP_NATIVE_OK;
}

static void host_log(uint8_t level, const char *message)
{
    (void)level;
    (void)message;
}

static int host_file_size(const char *relative_path)
{
    return native.metadata != NULL
        ? crazypod_miniapp_file_size(native.metadata->id, relative_path)
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

static int host_file_read(
    const char *relative_path, void *buffer, size_t capacity)
{
    return native.metadata != NULL
        ? crazypod_miniapp_file_read(
            native.metadata->id, relative_path, buffer, capacity)
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

static int host_file_write(
    const char *relative_path, const void *buffer, size_t size)
{
    return native.metadata != NULL
        ? crazypod_miniapp_file_write(
            native.metadata->id, relative_path, buffer, size)
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

static int host_file_remove(const char *relative_path)
{
    return native.metadata != NULL
        ? crazypod_miniapp_file_remove(native.metadata->id, relative_path)
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

static const struct cp_native_host_api host_api = {
    .abi_major = CP_NATIVE_ABI_MAJOR,
    .abi_minor = CP_NATIVE_ABI_MINOR,
    .struct_size = sizeof(struct cp_native_host_api),
    .capabilities =
        CP_NATIVE_CAP_STATE |
        CP_NATIVE_CAP_RESOURCES |
        CP_NATIVE_CAP_REQUEST_CLOSE |
        CP_NATIVE_CAP_LOG |
        CP_NATIVE_CAP_FILES,
    .ui = &ui_api,
    .epoch_seconds = crazypod_miniapp_host_epoch_seconds,
    .monotonic_ms = crazypod_miniapp_host_monotonic_ms,
    .state_read = host_state_read,
    .state_write = host_state_write,
    .resource_stat = host_resource_stat,
    .resource_read = host_resource_read,
    .request_close = host_request_close,
    .log = host_log,
    .file_size = host_file_size,
    .file_read = host_file_read,
    .file_write = host_file_write,
    .file_remove = host_file_remove,
};

static size_t bounded_length(const char *text, size_t capacity)
{
    size_t length = 0;

    if(text == NULL)
        return capacity;
    while(length < capacity && text[length] != '\0')
        ++length;
    return length;
}

static bool runtime_string_matches(
    const struct cp_native_binary_header_runtime *header,
    const char *actual, const char *expected, size_t maximum)
{
    size_t length;

    if(actual == NULL || expected == NULL)
        return false;
#if CONFIG_BINFMT == BINFMT_ROCK
    if((uintptr_t)actual <
           (uintptr_t)header->lc_header.load_addr ||
       (uintptr_t)actual >=
           (uintptr_t)header->lc_header.end_addr)
        return false;
    maximum =
        (size_t)((uintptr_t)header->lc_header.end_addr -
                 (uintptr_t)actual);
#else
    (void)header;
#endif
    length = bounded_length(actual, maximum);
    return length < maximum && strcmp(actual, expected) == 0;
}

static bool ops_valid(
    const struct cp_native_binary_header_runtime *header,
    const struct cp_native_app_ops *ops,
    const struct crazypod_miniapp_metadata *metadata)
{
    if(ops == NULL ||
       ops->abi_major != CP_NATIVE_ABI_MAJOR ||
       ops->abi_minor > CP_NATIVE_ABI_MINOR ||
       ops->struct_size != sizeof(*ops) ||
       ops->mount == NULL || ops->unmount == NULL ||
       ops->input == NULL || ops->ui_event == NULL ||
       ops->tick == NULL || ops->has_scheduled_work == NULL)
        return false;
#if CONFIG_BINFMT == BINFMT_ROCK
    {
        uintptr_t address = (uintptr_t)ops;
        uintptr_t load =
            (uintptr_t)header->lc_header.load_addr;
        uintptr_t end =
            (uintptr_t)header->lc_header.end_addr;

        if(address < load || address > end ||
           sizeof(*ops) > end - address)
            return false;
    }
#endif
    return runtime_string_matches(
               header, ops->id, metadata->id,
               CRAZYPOD_MINIAPP_ID_SIZE) &&
           runtime_string_matches(
               header, ops->name, metadata->name,
               CRAZYPOD_MINIAPP_NAME_SIZE) &&
           runtime_string_matches(
               header, ops->version, metadata->version,
               CRAZYPOD_MINIAPP_VERSION_SIZE);
}

static bool header_valid(
    const struct cp_native_binary_header_runtime *header)
{
    if(header == NULL ||
       header->lc_header.magic != CP_NATIVE_BINARY_MAGIC ||
       header->lc_header.target_id != TARGET_ID ||
       header->lc_header.api_version != CP_NATIVE_ABI_MAJOR ||
       header->abi_minor > CP_NATIVE_ABI_MINOR ||
       header->react_profile != CP_NATIVE_REACT_PROFILE ||
       header->host_api_size != sizeof(host_api) ||
       header->ui_api_size != sizeof(ui_api) ||
       header->app_ops_size !=
           sizeof(struct cp_native_app_ops) ||
       header->entry == NULL)
        return false;
#if CONFIG_BINFMT == BINFMT_ROCK
    if(header->bss_start == NULL ||
       header->bss_start < header->lc_header.load_addr ||
       header->bss_start > header->lc_header.end_addr)
        return false;
#endif
    return true;
}

void crazypod_miniapp_native_close(void)
{
    void *handle = native.handle;

    if(native.ops != NULL)
        native.ops->unmount();
    memset(&native, 0, sizeof(native));
    if(handle != NULL)
        lc_close(handle);
}

static bool apply_update(uint32_t update)
{
    bool changed = (update & CP_NATIVE_UPDATE_UI) != 0;

    if((update & CP_NATIVE_UPDATE_CLOSE) != 0 ||
       native.close_requested)
        crazypod_miniapp_native_close();
    return changed;
}

int crazypod_miniapp_native_open(
    const struct crazypod_miniapp_metadata *metadata)
{
    struct cp_native_binary_header_runtime *header;
    const struct cp_native_app_ops *ops;
    void *handle;
    int result;

    if(metadata == NULL || native.handle != NULL ||
       metadata->package_format != CP_NATIVE_PACKAGE_FORMAT ||
       metadata->abi_version != CP_NATIVE_ABI_MAJOR ||
       metadata->binary_path[0] == '\0')
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(!crazypod_miniapp_host_session_begin(0))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
#if CONFIG_BINFMT == BINFMT_ROCK
    handle = lc_open(
        metadata->binary_path, pluginbuf, PLUGIN_BUFFER_SIZE);
#else
    handle = lc_open(metadata->binary_path, NULL, 0);
#endif
    if(handle == NULL)
        return CRAZYPOD_MINIAPP_ERROR_ABI;
    header = lc_get_header(handle);
    if(!header_valid(header)) {
        lc_close(handle);
        return CRAZYPOD_MINIAPP_ERROR_ABI;
    }
#if CONFIG_BINFMT == BINFMT_ROCK
    memset(
        header->bss_start, 0,
        (size_t)(header->lc_header.end_addr -
                 header->bss_start));
#endif
    native.metadata = metadata;
    native.handle = handle;
    ops = header->entry(&host_api);
    if(!ops_valid(header, ops, metadata)) {
        memset(&native, 0, sizeof(native));
        lc_close(handle);
        return CRAZYPOD_MINIAPP_ERROR_ABI;
    }
    native.ops = ops;
    result = native.ops->mount();
    if(result != CP_NATIVE_OK) {
        crazypod_miniapp_native_close();
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    }
    return CRAZYPOD_MINIAPP_OK;
}

bool crazypod_miniapp_native_is_open(void)
{
    return native.handle != NULL && native.ops != NULL;
}

bool crazypod_miniapp_native_event(
    const struct cp_input_event *event)
{
    return crazypod_miniapp_native_is_open() &&
        apply_update(native.ops->input(event));
}

bool crazypod_miniapp_native_ui_event(
    uint32_t handler, uint8_t event_type,
    uint32_t target, int32_t value)
{
    return crazypod_miniapp_native_is_open() &&
        apply_update(native.ops->ui_event(
            handler, event_type, target, value));
}

bool crazypod_miniapp_native_tick(void)
{
    return crazypod_miniapp_native_is_open() &&
        apply_update(native.ops->tick(
            crazypod_miniapp_host_epoch_seconds(),
            crazypod_miniapp_host_monotonic_ms()));
}

bool crazypod_miniapp_native_has_scheduled_work(void)
{
    return crazypod_miniapp_native_is_open() &&
        native.ops->has_scheduled_work();
}

#endif
