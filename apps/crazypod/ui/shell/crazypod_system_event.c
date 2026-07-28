#include "config.h"

#ifdef IPOD_6G

#include "button.h"
#include "events.h"
#include "kernel.h"

#include "crazypod_system_event.h"
#include "crazypod_usb_prompt.h"

bool crazypod_system_event_handle(
    long event, intptr_t data,
    const struct crazypod_system_event_actions *actions)
{
    if((event & SYS_EVENT) == 0)
        return false;
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    if(event == CRAZYPOD_USB_PROMPT_REQUEST)
        actions->usb_prompt_request((unsigned)data);
    else if(event == CRAZYPOD_USB_PROMPT_DONE)
        actions->usb_prompt_done((unsigned)data);
    else
#endif
    if(event == SYS_USB_CONNECTED)
        actions->usb_connected(data);
    else if(event == SYS_USB_DISCONNECTED)
        actions->usb_disconnected();
    else if(event == SYS_POWEROFF)
        actions->power_off();
    else if(event == SYS_REBOOT)
        actions->reboot();
    return true;
}

#endif
