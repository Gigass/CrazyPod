#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>

#include "crazypod_ui_routes.h"

#define CRAZYPOD_UI_ROUTE_CAPACITY 8

static struct route_state route_stack[CRAZYPOD_UI_ROUTE_CAPACITY];
static int route_depth;

void crazypod_ui_routes_clear(void)
{
    route_depth = 0;
}

void crazypod_ui_routes_reset(
    enum crazypod_route route, int group, int selected)
{
    route_stack[0].route = route;
    route_stack[0].selected = selected;
    route_stack[0].group = group;
    route_depth = 1;
}

int crazypod_ui_routes_depth(void)
{
    return route_depth;
}

struct route_state *crazypod_ui_routes_current(void)
{
    if(route_depth <= 0)
        return NULL;
    return &route_stack[route_depth - 1];
}

struct route_state *crazypod_ui_routes_at(int index)
{
    if(index < 0 || index >= route_depth)
        return NULL;
    return &route_stack[index];
}

bool crazypod_ui_routes_push(
    enum crazypod_route route, int group, int selected)
{
    if(route_depth >= CRAZYPOD_UI_ROUTE_CAPACITY)
        return false;

    route_stack[route_depth].route = route;
    route_stack[route_depth].selected = selected;
    route_stack[route_depth].group = group;
    ++route_depth;
    return true;
}

bool crazypod_ui_routes_pop(void)
{
    if(route_depth <= 1)
        return false;
    --route_depth;
    return true;
}

void crazypod_ui_routes_truncate(int depth)
{
    if(depth < 0)
        depth = 0;
    if(depth > route_depth)
        depth = route_depth;
    route_depth = depth;
}

#endif
