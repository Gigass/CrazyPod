#ifndef CRAZYPOD_BOOKS_FEATURE_H
#define CRAZYPOD_BOOKS_FEATURE_H

#include "lvgl.h"

#include "../../crazypod_menu_icon.h"
#include "../../navigation/crazypod_ui_routes.h"
#include "../crazypod_feature.h"

struct crazypod_books_activation_host {
    void (*render)(bool transition);
    void (*operation_failed)(void);
    void (*push)(enum crazypod_route route, int group);
    void (*pop)(void);
    void (*show_font_size)(int selected);
    void (*show_theme)(int selected);
};

struct crazypod_books_runtime_host {
    lv_obj_t *parent;
    const lv_font_t *metadata_font;
    const uint32_t *page_colors;
    const uint32_t *ink_colors;
    void (*set_status_palette)(
        uint32_t foreground, uint32_t background);
    void (*status_foreground)(void);
    void (*present)(void);
    void (*render_route)(bool transition);
    void (*push_reader)(int book_index);
};

struct crazypod_books_confirmation_result {
    bool handled;
    bool deleted;
};

int crazypod_books_feature_item_count(
    const struct route_state *state);
const char *crazypod_books_feature_title(
    const struct route_state *state);
bool crazypod_books_feature_item_title(
    const struct route_state *state, int index,
    const char **title);
enum crazypod_menu_icon crazypod_books_feature_item_icon(
    const struct route_state *state, int index);
bool crazypod_books_feature_activate(
    const struct route_state *state,
    const struct crazypod_books_activation_host *host);
bool crazypod_books_feature_render(
    const struct route_state *state, lv_obj_t *parent);
const uint32_t *crazypod_books_feature_page_colors(void);
const uint32_t *crazypod_books_feature_ink_colors(void);
void crazypod_books_feature_reset_view(void);
bool crazypod_books_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context);
void crazypod_books_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font);
void crazypod_books_feature_configure_runtime(
    const struct crazypod_books_runtime_host *host);
void crazypod_books_feature_ensure_metadata(void);
void crazypod_books_feature_invalidate_metadata(void);
void crazypod_books_feature_apply_font_size(int value);
void crazypod_books_feature_begin_reader(
    int index, uint32_t offset);
void crazypod_books_feature_turn_page(int direction);
struct crazypod_books_confirmation_result
crazypod_books_feature_confirm(
    const struct route_state *state);

#endif
