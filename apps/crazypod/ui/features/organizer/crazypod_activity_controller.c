#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "kernel.h"
#include "timefuncs.h"

#include "../../../crazypod_workouts.h"
#include "crazypod_activity_controller.h"

#define STOPWATCH_LAP_CAPACITY 32

static bool stopwatch_running;
static long stopwatch_started_at;
static long stopwatch_accumulated_ticks;
static long stopwatch_last_render_tick;
static long stopwatch_laps[STOPWATCH_LAP_CAPACITY];
static int stopwatch_lap_count;
static int stopwatch_style;
static long stopwatch_reset_deadline;

static bool workout_running;
static long workout_started_at;
static long workout_accumulated_ticks;
static long workout_last_render_tick;
static int workout_activity;

static struct crazypod_activity_action action(
    enum crazypod_activity_action_kind kind)
{
    const struct crazypod_activity_action result = {
        .kind = kind,
    };
    return result;
}

static struct crazypod_activity_action push(
    enum crazypod_route route, int group)
{
    struct crazypod_activity_action result =
        action(CRAZYPOD_ACTIVITY_ACTION_PUSH);

    result.route = route;
    result.group = group;
    return result;
}

static long stopwatch_elapsed(long now)
{
    return stopwatch_accumulated_ticks +
           (stopwatch_running ? now - stopwatch_started_at : 0);
}

static long workout_elapsed(long now)
{
    long elapsed = workout_accumulated_ticks;

    if(workout_running)
        elapsed += now - workout_started_at;
    return elapsed > 0 ? elapsed : 0;
}

bool crazypod_activity_stopwatch_running(void)
{
    return stopwatch_running;
}

int crazypod_activity_stopwatch_style(void)
{
    return stopwatch_style;
}

bool crazypod_activity_workout_running(void)
{
    return workout_running;
}

int crazypod_activity_workout_type(void)
{
    return workout_activity;
}

uint32_t crazypod_activity_workout_seconds(
    long now, int ticks_per_second)
{
    return ticks_per_second > 0
        ? (uint32_t)(workout_elapsed(now) / ticks_per_second) : 0;
}

void crazypod_activity_stopwatch_model(
    long now, int ticks_per_second,
    struct crazypod_stopwatch_screen_model *model)
{
    model->elapsed_ticks = stopwatch_elapsed(now);
    model->ticks_per_second = ticks_per_second;
    model->running = stopwatch_running;
    model->style = stopwatch_style;
    model->laps = stopwatch_laps;
    model->lap_count = stopwatch_lap_count;
    model->reset_armed =
        TIME_BEFORE(now, stopwatch_reset_deadline);
}

struct crazypod_activity_action crazypod_activity_activate(
    const struct route_state *state, long now)
{
    switch(state->route) {
    case STOPWATCH_ROUTE_VIEW:
        stopwatch_reset_deadline = 0;
        if(stopwatch_running) {
            stopwatch_accumulated_ticks +=
                now - stopwatch_started_at;
            stopwatch_running = false;
        }
        else {
            stopwatch_started_at = now;
            stopwatch_running = true;
        }
        stopwatch_last_render_tick = now;
        return action(CRAZYPOD_ACTIVITY_ACTION_RENDER);
    case WORKOUT_ROUTE_MENU:
        if(state->selected == 0)
            return push(WORKOUT_ROUTE_TYPES, -1);
        if(state->selected == 1)
            return push(WORKOUT_ROUTE_HISTORY, -1);
        return push(WORKOUT_ROUTE_SUMMARY, -1);
    case WORKOUT_ROUTE_TYPES:
        workout_activity = state->selected;
        workout_running = false;
        workout_accumulated_ticks = 0;
        return push(WORKOUT_ROUTE_READY, -1);
    case WORKOUT_ROUTE_READY:
        workout_accumulated_ticks = 0;
        workout_started_at = now;
        workout_last_render_tick = now;
        workout_running = true;
        return push(WORKOUT_ROUTE_ACTIVE, -1);
    case WORKOUT_ROUTE_ACTIVE:
        if(workout_running) {
            workout_accumulated_ticks +=
                now - workout_started_at;
            workout_running = false;
        }
        else {
            workout_started_at = now;
            workout_running = true;
        }
        return action(CRAZYPOD_ACTIVITY_ACTION_RENDER);
    case WORKOUT_ROUTE_HISTORY:
        return crazypod_workout_get(state->selected) != NULL
            ? push(WORKOUT_ROUTE_DETAIL, state->selected)
            : action(CRAZYPOD_ACTIVITY_ACTION_NONE);
    case WORKOUT_ROUTE_DETAIL: {
        const struct crazypod_workout *workout =
            crazypod_workout_get(state->group);

        return workout != NULL
            ? push(WORKOUT_ROUTE_DELETE_CONFIRM, (int)workout->id)
            : action(CRAZYPOD_ACTIVITY_ACTION_NONE);
    }
    case WORKOUT_ROUTE_FINISH_CONFIRM:
    case WORKOUT_ROUTE_SUMMARY:
    case WORKOUT_ROUTE_DELETE_CONFIRM:
        return action(CRAZYPOD_ACTIVITY_ACTION_NONE);
    default:
        return action(CRAZYPOD_ACTIVITY_ACTION_UNHANDLED);
    }
}

bool crazypod_activity_stopwatch_change_style(int direction)
{
    if(stopwatch_lap_count != 0)
        return false;
    stopwatch_style = (stopwatch_style + direction) % 3;
    if(stopwatch_style < 0)
        stopwatch_style += 3;
    return true;
}

bool crazypod_activity_stopwatch_add_lap(long now)
{
    if(!stopwatch_running ||
       stopwatch_lap_count >= STOPWATCH_LAP_CAPACITY)
        return false;
    stopwatch_laps[stopwatch_lap_count++] =
        stopwatch_elapsed(now);
    return true;
}

bool crazypod_activity_stopwatch_reset(
    long now, int ticks_per_second)
{
    if(stopwatch_running ||
       (stopwatch_accumulated_ticks <= 0 &&
        stopwatch_lap_count <= 0))
        return false;
    if(TIME_BEFORE(now, stopwatch_reset_deadline)) {
        memset(stopwatch_laps, 0, sizeof(stopwatch_laps));
        stopwatch_lap_count = 0;
        stopwatch_started_at = 0;
        stopwatch_accumulated_ticks = 0;
        stopwatch_reset_deadline = 0;
    }
    else
        stopwatch_reset_deadline =
            now + ticks_per_second * 3 / 2;
    stopwatch_last_render_tick = now;
    return true;
}

void crazypod_activity_stopwatch_leave(void)
{
    stopwatch_reset_deadline = 0;
}

void crazypod_activity_workout_pause(long now)
{
    if(workout_running) {
        workout_accumulated_ticks += now - workout_started_at;
        workout_running = false;
    }
}

bool crazypod_activity_workout_finish(
    long now, int ticks_per_second)
{
    struct tm *time = get_time();
    uint32_t seconds;
    int date;

    crazypod_activity_workout_pause(now);
    seconds = ticks_per_second > 0
        ? (uint32_t)(workout_accumulated_ticks / ticks_per_second)
        : 0;
    if(seconds == 0)
        seconds = 1;
    date = (time->tm_year + 1900) * 10000 +
           (time->tm_mon + 1) * 100 + time->tm_mday;
    if(crazypod_workout_add(
           workout_activity, date, seconds) == 0)
        return false;
    workout_accumulated_ticks = 0;
    return true;
}

bool crazypod_activity_workout_delete(uint32_t id)
{
    return crazypod_workout_delete(id);
}

bool crazypod_activity_service_stopwatch(
    long now, int ticks_per_second)
{
    bool render = false;

    if(stopwatch_reset_deadline != 0 &&
       !TIME_BEFORE(now, stopwatch_reset_deadline)) {
        stopwatch_reset_deadline = 0;
        render = true;
    }
    if(stopwatch_running &&
       !TIME_BEFORE(
           now, stopwatch_last_render_tick + ticks_per_second / 10)) {
        stopwatch_last_render_tick = now;
        render = true;
    }
    return render;
}

bool crazypod_activity_service_workout(
    long now, int ticks_per_second)
{
    if(!workout_running ||
       TIME_BEFORE(
           now, workout_last_render_tick + ticks_per_second / 10))
        return false;
    workout_last_render_tick = now;
    return true;
}

#ifdef SIMULATOR
void crazypod_activity_simulator_workout(
    int activity, long accumulated_ticks,
    long started_at, bool running)
{
    workout_activity = activity;
    workout_accumulated_ticks = accumulated_ticks;
    workout_started_at = started_at;
    workout_running = running;
}
#endif

#endif
