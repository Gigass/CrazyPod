#ifndef CRAZYPOD_CUSTOMIZE_FEATURE_H
#define CRAZYPOD_CUSTOMIZE_FEATURE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"
#include "../crazypod_feature.h"
#include "../../navigation/crazypod_input_event.h"

struct crazypod_customize_activation_host {
    void (*render)(bool transition);
    void (*push)(enum crazypod_route route, int group);
    void (*push_selected)(
        enum crazypod_route route, int group, int selected);
    void (*pop)(void);
    void (*appearance_changed)(void);
    void (*show_icon_choices)(int group, int selected);
    void (*show_appearance_choices)(int group, int selected);
    void (*show_background_choices)(int group, int selected);
    void (*preset_deleted)(void);
};

struct crazypod_wallpaper_crop_runtime_host {
    bool (*product_active)(void);
    int (*route_depth)(void);
    enum crazypod_route (*current_route)(void);
    void (*render)(bool transition);
    void (*truncate_routes)(int depth);
    void (*pop)(void);
    void (*appearance_changed)(void);
};

int crazypod_customize_feature_item_count(
    const struct route_state *state);
const char *crazypod_customize_feature_title(
    const struct route_state *state);
bool crazypod_customize_feature_item_title(
    const struct route_state *state, int index,
    const char **title);
bool crazypod_customize_feature_item_is_current(
    const struct route_state *state, int index);
void crazypod_customize_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const char *title, uint32_t primary_color,
    uint32_t secondary_color, const char *editor_value);
bool crazypod_customize_feature_render(
    const struct route_state *state, lv_obj_t *parent,
    const lv_font_t *metadata_font, uint32_t primary_color,
    uint32_t panel_color, uint32_t foreground_color,
    uint32_t accent_color);
void crazypod_customize_feature_reset_view(void);
void crazypod_customize_feature_initialize_media(void);
enum crazypod_feature_media_update
crazypod_customize_feature_poll_media(
    enum crazypod_route route, bool blocked);
bool crazypod_customize_feature_activate(
    const struct route_state *state,
    const struct crazypod_customize_activation_host *host);
void crazypod_wallpaper_crop_runtime_configure(
    const struct crazypod_wallpaper_crop_runtime_host *host);
bool crazypod_wallpaper_crop_runtime_handle_input(
    const struct crazypod_input_event *event, long now);
void crazypod_wallpaper_crop_runtime_service(long now);
const char *crazypod_customize_feature_menu_symbol(int index);
int crazypod_customize_feature_choice_count(int field);
int crazypod_customize_feature_choice_index(int field);
const char *crazypod_customize_feature_choice_title(
    int field, int index);
int crazypod_customize_feature_choice_value(
    int field, int index);
const char *crazypod_customize_feature_field_title(int field);
int crazypod_customize_feature_field_value(int field);
int crazypod_customize_feature_background_target(int field);
const char *crazypod_customize_feature_background_title(int target);
uint32_t crazypod_customize_feature_background_color(int target);
const char *crazypod_customize_feature_background_wallpaper(int field);
void crazypod_customize_feature_clear_input_holds(void);
const char *crazypod_customize_feature_preset_editor_value(void);

#endif
