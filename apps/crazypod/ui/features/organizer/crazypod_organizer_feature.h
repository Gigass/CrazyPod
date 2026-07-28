#ifndef CRAZYPOD_ORGANIZER_FEATURE_H
#define CRAZYPOD_ORGANIZER_FEATURE_H

#include <stdint.h>

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"

struct crazypod_organizer_activation_host {
    void (*render)(bool transition);
    void (*push)(enum crazypod_route route, int group);
    void (*pop)(void);
    void (*begin_editor)(uint32_t id, int fallback_date);
    bool (*commit_editor)(void);
};

struct crazypod_organizer_render_context {
    lv_obj_t *parent;
    long now;
    long ticks_per_second;
};

int crazypod_organizer_feature_item_count(
    const struct route_state *state);
const char *crazypod_organizer_feature_title(
    const struct route_state *state);
bool crazypod_organizer_feature_item_title(
    const struct route_state *state, int index,
    bool stopwatch_running, bool workout_running,
    const char **title);
bool crazypod_organizer_feature_service(
    enum crazypod_route route, long now,
    long ticks_per_second);
bool crazypod_organizer_feature_activate(
    struct route_state *state, long now,
    const struct crazypod_organizer_activation_host *host);
bool crazypod_organizer_feature_render(
    const struct route_state *state,
    const struct crazypod_organizer_render_context *context);
uint32_t crazypod_organizer_feature_background(
    enum crazypod_route route);

#endif
