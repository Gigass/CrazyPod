#ifndef CRAZYPOD_ACTIVITY_CONTROLLER_H
#define CRAZYPOD_ACTIVITY_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "../../navigation/crazypod_ui_routes.h"
#include "crazypod_clock_screen.h"

enum crazypod_activity_action_kind {
    CRAZYPOD_ACTIVITY_ACTION_UNHANDLED,
    CRAZYPOD_ACTIVITY_ACTION_NONE,
    CRAZYPOD_ACTIVITY_ACTION_RENDER,
    CRAZYPOD_ACTIVITY_ACTION_PUSH,
};

struct crazypod_activity_action {
    enum crazypod_activity_action_kind kind;
    enum crazypod_route route;
    int group;
};

bool crazypod_activity_stopwatch_running(void);
int crazypod_activity_stopwatch_style(void);
bool crazypod_activity_workout_running(void);
int crazypod_activity_workout_type(void);
uint32_t crazypod_activity_workout_seconds(long now, int ticks_per_second);

void crazypod_activity_stopwatch_model(
    long now, int ticks_per_second,
    struct crazypod_stopwatch_screen_model *model);
struct crazypod_activity_action crazypod_activity_activate(
    const struct route_state *state, long now);

bool crazypod_activity_stopwatch_change_style(int direction);
bool crazypod_activity_stopwatch_add_lap(long now);
bool crazypod_activity_stopwatch_reset(long now, int ticks_per_second);
void crazypod_activity_stopwatch_leave(void);
void crazypod_activity_workout_pause(long now);
bool crazypod_activity_workout_finish(long now, int ticks_per_second);
bool crazypod_activity_workout_delete(uint32_t id);

bool crazypod_activity_service_stopwatch(
    long now, int ticks_per_second);
bool crazypod_activity_service_workout(
    long now, int ticks_per_second);

#ifdef SIMULATOR
void crazypod_activity_simulator_workout(
    int activity, long accumulated_ticks,
    long started_at, bool running);
#endif

#endif
