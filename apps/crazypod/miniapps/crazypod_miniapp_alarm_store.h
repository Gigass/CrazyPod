#ifndef CRAZYPOD_MINIAPP_ALARM_STORE_H
#define CRAZYPOD_MINIAPP_ALARM_STORE_H

#include <stdbool.h>
#include <stdint.h>

enum crazypod_miniapp_alarm_flags {
    ALARM_ACTIVE = 1u << 0,
    ALARM_FIRED = 1u << 1
};

struct crazypod_miniapp_alarm_record {
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    uint32_t deadline_epoch;
    uint32_t token;
    uint32_t flags;
    uint32_t checksum;
};

int crazypod_miniapp_alarm_store_load(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record);
bool crazypod_miniapp_alarm_store_save(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record);
int crazypod_miniapp_notification_store_load(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record);
bool crazypod_miniapp_notification_store_save(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record);
bool crazypod_miniapp_alarm_store_same_event(
    const struct crazypod_miniapp_alarm_record *left,
    const struct crazypod_miniapp_alarm_record *right);

#endif
