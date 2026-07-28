#ifndef CRAZYPOD_ACTIVITY_INPUT_H
#define CRAZYPOD_ACTIVITY_INPUT_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"
#include "../../navigation/crazypod_input_event.h"

struct crazypod_activity_input_actions {
    void (*activate)(void);
    void (*render)(void);
    void (*show_finish_confirmation)(void);
    void (*leave)(void);
};

bool crazypod_activity_input_handle(
    enum crazypod_route route,
    const struct crazypod_input_event *event,
    long now, int ticks_per_second,
    const struct crazypod_activity_input_actions *actions);

#endif
