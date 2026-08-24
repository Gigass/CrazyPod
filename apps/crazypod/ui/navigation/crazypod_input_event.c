#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_input_event.h"

bool crazypod_input_button_is_remote(long button)
{
#ifdef IPOD_ACCESSORY_PROTOCOL
    return (button & BUTTON_REMOTE) != 0;
#else
    (void)button;
    return false;
#endif
}

long crazypod_input_translate_remote(long button)
{
#ifdef IPOD_ACCESSORY_PROTOCOL
    long remote = button & BUTTON_REMOTE;
    long modifiers = button & (BUTTON_REL | BUTTON_REPEAT);
    long translated = BUTTON_NONE;

    if(remote == BUTTON_NONE)
        return button;
    if(remote & (BUTTON_RC_VOL_DOWN | BUTTON_RC_DOWN))
        translated = BUTTON_NONE;
    else if(remote & BUTTON_RC_RIGHT)
        translated = BUTTON_SCROLL_FWD;
    else if(remote & BUTTON_RC_LEFT)
        translated = BUTTON_SCROLL_BACK;
    else if(remote & (BUTTON_RC_PLAY | BUTTON_RC_SELECT))
        translated = BUTTON_SELECT;
    else if(remote & (BUTTON_RC_VOL_UP | BUTTON_RC_UP |
                      BUTTON_RC_MENU))
        translated = BUTTON_MENU;

    return translated == BUTTON_NONE
        ? BUTTON_NONE : translated | modifiers;
#else
    return button;
#endif
}

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
