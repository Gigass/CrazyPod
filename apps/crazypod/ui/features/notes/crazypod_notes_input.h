#ifndef CRAZYPOD_NOTES_INPUT_H
#define CRAZYPOD_NOTES_INPUT_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"
#include "../../navigation/crazypod_input_event.h"

struct crazypod_notes_input_actions {
    void (*move_selection)(int direction);
    void (*activate)(void);
    void (*render)(void);
    void (*leave)(void);
    void (*show_exit_actions)(void);
    void (*show_search_results)(void);
};

bool crazypod_notes_input_handle(
    enum crazypod_route route, bool editor_dirty,
    const struct crazypod_input_event *event,
    const struct crazypod_notes_input_actions *actions);

#endif
