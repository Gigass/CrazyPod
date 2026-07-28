#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "button.h"

#include "../../../crazypod_miniapps.h"
#include "crazypod_miniapp_input.h"
#include "crazypod_miniapp_runtime_controller.h"

static bool translate_event(
    const struct crazypod_input_event *input,
    struct cp_input_event *output)
{
    memset(output, 0, sizeof(*output));
    output->struct_size = sizeof(*output);
    output->steps = 1;
    output->repeated = input->repeated ? 1 : 0;

    if(input->base == BUTTON_SCROLL_FWD) {
        output->type = CP_INPUT_WHEEL_CLOCKWISE;
        output->steps =
            (uint8_t)crazypod_input_wheel_steps(input, 4);
    }
    else if(input->base == BUTTON_SCROLL_BACK) {
        output->type = CP_INPUT_WHEEL_COUNTERCLOCKWISE;
        output->steps =
            (uint8_t)crazypod_input_wheel_steps(input, 4);
    }
    else if(input->base == BUTTON_LEFT)
        output->type = CP_INPUT_LEFT;
    else if(input->base == BUTTON_RIGHT)
        output->type = CP_INPUT_RIGHT;
    else if(input->base == BUTTON_SELECT)
        output->type = CP_INPUT_SELECT;
    else if(input->base == BUTTON_PLAY)
        output->type = CP_INPUT_PLAY;
    else if(input->base == BUTTON_MENU)
        output->type = CP_INPUT_MENU;
    else
        return false;
    return true;
}

bool crazypod_miniapp_input_handle(
    const struct crazypod_input_event *input, int boost_ticks,
    const struct crazypod_miniapp_input_actions *actions)
{
    struct cp_input_event event;
    bool handled;
    bool ui_refresh;

    if(input->release)
        return true;
    if(!translate_event(input, &event))
        return true;

    actions->wake_display();
    if((input->base == BUTTON_MENU ||
        input->base == BUTTON_PLAY ||
        input->base == BUTTON_SELECT) && input->repeated)
        return true;
    if(event.type == CP_INPUT_WHEEL_CLOCKWISE ||
       event.type == CP_INPUT_WHEEL_COUNTERCLOCKWISE) {
        crazypod_miniapp_runtime_push_wheel(&event);
        actions->keep_boosted(boost_ticks);
        return true;
    }

    /*
     * Direct buttons act on the focus already presented on screen.
     * Drop wheel intent that has not reached a presented frame.
     */
    crazypod_miniapp_runtime_reset_input();
    handled = crazypod_miniapps_event(&event);
    if(crazypod_miniapps_take_close_request()) {
        actions->close();
        return true;
    }
    if(input->base == BUTTON_MENU && !handled) {
        actions->close();
        return true;
    }
    ui_refresh = crazypod_miniapps_take_ui_refresh();
    if(handled || ui_refresh) {
        if(ui_refresh)
            crazypod_miniapp_runtime_reset_input();
        crazypod_miniapp_runtime_request_render();
        actions->keep_boosted(boost_ticks);
    }
    return true;
}

#endif
