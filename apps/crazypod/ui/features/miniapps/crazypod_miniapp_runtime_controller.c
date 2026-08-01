#include "crazypod_miniapp_runtime_controller.h"

#include "../../../crazypod_miniapp_input.h"
#include "../../../crazypod_miniapps.h"
#include "crazypod_miniapp_activation.h"

static struct {
    struct crazypod_miniapp_input_queue input;
    bool render_pending;
    int last_error;
    long last_service_tick;
} runtime;

void crazypod_miniapp_runtime_initialize(void)
{
    runtime.last_error = crazypod_miniapps_init();
    crazypod_miniapp_runtime_opened();
}

int crazypod_miniapp_runtime_prepare(void)
{
    runtime.last_error = crazypod_miniapps_prepare();
    return runtime.last_error;
}

void crazypod_miniapp_runtime_rescan(void)
{
    runtime.last_error = crazypod_miniapps_rescan();
}

void crazypod_miniapp_runtime_note_error(int error)
{
    runtime.last_error = error;
}

int crazypod_miniapp_runtime_last_error(void)
{
    return runtime.last_error;
}

static bool tick_due(long tick, long deadline)
{
    return (long)(tick - deadline) >= 0;
}

void crazypod_miniapp_runtime_opened(void)
{
    crazypod_miniapp_runtime_reset_input();
    runtime.last_service_tick = 0;
    runtime.render_pending = false;
    (void)crazypod_miniapps_take_ui_refresh();
}

void crazypod_miniapp_runtime_reset_input(void)
{
    crazypod_miniapp_input_reset(&runtime.input);
}

void crazypod_miniapp_runtime_push_wheel(
    const struct cp_input_event *event)
{
    (void)crazypod_miniapp_input_push_wheel(&runtime.input, event);
}

bool crazypod_miniapp_runtime_next_input(
    bool frame_due, struct cp_input_event *event)
{
    return crazypod_miniapp_input_next(&runtime.input, frame_due, event);
}

void crazypod_miniapp_runtime_request_render(void)
{
    runtime.render_pending = true;
}

bool crazypod_miniapp_runtime_take_render(void)
{
    bool pending = runtime.render_pending;

    runtime.render_pending = false;
    return pending;
}

bool crazypod_miniapp_runtime_motion_active(void)
{
    return crazypod_miniapp_input_count(&runtime.input) > 0 ||
        runtime.render_pending ||
        crazypod_miniapps_has_scheduled_work();
}

bool
crazypod_miniapp_runtime_service(
    bool active, bool frame_due, long tick, long ticks_per_second)
{
    struct cp_input_event input_event;
    bool service_due = frame_due ||
        runtime.last_service_tick == 0 ||
        tick_due(tick, runtime.last_service_tick + ticks_per_second);

    if(!active)
        crazypod_miniapp_runtime_reset_input();
    else if(crazypod_miniapp_runtime_next_input(frame_due, &input_event)) {
        (void)crazypod_miniapps_event(&input_event);
        if(!crazypod_miniapps_is_open())
            return true;
    }

    if(service_due) {
        runtime.last_service_tick = tick;
        if(active) {
            (void)crazypod_miniapps_tick();
            if(!crazypod_miniapps_is_open())
                return true;
        }
    }

    (void)crazypod_miniapps_take_ui_refresh();
    return false;
}

bool crazypod_miniapp_runtime_activate(
    const struct route_state *state,
    const struct crazypod_miniapp_activation_host *host)
{
    const struct crazypod_miniapp_activation_result action =
        crazypod_miniapp_activation_execute(
            state->route, state->selected);

    if(!action.handled)
        return false;
    crazypod_miniapp_runtime_note_error(action.error);
    if(action.opened)
        host->push(MINIAPP_ROUTE_VIEW, action.selected);
    else if(state->route == UTILITIES_ROUTE_MENU)
        host->render(false);
    return true;
}

unsigned crazypod_miniapp_runtime_input_count(void)
{
    return crazypod_miniapp_input_count(&runtime.input);
}
