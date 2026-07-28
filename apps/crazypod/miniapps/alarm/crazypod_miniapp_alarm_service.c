#include <stdio.h>
#include <string.h>

#include "../catalog/crazypod_miniapp_catalog.h"
#include "../crazypod_miniapp_alarm_store.h"
#include "crazypod_miniapp_alarm_service.h"

static bool known_id(const char *id)
{
    return crazypod_miniapp_catalog_find(id) >= 0;
}

int crazypod_miniapp_alarm_set(
    const char *id, uint8_t slot,
    uint32_t deadline_epoch, uint32_t token, uint32_t now)
{
    struct crazypod_miniapp_alarm_record record;
    int loaded;

    if(!known_id(id) || slot >= CP_MINIAPP_ALARM_SLOT_COUNT ||
       deadline_epoch == 0 || token == 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    (void)crazypod_miniapp_alarm_service(now, NULL);
    loaded = crazypod_miniapp_alarm_store_load(id, slot, &record);
    if(loaded == 1 &&
       ((record.flags & ALARM_FIRED) != 0 ||
        ((record.flags & ALARM_ACTIVE) != 0 &&
         now != 0 && record.deadline_epoch <= now)))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    memset(&record, 0, sizeof(record));
    record.deadline_epoch = deadline_epoch;
    record.token = token;
    record.flags = ALARM_ACTIVE;
    return crazypod_miniapp_alarm_store_save(id, slot, &record)
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_STATE;
}

void crazypod_miniapp_alarm_cancel(
    const char *id, uint8_t slot, uint32_t now)
{
    struct crazypod_miniapp_alarm_record record;

    if(!known_id(id) || slot >= CP_MINIAPP_ALARM_SLOT_COUNT)
        return;
    (void)crazypod_miniapp_alarm_service(now, NULL);
    if(crazypod_miniapp_alarm_store_load(id, slot, &record) != 1)
        return;
    if((record.flags & ALARM_ACTIVE) != 0 &&
       (record.flags & ALARM_FIRED) == 0 &&
       now != 0 && record.deadline_epoch <= now)
        return;
    memset(&record, 0, sizeof(record));
    (void)crazypod_miniapp_alarm_store_save(id, slot, &record);
}

bool crazypod_miniapp_alarm_fired(
    const char *id, uint8_t slot, uint32_t now, uint32_t *token)
{
    struct crazypod_miniapp_alarm_record record;

    if(!known_id(id) || slot >= CP_MINIAPP_ALARM_SLOT_COUNT)
        return false;
    (void)crazypod_miniapp_alarm_service(now, NULL);
    if(crazypod_miniapp_alarm_store_load(id, slot, &record) != 1 ||
       (record.flags & (ALARM_ACTIVE | ALARM_FIRED)) !=
           (ALARM_ACTIVE | ALARM_FIRED))
        return false;
    if(token != NULL)
        *token = record.token;
    return true;
}

void crazypod_miniapp_alarm_acknowledge_slot(
    const char *id, uint8_t slot, uint32_t now)
{
    struct crazypod_miniapp_alarm_record record;

    if(!known_id(id) || slot >= CP_MINIAPP_ALARM_SLOT_COUNT)
        return;
    (void)crazypod_miniapp_alarm_service(now, NULL);
    if(crazypod_miniapp_alarm_store_load(id, slot, &record) != 1 ||
       (record.flags & ALARM_FIRED) == 0)
        return;
    memset(&record, 0, sizeof(record));
    (void)crazypod_miniapp_alarm_store_save(id, slot, &record);
}

static bool service_slot(
    const struct crazypod_miniapp_metadata *metadata,
    uint8_t slot, uint32_t now,
    struct crazypod_miniapp_alarm *alarm)
{
    struct crazypod_miniapp_alarm_record record;
    struct crazypod_miniapp_alarm_record notification;
    int loaded = crazypod_miniapp_alarm_store_load(
        metadata->id, slot, &record);
    int notification_loaded = crazypod_miniapp_notification_store_load(
        metadata->id, slot, &notification);

    if(notification_loaded == 1 && notification.flags == ALARM_ACTIVE) {
        bool ready = false;
        if(loaded == 1 && (record.flags & ALARM_ACTIVE) != 0 &&
           crazypod_miniapp_alarm_store_same_event(
               &record, &notification)) {
            if((record.flags & ALARM_FIRED) != 0)
                ready = true;
            else {
                record.flags |= ALARM_FIRED;
                ready = crazypod_miniapp_alarm_store_save(
                    metadata->id, slot, &record);
            }
        }
        else {
            ready = true;
        }
        if(ready) {
            notification.flags |= ALARM_FIRED;
            (void)crazypod_miniapp_notification_store_save(
                metadata->id, slot, &notification);
        }
    }

    if(loaded == 1 && (record.flags & ALARM_ACTIVE) != 0 &&
       (record.flags & ALARM_FIRED) == 0 &&
       record.deadline_epoch <= now) {
        bool queued = false;
        if(notification_loaded != 1 || notification.flags == 0) {
            notification = record;
            notification.flags = ALARM_ACTIVE;
            queued = crazypod_miniapp_notification_store_save(
                metadata->id, slot, &notification);
        }
        else if(crazypod_miniapp_alarm_store_same_event(
                    &record, &notification)) {
            queued = true;
        }
        if(queued) {
            record.flags |= ALARM_FIRED;
            if(crazypod_miniapp_alarm_store_save(
                   metadata->id, slot, &record) &&
               notification.flags == ALARM_ACTIVE) {
                notification.flags |= ALARM_FIRED;
                (void)crazypod_miniapp_notification_store_save(
                    metadata->id, slot, &notification);
            }
        }
    }

    if(alarm == NULL ||
       crazypod_miniapp_notification_store_load(
           metadata->id, slot, &notification) != 1 ||
       notification.flags != (ALARM_ACTIVE | ALARM_FIRED))
        return false;
    memset(alarm, 0, sizeof(*alarm));
    snprintf(alarm->id, sizeof(alarm->id), "%s", metadata->id);
    snprintf(alarm->name, sizeof(alarm->name), "%s", metadata->name);
    alarm->deadline_epoch = notification.deadline_epoch;
    alarm->token = notification.token;
    alarm->fired = true;
    return true;
}

bool crazypod_miniapp_alarm_service(
    uint32_t now, struct crazypod_miniapp_alarm *alarm)
{
    bool found = false;
    int index;

    if(now == 0)
        return false;
    for(index = 0; index < crazypod_miniapp_catalog_count(); ++index) {
        const struct crazypod_miniapp_metadata *metadata =
            crazypod_miniapp_catalog_get(index);
        uint8_t slot;
        for(slot = 0; slot < CP_MINIAPP_ALARM_SLOT_COUNT; ++slot)
            if(service_slot(
                   metadata, slot, now, found ? NULL : alarm))
                found = true;
    }
    return found;
}

int crazypod_miniapp_alarm_acknowledge(
    const char *id, uint32_t now)
{
    struct crazypod_miniapp_alarm_record record;
    int loaded;

    if(!known_id(id))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    (void)crazypod_miniapp_alarm_service(now, NULL);
    loaded = crazypod_miniapp_alarm_store_load(id, 0, &record);
    if(loaded < 0)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    if(loaded == 0 || (record.flags & ALARM_FIRED) == 0)
        return CRAZYPOD_MINIAPP_OK;
    memset(&record, 0, sizeof(record));
    return crazypod_miniapp_alarm_store_save(id, 0, &record)
        ? CRAZYPOD_MINIAPP_OK : CRAZYPOD_MINIAPP_ERROR_STATE;
}

int crazypod_miniapp_alarm_delivery_acknowledge(
    const char *id, uint32_t deadline_epoch, uint32_t token)
{
    struct crazypod_miniapp_alarm_record notification;
    uint8_t slot;

    if(!known_id(id))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    for(slot = 0; slot < CP_MINIAPP_ALARM_SLOT_COUNT; ++slot) {
        int loaded = crazypod_miniapp_notification_store_load(
            id, slot, &notification);
        if(loaded < 0)
            return CRAZYPOD_MINIAPP_ERROR_STATE;
        if(loaded == 1 &&
           notification.flags == (ALARM_ACTIVE | ALARM_FIRED) &&
           notification.deadline_epoch == deadline_epoch &&
           notification.token == token) {
            memset(&notification, 0, sizeof(notification));
            return crazypod_miniapp_notification_store_save(
                       id, slot, &notification)
                ? CRAZYPOD_MINIAPP_OK
                : CRAZYPOD_MINIAPP_ERROR_STATE;
        }
    }
    return CRAZYPOD_MINIAPP_OK;
}
