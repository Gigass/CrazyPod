#ifndef CRAZYPOD_FEATURE_INPUT_H
#define CRAZYPOD_FEATURE_INPUT_H

#include "../navigation/crazypod_feature_dispatcher.h"

struct crazypod_feature_input_host {
    long (*now)(void);
    void (*render)(bool transition);
    void (*boost)(int ticks);
};

void crazypod_feature_input_configure(
    const struct crazypod_feature_input_host *host);
const struct crazypod_feature_bindings *
crazypod_feature_input_bindings(void);

#endif
