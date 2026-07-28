#ifndef CRAZYPOD_SEARCH_SCREEN_H
#define CRAZYPOD_SEARCH_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"
#include "../../presentation/crazypod_glass_slots.h"

typedef lv_obj_t *(*crazypod_search_panel_factory)(
    lv_obj_t *parent, enum crazypod_glass_slot slot,
    int x, int y, int width, int height, int radius);

struct crazypod_search_screen_context {
    lv_obj_t *parent;
    const char *query;
    int item_count;
    uint32_t primary_color;
    uint32_t secondary_color;
    uint32_t panel_color;
    bool gradient_highlight;
    const lv_font_t *metadata_font;
    const char *(*item_title)(
        const struct route_state *state, int index);
    crazypod_search_panel_factory make_panel;
};

void crazypod_search_screen_render(
    const struct route_state *state,
    const struct crazypod_search_screen_context *context);

#endif
