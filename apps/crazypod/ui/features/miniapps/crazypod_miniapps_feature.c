#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <string.h>

#include "../../../crazypod_miniapps.h"
#include "crazypod_miniapp_input.h"
#include "crazypod_miniapp_runtime_controller.h"
#include "crazypod_miniapp_screen.h"
#include "crazypod_miniapps_feature.h"

static const char *localized_miniapp_name(
    const struct crazypod_miniapp_metadata *metadata)
{
    if(metadata == NULL)
        return NULL;
    if(strcmp(metadata->id, "calculator") == 0)
        return CP_TR("Calculator");
    if(strcmp(metadata->id, "pomodoro") == 0)
        return "Pomodoro";
    return metadata->name;
}

int crazypod_miniapps_feature_item_count(
    const struct route_state *state)
{
    if(state->route == UTILITIES_ROUTE_MENU)
        return crazypod_miniapps_count();
    return state->route == MINIAPP_ROUTE_VIEW ? 1 : 0;
}

const char *crazypod_miniapps_feature_title(
    const struct route_state *state)
{
    if(state->route == MINIAPP_ROUTE_VIEW) {
        const struct crazypod_miniapp_metadata *metadata =
            crazypod_miniapps_metadata(state->group);

        const char *name = localized_miniapp_name(metadata);

        return name != NULL ? name : CP_TR("MINI APP");
    }
    return CP_TR("MINI APPS");
}

bool crazypod_miniapps_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    const struct crazypod_miniapp_metadata *metadata =
        crazypod_miniapps_metadata(
            state->route == MINIAPP_ROUTE_VIEW
                ? state->group : index);

    *title = metadata != NULL
        ? localized_miniapp_name(metadata)
        : state->route == MINIAPP_ROUTE_VIEW ? CP_TR("Mini App") : "";
    return state->route == UTILITIES_ROUTE_MENU ||
        state->route == MINIAPP_ROUTE_VIEW;
}

bool crazypod_miniapps_feature_render(
    const struct route_state *state, lv_obj_t *parent,
    uint32_t primary_color)
{
    if(state->route != MINIAPP_ROUTE_VIEW)
        return false;
    crazypod_miniapp_screen_render(parent, primary_color);
    return true;
}

void crazypod_miniapps_feature_initialize(void)
{
    crazypod_miniapp_screen_reset();
    crazypod_miniapp_input_reset_state();
}

bool crazypod_miniapps_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context)
{
    const struct crazypod_miniapp_input_actions actions = {
        .wake_display = context->wake_display,
        .keep_boosted = context->boost,
        .close = context->pop,
    };

    (void)state;
    return crazypod_miniapp_input_handle(
        event, context->now, context->ticks_per_second / 2,
        context->ticks_per_second / 10,
        &actions);
}

int crazypod_miniapps_feature_service(
    bool active, bool frame_due, long now,
    long ticks_per_second)
{
    struct crazypod_miniapp_runtime_service_result result =
        { 0 };
    int events = CRAZYPOD_MINIAPPS_SERVICE_NONE;

    crazypod_miniapp_input_service(active, now);
    result = crazypod_miniapp_runtime_service(
        active, frame_due, now, ticks_per_second);
    if(result.close_requested)
        events |= CRAZYPOD_MINIAPPS_SERVICE_CLOSE;
    if(result.beep_requested)
        events |= CRAZYPOD_MINIAPPS_SERVICE_BEEP;
    if(active && crazypod_miniapp_runtime_take_render())
        events |= CRAZYPOD_MINIAPPS_SERVICE_RENDER;
    return events;
}

bool crazypod_miniapps_feature_is_open(void)
{
    return crazypod_miniapps_is_open();
}

bool crazypod_miniapps_feature_motion_active(void)
{
    return crazypod_miniapps_is_open() &&
        (crazypod_miniapp_runtime_motion_active() ||
         crazypod_miniapp_input_motion_active());
}

bool crazypod_miniapps_feature_alert_active(void)
{
    return crazypod_miniapp_runtime_alert_active();
}

void crazypod_miniapps_feature_close(void)
{
    crazypod_miniapps_close();
}

void crazypod_miniapps_feature_reset_input(void)
{
    crazypod_miniapp_runtime_reset_input();
    crazypod_miniapp_input_reset_state();
}

void crazypod_miniapps_feature_rescan(void)
{
    crazypod_miniapp_runtime_rescan();
}

int crazypod_miniapps_feature_last_error(void)
{
    return crazypod_miniapp_runtime_last_error();
}

void crazypod_miniapps_feature_initialize_runtime(void)
{
    crazypod_miniapp_runtime_initialize();
}

int crazypod_miniapps_feature_prepare(void)
{
    return crazypod_miniapp_runtime_prepare();
}

bool crazypod_miniapps_feature_activate(
    const struct route_state *state,
    const struct crazypod_miniapps_activation_host *host)
{
    const struct crazypod_miniapp_activation_host internal = {
        .push = host->push,
        .render = host->render,
    };

    return crazypod_miniapp_runtime_activate(
        state, &internal);
}

#endif
