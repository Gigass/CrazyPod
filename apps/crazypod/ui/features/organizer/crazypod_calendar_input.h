#ifndef CRAZYPOD_CALENDAR_INPUT_H
#define CRAZYPOD_CALENDAR_INPUT_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"
#include "../../navigation/crazypod_input_event.h"

struct crazypod_calendar_input_actions {
    void (*move_selection)(int direction);
    void (*activate)(void);
    void (*render)(void);
    /* Restyles the month grid in place; false when the pane has to be
     * rebuilt after all (the focus crossed into another month). */
    bool (*refocus)(void);
    void (*leave)(void);
};

bool crazypod_calendar_input_handle(
    const struct route_state *state,
    const struct crazypod_input_event *event, int today,
    const struct crazypod_calendar_input_actions *actions);

#endif
