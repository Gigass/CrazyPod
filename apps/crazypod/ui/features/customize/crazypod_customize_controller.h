#ifndef CRAZYPOD_CUSTOMIZE_CONTROLLER_H
#define CRAZYPOD_CUSTOMIZE_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_customize_command_action {
    CRAZYPOD_CUSTOMIZE_COMMAND_NONE = 0,
    CRAZYPOD_CUSTOMIZE_COMMAND_RENDER,
    CRAZYPOD_CUSTOMIZE_COMMAND_APPEARANCE_CHANGED,
    CRAZYPOD_CUSTOMIZE_COMMAND_PUSH_ROUTE,
    CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_ICON_CHOICES,
    CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_APPEARANCE_CHOICES,
    CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_BACKGROUND_CHOICES,
};

struct crazypod_customize_command_result {
    enum crazypod_customize_command_action action;
    enum crazypod_route route;
    int group;
    int selected;
};

enum crazypod_customize_overlay {
    CRAZYPOD_CUSTOMIZE_OVERLAY_ICONS = 0,
    CRAZYPOD_CUSTOMIZE_OVERLAY_APPEARANCE,
    CRAZYPOD_CUSTOMIZE_OVERLAY_BACKGROUND,
};

bool crazypod_customize_controller_handles(enum crazypod_route route);
struct crazypod_customize_command_result
crazypod_customize_controller_select(
    enum crazypod_route route, int selected, int group);
int crazypod_customize_overlay_count(
    enum crazypod_customize_overlay overlay, int field);
int crazypod_customize_overlay_current(
    enum crazypod_customize_overlay overlay, int field);
const char *crazypod_customize_overlay_title(
    enum crazypod_customize_overlay overlay, int field);
const char *crazypod_customize_overlay_item_title(
    enum crazypod_customize_overlay overlay, int field, int index);
bool crazypod_customize_overlay_item_color(
    enum crazypod_customize_overlay overlay, int field,
    int index, uint32_t *color);
struct crazypod_customize_command_result
crazypod_customize_overlay_apply(
    enum crazypod_customize_overlay overlay, int field, int selected);

#endif
