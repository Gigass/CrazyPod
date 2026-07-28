#ifndef CRAZYPOD_COMPOSITION_H
#define CRAZYPOD_COMPOSITION_H

#include "lvgl.h"

#include "../../crazypod_music.h"
#include "../navigation/crazypod_ui_routes.h"

struct crazypod_composition_host {
    const lv_font_t *metadata_font;
    long (*now)(void);
    void (*render)(bool transition);
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
    void (*set_boost)(bool enabled);
    void (*close_product)(void);
    void (*refresh_menu_rows)(
        const struct route_state *state);
    void (*begin_music_scan)(void);
    void (*show_lock)(bool turn_display_off);
};

void crazypod_composition_configure(
    const struct crazypod_composition_host *host);

#endif
