#ifndef CRAZYPOD_HOME_INPUT_H
#define CRAZYPOD_HOME_INPUT_H

#include "../navigation/crazypod_input_event.h"

struct crazypod_home_input_actions {
    void (*move_selection)(int direction);
    void (*next_track)(void);
    void (*previous_track)(void);
    void (*open_selected_app)(void);
    void (*toggle_playback)(void);
};

void crazypod_home_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_home_input_actions *actions);

#endif
