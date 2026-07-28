#ifndef CRAZYPOD_MENU_LIST_H
#define CRAZYPOD_MENU_LIST_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "../navigation/crazypod_ui_routes.h"

void crazypod_menu_list_reset(enum crazypod_route route);
void crazypod_menu_list_clear(void);
bool crazypod_menu_list_matches(enum crazypod_route route);

void crazypod_menu_list_bind_row(int row, lv_obj_t *box,
                                 lv_obj_t *label, lv_obj_t *marker);
void crazypod_menu_list_bind_icon(int row, lv_obj_t *circle,
                                  lv_obj_t *icon, bool text_icon);
void crazypod_menu_list_bind_scroll_thumb(lv_obj_t *thumb);

void crazypod_menu_list_refresh_row(
    int row, bool visible, const char *title, bool selected,
    lv_opa_t label_opa, uint32_t panel_color, uint32_t primary_color,
    uint32_t secondary_color, bool gradient, const char *icon_text,
    lv_opa_t icon_opa, const char *marker_text, lv_opa_t marker_opa);
void crazypod_menu_list_refresh_scroll(int y, int height);

#endif
