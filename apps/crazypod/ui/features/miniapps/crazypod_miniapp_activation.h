#ifndef CRAZYPOD_MINIAPP_ACTIVATION_H
#define CRAZYPOD_MINIAPP_ACTIVATION_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"

struct crazypod_miniapp_activation_result {
    bool handled;
    bool opened;
    int selected;
    int error;
};

struct crazypod_miniapp_activation_result
crazypod_miniapp_activation_execute(
    enum crazypod_route route, int selected);

#endif
