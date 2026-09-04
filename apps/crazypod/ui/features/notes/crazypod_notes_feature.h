#ifndef CRAZYPOD_NOTES_FEATURE_H
#define CRAZYPOD_NOTES_FEATURE_H

#include <stdint.h>

#include "lvgl.h"

#include "../../crazypod_menu_icon.h"
#include "../../navigation/crazypod_ui_routes.h"
#include "../crazypod_feature.h"

struct crazypod_notes_activation_host {
    void (*render)(bool transition);
    void (*operation_failed)(void);
    void (*push)(enum crazypod_route route, int group);
    void (*pop)(void);
    void (*pop_composer)(void);
    void (*open_composer)(uint32_t id, bool resume_draft);
    void (*open_reader)(uint32_t id);
    void (*reset_open_reader)(uint32_t id);
    void (*commit_editor)(void);
};

enum crazypod_notes_confirmation_navigation {
    CRAZYPOD_NOTES_CONFIRMATION_NONE = 0,
    CRAZYPOD_NOTES_CONFIRMATION_RESET_MENU,
    CRAZYPOD_NOTES_CONFIRMATION_RESET_MENU_SHOW_DELETED,
    CRAZYPOD_NOTES_CONFIRMATION_TRUNCATE,
};

struct crazypod_notes_confirmation_result {
    bool handled;
    bool succeeded;
    enum crazypod_notes_confirmation_navigation navigation;
    int depth;
};

int crazypod_notes_feature_item_count(
    const struct route_state *state);
const char *crazypod_notes_feature_title(
    const struct route_state *state);
bool crazypod_notes_feature_item_title(
    const struct route_state *state, int index,
    const char **title);
enum crazypod_menu_icon crazypod_notes_feature_item_icon(
    const struct route_state *state, int index);
bool crazypod_notes_feature_activate(
    const struct route_state *state,
    const struct crazypod_notes_activation_host *host);
bool crazypod_notes_feature_render(
    const struct route_state *state, lv_obj_t *parent);
bool crazypod_notes_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context);
void crazypod_notes_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font);
void crazypod_notes_feature_refresh_draft(void);
void crazypod_notes_feature_toggle_editor_field(void);
void crazypod_notes_feature_begin_editor(
    uint32_t id, bool resume_draft);
void crazypod_notes_feature_load_reader(uint32_t id);
bool crazypod_notes_feature_editor_dirty(void);
void crazypod_notes_feature_service_editor(void);
void crazypod_notes_feature_save_draft(void);
uint32_t crazypod_notes_feature_commit_editor(void);
bool crazypod_notes_feature_draft_available(void);
struct crazypod_notes_confirmation_result
crazypod_notes_feature_confirm(
    const struct route_state *state, int route_depth);

#endif
