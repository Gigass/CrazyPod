#ifndef CRAZYPOD_NOTES_FEATURE_H
#define CRAZYPOD_NOTES_FEATURE_H

#include <stdint.h>

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"

struct crazypod_notes_activation_host {
    void (*render)(bool transition);
    void (*push)(enum crazypod_route route, int group);
    void (*pop)(void);
    void (*pop_composer)(void);
    void (*open_composer)(uint32_t id, bool resume_draft);
    void (*open_reader)(uint32_t id);
    void (*reset_open_reader)(uint32_t id);
    void (*commit_editor)(void);
};

int crazypod_notes_feature_item_count(
    const struct route_state *state);
const char *crazypod_notes_feature_title(
    const struct route_state *state);
bool crazypod_notes_feature_item_title(
    const struct route_state *state, int index,
    const char **title);
bool crazypod_notes_feature_activate(
    const struct route_state *state,
    const struct crazypod_notes_activation_host *host);
bool crazypod_notes_feature_render(
    const struct route_state *state, lv_obj_t *parent);

#endif
