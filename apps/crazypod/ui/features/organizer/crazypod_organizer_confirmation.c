#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_organizer.h"
#include "crazypod_activity_controller.h"
#include "crazypod_organizer_confirmation.h"

struct crazypod_organizer_confirmation_result
crazypod_organizer_confirmation_execute(
    const struct route_state *state, long now, int ticks_per_second,
    int fallback_date)
{
    struct crazypod_organizer_confirmation_result result = { 0 };

    if(state->route == CALENDAR_ROUTE_DELETE_CONFIRM) {
        const struct crazypod_calendar_event *event =
            crazypod_calendar_event_find((uint32_t)state->group);

        result.handled = true;
        result.date = event != NULL ? event->date : fallback_date;
        result.succeeded =
            crazypod_calendar_event_delete((uint32_t)state->group);
        result.navigation =
            CRAZYPOD_ORGANIZER_CONFIRMATION_SHOW_CALENDAR_DAY;
    }
    else if(state->route == WORKOUT_ROUTE_FINISH_CONFIRM) {
        result.handled = true;
        result.succeeded = crazypod_activity_workout_finish(
            now, ticks_per_second);
        result.navigation =
            CRAZYPOD_ORGANIZER_CONFIRMATION_RESET_WORKOUT_MENU;
    }
    else if(state->route == WORKOUT_ROUTE_DELETE_CONFIRM) {
        result.handled = true;
        result.succeeded = crazypod_activity_workout_delete(
            (uint32_t)state->group);
        result.navigation =
            CRAZYPOD_ORGANIZER_CONFIRMATION_SHOW_WORKOUT_HISTORY;
    }
    return result;
}

#endif
