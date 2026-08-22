#include "config.h"

#include <stdbool.h>
#include <stdio.h>

#include "backlight.h"
#include "lcd.h"
#include "powermgmt.h"
#include "settings.h"
#include "sound.h"
#include "storage.h"
#include "timefuncs.h"
#include "usb.h"

#include "lvgl.h"

#include "../../../crazypod_audio_shims.h"
#include "../../../crazypod_l10n.h"
#include "../../../crazypod_playlist.h"
#include "../../../crazypod_state.h"
#include "crazypod_settings_model.h"

static const int setting_timeout_values[] = {
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    15, 20, 25, 30, 45, 60, 90, 120, 180, 240, 300
};

static const int setting_lcd_sleep_values[] = {
    -1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    15, 20, 25, 30, 45, 60, 90, 120, 180, 240, 300
};

static const int setting_sleep_timer_values[] = {
    0, 5, 10, 15, 30, 45, 60, 90, 120, 180, 240, 300
};

static const int setting_idle_poweroff_values[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 30, 45, 60
};

static const int setting_repeat_values[] = {
    REPEAT_OFF, REPEAT_ALL, REPEAT_ONE
};

#define SETTINGS_RTC_YEAR_MIN 2000
#define SETTINGS_RTC_YEAR_MAX 2099

static bool settings_is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int settings_days_in_month(int year, int month)
{
    static const unsigned char days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if(month < 0 || month > 11)
        return 31;
    if(month == 1 && settings_is_leap_year(year))
        return 29;
    return days[month];
}

static struct tm settings_rtc_time(void)
{
    struct tm value = *get_time();

    if(!valid_time(&value)) {
        value.tm_sec = 0;
        value.tm_min = 0;
        value.tm_hour = 0;
        value.tm_mday = 1;
        value.tm_mon = 0;
        value.tm_year = SETTINGS_RTC_YEAR_MIN - 1900;
        set_day_of_week(&value);
        set_day_of_year(&value);
    }
    return value;
}

const char *crazypod_ui_settings_item_title(int item)
{
    switch(item) {
    case SETTINGS_ITEM_LANGUAGE: return CP_TR("Language");
    case SETTINGS_ITEM_EQ_ENABLED: return CP_TR("Equalizer");
    case SETTINGS_ITEM_BASS: return CP_TR("Bass");
    case SETTINGS_ITEM_TREBLE: return CP_TR("Treble");
    case SETTINGS_ITEM_BALANCE: return CP_TR("Balance");
    case SETTINGS_ITEM_BRIGHTNESS: return CP_TR("Brightness");
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT: return CP_TR("Backlight");
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED: return CP_TR("Charging Light");
    case SETTINGS_ITEM_LCD_SLEEP: return CP_TR("LCD Sleep");
    case SETTINGS_ITEM_REDUCE_MOTION: return CP_TR("Reduce Motion");
    case SETTINGS_ITEM_DATE_YEAR: return CP_TR("Year");
    case SETTINGS_ITEM_DATE_MONTH: return CP_TR("Month");
    case SETTINGS_ITEM_DATE_DAY: return CP_TR("Day");
    case SETTINGS_ITEM_TIME_HOUR: return CP_TR("Hour");
    case SETTINGS_ITEM_TIME_MINUTE: return CP_TR("Minute");
    case SETTINGS_ITEM_TIME_SECOND: return CP_TR("Second");
    case SETTINGS_ITEM_SHUFFLE: return CP_TR("Shuffle");
    case SETTINGS_ITEM_REPEAT: return CP_TR("Repeat");
    case SETTINGS_ITEM_IDLE_POWEROFF: return CP_TR("Idle Power Off");
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION: return CP_TR("Sleep Timer");
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP: return CP_TR("Timer on Boot");
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS: return CP_TR("Key Reset Timer");
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING: return CP_TR("USB Charging");
#endif
#ifdef HAVE_DISK_STORAGE
    case SETTINGS_ITEM_STORAGE_MODE: return CP_TR("Storage Type");
#endif
    case SETTINGS_ITEM_BEEP: return CP_TR("System Beep");
    case SETTINGS_ITEM_KEYCLICK: return CP_TR("Keyclick");
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK: return CP_TR("Speaker Click");
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS: return CP_TR("Repeat Clicks");
    default: return "";
    }
}

const char *crazypod_ui_settings_item_symbol(int item)
{
    switch(item) {
    case SETTINGS_ITEM_LANGUAGE:
        return LV_SYMBOL_HOME;
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_BASS:
    case SETTINGS_ITEM_TREBLE:
    case SETTINGS_ITEM_BALANCE:
        return LV_SYMBOL_AUDIO;
    case SETTINGS_ITEM_BRIGHTNESS:
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
    case SETTINGS_ITEM_LCD_SLEEP:
        return LV_SYMBOL_EYE_OPEN;
    case SETTINGS_ITEM_REDUCE_MOTION:
        return LV_SYMBOL_EYE_CLOSE;
    case SETTINGS_ITEM_DATE_YEAR:
    case SETTINGS_ITEM_DATE_MONTH:
    case SETTINGS_ITEM_DATE_DAY:
    case SETTINGS_ITEM_TIME_HOUR:
    case SETTINGS_ITEM_TIME_MINUTE:
    case SETTINGS_ITEM_TIME_SECOND:
        return LV_SYMBOL_SETTINGS;
    case SETTINGS_ITEM_SHUFFLE:
        return LV_SYMBOL_SHUFFLE;
    case SETTINGS_ITEM_REPEAT:
        return LV_SYMBOL_LOOP;
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
#endif
#ifdef HAVE_DISK_STORAGE
    case SETTINGS_ITEM_STORAGE_MODE:
#endif
        return LV_SYMBOL_POWER;
    default:
        return LV_SYMBOL_SETTINGS;
    }
}

const char *crazypod_ui_settings_group_detail(int index)
{
    static char date_time[40];
    char date[16];
    char time[16];
    struct tm now;

    switch(index) {
    case 0: return CP_TR("EQ, tone and balance");
    case 1: return CP_TR("Backlight, sleep and brightness");
    case 2:
        now = *get_time();
        if(!valid_time(&now))
            return CP_TR("Date & Time");
        snprintf(date, sizeof(date), CP_FMT("%04d-%02d-%02d"),
                 now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
        snprintf(time, sizeof(time), CP_FMT("%02d:%02d"),
                 now.tm_hour, now.tm_min);
        snprintf(date_time, sizeof(date_time), CP_FMT("%s · %s"), date, time);
        return date_time;
    case 3: return CP_TR("Shuffle and repeat");
    case 4: return CP_TR("Charging, storage and sleep");
    case 5: return CP_TR("Beeps and wheel feedback");
    case 6: return CP_TR("Reorder or hide entries");
    case 7:
        return crazypod_language_native_name(crazypod_language_current());
    default: return "";
    }
}

static const char *format_bool_value(bool value)
{
    return value ? CP_TR("On") : CP_TR("Off");
}

static const char *format_timeout_value(int value)
{
    static char text[20];

    if(value < 0)
        return CP_TR("Never");
    if(value == 0)
        return CP_TR("Always");
    if(value >= 60 && value % 60 == 0)
        snprintf(text, sizeof(text), CP_FMT("%d min"), value / 60);
    else
        snprintf(text, sizeof(text), CP_FMT("%d sec"), value);
    return text;
}

static const char *format_sleep_timer_value(int value)
{
    static char text[20];

    if(value <= 0)
        return CP_TR("Off");
    if(value >= 60 && value % 60 == 0)
        snprintf(text, sizeof(text), CP_FMT("%d hr"), value / 60);
    else
        snprintf(text, sizeof(text), CP_FMT("%d min"), value);
    return text;
}

static int range_choice_count(int minimum, int maximum, int step)
{
    if(step <= 0)
        step = 1;
    if(maximum < minimum)
        return 0;
    return (maximum - minimum) / step + 1;
}

static int range_choice_index(int value, int minimum, int maximum, int step)
{
    int index;
    int count = range_choice_count(minimum, maximum, step);

    if(count <= 0)
        return 0;
    if(value < minimum)
        value = minimum;
    if(value > maximum)
        value = maximum;
    if(step <= 0)
        step = 1;
    index = (value - minimum + step / 2) / step;
    if(index < 0)
        index = 0;
    if(index >= count)
        index = count - 1;
    return index;
}

static int range_choice_value(int index, int minimum, int maximum, int step)
{
    int value;

    if(step <= 0)
        step = 1;
    value = minimum + index * step;
    if(value > maximum)
        value = maximum;
    return value;
}

static int setting_sound_step(int setting)
{
    int step = sound_steps(setting);

    return step > 0 ? step : 1;
}

static int settings_item_current_value(int item)
{
    switch(item) {
    case SETTINGS_ITEM_LANGUAGE:
        return crazypod_language_current();
    case SETTINGS_ITEM_EQ_ENABLED:
        return global_settings.eq_enabled ? 1 : 0;
    case SETTINGS_ITEM_BASS:
        return global_settings.bass;
    case SETTINGS_ITEM_TREBLE:
        return global_settings.treble;
    case SETTINGS_ITEM_BALANCE:
        return global_settings.balance;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        return global_settings.brightness;
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
        return global_settings.backlight_timeout;
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
        return global_settings.backlight_timeout_plugged;
#endif
    case SETTINGS_ITEM_LCD_SLEEP:
        return global_settings.lcd_sleep_after_backlight_off;
    case SETTINGS_ITEM_REDUCE_MOTION:
        return crazypod_state_reduce_motion() ? 1 : 0;
    case SETTINGS_ITEM_DATE_YEAR:
    case SETTINGS_ITEM_DATE_MONTH:
    case SETTINGS_ITEM_DATE_DAY:
    case SETTINGS_ITEM_TIME_HOUR:
    case SETTINGS_ITEM_TIME_MINUTE:
    case SETTINGS_ITEM_TIME_SECOND: {
        struct tm now = settings_rtc_time();

        if(item == SETTINGS_ITEM_DATE_YEAR)
            return now.tm_year + 1900;
        if(item == SETTINGS_ITEM_DATE_MONTH)
            return now.tm_mon;
        if(item == SETTINGS_ITEM_DATE_DAY)
            return now.tm_mday;
        if(item == SETTINGS_ITEM_TIME_HOUR)
            return now.tm_hour;
        if(item == SETTINGS_ITEM_TIME_MINUTE)
            return now.tm_min;
        return now.tm_sec;
    }
    case SETTINGS_ITEM_SHUFFLE:
        return global_settings.playlist_shuffle ? 1 : 0;
    case SETTINGS_ITEM_REPEAT:
        return crazypod_queue_repeat();
    case SETTINGS_ITEM_IDLE_POWEROFF:
        return global_settings.poweroff;
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return global_settings.sleeptimer_duration;
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
        return global_settings.sleeptimer_on_startup ? 1 : 0;
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
        return global_settings.keypress_restarts_sleeptimer ? 1 : 0;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        return global_settings.usb_charging;
#endif
#ifdef HAVE_DISK_STORAGE
    case SETTINGS_ITEM_STORAGE_MODE:
        return global_settings.storage_mode;
#endif
    case SETTINGS_ITEM_BEEP:
        return global_settings.beep;
    case SETTINGS_ITEM_KEYCLICK:
        return global_settings.keyclick;
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
        return global_settings.keyclick_hardware ? 1 : 0;
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return global_settings.keyclick_repeats ? 1 : 0;
    default:
        return 0;
    }
}

int crazypod_ui_settings_choice_count(int item)
{
    switch(item) {
    case SETTINGS_ITEM_LANGUAGE:
        return CRAZYPOD_LANGUAGE_COUNT;
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_REDUCE_MOTION:
    case SETTINGS_ITEM_SHUFFLE:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return 2;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        return 3;
#endif
#ifdef HAVE_DISK_STORAGE
    case SETTINGS_ITEM_STORAGE_MODE:
        return 3;
#endif
    case SETTINGS_ITEM_BASS:
        return range_choice_count(sound_min(SOUND_BASS),
                                  sound_max(SOUND_BASS),
                                  setting_sound_step(SOUND_BASS));
    case SETTINGS_ITEM_TREBLE:
        return range_choice_count(sound_min(SOUND_TREBLE),
                                  sound_max(SOUND_TREBLE),
                                  setting_sound_step(SOUND_TREBLE));
    case SETTINGS_ITEM_BALANCE:
        return range_choice_count(sound_min(SOUND_BALANCE),
                                  sound_max(SOUND_BALANCE),
                                  setting_sound_step(SOUND_BALANCE));
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        return range_choice_count(MIN_BRIGHTNESS_SETTING,
                                  MAX_BRIGHTNESS_SETTING, 1);
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
#endif
        return (int)(sizeof(setting_timeout_values) /
                     sizeof(setting_timeout_values[0]));
    case SETTINGS_ITEM_LCD_SLEEP:
        return (int)(sizeof(setting_lcd_sleep_values) /
                     sizeof(setting_lcd_sleep_values[0]));
    case SETTINGS_ITEM_DATE_YEAR:
        return SETTINGS_RTC_YEAR_MAX - SETTINGS_RTC_YEAR_MIN + 1;
    case SETTINGS_ITEM_DATE_MONTH:
        return 12;
    case SETTINGS_ITEM_DATE_DAY: {
        struct tm now = settings_rtc_time();

        return settings_days_in_month(now.tm_year + 1900, now.tm_mon);
    }
    case SETTINGS_ITEM_TIME_HOUR:
        return 24;
    case SETTINGS_ITEM_TIME_MINUTE:
    case SETTINGS_ITEM_TIME_SECOND:
        return 60;
    case SETTINGS_ITEM_REPEAT:
        return (int)(sizeof(setting_repeat_values) /
                     sizeof(setting_repeat_values[0]));
    case SETTINGS_ITEM_IDLE_POWEROFF:
        return (int)(sizeof(setting_idle_poweroff_values) /
                     sizeof(setting_idle_poweroff_values[0]));
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return (int)(sizeof(setting_sleep_timer_values) /
                     sizeof(setting_sleep_timer_values[0]));
    case SETTINGS_ITEM_BEEP:
    case SETTINGS_ITEM_KEYCLICK:
        return 4;
    default:
        return 0;
    }
}

static int settings_choice_value(int item, int index)
{
    switch(item) {
    case SETTINGS_ITEM_LANGUAGE:
        if(index < 0)
            return CRAZYPOD_LANGUAGE_ENGLISH;
        return index < CRAZYPOD_LANGUAGE_COUNT
            ? index : CRAZYPOD_LANGUAGE_COUNT - 1;
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_REDUCE_MOTION:
    case SETTINGS_ITEM_SHUFFLE:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return index > 0 ? 1 : 0;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        if(index < USB_CHARGING_DISABLE)
            return USB_CHARGING_DISABLE;
        if(index > USB_CHARGING_FORCE)
            return USB_CHARGING_FORCE;
        return index;
#endif
#ifdef HAVE_DISK_STORAGE
    case SETTINGS_ITEM_STORAGE_MODE:
        if(index < 0)
            return 0;
        return index > 2 ? 2 : index;
#endif
    case SETTINGS_ITEM_BASS:
        return range_choice_value(index, sound_min(SOUND_BASS),
                                  sound_max(SOUND_BASS),
                                  setting_sound_step(SOUND_BASS));
    case SETTINGS_ITEM_TREBLE:
        return range_choice_value(index, sound_min(SOUND_TREBLE),
                                  sound_max(SOUND_TREBLE),
                                  setting_sound_step(SOUND_TREBLE));
    case SETTINGS_ITEM_BALANCE:
        return range_choice_value(index, sound_min(SOUND_BALANCE),
                                  sound_max(SOUND_BALANCE),
                                  setting_sound_step(SOUND_BALANCE));
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        return range_choice_value(index, MIN_BRIGHTNESS_SETTING,
                                  MAX_BRIGHTNESS_SETTING, 1);
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
#endif
        return setting_timeout_values[index];
    case SETTINGS_ITEM_LCD_SLEEP:
        return setting_lcd_sleep_values[index];
    case SETTINGS_ITEM_DATE_YEAR:
        return range_choice_value(index, SETTINGS_RTC_YEAR_MIN,
                                  SETTINGS_RTC_YEAR_MAX, 1);
    case SETTINGS_ITEM_DATE_MONTH:
        return range_choice_value(index, 0, 11, 1);
    case SETTINGS_ITEM_DATE_DAY: {
        struct tm now = settings_rtc_time();

        return range_choice_value(
            index, 1,
            settings_days_in_month(now.tm_year + 1900, now.tm_mon), 1);
    }
    case SETTINGS_ITEM_TIME_HOUR:
        return range_choice_value(index, 0, 23, 1);
    case SETTINGS_ITEM_TIME_MINUTE:
    case SETTINGS_ITEM_TIME_SECOND:
        return range_choice_value(index, 0, 59, 1);
    case SETTINGS_ITEM_REPEAT:
        return setting_repeat_values[index];
    case SETTINGS_ITEM_IDLE_POWEROFF:
        return setting_idle_poweroff_values[index];
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return setting_sleep_timer_values[index];
    case SETTINGS_ITEM_BEEP:
    case SETTINGS_ITEM_KEYCLICK:
        return index;
    default:
        return 0;
    }
}

static int find_value_index(const int *values, int count, int value)
{
    int index;

    for(index = 0; index < count; ++index) {
        if(values[index] == value)
            return index;
    }
    return 0;
}

int crazypod_ui_settings_choice_index(int item)
{
    int current = settings_item_current_value(item);

    switch(item) {
    case SETTINGS_ITEM_LANGUAGE:
        return current;
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_REDUCE_MOTION:
    case SETTINGS_ITEM_SHUFFLE:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return current ? 1 : 0;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        if(current < USB_CHARGING_DISABLE)
            return USB_CHARGING_DISABLE;
        if(current > USB_CHARGING_FORCE)
            return USB_CHARGING_FORCE;
        return current;
#endif
#ifdef HAVE_DISK_STORAGE
    case SETTINGS_ITEM_STORAGE_MODE:
        if(current < 0)
            return 0;
        return current > 2 ? 2 : current;
#endif
    case SETTINGS_ITEM_BASS:
        return range_choice_index(current, sound_min(SOUND_BASS),
                                  sound_max(SOUND_BASS),
                                  setting_sound_step(SOUND_BASS));
    case SETTINGS_ITEM_TREBLE:
        return range_choice_index(current, sound_min(SOUND_TREBLE),
                                  sound_max(SOUND_TREBLE),
                                  setting_sound_step(SOUND_TREBLE));
    case SETTINGS_ITEM_BALANCE:
        return range_choice_index(current, sound_min(SOUND_BALANCE),
                                  sound_max(SOUND_BALANCE),
                                  setting_sound_step(SOUND_BALANCE));
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        return range_choice_index(current, MIN_BRIGHTNESS_SETTING,
                                  MAX_BRIGHTNESS_SETTING, 1);
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
#endif
        return find_value_index(
            setting_timeout_values,
            (int)(sizeof(setting_timeout_values) /
                  sizeof(setting_timeout_values[0])),
            current);
    case SETTINGS_ITEM_LCD_SLEEP:
        return find_value_index(
            setting_lcd_sleep_values,
            (int)(sizeof(setting_lcd_sleep_values) /
                  sizeof(setting_lcd_sleep_values[0])),
            current);
    case SETTINGS_ITEM_DATE_YEAR:
        return range_choice_index(current, SETTINGS_RTC_YEAR_MIN,
                                  SETTINGS_RTC_YEAR_MAX, 1);
    case SETTINGS_ITEM_DATE_MONTH:
        return range_choice_index(current, 0, 11, 1);
    case SETTINGS_ITEM_DATE_DAY: {
        struct tm now = settings_rtc_time();

        return range_choice_index(
            current, 1,
            settings_days_in_month(now.tm_year + 1900, now.tm_mon), 1);
    }
    case SETTINGS_ITEM_TIME_HOUR:
        return range_choice_index(current, 0, 23, 1);
    case SETTINGS_ITEM_TIME_MINUTE:
    case SETTINGS_ITEM_TIME_SECOND:
        return range_choice_index(current, 0, 59, 1);
    case SETTINGS_ITEM_REPEAT:
        return find_value_index(
            setting_repeat_values,
            (int)(sizeof(setting_repeat_values) /
                  sizeof(setting_repeat_values[0])),
            current);
    case SETTINGS_ITEM_IDLE_POWEROFF:
        return find_value_index(
            setting_idle_poweroff_values,
            (int)(sizeof(setting_idle_poweroff_values) /
                  sizeof(setting_idle_poweroff_values[0])),
            current);
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return find_value_index(
            setting_sleep_timer_values,
            (int)(sizeof(setting_sleep_timer_values) /
                  sizeof(setting_sleep_timer_values[0])),
            current);
    case SETTINGS_ITEM_BEEP:
    case SETTINGS_ITEM_KEYCLICK:
        if(current < 0)
            return 0;
        return current > 3 ? 3 : current;
    default:
        return 0;
    }
}

static const char *settings_repeat_title(int value)
{
    switch(value) {
    case REPEAT_ALL: return CP_TR("All");
    case REPEAT_ONE: return CP_TR("One");
    default: return CP_TR("Off");
    }
}

static const char *settings_level_title(int value)
{
    switch(value) {
    case 1: return CP_TR("Weak");
    case 2: return CP_TR("Moderate");
    case 3: return CP_TR("Strong");
    default: return CP_TR("Off");
    }
}

#ifdef HAVE_USB_CHARGING_ENABLE
static const char *settings_usb_charging_title(int value)
{
    switch(value) {
    case USB_CHARGING_ENABLE:
        return CP_TR("On");
    case USB_CHARGING_FORCE:
        return CP_TR("Force");
    default:
        return CP_TR("Off");
    }
}
#endif

#ifdef HAVE_DISK_STORAGE
static const char *settings_storage_mode_title(int value)
{
    switch(value) {
    case 1: return CP_TR("HDD");
    case 2: return CP_TR("SSD");
    default: return CP_TR("Auto");
    }
}
#endif

const char *crazypod_ui_settings_choice_title(int item, int index)
{
    static char text[24];
    int value = settings_choice_value(item, index);

    switch(item) {
    case SETTINGS_ITEM_LANGUAGE:
        return crazypod_language_native_name(
            (enum crazypod_language)value);
    case SETTINGS_ITEM_EQ_ENABLED:
    case SETTINGS_ITEM_REDUCE_MOTION:
    case SETTINGS_ITEM_SHUFFLE:
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        return format_bool_value(value != 0);
    case SETTINGS_ITEM_BASS:
    case SETTINGS_ITEM_TREBLE:
        snprintf(text, sizeof(text), CP_FMT("%+d dB"), value);
        return text;
    case SETTINGS_ITEM_BALANCE:
        if(value < 0)
            snprintf(text, sizeof(text), CP_FMT("Left %d%%"), -value);
        else if(value > 0)
            snprintf(text, sizeof(text), CP_FMT("Right %d%%"), value);
        else
            snprintf(text, sizeof(text), CP_FMT("Center"));
        return text;
    case SETTINGS_ITEM_DATE_YEAR:
    case SETTINGS_ITEM_DATE_DAY:
    case SETTINGS_ITEM_TIME_HOUR:
    case SETTINGS_ITEM_TIME_MINUTE:
    case SETTINGS_ITEM_TIME_SECOND:
        snprintf(text, sizeof(text), CP_FMT("%d"), value);
        return text;
    case SETTINGS_ITEM_DATE_MONTH:
        snprintf(text, sizeof(text), CP_FMT("%d"), value + 1);
        return text;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        snprintf(text, sizeof(text), CP_FMT("%d"), value);
        return text;
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
#endif
    case SETTINGS_ITEM_LCD_SLEEP:
        return format_timeout_value(value);
    case SETTINGS_ITEM_REPEAT:
        return settings_repeat_title(value);
    case SETTINGS_ITEM_IDLE_POWEROFF:
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        return format_sleep_timer_value(value);
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        return settings_usb_charging_title(value);
#endif
#ifdef HAVE_DISK_STORAGE
    case SETTINGS_ITEM_STORAGE_MODE:
        return settings_storage_mode_title(value);
#endif
    case SETTINGS_ITEM_BEEP:
    case SETTINGS_ITEM_KEYCLICK:
        return settings_level_title(value);
    default:
        return "";
    }
}

const char *crazypod_ui_settings_item_value_label(int item)
{
#ifdef HAVE_DISK_STORAGE
    if(item == SETTINGS_ITEM_STORAGE_MODE &&
       global_settings.storage_mode == 0)
        return storage_get_ssd_mode() ? CP_TR("Auto (SSD)") : CP_TR("Auto (HDD)");
#endif
    return crazypod_ui_settings_choice_title(item, crazypod_ui_settings_choice_index(item));
}

bool crazypod_ui_settings_apply_choice(int item, int index)
{
    int value = settings_choice_value(item, index);

    if(item >= SETTINGS_ITEM_DATE_YEAR &&
       item <= SETTINGS_ITEM_TIME_SECOND) {
        struct tm now = settings_rtc_time();
        int max_day;

        switch(item) {
        case SETTINGS_ITEM_DATE_YEAR:
            now.tm_year = value - 1900;
            break;
        case SETTINGS_ITEM_DATE_MONTH:
            now.tm_mon = value;
            break;
        case SETTINGS_ITEM_DATE_DAY:
            now.tm_mday = value;
            break;
        case SETTINGS_ITEM_TIME_HOUR:
            now.tm_hour = value;
            break;
        case SETTINGS_ITEM_TIME_MINUTE:
            now.tm_min = value;
            break;
        case SETTINGS_ITEM_TIME_SECOND:
            now.tm_sec = value;
            break;
        default:
            break;
        }
        max_day = settings_days_in_month(now.tm_year + 1900, now.tm_mon);
        if(now.tm_mday > max_day)
            now.tm_mday = max_day;
        set_day_of_week(&now);
        set_day_of_year(&now);
        return set_time(&now) == 0;
    }

    switch(item) {
    case SETTINGS_ITEM_LANGUAGE:
        crazypod_language_set((enum crazypod_language)value);
        break;
    case SETTINGS_ITEM_EQ_ENABLED:
        global_settings.eq_enabled = value != 0;
        crazypod_eq_settings_apply();
        break;
    case SETTINGS_ITEM_BASS:
        global_settings.bass = value;
        sound_set(SOUND_BASS, global_settings.bass);
        break;
    case SETTINGS_ITEM_TREBLE:
        global_settings.treble = value;
        sound_set(SOUND_TREBLE, global_settings.treble);
        break;
    case SETTINGS_ITEM_BALANCE:
        global_settings.balance = value;
        sound_set(SOUND_BALANCE, global_settings.balance);
        break;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    case SETTINGS_ITEM_BRIGHTNESS:
        global_settings.brightness = value;
        backlight_set_brightness(global_settings.brightness);
        break;
#endif
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT:
        global_settings.backlight_timeout = value;
        backlight_set_timeout(global_settings.backlight_timeout);
        break;
#if CONFIG_CHARGING
    case SETTINGS_ITEM_BACKLIGHT_TIMEOUT_PLUGGED:
        global_settings.backlight_timeout_plugged = value;
        backlight_set_timeout_plugged(
            global_settings.backlight_timeout_plugged);
        break;
#endif
    case SETTINGS_ITEM_LCD_SLEEP:
        global_settings.lcd_sleep_after_backlight_off = value;
        lcd_set_sleep_after_backlight_off(
            global_settings.lcd_sleep_after_backlight_off);
        break;
    case SETTINGS_ITEM_REDUCE_MOTION:
        crazypod_state_set_reduce_motion(value != 0);
        break;
    case SETTINGS_ITEM_SHUFFLE:
        crazypod_queue_set_shuffle(value != 0);
        crazypod_state_mark_dirty();
        break;
    case SETTINGS_ITEM_REPEAT:
        crazypod_queue_set_repeat(value);
        crazypod_state_mark_dirty();
        break;
    case SETTINGS_ITEM_IDLE_POWEROFF:
        global_settings.poweroff = value;
        set_poweroff_timeout(global_settings.poweroff);
        reset_poweroff_timer();
        break;
    case SETTINGS_ITEM_SLEEP_TIMER_DURATION:
        global_settings.sleeptimer_duration = value;
        set_sleeptimer_duration(global_settings.sleeptimer_duration);
        break;
    case SETTINGS_ITEM_SLEEP_TIMER_STARTUP:
        global_settings.sleeptimer_on_startup = value != 0;
        break;
    case SETTINGS_ITEM_SLEEP_TIMER_KEYPRESS:
        global_settings.keypress_restarts_sleeptimer = value != 0;
        set_keypress_restarts_sleep_timer(
            global_settings.keypress_restarts_sleeptimer);
        break;
#ifdef HAVE_USB_CHARGING_ENABLE
    case SETTINGS_ITEM_USB_CHARGING:
        global_settings.usb_charging = value;
        usb_charging_enable(global_settings.usb_charging);
        break;
#endif
#ifdef HAVE_DISK_STORAGE
    case SETTINGS_ITEM_STORAGE_MODE:
        global_settings.storage_mode = value;
        storage_set_storage_mode(global_settings.storage_mode);
        break;
#endif
    case SETTINGS_ITEM_BEEP:
        global_settings.beep = value;
        break;
    case SETTINGS_ITEM_KEYCLICK:
        global_settings.keyclick = value;
        break;
#ifdef HAVE_HARDWARE_CLICK
    case SETTINGS_ITEM_SPEAKER_CLICK:
        global_settings.keyclick_hardware = value != 0;
        break;
#endif
    case SETTINGS_ITEM_KEYCLICK_REPEATS:
        global_settings.keyclick_repeats = value != 0;
        break;
    default:
        break;
    }
    crazypod_state_mark_dirty();
    crazypod_state_save(false);
    return true;
}
