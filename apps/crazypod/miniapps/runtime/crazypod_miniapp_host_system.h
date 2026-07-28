#ifndef CRAZYPOD_MINIAPP_HOST_SYSTEM_H
#define CRAZYPOD_MINIAPP_HOST_SYSTEM_H

#include "../../crazypod_miniapps.h"

uint32_t crazypod_miniapp_host_epoch_seconds(void);
uint32_t crazypod_miniapp_host_monotonic_ms(void);
void crazypod_miniapp_host_format_number(
    double value, char *buffer, size_t capacity);
int crazypod_miniapp_host_system_info(struct cp_system_info *info);
void crazypod_miniapp_host_format_duration(
    uint32_t seconds, char *buffer, size_t capacity);
void crazypod_miniapp_host_format_datetime(
    uint32_t epoch_seconds, enum cp_datetime_format format,
    char *buffer, size_t capacity);
int crazypod_miniapp_host_now_playing(struct cp_now_playing *info);

#endif
