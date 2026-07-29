#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_apps.h"
#include "crazypod_settings_model.h"
#include "crazypod_eq_studio_controller.h"
#include "crazypod_eq_studio_input.h"
#include "crazypod_eq_studio_screen.h"
#include "crazypod_settings_controller.h"
#include "../../shell/crazypod_app_catalog.h"
#include "crazypod_settings_catalog.h"
#include "crazypod_settings_feature.h"

int crazypod_settings_feature_item_count(
    const struct route_state *state)
{
    switch(state->route) {
    case SETTINGS_ROUTE_MENU:
    case SETTINGS_ROUTE_SOUND:
    case SETTINGS_ROUTE_EQ_STUDIO:
    case SETTINGS_ROUTE_DISPLAY:
    case SETTINGS_ROUTE_PLAYBACK:
    case SETTINGS_ROUTE_POWER:
    case SETTINGS_ROUTE_CONTROLS:
    case SETTINGS_ROUTE_MAIN_MENU:
        return crazypod_settings_catalog_count(state->route);
    case SETTINGS_ROUTE_MAIN_MENU_ACTIONS:
        return crazypod_apps_is_fixed(
            (enum crazypod_app_id)state->group) ? 2 : 3;
    default:
        return 0;
    }
}

const char *crazypod_settings_feature_title(
    const struct route_state *state)
{
    switch(state->route) {
    case SETTINGS_ROUTE_MENU:
        return "SETTINGS";
    case SETTINGS_ROUTE_SOUND:
        return "SOUND";
    case SETTINGS_ROUTE_EQ_STUDIO:
        return "EQ STUDIO";
    case SETTINGS_ROUTE_DISPLAY:
        return "DISPLAY";
    case SETTINGS_ROUTE_PLAYBACK:
        return "PLAYBACK";
    case SETTINGS_ROUTE_POWER:
        return "POWER";
    case SETTINGS_ROUTE_CONTROLS:
        return "CONTROLS";
    case SETTINGS_ROUTE_MAIN_MENU:
        return "MAIN MENU";
    case SETTINGS_ROUTE_MAIN_MENU_ACTIONS: {
        const struct crazypod_app_descriptor *app =
            crazypod_app_catalog_find(
                (enum crazypod_app_id)state->group);

        return app != NULL ? app->name : "MAIN MENU";
    }
    default:
        return "";
    }
}

bool crazypod_settings_feature_item_is_current(
    const struct route_state *state, int index)
{
    return state->route == SETTINGS_ROUTE_MAIN_MENU &&
        crazypod_apps_is_enabled(crazypod_apps_ordered_id(index));
}

bool crazypod_settings_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    switch(state->route) {
    case SETTINGS_ROUTE_MENU:
        *title = index >= 0 &&
            index < CRAZYPOD_SETTINGS_MENU_COUNT
                ? crazypod_settings_menu_titles[index] : "";
        return true;
    case SETTINGS_ROUTE_MAIN_MENU: {
        const struct crazypod_app_descriptor *app =
            crazypod_app_catalog_find(crazypod_apps_ordered_id(index));

        *title = app != NULL ? app->name : "";
        return true;
    }
    case SETTINGS_ROUTE_MAIN_MENU_ACTIONS: {
        bool fixed = crazypod_apps_is_fixed(
            (enum crazypod_app_id)state->group);

        if(index == 0 && !fixed)
            *title = crazypod_apps_is_enabled(
                (enum crazypod_app_id)state->group) ? "Hide" : "Show";
        else {
            index += fixed ? 1 : 0;
            *title = index == 1 ? "Up" :
                index == 2 ? "Down" : "";
        }
        return true;
    }
    case SETTINGS_ROUTE_EQ_STUDIO: {
        static const char *const labels[] = {
            "32Hz", "64Hz", "125Hz", "250Hz", "500Hz",
            "1kHz", "2kHz", "4kHz", "8kHz", "16kHz"
        };

        *title = index >= 0 && index < 10 ? labels[index] : "";
        return true;
    }
    case SETTINGS_ROUTE_SOUND:
    case SETTINGS_ROUTE_DISPLAY:
    case SETTINGS_ROUTE_PLAYBACK:
    case SETTINGS_ROUTE_POWER:
    case SETTINGS_ROUTE_CONTROLS:
        *title = crazypod_ui_settings_item_title(
            crazypod_settings_catalog_item(state->route, index));
        return true;
    default:
        return false;
    }
}

bool crazypod_settings_feature_activate(
    const struct route_state *state,
    const struct crazypod_settings_activation_host *host)
{
    struct crazypod_settings_command command;
    enum crazypod_app_id preferred;

    if(!crazypod_settings_catalog_handles(state->route) ||
       state->route == SETTINGS_ROUTE_EQ_STUDIO)
        return false;
    preferred = host->selected_app();
    command = crazypod_settings_activate(state);
    if(command.kind == CRAZYPOD_SETTINGS_COMMAND_PUSH_ROUTE) {
        host->push(
            command.route,
            command.route == SETTINGS_ROUTE_MAIN_MENU_ACTIONS
                ? (int)command.app_id : -1);
    }
    else if(command.kind ==
            CRAZYPOD_SETTINGS_COMMAND_MAIN_MENU_CHANGED) {
        host->main_menu_changed(preferred, command.app_id);
        host->render(false);
    }
    else if(command.kind == CRAZYPOD_SETTINGS_COMMAND_OPEN_EQ) {
        crazypod_eq_studio_open();
        host->push(SETTINGS_ROUTE_EQ_STUDIO, -1);
    }
    else if(command.kind ==
            CRAZYPOD_SETTINGS_COMMAND_SHOW_CHOICES) {
        host->show_choices(
            command.item,
            crazypod_ui_settings_choice_index(command.item));
    }
    else if(state->route == SETTINGS_ROUTE_MAIN_MENU_ACTIONS)
        host->render(false);
    return true;
}

bool crazypod_settings_feature_render(
    const struct route_state *state, lv_obj_t *parent,
    const lv_font_t *metadata_font, uint32_t primary_color)
{
    struct crazypod_eq_studio_model model;

    if(state->route != SETTINGS_ROUTE_EQ_STUDIO)
        return false;
    model = crazypod_eq_studio_model();
    crazypod_eq_studio_screen_render(
        parent, &model, metadata_font, primary_color);
    return true;
}

static struct crazypod_feature_input_context settings_input_context;

static void settings_input_render(void)
{
    settings_input_context.render(false);
}

bool crazypod_settings_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context)
{
    const struct crazypod_eq_studio_input_actions actions = {
        .render = settings_input_render,
        .leave = context->pop,
    };

    if(state->route != SETTINGS_ROUTE_EQ_STUDIO)
        return false;
    settings_input_context = *context;
    crazypod_eq_studio_input_handle(event, &actions);
    return true;
}

const char *crazypod_settings_feature_menu_symbol(int index)
{
    return crazypod_settings_menu_symbols[index];
}

int crazypod_settings_feature_choice_count(int item)
{
    return crazypod_ui_settings_choice_count(item);
}

int crazypod_settings_feature_choice_index(int item)
{
    return crazypod_ui_settings_choice_index(item);
}

const char *crazypod_settings_feature_choice_item_title(int item)
{
    return crazypod_ui_settings_item_title(item);
}

const char *crazypod_settings_feature_choice_title(
    int item, int index)
{
    return crazypod_ui_settings_choice_title(
        item, index);
}

void crazypod_settings_feature_apply_choice(
    int item, int index)
{
    crazypod_ui_settings_apply_choice(item, index);
}

#endif
