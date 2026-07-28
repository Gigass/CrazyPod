#include "crazypod_navigation_command.h"

static struct crazypod_navigation_command command(
    enum crazypod_navigation_command_kind kind,
    enum crazypod_route route, int group, int selected,
    bool transition)
{
    const struct crazypod_navigation_command result = {
        kind, route, group, selected, transition
    };

    return result;
}

struct crazypod_navigation_command crazypod_navigation_none(void)
{
    return command(
        CRAZYPOD_NAVIGATION_NONE, MUSIC_ROUTE_MENU,
        -1, 0, false);
}

struct crazypod_navigation_command crazypod_navigation_push(
    enum crazypod_route route, int group, int selected)
{
    return command(
        CRAZYPOD_NAVIGATION_PUSH, route, group, selected, true);
}

struct crazypod_navigation_command crazypod_navigation_pop(void)
{
    return command(
        CRAZYPOD_NAVIGATION_POP, MUSIC_ROUTE_MENU,
        -1, 0, true);
}

struct crazypod_navigation_command crazypod_navigation_render(
    bool transition)
{
    return command(
        CRAZYPOD_NAVIGATION_RENDER, MUSIC_ROUTE_MENU,
        -1, 0, transition);
}
