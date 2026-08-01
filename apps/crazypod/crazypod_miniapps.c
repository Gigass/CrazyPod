#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "crazypod_miniapps.h"
#include "miniapps/catalog/crazypod_miniapp_catalog.h"
#include "miniapps/runtime/crazypod_miniapp_installed_verifier.h"
#include "miniapps/runtime/crazypod_miniapp_host_system.h"
#include "miniapps/runtime/crazypod_miniapp_native_runtime.h"
#include "miniapps/runtime/crazypod_miniapp_resource_host.h"
#include "miniapps/runtime/crazypod_miniapp_verification_cache.h"

static int active_index = -1;
static bool active_ui_changed;
static struct crazypod_miniapp_ui_host ui_host;

void crazypod_miniapps_set_ui_host(
    const struct crazypod_miniapp_ui_host *host)
{
    if(host == NULL)
        memset(&ui_host, 0, sizeof(ui_host));
    else
        ui_host = *host;
}

static int ui_result(int result)
{
    if(result == CRAZYPOD_MINIAPP_OK)
        active_ui_changed = true;
    return result;
}

int crazypod_miniapps_ui_begin_update(void)
{
    return ui_host.begin_update != NULL
        ? ui_host.begin_update() : CRAZYPOD_MINIAPP_ERROR_STATE;
}

uint32_t crazypod_miniapps_ui_create(uint8_t object_type)
{
    uint32_t handle = ui_host.create != NULL
        ? ui_host.create(object_type) : CP_NATIVE_UI_HANDLE_NONE;

    if(handle != CP_NATIVE_UI_HANDLE_NONE)
        active_ui_changed = true;
    return handle;
}

int crazypod_miniapps_ui_insert(
    uint32_t child, uint32_t parent, uint32_t before)
{
    return ui_host.insert != NULL
        ? ui_result(ui_host.insert(child, parent, before))
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_ui_set_i32(
    uint32_t target, uint16_t property, int32_t value)
{
    return ui_host.set_i32 != NULL
        ? ui_result(ui_host.set_i32(target, property, value))
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_ui_set_color(
    uint32_t target, uint16_t property, uint32_t rgb)
{
    return ui_host.set_color != NULL
        ? ui_result(ui_host.set_color(target, property, rgb))
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_ui_set_string(
    uint32_t target, uint16_t property, const char *value)
{
    return ui_host.set_string != NULL
        ? ui_result(ui_host.set_string(target, property, value))
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_ui_set_bytes(
    uint32_t target, uint16_t property,
    const void *data, size_t size)
{
    return ui_host.set_bytes != NULL
        ? ui_result(
            ui_host.set_bytes(target, property, data, size))
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_ui_listen(
    uint32_t target, uint8_t event_type, uint32_t handler)
{
    return ui_host.listen != NULL
        ? ui_result(ui_host.listen(target, event_type, handler))
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_ui_animate(
    uint32_t target, uint16_t property,
    int32_t from, int32_t to,
    uint32_t duration_ms, uint32_t delay_ms,
    uint16_t easing, uint32_t completion_handler)
{
    return ui_host.animate != NULL
        ? ui_result(ui_host.animate(
            target, property, from, to,
            duration_ms, delay_ms, easing,
            completion_handler))
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_ui_commit_drawing(
    uint32_t target, const void *data, size_t size)
{
    return ui_host.commit_drawing != NULL
        ? ui_result(ui_host.commit_drawing(target, data, size))
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_ui_remove(uint32_t target)
{
    return ui_host.remove != NULL
        ? ui_result(ui_host.remove(target))
        : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapps_ui_end_update(void)
{
    return ui_host.end_update != NULL
        ? ui_result(ui_host.end_update())
        : CRAZYPOD_MINIAPP_ERROR_STATE;
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
    return valid_id(id) ? crazypod_miniapp_catalog_find(id) : -1;
}

static const struct crazypod_miniapp_metadata *current_metadata(void)
{
    return crazypod_miniapp_catalog_get(active_index);
}

static void finish_active_session(void)
{
    if(ui_host.reset != NULL)
        ui_host.reset();
    crazypod_miniapp_host_session_finish();
    active_index = -1;
    active_ui_changed = false;
}

int crazypod_miniapps_open(int index)
{
    const struct crazypod_miniapp_metadata *metadata;
    int result;

    if(index < 0 || index >= crazypod_miniapp_catalog_count())
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(active_index >= 0)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    metadata = crazypod_miniapp_catalog_get(index);
    if(metadata == NULL)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(!crazypod_miniapp_verification_cache_contains(
           metadata->id, metadata->version_code)) {
        result = crazypod_miniapp_installed_verify(metadata);
        if(result != CRAZYPOD_MINIAPP_OK)
            return result;
        crazypod_miniapp_verification_cache_mark(
            metadata->id, metadata->version_code);
    }
    active_index = index;
    if(metadata->package_format != CP_NATIVE_PACKAGE_FORMAT)
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    result = crazypod_miniapp_native_open(metadata);
    if(result != CRAZYPOD_MINIAPP_OK) {
        finish_active_session();
        return result;
    }
    active_ui_changed = true;
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
    if(active_index < 0)
        return;
    crazypod_miniapp_native_close();
    finish_active_session();
}

bool crazypod_miniapps_is_open(void)
{
    return active_index >= 0 &&
        crazypod_miniapp_native_is_open();
}

int crazypod_miniapps_current(void)
{
    return crazypod_miniapps_is_open() ? active_index : -1;
}

bool crazypod_miniapps_take_ui_refresh(void)
{
    bool changed =
        crazypod_miniapps_is_open() && active_ui_changed;
    active_ui_changed = false;
    return changed;
}

bool crazypod_miniapps_event(const struct cp_input_event *event)
{
    bool changed;

    if(!crazypod_miniapps_is_open() || event == NULL ||
       event->struct_size < sizeof(*event) ||
       event->type > CP_INPUT_MENU ||
       ((event->type == CP_INPUT_WHEEL_CLOCKWISE ||
         event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE) &&
        event->steps == 0))
        return false;
    if(ui_host.input != NULL &&
       ui_host.input(event)) {
        active_ui_changed = true;
        return true;
    }
    changed = crazypod_miniapp_native_event(event);
    active_ui_changed |= changed;
    if(!crazypod_miniapp_native_is_open())
        finish_active_session();
    return changed;
}

bool crazypod_miniapps_ui_event(
    uint32_t handler, uint8_t event_type,
    uint32_t target, int32_t value)
{
    bool changed;

    if(!crazypod_miniapps_is_open())
        return false;
    changed = crazypod_miniapp_native_ui_event(
        handler, event_type, target, value);
    active_ui_changed |= changed;
    if(!crazypod_miniapp_native_is_open())
        finish_active_session();
    return changed;
}

bool crazypod_miniapps_tick(void)
{
    bool changed;

    if(!crazypod_miniapps_is_open())
        return false;
    changed = crazypod_miniapp_native_tick();
    active_ui_changed |= changed;
    if(!crazypod_miniapp_native_is_open())
        finish_active_session();
    return changed;
}

bool crazypod_miniapps_has_scheduled_work(void)
{
    return crazypod_miniapp_native_has_scheduled_work();
}

int crazypod_miniapps_resource_stat(
    const char *id, struct cp_resource_info *info)
{
    return crazypod_miniapp_resource_stat(
        current_metadata(), id, info);
}

int crazypod_miniapps_resource_read(
    const char *id, uint32_t offset, void *buffer, size_t capacity)
{
    return crazypod_miniapp_resource_read(
        current_metadata(), id, offset, buffer, capacity);
}

#endif
