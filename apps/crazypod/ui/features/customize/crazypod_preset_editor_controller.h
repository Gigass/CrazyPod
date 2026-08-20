#ifndef CRAZYPOD_PRESET_EDITOR_CONTROLLER_H
#define CRAZYPOD_PRESET_EDITOR_CONTROLLER_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_preset_command_action {
    CRAZYPOD_PRESET_COMMAND_NONE = 0,
    CRAZYPOD_PRESET_COMMAND_RENDER,
    CRAZYPOD_PRESET_COMMAND_PUSH_ACTIONS,
    CRAZYPOD_PRESET_COMMAND_PUSH_LIBRARY,
    CRAZYPOD_PRESET_COMMAND_PUSH_EDIT,
    CRAZYPOD_PRESET_COMMAND_PUSH_RENAME,
    CRAZYPOD_PRESET_COMMAND_POP,
    CRAZYPOD_PRESET_COMMAND_APPLIED,
    CRAZYPOD_PRESET_COMMAND_DELETED,
    CRAZYPOD_PRESET_COMMAND_FAILED,
};

struct crazypod_preset_command_result {
    enum crazypod_preset_command_action action;
    int preset_index;
};

void crazypod_preset_editor_begin(void);
const char *crazypod_preset_editor_value(void);
void crazypod_preset_editor_append(const char *text);
void crazypod_preset_editor_backspace(void);
bool crazypod_preset_editor_commit(int preset_index);
bool crazypod_preset_controller_handles(enum crazypod_route route);
struct crazypod_preset_command_result
crazypod_preset_controller_select(
    enum crazypod_route route, int selected, int group,
    const char *editor_character);

#endif
