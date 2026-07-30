#include "config.h"

#include "../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "dir.h"
#include "errno.h"
#include "file.h"
#include "kernel.h"

#include "../../../miniapps/sdk/crazypod_miniapp.h"
#include "../crazypod_miniapps.h"
#include "catalog/crazypod_miniapp_catalog.h"
#include "crazypod_miniapp_alarm_store.h"

#define DATA_ROOT "/.crazypod/miniapp-data"
#define ALARM_MAGIC 0x4350414cu
#define DISK_VERSION 1u
#define FILENAME_SIZE 24
#define WRITE_RETRY_INTERVAL (30 * HZ)

struct failed_alarm_write {
    struct crazypod_miniapp_alarm_record desired;
    long retry_not_before;
    bool pending;
};

struct cached_alarm_slots {
    char id[CRAZYPOD_MINIAPP_ID_SIZE];
    struct crazypod_miniapp_alarm_record
        alarms[CP_MINIAPP_ALARM_SLOT_COUNT];
    struct crazypod_miniapp_alarm_record
        notifications[CP_MINIAPP_ALARM_SLOT_COUNT];
    int8_t alarm_status[CP_MINIAPP_ALARM_SLOT_COUNT];
    int8_t notification_status[CP_MINIAPP_ALARM_SLOT_COUNT];
    struct failed_alarm_write
        alarm_failures[CP_MINIAPP_ALARM_SLOT_COUNT];
    struct failed_alarm_write
        notification_failures[CP_MINIAPP_ALARM_SLOT_COUNT];
};

static struct cached_alarm_slots cache[CRAZYPOD_MINIAPP_MAX_APPS];
static int cache_count;
static bool cache_ready;

static uint32_t record_checksum(
    const struct crazypod_miniapp_alarm_record *record)
{
    return ~crc_32r(
        record,
        (uint32_t)offsetof(
            struct crazypod_miniapp_alarm_record, checksum),
        0xffffffffu);
}

static bool valid_id(const char *id)
{
    size_t i;
    size_t length;

    if(id == NULL)
        return false;
    length = strlen(id);
    if(length == 0 || length >= CRAZYPOD_MINIAPP_ID_SIZE ||
       id[0] < 'a' || id[0] > 'z')
        return false;
    for(i = 1; i < length; ++i) {
        char value = id[i];
        if(!((value >= 'a' && value <= 'z') ||
             (value >= '0' && value <= '9') ||
             value == '_' || value == '-'))
            return false;
    }
    return true;
}

static bool ensure_directory(const char *path)
{
    if(mkdir(path) == 0)
        return true;
    return errno == EEXIST && dir_exists(path);
}

static bool make_data_path(
    char *path, size_t capacity, const char *id, const char *filename)
{
    int length;

    if(!valid_id(id))
        return false;
    length = snprintf(
        path, capacity, "%s/%s/%s", DATA_ROOT, id, filename);
    return length >= 0 && (size_t)length < capacity;
}

static bool ensure_data_directory(const char *id)
{
    char path[MAX_PATH];
    int length;

    if(!valid_id(id) || !ensure_directory("/.crazypod") ||
       !ensure_directory(DATA_ROOT))
        return false;
    length = snprintf(path, sizeof(path), "%s/%s", DATA_ROOT, id);
    return length >= 0 && (size_t)length < sizeof(path) &&
           ensure_directory(path);
}

static bool read_exact(int fd, void *buffer, size_t size)
{
    uint8_t *cursor = buffer;

    while(size > 0) {
        ssize_t count = read(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const uint8_t *cursor = buffer;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool slot_filenames(
    uint8_t slot, bool notification,
    char *filename, size_t filename_size,
    char *temporary, size_t temporary_size)
{
    const char *base = notification ? "notification" : "alarm";

    if(slot >= CP_MINIAPP_ALARM_SLOT_COUNT)
        return false;
    if(slot == 0) {
        snprintf(filename, filename_size, "%s.bin", base);
        snprintf(temporary, temporary_size, "%s.tmp", base);
    }
    else {
        snprintf(filename, filename_size, "%s%u.bin",
                 base, (unsigned)slot);
        snprintf(temporary, temporary_size, "%s%u.tmp",
                 base, (unsigned)slot);
    }
    filename[filename_size - 1] = '\0';
    temporary[temporary_size - 1] = '\0';
    return true;
}

static int record_load(
    const char *id, const char *filename,
    struct crazypod_miniapp_alarm_record *record)
{
    char path[MAX_PATH];
    int fd;
    bool valid;

    memset(record, 0, sizeof(*record));
    if(!make_data_path(path, sizeof(path), id, filename))
        return -1;
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return 0;
    valid = filesize(fd) == (off_t)sizeof(*record) &&
            read_exact(fd, record, sizeof(*record));
    close(fd);
    if(!valid || record->magic != ALARM_MAGIC ||
       record->version != DISK_VERSION ||
       record->struct_size != sizeof(*record) ||
       (record->flags & ~(ALARM_ACTIVE | ALARM_FIRED)) != 0 ||
       ((record->flags & ALARM_FIRED) != 0 &&
        (record->flags & ALARM_ACTIVE) == 0) ||
       ((record->flags & ALARM_ACTIVE) != 0 &&
        record->deadline_epoch == 0) ||
       record->checksum != record_checksum(record))
        return -1;
    return 1;
}

static bool record_save(
    const char *id, const char *filename, const char *temporary_filename,
    struct crazypod_miniapp_alarm_record *record)
{
    char path[MAX_PATH];
    char temporary[MAX_PATH];
    int fd;
    bool success;

    if(!ensure_data_directory(id) ||
       !make_data_path(path, sizeof(path), id, filename) ||
       !make_data_path(
           temporary, sizeof(temporary), id, temporary_filename))
        return false;
    record->magic = ALARM_MAGIC;
    record->version = DISK_VERSION;
    record->struct_size = sizeof(*record);
    record->checksum = record_checksum(record);
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, record, sizeof(*record)) &&
              fsync(fd) == 0;
    if(close(fd) < 0)
        success = false;
    if(!success || rename(temporary, path) < 0) {
        remove(temporary);
        return false;
    }
    return true;
}

static int disk_load_slot(
    const char *id, uint8_t slot, bool notification,
    struct crazypod_miniapp_alarm_record *record)
{
    char filename[FILENAME_SIZE];
    char temporary[FILENAME_SIZE];
    int loaded;

    if(!slot_filenames(
           slot, notification, filename, sizeof(filename),
           temporary, sizeof(temporary)))
        return -1;
    loaded = record_load(id, filename, record);
    if(notification && loaded == 1 && record->flags != 0 &&
       record->flags != ALARM_ACTIVE &&
       record->flags != (ALARM_ACTIVE | ALARM_FIRED))
        return -1;
    return loaded;
}

static bool disk_save_slot(
    const char *id, uint8_t slot, bool notification,
    struct crazypod_miniapp_alarm_record *record)
{
    char filename[FILENAME_SIZE];
    char temporary[FILENAME_SIZE];

    return slot_filenames(
               slot, notification, filename, sizeof(filename),
               temporary, sizeof(temporary)) &&
           record_save(id, filename, temporary, record);
}

static struct cached_alarm_slots *cache_find(const char *id)
{
    int index;

    if(id == NULL)
        return NULL;
    for(index = 0; index < cache_count; ++index)
        if(strcmp(cache[index].id, id) == 0)
            return &cache[index];
    return NULL;
}

static void cache_load_entry(
    struct cached_alarm_slots *entry, const char *id)
{
    uint8_t slot;

    memset(entry, 0, sizeof(*entry));
    snprintf(entry->id, sizeof(entry->id), "%s", id);
    for(slot = 0; slot < CP_MINIAPP_ALARM_SLOT_COUNT; ++slot) {
        entry->alarm_status[slot] = disk_load_slot(
            id, slot, false, &entry->alarms[slot]);
        entry->notification_status[slot] = disk_load_slot(
            id, slot, true, &entry->notifications[slot]);
    }
}

void crazypod_miniapp_alarm_store_reload(void)
{
    int index;

    cache_ready = false;
    cache_count = 0;
    memset(cache, 0, sizeof(cache));
    for(index = 0;
        index < crazypod_miniapp_catalog_count() &&
        cache_count < CRAZYPOD_MINIAPP_MAX_APPS;
        ++index) {
        const struct crazypod_miniapp_metadata *metadata =
            crazypod_miniapp_catalog_get(index);

        if(metadata == NULL || !valid_id(metadata->id))
            continue;
        cache_load_entry(&cache[cache_count], metadata->id);
        ++cache_count;
    }
    cache_ready = true;
}

static void ensure_cache_ready(void)
{
    if(!cache_ready)
        crazypod_miniapp_alarm_store_reload();
}

static struct cached_alarm_slots *cache_get_or_add(const char *id)
{
    struct cached_alarm_slots *entry;

    ensure_cache_ready();
    entry = cache_find(id);
    if(entry != NULL)
        return entry;
    if(!valid_id(id) || crazypod_miniapp_catalog_find(id) < 0 ||
       cache_count >= CRAZYPOD_MINIAPP_MAX_APPS)
        return NULL;
    entry = &cache[cache_count++];
    cache_load_entry(entry, id);
    return entry;
}

static int cached_load(
    const char *id, uint8_t slot, bool notification,
    struct crazypod_miniapp_alarm_record *record)
{
    struct cached_alarm_slots *entry;
    int status;

    if(record == NULL || slot >= CP_MINIAPP_ALARM_SLOT_COUNT)
        return -1;
    memset(record, 0, sizeof(*record));
    ensure_cache_ready();
    entry = cache_find(id);
    if(entry == NULL)
        return -1;
    status = notification
        ? entry->notification_status[slot]
        : entry->alarm_status[slot];
    if(status == 1)
        *record = notification
            ? entry->notifications[slot]
            : entry->alarms[slot];
    return status;
}

static bool same_persisted_state(
    const struct crazypod_miniapp_alarm_record *left,
    const struct crazypod_miniapp_alarm_record *right)
{
    return left->deadline_epoch == right->deadline_epoch &&
           left->token == right->token &&
           left->flags == right->flags;
}

static bool cached_save(
    const char *id, uint8_t slot, bool notification,
    struct crazypod_miniapp_alarm_record *record)
{
    struct cached_alarm_slots *entry;
    struct crazypod_miniapp_alarm_record *cached_record;
    struct failed_alarm_write *failure;
    int8_t *status;

    if(record == NULL || slot >= CP_MINIAPP_ALARM_SLOT_COUNT)
        return false;
    entry = cache_get_or_add(id);
    if(entry == NULL)
        return false;
    cached_record = notification
        ? &entry->notifications[slot]
        : &entry->alarms[slot];
    status = notification
        ? &entry->notification_status[slot]
        : &entry->alarm_status[slot];
    failure = notification
        ? &entry->notification_failures[slot]
        : &entry->alarm_failures[slot];
    if(*status == 1 && same_persisted_state(cached_record, record)) {
        failure->pending = false;
        return true;
    }
    if(failure->pending &&
       same_persisted_state(&failure->desired, record) &&
       TIME_BEFORE(current_tick, failure->retry_not_before))
        return false;
    if(!disk_save_slot(id, slot, notification, record)) {
        failure->desired = *record;
        failure->retry_not_before =
            current_tick + WRITE_RETRY_INTERVAL;
        failure->pending = true;
        return false;
    }
    *cached_record = *record;
    *status = 1;
    failure->pending = false;
    return true;
}

int crazypod_miniapp_alarm_store_load(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    return cached_load(id, slot, false, record);
}

bool crazypod_miniapp_alarm_store_save(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    return cached_save(id, slot, false, record);
}

int crazypod_miniapp_notification_store_load(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    return cached_load(id, slot, true, record);
}

bool crazypod_miniapp_notification_store_save(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    return cached_save(id, slot, true, record);
}

bool crazypod_miniapp_alarm_store_same_event(
    const struct crazypod_miniapp_alarm_record *left,
    const struct crazypod_miniapp_alarm_record *right)
{
    return left->deadline_epoch == right->deadline_epoch &&
           left->token == right->token;
}

#endif
