#ifndef CRAZYPOD_NOTES_CONFIRMATION_H
#define CRAZYPOD_NOTES_CONFIRMATION_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_notes_confirmation_navigation {
    CRAZYPOD_NOTES_CONFIRMATION_NONE = 0,
    CRAZYPOD_NOTES_CONFIRMATION_RESET_MENU,
    CRAZYPOD_NOTES_CONFIRMATION_RESET_MENU_SHOW_DELETED,
    CRAZYPOD_NOTES_CONFIRMATION_TRUNCATE,
};

struct crazypod_notes_confirmation_result {
    bool handled;
    enum crazypod_notes_confirmation_navigation navigation;
    int depth;
};

struct crazypod_notes_confirmation_result
crazypod_notes_confirmation_execute(
    const struct route_state *state, int route_depth);

#endif
