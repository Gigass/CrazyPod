#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "button.h"

#include "../../../crazypod_miniapps.h"
#include "crazypod_miniapp_input.h"
#include "crazypod_miniapp_runtime_controller.h"

static struct {
    bool menu_holding;
    bool suppress_menu_release;
    bool exit_prompt_visible;
    bool exit_selected;
    bool select_long_sent;
    bool play_long_sent;
    long menu_hold_deadline;
} state;

static bool tick_due(long now, long deadline)
{
    return (long)(now - deadline) >= 0;
}

void crazypod_miniapp_input_reset_state(void)
{
    memset(&state, 0, sizeof(state));
}

static void request_render(int boost_ticks,
    const struct crazypod_miniapp_input_actions *actions)
{
    crazypod_miniapp_runtime_request_render();
    actions->keep_boosted(boost_ticks);
}

static void show_exit_prompt(void)
{
    state.menu_holding = false;
    state.suppress_menu_release = true;
    state.exit_prompt_visible = true;
    state.exit_selected = false;
    crazypod_miniapp_runtime_reset_input();
    crazypod_miniapp_runtime_request_render();
}

void crazypod_miniapp_input_service(bool active, long now)
{
    if(!active) {
        crazypod_miniapp_input_reset_state();
        return;
    }
    if(state.menu_holding &&
       tick_due(now, state.menu_hold_deadline))
        show_exit_prompt();
}

bool crazypod_miniapp_input_motion_active(void)
{
    return state.menu_holding;
}

bool crazypod_miniapp_input_exit_prompt_visible(void)
{
    return state.exit_prompt_visible;
}

bool crazypod_miniapp_input_exit_selected(void)
{
    return state.exit_selected;
}

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

static bool handle_exit_prompt(
    const struct crazypod_input_event *input, int boost_ticks,
    const struct crazypod_miniapp_input_actions *actions)
{
    bool selected = state.exit_selected;

    if(input->release) {
        if(input->base == BUTTON_MENU)
            state.suppress_menu_release = false;
        return true;
    }
    if(input->repeated)
        return true;
    if(input->base == BUTTON_SCROLL_FWD ||
       input->base == BUTTON_RIGHT)
        selected = true;
    else if(input->base == BUTTON_SCROLL_BACK ||
            input->base == BUTTON_LEFT)
        selected = false;
    else if(input->base == BUTTON_MENU) {
        state.exit_prompt_visible = false;
        state.suppress_menu_release = true;
        request_render(boost_ticks, actions);
        return true;
    }
    else if(input->base == BUTTON_SELECT) {
        state.exit_prompt_visible = false;
        if(state.exit_selected) {
            actions->keep_boosted(boost_ticks);
            actions->close();
            return true;
        }
        request_render(boost_ticks, actions);
        return true;
    }
    else
        return true;

    if(selected != state.exit_selected) {
        state.exit_selected = selected;
        request_render(boost_ticks, actions);
    }
    return true;
}

bool crazypod_miniapp_input_handle(
    const struct crazypod_input_event *input, long now,
    long menu_hold_ticks, int boost_ticks,
    const struct crazypod_miniapp_input_actions *actions)
{
    struct cp_input_event event;

    if(!crazypod_miniapps_is_open())
        return false;

    actions->wake_display();
    if(state.exit_prompt_visible)
        return handle_exit_prompt(input, boost_ticks, actions);

    if(input->base == BUTTON_MENU) {
        if(input->release) {
            if(state.suppress_menu_release) {
                state.suppress_menu_release = false;
                return true;
            }
            if(!state.menu_holding)
                return true;
            if(tick_due(now, state.menu_hold_deadline)) {
                show_exit_prompt();
                actions->keep_boosted(boost_ticks);
                return true;
            }
            state.menu_holding = false;
        }
        else if(!input->repeated) {
            state.menu_holding = true;
            state.menu_hold_deadline =
                now + (menu_hold_ticks > 0 ? menu_hold_ticks : 1);
            return true;
        }
        else {
            if(state.menu_holding &&
               tick_due(now, state.menu_hold_deadline)) {
                show_exit_prompt();
                actions->keep_boosted(boost_ticks);
            }
            return true;
        }
    }
    else if(input->release) {
        if(input->base == BUTTON_SELECT)
            state.select_long_sent = false;
        else if(input->base == BUTTON_PLAY)
            state.play_long_sent = false;
        return true;
    }

    if(!translate_event(input, &event))
        return true;

    if(input->repeated && input->base == BUTTON_SELECT) {
        if(state.select_long_sent)
            return true;
        state.select_long_sent = true;
    }
    else if(input->repeated && input->base == BUTTON_PLAY) {
        if(state.play_long_sent)
            return true;
        state.play_long_sent = true;
    }
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
    actions->keep_boosted(boost_ticks);
    (void)crazypod_miniapps_event(&event);
    if(!crazypod_miniapps_is_open()) {
        actions->close();
        return true;
    }
    (void)crazypod_miniapps_take_ui_refresh();
    return true;
}

#endif
