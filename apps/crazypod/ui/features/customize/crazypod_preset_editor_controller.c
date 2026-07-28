#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <string.h>

#include "../../../crazypod_presets.h"
#include "../../../crazypod_wallpaper.h"
#include "crazypod_preset_editor_controller.h"

#define PRESET_EDITOR_CHARACTER_COUNT 36

static char name_buffer[CRAZYPOD_PRESET_NAME_SIZE];

void crazypod_preset_editor_begin(void)
{
    name_buffer[0] = '\0';
}

const char *crazypod_preset_editor_value(void)
{
    return name_buffer;
}

void crazypod_preset_editor_append(const char *text)
{
    size_t used;
    size_t available;
    size_t append_length;

    if(text == NULL)
        return;
    used = strlen(name_buffer);
    if(used >= sizeof(name_buffer) - 1)
        return;
    available = sizeof(name_buffer) - used - 1;
    append_length = strlen(text);
    if(append_length > available)
        append_length = available;
    memcpy(name_buffer + used, text, append_length);
    name_buffer[used + append_length] = '\0';
}

void crazypod_preset_editor_backspace(void)
{
    size_t length = strlen(name_buffer);

    if(length > 0)
        name_buffer[length - 1] = '\0';
}

bool crazypod_preset_editor_commit(int preset_index)
{
    return crazypod_preset_rename(preset_index, name_buffer);
}

bool crazypod_preset_controller_handles(enum crazypod_route route)
{
    return route >= DIY_ROUTE_PRESETS &&
        route <= DIY_ROUTE_PRESET_RENAME;
}

struct crazypod_preset_command_result
crazypod_preset_controller_select(
    enum crazypod_route route, int selected, int group,
    const char *editor_character)
{
    struct crazypod_preset_command_result result = {
        .action = CRAZYPOD_PRESET_COMMAND_NONE,
        .preset_index = group,
    };

    switch(route) {
    case DIY_ROUTE_PRESETS:
        if(selected == 0) {
            result.preset_index = crazypod_preset_save_current();
            result.action = result.preset_index >= 0
                ? CRAZYPOD_PRESET_COMMAND_PUSH_ACTIONS
                : CRAZYPOD_PRESET_COMMAND_RENDER;
        }
        else if(selected == 1)
            result.action = CRAZYPOD_PRESET_COMMAND_PUSH_LIBRARY;
        else {
            result.preset_index = crazypod_preset_import();
            result.action = result.preset_index >= 0
                ? CRAZYPOD_PRESET_COMMAND_PUSH_ACTIONS
                : CRAZYPOD_PRESET_COMMAND_RENDER;
        }
        break;
    case DIY_ROUTE_PRESET_LIBRARY:
        result.preset_index = selected;
        result.action = CRAZYPOD_PRESET_COMMAND_PUSH_ACTIONS;
        break;
    case DIY_ROUTE_PRESET_ACTIONS: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(group);

        if(preset == NULL) {
            result.action = CRAZYPOD_PRESET_COMMAND_POP;
            break;
        }
        if(selected == 0) {
            crazypod_preset_apply(group);
            crazypod_wallpaper_reload_custom();
            result.action = CRAZYPOD_PRESET_COMMAND_APPLIED;
        }
        else if(selected == 1) {
            crazypod_preset_export(group);
            result.action = CRAZYPOD_PRESET_COMMAND_RENDER;
        }
        else if(!preset->builtin)
            result.action = CRAZYPOD_PRESET_COMMAND_PUSH_EDIT;
        break;
    }
    case DIY_ROUTE_PRESET_EDIT:
        if(selected == 0) {
            crazypod_preset_editor_begin();
            result.action = CRAZYPOD_PRESET_COMMAND_PUSH_RENAME;
        }
        else if(selected == 1) {
            crazypod_preset_update(group);
            result.action = CRAZYPOD_PRESET_COMMAND_RENDER;
        }
        else if(crazypod_preset_delete(group))
            result.action = CRAZYPOD_PRESET_COMMAND_DELETED;
        break;
    case DIY_ROUTE_PRESET_RENAME:
        if(editor_character != NULL)
            crazypod_preset_editor_append(editor_character);
        else if(selected == PRESET_EDITOR_CHARACTER_COUNT)
            crazypod_preset_editor_append(" ");
        else if(selected == PRESET_EDITOR_CHARACTER_COUNT + 1)
            crazypod_preset_editor_backspace();
        else if(crazypod_preset_editor_commit(group))
            result.action = CRAZYPOD_PRESET_COMMAND_POP;
        if(result.action == CRAZYPOD_PRESET_COMMAND_NONE)
            result.action = CRAZYPOD_PRESET_COMMAND_RENDER;
        break;
    default:
        break;
    }
    return result;
}

#endif
