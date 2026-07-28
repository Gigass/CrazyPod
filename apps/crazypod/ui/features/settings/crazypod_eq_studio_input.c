#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_eq_studio_controller.h"
#include "crazypod_eq_studio_input.h"

void crazypod_eq_studio_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_eq_studio_input_actions *actions)
{
    bool changed = true;

    if(event->base == BUTTON_SCROLL_FWD)
        crazypod_eq_studio_adjust(
            crazypod_input_wheel_steps(event, 8));
    else if(event->base == BUTTON_SCROLL_BACK)
        crazypod_eq_studio_adjust(
            -crazypod_input_wheel_steps(event, 8));
    else if(event->base == BUTTON_RIGHT)
        crazypod_eq_studio_select_band(1);
    else if(event->base == BUTTON_LEFT)
        crazypod_eq_studio_select_band(-1);
    else if(event->base == BUTTON_SELECT && !event->repeated)
        crazypod_eq_studio_toggle_editing();
    else if(event->base == BUTTON_PLAY && !event->repeated) {
        if(crazypod_eq_studio_model().editing)
            crazypod_eq_studio_cycle_mode();
        else
            crazypod_eq_studio_toggle_enabled();
    }
    else if(event->base == BUTTON_MENU && !event->repeated) {
        crazypod_eq_studio_close();
        actions->leave();
        return;
    }
    else
        changed = false;

    if(changed)
        actions->render();
}

#endif
