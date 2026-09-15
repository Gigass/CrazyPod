#ifndef CRAZYPOD_CLOCK_SCREEN_H
#define CRAZYPOD_CLOCK_SCREEN_H

#include <stdbool.h>

#include "lvgl.h"

struct crazypod_clock_screen_time {
    int hour;
    int minute;
    int second;
    int second_tenths;
    int weekday;
    int month;
    int month_day;
};

struct crazypod_stopwatch_screen_model {
    long elapsed_ticks;
    int ticks_per_second;
    bool running;
    int style;
    const long *laps;
    int lap_count;
    bool reset_armed;
};

/* Drops the cached dial before the pane is cleaned. */
void crazypod_clock_screen_forget(void);
/* Rotate the hands and retitle the numbers on the dial already up.
 * False when there is none, or when something that is not the time has
 * changed, and the caller must render in full. */
bool crazypod_clock_screen_refresh(
    const struct crazypod_clock_screen_time *time);
bool crazypod_stopwatch_screen_refresh(
    const struct crazypod_stopwatch_screen_model *model);
void crazypod_clock_screen_render(
    lv_obj_t *content,
    const struct crazypod_clock_screen_time *time);
void crazypod_stopwatch_screen_render(
    lv_obj_t *content,
    const struct crazypod_stopwatch_screen_model *model);

#endif
