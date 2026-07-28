#ifndef CRAZYPOD_BOOKS_CONFIRMATION_H
#define CRAZYPOD_BOOKS_CONFIRMATION_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"

struct crazypod_books_confirmation_result {
    bool handled;
    bool deleted;
};

struct crazypod_books_confirmation_result
crazypod_books_confirmation_execute(const struct route_state *state);

#endif
