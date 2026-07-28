#ifndef CRAZYPOD_FEATURE_DISPATCHER_H
#define CRAZYPOD_FEATURE_DISPATCHER_H

#include <stdbool.h>

#include "crazypod_ui_routes.h"
#include "../features/crazypod_feature.h"
#include "crazypod_input_event.h"

enum crazypod_feature_input_phase {
    CRAZYPOD_FEATURE_INPUT_RAW = 0,
    CRAZYPOD_FEATURE_INPUT_PRESSED,
};

typedef bool (*crazypod_feature_input_handler)(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context);
typedef bool (*crazypod_feature_activate_handler)(
    const struct route_state *state);
typedef void (*crazypod_feature_render_handler)(
    const struct route_state *state);

struct crazypod_feature_bindings {
    crazypod_feature_input_handler
        raw[CRAZYPOD_FEATURE_COUNT];
    crazypod_feature_input_handler
        pressed[CRAZYPOD_FEATURE_COUNT];
    crazypod_feature_activate_handler
        activate[CRAZYPOD_FEATURE_COUNT];
    crazypod_feature_render_handler
        render[CRAZYPOD_FEATURE_COUNT];
    void *context;
};

bool crazypod_feature_input_dispatch(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    enum crazypod_feature_input_phase phase,
    const struct crazypod_feature_bindings *bindings);
bool crazypod_feature_activate_dispatch(
    const struct route_state *state,
    const struct crazypod_feature_bindings *bindings);
bool crazypod_feature_render_dispatch(
    const struct route_state *state,
    const struct crazypod_feature_bindings *bindings);

#endif
