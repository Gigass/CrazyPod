#ifndef CRAZYPOD_UI_SETTINGS_MODEL_H
#define CRAZYPOD_UI_SETTINGS_MODEL_H

#include <stdbool.h>

#include "config.h"

enum settings_item {
    SETTINGS_ITEM_LANGUAGE,
    SETTINGS_ITEM_EQ_ENABLED,
    SETTINGS_ITEM_BASS,
    SETTINGS_ITEM_TREBLE,
    SETTINGS_ITEM_BALANCE,
    SETTINGS_ITEM_BRIGHTNESS,
    SETTINGS_ITEM_BACKLIGHT_TIMEOUT,
    SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED,
    SETTINGS_ITEM_LCD_SLEEP,
    SETTINGS_ITEM_REDUCE_MOTION,
    SETTINGS_ITEM_DATE_YEAR,
    SETTINGS_ITEM_DATE_MONTH,
    SETTINGS_ITEM_DATE_DAY,
    SETTINGS_ITEM_TIME_HOUR,
    SETTINGS_ITEM_TIME_MINUTE,
    SETTINGS_ITEM_TIME_SECOND,
    SETTINGS_ITEM_SHUFFLE,
    SETTINGS_ITEM_REPEAT,
    SETTINGS_ITEM_ORIGINAL_IPOD_MUSIC,
    SETTINGS_ITEM_IDLE_POWEROFF,
    SETTINGS_ITEM_SLEEP_TIMER_DURATION,
    SETTINGS_ITEM_SLEEP_TIMER_STARTUP,
    SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS,
#ifdef HAVE_USB_CHARGING_ENABLE
    SETTINGS_ITEM_USB_CHARGING,
#endif
#ifdef HAVE_DISK_STORAGE
    SETTINGS_ITEM_STORAGE_MODE,
#endif
    SETTINGS_ITEM_BEEP,
    SETTINGS_ITEM_KEYCLICK,
#ifdef HAVE_HARDWARE_CLICK
    SETTINGS_ITEM_SPEAKER_CLICK,
#endif
    SETTINGS_ITEM_KEYCLICK_REPEATS,
    SETTINGS_ITEM_COUNT,
};

const char *crazypod_ui_settings_item_title(int item);
const char *crazypod_ui_settings_item_symbol(int item);
const char *crazypod_ui_settings_group_detail(int index);
int crazypod_ui_settings_choice_count(int item);
int crazypod_ui_settings_choice_index(int item);
const char *crazypod_ui_settings_choice_title(int item, int index);
const char *crazypod_ui_settings_item_value_label(int item);
bool crazypod_ui_settings_apply_choice(int item, int index);

#endif
