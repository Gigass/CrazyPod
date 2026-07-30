#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_home_input.h"

#define HOME_WHEEL_MAX_STEPS 3

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
