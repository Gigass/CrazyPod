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
    CRAZYPOD_SETTINGS_COMMAND_SHOW_MAIN_MENU_ACTIONS,
};

struct crazypod_settings_command {
    enum crazypod_settings_command_kind kind;
    enum crazypod_route route;
    enum crazypod_app_id app_id;
    int item;
};

struct crazypod_settings_command crazypod_settings_activate(
    const struct route_state *state);
void crazypod_settings_begin_main_menu_reorder(
    enum crazypod_app_id id);
bool crazypod_settings_main_menu_reordering(void);
enum crazypod_app_id crazypod_settings_main_menu_reorder_id(void);
bool crazypod_settings_move_main_menu_item(int direction);
enum crazypod_app_id crazypod_settings_finish_main_menu_reorder(void);

#endif
