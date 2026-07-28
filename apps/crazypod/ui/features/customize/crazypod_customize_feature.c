#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "../../../crazypod_appearance.h"
#include "../../../crazypod_icons.h"
#include "../../../crazypod_photos.h"
#include "../../../crazypod_presets.h"
#include "crazypod_customize_catalog.h"
#include "crazypod_customize_controller.h"
#include "crazypod_customize_feature.h"
#include "../photos/crazypod_photos_feature.h"
#include "crazypod_preset_editor_controller.h"
#include "crazypod_wallpaper_crop_controller.h"
#include "crazypod_wallpaper_crop_screen.h"

#define EDITOR_CHARACTER_COUNT 36

static unsigned photo_generation_seen;
static unsigned photo_view_generation_seen;

static const char *const editor_characters[EDITOR_CHARACTER_COUNT] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

static const char *editor_title(int index)
{
    if(index >= 0 && index < EDITOR_CHARACTER_COUNT)
        return editor_characters[index];
    if(index == EDITOR_CHARACTER_COUNT)
        return "Space";
    if(index == EDITOR_CHARACTER_COUNT + 1)
        return "Backspace";
    if(index == EDITOR_CHARACTER_COUNT + 2)
        return "Save Name";
    return "";
}

int crazypod_customize_feature_item_count(
    const struct route_state *state)
{
    switch(state->route) {
    case DIY_ROUTE_MENU:
        return CRAZYPOD_CUSTOMIZE_MENU_COUNT;
    case DIY_ROUTE_PRESETS:
        return 3;
    case DIY_ROUTE_PRESET_LIBRARY:
        return crazypod_preset_count();
    case DIY_ROUTE_PRESET_ACTIONS: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);

        return preset != NULL && preset->builtin ? 2 : 3;
    }
    case DIY_ROUTE_PRESET_EDIT:
        return 3;
    case DIY_ROUTE_PRESET_RENAME:
        return 39;
    case DIY_ROUTE_ICONS:
        return CRAZYPOD_ICON_THEME_COUNT;
    case DIY_ROUTE_DETAILS:
        return CRAZYPOD_CUSTOMIZE_DETAIL_COUNT;
    case DIY_ROUTE_CHOICES:
        return crazypod_customize_choice_count(
            (enum crazypod_appearance_field)state->group);
    case DIY_ROUTE_BACKGROUNDS:
        return CRAZYPOD_CUSTOMIZE_BACKGROUND_COUNT;
    case DIY_ROUTE_BACKGROUND_CHOICES:
        return CRAZYPOD_APPEARANCE_COLOR_COUNT + 2;
    case DIY_ROUTE_WALLPAPER_FILES:
        return crazypod_photo_count();
    case DIY_ROUTE_WALLPAPER_CROP:
        return 0;
    case DIY_ROUTE_LAYOUT:
        return CRAZYPOD_CUSTOMIZE_LAYOUT_COUNT;
    default:
        return 0;
    }
}

const char *crazypod_customize_feature_title(
    const struct route_state *state)
{
    switch(state->route) {
    case DIY_ROUTE_MENU:
        return "CUSTOMIZE";
    case DIY_ROUTE_PRESETS:
        return "PRESETS";
    case DIY_ROUTE_PRESET_LIBRARY:
        return "SAVED";
    case DIY_ROUTE_PRESET_ACTIONS: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);

        return preset != NULL ? preset->name : "PRESET";
    }
    case DIY_ROUTE_PRESET_EDIT:
        return "EDIT";
    case DIY_ROUTE_PRESET_RENAME:
        return "RENAME";
    case DIY_ROUTE_ICONS:
        return "ICONS";
    case DIY_ROUTE_DETAILS:
        return "DETAILS";
    case DIY_ROUTE_CHOICES:
        return crazypod_customize_field_title(
            (enum crazypod_appearance_field)state->group);
    case DIY_ROUTE_BACKGROUNDS:
        return "BACKGROUNDS";
    case DIY_ROUTE_BACKGROUND_CHOICES:
        return crazypod_customize_background_title(
            (enum crazypod_appearance_field)state->group);
    case DIY_ROUTE_WALLPAPER_FILES:
        if(state->group == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
            return "MENU PICTURE";
        if(state->group == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
            return "LOCK PICTURE";
        return "HOME PICTURE";
    case DIY_ROUTE_WALLPAPER_CROP:
        if(crazypod_wallpaper_crop_controller_model()->target ==
           CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
            return "CROP MENU";
        if(crazypod_wallpaper_crop_controller_model()->target ==
           CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
            return "CROP LOCK";
        return "CROP HOME";
    case DIY_ROUTE_LAYOUT:
        return "LAYOUT";
    default:
        return "";
    }
}

bool crazypod_customize_feature_item_is_current(
    const struct route_state *state, int index)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();

    switch(state->route) {
    case DIY_ROUTE_ICONS:
        return index == appearance->icon_theme;
    case DIY_ROUTE_CHOICES: {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)state->group;

        return crazypod_customize_choice_value(field, index) ==
            crazypod_customize_field_value(field);
    }
    case DIY_ROUTE_BACKGROUND_CHOICES: {
        const char *path = crazypod_customize_background_wallpaper(
            appearance,
            (enum crazypod_appearance_field)state->group);

        if(index == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1)
            return path[0] != '\0';
        return path[0] == '\0' &&
            index == crazypod_customize_field_value(
                (enum crazypod_appearance_field)state->group);
    }
    case DIY_ROUTE_WALLPAPER_FILES: {
        const char *path = crazypod_customize_background_wallpaper(
            appearance,
            (enum crazypod_appearance_field)state->group);

        return index >= 0 && index < crazypod_photo_count() &&
            strcmp(path, crazypod_photo_path(index)) == 0;
    }
    default:
        return false;
    }
}

bool crazypod_customize_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    switch(state->route) {
    case DIY_ROUTE_MENU:
        *title = index >= 0 && index < CRAZYPOD_CUSTOMIZE_MENU_COUNT
            ? crazypod_customize_menu_titles[index] : "";
        return true;
    case DIY_ROUTE_PRESETS:
        *title = index == 0 ? "Save" :
            index == 1 ? "Saved" :
            index == 2 ? "Import" : "";
        return true;
    case DIY_ROUTE_PRESET_LIBRARY: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(index);

        *title = preset != NULL ? preset->name : "";
        return true;
    }
    case DIY_ROUTE_PRESET_ACTIONS: {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);

        if(preset != NULL && preset->builtin)
            *title = index == 0 ? "Apply" :
                index == 1 ? "Export" : "";
        else
            *title = index >= 0 &&
                index < CRAZYPOD_CUSTOMIZE_PRESET_ACTION_COUNT
                    ? crazypod_customize_preset_actions[index] : "";
        return true;
    }
    case DIY_ROUTE_PRESET_EDIT:
        *title = index >= 0 &&
            index < CRAZYPOD_CUSTOMIZE_PRESET_EDIT_COUNT
                ? crazypod_customize_preset_edit_actions[index] : "";
        return true;
    case DIY_ROUTE_PRESET_RENAME:
        *title = editor_title(index);
        return true;
    case DIY_ROUTE_ICONS:
        *title = crazypod_icon_theme_name(index);
        return true;
    case DIY_ROUTE_DETAILS:
        *title = index >= 0 && index < CRAZYPOD_CUSTOMIZE_DETAIL_COUNT
            ? crazypod_customize_detail_titles[index] : "";
        return true;
    case DIY_ROUTE_CHOICES:
        *title = index >= 0 &&
            index < crazypod_customize_choice_count(
                (enum crazypod_appearance_field)state->group)
                    ? crazypod_customize_choice_title(
                        (enum crazypod_appearance_field)state->group,
                        index) : "";
        return true;
    case DIY_ROUTE_BACKGROUNDS:
        *title = index >= 0 &&
            index < CRAZYPOD_CUSTOMIZE_BACKGROUND_COUNT
                ? crazypod_customize_background_titles[index] : "";
        return true;
    case DIY_ROUTE_BACKGROUND_CHOICES:
        if(index == 0)
            *title = state->group ==
                CRAZYPOD_APPEARANCE_LOCK_BACKGROUND
                    ? "Follow Home" : "Default";
        else if(index <= CRAZYPOD_APPEARANCE_COLOR_COUNT)
            *title = crazypod_appearance_color_name(index - 1);
        else
            *title = index == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
                ? "Choose Picture" : "";
        return true;
    case DIY_ROUTE_WALLPAPER_FILES:
        *title = crazypod_photo_name(index);
        return true;
    case DIY_ROUTE_WALLPAPER_CROP:
        *title = "";
        return true;
    case DIY_ROUTE_LAYOUT:
        *title = index >= 0 && index < CRAZYPOD_CUSTOMIZE_LAYOUT_COUNT
            ? crazypod_customize_layout_titles[index] : "";
        return true;
    default:
        return false;
    }
}

void crazypod_customize_feature_initialize_media(void)
{
    photo_generation_seen = crazypod_photo_generation();
    photo_view_generation_seen =
        crazypod_photo_view_generation();
}

enum crazypod_feature_media_update
crazypod_customize_feature_poll_media(
    enum crazypod_route route, bool blocked)
{
    unsigned generation;

    if(blocked)
        return CRAZYPOD_FEATURE_MEDIA_NONE;
    if(route == DIY_ROUTE_WALLPAPER_CROP) {
        generation = crazypod_photo_view_generation();
        if(generation == photo_view_generation_seen)
            return CRAZYPOD_FEATURE_MEDIA_NONE;
        photo_view_generation_seen = generation;
        return CRAZYPOD_FEATURE_MEDIA_ROUTE;
    }
    if(route == DIY_ROUTE_WALLPAPER_FILES) {
        generation = crazypod_photo_generation();
        if(generation == photo_generation_seen)
            return CRAZYPOD_FEATURE_MEDIA_NONE;
        photo_generation_seen = generation;
        return CRAZYPOD_FEATURE_MEDIA_ROUTE;
    }
    return CRAZYPOD_FEATURE_MEDIA_NONE;
}

bool crazypod_customize_feature_activate(
    const struct route_state *state,
    const struct crazypod_customize_activation_host *host)
{
    const struct crazypod_customize_command_result command =
        crazypod_customize_controller_select(
            state->route, state->selected, state->group);

    if(crazypod_customize_controller_handles(state->route)) {
        if(command.action == CRAZYPOD_CUSTOMIZE_COMMAND_RENDER)
            host->render(false);
        else if(command.action ==
                CRAZYPOD_CUSTOMIZE_COMMAND_APPEARANCE_CHANGED)
            host->appearance_changed();
        else if(command.action ==
                CRAZYPOD_CUSTOMIZE_COMMAND_PUSH_ROUTE)
            host->push_selected(
                command.route, command.group, command.selected);
        else if(command.action ==
                CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_ICON_CHOICES)
            host->show_icon_choices(
                command.group, command.selected);
        else if(command.action ==
                CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_APPEARANCE_CHOICES)
            host->show_appearance_choices(
                command.group, command.selected);
        else if(command.action ==
                CRAZYPOD_CUSTOMIZE_COMMAND_SHOW_BACKGROUND_CHOICES)
            host->show_background_choices(
                command.group, command.selected);
        return true;
    }

    {
        const char *character =
            state->selected >= 0 &&
            state->selected < EDITOR_CHARACTER_COUNT
                ? editor_characters[state->selected] : NULL;
        const struct crazypod_preset_command_result preset =
            crazypod_preset_controller_select(
                state->route, state->selected,
                state->group, character);

        if(!crazypod_preset_controller_handles(state->route))
            return false;
        switch(preset.action) {
        case CRAZYPOD_PRESET_COMMAND_PUSH_ACTIONS:
            host->push(
                DIY_ROUTE_PRESET_ACTIONS, preset.preset_index);
            break;
        case CRAZYPOD_PRESET_COMMAND_PUSH_LIBRARY:
            host->push(DIY_ROUTE_PRESET_LIBRARY, -1);
            break;
        case CRAZYPOD_PRESET_COMMAND_PUSH_EDIT:
            host->push(
                DIY_ROUTE_PRESET_EDIT, preset.preset_index);
            break;
        case CRAZYPOD_PRESET_COMMAND_PUSH_RENAME:
            host->push(
                DIY_ROUTE_PRESET_RENAME, preset.preset_index);
            break;
        case CRAZYPOD_PRESET_COMMAND_POP:
            host->pop();
            break;
        case CRAZYPOD_PRESET_COMMAND_APPLIED:
            host->appearance_changed();
            break;
        case CRAZYPOD_PRESET_COMMAND_DELETED:
            host->preset_deleted();
            break;
        case CRAZYPOD_PRESET_COMMAND_RENDER:
            host->render(false);
            break;
        case CRAZYPOD_PRESET_COMMAND_NONE:
        default:
            break;
        }
    }
    return true;
}

bool crazypod_customize_feature_render(
    const struct route_state *state, lv_obj_t *parent,
    const lv_font_t *metadata_font, uint32_t primary_color,
    uint32_t panel_color, uint32_t foreground_color,
    uint32_t accent_color)
{
    if(state->route == DIY_ROUTE_WALLPAPER_FILES) {
        crazypod_photos_feature_render_wallpaper_grid(
            parent,
            state->selected,
            crazypod_customize_feature_title(state),
            metadata_font, primary_color, panel_color);
        return true;
    }
    if(state->route != DIY_ROUTE_WALLPAPER_CROP)
        return false;
    crazypod_wallpaper_crop_screen_render(
        parent, foreground_color, accent_color);
    return true;
}

void crazypod_customize_feature_reset_view(void)
{
    crazypod_wallpaper_crop_screen_reset();
}

#endif
