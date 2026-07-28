#ifndef CRAZYPOD_NAVIGATION_COMMAND_H
#define CRAZYPOD_NAVIGATION_COMMAND_H

#include <stdbool.h>

#include "crazypod_ui_routes.h"

enum crazypod_navigation_command_kind {
    CRAZYPOD_NAVIGATION_NONE = 0,
    CRAZYPOD_NAVIGATION_PUSH,
    CRAZYPOD_NAVIGATION_POP,
    CRAZYPOD_NAVIGATION_RENDER,
};

struct crazypod_navigation_command {
    enum crazypod_navigation_command_kind kind;
    enum crazypod_route route;
    int group;
    int selected;
    bool transition;
};

struct crazypod_navigation_command crazypod_navigation_none(void);
struct crazypod_navigation_command crazypod_navigation_push(
    enum crazypod_route route, int group, int selected);
struct crazypod_navigation_command crazypod_navigation_pop(void);
struct crazypod_navigation_command crazypod_navigation_render(
    bool transition);

#endif
