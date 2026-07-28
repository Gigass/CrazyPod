#ifndef CRAZYPOD_MENU_PREVIEW_H
#define CRAZYPOD_MENU_PREVIEW_H

#include "lvgl.h"

#include "../../crazypod_artwork.h"
#include "../navigation/crazypod_ui_routes.h"

#define CRAZYPOD_MENU_PREVIEW_FLOW_SLOT_BASE \
    (CRAZYPOD_COVERFLOW_ARTWORK_SLOTS - 3)

struct crazypod_menu_preview_host {
    lv_obj_t *parent;
    const lv_font_t *metadata_font;
    const char *(*item_title)(
        const struct route_state *state, int index);
};

void crazypod_menu_preview_configure(
    const struct crazypod_menu_preview_host *host);
void crazypod_menu_preview_reset(void);
void crazypod_menu_preview_render(
    const struct route_state *state, bool animated);
void crazypod_menu_preview_prefetch(
    const struct route_state *state);
void crazypod_menu_preview_settle(void);
bool crazypod_menu_preview_motion_ready(void);
bool crazypod_menu_preview_is_music_route(
    enum crazypod_route route);
bool crazypod_menu_preview_is_skeuomorphic_route(
    enum crazypod_route route);

#endif
