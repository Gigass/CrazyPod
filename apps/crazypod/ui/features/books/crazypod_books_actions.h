#ifndef CRAZYPOD_BOOKS_ACTIONS_H
#define CRAZYPOD_BOOKS_ACTIONS_H

#include <stdint.h>

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_books_action_kind {
    CRAZYPOD_BOOKS_ACTION_UNHANDLED,
    CRAZYPOD_BOOKS_ACTION_NONE,
    CRAZYPOD_BOOKS_ACTION_RENDER,
    CRAZYPOD_BOOKS_ACTION_PUSH,
    CRAZYPOD_BOOKS_ACTION_POP,
    CRAZYPOD_BOOKS_ACTION_BEGIN_READER,
    CRAZYPOD_BOOKS_ACTION_SHOW_FONT_SIZE,
    CRAZYPOD_BOOKS_ACTION_SHOW_THEME,
    CRAZYPOD_BOOKS_ACTION_RESCAN,
};

struct crazypod_books_action {
    enum crazypod_books_action_kind kind;
    enum crazypod_route route;
    int group;
    int book_index;
    uint32_t offset;
};

struct crazypod_books_action crazypod_books_actions_activate(
    const struct route_state *state);

#endif
