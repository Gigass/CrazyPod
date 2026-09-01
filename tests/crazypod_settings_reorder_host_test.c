#include <assert.h>

#include "crazypod_apps.h"
#include "features/settings/crazypod_settings_catalog.h"
#include "features/settings/crazypod_settings_controller.h"

bool crazypod_settings_catalog_handles(enum crazypod_route route)
{
    return route >= SETTINGS_ROUTE_MENU &&
        route <= SETTINGS_ROUTE_MAIN_MENU_ACTIONS;
}

int crazypod_settings_catalog_count(enum crazypod_route route)
{
    (void)route;
    return 0;
}

int crazypod_settings_catalog_item(
    enum crazypod_route route, int index)
{
    (void)route;
    (void)index;
    return -1;
}

static void test_main_menu_opens_actions_overlay(void)
{
    struct route_state state = {
        .route = SETTINGS_ROUTE_MAIN_MENU,
        .selected = 2,
        .group = -1,
    };
    struct crazypod_settings_command command;

    crazypod_apps_reset();
    command = crazypod_settings_activate(&state);
    assert(command.kind ==
        CRAZYPOD_SETTINGS_COMMAND_SHOW_MAIN_MENU_ACTIONS);
    assert(command.app_id == CRAZYPOD_APP_MINI_APPS);
}

static void test_direct_reorder_transaction(void)
{
    enum crazypod_app_id id = CRAZYPOD_APP_MINI_APPS;
    int original;

    crazypod_apps_reset();
    original = crazypod_apps_order_index(id);
    crazypod_settings_begin_main_menu_reorder(id);
    assert(crazypod_settings_main_menu_reordering());
    assert(crazypod_settings_main_menu_reorder_id() == id);
    assert(crazypod_settings_move_main_menu_item(1));
    assert(crazypod_apps_order_index(id) == original + 1);
    assert(crazypod_settings_move_main_menu_item(-1));
    assert(crazypod_apps_order_index(id) == original);
    assert(crazypod_settings_finish_main_menu_reorder() == id);
    assert(!crazypod_settings_main_menu_reordering());
    assert(!crazypod_settings_move_main_menu_item(1));
}

static void test_reorder_respects_bounds(void)
{
    crazypod_apps_reset();
    crazypod_settings_begin_main_menu_reorder(
        CRAZYPOD_APP_MUSIC);
    assert(!crazypod_settings_move_main_menu_item(-1));
    assert(crazypod_apps_order_index(
        CRAZYPOD_APP_MUSIC) == 0);
    assert(crazypod_settings_finish_main_menu_reorder() ==
        CRAZYPOD_APP_MUSIC);
}

static void test_gameboy_is_visible_after_legacy_restore(void)
{
    uint8_t legacy_order[CRAZYPOD_APP_LEGACY_COUNT];
    uint32_t enabled_mask;

    crazypod_apps_reset();
    assert(crazypod_apps_is_enabled(CRAZYPOD_APP_GAMEBOY));
    assert(crazypod_apps_visible_index(CRAZYPOD_APP_GAMEBOY) >= 0);

    crazypod_apps_export(legacy_order, sizeof(legacy_order), &enabled_mask);
    enabled_mask &= ~((uint32_t)1u << CRAZYPOD_APP_GAMEBOY);
    crazypod_apps_restore(legacy_order, sizeof(legacy_order), enabled_mask);
    assert(crazypod_apps_is_enabled(CRAZYPOD_APP_GAMEBOY));
    assert(crazypod_apps_visible_index(CRAZYPOD_APP_GAMEBOY) >= 0);
}

int main(void)
{
    test_main_menu_opens_actions_overlay();
    test_direct_reorder_transaction();
    test_reorder_respects_bounds();
    test_gameboy_is_visible_after_legacy_restore();
    return 0;
}
