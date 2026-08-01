#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_home_input.h"

#define HOME_WHEEL_MAX_STEPS 3
#define HOME_WHEEL_HYSTERESIS 2

void crazypod_home_wheel_filter_reset(
    struct crazypod_home_wheel_filter *filter)
{
    filter->pending_delta = 0;
    filter->direction = 0;
}

int crazypod_home_wheel_filter_apply(
    struct crazypod_home_wheel_filter *filter, int delta)
{
    int pending_direction;
    int result;

    if(delta == 0)
        return 0;
    if(filter->direction != 0 &&
       filter->pending_delta == 0 &&
       (delta < 0 ? -1 : 1) == filter->direction)
        return delta;
    filter->pending_delta += delta;
    if(filter->pending_delta == 0)
        return 0;
    pending_direction = filter->pending_delta < 0 ? -1 : 1;
    if(pending_direction == filter->direction) {
        result = filter->pending_delta;
        filter->pending_delta = 0;
        return result;
    }
    if(filter->pending_delta > -HOME_WHEEL_HYSTERESIS &&
       filter->pending_delta < HOME_WHEEL_HYSTERESIS)
        return 0;
    filter->direction = pending_direction;
    result = filter->pending_delta -
        pending_direction * (HOME_WHEEL_HYSTERESIS - 1);
    filter->pending_delta = 0;
    return result;
}

void crazypod_home_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_home_input_actions *actions)
{
    if(event->base == BUTTON_SCROLL_FWD) {
        actions->move_selection(
            crazypod_input_wheel_steps(
                event, HOME_WHEEL_MAX_STEPS));
        return;
    }
    if(event->base == BUTTON_SCROLL_BACK) {
        actions->move_selection(
            -crazypod_input_wheel_steps(
                event, HOME_WHEEL_MAX_STEPS));
        return;
    }
    if(event->base == BUTTON_RIGHT && !event->repeated)
        actions->next_track();
    else if(event->base == BUTTON_LEFT && !event->repeated)
        actions->previous_track();
    else if(event->base == BUTTON_SELECT)
        actions->open_selected_app();
    else if(event->base == BUTTON_PLAY && !event->repeated)
        actions->toggle_playback();
}

#endif
