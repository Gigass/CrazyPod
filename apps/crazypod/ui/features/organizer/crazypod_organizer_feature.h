#ifndef CRAZYPOD_ORGANIZER_FEATURE_H
#define CRAZYPOD_ORGANIZER_FEATURE_H

#include <stdint.h>
#include <stddef.h>

#include "lvgl.h"

#include "../../crazypod_menu_icon.h"
#include "../../navigation/crazypod_ui_routes.h"
#include "../crazypod_feature.h"

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

enum crazypod_organizer_confirmation_navigation {
    CRAZYPOD_ORGANIZER_CONFIRMATION_NONE = 0,
    CRAZYPOD_ORGANIZER_CONFIRMATION_SHOW_CALENDAR_DAY,
    CRAZYPOD_ORGANIZER_CONFIRMATION_RESET_WORKOUT_MENU,
    CRAZYPOD_ORGANIZER_CONFIRMATION_SHOW_WORKOUT_HISTORY,
};

struct crazypod_organizer_confirmation_result {
    bool handled;
    bool succeeded;
    enum crazypod_organizer_confirmation_navigation navigation;
    int date;
};

int crazypod_organizer_feature_item_count(
    const struct route_state *state);
const char *crazypod_organizer_feature_title(
    const struct route_state *state);
bool crazypod_organizer_feature_item_title(
    const struct route_state *state, int index,
    bool stopwatch_running, bool workout_running,
    const char **title);
enum crazypod_menu_icon crazypod_organizer_feature_item_icon(
    const struct route_state *state, int index);
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
bool crazypod_organizer_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context);
bool crazypod_organizer_feature_stopwatch_running(void);
bool crazypod_organizer_feature_workout_running(void);
void crazypod_organizer_feature_pause_workout(long now);
const char *crazypod_organizer_feature_editor_title(
    char *buffer, size_t buffer_size);
void crazypod_organizer_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const char *title, int miniapp_error,
    const lv_font_t *metadata_font);
void crazypod_organizer_feature_set_focus_date(int date);
int crazypod_organizer_feature_focus_date(void);
void crazypod_organizer_feature_begin_editor(
    uint32_t id, int fallback_date);
uint32_t crazypod_organizer_feature_commit_editor(int *date);
#ifdef SIMULATOR
void crazypod_organizer_feature_simulator_workout(
    int activity, long accumulated_ticks,
    long started_at, bool running);
#endif
struct crazypod_organizer_confirmation_result
crazypod_organizer_feature_confirm(
    const struct route_state *state, long now,
    int ticks_per_second, int fallback_date);

#endif
