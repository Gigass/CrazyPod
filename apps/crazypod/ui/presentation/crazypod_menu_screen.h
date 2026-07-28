#ifndef CRAZYPOD_MENU_SCREEN_H
#define CRAZYPOD_MENU_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "../navigation/crazypod_ui_routes.h"

struct crazypod_menu_screen_context {
    lv_obj_t *parent;
    int item_count;
    uint32_t primary_color;
    uint32_t secondary_color;
    uint32_t panel_color;
    bool gradient_highlight;
    const lv_font_t *metadata_font;
    const char *(*item_title)(
        const struct route_state *state, int index);
    bool (*item_is_current)(
        const struct route_state *state, int index);
};

void crazypod_menu_screen_render(
    const struct route_state *state,
    const struct crazypod_menu_screen_context *context);

#endif
