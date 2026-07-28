#ifndef CRAZYPOD_MINIAPP_ALARM_SERVICE_H
#define CRAZYPOD_MINIAPP_ALARM_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "../../crazypod_miniapps.h"

int crazypod_miniapp_alarm_set(
    const char *id, uint8_t slot,
    uint32_t deadline_epoch, uint32_t token, uint32_t now);
void crazypod_miniapp_alarm_cancel(
    const char *id, uint8_t slot, uint32_t now);
bool crazypod_miniapp_alarm_fired(
    const char *id, uint8_t slot, uint32_t now, uint32_t *token);
void crazypod_miniapp_alarm_acknowledge_slot(
    const char *id, uint8_t slot, uint32_t now);
bool crazypod_miniapp_alarm_service(
    uint32_t now, struct crazypod_miniapp_alarm *alarm);
int crazypod_miniapp_alarm_acknowledge(
    const char *id, uint32_t now);
int crazypod_miniapp_alarm_delivery_acknowledge(
    const char *id, uint32_t deadline_epoch, uint32_t token);

#endif
