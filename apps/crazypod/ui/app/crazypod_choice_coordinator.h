#ifndef CRAZYPOD_CHOICE_COORDINATOR_H
#define CRAZYPOD_CHOICE_COORDINATOR_H

#include "lvgl.h"

#include "../navigation/crazypod_ui_routes.h"

enum crazypod_choice_kind {
    CRAZYPOD_CHOICE_NONE = 0,
    CRAZYPOD_CHOICE_ICON_THEME,
    CRAZYPOD_CHOICE_APPEARANCE,
    CRAZYPOD_CHOICE_BACKGROUND,
    CRAZYPOD_CHOICE_SETTING,
    CRAZYPOD_CHOICE_BOOK_FONT_SIZE,
    CRAZYPOD_CHOICE_BOOK_THEME,
};

struct crazypod_choice_coordinator_host {
    lv_obj_t *parent;
    const lv_font_t *metadata_font;
    bool (*route_available)(void);
    void (*render)(bool transition);
    void (*push_selected)(
        enum crazypod_route route, int group, int selected);
    void (*appearance_changed)(void);
};

void crazypod_choice_coordinator_configure(
    const struct crazypod_choice_coordinator_host *host);
void crazypod_choice_coordinator_reset(void);
bool crazypod_choice_coordinator_visible(void);
void crazypod_choice_coordinator_show(
    enum crazypod_choice_kind kind, int id, int selected);
void crazypod_choice_coordinator_dismiss(bool refresh_route);
void crazypod_choice_coordinator_move(int direction);
void crazypod_choice_coordinator_activate(void);

#endif
