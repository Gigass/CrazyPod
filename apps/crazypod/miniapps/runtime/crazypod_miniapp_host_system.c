#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "kernel.h"
#include "metadata.h"
#include "power.h"
#include "powermgmt.h"
#include "timefuncs.h"
#include "usb.h"

#include "../../crazypod_state.h"
#include "crazypod_miniapp_host_system.h"

uint32_t crazypod_miniapp_host_epoch_seconds(void)
{
    struct tm *now = get_time();
    time_t epoch;

    if(now == NULL)
        return 0;
#if CONFIG_RTC
    if(!valid_time(now))
        return 0;
#endif
    epoch = mktime(now);
    return epoch > 0 ? (uint32_t)epoch : 0;
}

uint32_t crazypod_miniapp_host_monotonic_ms(void)
{
    return (uint32_t)(((uint64_t)(uint32_t)current_tick * 1000u) / HZ);
}

void crazypod_miniapp_host_format_number(
    double value, char *buffer, size_t capacity)
{
    if(buffer == NULL || capacity == 0)
        return;
    snprintf(buffer, capacity, "%.12g", value);
    buffer[capacity - 1] = '\0';
}

int crazypod_miniapp_host_system_info(struct cp_system_info *info)
{
    struct tm *now;
    int battery;
    int minutes;
    int audio;

    if(info == NULL || info->struct_size < sizeof(*info))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    memset(info, 0, sizeof(*info));
    info->struct_size = sizeof(*info);
    info->language = (uint32_t)crazypod_language_current();
    battery = battery_level();
    minutes = battery_time();
    info->battery_percent =
        (int16_t)(battery < 0 ? -1 : battery > 100 ? 100 : battery);
    info->battery_minutes =
        (int16_t)(minutes < 0 ? -1 :
                  minutes > INT16_MAX ? INT16_MAX : minutes);
    now = get_time();
#if CONFIG_RTC
    if(now != NULL && valid_time(now))
#else
    if(now != NULL)
#endif
        info->flags |= CP_SYSTEM_TIME_VALID;
#if CONFIG_CHARGING
    if(power_input_present())
        info->flags |= CP_SYSTEM_EXTERNAL_POWER;
#if CONFIG_CHARGING >= CHARGING_MONITOR
    if(charging_state())
        info->flags |= CP_SYSTEM_CHARGING;
#endif
#endif
#if defined(HAVE_USBSTACK)
    if(usb_inserted())
        info->flags |= CP_SYSTEM_USB_CONNECTED;
#endif
    audio = audio_status();
    if((audio & AUDIO_STATUS_PLAY) != 0)
        info->flags |= CP_SYSTEM_AUDIO_PLAYING;
    if((audio & AUDIO_STATUS_PAUSE) != 0)
        info->flags |= CP_SYSTEM_AUDIO_PAUSED;
    if(crazypod_state_reduce_motion())
        info->flags |= CP_SYSTEM_REDUCE_MOTION;
    return CRAZYPOD_MINIAPP_OK;
}

void crazypod_miniapp_host_format_duration(
    uint32_t seconds, char *buffer, size_t capacity)
{
    uint32_t hours;
    uint32_t minutes;

    if(buffer == NULL || capacity == 0)
        return;
    hours = seconds / 3600u;
    minutes = (seconds % 3600u) / 60u;
    if(hours > 0)
        snprintf(buffer, capacity, "%lu:%02lu:%02lu",
                 (unsigned long)hours, (unsigned long)minutes,
                 (unsigned long)(seconds % 60u));
    else
        snprintf(buffer, capacity, "%lu:%02lu",
                 (unsigned long)minutes,
                 (unsigned long)(seconds % 60u));
    buffer[capacity - 1] = '\0';
}

void crazypod_miniapp_host_format_datetime(
    uint32_t epoch_seconds, enum cp_datetime_format format,
    char *buffer, size_t capacity)
{
    time_t timestamp = (time_t)epoch_seconds;
    struct tm *value;

    if(buffer == NULL || capacity == 0)
        return;
    buffer[0] = '\0';
    if(epoch_seconds == 0 || format > CP_DATETIME_DATE_TIME)
        return;
    value = gmtime(&timestamp);
    if(value == NULL)
        return;
    if(format == CP_DATETIME_DATE)
        snprintf(buffer, capacity, CP_FMT("%04d-%02d-%02d"),
                 value->tm_year + 1900, value->tm_mon + 1,
                 value->tm_mday);
    else if(format == CP_DATETIME_TIME)
        snprintf(buffer, capacity, CP_FMT("%02d:%02d"),
                 value->tm_hour, value->tm_min);
    else
        snprintf(buffer, capacity, CP_FMT("%04d-%02d-%02d %02d:%02d"),
                 value->tm_year + 1900, value->tm_mon + 1,
                 value->tm_mday, value->tm_hour, value->tm_min);
    buffer[capacity - 1] = '\0';
}

int crazypod_miniapp_host_now_playing(struct cp_now_playing *info)
{
    const struct mp3entry *track;
    int status;

    if(info == NULL || info->struct_size < sizeof(*info))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    memset(info, 0, sizeof(*info));
    info->struct_size = sizeof(*info);
    status = audio_status();
    track = audio_current_track();
    if(track == NULL)
        return CRAZYPOD_MINIAPP_OK;
    info->flags |= CP_NOW_PLAYING_AVAILABLE;
    if((status & AUDIO_STATUS_PLAY) != 0)
        info->flags |= CP_NOW_PLAYING_PLAYING;
    if((status & AUDIO_STATUS_PAUSE) != 0)
        info->flags |= CP_NOW_PLAYING_PAUSED;
    info->elapsed_ms = track->elapsed;
    info->duration_ms = track->length;
    cp_text_copy(info->title, sizeof(info->title), track->title);
    cp_text_copy(info->artist, sizeof(info->artist), track->artist);
    cp_text_copy(info->album, sizeof(info->album), track->album);
    return CRAZYPOD_MINIAPP_OK;
}

#endif
