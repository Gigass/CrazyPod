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

void crazypod_clock_screen_render(
    lv_obj_t *content,
    const struct crazypod_clock_screen_time *time);
void crazypod_stopwatch_screen_render(
    lv_obj_t *content,
    const struct crazypod_stopwatch_screen_model *model);

#endif
