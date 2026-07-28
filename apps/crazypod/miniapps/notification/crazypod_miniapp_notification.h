#ifndef CRAZYPOD_MINIAPP_NOTIFICATION_H
#define CRAZYPOD_MINIAPP_NOTIFICATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void crazypod_miniapp_notification_reset(void);
int crazypod_miniapp_notification_show(
    const char *text, uint32_t duration_ms,
    long tick, long ticks_per_second);
bool crazypod_miniapp_notification_take_changed(long tick);
bool crazypod_miniapp_notification_get(
    long tick, char *buffer, size_t capacity);

#endif
