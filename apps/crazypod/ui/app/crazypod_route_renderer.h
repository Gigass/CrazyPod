#ifndef CRAZYPOD_ROUTE_RENDERER_H
#define CRAZYPOD_ROUTE_RENDERER_H

#include "lvgl.h"

#include "../navigation/crazypod_ui_routes.h"

struct crazypod_track;

struct crazypod_route_renderer_host {
    const lv_font_t *metadata_font;
    int (*item_count)(const struct route_state *state);
    const char *(*item_title)(
        const struct route_state *state, int index);
    bool (*item_is_current)(
        const struct route_state *state, int index);
    lv_obj_t *(*render_artwork)(
        lv_obj_t *parent, const struct crazypod_track *track,
        int x, int y, int display_size,
        const lv_image_dsc_t *descriptor,
        bool scale_descriptor);
    void (*boost)(int ticks);
};

void crazypod_route_renderer_configure(
    const struct crazypod_route_renderer_host *host);
void crazypod_route_renderer_render(
    const struct route_state *state, long now,
    bool transition);
void crazypod_route_renderer_prepare_loading(void);

#endif
