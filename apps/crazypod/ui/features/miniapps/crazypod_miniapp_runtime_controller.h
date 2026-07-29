#ifndef CRAZYPOD_MINIAPP_RUNTIME_CONTROLLER_H
#define CRAZYPOD_MINIAPP_RUNTIME_CONTROLLER_H

#include <stdbool.h>

#include "../../../../../miniapps/sdk/crazypod_miniapp.h"
#include "../../navigation/crazypod_ui_routes.h"

struct crazypod_miniapp_activation_host {
    void (*push)(enum crazypod_route route, int group);
    void (*render)(bool transition);
};

void crazypod_miniapp_runtime_reset_input(void);
void crazypod_miniapp_runtime_initialize(void);
int crazypod_miniapp_runtime_prepare(void);
void crazypod_miniapp_runtime_rescan(void);
void crazypod_miniapp_runtime_note_error(int error);
int crazypod_miniapp_runtime_last_error(void);
void crazypod_miniapp_runtime_opened(void);
void crazypod_miniapp_runtime_push_wheel(
    const struct cp_input_event *event);
bool crazypod_miniapp_runtime_next_input(
    bool frame_due, struct cp_input_event *event);
void crazypod_miniapp_runtime_request_render(void);
bool crazypod_miniapp_runtime_take_render(void);
bool crazypod_miniapp_runtime_motion_active(void);
bool crazypod_miniapp_runtime_alert_active(void);

struct crazypod_miniapp_runtime_service_result {
    bool close_requested;
    bool beep_requested;
};

struct crazypod_miniapp_runtime_service_result
crazypod_miniapp_runtime_service(
    bool active, bool frame_due, long tick, long ticks_per_second);
bool crazypod_miniapp_runtime_activate(
    const struct route_state *state,
    const struct crazypod_miniapp_activation_host *host);

#endif
