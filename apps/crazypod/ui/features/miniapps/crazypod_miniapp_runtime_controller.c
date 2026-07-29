#include "crazypod_miniapp_runtime_controller.h"

#include <stdio.h>
#include <string.h>

#include "../../../crazypod_miniapp_input.h"
#include "../../../crazypod_miniapps.h"
#include "crazypod_miniapp_activation.h"

static struct {
    struct crazypod_miniapp_input_queue input;
    bool render_pending;
    int last_error;
    long last_service_tick;
    char alert_id[CRAZYPOD_MINIAPP_ID_SIZE];
    uint32_t alert_deadline;
    uint32_t alert_token;
    int alert_remaining;
    long alert_next_tick;
    long alert_ack_retry_tick;
    bool alert_delivery_pending;
} runtime;

#define ALERT_ACK_RETRY_SECONDS 30

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

static bool same_alert(
    const struct crazypod_miniapp_alarm *alarm)
{
    return runtime.alert_id[0] != '\0' &&
        strcmp(runtime.alert_id, alarm->id) == 0 &&
        runtime.alert_deadline == alarm->deadline_epoch &&
        runtime.alert_token == alarm->token;
}

static void acknowledge_alert_delivery(
    long tick, long ticks_per_second)
{
    if(!runtime.alert_delivery_pending ||
       !tick_due(tick, runtime.alert_ack_retry_tick))
        return;
    if(crazypod_miniapps_alarm_delivery_acknowledge(
           runtime.alert_id, runtime.alert_deadline,
           runtime.alert_token) == CRAZYPOD_MINIAPP_OK) {
        runtime.alert_delivery_pending = false;
        return;
    }
    runtime.alert_ack_retry_tick =
        tick + ALERT_ACK_RETRY_SECONDS * ticks_per_second;
}

void crazypod_miniapp_runtime_opened(void)
{
    crazypod_miniapp_runtime_reset_input();
    runtime.last_service_tick = 0;
    runtime.render_pending = false;
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
        runtime.alert_remaining > 0;
}

bool crazypod_miniapp_runtime_alert_active(void)
{
    return runtime.alert_remaining > 0;
}

struct crazypod_miniapp_runtime_service_result
crazypod_miniapp_runtime_service(
    bool active, bool frame_due, long tick, long ticks_per_second)
{
    struct crazypod_miniapp_runtime_service_result result = { 0 };
    struct cp_input_event input_event;
    bool service_due = runtime.last_service_tick == 0 ||
        tick_due(tick, runtime.last_service_tick + ticks_per_second);

    if(!active)
        crazypod_miniapp_runtime_reset_input();
    else if(crazypod_miniapp_runtime_next_input(frame_due, &input_event) &&
            crazypod_miniapps_event(&input_event))
        crazypod_miniapp_runtime_request_render();

    if(service_due) {
        struct crazypod_miniapp_alarm alarm;
        bool alarm_found;

        runtime.last_service_tick = tick;
        alarm_found = crazypod_miniapps_alarm_service(&alarm);
        if(alarm_found && !same_alert(&alarm)) {
            snprintf(runtime.alert_id, sizeof(runtime.alert_id),
                     "%s", alarm.id);
            runtime.alert_deadline = alarm.deadline_epoch;
            runtime.alert_token = alarm.token;
            runtime.alert_remaining = 3;
            runtime.alert_next_tick = tick;
            runtime.alert_ack_retry_tick = tick;
            runtime.alert_delivery_pending = true;
        }
        if(active && crazypod_miniapps_tick())
            crazypod_miniapp_runtime_request_render();
        if(runtime.alert_remaining <= 0)
            acknowledge_alert_delivery(tick, ticks_per_second);
        if(!alarm_found &&
           !runtime.alert_delivery_pending &&
           runtime.alert_remaining <= 0)
            runtime.alert_id[0] = '\0';
    }

    if(crazypod_miniapps_take_close_request()) {
        crazypod_miniapp_runtime_reset_input();
        result.close_requested = true;
        return result;
    }
    if(crazypod_miniapps_take_ui_refresh()) {
        crazypod_miniapp_runtime_reset_input();
        crazypod_miniapp_runtime_request_render();
    }

    if(runtime.alert_remaining > 0 &&
       tick_due(tick, runtime.alert_next_tick)) {
        result.beep_requested = true;
        acknowledge_alert_delivery(tick, ticks_per_second);
        --runtime.alert_remaining;
        runtime.alert_next_tick = tick + ticks_per_second / 3;
    }
    return result;
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
