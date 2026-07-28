#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_notes_controller.h"
#include "crazypod_notes_input.h"

static void handle_composer(
    bool editor_dirty,
    const struct crazypod_input_event *event,
    const struct crazypod_notes_input_actions *actions)
{
    if(event->base == BUTTON_SCROLL_FWD)
        actions->move_selection(
            crazypod_input_wheel_steps(event, 12));
    else if(event->base == BUTTON_SCROLL_BACK)
        actions->move_selection(
            -crazypod_input_wheel_steps(event, 12));
    else if(event->base == BUTTON_RIGHT && !event->repeated) {
        crazypod_notes_controller_move_cursor(1);
        actions->render();
    }
    else if(event->base == BUTTON_LEFT && !event->repeated) {
        crazypod_notes_controller_move_cursor(-1);
        actions->render();
    }
    else if(event->base == BUTTON_SELECT && !event->repeated)
        actions->activate();
    else if(event->base == BUTTON_PLAY && !event->repeated) {
        crazypod_notes_controller_toggle_field();
        actions->render();
    }
    else if(event->base == BUTTON_MENU && !event->repeated) {
        if(editor_dirty)
            actions->show_exit_actions();
        else
            actions->leave();
    }
}

static void handle_search(
    const struct crazypod_input_event *event,
    const struct crazypod_notes_input_actions *actions)
{
    if(event->base == BUTTON_SCROLL_FWD)
        actions->move_selection(
            crazypod_input_wheel_steps(event, 12));
    else if(event->base == BUTTON_SCROLL_BACK)
        actions->move_selection(
            -crazypod_input_wheel_steps(event, 12));
    else if(event->base == BUTTON_RIGHT)
        actions->move_selection(1);
    else if(event->base == BUTTON_LEFT)
        actions->move_selection(-1);
    else if(event->base == BUTTON_SELECT && !event->repeated)
        actions->activate();
    else if(event->base == BUTTON_MENU && !event->repeated) {
        if(crazypod_notes_controller_query()[0] != '\0') {
            crazypod_notes_controller_backspace_query();
            actions->render();
        }
        else
            actions->leave();
    }
    else if(event->base == BUTTON_PLAY && !event->repeated &&
            crazypod_notes_controller_query()[0] != '\0')
        actions->show_search_results();
}

bool crazypod_notes_input_handle(
    enum crazypod_route route, bool editor_dirty,
    const struct crazypod_input_event *event,
    const struct crazypod_notes_input_actions *actions)
{
    if(route == NOTES_ROUTE_COMPOSER)
        handle_composer(editor_dirty, event, actions);
    else if(route == NOTES_ROUTE_SEARCH)
        handle_search(event, actions);
    else
        return false;
    return true;
}

#endif
