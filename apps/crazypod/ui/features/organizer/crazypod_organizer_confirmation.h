#ifndef CRAZYPOD_ORGANIZER_CONFIRMATION_H
#define CRAZYPOD_ORGANIZER_CONFIRMATION_H

#include "crazypod_organizer_feature.h"

struct crazypod_organizer_confirmation_result
crazypod_organizer_confirmation_execute(
    const struct route_state *state, long now, int ticks_per_second,
    int fallback_date);

#endif
