#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_input_event.h"

struct crazypod_input_event crazypod_input_event_make(
    long button, intptr_t data)
{
    struct crazypod_input_event event = {
        .raw = button,
        .base = button & BUTTON_MAIN,
        .data = data,
        .release = (button & BUTTON_REL) != 0,
        .repeated = (button & BUTTON_REPEAT) != 0,
    };

    return event;
}

int crazypod_input_wheel_steps(
    const struct crazypod_input_event *event, int maximum)
{
    int steps = 1;

#ifdef HAVE_WHEEL_ACCELERATION
    steps = button_apply_acceleration((unsigned int)event->data);
#endif
    if(steps < 1)
        steps = 1;
    if(steps > maximum)
        steps = maximum;
    return steps;
}

#endif
