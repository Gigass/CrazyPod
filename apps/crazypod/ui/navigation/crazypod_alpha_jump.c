#include <string.h>

#include "crazypod_alpha_jump.h"

void crazypod_alpha_jump_reset(
    struct crazypod_alpha_jump_state *state)
{
    if(state != NULL)
        memset(state, 0, sizeof(*state));
}

bool crazypod_alpha_jump_consume(
    struct crazypod_alpha_jump_state *state,
    enum crazypod_route route, int group,
    int direction, int steps, long now,
    long window_ticks, int threshold)
{
    int sign;
    bool same_burst;

    if(state == NULL || direction == 0 ||
       steps <= 0 || window_ticks <= 0 || threshold <= 0)
        return false;
    sign = direction > 0 ? 1 : -1;
    same_burst =
        state->valid &&
        state->route == route &&
        state->group == group &&
        state->direction == sign &&
        (long)(now - state->last_tick) >= 0 &&
        (long)(now - state->last_tick) <= window_ticks;
    if(!same_burst) {
        state->route = route;
        state->group = group;
        state->direction = sign;
        state->steps = 0;
        state->jumping = false;
        state->valid = true;
    }
    state->last_tick = now;
    if(state->steps < threshold) {
        state->steps += steps;
        if(state->steps > threshold)
            state->steps = threshold;
    }
    if(state->steps >= threshold)
        state->jumping = true;
    return state->jumping;
}
