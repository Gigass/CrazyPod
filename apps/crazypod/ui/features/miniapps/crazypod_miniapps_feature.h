#ifndef CRAZYPOD_MINIAPPS_FEATURE_H
#define CRAZYPOD_MINIAPPS_FEATURE_H

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"

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

#endif
