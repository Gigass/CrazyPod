#include <stddef.h>

#include "crazypod_feature_dispatcher.h"
#include "crazypod_route_registry.h"

bool crazypod_feature_input_dispatch(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    enum crazypod_feature_input_phase phase,
    const struct crazypod_feature_bindings *bindings)
{
    const struct crazypod_feature *feature;
    crazypod_feature_input_handler handler;

    if(state == NULL || event == NULL || bindings == NULL)
        return false;
    feature = crazypod_route_registry_feature(state->route);
    if(feature == NULL)
        return false;
    handler = phase == CRAZYPOD_FEATURE_INPUT_RAW
        ? bindings->raw[feature->id]
        : bindings->pressed[feature->id];
    return handler != NULL
        ? handler(state, event, bindings->context) : false;
}

bool crazypod_feature_activate_dispatch(
    const struct route_state *state,
    const struct crazypod_feature_bindings *bindings)
{
    const struct crazypod_feature *feature;
    crazypod_feature_activate_handler handler;

    if(state == NULL || bindings == NULL)
        return false;
    feature = crazypod_route_registry_feature(state->route);
    if(feature == NULL)
        return false;
    handler = bindings->activate[feature->id];
    return handler != NULL ? handler(state) : false;
}

bool crazypod_feature_render_dispatch(
    const struct route_state *state,
    const struct crazypod_feature_bindings *bindings)
{
    const struct crazypod_feature *feature;
    crazypod_feature_render_handler handler;

    if(state == NULL || bindings == NULL)
        return false;
    feature = crazypod_route_registry_feature(state->route);
    if(feature == NULL)
        return false;
    handler = bindings->render[feature->id];
    if(handler == NULL)
        return false;
    handler(state);
    return true;
}
