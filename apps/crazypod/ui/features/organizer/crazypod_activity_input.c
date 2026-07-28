#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_activity_controller.h"
#include "crazypod_activity_input.h"

bool crazypod_activity_input_handle(
    enum crazypod_route route,
    const struct crazypod_input_event *event,
    long now, int ticks_per_second,
    const struct crazypod_activity_input_actions *actions)
{
    if(route == WORKOUT_ROUTE_ACTIVE) {
        if(event->base == BUTTON_SELECT && !event->repeated)
            actions->activate();
        else if((event->base == BUTTON_PLAY ||
                 event->base == BUTTON_MENU) &&
                !event->repeated) {
            crazypod_activity_workout_pause(now);
            actions->show_finish_confirmation();
        }
        return true;
    }
    if(route != STOPWATCH_ROUTE_VIEW)
        return false;

    if(event->base == BUTTON_SCROLL_FWD ||
       event->base == BUTTON_SCROLL_BACK) {
        int direction = crazypod_input_wheel_steps(event, 12);

        if(event->base == BUTTON_SCROLL_BACK)
            direction = -direction;
        if(crazypod_activity_stopwatch_change_style(direction))
            actions->render();
    }
    else if(event->base == BUTTON_SELECT && !event->repeated)
        actions->activate();
    else if(event->base == BUTTON_RIGHT && !event->repeated) {
        if(crazypod_activity_stopwatch_add_lap(now))
            actions->render();
    }
    else if((event->base == BUTTON_LEFT ||
             event->base == BUTTON_PLAY) &&
            !event->repeated) {
        if(crazypod_activity_stopwatch_reset(
               now, ticks_per_second))
            actions->render();
        else if(event->base == BUTTON_PLAY &&
                crazypod_activity_stopwatch_add_lap(now))
            actions->render();
    }
    else if(event->base == BUTTON_MENU && !event->repeated) {
        crazypod_activity_stopwatch_leave();
        actions->leave();
    }
    return true;
}

#endif
