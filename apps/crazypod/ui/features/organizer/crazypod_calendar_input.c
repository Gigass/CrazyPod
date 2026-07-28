#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_calendar_controller.h"
#include "crazypod_calendar_input.h"

static void handle_month(
    const struct crazypod_input_event *event, int today,
    const struct crazypod_calendar_input_actions *actions)
{
    if(event->base == BUTTON_SCROLL_FWD ||
       event->base == BUTTON_SCROLL_BACK) {
        int direction =
            event->base == BUTTON_SCROLL_FWD ? 1 : -1;
        int count = crazypod_input_wheel_steps(event, 12);

        while(count-- > 0)
            crazypod_calendar_controller_move_focus(direction);
        actions->render();
    }
    else if(event->base == BUTTON_RIGHT && !event->repeated) {
        crazypod_calendar_controller_move_focus(1);
        actions->render();
    }
    else if(event->base == BUTTON_LEFT && !event->repeated) {
        crazypod_calendar_controller_move_focus(-1);
        actions->render();
    }
    else if(event->base == BUTTON_SELECT && !event->repeated)
        actions->activate();
    else if(event->base == BUTTON_PLAY && !event->repeated) {
        crazypod_calendar_controller_set_focus_date(today);
        actions->render();
    }
    else if(event->base == BUTTON_MENU && !event->repeated)
        actions->leave();
}

static void handle_editor(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_calendar_input_actions *actions)
{
    int direction = 0;

    if(event->base == BUTTON_SCROLL_FWD)
        actions->move_selection(
            crazypod_input_wheel_steps(event, 12));
    else if(event->base == BUTTON_SCROLL_BACK)
        actions->move_selection(
            -crazypod_input_wheel_steps(event, 12));
    else if(event->base == BUTTON_RIGHT && !event->repeated)
        direction = 1;
    else if(event->base == BUTTON_LEFT && !event->repeated)
        direction = -1;
    else if(event->base == BUTTON_SELECT && !event->repeated)
        actions->activate();
    else if(event->base == BUTTON_MENU && !event->repeated)
        actions->leave();

    if(direction != 0 && state->selected == 1) {
        crazypod_calendar_controller_shift_editor_date(direction);
        actions->render();
    }
    else if(direction != 0 && state->selected == 2) {
        crazypod_calendar_controller_shift_editor_time(direction);
        actions->render();
    }
}

static void handle_title_editor(
    const struct crazypod_input_event *event,
    const struct crazypod_calendar_input_actions *actions)
{
    if(event->base == BUTTON_SCROLL_FWD)
        actions->move_selection(
            crazypod_input_wheel_steps(event, 12));
    else if(event->base == BUTTON_SCROLL_BACK)
        actions->move_selection(
            -crazypod_input_wheel_steps(event, 12));
    else if(event->base == BUTTON_RIGHT && !event->repeated) {
        crazypod_calendar_controller_move_editor_cursor(1);
        actions->render();
    }
    else if(event->base == BUTTON_LEFT && !event->repeated) {
        crazypod_calendar_controller_move_editor_cursor(-1);
        actions->render();
    }
    else if(event->base == BUTTON_SELECT && !event->repeated)
        actions->activate();
    else if(event->base == BUTTON_MENU && !event->repeated)
        actions->leave();
}

bool crazypod_calendar_input_handle(
    const struct route_state *state,
    const struct crazypod_input_event *event, int today,
    const struct crazypod_calendar_input_actions *actions)
{
    if(state->route == CALENDAR_ROUTE_MONTH)
        handle_month(event, today, actions);
    else if(state->route == CALENDAR_ROUTE_EDITOR)
        handle_editor(state, event, actions);
    else if(state->route == CALENDAR_ROUTE_TITLE_EDITOR)
        handle_title_editor(event, actions);
    else
        return false;
    return true;
}

#endif
