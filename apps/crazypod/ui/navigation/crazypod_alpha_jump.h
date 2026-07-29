#ifndef CRAZYPOD_ALPHA_JUMP_H
#define CRAZYPOD_ALPHA_JUMP_H

#include <stdbool.h>

#include "crazypod_ui_routes.h"

struct crazypod_alpha_jump_state {
    enum crazypod_route route;
    int group;
    int direction;
    int steps;
    long last_tick;
    bool valid;
    bool jumping;
};

void crazypod_alpha_jump_reset(
    struct crazypod_alpha_jump_state *state);
bool crazypod_alpha_jump_consume(
    struct crazypod_alpha_jump_state *state,
    enum crazypod_route route, int group,
    int direction, int steps, long now,
    long window_ticks, int threshold);

#endif
