#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_book_reader_input.h"

void crazypod_book_reader_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_book_reader_input_actions *actions)
{
    int count;

    if(event->base == BUTTON_SCROLL_FWD ||
       event->base == BUTTON_SCROLL_BACK) {
        int direction =
            event->base == BUTTON_SCROLL_FWD ? 1 : -1;

        count = crazypod_input_wheel_steps(event, 12);
        while(count-- > 0)
            actions->turn_page(direction);
    }
    else if(event->base == BUTTON_RIGHT)
        actions->turn_page(1);
    else if(event->base == BUTTON_LEFT)
        actions->turn_page(-1);
    else if(event->base == BUTTON_SELECT && !event->repeated)
        actions->activate();
    else if(event->base == BUTTON_PLAY && !event->repeated)
        actions->toggle_bookmark();
    else if(event->base == BUTTON_MENU && !event->repeated)
        actions->leave();
}

#endif
