#ifndef CRAZYPOD_MINIAPPS_FEATURE_H
#define CRAZYPOD_MINIAPPS_FEATURE_H

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"
#include "../crazypod_feature.h"

int crazypod_miniapps_feature_item_count(
    const struct route_state *state);
const char *crazypod_miniapps_feature_title(
    const struct route_state *state);
bool crazypod_miniapps_feature_item_title(
    const struct route_state *state, int index,
    const char **title);
bool crazypod_miniapps_feature_render(
    const struct route_state *state, lv_obj_t *parent,
    uint32_t primary_color);
void crazypod_miniapps_feature_initialize(void);
bool crazypod_miniapps_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context);
enum crazypod_miniapps_service_event {
    CRAZYPOD_MINIAPPS_SERVICE_NONE = 0,
    CRAZYPOD_MINIAPPS_SERVICE_CLOSE = 1,
    CRAZYPOD_MINIAPPS_SERVICE_BEEP = 2,
    CRAZYPOD_MINIAPPS_SERVICE_RENDER = 4,
};
int crazypod_miniapps_feature_service(
    bool active, bool frame_due, long now,
    long ticks_per_second);
bool crazypod_miniapps_feature_is_open(void);
void crazypod_miniapps_feature_close(void);
void crazypod_miniapps_feature_reset_input(void);
void crazypod_miniapps_feature_rescan(void);
int crazypod_miniapps_feature_last_error(void);
void crazypod_miniapps_feature_initialize_runtime(void);
struct crazypod_miniapps_activation_host {
    void (*push)(enum crazypod_route route, int group);
    void (*render)(bool transition);
};
bool crazypod_miniapps_feature_activate(
    const struct route_state *state,
    const struct crazypod_miniapps_activation_host *host);

#endif
