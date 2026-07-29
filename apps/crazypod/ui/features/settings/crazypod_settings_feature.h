#ifndef CRAZYPOD_SETTINGS_FEATURE_H
#define CRAZYPOD_SETTINGS_FEATURE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "../../../crazypod_apps.h"
#include "../../navigation/crazypod_ui_routes.h"
#include "../crazypod_feature.h"

struct crazypod_settings_activation_host {
    enum crazypod_app_id (*selected_app)(void);
    void (*push)(enum crazypod_route route, int group);
    void (*render)(bool transition);
    void (*main_menu_changed)(
        enum crazypod_app_id preferred,
        enum crazypod_app_id changed);
    void (*show_choices)(int item, int selected);
};

int crazypod_settings_feature_item_count(
    const struct route_state *state);
const char *crazypod_settings_feature_title(
    const struct route_state *state);
bool crazypod_settings_feature_item_title(
    const struct route_state *state, int index,
    const char **title);
bool crazypod_settings_feature_item_is_current(
    const struct route_state *state, int index);
void crazypod_settings_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const char *title, uint32_t primary_color,
    uint32_t secondary_color, bool eq_enabled,
    bool shuffle_enabled, bool repeat_enabled);
bool crazypod_settings_feature_activate(
    const struct route_state *state,
    const struct crazypod_settings_activation_host *host);
bool crazypod_settings_feature_render(
    const struct route_state *state, lv_obj_t *parent,
    const lv_font_t *metadata_font, uint32_t primary_color);
bool crazypod_settings_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context);
const char *crazypod_settings_feature_menu_symbol(int index);
int crazypod_settings_feature_choice_count(int item);
int crazypod_settings_feature_choice_index(int item);
const char *crazypod_settings_feature_choice_item_title(int item);
const char *crazypod_settings_feature_choice_title(
    int item, int index);
void crazypod_settings_feature_apply_choice(
    int item, int index);

#endif
