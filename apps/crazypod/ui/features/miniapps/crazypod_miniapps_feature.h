#ifndef CRAZYPOD_MINIAPPS_FEATURE_H
#define CRAZYPOD_MINIAPPS_FEATURE_H

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"
#include "../crazypod_feature.h"

struct cp_input_event;

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
void crazypod_miniapps_feature_render_active(
    lv_obj_t *parent, uint32_t primary_color);
void crazypod_miniapps_feature_note_opened(void);
void crazypod_miniapps_feature_push_wheel(
    const struct cp_input_event *event);
void crazypod_miniapps_feature_push_wheel_coalesced(
    const struct cp_input_event *event);
bool crazypod_miniapps_feature_modal_visible(void);
bool crazypod_miniapps_feature_surface_attached(lv_obj_t *parent);
void crazypod_miniapps_feature_initialize(void);
bool crazypod_miniapps_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context);
enum crazypod_miniapps_service_event {
    CRAZYPOD_MINIAPPS_SERVICE_NONE = 0,
    CRAZYPOD_MINIAPPS_SERVICE_CLOSE = 1,
    CRAZYPOD_MINIAPPS_SERVICE_RENDER = 2,
};
int crazypod_miniapps_feature_service(
    bool active, bool frame_due, long now,
    long ticks_per_second);
bool crazypod_miniapps_feature_is_open(void);
bool crazypod_miniapps_feature_motion_active(void);
void crazypod_miniapps_feature_close(void);
void crazypod_miniapps_feature_reset_input(void);
void crazypod_miniapps_feature_rescan(void);
int crazypod_miniapps_feature_last_error(void);
void crazypod_miniapps_feature_initialize_runtime(void);
int crazypod_miniapps_feature_prepare(void);
struct crazypod_miniapps_activation_host {
    void (*push)(enum crazypod_route route, int group);
    void (*render)(bool transition);
};
bool crazypod_miniapps_feature_activate(
    const struct route_state *state,
    const struct crazypod_miniapps_activation_host *host);
unsigned crazypod_miniapps_feature_input_count(void);
bool crazypod_miniapps_feature_exit_prompt_visible(void);
bool crazypod_miniapps_feature_has_scene_content(void);
void crazypod_miniapps_feature_refresh_now_playing_artwork(void);
#ifdef SIMULATOR
bool crazypod_miniapps_feature_simulate_long_menu(
    long now, long ticks_per_second);
#endif
#endif
