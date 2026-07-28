#ifndef CRAZYPOD_ALBUM_FLOW_SCREEN_H
#define CRAZYPOD_ALBUM_FLOW_SCREEN_H

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"

void crazypod_album_flow_screen_reset(void);
void crazypod_album_flow_screen_render(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font);
int crazypod_album_flow_screen_sync(void);

#endif
