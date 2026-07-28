#ifndef CRAZYPOD_ORGANIZER_ACTIONS_H
#define CRAZYPOD_ORGANIZER_ACTIONS_H

#include <stdint.h>

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_organizer_action_kind {
    CRAZYPOD_ORGANIZER_ACTION_UNHANDLED,
    CRAZYPOD_ORGANIZER_ACTION_NONE,
    CRAZYPOD_ORGANIZER_ACTION_RENDER,
    CRAZYPOD_ORGANIZER_ACTION_PUSH,
    CRAZYPOD_ORGANIZER_ACTION_POP,
    CRAZYPOD_ORGANIZER_ACTION_BEGIN_EDITOR,
    CRAZYPOD_ORGANIZER_ACTION_COMMIT_EDITOR,
};

struct crazypod_organizer_action {
    enum crazypod_organizer_action_kind kind;
    enum crazypod_route route;
    int group;
    uint32_t event_id;
    int fallback_date;
};

struct crazypod_organizer_action
crazypod_organizer_actions_activate(
    const struct route_state *state);

#endif
