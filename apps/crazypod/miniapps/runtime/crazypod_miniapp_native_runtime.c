#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef SIMULATOR
#include <stdio.h>
#endif

#include "load_code.h"
#include "logf.h"
#include "misc.h"
#include "powermgmt.h"
#include "timefuncs.h"

#include "../../crazypod_miniapps.h"
#include "../../crazypod_state.h"
#include "../crazypod_miniapp_storage.h"
#include "crazypod_miniapp_host_system.h"
#include "crazypod_miniapp_native_runtime.h"

#include "../../crazypod_runtime_font.h"
#include "crazypod_miniapp_now_playing_service.h"
#include "crazypod_miniapp_media_library_service.h"
#include "crazypod_miniapp_text_prompt_service.h"
#include "crazypod_miniapp_file_exchange_service.h"
#include "crazypod_miniapp_alarm_service.h"
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

static struct {
    struct cp_diagnostics_log_entry logs[CP_DIAGNOSTICS_LOG_CAPACITY];
    uint32_t next_log;
    uint32_t log_count;
    uint32_t log_dropped;
    uint32_t log_sequence;
    uint32_t update_started_ms;
    uint32_t update_last_ms;
    uint32_t update_max_ms;
} diagnostics;

static size_t bounded_length(const char *text, size_t capacity);

#define CRAZYPOD_NATIVE_CAPABILITIES ( \
    CP_NATIVE_CAP_STATE | CP_NATIVE_CAP_RESOURCES | \
    CP_NATIVE_CAP_REQUEST_CLOSE | CP_NATIVE_CAP_LOG | \
    CP_NATIVE_CAP_FILES | CP_NATIVE_CAP_SERVICES | \
    CP_NATIVE_CAP_DIAGNOSTICS | CP_NATIVE_CAP_MEDIA_LIBRARY | \
    CP_NATIVE_CAP_TEXT_PROMPT | CP_NATIVE_CAP_FILE_EXCHANGE | \
    CP_NATIVE_CAP_SOUND_EFFECTS | CP_NATIVE_CAP_ALARMS)

static int ui_begin_update(void)
{
    diagnostics.update_started_ms =
        crazypod_miniapp_host_monotonic_ms();
    return crazypod_miniapps_ui_begin_update();
}

static int ui_end_update(void)
{
    int result = crazypod_miniapps_ui_end_update();
    uint32_t now = crazypod_miniapp_host_monotonic_ms();

    diagnostics.update_last_ms =
        now - diagnostics.update_started_ms;
    if(diagnostics.update_last_ms > diagnostics.update_max_ms)
        diagnostics.update_max_ms = diagnostics.update_last_ms;
    return result;
}

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
    .begin_update = ui_begin_update,
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
    .end_update = ui_end_update,
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
    struct cp_diagnostics_log_entry *entry;
    size_t length;

    if(message == NULL)
        return;
    entry = &diagnostics.logs[diagnostics.next_log];
    memset(entry, 0, sizeof(*entry));
    entry->struct_size = sizeof(*entry);
    entry->sequence = ++diagnostics.log_sequence;
    entry->monotonic_ms =
        crazypod_miniapp_host_monotonic_ms();
    entry->level = level;
    length = bounded_length(
        message, CP_DIAGNOSTICS_LOG_TEXT_CAPACITY - 1u);
    memcpy(entry->message, message, length);
    entry->message[length] = '\0';
    diagnostics.next_log =
        (diagnostics.next_log + 1u) % CP_DIAGNOSTICS_LOG_CAPACITY;
    if(diagnostics.log_count < CP_DIAGNOSTICS_LOG_CAPACITY)
        ++diagnostics.log_count;
    else
        ++diagnostics.log_dropped;
    logf("miniapp[%u] %s", (unsigned int)level, entry->message);
}

static int diagnostics_service_call(
    uint32_t operation, const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    if(operation == CP_NATIVE_DIAGNOSTICS_SNAPSHOT) {
        struct cp_diagnostics_snapshot snapshot;

        if(request_size != 0 ||
           response_capacity < sizeof(snapshot))
            return request_size != 0
                ? CP_NATIVE_ERROR_ARGUMENT : CP_NATIVE_ERROR_LIMIT;
        (void)crazypod_miniapp_native_diagnostics(&snapshot);
        memcpy(response, &snapshot, sizeof(snapshot));
        return (int)sizeof(snapshot);
    }
    if(operation == CP_NATIVE_DIAGNOSTICS_LOG_ENTRY) {
        const struct cp_diagnostics_log_request *log_request = request;
        uint32_t index;

        if(request_size != sizeof(*log_request) ||
           log_request->struct_size < sizeof(*log_request))
            return CP_NATIVE_ERROR_ARGUMENT;
        if(response_capacity < sizeof(struct cp_diagnostics_log_entry))
            return CP_NATIVE_ERROR_LIMIT;
        if(log_request->newest_offset >= diagnostics.log_count)
            return CP_NATIVE_ERROR_LIMIT;
        index = (diagnostics.next_log + CP_DIAGNOSTICS_LOG_CAPACITY - 1u -
                 log_request->newest_offset) %
            CP_DIAGNOSTICS_LOG_CAPACITY;
        memcpy(response, &diagnostics.logs[index],
               sizeof(diagnostics.logs[index]));
        return (int)sizeof(diagnostics.logs[index]);
    }
    if(operation == CP_NATIVE_DIAGNOSTICS_RESET_PEAKS) {
        if(request_size != 0 || response_capacity != 0)
            return CP_NATIVE_ERROR_ARGUMENT;
        crazypod_miniapp_host_memory_reset_high_water();
        crazypod_miniapps_ui_reset_handle_high_water();
        diagnostics.update_max_ms = diagnostics.update_last_ms;
        return CP_NATIVE_OK;
    }
    return CP_NATIVE_ERROR_UNSUPPORTED;
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

static int host_service_call(
    uint32_t service, uint32_t operation,
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    struct cp_native_system_info info;

    if(request_size > CP_NATIVE_SERVICE_PAYLOAD_MAX ||
       response_capacity > CP_NATIVE_SERVICE_PAYLOAD_MAX ||
       (request_size > 0 && request == NULL) ||
       (response_capacity > 0 && response == NULL))
        return CP_NATIVE_ERROR_ARGUMENT;
    if(service == CP_NATIVE_SERVICE_SYSTEM &&
       operation == CP_NATIVE_SYSTEM_STATUS) {
        int32_t status[CP_NATIVE_SYSTEM_STATUS_COUNT];
        struct tm *now;
        int level;

        if(request_size != 0 && request_size != sizeof(int32_t))
            return CP_NATIVE_ERROR_ARGUMENT;
        size_t response_size;

        if(response_capacity <
           CP_NATIVE_SYSTEM_STATUS_LEGACY_COUNT * sizeof(int32_t))
            return CP_NATIVE_ERROR_LIMIT;
        now = get_time();
        level = battery_level();
        if(level < 0)
            level = 0;
        if(level > 100)
            level = 100;
        status[0] = now->tm_hour;
        status[1] = now->tm_min / 10;
        status[2] = now->tm_min % 10;
        status[3] = level;
#if CONFIG_CHARGING >= CHARGING_MONITOR
        status[4] = charge_state > DISCHARGING ? 1 : 0;
#else
        status[4] = 0;
#endif
        status[5] = crazypod_state_reduce_motion() ? 1 : 0;
        response_size = response_capacity >= sizeof(status)
            ? sizeof(status)
            : CP_NATIVE_SYSTEM_STATUS_LEGACY_COUNT * sizeof(int32_t);
        memcpy(response, status, response_size);
        return (int)response_size;
    }
    if(service == CP_NATIVE_SERVICE_DIAGNOSTICS)
        return diagnostics_service_call(
            operation, request, request_size,
            response, response_capacity);
    if(service == CP_NATIVE_SERVICE_MEDIA_LIBRARY &&
       native.metadata != NULL &&
       (native.metadata->permissions &
        CRAZYPOD_MINIAPP_PERMISSION_MEDIA_LIBRARY_READ) != 0)
        return crazypod_miniapp_media_library_service_call(
            operation, request, request_size,
            response, response_capacity);
    if(service == CP_NATIVE_SERVICE_TEXT_PROMPT)
        return crazypod_miniapp_text_prompt_service_call(
            operation, request, request_size,
            response, response_capacity);
    if(service == CP_NATIVE_SERVICE_FILE_EXCHANGE)
        return crazypod_miniapp_file_exchange_service_call(
            native.metadata, operation, request, request_size,
            response, response_capacity);
    if(service == CP_NATIVE_SERVICE_SOUND_EFFECTS &&
       operation == CP_NATIVE_SOUND_EFFECT_PLAY) {
        const struct cp_sound_effect_request *effect = request;
        static const struct {
            unsigned frequency;
            unsigned duration;
            unsigned amplitude;
        } effects[] = {
            { 900, 20, 3000 }, { 1200, 60, 5000 },
            { 600, 100, 6000 }, { 300, 160, 7000 },
        };

        if(native.metadata == NULL ||
           (native.metadata->permissions &
            CRAZYPOD_MINIAPP_PERMISSION_SOUND_EFFECTS_PLAY) == 0)
            return CP_NATIVE_ERROR_UNSUPPORTED;
        if(effect == NULL || request_size != sizeof(*effect) ||
           response != NULL || response_capacity != 0 ||
           effect->struct_size != sizeof(*effect) ||
           effect->effect >= ARRAYLEN(effects))
            return CP_NATIVE_ERROR_ARGUMENT;
        beep_play(effects[effect->effect].frequency,
                  effects[effect->effect].duration,
                  effects[effect->effect].amplitude);
        return CP_NATIVE_OK;
    }
    if(service == CP_NATIVE_SERVICE_ALARMS)
        return crazypod_miniapp_alarm_service_call(
            native.metadata, crazypod_miniapp_host_epoch_seconds(),
            operation, request, request_size,
            response, response_capacity);
    if(service != CP_NATIVE_SERVICE_SYSTEM ||
       operation != CP_NATIVE_SYSTEM_INFO) {
        if(service == CP_NATIVE_SERVICE_NOW_PLAYING &&
           native.metadata != NULL &&
           native.metadata->kind ==
               CRAZYPOD_MINIAPP_KIND_NOW_PLAYING_THEME)
            return crazypod_miniapp_now_playing_service_call(
                operation, request, request_size,
                response, response_capacity);
        return CP_NATIVE_ERROR_UNSUPPORTED;
    }
    if(response_capacity < sizeof(info))
        return CP_NATIVE_ERROR_LIMIT;

    info.abi_major = CP_NATIVE_ABI_MAJOR;
    info.abi_minor = CP_NATIVE_ABI_MINOR;
    info.capabilities = CRAZYPOD_NATIVE_CAPABILITIES;
    info.service_payload_max = CP_NATIVE_SERVICE_PAYLOAD_MAX;
    memcpy(response, &info, sizeof(info));
    return (int)sizeof(info);
}

static const struct cp_native_host_api host_api = {
    .abi_major = CP_NATIVE_ABI_MAJOR,
    .abi_minor = CP_NATIVE_ABI_MINOR,
    .struct_size = sizeof(struct cp_native_host_api),
    .capabilities = CRAZYPOD_NATIVE_CAPABILITIES,
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
    .service_call = host_service_call,
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
       ops->abi_minor == CP_NATIVE_ABI_REJECTED_MINOR ||
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
       header->abi_minor == CP_NATIVE_ABI_REJECTED_MINOR ||
       header->react_profile != CP_NATIVE_REACT_PROFILE ||
       header->host_api_size <
           offsetof(struct cp_native_host_api, file_size) ||
       (header->abi_minor >= 2u &&
        header->host_api_size <
            offsetof(struct cp_native_host_api, service_call)) ||
       (header->abi_minor >= 3u &&
        header->host_api_size < sizeof(host_api)) ||
       header->host_api_size > sizeof(host_api) ||
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
    crazypod_miniapp_text_prompt_reset();
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
    memset(&diagnostics, 0, sizeof(diagnostics));
    crazypod_miniapp_text_prompt_reset();
    ops = header->entry(&host_api);
    if(!ops_valid(header, ops, metadata)) {
        memset(&native, 0, sizeof(native));
        lc_close(handle);
        return CRAZYPOD_MINIAPP_ERROR_ABI;
    }
    native.ops = ops;
    crazypod_runtime_font_error_clear();
    result = native.ops->mount();
    if(result != CP_NATIVE_OK) {
#ifdef SIMULATOR
        fprintf(stderr,
                "CrazyPod miniapp mount failed: result=%d font=%s\n",
                result, crazypod_runtime_font_last_error());
#endif
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

bool crazypod_miniapp_native_diagnostics(
    struct cp_diagnostics_snapshot *snapshot)
{
    if(snapshot == NULL)
        return false;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->struct_size = sizeof(*snapshot);
    snapshot->monotonic_ms =
        crazypod_miniapp_host_monotonic_ms();
    snapshot->memory_used = (uint32_t)
        crazypod_miniapp_host_memory_used();
    snapshot->memory_high_water = (uint32_t)
        crazypod_miniapp_host_memory_high_water();
    snapshot->memory_limit = (uint32_t)
        crazypod_miniapp_host_memory_limit();
    snapshot->ui_handles_used =
        crazypod_miniapps_ui_handle_count();
    snapshot->ui_handles_high_water =
        crazypod_miniapps_ui_handle_high_water();
    snapshot->update_last_ms = diagnostics.update_last_ms;
    snapshot->update_max_ms = diagnostics.update_max_ms;
    snapshot->log_count = diagnostics.log_count;
    snapshot->log_dropped = diagnostics.log_dropped;
    return crazypod_miniapp_native_is_open();
}

#endif
