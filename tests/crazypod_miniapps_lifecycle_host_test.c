#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "crazypod_miniapps.h"

static struct crazypod_miniapp_metadata apps[] = {
    {
        .id = "game2048",
        .name = "2048",
        .version = "5.0.0",
        .version_code = 50000,
        .abi_version = CP_NATIVE_ABI_MAJOR,
        .package_format = CP_NATIVE_PACKAGE_FORMAT,
        .binary_size = 1,
        .binary_path = "app.dylib",
    },
    {
        .id = "capability-lab",
        .name = "Capability Lab",
        .version = "5.0.0",
        .version_code = 50000,
        .abi_version = CP_NATIVE_ABI_MAJOR,
        .package_format = CP_NATIVE_PACKAGE_FORMAT,
        .binary_size = 1,
        .binary_path = "app.dylib",
    },
};

static bool native_open;
static bool session_active;
static bool close_on_event;
static int next_open_result;
static int open_count;
static int close_count;
static int finish_count;
static int scene_reset_count;

int crazypod_miniapp_catalog_count(void)
{
    return (int)(sizeof(apps) / sizeof(apps[0]));
}

const struct crazypod_miniapp_metadata *
crazypod_miniapp_catalog_get(int index)
{
    return index >= 0 && index < crazypod_miniapp_catalog_count()
        ? &apps[index] : NULL;
}

int crazypod_miniapp_catalog_find(const char *id)
{
    int index;
    for(index = 0; index < crazypod_miniapp_catalog_count(); ++index)
        if(strcmp(apps[index].id, id) == 0)
            return index;
    return -1;
}

bool crazypod_miniapp_verification_cache_contains(
    const char *id, uint32_t version_code)
{
    (void)id;
    (void)version_code;
    return true;
}

void crazypod_miniapp_verification_cache_mark(
    const char *id, uint32_t version_code)
{
    (void)id;
    (void)version_code;
}

int crazypod_miniapp_installed_verify(
    const struct crazypod_miniapp_metadata *metadata)
{
    (void)metadata;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_native_open(
    const struct crazypod_miniapp_metadata *metadata)
{
    int result = next_open_result;
    (void)metadata;
    ++open_count;
    next_open_result = CRAZYPOD_MINIAPP_OK;
    if(session_active)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    session_active = true;
    if(result != CRAZYPOD_MINIAPP_OK)
        return result;
    native_open = true;
    return CRAZYPOD_MINIAPP_OK;
}

void crazypod_miniapp_native_close(void)
{
    ++close_count;
    native_open = false;
}

bool crazypod_miniapp_native_is_open(void)
{
    return native_open;
}

bool crazypod_miniapp_native_event(
    const struct cp_input_event *event)
{
    (void)event;
    if(close_on_event) {
        close_on_event = false;
        native_open = false;
    }
    return false;
}

bool crazypod_miniapp_native_ui_event(
    uint32_t handler, uint8_t event_type,
    uint32_t target, int32_t value)
{
    (void)handler;
    (void)event_type;
    (void)target;
    (void)value;
    native_open = false;
    return true;
}

bool crazypod_miniapp_native_tick(void)
{
    return false;
}

bool crazypod_miniapp_native_has_scheduled_work(void)
{
    return false;
}

void crazypod_miniapp_host_session_finish(void)
{
    ++finish_count;
    session_active = false;
}

int crazypod_miniapp_resource_stat(
    const struct crazypod_miniapp_metadata *metadata,
    const char *id, struct cp_resource_info *info)
{
    (void)metadata;
    (void)id;
    (void)info;
    return CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapp_resource_read(
    const struct crazypod_miniapp_metadata *metadata,
    const char *id, uint32_t offset,
    void *buffer, size_t capacity)
{
    (void)metadata;
    (void)id;
    (void)offset;
    (void)buffer;
    (void)capacity;
    return CRAZYPOD_MINIAPP_ERROR_STATE;
}

static bool scene_input(const struct cp_input_event *event)
{
    (void)event;
    return false;
}

static void reset_scene(void)
{
    ++scene_reset_count;
}

static void assert_clean(void)
{
    assert(!crazypod_miniapps_is_open());
    assert(crazypod_miniapps_current() == -1);
    assert(!native_open);
    assert(!session_active);
}

int main(void)
{
    const struct crazypod_miniapp_ui_host host = {
        .input = scene_input,
        .reset = reset_scene,
    };
    const struct cp_input_event event = {
        .struct_size = sizeof(event),
        .type = CP_INPUT_SELECT,
        .steps = 1,
    };

    crazypod_miniapps_set_ui_host(&host);
    assert(crazypod_miniapps_open_id("game2048") ==
           CRAZYPOD_MINIAPP_OK);
    assert(crazypod_miniapps_open_id("capability-lab") ==
           CRAZYPOD_MINIAPP_ERROR_BUSY);
    assert(open_count == 1);
    crazypod_miniapps_close();
    assert_clean();
    assert(close_count == 1);
    assert(finish_count == 1);
    assert(scene_reset_count == 1);

    assert(crazypod_miniapps_open_id("game2048") ==
           CRAZYPOD_MINIAPP_OK);
    close_on_event = true;
    (void)crazypod_miniapps_event(&event);
    assert_clean();
    assert(finish_count == 2);

    assert(crazypod_miniapps_open_id("capability-lab") ==
           CRAZYPOD_MINIAPP_OK);
    (void)crazypod_miniapps_ui_event(
        1, CP_UI_EVENT_SELECT, 1, 0);
    assert_clean();

    next_open_result = CRAZYPOD_MINIAPP_ERROR_ABI;
    assert(crazypod_miniapps_open_id("game2048") ==
           CRAZYPOD_MINIAPP_ERROR_ABI);
    assert_clean();
    return 0;
}
