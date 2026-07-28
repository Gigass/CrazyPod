#ifndef CRAZYPOD_SETTINGS_CONTROLLER_H
#define CRAZYPOD_SETTINGS_CONTROLLER_H

#include "../../../crazypod_apps.h"
#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_settings_command_kind {
    CRAZYPOD_SETTINGS_COMMAND_NONE,
    CRAZYPOD_SETTINGS_COMMAND_PUSH_ROUTE,
    CRAZYPOD_SETTINGS_COMMAND_MAIN_MENU_CHANGED,
    CRAZYPOD_SETTINGS_COMMAND_OPEN_EQ,
    CRAZYPOD_SETTINGS_COMMAND_SHOW_CHOICES,
};

struct crazypod_settings_command {
    enum crazypod_settings_command_kind kind;
    enum crazypod_route route;
    enum crazypod_app_id app_id;
    int item;
};

struct crazypod_settings_command crazypod_settings_activate(
    const struct route_state *state);

#endif
