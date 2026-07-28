#include <assert.h>
#include <string.h>

#include "crazypod_miniapp_alarm_service.h"
#include "crazypod_miniapp_alarm_store.h"
#include "crazypod_miniapp_catalog.h"

struct memory_slot {
    struct crazypod_miniapp_alarm_record alarm;
    struct crazypod_miniapp_alarm_record notification;
    bool has_alarm;
    bool has_notification;
};

static struct memory_slot slots[CP_MINIAPP_ALARM_SLOT_COUNT];

int crazypod_miniapp_alarm_store_load(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    (void)id;
    if(slot >= CP_MINIAPP_ALARM_SLOT_COUNT || !slots[slot].has_alarm)
        return 0;
    *record = slots[slot].alarm;
    return 1;
}

bool crazypod_miniapp_alarm_store_save(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    (void)id;
    slots[slot].alarm = *record;
    slots[slot].has_alarm = record->flags != 0;
    return true;
}

int crazypod_miniapp_notification_store_load(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    (void)id;
    if(slot >= CP_MINIAPP_ALARM_SLOT_COUNT ||
       !slots[slot].has_notification)
        return 0;
    *record = slots[slot].notification;
    return 1;
}

bool crazypod_miniapp_notification_store_save(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    (void)id;
    slots[slot].notification = *record;
    slots[slot].has_notification = record->flags != 0;
    return true;
}

bool crazypod_miniapp_alarm_store_same_event(
    const struct crazypod_miniapp_alarm_record *left,
    const struct crazypod_miniapp_alarm_record *right)
{
    return left->deadline_epoch == right->deadline_epoch &&
           left->token == right->token;
}

int main(void)
{
    struct crazypod_miniapp_metadata metadata = { 0 };
    struct crazypod_miniapp_alarm alarm;
    uint32_t token = 0;

    strcpy(metadata.id, "timer");
    strcpy(metadata.name, "Timer");
    crazypod_miniapp_catalog_reset();
    assert(crazypod_miniapp_catalog_add(&metadata));

    assert(crazypod_miniapp_alarm_set(
        "timer", 0, 200, 42, 100) == CRAZYPOD_MINIAPP_OK);
    assert(!crazypod_miniapp_alarm_service(199, &alarm));
    assert(crazypod_miniapp_alarm_service(200, &alarm));
    assert(strcmp(alarm.id, "timer") == 0);
    assert(alarm.token == 42);
    assert(crazypod_miniapp_alarm_fired(
        "timer", 0, 200, &token));
    assert(token == 42);
    assert(crazypod_miniapp_alarm_delivery_acknowledge(
        "timer", 200, 42) == CRAZYPOD_MINIAPP_OK);
    assert(!crazypod_miniapp_alarm_service(201, &alarm));
    crazypod_miniapp_alarm_acknowledge_slot("timer", 0, 201);
    assert(!crazypod_miniapp_alarm_fired(
        "timer", 0, 201, &token));
    return 0;
}
