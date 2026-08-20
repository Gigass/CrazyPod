#ifndef CRAZYPOD_SYSTEM_EVENT_H
#define CRAZYPOD_SYSTEM_EVENT_H

#include <stdbool.h>
#include <stdint.h>

struct crazypod_system_event_actions {
    void (*usb_prompt_request)(unsigned request);
    void (*usb_prompt_done)(unsigned request);
    void (*usb_connected)(intptr_t data);
    void (*usb_disconnected)(void);
    void (*headphone_changed)(bool inserted);
    void (*power_off)(void);
    void (*reboot)(void);
};

bool crazypod_system_event_handle(
    long event, intptr_t data,
    const struct crazypod_system_event_actions *actions);

#endif
