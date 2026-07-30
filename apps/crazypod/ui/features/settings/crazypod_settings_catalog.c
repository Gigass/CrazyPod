#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "lvgl.h"
#include "settings.h"

#include "../../../crazypod_apps.h"
#include "crazypod_settings_model.h"
#include "crazypod_settings_catalog.h"

const char *const crazypod_settings_menu_titles[] = {
    CP_TR("Sound"), CP_TR("Display"), CP_TR("Playback"), CP_TR("Power"), CP_TR("Controls"), CP_TR("Main Menu"),
    CP_TR("Language")
};

const char *const crazypod_settings_menu_symbols[] = {
    LV_SYMBOL_AUDIO, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_PLAY,
    LV_SYMBOL_POWER, LV_SYMBOL_SETTINGS, LV_SYMBOL_LIST,
    LV_SYMBOL_HOME
};

static const int sound_items[] = {
    SETTINGS_ITEM_EQ_ENABLED,
    SETTINGS_ITEM_BASS,
    SETTINGS_ITEM_TREBLE,
    SETTINGS_ITEM_BALANCE,
};

static const int display_items[] = {
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    SETTINGS_ITEM_BRIGHTNESS,
#endif
    SETTINGS_ITEM_BACKLIGHT_TIMEOUT,
#if CONFIG_CHARGING
    SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED,
#endif
    SETTINGS_ITEM_LCD_SLEEP,
    SETTINGS_ITEM_REDUCE_MOTION,
};

static const int playback_items[] = {
    SETTINGS_ITEM_SHUFFLE,
    SETTINGS_ITEM_REPEAT,
};

static const int power_items[] = {
#ifdef HAVE_USB_CHARGING_ENABLE
    SETTINGS_ITEM_USB_CHARGING,
#endif
#ifdef HAVE_DISK_STORAGE
    SETTINGS_ITEM_STORAGE_MODE,
#endif
    SETTINGS_ITEM_SLEEP_TIMER_DURATION,
    SETTINGS_ITEM_SLEEP_TIMER_STARTUP,
    SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS,
};

static const int controls_items[] = {
    SETTINGS_ITEM_BEEP,
    SETTINGS_ITEM_KEYCLICK,
#ifdef HAVE_HARDWARE_CLICK
    SETTINGS_ITEM_SPEAKER_CLICK,
#endif
    SETTINGS_ITEM_KEYCLICK_REPEATS,
};

#define ARRAY_COUNT(values) ((int)(sizeof(values) / sizeof((values)[0])))

bool crazypod_settings_catalog_handles(enum crazypod_route route)
{
    return route >= SETTINGS_ROUTE_MENU &&
        route <= SETTINGS_ROUTE_MAIN_MENU_ACTIONS;
}

int crazypod_settings_catalog_count(enum crazypod_route route)
{
    switch(route) {
    case SETTINGS_ROUTE_MENU:
        return CRAZYPOD_SETTINGS_MENU_COUNT;
    case SETTINGS_ROUTE_SOUND:
        return ARRAY_COUNT(sound_items);
    case SETTINGS_ROUTE_EQ_STUDIO:
        return EQ_NUM_BANDS;
    case SETTINGS_ROUTE_DISPLAY:
        return ARRAY_COUNT(display_items);
    case SETTINGS_ROUTE_PLAYBACK:
        return ARRAY_COUNT(playback_items);
    case SETTINGS_ROUTE_POWER:
        return ARRAY_COUNT(power_items);
    case SETTINGS_ROUTE_CONTROLS:
        return ARRAY_COUNT(controls_items);
    case SETTINGS_ROUTE_MAIN_MENU:
        return crazypod_apps_count();
    default:
        return 0;
    }
}

int crazypod_settings_catalog_item(
    enum crazypod_route route, int index)
{
    const int *items = NULL;
    int count = 0;

    switch(route) {
    case SETTINGS_ROUTE_SOUND:
        items = sound_items;
        count = ARRAY_COUNT(sound_items);
        break;
    case SETTINGS_ROUTE_DISPLAY:
        items = display_items;
        count = ARRAY_COUNT(display_items);
        break;
    case SETTINGS_ROUTE_PLAYBACK:
        items = playback_items;
        count = ARRAY_COUNT(playback_items);
        break;
    case SETTINGS_ROUTE_POWER:
        items = power_items;
        count = ARRAY_COUNT(power_items);
        break;
    case SETTINGS_ROUTE_CONTROLS:
        items = controls_items;
        count = ARRAY_COUNT(controls_items);
        break;
    default:
        break;
    }
    return items != NULL && index >= 0 && index < count
        ? items[index] : -1;
}

#endif
