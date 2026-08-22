#ifndef CRAZYPOD_SETTINGS_CATALOG_H
#define CRAZYPOD_SETTINGS_CATALOG_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"

#define CRAZYPOD_SETTINGS_MENU_COUNT 8

extern const char *const crazypod_settings_menu_titles[
    CRAZYPOD_SETTINGS_MENU_COUNT];
extern const char *const crazypod_settings_menu_symbols[
    CRAZYPOD_SETTINGS_MENU_COUNT];

bool crazypod_settings_catalog_handles(enum crazypod_route route);
int crazypod_settings_catalog_count(enum crazypod_route route);
int crazypod_settings_catalog_item(
    enum crazypod_route route, int index);

#endif
