#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "crazypod_miniapp_manifest.h"
#include "crazypod_miniapp_catalog.h"
#include "crazypod_miniapp_modal.h"
#include "crazypod_miniapp_notification.h"

static const char valid_manifest[] =
    "format=1\n"
    "id=focus_timer\n"
    "name=Focus Timer\n"
    "version=1.0\n"
    "version_code=1\n"
    "abi=1\n"
    "target=simulator\n"
    "binary=app.dylib\n"
    "binary_sha256="
    "0000000000000000000000000000000000000000000000000000000000000000\n"
    "icon=icon.bmp\n"
    "icon_sha256="
    "1111111111111111111111111111111111111111111111111111111111111111\n"
    "symbol=F\n"
    "accent=26CFF5\n"
    "summary=Simple focus timer\n";

static int parse(
    const char *text, struct crazypod_miniapp_metadata *metadata)
{
    char buffer[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    size_t size = strlen(text);

    assert(size <= CRAZYPOD_MINIAPP_MANIFEST_MAX);
    memcpy(buffer, text, size);
    return crazypod_miniapp_manifest_parse(buffer, size, metadata);
}

int main(void)
{
    struct crazypod_miniapp_metadata metadata;
    struct crazypod_miniapp_metadata second;
    char duplicate[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];
    char bad_abi[CRAZYPOD_MINIAPP_MANIFEST_MAX + 1];

    assert(parse(valid_manifest, &metadata) == CRAZYPOD_MINIAPP_OK);
    assert(strcmp(metadata.id, "focus_timer") == 0);
    assert(metadata.package_format == 1);
    assert(metadata.accent_rgb == 0x26CFF5);
    assert(metadata.binary_sha256[0] == 0);
    assert(metadata.icon_sha256[0] == 0x11);

    snprintf(duplicate, sizeof(duplicate), "%sid=again\n", valid_manifest);
    assert(parse(duplicate, &metadata) ==
           CRAZYPOD_MINIAPP_ERROR_MANIFEST);

    snprintf(bad_abi, sizeof(bad_abi), "%s", valid_manifest);
    {
        char *abi = strstr(bad_abi, "abi=1");
        assert(abi != NULL);
        abi[4] = '2';
    }
    assert(parse(bad_abi, &metadata) == CRAZYPOD_MINIAPP_ERROR_ABI);

    assert(!crazypod_miniapp_text_valid("bad\x01text", true));
    assert(!crazypod_miniapp_text_valid("two words", false));
    assert(crazypod_miniapp_text_valid("计时器", true));

    second = metadata;
    snprintf(second.id, sizeof(second.id), "alarm");
    crazypod_miniapp_catalog_reset();
    assert(crazypod_miniapp_catalog_add(&metadata));
    assert(crazypod_miniapp_catalog_add(&second));
    crazypod_miniapp_catalog_sort();
    assert(crazypod_miniapp_catalog_count() == 2);
    assert(strcmp(crazypod_miniapp_catalog_get(0)->id, "alarm") == 0);
    assert(crazypod_miniapp_catalog_find("focus_timer") == 1);
    assert(crazypod_miniapp_catalog_get(-1) == NULL);

    {
        struct cp_ui_choice_item items[2] = {
            { .label = "First" },
            { .label = "Second" },
        };
        struct cp_input_event event = {
            .struct_size = sizeof(event),
            .type = CP_INPUT_WHEEL_CLOCKWISE,
            .steps = 1,
        };
        struct cp_ui_result result = {
            .struct_size = sizeof(result),
        };
        struct cp_scene scene;

        crazypod_miniapp_modal_open();
        assert(crazypod_miniapp_modal_choice(
            7, "Choose", items, 2, 0) == CRAZYPOD_MINIAPP_OK);
        cp_scene_reset(&scene);
        crazypod_miniapp_modal_render(&scene);
        assert(scene.command_count > 0);
        assert(crazypod_miniapp_modal_event(&event));
        event.type = CP_INPUT_SELECT;
        assert(crazypod_miniapp_modal_event(&event));
        assert(crazypod_miniapp_modal_poll(&result) == 1);
        assert(result.request_id == 7);
        assert(result.status == CP_UI_RESULT_ACCEPTED);
        assert(result.selected_index == 1);
        assert(crazypod_miniapp_modal_poll(&result) == 0);
        crazypod_miniapp_modal_close();
    }

    {
        char toast[CP_MINIAPP_TOAST_TEXT_SIZE];

        crazypod_miniapp_notification_reset();
        assert(crazypod_miniapp_notification_show(
            "Saved", 1000, 10, 100) == CRAZYPOD_MINIAPP_OK);
        assert(crazypod_miniapp_notification_take_changed(10));
        assert(crazypod_miniapp_notification_get(
            50, toast, sizeof(toast)));
        assert(strcmp(toast, "Saved") == 0);
        assert(crazypod_miniapp_notification_take_changed(110));
        assert(!crazypod_miniapp_notification_get(
            110, toast, sizeof(toast)));
    }
    return 0;
}
