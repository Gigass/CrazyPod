#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "crc32.h"
#include "dir.h"
#include "errno.h"
#include "file.h"

#include "../../../miniapps/sdk/crazypod_miniapp.h"
#include "../crazypod_miniapps.h"
#include "crazypod_miniapp_alarm_store.h"

#define DATA_ROOT "/.crazypod/miniapp-data"
#define ALARM_MAGIC 0x4350414cu
#define DISK_VERSION 1u
#define FILENAME_SIZE 24

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

static int load_slot(
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

static bool save_slot(
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

int crazypod_miniapp_alarm_store_load(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    return load_slot(id, slot, false, record);
}

bool crazypod_miniapp_alarm_store_save(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    return save_slot(id, slot, false, record);
}

int crazypod_miniapp_notification_store_load(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    return load_slot(id, slot, true, record);
}

bool crazypod_miniapp_notification_store_save(
    const char *id, uint8_t slot,
    struct crazypod_miniapp_alarm_record *record)
{
    return save_slot(id, slot, true, record);
}

bool crazypod_miniapp_alarm_store_same_event(
    const struct crazypod_miniapp_alarm_record *left,
    const struct crazypod_miniapp_alarm_record *right)
{
    return left->deadline_epoch == right->deadline_epoch &&
           left->token == right->token;
}

#endif
