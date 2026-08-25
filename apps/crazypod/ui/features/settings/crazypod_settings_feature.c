#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "button.h"

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
    case SETTINGS_ROUTE_DATE_TIME:
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
        return CP_TR("SETTINGS");
    case SETTINGS_ROUTE_SOUND:
        return CP_TR("SOUND");
    case SETTINGS_ROUTE_EQ_STUDIO:
        return CP_TR("EQ STUDIO");
    case SETTINGS_ROUTE_DISPLAY:
        return CP_TR("DISPLAY");
    case SETTINGS_ROUTE_DATE_TIME:
        return CP_TR("DATE & TIME");
    case SETTINGS_ROUTE_PLAYBACK:
        return CP_TR("PLAYBACK");
    case SETTINGS_ROUTE_POWER:
        return CP_TR("POWER");
    case SETTINGS_ROUTE_CONTROLS:
        return CP_TR("CONTROLS");
    case SETTINGS_ROUTE_MAIN_MENU:
        return CP_TR("MAIN MENU");
    case SETTINGS_ROUTE_MAIN_MENU_ACTIONS: {
        const struct crazypod_app_descriptor *app =
            crazypod_app_catalog_find(
                (enum crazypod_app_id)state->group);

        return app != NULL ? app->name : CP_TR("MAIN MENU");
    }
    default:
        return "";
    }
}

bool crazypod_settings_feature_item_is_current(
    const struct route_state *state, int index)
{
    if(state->route == SETTINGS_ROUTE_MAIN_MENU &&
       crazypod_settings_main_menu_reordering())
        return false;
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
                (enum crazypod_app_id)state->group) ? CP_TR("Hide") : CP_TR("Show");
        else {
            index += fixed ? 1 : 0;
            *title = index == 1 ? CP_TR("Up") :
                index == 2 ? CP_TR("Down") : "";
        }
        return true;
    }
    case SETTINGS_ROUTE_EQ_STUDIO: {
        static const char *const labels[] = {
            CP_TR("32Hz"), CP_TR("64Hz"), CP_TR("125Hz"), CP_TR("250Hz"), CP_TR("500Hz"),
            CP_TR("1kHz"), CP_TR("2kHz"), CP_TR("4kHz"), CP_TR("8kHz"), CP_TR("16kHz")
        };

        *title = index >= 0 && index < 10 ? labels[index] : "";
        return true;
    }
    case SETTINGS_ROUTE_SOUND:
    case SETTINGS_ROUTE_DISPLAY:
    case SETTINGS_ROUTE_DATE_TIME:
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

static enum crazypod_menu_icon settings_item_icon(int item)
{
    switch(item) {
    case SETTINGS_ITEM_LANGUAGE:
        return CRAZYPOD_MENU_ICON_LANGUAGE;
    case SETTINGS_ITEM_EQ_ENABLED:
        return CRAZYPOD_MENU_ICON_EQUALIZER;
    case SETTINGS_ITEM_BASS:
        return CRAZYPOD_MENU_ICON_BASS;
    case SETTINGS_ITEM_TREBLE:
        return CRAZYPOD_MENU_ICON_TREBLE;
    case SETTINGS_ITEM_BALANCE:
        return CRAZYPOD_MENU_ICON_BALANCE;
    case SETTINGS_ITEM_BRIGHTNESS:
        return CRAZYPOD_MENU_ICON_BRIGHTNESS;
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
        return CRAZYPOD_MENU_ICON_BACKLIGHT;
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
        return CRAZYPOD_MENU_ICON_CHARGING;
    case SETTINGS_ITEM_LCD_SLEEP:
        return CRAZYPOD_MENU_ICON_DISPLAY_SLEEP;
    case SETTINGS_ITEM_REDUCE_MOTION:
        return CRAZYPOD_MENU_ICON_MOTION_OFF;
    case SETTINGS_ITEM_DATE_YEAR:
    case SETTINGS_ITEM_DATE_MONTH:
    case SETTINGS_ITEM_DATE_DAY:
        return CRAZYPOD_MENU_ICON_DATE;
    case SETTINGS_ITEM_TIME_HOUR:
    case SETTINGS_ITEM_TIME_MINUTE:
    case SETTINGS_ITEM_TIME_SECOND:
        return CRAZYPOD_MENU_ICON_TIME;
    case SETTINGS_ITEM_SHUFFLE:
        return CRAZYPOD_MENU_ICON_SHUFFLE;
    case SETTINGS_ITEM_REPEAT:
        return CRAZYPOD_MENU_ICON_REPEAT;
    case SETTINGS_ITEM_ORIGINAL_IPOD_MUSIC:
        return CRAZYPOD_MENU_ICON_MUSIC_LIBRARY;
    case SETTINGS_ITEM_IDLE_POWEROFF:
        return CRAZYPOD_MENU_ICON_POWER_TIMER;
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return CRAZYPOD_MENU_ICON_SLEEP_TIMER;
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
        return CRAZYPOD_MENU_ICON_TIMER_BOOT;
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
        return CRAZYPOD_MENU_ICON_RESET_TIMER;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        return CRAZYPOD_MENU_ICON_USB;
#endif
#ifdef HAVE_DISK_STORAGE
    case SETTINGS_ITEM_STORAGE_MODE:
        return CRAZYPOD_MENU_ICON_STORAGE;
#endif
    case SETTINGS_ITEM_BEEP:
        return CRAZYPOD_MENU_ICON_BEEP;
    case SETTINGS_ITEM_KEYCLICK:
        return CRAZYPOD_MENU_ICON_KEYCLICK;
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
        return CRAZYPOD_MENU_ICON_SPEAKER;
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return CRAZYPOD_MENU_ICON_REPEAT_CLICKS;
    default:
        return CRAZYPOD_MENU_ICON_NONE;
    }
}

enum crazypod_menu_icon crazypod_settings_feature_item_icon(
    const struct route_state *state, int index)
{
    static const enum crazypod_menu_icon root_icons[] = {
        CRAZYPOD_MENU_ICON_SOUND,
        CRAZYPOD_MENU_ICON_DISPLAY,
        CRAZYPOD_MENU_ICON_DATE,
        CRAZYPOD_MENU_ICON_PLAYBACK,
        CRAZYPOD_MENU_ICON_POWER,
        CRAZYPOD_MENU_ICON_CONTROLS,
        CRAZYPOD_MENU_ICON_MENU,
        CRAZYPOD_MENU_ICON_LANGUAGE,
    };

    if(index < 0)
        return CRAZYPOD_MENU_ICON_NONE;
    switch(state->route) {
    case SETTINGS_ROUTE_MENU:
        return index < (int)(sizeof(root_icons) / sizeof(root_icons[0]))
            ? root_icons[index] : CRAZYPOD_MENU_ICON_NONE;
    case SETTINGS_ROUTE_SOUND:
    case SETTINGS_ROUTE_DISPLAY:
    case SETTINGS_ROUTE_DATE_TIME:
    case SETTINGS_ROUTE_PLAYBACK:
    case SETTINGS_ROUTE_POWER:
    case SETTINGS_ROUTE_CONTROLS:
        return settings_item_icon(
            crazypod_settings_catalog_item(state->route, index));
    case SETTINGS_ROUTE_EQ_STUDIO:
        return CRAZYPOD_MENU_ICON_EQUALIZER;
    case SETTINGS_ROUTE_MAIN_MENU: {
        const struct crazypod_app_descriptor *app =
            crazypod_app_catalog_find(crazypod_apps_ordered_id(index));

        return app != NULL ? app->menu_icon : CRAZYPOD_MENU_ICON_NONE;
    }
    case SETTINGS_ROUTE_MAIN_MENU_ACTIONS:
        if(!crazypod_apps_is_fixed(
               (enum crazypod_app_id)state->group)) {
            return index == 0 ? CRAZYPOD_MENU_ICON_VISIBILITY :
                index == 1 ? CRAZYPOD_MENU_ICON_MOVE_UP :
                index == 2 ? CRAZYPOD_MENU_ICON_MOVE_DOWN :
                CRAZYPOD_MENU_ICON_NONE;
        }
        return index == 0 ? CRAZYPOD_MENU_ICON_MOVE_UP :
            index == 1 ? CRAZYPOD_MENU_ICON_MOVE_DOWN :
            CRAZYPOD_MENU_ICON_NONE;
    default:
        return CRAZYPOD_MENU_ICON_NONE;
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
    else if(command.kind ==
            CRAZYPOD_SETTINGS_COMMAND_SHOW_MAIN_MENU_ACTIONS) {
        host->show_main_menu_actions(command.app_id);
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

    if(state->route == SETTINGS_ROUTE_MAIN_MENU &&
       crazypod_settings_main_menu_reordering()) {
        if(event->base == BUTTON_SCROLL_FWD ||
           event->base == BUTTON_RIGHT)
            context->move(1);
        else if(event->base == BUTTON_SCROLL_BACK ||
                event->base == BUTTON_LEFT)
            context->move(-1);
        else if((event->base == BUTTON_SELECT ||
                 event->base == BUTTON_MENU) &&
                !event->repeated)
            context->activate();
        return true;
    }
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

bool crazypod_settings_feature_apply_choice(
    int item, int index)
{
    return crazypod_ui_settings_apply_choice(item, index);
}

void crazypod_settings_feature_begin_main_menu_reorder(
    enum crazypod_app_id id)
{
    crazypod_settings_begin_main_menu_reorder(id);
}

bool crazypod_settings_feature_main_menu_reordering(void)
{
    return crazypod_settings_main_menu_reordering();
}

enum crazypod_app_id
crazypod_settings_feature_main_menu_reorder_id(void)
{
    return crazypod_settings_main_menu_reorder_id();
}

bool crazypod_settings_feature_move_main_menu_item(int direction)
{
    return crazypod_settings_move_main_menu_item(direction);
}

enum crazypod_app_id
crazypod_settings_feature_finish_main_menu_reorder(void)
{
    return crazypod_settings_finish_main_menu_reorder();
}

#endif
