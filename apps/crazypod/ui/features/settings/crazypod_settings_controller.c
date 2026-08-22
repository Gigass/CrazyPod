#include "crazypod_settings_controller.h"

#include "crazypod_settings_model.h"
#include "crazypod_settings_catalog.h"

static enum crazypod_app_id reorder_id = CRAZYPOD_APP_INVALID;

static struct crazypod_settings_command command(
    enum crazypod_settings_command_kind kind)
{
    struct crazypod_settings_command result = {
        .kind = kind,
        .route = SETTINGS_ROUTE_MENU,
        .app_id = CRAZYPOD_APP_INVALID,
        .item = -1,
    };
    return result;
}

static enum crazypod_route settings_group_route(int index)
{
    static const enum crazypod_route routes[] = {
        SETTINGS_ROUTE_SOUND,
        SETTINGS_ROUTE_DISPLAY,
        SETTINGS_ROUTE_DATE_TIME,
        SETTINGS_ROUTE_PLAYBACK,
        SETTINGS_ROUTE_POWER,
        SETTINGS_ROUTE_CONTROLS,
        SETTINGS_ROUTE_MAIN_MENU,
    };

    if(index < 0 || index >= (int)(sizeof(routes) / sizeof(routes[0])))
        return SETTINGS_ROUTE_MENU;
    return routes[index];
}

struct crazypod_settings_command crazypod_settings_activate(
    const struct route_state *state)
{
    struct crazypod_settings_command result;
    enum crazypod_app_id id;
    int action;

    if(state == NULL)
        return command(CRAZYPOD_SETTINGS_COMMAND_NONE);

    if(state->route == SETTINGS_ROUTE_MENU) {
        if(state->selected == CRAZYPOD_SETTINGS_MENU_COUNT - 1) {
            result = command(CRAZYPOD_SETTINGS_COMMAND_SHOW_CHOICES);
            result.item = SETTINGS_ITEM_LANGUAGE;
            return result;
        }
        result = command(CRAZYPOD_SETTINGS_COMMAND_PUSH_ROUTE);
        result.route = settings_group_route(state->selected);
        if(result.route == SETTINGS_ROUTE_MENU)
            result.kind = CRAZYPOD_SETTINGS_COMMAND_NONE;
        return result;
    }

    if(state->route == SETTINGS_ROUTE_MAIN_MENU) {
        id = crazypod_apps_ordered_id(state->selected);
        if(!crazypod_apps_is_known(id))
            return command(CRAZYPOD_SETTINGS_COMMAND_NONE);
        result = command(
            CRAZYPOD_SETTINGS_COMMAND_SHOW_MAIN_MENU_ACTIONS);
        result.app_id = id;
        return result;
    }

    if(state->route == SETTINGS_ROUTE_MAIN_MENU_ACTIONS) {
        id = (enum crazypod_app_id)state->group;
        if(!crazypod_apps_is_known(id))
            return command(CRAZYPOD_SETTINGS_COMMAND_NONE);
        action = state->selected + (crazypod_apps_is_fixed(id) ? 1 : 0);
        if((action == 0 &&
            crazypod_apps_set_enabled(id, !crazypod_apps_is_enabled(id))) ||
           (action == 1 && crazypod_apps_move(id, -1)) ||
           (action == 2 && crazypod_apps_move(id, 1))) {
            result = command(CRAZYPOD_SETTINGS_COMMAND_MAIN_MENU_CHANGED);
            result.app_id = id;
            return result;
        }
        return command(CRAZYPOD_SETTINGS_COMMAND_NONE);
    }

    if(crazypod_settings_catalog_handles(state->route)) {
        result = command(CRAZYPOD_SETTINGS_COMMAND_SHOW_CHOICES);
        result.item = crazypod_settings_catalog_item(
            state->route, state->selected);
        if(result.item == SETTINGS_ITEM_EQ_ENABLED)
            result.kind = CRAZYPOD_SETTINGS_COMMAND_OPEN_EQ;
        return result;
    }

    return command(CRAZYPOD_SETTINGS_COMMAND_NONE);
}

void crazypod_settings_begin_main_menu_reorder(
    enum crazypod_app_id id)
{
    reorder_id = crazypod_apps_is_known(id)
        ? id : CRAZYPOD_APP_INVALID;
}

bool crazypod_settings_main_menu_reordering(void)
{
    return reorder_id != CRAZYPOD_APP_INVALID;
}

enum crazypod_app_id crazypod_settings_main_menu_reorder_id(void)
{
    return reorder_id;
}

bool crazypod_settings_move_main_menu_item(int direction)
{
    return crazypod_settings_main_menu_reordering() &&
        crazypod_apps_move(reorder_id, direction);
}

enum crazypod_app_id crazypod_settings_finish_main_menu_reorder(void)
{
    enum crazypod_app_id result = reorder_id;

    reorder_id = CRAZYPOD_APP_INVALID;
    return result;
}
