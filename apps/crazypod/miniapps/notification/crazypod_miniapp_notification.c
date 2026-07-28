#include <stdio.h>
#include <string.h>

#include "../../crazypod_miniapps.h"
#include "crazypod_miniapp_notification.h"

static struct {
    char text[CP_MINIAPP_TOAST_TEXT_SIZE];
    long until;
    bool changed;
} notification;

static bool expired(long tick)
{
    return notification.text[0] != '\0' &&
           (long)(tick - notification.until) >= 0;
}

void crazypod_miniapp_notification_reset(void)
{
    memset(&notification, 0, sizeof(notification));
}

int crazypod_miniapp_notification_show(
    const char *text, uint32_t duration_ms,
    long tick, long ticks_per_second)
{
    size_t length;
    long duration_ticks;

    if(text == NULL)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    length = strlen(text);
    if(length == 0 || length >= sizeof(notification.text))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(duration_ms == 0)
        duration_ms = 2000;
    if(duration_ms < 500)
        duration_ms = 500;
    if(duration_ms > 5000)
        duration_ms = 5000;
    memcpy(notification.text, text, length + 1);
    duration_ticks =
        (long)(((uint64_t)duration_ms * ticks_per_second + 999u) / 1000u);
    notification.until = tick + (duration_ticks > 0 ? duration_ticks : 1);
    notification.changed = true;
    return CRAZYPOD_MINIAPP_OK;
}

bool crazypod_miniapp_notification_take_changed(long tick)
{
    bool changed;

    if(expired(tick)) {
        notification.text[0] = '\0';
        notification.until = 0;
        notification.changed = true;
    }
    changed = notification.changed;
    notification.changed = false;
    return changed;
}

bool crazypod_miniapp_notification_get(
    long tick, char *buffer, size_t capacity)
{
    if(buffer == NULL || capacity == 0)
        return false;
    buffer[0] = '\0';
    if(expired(tick)) {
        notification.text[0] = '\0';
        notification.until = 0;
        return false;
    }
    if(notification.text[0] == '\0')
        return false;
    snprintf(buffer, capacity, "%s", notification.text);
    buffer[capacity - 1] = '\0';
    return true;
}
