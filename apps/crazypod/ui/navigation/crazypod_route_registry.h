#ifndef CRAZYPOD_ROUTE_REGISTRY_H
#define CRAZYPOD_ROUTE_REGISTRY_H

#include <stdbool.h>

#include "crazypod_ui_routes.h"
#include "../features/crazypod_feature.h"

enum crazypod_route_owner {
    CRAZYPOD_ROUTE_OWNER_FEATURE = 0,
    CRAZYPOD_ROUTE_OWNER_SHELL,
};

enum crazypod_route_flag {
    CRAZYPOD_ROUTE_FLAG_NONE = 0,
    CRAZYPOD_ROUTE_FLAG_FULLSCREEN = 1 << 0,
    CRAZYPOD_ROUTE_FLAG_SOLID_BLACK = 1 << 1,
    CRAZYPOD_ROUTE_FLAG_DARK_STATUS = 1 << 2,
    CRAZYPOD_ROUTE_FLAG_BOOK_READER = 1 << 3,
    CRAZYPOD_ROUTE_FLAG_PREVIEW = 1 << 4,
    CRAZYPOD_ROUTE_FLAG_SKEUOMORPHIC_PREVIEW = 1 << 5,
};

struct crazypod_route_descriptor {
    enum crazypod_route route;
    enum crazypod_route_owner owner;
    const struct crazypod_feature *feature;
    unsigned int flags;
};

const struct crazypod_route_descriptor *
crazypod_route_registry_get(enum crazypod_route route);
const struct crazypod_feature *
crazypod_route_registry_feature(enum crazypod_route route);
bool crazypod_route_registry_is_shell(enum crazypod_route route);
bool crazypod_route_registry_has_flag(
    enum crazypod_route route, enum crazypod_route_flag flag);
bool crazypod_route_registry_validate(void);

#endif
