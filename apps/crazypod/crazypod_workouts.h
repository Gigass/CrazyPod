#ifndef CRAZYPOD_WORKOUTS_H
#define CRAZYPOD_WORKOUTS_H

#include <stdbool.h>
#include <stdint.h>

#define CRAZYPOD_WORKOUT_ACTIVITY_COUNT 20

struct crazypod_workout {
    uint32_t id;
    int32_t date;
    uint32_t duration_seconds;
    uint8_t activity;
};

void crazypod_workouts_init(void);
int crazypod_workouts_count(void);
const struct crazypod_workout *crazypod_workout_get(int index);
const char *crazypod_workout_activity_title(int activity);
uint32_t crazypod_workout_add(int activity, int date,
                              uint32_t duration_seconds);
bool crazypod_workout_delete(uint32_t id);

#endif
