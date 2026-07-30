#include "config.h"

#include "crazypod_l10n.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "crazypod_checksum.h"
#include "crazypod_workouts.h"

#define WORKOUTS_DIRECTORY "/.crazypod"
#define WORKOUTS_PATH WORKOUTS_DIRECTORY "/workouts.bin"
#define WORKOUTS_TEMP WORKOUTS_DIRECTORY "/workouts.tmp"
#define WORKOUTS_MAGIC 0x4350574fu
#define WORKOUTS_VERSION 1u
#define WORKOUTS_MAX 64

struct workouts_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t count;
    uint32_t next_id;
    struct crazypod_workout entries[WORKOUTS_MAX];
    uint32_t checksum;
};

static struct workouts_disk persisted;

static const char *const activity_titles[] = {
    CP_TR("Run"), CP_TR("Walk"), CP_TR("Cycling"), CP_TR("Hiking"), CP_TR("Stairs"),
    CP_TR("Elliptical"), CP_TR("Rowing"), CP_TR("HIIT"), CP_TR("Functional Strength"),
    CP_TR("Strength"), CP_TR("Core"), CP_TR("Yoga"), CP_TR("Pilates"), CP_TR("Flexibility"),
    CP_TR("Dance"), CP_TR("Tennis"), CP_TR("Basketball"), CP_TR("Soccer"),
    CP_TR("Cross Training"), CP_TR("Cooldown")
};

static uint32_t workouts_checksum(const struct workouts_disk *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct workouts_disk, checksum));
}

static bool read_exact(int fd, void *buffer, size_t size)
{
    unsigned char *cursor = buffer;

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
    const unsigned char *cursor = buffer;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool save_workouts(void)
{
    int fd;
    bool success;

    mkdir(WORKOUTS_DIRECTORY);
    persisted.checksum = workouts_checksum(&persisted);
    fd = open(WORKOUTS_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, &persisted, sizeof(persisted));
    if(fsync(fd) < 0)
        success = false;
    close(fd);
    return success && rename(WORKOUTS_TEMP, WORKOUTS_PATH) == 0;
}

void crazypod_workouts_init(void)
{
    int fd;

    memset(&persisted, 0, sizeof(persisted));
    persisted.magic = WORKOUTS_MAGIC;
    persisted.version = WORKOUTS_VERSION;
    persisted.size = sizeof(persisted);
    persisted.next_id = 1;
    fd = open(WORKOUTS_PATH, O_RDONLY);
    if(fd < 0)
        return;
    if(!read_exact(fd, &persisted, sizeof(persisted)) ||
       persisted.magic != WORKOUTS_MAGIC ||
       persisted.version != WORKOUTS_VERSION ||
       persisted.size != sizeof(persisted) ||
       persisted.count > WORKOUTS_MAX ||
       persisted.checksum != workouts_checksum(&persisted)) {
        memset(&persisted, 0, sizeof(persisted));
        persisted.magic = WORKOUTS_MAGIC;
        persisted.version = WORKOUTS_VERSION;
        persisted.size = sizeof(persisted);
        persisted.next_id = 1;
    }
    close(fd);
}

int crazypod_workouts_count(void)
{
    return (int)persisted.count;
}

const struct crazypod_workout *crazypod_workout_get(int index)
{
    if(index < 0 || index >= (int)persisted.count)
        return NULL;
    return &persisted.entries[index];
}

const char *crazypod_workout_activity_title(int activity)
{
    if(activity < 0 || activity >= CRAZYPOD_WORKOUT_ACTIVITY_COUNT)
        return CP_TR("Workout");
    return activity_titles[activity];
}

uint32_t crazypod_workout_add(int activity, int date,
                              uint32_t duration_seconds)
{
    struct workouts_disk before = persisted;
    struct crazypod_workout *entry;

    if(activity < 0 || activity >= CRAZYPOD_WORKOUT_ACTIVITY_COUNT ||
       date < 19000101 || duration_seconds == 0)
        return 0;
    if(persisted.count >= WORKOUTS_MAX)
        --persisted.count;
    if(persisted.count > 0)
        memmove(&persisted.entries[1], &persisted.entries[0],
                persisted.count * sizeof(persisted.entries[0]));
    entry = &persisted.entries[0];
    memset(entry, 0, sizeof(*entry));
    entry->id = persisted.next_id++;
    if(entry->id == 0)
        entry->id = persisted.next_id++;
    entry->date = date;
    entry->duration_seconds = duration_seconds;
    entry->activity = (uint8_t)activity;
    ++persisted.count;
    if(!save_workouts()) {
        persisted = before;
        return 0;
    }
    return entry->id;
}

bool crazypod_workout_delete(uint32_t id)
{
    struct workouts_disk before = persisted;
    uint32_t i;

    for(i = 0; i < persisted.count; ++i) {
        if(persisted.entries[i].id != id)
            continue;
        if(i + 1 < persisted.count)
            memmove(&persisted.entries[i], &persisted.entries[i + 1],
                    (persisted.count - i - 1) *
                        sizeof(persisted.entries[0]));
        --persisted.count;
        memset(&persisted.entries[persisted.count], 0,
               sizeof(persisted.entries[0]));
        if(!save_workouts()) {
            persisted = before;
            return false;
        }
        return true;
    }
    return false;
}

#endif
