#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_music_activation.h"
#include "crazypod_music_input.h"

bool crazypod_music_search_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_music_input_actions *actions)
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
        if(crazypod_music_search_query()[0] != '\0') {
            crazypod_music_search_backspace();
            actions->render();
        }
        else
            actions->leave();
    }
    else if(event->base == BUTTON_PLAY && !event->repeated &&
            crazypod_music_search_query()[0] != '\0')
        actions->show_search_results();
    return true;
}

#endif
