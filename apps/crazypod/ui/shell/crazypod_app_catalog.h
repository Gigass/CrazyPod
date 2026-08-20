#ifndef CRAZYPOD_APP_CATALOG_H
#define CRAZYPOD_APP_CATALOG_H

#include <stdint.h>

#include "../../crazypod_apps.h"
#include "../crazypod_menu_icon.h"

struct crazypod_app_descriptor {
    enum crazypod_app_id id;
    const char *name;
    const char *symbol;
    enum crazypod_menu_icon menu_icon;
    uint32_t color;
};

const struct crazypod_app_descriptor *crazypod_app_catalog_at(int index);
const struct crazypod_app_descriptor *crazypod_app_catalog_find(
    enum crazypod_app_id id);
int crazypod_app_catalog_index(enum crazypod_app_id id);

#endif
