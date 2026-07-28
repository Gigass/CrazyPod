#ifndef CRAZYPOD_NOTES_ACTIONS_H
#define CRAZYPOD_NOTES_ACTIONS_H

#include <stdbool.h>
#include <stdint.h>

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_notes_action_kind {
    CRAZYPOD_NOTES_ACTION_UNHANDLED,
    CRAZYPOD_NOTES_ACTION_NONE,
    CRAZYPOD_NOTES_ACTION_RENDER,
    CRAZYPOD_NOTES_ACTION_PUSH,
    CRAZYPOD_NOTES_ACTION_POP,
    CRAZYPOD_NOTES_ACTION_POP_COMPOSER,
    CRAZYPOD_NOTES_ACTION_OPEN_COMPOSER,
    CRAZYPOD_NOTES_ACTION_OPEN_READER,
    CRAZYPOD_NOTES_ACTION_RESET_OPEN_READER,
    CRAZYPOD_NOTES_ACTION_COMMIT_EDITOR,
};

struct crazypod_notes_action {
    enum crazypod_notes_action_kind kind;
    enum crazypod_route route;
    int group;
    uint32_t note_id;
    bool resume_draft;
};

struct crazypod_notes_action crazypod_notes_actions_activate(
    const struct route_state *state);

#endif
