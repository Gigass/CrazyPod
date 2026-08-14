#ifndef CRAZYPOD_MINIAPP_ALARM_SERVICE_H
#define CRAZYPOD_MINIAPP_ALARM_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct crazypod_miniapp_metadata;

void crazypod_miniapp_alarm_initialize(void);
void crazypod_miniapp_alarm_tick(uint32_t epoch_seconds);
uint32_t crazypod_miniapp_alarm_next_epoch(void);
bool crazypod_miniapp_alarm_save_pending(void);
int crazypod_miniapp_alarm_service_call(
    const struct crazypod_miniapp_metadata *metadata,
    uint32_t epoch_seconds, uint32_t operation,
    const void *request, size_t request_size,
    void *response, size_t response_capacity);

#endif
