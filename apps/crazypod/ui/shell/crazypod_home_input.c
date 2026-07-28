#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_home_input.h"

void crazypod_home_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_home_input_actions *actions)
{
    if(event->base == BUTTON_SCROLL_FWD)
        actions->move_selection(1);
    else if(event->base == BUTTON_SCROLL_BACK)
        actions->move_selection(-1);
    else if(event->base == BUTTON_RIGHT && !event->repeated)
        actions->next_track();
    else if(event->base == BUTTON_LEFT && !event->repeated)
        actions->previous_track();
    else if(event->base == BUTTON_SELECT)
        actions->open_selected_app();
    else if(event->base == BUTTON_PLAY && !event->repeated)
        actions->toggle_playback();
}

#endif
