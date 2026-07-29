#ifndef CRAZYPOD_NOTES_CONFIRMATION_H
#define CRAZYPOD_NOTES_CONFIRMATION_H

#include "crazypod_notes_feature.h"

struct crazypod_notes_confirmation_result
crazypod_notes_confirmation_execute(
    const struct route_state *state, int route_depth);

#endif
