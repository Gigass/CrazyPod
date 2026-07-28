#ifndef CRAZYPOD_EQ_STUDIO_INPUT_H
#define CRAZYPOD_EQ_STUDIO_INPUT_H

#include "../../navigation/crazypod_input_event.h"

struct crazypod_eq_studio_input_actions {
    void (*render)(void);
    void (*leave)(void);
};

void crazypod_eq_studio_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_eq_studio_input_actions *actions);

#endif
