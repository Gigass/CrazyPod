#ifndef CRAZYPOD_ORGANIZER_CONFIRMATION_H
#define CRAZYPOD_ORGANIZER_CONFIRMATION_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_organizer_confirmation_navigation {
    CRAZYPOD_ORGANIZER_CONFIRMATION_NONE = 0,
    CRAZYPOD_ORGANIZER_CONFIRMATION_SHOW_CALENDAR_DAY,
    CRAZYPOD_ORGANIZER_CONFIRMATION_RESET_WORKOUT_MENU,
    CRAZYPOD_ORGANIZER_CONFIRMATION_SHOW_WORKOUT_HISTORY,
};

struct crazypod_organizer_confirmation_result {
    bool handled;
    bool succeeded;
    enum crazypod_organizer_confirmation_navigation navigation;
    int date;
};

struct crazypod_organizer_confirmation_result
crazypod_organizer_confirmation_execute(
    const struct route_state *state, long now, int ticks_per_second,
    int fallback_date);

#endif
