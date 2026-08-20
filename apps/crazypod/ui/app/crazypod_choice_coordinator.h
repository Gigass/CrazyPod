#ifndef CRAZYPOD_CHOICE_COORDINATOR_H
#define CRAZYPOD_CHOICE_COORDINATOR_H

#include "lvgl.h"

#include "../../crazypod_apps.h"
#include "../navigation/crazypod_ui_routes.h"

enum crazypod_choice_kind {
    CRAZYPOD_CHOICE_NONE = 0,
    CRAZYPOD_CHOICE_ICON_THEME,
    CRAZYPOD_CHOICE_APPEARANCE,
    CRAZYPOD_CHOICE_BACKGROUND,
    CRAZYPOD_CHOICE_SETTING,
    CRAZYPOD_CHOICE_BOOK_FONT_SIZE,
    CRAZYPOD_CHOICE_BOOK_THEME,
    CRAZYPOD_CHOICE_NOW_PLAYING_THEME,
    CRAZYPOD_CHOICE_MAIN_MENU_ITEM_ACTIONS,
    CRAZYPOD_CHOICE_ROUTE_ACTIONS,
    CRAZYPOD_CHOICE_RECEIPT,
};

struct crazypod_choice_coordinator_host {
    lv_obj_t *parent;
    const lv_font_t *metadata_font;
    bool (*route_available)(void);
    void (*render)(bool transition);
    void (*push_selected)(
        enum crazypod_route route, int group, int selected);
    void (*appearance_changed)(void);
    int (*item_count)(const struct route_state *state);
    const char *(*item_title)(
        const struct route_state *state, int index);
    bool (*item_is_current)(
        const struct route_state *state, int index);
    void (*activate_route)(
        const struct route_state *state, long now);
    enum crazypod_app_id (*selected_app)(void);
    void (*main_menu_changed)(
        enum crazypod_app_id preferred,
        enum crazypod_app_id changed);
    void (*begin_main_menu_reorder)(
        enum crazypod_app_id id);
};

void crazypod_choice_coordinator_configure(
    const struct crazypod_choice_coordinator_host *host);
void crazypod_choice_coordinator_reset(void);
bool crazypod_choice_coordinator_visible(void);
void crazypod_choice_coordinator_show(
    enum crazypod_choice_kind kind, int id, int selected);
void crazypod_choice_coordinator_show_route(
    enum crazypod_route route, int group, int selected);
void crazypod_choice_coordinator_show_receipt(
    const char *label, bool success, long now,
    bool refresh_route);
void crazypod_choice_coordinator_dismiss(bool refresh_route);
bool crazypod_choice_coordinator_back(void);
void crazypod_choice_coordinator_move(int direction);
void crazypod_choice_coordinator_activate(long now);
void crazypod_choice_coordinator_tick(long now);
int crazypod_choice_coordinator_wait_ticks(long now);
bool crazypod_choice_coordinator_route_should_overlay(
    enum crazypod_route route);
bool crazypod_choice_coordinator_confirmation_visible(void);
const struct route_state *
crazypod_choice_coordinator_route_state(void);
bool crazypod_choice_coordinator_owns_route_state(
    const struct route_state *state);

#endif
