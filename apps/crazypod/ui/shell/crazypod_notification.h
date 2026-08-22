#ifndef CRAZYPOD_NOTIFICATION_H
#define CRAZYPOD_NOTIFICATION_H

#include <stdbool.h>
#include <stdint.h>

enum crazypod_notification_kind {
    CRAZYPOD_NOTIFICATION_INFO = 0,
    CRAZYPOD_NOTIFICATION_SUCCESS,
    CRAZYPOD_NOTIFICATION_ERROR,
};

void crazypod_notification_show(
    enum crazypod_notification_kind kind,
    const char *message);
void crazypod_notification_show_for(
    enum crazypod_notification_kind kind,
    const char *message, uint32_t duration_ms);
void crazypod_notification_flash(void);
void crazypod_notification_dismiss(void);
bool crazypod_notification_bounds(
    int *left, int *top, int *right, int *bottom);

#endif
