#ifndef CRAZYPOD_BOOKS_FEATURE_H
#define CRAZYPOD_BOOKS_FEATURE_H

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"

struct crazypod_books_activation_host {
    void (*render)(bool transition);
    void (*push)(enum crazypod_route route, int group);
    void (*pop)(void);
    void (*show_font_size)(int selected);
    void (*show_theme)(int selected);
};

int crazypod_books_feature_item_count(
    const struct route_state *state);
const char *crazypod_books_feature_title(
    const struct route_state *state);
bool crazypod_books_feature_item_title(
    const struct route_state *state, int index,
    const char **title);
bool crazypod_books_feature_activate(
    const struct route_state *state,
    const struct crazypod_books_activation_host *host);
bool crazypod_books_feature_render(
    const struct route_state *state, lv_obj_t *parent);
const uint32_t *crazypod_books_feature_page_colors(void);
const uint32_t *crazypod_books_feature_ink_colors(void);
void crazypod_books_feature_reset_view(void);

#endif
