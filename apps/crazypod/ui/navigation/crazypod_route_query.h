#ifndef CRAZYPOD_ROUTE_QUERY_H
#define CRAZYPOD_ROUTE_QUERY_H

#include "../crazypod_menu_icon.h"
#include "crazypod_ui_routes.h"

int crazypod_route_query_item_count(
    const struct route_state *state, const char *music_search_query);
const char *crazypod_route_query_item_title(
    const struct route_state *state, int index,
    const char *music_search_query,
    bool stopwatch_running, bool workout_running);
enum crazypod_menu_icon crazypod_route_query_item_icon(
    const struct route_state *state, int index);
const char *crazypod_route_query_title(
    const struct route_state *state);
bool crazypod_route_query_item_is_current(
    const struct route_state *state, int index);

#endif
