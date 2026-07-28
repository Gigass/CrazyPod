#ifndef CRAZYPOD_MUSIC_INPUT_H
#define CRAZYPOD_MUSIC_INPUT_H

#include <stdbool.h>

#include "../../navigation/crazypod_input_event.h"

struct crazypod_music_input_actions {
    void (*move_selection)(int direction);
    void (*activate)(void);
    void (*render)(void);
    void (*leave)(void);
    void (*show_search_results)(void);
};

bool crazypod_music_search_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_music_input_actions *actions);

#endif
