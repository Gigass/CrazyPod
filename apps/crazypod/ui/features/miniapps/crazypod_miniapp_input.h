#ifndef CRAZYPOD_UI_MINIAPP_INPUT_H
#define CRAZYPOD_UI_MINIAPP_INPUT_H

#include <stdbool.h>

#include "../../navigation/crazypod_input_event.h"

struct crazypod_miniapp_input_actions {
    void (*wake_display)(void);
    void (*keep_boosted)(int ticks);
    void (*close)(void);
};

bool crazypod_miniapp_input_handle(
    const struct crazypod_input_event *event, int boost_ticks,
    const struct crazypod_miniapp_input_actions *actions);

#endif
