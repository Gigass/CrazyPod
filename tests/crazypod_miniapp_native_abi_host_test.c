#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "crazypod_miniapp_native.h"

static void verify_constants(void)
{
    assert(CP_NATIVE_ABI_MAJOR == 1u);
    assert(CP_NATIVE_ABI_MINOR == 2u);
    assert(CP_NATIVE_PACKAGE_FORMAT == 5u);
    assert(CP_NATIVE_REACT_PROFILE == 1u);
    assert(CP_NATIVE_CAP_FILES == (1u << 4));
    assert(CP_UI_OBJECT_SCREEN == 1);
    assert(CP_UI_OBJECT_TYPE_COUNT < 256);
    assert(CP_UI_PROP_COUNT < 65536);
}

static void verify_layout(void)
{
    assert(offsetof(struct cp_native_ui_api, begin_update) == 8u);
    assert(offsetof(struct cp_native_host_api, ui) >= 12u);
    assert(offsetof(struct cp_native_host_api, file_size) >
           offsetof(struct cp_native_host_api, log));
    assert(offsetof(struct cp_native_host_api, file_read) ==
           offsetof(struct cp_native_host_api, file_size) +
               sizeof(((struct cp_native_host_api *)0)->file_size));
    assert(offsetof(struct cp_native_host_api, file_write) ==
           offsetof(struct cp_native_host_api, file_read) +
               sizeof(((struct cp_native_host_api *)0)->file_read));
    assert(offsetof(struct cp_native_host_api, file_remove) ==
           offsetof(struct cp_native_host_api, file_write) +
               sizeof(((struct cp_native_host_api *)0)->file_write));
    assert(sizeof(struct cp_native_host_api) ==
           offsetof(struct cp_native_host_api, file_remove) +
               sizeof(((struct cp_native_host_api *)0)->file_remove));
    assert(sizeof(struct cp_input_event) == 8u);
    assert(sizeof(struct cp_resource_info) == 20u);
    assert(sizeof(struct cp_native_ui_animation) == 24u);
    assert(CP_NATIVE_HOST_V1_SIZE ==
           sizeof(struct cp_native_host_api));
    assert(CP_NATIVE_UI_V1_SIZE ==
           sizeof(struct cp_native_ui_api));
    assert(CP_NATIVE_APP_OPS_V1_SIZE ==
           sizeof(struct cp_native_app_ops));
}

int main(void)
{
    verify_constants();
    verify_layout();
    return 0;
}
