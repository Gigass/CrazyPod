#ifndef CRAZYPOD_CLOCK_ACTIVATION_H
#define CRAZYPOD_CLOCK_ACTIVATION_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_clock_activation_kind {
    CRAZYPOD_CLOCK_ACTIVATION_UNHANDLED = 0,
    CRAZYPOD_CLOCK_ACTIVATION_NONE,
    CRAZYPOD_CLOCK_ACTIVATION_PUSH,
    CRAZYPOD_CLOCK_ACTIVATION_RENDER,
};

struct crazypod_clock_activation_result {
    enum crazypod_clock_activation_kind kind;
    enum crazypod_route route;
};

struct crazypod_clock_activation_result
crazypod_clock_activation_execute(struct route_state *state);

#endif
