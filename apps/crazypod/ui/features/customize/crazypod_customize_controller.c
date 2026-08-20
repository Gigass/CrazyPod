#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <string.h>

#include "../../../crazypod_appearance.h"
#include "../../../crazypod_photos.h"
#include "../../../crazypod_state.h"
#include "../../../crazypod_wallpaper.h"
#include "crazypod_wallpaper_crop_controller.h"
#include "crazypod_customize_catalog.h"
#include "crazypod_customize_controller.h"
#include "../now_playing/crazypod_now_playing_feature.h"

static int photo_index_for_path(const char *path)
{
    int index;

    if(path == NULL || path[0] == '\0')
        return 0;
    for(index = 0; index < crazypod_photo_count(); ++index) {
        if(strcmp(path, crazypod_photo_path(index)) == 0)
            return index;
    }
    return 0;
}

static struct crazypod_customize_command_result result(
    enum crazypod_customize_command_action action,
    enum crazypod_route route, int group, int selected)
{
    struct crazypod_customize_command_result value = {
        .action = action,
        .route = route,
        .group = group,
        .selected = selected,
    };

    return value;
}

bool crazypod_customize_controller_handles(enum crazypod_route route)
{
    return route == DIY_ROUTE_MENU ||
        route == DIY_ROUTE_ICONS ||
        route == DIY_ROUTE_DETAILS ||
        route == DIY_ROUTE_CHOICES ||
        route == DIY_ROUTE_BACKGROUNDS ||
        route == DIY_ROUTE_BACKGROUND_CHOICES ||
        route == DIY_ROUTE_WALLPAPER_FILES ||
        route == DIY_ROUTE_LAYOUT ||
        route == DIY_ROUTE_NOW_PLAYING_THEMES ||
        route == DIY_ROUTE_HEADPHONE_POPUP;
}

struct crazypod_customize_command_result
crazypod_customize_controller_select(
    enum crazypod_route route, int selected, int group)
{
    enum crazypod_appearance_field field;

    switch(route) {
    case DIY_ROUTE_MENU:
        if(selected == 0)
            return result(
                CRAZYPOD_CUSTOMIZE_COMMAND_PUSH_ROUTE,
                DIY_ROUTE_PRESETS, -1, 0);
        if(selected == 1)
            return result(
                CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_ICON_CHOICES,
                route, CRAZYPOD_APPEARANCE_ICON_THEME,
                crazypod_appearance_get()->icon_theme);
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_PUSH_ROUTE,
            selected == 2 ? DIY_ROUTE_DETAILS :
            selected == 3 ? DIY_ROUTE_BACKGROUNDS :
            selected == 4 ? DIY_ROUTE_NOW_PLAYING_THEMES :
            selected == 5 ? DIY_ROUTE_HEADPHONE_POPUP :
                            DIY_ROUTE_LAYOUT,
            -1, selected == 5
                ? crazypod_state_headphone_popup_style() : 0);
    case DIY_ROUTE_ICONS:
        crazypod_appearance_set_icon_theme(selected);
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_APPEARANCE_CHANGED,
            route, 0, 0);
    case DIY_ROUTE_DETAILS:
        field = crazypod_customize_detail_fields[selected];
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_APPEARANCE_CHOICES,
            route, field, crazypod_customize_choice_index(field));
    case DIY_ROUTE_CHOICES:
        field = (enum crazypod_appearance_field)group;
        crazypod_appearance_set_value(
            field, crazypod_customize_choice_value(field, selected));
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_APPEARANCE_CHANGED,
            route, 0, 0);
    case DIY_ROUTE_BACKGROUNDS: {
        const struct crazypod_appearance *appearance =
            crazypod_appearance_get();
        const char *path;

        field = crazypod_customize_background_field(selected);
        path = crazypod_customize_background_wallpaper(
            appearance, field);
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_BACKGROUND_CHOICES,
            route, field,
            path[0] != '\0'
                ? CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
                : crazypod_customize_field_value(field));
    }
    case DIY_ROUTE_BACKGROUND_CHOICES:
        field = (enum crazypod_appearance_field)group;
        if(selected == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1) {
            const char *path =
                crazypod_customize_background_wallpaper(
                    crazypod_appearance_get(), field);

            crazypod_photos_set_route_suspended(false);
            crazypod_photos_ensure_catalog();
            return result(
                CRAZYPOD_CUSTOMIZE_COMMAND_PUSH_ROUTE,
                DIY_ROUTE_WALLPAPER_FILES, field,
                photo_index_for_path(path));
        }
        crazypod_wallpaper_clear(
            crazypod_customize_background_target(field));
        crazypod_appearance_set_value(field, selected);
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_APPEARANCE_CHANGED,
            route, 0, 0);
    case DIY_ROUTE_WALLPAPER_FILES:
        if(selected < 0 || selected >= crazypod_photo_count())
            return result(
                CRAZYPOD_CUSTOMIZE_COMMAND_RENDER, route, 0, 0);
        crazypod_wallpaper_crop_controller_start(
            selected, (enum crazypod_appearance_field)group);
        crazypod_photo_view(selected);
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_PUSH_ROUTE,
            DIY_ROUTE_WALLPAPER_CROP, selected, 0);
    case DIY_ROUTE_LAYOUT:
        field = crazypod_customize_layout_fields[selected];
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_APPEARANCE_CHOICES,
            route, field, crazypod_customize_choice_index(field));
    case DIY_ROUTE_NOW_PLAYING_THEMES:
        return result(
            crazypod_now_playing_theme_select(selected)
                ? CRAZYPOD_CUSTOMIZE_COMMAND_RENDER
                : CRAZYPOD_CUSTOMIZE_COMMAND_NONE,
            route, 0, selected);
    case DIY_ROUTE_HEADPHONE_POPUP:
        crazypod_state_set_headphone_popup_style(
            (enum crazypod_headphone_popup_style)selected);
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_RENDER,
            route, 0, selected);
    default:
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_NONE, route, 0, 0);
    }
}

int crazypod_customize_overlay_count(
    enum crazypod_customize_overlay overlay, int field)
{
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_ICONS)
        return CRAZYPOD_ICON_THEME_COUNT;
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_APPEARANCE)
        return crazypod_customize_choice_count(
            (enum crazypod_appearance_field)field);
    return CRAZYPOD_APPEARANCE_COLOR_COUNT + 2;
}

int crazypod_customize_overlay_current(
    enum crazypod_customize_overlay overlay, int field)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    const char *path;

    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_ICONS)
        return appearance->icon_theme;
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_APPEARANCE)
        return crazypod_customize_choice_index(
            (enum crazypod_appearance_field)field);
    path = crazypod_customize_background_wallpaper(
        appearance, (enum crazypod_appearance_field)field);
    return path[0] != '\0'
        ? CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
        : crazypod_customize_field_value(
            (enum crazypod_appearance_field)field);
}

const char *crazypod_customize_overlay_title(
    enum crazypod_customize_overlay overlay, int field)
{
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_ICONS)
        return CP_TR("ICON THEME");
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_APPEARANCE)
        return crazypod_customize_field_title(
            (enum crazypod_appearance_field)field);
    return crazypod_customize_background_title(
        (enum crazypod_appearance_field)field);
}

const char *crazypod_customize_overlay_item_title(
    enum crazypod_customize_overlay overlay, int field, int index)
{
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_ICONS)
        return crazypod_icon_theme_name(index);
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_APPEARANCE)
        return crazypod_customize_choice_title(
            (enum crazypod_appearance_field)field, index);
    if(index == 0)
        return CP_TR("Default");
    if(index <= CRAZYPOD_APPEARANCE_COLOR_COUNT)
        return crazypod_appearance_color_name(index - 1);
    return CP_TR("Choose Picture");
}

bool crazypod_customize_overlay_item_color(
    enum crazypod_customize_overlay overlay, int field,
    int index, uint32_t *color)
{
    enum crazypod_appearance_field appearance_field =
        (enum crazypod_appearance_field)field;

    if(color == NULL)
        return false;
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_APPEARANCE &&
       (appearance_field == CRAZYPOD_APPEARANCE_PRIMARY ||
        appearance_field == CRAZYPOD_APPEARANCE_SECONDARY)) {
        *color = crazypod_appearance_color(
            crazypod_customize_choice_value(
                appearance_field, index));
        return true;
    }
    if(overlay != CRAZYPOD_CUSTOMIZE_OVERLAY_BACKGROUND)
        return false;
    *color = index > 0 &&
             index <= CRAZYPOD_APPEARANCE_COLOR_COUNT
        ? crazypod_appearance_color(index - 1)
        : crazypod_customize_background_default_color(
            appearance_field);
    return true;
}

struct crazypod_customize_command_result
crazypod_customize_overlay_apply(
    enum crazypod_customize_overlay overlay, int field, int selected)
{
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_ICONS) {
        crazypod_appearance_set_icon_theme(selected);
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_APPEARANCE_CHANGED,
            DIY_ROUTE_ICONS, 0, 0);
    }
    if(overlay == CRAZYPOD_CUSTOMIZE_OVERLAY_APPEARANCE) {
        enum crazypod_appearance_field appearance_field =
            (enum crazypod_appearance_field)field;

        crazypod_appearance_set_value(
            appearance_field,
            crazypod_customize_choice_value(
                appearance_field, selected));
        return result(
            CRAZYPOD_CUSTOMIZE_COMMAND_APPEARANCE_CHANGED,
            DIY_ROUTE_CHOICES, 0, 0);
    }
    return crazypod_customize_controller_select(
        DIY_ROUTE_BACKGROUND_CHOICES, selected, field);
}

#endif
