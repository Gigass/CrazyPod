#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "dir.h"
#include "file.h"
#include "logf.h"
#include "misc.h"
#include "string-extra.h"

#include "../../crazypod_miniapps.h"
#include "crazypod_miniapp_alarm_service.h"

#define ALARM_STORE_MAGIC 0x31414d43u
#define ALARM_STORE_PATH "/.crazypod/miniapp-alarms.bin"
#define ALARM_STORE_TEMP "/.crazypod/miniapp-alarms.tmp"
#define ALARM_MAX_DELAY (30u * 24u * 60u * 60u)

struct alarm_record {
    uint32_t alarm_id;
    uint32_t epoch_seconds;
    char owner[CRAZYPOD_MINIAPP_ID_SIZE];
    char label[CP_ALARM_LABEL_CAPACITY];
    uint8_t active;
    uint8_t reserved[3];
};

struct alarm_store {
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    uint32_t next_id;
    struct alarm_record records[CP_ALARM_CAPACITY];
    uint32_t checksum;
};

static struct alarm_store alarms;
static uint32_t last_tick_epoch;
static bool store_dirty;

static uint32_t alarm_checksum(const struct alarm_store *store)
{
    return ~crc_32r(store, offsetof(struct alarm_store, checksum),
                    0xffffffffu);
}

static bool write_exact(int file, const void *buffer, size_t size)
{
    const uint8_t *bytes = buffer;

    while(size > 0) {
        ssize_t count = write(file, bytes, size);
        if(count <= 0)
            return false;
        bytes += count;
        size -= (size_t)count;
    }
    return true;
}

static bool read_exact(int file, void *buffer, size_t size)
{
    uint8_t *bytes = buffer;

    while(size > 0) {
        ssize_t count = read(file, bytes, size);
        if(count <= 0)
            return false;
        bytes += count;
        size -= (size_t)count;
    }
    return true;
}

static bool save_alarms(void)
{
    int file;
    bool success;

    mkdir("/.crazypod");
    alarms.checksum = alarm_checksum(&alarms);
    file = open(ALARM_STORE_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(file < 0)
        return false;
    success = write_exact(file, &alarms, sizeof(alarms)) && fsync(file) == 0;
    if(close(file) < 0)
        success = false;
    if(!success || rename(ALARM_STORE_TEMP, ALARM_STORE_PATH) < 0) {
        remove(ALARM_STORE_TEMP);
        return false;
    }
    return true;
}

void crazypod_miniapp_alarm_initialize(void)
{
    int file = open(ALARM_STORE_PATH, O_RDONLY);
    bool valid = false;

    memset(&alarms, 0, sizeof(alarms));
    if(file >= 0) {
        valid = filesize(file) == (off_t)sizeof(alarms) &&
            read_exact(file, &alarms, sizeof(alarms));
        close(file);
    }
    if(!valid || alarms.magic != ALARM_STORE_MAGIC ||
       alarms.version != 1u || alarms.struct_size != sizeof(alarms) ||
       alarms.checksum != alarm_checksum(&alarms)) {
        memset(&alarms, 0, sizeof(alarms));
        alarms.magic = ALARM_STORE_MAGIC;
        alarms.version = 1u;
        alarms.struct_size = sizeof(alarms);
        alarms.next_id = 1u;
    }
    last_tick_epoch = 0;
    store_dirty = false;
}

void crazypod_miniapp_alarm_tick(uint32_t epoch_seconds)
{
    unsigned index;
    bool changed = false;

    if(epoch_seconds == 0)
        return;
    if(epoch_seconds != last_tick_epoch) {
        last_tick_epoch = epoch_seconds;
        for(index = 0; index < CP_ALARM_CAPACITY; ++index) {
            struct alarm_record *record = &alarms.records[index];

            if(record->active == 0 || record->epoch_seconds > epoch_seconds)
                continue;
            record->active = 0;
            changed = true;
            logf("miniapp alarm %s: %s", record->owner, record->label);
            beep_play(1000, 500, 8000);
        }
    }
    if(changed)
        store_dirty = true;
    if(store_dirty && save_alarms())
        store_dirty = false;
}

static int schedule_alarm(
    const struct crazypod_miniapp_metadata *metadata,
    uint32_t now, const struct cp_alarm_request *request,
    size_t request_size, void *response, size_t response_capacity)
{
    struct cp_alarm_entry result;
    struct alarm_store previous;
    struct alarm_record *record = NULL;
    unsigned index;

    if(request == NULL || request_size != sizeof(*request) ||
       request->struct_size != sizeof(*request) ||
       memchr(request->label, '\0', sizeof(request->label)) == NULL ||
       request->epoch_seconds <= now ||
       request->epoch_seconds - now > ALARM_MAX_DELAY ||
       response == NULL || response_capacity < sizeof(result))
        return CP_NATIVE_ERROR_ARGUMENT;
    for(index = 0; index < CP_ALARM_CAPACITY; ++index) {
        if(alarms.records[index].active == 0) {
            record = &alarms.records[index];
            break;
        }
    }
    if(record == NULL)
        return CP_NATIVE_ERROR_LIMIT;
    previous = alarms;
    memset(record, 0, sizeof(*record));
    record->active = 1;
    record->alarm_id = alarms.next_id++;
    if(alarms.next_id == 0)
        alarms.next_id = 1;
    record->epoch_seconds = request->epoch_seconds;
    strlcpy(record->owner, metadata->id, sizeof(record->owner));
    strlcpy(record->label, request->label, sizeof(record->label));
    if(!save_alarms()) {
        alarms = previous;
        return CP_NATIVE_ERROR_IO;
    }
    store_dirty = false;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.alarm_id = record->alarm_id;
    result.epoch_seconds = record->epoch_seconds;
    result.active = 1;
    strlcpy(result.label, record->label, sizeof(result.label));
    memcpy(response, &result, sizeof(result));
    return (int)sizeof(result);
}

static int cancel_alarm(
    const struct crazypod_miniapp_metadata *metadata,
    const struct cp_alarm_request *request, size_t request_size)
{
    struct alarm_store previous;
    unsigned index;

    if(request == NULL || request_size != sizeof(*request) ||
       request->struct_size != sizeof(*request) || request->alarm_id == 0)
        return CP_NATIVE_ERROR_ARGUMENT;
    for(index = 0; index < CP_ALARM_CAPACITY; ++index) {
        struct alarm_record *record = &alarms.records[index];

        if(record->active != 0 && record->alarm_id == request->alarm_id &&
           strcmp(record->owner, metadata->id) == 0) {
            previous = alarms;
            record->active = 0;
            if(!save_alarms()) {
                alarms = previous;
                return CP_NATIVE_ERROR_IO;
            }
            store_dirty = false;
            return CP_NATIVE_OK;
        }
    }
    return CP_NATIVE_ERROR_STATE;
}

static int list_alarms(
    const struct crazypod_miniapp_metadata *metadata,
    void *response, size_t response_capacity)
{
    struct cp_alarm_list list;
    unsigned index;

    if(response == NULL || response_capacity < sizeof(list))
        return CP_NATIVE_ERROR_ARGUMENT;
    memset(&list, 0, sizeof(list));
    list.struct_size = sizeof(list);
    for(index = 0; index < CP_ALARM_CAPACITY; ++index) {
        const struct alarm_record *record = &alarms.records[index];
        struct cp_alarm_entry *entry;

        if(record->active == 0 || strcmp(record->owner, metadata->id) != 0)
            continue;
        entry = &list.entries[list.count++];
        entry->struct_size = sizeof(*entry);
        entry->alarm_id = record->alarm_id;
        entry->epoch_seconds = record->epoch_seconds;
        entry->active = 1;
        strlcpy(entry->label, record->label, sizeof(entry->label));
    }
    memcpy(response, &list, sizeof(list));
    return (int)sizeof(list);
}

int crazypod_miniapp_alarm_service_call(
    const struct crazypod_miniapp_metadata *metadata,
    uint32_t epoch_seconds, uint32_t operation,
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    if(metadata == NULL ||
       (metadata->permissions &
        CRAZYPOD_MINIAPP_PERMISSION_ALARMS_SCHEDULE) == 0)
        return CP_NATIVE_ERROR_UNSUPPORTED;
    if(operation == CP_NATIVE_ALARM_SCHEDULE)
        return schedule_alarm(metadata, epoch_seconds, request, request_size,
                              response, response_capacity);
    if(operation == CP_NATIVE_ALARM_CANCEL) {
        if(response != NULL || response_capacity != 0)
            return CP_NATIVE_ERROR_ARGUMENT;
        return cancel_alarm(metadata, request, request_size);
    }
    if(operation == CP_NATIVE_ALARM_LIST) {
        if(request != NULL || request_size != 0)
            return CP_NATIVE_ERROR_ARGUMENT;
        return list_alarms(metadata, response, response_capacity);
    }
    return CP_NATIVE_ERROR_UNSUPPORTED;
}

#endif
